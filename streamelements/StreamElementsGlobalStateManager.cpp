#include "StreamElementsGlobalStateManager.hpp"
#include "StreamElementsApiMessageHandler.hpp"
#include "StreamElementsUtils.hpp"
#include "Version.hpp"
#include "StreamElementsBrowserDialog.hpp"
#include "StreamElementsReportIssueDialog.hpp"

#include "base64/base64.hpp"
#include "json11/json11.hpp"
#include "cef-headers.hpp"

#include <util/threading.h>

#include <QPushButton>
#include <QMessageBox>

/* ========================================================================= */

static std::string GetCEFStoragePath()
{
	std::string version = GetCefVersionString();

	return obs_module_config_path(version.c_str());
}

/* ========================================================================= */

StreamElementsGlobalStateManager::ApplicationStateListener::ApplicationStateListener() :
	m_timer(this)
{
	uint32_t obsMajorVersion = obs_get_version() >> 24;

	if (obsMajorVersion < 22) {
		m_timer.setSingleShot(false);
		m_timer.setInterval(100);

		QObject::connect(&m_timer, &QTimer::timeout, [this]() {
			applicationStateChanged();
		});

		m_timer.start();
	}
}

StreamElementsGlobalStateManager::ApplicationStateListener::~ApplicationStateListener()
{
	uint32_t obsMajorVersion = obs_get_version() >> 24;

	if (obsMajorVersion < 22) {
		m_timer.stop();
	}
}

void StreamElementsGlobalStateManager::ApplicationStateListener::applicationStateChanged()
{
	// This is an unpleasant hack for OBS versions before
	// OBS 22.
	//
	// Older versions of OBS enabled global hotkeys only
	// when the OBS window was not focused to prevent
	// hotkey collision with text boxes and "natural"
	// program hotkeys. This mechanism relies on Qt's
	// QGuiApplication::applicationStateChanged event which
	// does not fire under certain conditions with CEF in
	// focus for the first time you set a hotkey.
	//
	// To mitigate, we re-enable background hotkeys
	// 10 times/sec for older OBS versions.
	//
	obs_hotkey_enable_background_press(true);
}

/* ========================================================================= */

StreamElementsGlobalStateManager::ThemeChangeListener::ThemeChangeListener() : QDockWidget()
{
	setVisible(false);
}

void StreamElementsGlobalStateManager::ThemeChangeListener::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::StyleChange) {
		std::string newTheme = GetCurrentThemeName();

		if (newTheme != m_currentTheme) {
			json11::Json json = json11::Json::object{
				{ "name", newTheme },
			};

			StreamElementsCefClient::DispatchJSEvent("hostUIThemeChanged", json.dump());

			m_currentTheme = newTheme;
		}
	}
}

/* ========================================================================= */

static void handle_obs_frontend_event(enum obs_frontend_event event, void* data)
{
	std::string name;
	std::string args = "null";

	switch (event) {
	case OBS_FRONTEND_EVENT_STREAMING_STARTING:
		name = "hostStreamingStarting";
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
		name = "hostStreamingStarted";
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPING:
		name = "hostStreamingStopping";
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		name = "hostStreamingStopped";
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTING:
		name = "hostRecordingStarting";
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		name = "hostRecordingStarted";
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPING:
		name = "hostRecordingStopping";
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		name = "hostRecordingStopped";
		break;
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
	{
		obs_source_t *source = obs_frontend_get_current_scene();

		json11::Json json = json11::Json::object{
			{ "name", obs_source_get_name(source) },
			{ "width", (int)obs_source_get_width(source) },
			{ "height", (int)obs_source_get_height(source) }
		};

		obs_source_release(source);

		name = "hostActiveSceneChanged";
		args = json.dump();
		break;
	}
	case OBS_FRONTEND_EVENT_EXIT:
		name = "hostExit";
		break;
	default:
		return;
	}

	StreamElementsCefClient::DispatchJSEvent(name, args);
}

/* ========================================================================= */

static class BrowserTask : public CefTask {
public:
	std::function<void()> task;

