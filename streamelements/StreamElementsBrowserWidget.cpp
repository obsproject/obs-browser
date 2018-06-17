#include "StreamElementsBrowserWidget.hpp"
#include "StreamElementsCefClient.hpp"
#include "StreamElementsApiMessageHandler.hpp"
#include <functional>

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

/* ========================================================================= */

#include <QVBoxLayout>

StreamElementsBrowserWidget::StreamElementsBrowserWidget(
	QWidget* parent,
	const char* const url,
	const char* const executeJavaScriptCodeOnLoad,
	const char* const locationArea,
	const char* const id):
	QWidget(parent),
	m_url(url),
	m_window_handle(0),
	m_task_queue("StreamElementsBrowserWidget task queue"),
	m_executeJavaScriptCodeOnLoad(executeJavaScriptCodeOnLoad == nullptr ? "" : executeJavaScriptCodeOnLoad),
	m_pendingLocationArea(locationArea == nullptr ? "" : locationArea),
	m_pendingId(id == nullptr ? "" : id)
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

#ifdef ___NO_SUCH_THING___
void StreamElementsBrowserWidget::OnAfterCreated(CefRefPtr<CefBrowser> browser)
{
	// Must be executed on the UI thread.
	//CEF_REQUIRE_UI_THREAD();
	
	//if (!m_cef_browser.get()) {
		// Keep a reference to the main browser.
		//m_cef_browser = browser;
		//m_BrowserId = browser->GetIdentifier();
	//}

	// Keep track of how many browsers currently exist.
	//m_BrowserCount++;
}

bool StreamElementsBrowserWidget::DoClose(CefRefPtr<CefBrowser> browser)
{
	// Must be executed on the UI thread.
	//CEF_REQUIRE_UI_THREAD();

	// Closing the main window requires special handling. See the DoClose()
	// documentation in the CEF header for a detailed description of this
	// process.
	//if (m_BrowserId == browser->GetIdentifier()) {
	//	// Set a flag to indicate that the window close should be allowed.
	//	m_bIsClosing = true;
	//}

	// Allow the close. For windowed browsers this will result in the OS close
	// event being sent.

	m_cef_browser = NULL;

	return false;
}

void StreamElementsBrowserWidget::OnBeforeClose(CefRefPtr<CefBrowser> browser)
{
	// Must be executed on the UI thread.
	//CEF_REQUIRE_UI_THREAD();

	//if (m_BrowserId == browser->GetIdentifier()) {
	//	// Free the browser pointer so that the browser can be destroyed.
	//	m_Browser = NULL;
	//}

	//if (--m_BrowserCount == 0) {
	//	// All browser windows have closed. Quit the application message loop.
	//	CefQuitMessageLoop();
	//}
}
#endif

void StreamElementsBrowserWidget::InitBrowserAsync()
{
	// Make sure InitBrowserAsyncInternal() runs in Qt UI thread
	QMetaObject::invokeMethod(this, "InitBrowserAsyncInternal");
}

void StreamElementsBrowserWidget::InitBrowserAsyncInternal()
{
	if (!!m_cef_browser.get())
		return;

	m_window_handle = (cef_window_handle_t)winId();

	CefUIThreadExecute([this]() {
		StreamElementsBrowserWidget* self = this;

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
		windowInfo.SetAsChild(self->m_window_handle, clientRect);
		//windowInfo.SetAsPopup(0, CefString("Window Name"));

		CefBrowserSettings cefBrowserSettings;

		cefBrowserSettings.Reset();
		cefBrowserSettings.javascript_close_windows = STATE_DISABLED;
		cefBrowserSettings.local_storage = STATE_DISABLED;

		CefRefPtr<StreamElementsCefClient> cefClient =
			new StreamElementsCefClient(
				m_executeJavaScriptCodeOnLoad,
				new StreamElementsApiMessageHandler());

		cefClient->SetLocationArea(m_pendingLocationArea);
		cefClient->SetContainerId(m_pendingId);

		m_cef_browser =
			CefBrowserHost::CreateBrowserSync(
				windowInfo,
				cefClient,
				m_url.c_str(),
				cefBrowserSettings,
				nullptr);

		UpdateBrowserSize();
	}, true);
}

void StreamElementsBrowserWidget::CefUIThreadExecute(std::function<void()> func, bool async)
{
	if (!async) {
		os_event_t *finishedEvent;
		os_event_init(&finishedEvent, OS_EVENT_TYPE_AUTO);
		bool success = QueueCEFTask([&]() {
			func();

			os_event_signal(finishedEvent);
		});
		if (success)
			os_event_wait(finishedEvent);
		os_event_destroy(finishedEvent);
	}
	else {
		QueueCEFTask([this, func]() {
			func();
		});
	}
}

std::string StreamElementsBrowserWidget::GetCurrentUrl()
{
	if (!m_cef_browser.get()) {
		return std::string();
	}

	return m_cef_browser->GetMainFrame()->GetURL().ToString();
}


std::string StreamElementsBrowserWidget::GetExecuteJavaScriptCodeOnLoad()
{
	return m_executeJavaScriptCodeOnLoad;
}
