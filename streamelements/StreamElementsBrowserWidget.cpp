#include "StreamElementsBrowserWidget.hpp"
#include "StreamElementsCefClient.hpp"
#include "StreamElementsApiMessageHandler.hpp"
#include "StreamElementsUtils.hpp"
#include "StreamElementsGlobalStateManager.hpp"

#ifdef USE_QT_LOOP
#include "browser-app.hpp"
#endif

#include <functional>
#include <mutex>
#include <regex>

class BrowserTask : public CefTask {
public:
	std::function<void()> task;

	inline BrowserTask(std::function<void()> task_) : task(task_) {}
	virtual void Execute() override { task(); }

	IMPLEMENT_REFCOUNTING(BrowserTask);
};

static bool QueueCEFTask(std::function<void()> task)
{
	return CefPostTask(TID_UI,
			   CefRefPtr<BrowserTask>(new BrowserTask(task)));
}

/* ========================================================================= */

#include <QVBoxLayout>

StreamElementsBrowserWidget::StreamElementsBrowserWidget(
	QWidget *parent, const char *const url,
	const char *const executeJavaScriptCodeOnLoad,
	const char *const locationArea, const char *const id,
	StreamElementsApiMessageHandler *apiMessageHandler, bool isIncognito)
	: QWidget(parent),
	  m_url(url),
	  m_window_handle(0),
	  m_task_queue("StreamElementsBrowserWidget task queue"),
	  m_executeJavaScriptCodeOnLoad(executeJavaScriptCodeOnLoad == nullptr
						? ""
						: executeJavaScriptCodeOnLoad),
	  m_pendingLocationArea(locationArea == nullptr ? "" : locationArea),
	  m_pendingId(id == nullptr ? "" : id),
	  m_requestedApiMessageHandler(apiMessageHandler),
	  m_isIncognito(isIncognito)
{
	// Create native window
	setAttribute(Qt::WA_NativeWindow);
	// setAttribute(Qt::WA_QuitOnClose, false);

	// This influences docking widget width/height
	//setMinimumWidth(200);
	//setMinimumHeight(200);

	setLayout(new QVBoxLayout());

	QSizePolicy policy;
	policy.setHorizontalPolicy(QSizePolicy::MinimumExpanding);
	policy.setVerticalPolicy(QSizePolicy::MinimumExpanding);
	setSizePolicy(policy);
}

StreamElementsBrowserWidget::~StreamElementsBrowserWidget()
{
	DestroyBrowser();
}

void StreamElementsBrowserWidget::InitBrowserAsync()
{
	if (!!m_cef_browser.get()) {
		return;
	}

	// Make sure InitBrowserAsyncInternal() runs in Qt UI thread
	QMetaObject::invokeMethod(this, "InitBrowserAsyncInternal");
}

std::string StreamElementsBrowserWidget::GetInitialPageURLInternal()
{
	std::string htmlString = LoadResourceString(":/html/loading.html");
	htmlString = std::regex_replace(htmlString, std::regex("\\$\\{URL\\}"),
					m_url);
	std::string base64uri =
		"data:text/html;base64," +
		CefBase64Encode(htmlString.c_str(), htmlString.size())
			.ToString();

	return base64uri;
}

void StreamElementsBrowserWidget::InitBrowserAsyncInternal()
{
	if (!!m_cef_browser.get()) {
		return;
	}

	m_window_handle = (cef_window_handle_t)winId();

	CefUIThreadExecute(
		[this]() {
			std::lock_guard<std::mutex> guard(
				m_create_destroy_mutex);

			if (!!m_cef_browser.get()) {
				return;
			}

			StreamElementsBrowserWidget *self = this;

			// Client area rectangle
			RECT clientRect;

			clientRect.left = 0;
			clientRect.top = 0;
			clientRect.right = self->width();
			clientRect.bottom = self->height();

			// CEF window attributes
			CefWindowInfo windowInfo;
			windowInfo.width = clientRect.right - clientRect.left;
			windowInfo.height = clientRect.bottom - clientRect.top;
			windowInfo.windowless_rendering_enabled = false;
			windowInfo.SetAsChild(self->m_window_handle,
					      clientRect);
			//windowInfo.SetAsPopup(0, CefString("Window Name"));

			CefBrowserSettings cefBrowserSettings;

			cefBrowserSettings.Reset();
			cefBrowserSettings.javascript_close_windows =
				STATE_DISABLED;
			cefBrowserSettings.local_storage = STATE_ENABLED;
			cefBrowserSettings.databases = STATE_ENABLED;
			cefBrowserSettings.web_security = STATE_ENABLED;
			cefBrowserSettings.webgl = STATE_ENABLED;

			if (m_requestedApiMessageHandler == nullptr) {
				m_requestedApiMessageHandler =
					new StreamElementsApiMessageHandler();
			}

			CefRefPtr<StreamElementsCefClient> cefClient =
				new StreamElementsCefClient(
					m_executeJavaScriptCodeOnLoad,
					m_requestedApiMessageHandler,
					new StreamElementsBrowserWidget_EventHandler(
						this),
					StreamElementsMessageBus::DEST_UI);

			cefClient->SetLocationArea(m_pendingLocationArea);
			cefClient->SetContainerId(m_pendingId);

			CefRefPtr<CefRequestContext> cefRequestContext =
				StreamElementsGlobalStateManager::GetInstance()
					->GetCookieManager()
					->GetCefRequestContext();

				if (m_isIncognito)
			{
				CefRequestContextSettings
					cefRequestContextSettings;

				cefRequestContextSettings.Reset();

				///
				// CefRequestContext with empty cache path = incognito mode
				//
				// Docs:
				// https://magpcss.org/ceforum/viewtopic.php?f=6&t=10508
				// https://magpcss.org/ceforum/apidocs3/projects/(default)/CefRequestContext.html#GetCachePath()
				//
				CefString(
					&cefRequestContextSettings.cache_path) =
					"";

				cefRequestContext =
					CefRequestContext::CreateContext(
						cefRequestContextSettings,
						nullptr);
			}

			m_cef_browser = CefBrowserHost::CreateBrowserSync(
				windowInfo, cefClient,
				GetInitialPageURLInternal(), cefBrowserSettings,
#if CHROME_VERSION_BUILD >= 3770
				CefRefPtr<CefDictionaryValue>(),
#endif
				cefRequestContext);

			UpdateBrowserSize();
		},
		true);
}