	inline BrowserTask(std::function<void()> task_) : task(task_) {}
	virtual void Execute() override { task(); }

	IMPLEMENT_REFCOUNTING(BrowserTask);
};

static bool QueueCEFTask(std::function<void()> task)
{
	return CefPostTask(TID_UI, CefRefPtr<BrowserTask>(new BrowserTask(task)));
}

static void FlushCookiesSync()
{
	static std::mutex s_flush_cookies_sync_mutex;

	std::lock_guard<std::mutex> guard(s_flush_cookies_sync_mutex);

	os_event_t* complete_event;

	os_event_init(&complete_event, OS_EVENT_TYPE_AUTO);

	class CompletionCallback : public CefCompletionCallback
	{
	public:
		CompletionCallback(os_event_t* complete_event): m_complete_event(complete_event)
		{
		}

		virtual void OnComplete() override
		{
			os_event_signal(m_complete_event);
		}

		IMPLEMENT_REFCOUNTING(CompletionCallback);

	private:
		os_event_t* m_complete_event;
	};

	CefRefPtr<CefCompletionCallback> callback = new CompletionCallback(complete_event);

	auto task = [&]() {
		if (!CefCookieManager::GetGlobalManager(NULL)->FlushStore(callback)) {
			blog(LOG_ERROR, "CefCookieManager::GetGlobalManager(NULL)->FlushStore() failed.");

			os_event_signal(complete_event);
		}
	};

	CefPostTask(TID_IO, CefRefPtr<BrowserTask>(new BrowserTask(task)));

	os_event_wait(complete_event);
	os_event_destroy(complete_event);
}

/* ========================================================================= */

StreamElementsGlobalStateManager* StreamElementsGlobalStateManager::s_instance = nullptr;

StreamElementsGlobalStateManager::StreamElementsGlobalStateManager()
{
}

StreamElementsGlobalStateManager::~StreamElementsGlobalStateManager()
{
	Shutdown();
}

StreamElementsGlobalStateManager* StreamElementsGlobalStateManager::GetInstance()
{
	if (!s_instance) {
		s_instance = new StreamElementsGlobalStateManager();
	}

	return s_instance;
}

#include <QWindow>
#include <QObjectList>

