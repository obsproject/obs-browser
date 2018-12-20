#include "browser-panel-internal.hpp"
#include "browser-panel-client.hpp"
#include "cef-headers.hpp"

#include <QWindow>

#include <obs-module.h>
#include <util/threading.h>
#include <util/util.hpp>
#include <util/base.h>
#include <thread>

extern bool QueueCEFTask(std::function<void()> task);
extern "C" void obs_browser_initialize(void);
extern os_event_t *cef_started_event;

/* ------------------------------------------------------------------------- */

CefRefPtr<CefCookieManager> QCefRequestContextHandler::GetCookieManager()
{
	return cm;
}

struct QCefCookieManagerInternal : QCefCookieManager {
	CefRefPtr<CefCookieManager> cm;
	CefRefPtr<CefRequestContextHandler> rch;

	QCefCookieManagerInternal(
			const std::string &storage_path,
			bool persist_session_cookies)
	{
		if (os_event_try(cef_started_event) != 0)
			throw "Browser thread not initialized";

		BPtr<char> path = obs_module_config_path(storage_path.c_str());

		cm = CefCookieManager::CreateManager(
				path.Get(),
				persist_session_cookies,
				nullptr);
		if (!cm)
			throw "Failed to create cookie manager";

		rch = new QCefRequestContextHandler(cm);
	}

	virtual bool DeleteCookies(
			const std::string &url,
			const std::string &name) override
	{
		return cm->DeleteCookies(
				url,
				name,
				nullptr);
	}

	virtual bool SetStoragePath(
			const std::string &storage_path,
			bool persist_session_cookies) override
	{
		return cm->SetStoragePath(
				storage_path,
				persist_session_cookies,
				nullptr);
	}

	virtual bool FlushStore() override
	{
		return cm->FlushStore(nullptr);
	}
};

/* ------------------------------------------------------------------------- */

static void ExecuteOnBrowser(std::function<void()> func, bool async = false)
{
	if (!async) {
		os_event_t *finishedEvent;
		os_event_init(&finishedEvent, OS_EVENT_TYPE_AUTO);
		bool success = QueueCEFTask([&] () {
			func();
			os_event_signal(finishedEvent);
		});
		if (success)
			os_event_wait(finishedEvent);
		os_event_destroy(finishedEvent);
	} else {
		QueueCEFTask(func);
	}
}

QCefWidgetInternal::QCefWidgetInternal(
		QWidget *parent,
		const std::string &url_,
		CefRefPtr<CefRequestContextHandler> rch_)
	: QCefWidget (parent),
	  url        (url_),
	  rch        (rch_)
{
	setAttribute(Qt::WA_PaintOnScreen);
	setAttribute(Qt::WA_StaticContents);
	setAttribute(Qt::WA_NoSystemBackground);
	setAttribute(Qt::WA_OpaquePaintEvent);
	setAttribute(Qt::WA_DontCreateNativeAncestors);
	setAttribute(Qt::WA_NativeWindow);

	setFocusPolicy(Qt::ClickFocus);
}

QCefWidgetInternal::~QCefWidgetInternal()
{
	ExecuteOnBrowser([this] ()
	{
		if (!cefBrowser) {
			return;
		}
		CefRefPtr<CefClient> client =
				cefBrowser->GetHost()->GetClient();
		QCefBrowserClient *bc =
				reinterpret_cast<QCefBrowserClient*>(
					client.get());

		bc->widget = nullptr;
		cefBrowser = nullptr;
	});
}

void QCefWidgetInternal::Init()
{
	QSize size = this->size() * devicePixelRatio();
	WId id = winId();

	bool success = QueueCEFTask([this, size, id] ()
	{
		CefWindowInfo windowInfo;
#ifdef _WIN32
		RECT rect = {0, 0, size.width(), size.height()};
		windowInfo.SetAsChild((HWND)id, rect);
#endif

		CefRefPtr<QCefBrowserClient> browserClient =
			new QCefBrowserClient(this, script);

		CefRefPtr<CefRequestContext> rc;
		if (rch)
			rc = CefRequestContext::CreateContext(
					CefRequestContext::GetGlobalContext(),
					rch);

		CefBrowserSettings cefBrowserSettings;
		cefBrowser = CefBrowserHost::CreateBrowserSync(
				windowInfo,
				browserClient,
				url,
				cefBrowserSettings,
				rc);
	});

	if (success)
		timer.stop();
}

void QCefWidgetInternal::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);

	QSize size = this->size() * devicePixelRatio();

	QueueCEFTask([this, size] ()
	{
		if (!cefBrowser)
			return;
#ifdef _WIN32
		HWND hwnd = cefBrowser->GetHost()->GetWindowHandle();
		SetWindowPos(hwnd, nullptr, 0, 0, size.width(), size.height(),
				SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER);
#endif
	});
}

void QCefWidgetInternal::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);

	if (!cefBrowser) {
		obs_browser_initialize();
		connect(&timer, SIGNAL(timeout()), this, SLOT(Init()));
		timer.start(500);
		Init();
	}
}

QPaintEngine *QCefWidgetInternal::paintEngine() const
{
	return nullptr;
}

void QCefWidgetInternal::setURL(const std::string &url)
{
	if (cefBrowser) {
		cefBrowser->GetMainFrame()->LoadURL(url);
	}
}

void QCefWidgetInternal::setStartupScript(const std::string &script_)
{
	script = script_;
}

/* ------------------------------------------------------------------------- */

struct QCefInternal : QCef {
	virtual bool init_browser(void) override;
	virtual bool initialized(void) override;
	virtual bool wait_for_browser_init(void) override;

	virtual QCefWidget *create_widget(
			QWidget *parent,
			const std::string &url,
			QCefCookieManager *cookie_manager) override;

	virtual QCefCookieManager *create_cookie_manager(
			const std::string &storage_path,
			bool persist_session_cookies) override;

	virtual std::string get_cookie_path(
			const std::string &storage_path) override;
};

bool QCefInternal::init_browser(void)
{
	if (os_event_try(cef_started_event) == 0)
		return true;

	obs_browser_initialize();
	return false;
}

bool QCefInternal::initialized(void)
{
	return os_event_try(cef_started_event) == 0;
}

bool QCefInternal::wait_for_browser_init(void)
{
	return os_event_wait(cef_started_event) == 0;
}

QCefWidget *QCefInternal::create_widget(
		QWidget *parent,
		const std::string &url,
		QCefCookieManager *cm)
{
	QCefCookieManagerInternal *cmi =
		reinterpret_cast<QCefCookieManagerInternal*>(cm);

	return new QCefWidgetInternal(
			parent,
			url,
			cmi ? cmi->rch : nullptr);
}

QCefCookieManager *QCefInternal::create_cookie_manager(
		const std::string &storage_path,
		bool persist_session_cookies)
{
	try {
		return new QCefCookieManagerInternal(
				storage_path,
				persist_session_cookies);
	} catch (const char *error) {
		blog(LOG_ERROR, "Failed to create cookie manager: %s", error);
		return nullptr;
	}
}

std::string QCefInternal::get_cookie_path(
		const std::string &storage_path)
{
	BPtr<char> path = obs_module_config_path(storage_path.c_str());
	std::string string_path = path;
	return string_path;
}

extern "C" EXPORT QCef *obs_browser_create_qcef(void)
{
	return new QCefInternal();
}