void StreamElementsBrowserWidget::CefUIThreadExecute(std::function<void()> func,
						     bool async)
{
	if (!async) {
#ifdef USE_QT_LOOP
		if (QThread::currentThread() == qApp->thread()) {
			func();
			return;
		}
#endif
		os_event_t *finishedEvent;
		os_event_init(&finishedEvent, OS_EVENT_TYPE_AUTO);
		bool success = QueueCEFTask([&]() {
			func();

			os_event_signal(finishedEvent);
		});
		if (success) {
			/* fixes an issue on windows where blocking the main
			 * UI thread can cause CEF SendMessage calls to lock
			 * up */
			int code = ETIMEDOUT;
			while (code == ETIMEDOUT) {
#ifdef USE_QT_LOOP
				QCoreApplication::processEvents();
#endif
				code = os_event_timedwait(finishedEvent, 5);
			}
		}

		os_event_destroy(finishedEvent);
	} else {
#ifdef USE_QT_LOOP
		QueueBrowserTask(m_cef_browser,
				 [this, func](CefRefPtr<CefBrowser>) {
					 func();
				 });
#else
		QueueCEFTask([this, func]() { func(); });
#endif
	}
}

std::string StreamElementsBrowserWidget::GetStartUrl()
{
	return m_url;
}

std::string StreamElementsBrowserWidget::GetCurrentUrl()
{
	SYNC_ACCESS();

	if (!m_cef_browser.get()) {
		return m_url;
	}

	CefRefPtr<CefFrame> mainFrame = m_cef_browser->GetMainFrame();

	if (!mainFrame.get()) {
		return m_url;
	}

	std::string url = mainFrame->GetURL().ToString();

	if (url.substr(0, 5) == "data:") {
		// "data:" scheme means we're still at the loading page URL.
		//
		// Use the initially specified URL in that case.
		url = m_url;
	}

	return url;
}

std::string StreamElementsBrowserWidget::GetExecuteJavaScriptCodeOnLoad()
{
	return m_executeJavaScriptCodeOnLoad;
}

bool StreamElementsBrowserWidget::BrowserHistoryCanGoBack()
{
	if (!m_cef_browser.get()) {
		return false;
	}

	return m_cef_browser->CanGoBack();
}

bool StreamElementsBrowserWidget::BrowserHistoryCanGoForward()
{
	if (!m_cef_browser.get()) {
		return false;
	}

	return m_cef_browser->CanGoForward();
}

void StreamElementsBrowserWidget::BrowserHistoryGoBack()
{
	if (!BrowserHistoryCanGoBack()) {
		return;
	}

	m_cef_browser->GoBack();
}

void StreamElementsBrowserWidget::BrowserHistoryGoForward()
{
	if (!m_cef_browser.get()) {
		return;
	}

	m_cef_browser->GoForward();
}

void StreamElementsBrowserWidget::BrowserReload(bool ignoreCache)
{
	if (!m_cef_browser.get()) {
		return;
	}

	if (ignoreCache) {
		m_cef_browser->ReloadIgnoreCache();
	} else {
		m_cef_browser->Reload();
	}
}

void StreamElementsBrowserWidget::BrowserLoadInitialPage()
{
	if (!m_cef_browser.get()) {
		return;
	}

	m_cef_browser->GetMainFrame()->LoadURL(GetInitialPageURLInternal());
}