void StreamElementsGlobalStateManager::Initialize(QMainWindow* obs_main_window)
{
	m_mainWindow = obs_main_window;

	struct local_context {
		StreamElementsGlobalStateManager* self;
		QMainWindow* obs_main_window;
	};

	local_context context;

	context.self = this;
	context.obs_main_window = obs_main_window;

	QtExecSync([](void* data) -> void {
		local_context* context = (local_context*)data;

		// http://doc.qt.io/qt-5/qmainwindow.html#DockOption-enum
		context->obs_main_window->setDockOptions(
			QMainWindow::AnimatedDocks
			| QMainWindow::AllowNestedDocks
			| QMainWindow::AllowTabbedDocks
		);

		context->self->m_appStateListener = new ApplicationStateListener();
		context->self->m_themeChangeListener = new ThemeChangeListener();
		context->self->mainWindow()->addDockWidget(
			Qt::NoDockWidgetArea,
			context->self->m_themeChangeListener);

		context->self->m_widgetManager = new StreamElementsBrowserWidgetManager(context->obs_main_window);
		context->self->m_menuManager = new StreamElementsMenuManager(context->obs_main_window);
		context->self->m_bwTestManager = new StreamElementsBandwidthTestManager();
		context->self->m_outputSettingsManager = new StreamElementsOutputSettingsManager();
		context->self->m_workerManager = new StreamElementsWorkerManager();
		context->self->m_hotkeyManager = new StreamElementsHotkeyManager();
		context->self->m_performanceHistoryTracker = new StreamElementsPerformanceHistoryTracker();
		context->self->m_analyticsEventsManager = new StreamElementsAnalyticsEventsManager();

		{
			// Set up "Live Support" button
			/*QPushButton* liveSupport = new QPushButton(
				QIcon(QPixmap(QString(":/images/icon.png"))),
				obs_module_text("StreamElements.Action.LiveSupport"));*/

			QPushButton* liveSupport = new QPushButton(obs_module_text("StreamElements.Action.LiveSupport"));

			liveSupport->setStyleSheet(QString(
				"QPushButton { background-color: #d9dded; border: 1px solid #546ac8; color: #032ee1; padding: 2px; border-radius: 0px; } "
				"QPushButton:hover { background-color: #99aaec; border: 1px solid #546ac8; color: #ffffff; } "
				"QPushButton:pressed { background-color: #808dc0; border: 1px solid #546ac8; color: #ffffff; } "
			));

			QDockWidget* controlsDock = (QDockWidget*)context->obs_main_window->findChild<QDockWidget*>("controlsDock");
			QVBoxLayout* buttonsVLayout = (QVBoxLayout*)controlsDock->findChild<QVBoxLayout*>("buttonsVLayout");
			buttonsVLayout->addWidget(liveSupport);

			QObject::connect(liveSupport, &QPushButton::clicked, []()
			{
				QUrl navigate_url = QUrl(obs_module_text("StreamElements.Action.LiveSupport.URL"), QUrl::TolerantMode);
				QDesktopServices::openUrl(navigate_url);
			});
		}

		{
			// Set up status bar
			QWidget* container = new QWidget();
			QHBoxLayout* layout = new QHBoxLayout();

			layout->setContentsMargins(5, 0, 5, 0);

			container->setLayout(layout);

			char version_buf[512];
			sprintf(version_buf, "OBS.Live version %s powered by StreamElements",
				GetStreamElementsPluginVersionString());

			container->layout()->addWidget(new QLabel(version_buf, container));

			/*
			QLabel* h_logo = new QLabel();		
			h_logo->setPixmap(QPixmap(QString(":/images/logo_100x30.png")));
			h_logo->setScaledContents(true);
			h_logo->setFixedSize(100, 30);
			container->layout()->addWidget(h_logo);
			*/
			/*
			QLabel* suffix = new QLabel("", container);
			container->layout()->addWidget(suffix);
			*/

			context->obs_main_window->statusBar()->addPermanentWidget(container);
		}

		std::string storagePath = GetCEFStoragePath();
		int os_mkdirs_ret = os_mkdirs(storagePath.c_str());
		std::string onBoardingReason = "";

		if (MKDIR_ERROR == os_mkdirs_ret) {
			blog(LOG_ERROR, "obs-browser: init: failed creating new cookie storage path: %s", storagePath.c_str());
		}
		else {
			bool ret =
				CefCookieManager::GetGlobalManager(nullptr)->SetStoragePath(
					storagePath, false, nullptr);

			if (!ret) {
				blog(LOG_ERROR, "obs-browser: init: failed setting cookie storage path: %s", storagePath.c_str());
			}
			else {
				blog(LOG_INFO, "obs-browser: init: cookie storage path set to: %s", storagePath.c_str());
			}
		}

		bool isOnBoarding = false;

		if (MKDIR_ERROR == os_mkdirs_ret) {
			blog(LOG_WARNING, "obs-browser: init: set on-boarding mode due to error creating new cookie storage path: %s", storagePath.c_str());

			isOnBoarding = true;
			onBoardingReason = "Failed creating new cookie storage folder";
		}
		else if (MKDIR_SUCCESS == os_mkdirs_ret) {
			blog(LOG_INFO, "obs-browser: init: set on-boarding mode due to new cookie storage path: %s", storagePath.c_str());

			isOnBoarding = true;
			onBoardingReason = "New cookie storage folder";
		}
		else if (StreamElementsConfig::GetInstance()->GetStreamElementsPluginVersion() != STREAMELEMENTS_PLUGIN_VERSION) {
			blog(LOG_INFO, "obs-browser: init: set on-boarding mode due to configuration version mismatch");

			isOnBoarding = true;
			onBoardingReason = "State saved by other version of the plug-in";
		}
		else if (StreamElementsConfig::GetInstance()->GetStartupFlags() & StreamElementsConfig::STARTUP_FLAGS_ONBOARDING_MODE) {
			blog(LOG_INFO, "obs-browser: init: set on-boarding mode due to start-up flags");

			isOnBoarding = true;
			onBoardingReason = "Start-up flags indicate on-boarding mode";
		}

		if (isOnBoarding) {
			// On-boarding

			// Reset state but don't erase all cookies
			context->self->Reset(false);
		}
		else {
			// Regular

			context->self->RestoreState();
		}

		QApplication::sendPostedEvents();

		context->self->m_menuManager->Update();

		{
			json11::Json::object eventProps = {
				{ "isOnBoarding", isOnBoarding },
			};
			if (isOnBoarding) {
				eventProps["onBoardingReason"] = onBoardingReason.c_str();
			}

			context->self->GetAnalyticsEventsManager()->trackEvent("Initialized", eventProps);
		}
	}, &context);

	QtPostTask([](void* /*data*/) {
		// Update visible state
		StreamElementsGlobalStateManager::GetInstance()->GetMenuManager()->Update();
	}, this);

	m_initialized = true;
	m_persistStateEnabled = true;

	obs_frontend_add_event_callback(handle_obs_frontend_event, nullptr);
}

