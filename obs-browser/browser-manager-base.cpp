#include "browser-manager-base.hpp"
#include "browser-task.hpp"
#include "browser-app.hpp"
#include "browser-settings.hpp"
#include "browser-client.hpp"
#include "browser-render-handler.hpp"


BrowserManager::BrowserManager()
: pimpl(new BrowserManager::Impl())
{}

BrowserManager::~BrowserManager()
{}

void
BrowserManager::Startup()
{
	pimpl->Startup();
}

void
BrowserManager::Shutdown()
{
	pimpl->Shutdown();
}

int BrowserManager::CreateBrowser(
	const BrowserSettings &browserSettings,
	const std::shared_ptr<BrowserListener> &browserListener)
{
	return pimpl->CreateBrowser(browserSettings, browserListener);
}

void BrowserManager::DestroyBrowser(const int browserIdentifier)
{
	pimpl->DestroyBrowser(browserIdentifier);
}

void BrowserManager::TickBrowser(const int browserIdentifier)
{	
	pimpl->TickBrowser(browserIdentifier);
}

BrowserManager::Impl::Impl()
{
	os_event_init(&dispatchEvent, OS_EVENT_TYPE_AUTO);
	pthread_mutex_init(&dispatchLock, nullptr);
}

BrowserManager::Impl::~Impl()
{
	os_event_init(&dispatchEvent, OS_EVENT_TYPE_AUTO);
	pthread_mutex_init(&dispatchLock, nullptr);
}

int 
BrowserManager::Impl::CreateBrowser(
	const BrowserSettings &browserSettings,
	const std::shared_ptr<BrowserListener> &browserListener)
{
	int browserIdentifier = 0;
	os_event_t createdEvent;
	os_event_init(&createdEvent, OS_EVENT_TYPE_AUTO);

	CefPostTask(TID_UI, BrowserTask::newTask(
		[&, browserSettings, browserListener] 
	{

		CefRefPtr<BrowserRenderHandler> renderHandler(
			new BrowserRenderHandler(browserSettings.width, 
			browserSettings.height, browserListener));

		CefRefPtr<BrowserClient> browserClient(
			new BrowserClient(renderHandler)); 

		CefWindowInfo windowInfo;
		windowInfo.transparent_painting_enabled = true;
		windowInfo.width = browserSettings.width;
		windowInfo.height = browserSettings.height;
		windowInfo.windowless_rendering_enabled = true;

		CefBrowserSettings cefBrowserSettings;
		cefBrowserSettings.windowless_frame_rate = browserSettings.fps;

		CefRefPtr<CefBrowser> browser =
			CefBrowserHost::CreateBrowserSync(windowInfo, 
			browserClient, browserSettings.url, cefBrowserSettings, 
			nullptr);

		if (browser != nullptr) {
			browserIdentifier = browser->GetIdentifier();
			browserMap[browserIdentifier] = browser;
		}
		os_event_signal(createdEvent);
	}));

	os_event_wait(createdEvent);
	os_event_destroy(createdEvent);
	return browserIdentifier;
}

void 
BrowserManager::Impl::DestroyBrowser(const int browserIdentifier)
{
	if (browserMap.count(browserIdentifier) > 0) {
		CefRefPtr<CefBrowser> browser = browserMap[browserIdentifier];
		os_event_t closeEvent;
		os_event_init(&closeEvent, OS_EVENT_TYPE_AUTO);
		CefPostTask(TID_UI, BrowserTask::newTask([&, browser] 
		{
			browser->GetHost()->CloseBrowser(true);
			os_event_signal(closeEvent);
		}));
		os_event_wait(closeEvent);
		os_event_destroy(closeEvent);
		browserMap.erase(browserIdentifier);
	}
}

void 
BrowserManager::Impl::TickBrowser(const int browserIdentifier)
{}


void
BrowserManager::Impl::Startup() {
	int ret = pthread_create(&managerThread, nullptr,
		browserManagerEntry, this);
	
	if (ret != 0) {
		blog(LOG_ERROR,
			"BrowserManager: failed to create browser "
			"manager thread.");
		threadAlive = false;
	}
	else {
		threadAlive = true;
	}
		
	return;
}

void
BrowserManager::Impl::Shutdown() {
	os_event_t shutdown_event;
	os_event_init(&shutdown_event, OS_EVENT_TYPE_AUTO);

	// post the task
	CefPostTask(TID_UI, BrowserTask::newTask([] {
		CefQuitMessageLoop();
	}));

	// this event will then get processed and shut down the dispatcher loop
	PushEvent([this, shutdown_event] {
		threadAlive = false;
		os_event_signal(shutdown_event);
	});

	os_event_wait(shutdown_event);
	os_event_destroy(shutdown_event);
	return;
}

void *
BrowserManager::Impl::browserManagerEntry(void* threadArgument)
{
	BrowserManager::Impl *browserManager =
		static_cast<BrowserManager::Impl *>(threadArgument);
	browserManager->BrowserManagerEntry();
	return nullptr;
}

void
BrowserManager::Impl::PushEvent(std::function<void()> event)
{
	pthread_mutex_lock(&dispatchLock);
	queue.push_back(event);
	pthread_mutex_unlock(&dispatchLock);
	os_event_signal(dispatchEvent);
}

void
BrowserManager::Impl::BrowserManagerEntry()
{
	bool thread_exit = false;
	PushEvent([] {
		CefMainArgs mainArgs;
		CefSettings settings;
		settings.log_severity = LOGSEVERITY_VERBOSE;
		settings.windowless_rendering_enabled = true;
		settings.no_sandbox = true;
		CefString(&settings.browser_subprocess_path) = "C:\\dev\\obs-studio\\build\\rundir\\Debug\\obs-plugins\\64bit\\cef-bootstrap.exe";
		CefRefPtr<BrowserApp> app(new BrowserApp());
		CefExecuteProcess(mainArgs, app, nullptr);
		CefInitialize(mainArgs, settings, app, nullptr);
		CefRunMessageLoop();
	});

	while (true) {
		
		if (os_event_timedwait(dispatchEvent, 10) != ETIMEDOUT) {
			pthread_mutex_lock(&dispatchLock);
			while (!queue.empty()) {
				auto event = queue[0];
				event();
				queue.erase(queue.begin());
			}
			thread_exit = !threadAlive;
			pthread_mutex_unlock(&dispatchLock);
			if (thread_exit) {
				return;
			}
		}
	}
}

static BrowserManager *instance;

BrowserManager *
BrowserManager::Instance()
{
	if (instance == nullptr) {
		instance = new BrowserManager();
	}
	return instance;
}

void
BrowserManager::DestroyInstance()
{
	if (instance != nullptr) {
		delete instance;
	}
	instance = nullptr;

}