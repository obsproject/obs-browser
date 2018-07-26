#include "browser-panel-internal.hpp"
#include "browser-panel-client.hpp"
#include "cef-headers.hpp"

#include <QWindow>

#include <util/threading.h>
#include <util/base.h>
#include <thread>

extern bool QueueCEFTask(std::function<void()> task);
extern "C" void obs_browser_initialize(void);

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

QCefWidgetInternal::QCefWidgetInternal(QWidget *parent, const std::string &url_)
	: QCefWidget (parent),
	  url        (url_)
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
		RECT rc = {0, 0, size.width(), size.height()};
		windowInfo.SetAsChild((HWND)id, rc);
#endif

		CefRefPtr<QCefBrowserClient> browserClient =
			new QCefBrowserClient(this);

		CefBrowserSettings cefBrowserSettings;
		cefBrowser = CefBrowserHost::CreateBrowserSync(
				windowInfo,
				browserClient,
				url,
				cefBrowserSettings,
				nullptr);
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

/* ------------------------------------------------------------------------- */

extern "C" EXPORT QCefWidget *obs_browser_create_widget(QWidget *parent,
		const std::string &url)
{
	return new QCefWidgetInternal(parent, url);
}