void StreamElementsGlobalStateManager::Shutdown()
{
	if (!m_initialized) {
		return;
	}

	obs_frontend_remove_event_callback(handle_obs_frontend_event, nullptr);

	FlushCookiesSync();

	QtExecSync([](void* data) -> void {
		StreamElementsGlobalStateManager* self = (StreamElementsGlobalStateManager*)data;

		//self->mainWindow()->removeDockWidget(self->m_themeChangeListener);
		self->m_themeChangeListener->deleteLater();
		self->m_appStateListener->deleteLater();

		delete self->m_analyticsEventsManager;
		delete self->m_performanceHistoryTracker;
		delete self->m_outputSettingsManager;
		delete self->m_bwTestManager;
		delete self->m_widgetManager;
		delete self->m_menuManager;
		delete self->m_hotkeyManager;
	}, this);

	m_initialized = false;
}

void StreamElementsGlobalStateManager::StartOnBoardingUI(bool forceOnboarding)
{
	std::string onBoardingURL = GetCommandLineOptionValue("streamelements-onboarding-url");

	if (!onBoardingURL.size()) {
		onBoardingURL = StreamElementsConfig::GetInstance()->GetUrlOnBoarding();
	}

	{
		// Manipulate on-boarding URL query string to reflect forceOnboarding state

		const char* QS_ONBOARDING = "onboarding";

		QUrl url(onBoardingURL.c_str());
		QUrlQuery query(url);

		if (query.hasQueryItem(QS_ONBOARDING)) {
			query.removeQueryItem(QS_ONBOARDING);
		}

		if (forceOnboarding) {
			query.addQueryItem(QS_ONBOARDING, "1");
		}

		url.setQuery(query.query());

		onBoardingURL = url.toString().toStdString();
	}

	StopOnBoardingUI();

	GetWidgetManager()->PushCentralBrowserWidget(onBoardingURL.c_str(), nullptr);
	GetHotkeyManager()->RemoveAllManagedHotkeyBindings();

	StreamElementsConfig::GetInstance()->SetStartupFlags(StreamElementsConfig::STARTUP_FLAGS_ONBOARDING_MODE);

	QtPostTask([](void*) -> void {
		StreamElementsGlobalStateManager::GetInstance()->GetMenuManager()->Update();
		StreamElementsGlobalStateManager::GetInstance()->PersistState();
	}, nullptr);
}

void StreamElementsGlobalStateManager::StopOnBoardingUI()
{
	GetWorkerManager()->RemoveAll();
	GetWidgetManager()->HideNotificationBar();
	GetWidgetManager()->RemoveAllDockWidgets();
	GetWidgetManager()->DestroyCurrentCentralBrowserWidget();
}

