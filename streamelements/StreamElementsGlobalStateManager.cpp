#include "StreamElementsGlobalStateManager.hpp"
#include "StreamElementsApiMessageHandler.hpp"
#include "StreamElementsUtils.hpp"
#include "Version.hpp"

#include "base64/base64.hpp"

#include <util/threading.h>

#include <QPushButton>

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

		context->self->m_widgetManager = new StreamElementsBrowserWidgetManager(context->obs_main_window);
		context->self->m_menuManager = new StreamElementsMenuManager(context->obs_main_window);
		context->self->m_bwTestManager = new StreamElementsBandwidthTestManager();
		context->self->m_outputSettingsManager = new StreamElementsOutputSettingsManager();

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
			sprintf(version_buf, "OBS.Live version %d.%d.%d.%d powered by StreamElements",
				(int)((STREAMELEMENTS_PLUGIN_VERSION % 1000000000000L) / 10000000000L),
				(int)((STREAMELEMENTS_PLUGIN_VERSION % 10000000000L) / 100000000L),
				(int)((STREAMELEMENTS_PLUGIN_VERSION % 100000000L) / 1000000L),
				(int)(STREAMELEMENTS_PLUGIN_VERSION % 1000000L));

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

		bool isOnBoarding = false;

		if (StreamElementsConfig::GetInstance()->GetStreamElementsPluginVersion() != STREAMELEMENTS_PLUGIN_VERSION) {
			blog(LOG_INFO, "obs-browser: init: set on-boarding mode due to configuration version mismatch");

			isOnBoarding = true;
		}
		else if (StreamElementsConfig::GetInstance()->GetStartupFlags() & StreamElementsConfig::STARTUP_FLAGS_ONBOARDING_MODE) {
			blog(LOG_INFO, "obs-browser: init: set on-boarding mode due to start-up flags");

			isOnBoarding = true;
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
	}, &context);

	QtPostTask([](void* /*data*/) {
		// Update visible state
		StreamElementsGlobalStateManager::GetInstance()->GetMenuManager()->Update();
	}, this);

	m_initialized = true;
	m_persistStateEnabled = true;
}

void StreamElementsGlobalStateManager::Shutdown()
{
	if (!m_initialized) {
		return;
	}

	FlushCookiesSync();

	QtExecSync([](void* data) -> void {
		StreamElementsGlobalStateManager* self = (StreamElementsGlobalStateManager*)data;

		delete self->m_outputSettingsManager;
		delete self->m_bwTestManager;
		delete self->m_widgetManager;
		delete self->m_menuManager;
	}, this);

	m_initialized = false;
}

void StreamElementsGlobalStateManager::StartOnBoardingUI()
{
	std::string onBoardingURL = GetCommandLineOptionValue("streamelements-onboarding-url");

	if (!onBoardingURL.size()) {
		onBoardingURL = "https://obs.streamelements.com/welcome";
	}

	GetWidgetManager()->HideNotificationBar();
	GetWidgetManager()->RemoveAllDockWidgets();
	GetWidgetManager()->DestroyCurrentCentralBrowserWidget();
	GetWidgetManager()->PushCentralBrowserWidget(onBoardingURL.c_str(), nullptr);

	StreamElementsConfig::GetInstance()->SetStartupFlags(StreamElementsConfig::STARTUP_FLAGS_ONBOARDING_MODE);

	QtPostTask([](void*) -> void {
		StreamElementsGlobalStateManager::GetInstance()->GetMenuManager()->Update();
		StreamElementsGlobalStateManager::GetInstance()->PersistState();
	}, nullptr);
}

void StreamElementsGlobalStateManager::Reset(bool deleteAllCookies)
{
	if (deleteAllCookies) {
		if (!CefCookieManager::GetGlobalManager(NULL)->DeleteCookies(
			CefString(""), // URL
			CefString(""), // Cookie name
			nullptr)) {    // On-complete callback
			blog(LOG_ERROR, "CefCookieManager::GetGlobalManager(NULL)->DeleteCookies() failed.");
		}
	}

	StartOnBoardingUI();
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

	GetWidgetManager()->SerializeDockingWidgets(dockingWidgetsState);
	GetWidgetManager()->SerializeNotificationBar(notificationBarState);

	rootDictionary->SetValue("dockingBrowserWidgets", dockingWidgetsState);
	rootDictionary->SetValue("notificationBar", notificationBarState);

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

	if (dockingWidgetsState.get()) {
		blog(LOG_INFO, "obs-browser: state: restoring docking widgets: %s", CefWriteJSON(dockingWidgetsState, JSON_WRITER_DEFAULT).ToString().c_str());
		GetWidgetManager()->DeserializeDockingWidgets(dockingWidgetsState);
	}

	if (notificationBarState.get()) {
		blog(LOG_INFO, "obs-browser: state: restoring notification bar: %s", CefWriteJSON(notificationBarState, JSON_WRITER_DEFAULT).ToString().c_str());
		GetWidgetManager()->DeserializeNotificationBar(notificationBarState);
	}

	auto geometry = rootDictionary->GetValue("geometry");
	auto windowState = rootDictionary->GetValue("windowState");

	if (geometry.get() && geometry->GetType() == VTYPE_STRING) {
		blog(LOG_INFO, "obs-browser: state: restoring geometry: %s", geometry->GetString());

		mainWindow()->restoreGeometry(
			QByteArray::fromStdString(base64_decode(geometry->GetString().ToString())));
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
					enableHostApi ? new StreamElementsApiMessageHandler() : nullptr);

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