void StreamElementsGlobalStateManager::SwitchToOBSStudio()
{
	typedef std::vector<std::string> ids_t;

	ids_t workers;
	ids_t widgets;

	GetWorkerManager()->GetIdentifiers(workers);
	GetWidgetManager()->GetDockBrowserWidgetIdentifiers(widgets);

	bool isEnabled = false;

	isEnabled |= (!!workers.size());
	isEnabled |= (!!widgets.size());
	isEnabled |= GetWidgetManager()->HasNotificationBar();
	isEnabled |= GetWidgetManager()->HasCentralBrowserWidget();

	if (isEnabled) {
		QMessageBox::StandardButton reply;
		reply = QMessageBox::question(
			mainWindow(),
			obs_module_text("StreamElements.Action.StopOnBoardingUI.Confirmation.Title"),
			obs_module_text("StreamElements.Action.StopOnBoardingUI.Confirmation.Text"),
			QMessageBox::Ok | QMessageBox::Cancel);

		if (reply == QMessageBox::Ok) {
			StreamElementsGlobalStateManager::GetInstance()->StopOnBoardingUI();

			/*
			StreamElementsConfig::GetInstance()->SetStartupFlags(
				StreamElementsConfig::GetInstance()->GetStartupFlags() &
				~StreamElementsConfig::STARTUP_FLAGS_ONBOARDING_MODE);*/

			GetMenuManager()->Update();
		}
	}
	else {
		QMessageBox::question(
			mainWindow(),
			obs_module_text("StreamElements.Action.StopOnBoardingUI.Disabled.Title"),
			obs_module_text("StreamElements.Action.StopOnBoardingUI.Disabled.Text"),
			QMessageBox::Ok);
	}
}

void StreamElementsGlobalStateManager::DeleteCookies()
{
	class CookieVisitor :
		public CefCookieVisitor,
		public CefBaseRefCounted
	{
	public:
		CookieVisitor() { }
		~CookieVisitor() { }

		virtual bool Visit(const CefCookie& cookie, int count, int total, bool& deleteCookie) override
		{
			deleteCookie = true;

			CefString domain(&cookie.domain);
			CefString name(&cookie.name);

			// Process cookie whitelist. This is used for
			// preserving two-factor-authentication (2FA)
			// "remember this computer" state.
			//
			if (domain == ".twitch.tv" && name == "authy_id") {
				deleteCookie = false;
			}

			return true;
		}
	public:
		IMPLEMENT_REFCOUNTING(CookieVisitor);
	};

	if (!CefCookieManager::GetGlobalManager(NULL)->VisitAllCookies(new CookieVisitor())) {
		blog(LOG_ERROR, "CefCookieManager::GetGlobalManager(NULL)->VisitAllCookies() failed.");
	}

	/*
	if (!CefCookieManager::GetGlobalManager(NULL)->DeleteCookies(
	CefString(""), // URL
	CefString(""), // Cookie name
	nullptr)) {    // On-complete callback
	blog(LOG_ERROR, "CefCookieManager::GetGlobalManager(NULL)->DeleteCookies() failed.");
	}
	*/
}

void StreamElementsGlobalStateManager::Reset(bool deleteAllCookies, bool forceOnboarding)
{
	if (deleteAllCookies) {
		DeleteCookies();
	}

	StartOnBoardingUI(forceOnboarding);
}

void StreamElementsGlobalStateManager::PersistState()
{
	SYNC_ACCESS();

	if (!m_persistStateEnabled) {
		return;
	}

	FlushCookiesSync();

	CefRefPtr<CefValue> root = CefValue::Create();
	CefRefPtr<CefDictionaryValue> rootDictionary = CefDictionaryValue::Create();
	root->SetDictionary(rootDictionary);

	CefRefPtr<CefValue> dockingWidgetsState = CefValue::Create();
	CefRefPtr<CefValue> notificationBarState = CefValue::Create();
	CefRefPtr<CefValue> workersState = CefValue::Create();
	CefRefPtr<CefValue> hotkeysState = CefValue::Create();

	GetWidgetManager()->SerializeDockingWidgets(dockingWidgetsState);
	GetWidgetManager()->SerializeNotificationBar(notificationBarState);
	GetWorkerManager()->Serialize(workersState);
	GetHotkeyManager()->SerializeHotkeyBindings(hotkeysState, true);

	rootDictionary->SetValue("dockingBrowserWidgets", dockingWidgetsState);
	rootDictionary->SetValue("notificationBar", notificationBarState);
	rootDictionary->SetValue("workers", workersState);
	rootDictionary->SetValue("hotkeyBindings", hotkeysState);

	QByteArray geometry = mainWindow()->saveGeometry();
	QByteArray windowState = mainWindow()->saveState();

	rootDictionary->SetString("geometry", base64_encode(geometry.begin(), geometry.size()));
	rootDictionary->SetString("windowState", base64_encode(windowState.begin(), windowState.size()));

	CefString json = CefWriteJSON(root, JSON_WRITER_DEFAULT);

	std::string base64EncodedJSON = base64_encode(json.ToString());

	StreamElementsConfig::GetInstance()->SetStartupState(base64EncodedJSON);
}

void StreamElementsGlobalStateManager::RestoreState()
{
	std::string base64EncodedJSON = StreamElementsConfig::GetInstance()->GetStartupState();

	if (!base64EncodedJSON.size()) {
		return;
	}

	blog(LOG_INFO, "obs-browser: state: restoring state from base64: %s", base64EncodedJSON.c_str());

	CefString json = base64_decode(base64EncodedJSON);

	if (!json.size()) {
		return;
	}

	blog(LOG_INFO, "obs-browser: state: restoring state from json: %s", json.ToString().c_str());

	CefRefPtr<CefValue> root = CefParseJSON(json, JSON_PARSER_ALLOW_TRAILING_COMMAS);
	CefRefPtr<CefDictionaryValue> rootDictionary = root->GetDictionary();

	if (!rootDictionary.get()) {
		return;
	}

	auto dockingWidgetsState = rootDictionary->GetValue("dockingBrowserWidgets");
	auto notificationBarState = rootDictionary->GetValue("notificationBar");
	auto workersState = rootDictionary->GetValue("workers");
	auto hotkeysState = rootDictionary->GetValue("hotkeyBindings");

	if (workersState.get()) {
		blog(LOG_INFO, "obs-browser: state: restoring workers: %s", CefWriteJSON(workersState, JSON_WRITER_DEFAULT).ToString().c_str());
		GetWorkerManager()->Deserialize(workersState);
	}

	if (dockingWidgetsState.get()) {
		blog(LOG_INFO, "obs-browser: state: restoring docking widgets: %s", CefWriteJSON(dockingWidgetsState, JSON_WRITER_DEFAULT).ToString().c_str());
		GetWidgetManager()->DeserializeDockingWidgets(dockingWidgetsState);
	}

	if (notificationBarState.get()) {
		blog(LOG_INFO, "obs-browser: state: restoring notification bar: %s", CefWriteJSON(notificationBarState, JSON_WRITER_DEFAULT).ToString().c_str());
		GetWidgetManager()->DeserializeNotificationBar(notificationBarState);
	}

	if (hotkeysState.get()) {
		blog(LOG_INFO, "obs-browser: state: restoring hotkey bindings: %s", CefWriteJSON(hotkeysState, JSON_WRITER_DEFAULT).ToString().c_str());
		GetHotkeyManager()->DeserializeHotkeyBindings(hotkeysState);
	}

	auto geometry = rootDictionary->GetValue("geometry");
	auto windowState = rootDictionary->GetValue("windowState");

	if (geometry.get() && geometry->GetType() == VTYPE_STRING) {
		blog(LOG_INFO, "obs-browser: state: restoring geometry: %s", geometry->GetString());

		mainWindow()->restoreGeometry(
			QByteArray::fromStdString(base64_decode(geometry->GetString().ToString())));

		// https://bugreports.qt.io/browse/QTBUG-46620
		if (mainWindow()->isMaximized()) {
			mainWindow()->setGeometry(QApplication::desktop()->availableGeometry(mainWindow()));
		}
	}

	if (windowState.get() && windowState->GetType() == VTYPE_STRING) {
		blog(LOG_INFO, "obs-browser: state: restoring windowState: %s", windowState->GetString());

		mainWindow()->restoreState(
			QByteArray::fromStdString(base64_decode(windowState->GetString().ToString())));
	}
}

void StreamElementsGlobalStateManager::OnObsExit()
{
	PersistState();

	m_persistStateEnabled = false;
}

bool StreamElementsGlobalStateManager::DeserializeStatusBarTemporaryMessage(CefRefPtr<CefValue> input)
{
	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	if (d.get() && d->HasKey("text")) {
		int timeoutMilliseconds = 0;
		std::string text = d->GetString("text");

		if (d->HasKey("timeoutMilliseconds")) {
			timeoutMilliseconds = d->GetInt("timeoutMilliseconds");
		}

		mainWindow()->statusBar()->showMessage(text.c_str(), timeoutMilliseconds);

		return true;
	}

	return false;
}

#include <include/cef_parser.h>		// CefParseJSON, CefWriteJSON

bool StreamElementsGlobalStateManager::DeserializeModalDialog(CefRefPtr<CefValue> input, CefRefPtr<CefValue>& output)
{
	output->SetNull();

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	if (d.get() && d->HasKey("url")) {
		std::string url = d->GetString("url").ToString();
		std::string executeJavaScriptOnLoad;

		if (d->HasKey("executeJavaScriptOnLoad")) {
			executeJavaScriptOnLoad = d->GetString("executeJavaScriptOnLoad");
		}

		int width = 800;
		int height = 600;

		if (d->HasKey("width")) {
			width = d->GetInt("width");
		}

		if (d->HasKey("height")) {
			height = d->GetInt("height");
		}

		StreamElementsBrowserDialog dialog(mainWindow(), url, executeJavaScriptOnLoad);

		if (d->HasKey("title")) {
			dialog.setWindowTitle(QString(d->GetString("title").ToString().c_str()));
		}

		dialog.setFixedSize(width, height);

		if (dialog.exec() == QDialog::Accepted) {
			output = CefParseJSON(dialog.result(), JSON_PARSER_ALLOW_TRAILING_COMMAS);

			return true;
		}
	}

	return false;
}

bool StreamElementsGlobalStateManager::DeserializePopupWindow(CefRefPtr<CefValue> input)
{
	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	if (d.get() && d->HasKey("url")) {
		std::string url = d->GetString("url").ToString();
		std::string executeJavaScriptOnLoad;

		bool enableHostApi = d->HasKey("enableHostApi") && d->GetBool("enableHostApi");

		if (d->HasKey("executeJavaScriptOnLoad")) {
			executeJavaScriptOnLoad = d->GetString("executeJavaScriptOnLoad");
		}


		QueueCEFTask([this, url, executeJavaScriptOnLoad, enableHostApi]() {
			CefWindowInfo windowInfo;
			windowInfo.SetAsPopup(0, ""); // Initial title

			CefBrowserSettings cefBrowserSettings;

			cefBrowserSettings.Reset();
			cefBrowserSettings.javascript_close_windows = STATE_ENABLED;
			cefBrowserSettings.local_storage = STATE_ENABLED;

			CefRefPtr<StreamElementsCefClient> cefClient =
				new StreamElementsCefClient(
					executeJavaScriptOnLoad,
					enableHostApi ? new StreamElementsApiMessageHandler() : nullptr,
					nullptr);

			CefRefPtr<CefBrowser> browser =
				CefBrowserHost::CreateBrowserSync(
					windowInfo,
					cefClient,
					url.c_str(),
					cefBrowserSettings,
					nullptr);
		});
	}

	return false;
}

void StreamElementsGlobalStateManager::ReportIssue()
{
	obs_frontend_push_ui_translation(obs_module_get_string);

	StreamElementsReportIssueDialog dialog(mainWindow());

	obs_frontend_pop_ui_translation();

	if (dialog.exec() == QDialog::Accepted) {
	}
}
