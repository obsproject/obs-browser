#include <util/platform.h>
#include <include/cef_version.h>

#include "browser-manager-base.hpp"
#include "browser-task.hpp"
#include "browser-app.hpp"
#include "browser-settings.hpp"
#include "browser-scheme.hpp"
#include "browser-client.hpp"
#include "browser-render-handler.hpp"
#include "browser-load-handler.hpp"
#include "browser-obs-bridge-base.hpp"

BrowserManager::BrowserManager()
: pimpl(new BrowserManager::Impl())
{}

BrowserManager::~BrowserManager()
{}

void BrowserManager::Startup()
{
	pimpl->Startup();
}

void BrowserManager::Shutdown()
{
	pimpl->Shutdown();
}

int BrowserManager::CreateBrowser(
	const BrowserSettings &browserSettings,
	const std::shared_ptr<BrowserListener> &browserListener)
{
	return pimpl->CreateBrowser(browserSettings, browserListener);
}

void BrowserManager::DestroyBrowser(int browserIdentifier)
{
	pimpl->DestroyBrowser(browserIdentifier);
}

void BrowserManager::TickBrowser(int browserIdentifier)
{	
	pimpl->TickBrowser(browserIdentifier);
}

void BrowserManager::SendMouseClick(int browserIdentifier,
		const struct obs_mouse_event *event, int32_t type,
		bool mouse_up, uint32_t click_count)
{
	pimpl->SendMouseClick(browserIdentifier, event, type, mouse_up, 
			click_count);
}

void BrowserManager::SendMouseMove(int browserIdentifier,
		const struct obs_mouse_event *event, bool mouseLeave)
{
	pimpl->SendMouseMove(browserIdentifier, event, mouseLeave);
}

void BrowserManager::SendMouseWheel(int browserIdentifier,
		const struct obs_mouse_event *event, int xDelta,
		int yDelta)
{
	pimpl->SendMouseWheel(browserIdentifier, event, xDelta, yDelta);
}

void BrowserManager::SendFocus(int browserIdentifier, bool focus)
{
	pimpl->SendFocus(browserIdentifier, focus);
}

void BrowserManager::SendKeyClick(int browserIdentifier,
		const struct obs_key_event *event, bool keyUp)
{
	pimpl->SendKeyClick(browserIdentifier, event, keyUp);
}

void BrowserManager::ExecuteVisiblityJSCallback(int browserIdentifier, bool visible)
{
	pimpl->ExecuteVisiblityJSCallback(browserIdentifier, visible);
}

void BrowserManager::ExecuteActiveJSCallback(int browserIdentifier, bool active)
{
	pimpl->ExecuteActiveJSCallback(browserIdentifier, active);
}

void BrowserManager::ExecuteSceneChangeJSCallback(const char *name)
{
	pimpl->ExecuteSceneChangeJSCallback(name);
}

void BrowserManager::RefreshPageNoCache(int browserIdentifier)
{
	pimpl->RefreshPageNoCache(browserIdentifier);
}

/**
	Sends JSON Data about an OBS event to be executed as a DOM event.
	The jsonString is already encoded so that we can pass it across the process boundary that cef-isolation creates.

	@param eventName the name of the DOM event that we will fire
	@param jsonString A json encoded string that will be accessable from the detail field of the event object
	@return
*/
void BrowserManager::DispatchJSEvent(const char *eventName, const char *jsonString)
{
	pimpl->DispatchJSEvent(eventName, jsonString);
}

BrowserManager::Impl::Impl()
{
	os_event_init(&dispatchEvent, OS_EVENT_TYPE_AUTO);
	os_event_init(&startupEvent, OS_EVENT_TYPE_MANUAL);
	pthread_mutex_init(&dispatchLock, nullptr);
}

BrowserManager::Impl::~Impl()
{
	pthread_mutex_destroy(&dispatchLock);
	os_event_destroy(startupEvent);
	os_event_destroy(dispatchEvent);
}

int BrowserManager::Impl::CreateBrowser(
		const BrowserSettings &browserSettings,
		const std::shared_ptr<BrowserListener> &browserListener)
{
	int browserIdentifier = 0;
	os_event_t *createdEvent;
	os_event_init(&createdEvent, OS_EVENT_TYPE_AUTO);

	os_event_wait(startupEvent);

	BrowserOBSBridge *browserOBSBridge = new BrowserOBSBridgeBase();

	CefPostTask(TID_UI, BrowserTask::newTask(
			[&] 
	{
		CefRefPtr<BrowserRenderHandler> renderHandler(
				new BrowserRenderHandler(browserSettings.width, 
				browserSettings.height, browserListener));

		CefRefPtr<BrowserLoadHandler> loadHandler(
				new BrowserLoadHandler(browserSettings.css));

		CefRefPtr<BrowserClient> browserClient(
				new BrowserClient(renderHandler, loadHandler, browserOBSBridge));

		CefWindowInfo windowInfo;
#if CHROME_VERSION_BUILD < 3071
		windowInfo.transparent_painting_enabled = true;
#endif
		windowInfo.width = browserSettings.width;
		windowInfo.height = browserSettings.height;
		windowInfo.windowless_rendering_enabled = true;

		CefBrowserSettings cefBrowserSettings;
		cefBrowserSettings.windowless_frame_rate = browserSettings.fps;

		CefRefPtr<CefBrowser> browser =
				CefBrowserHost::CreateBrowserSync(windowInfo, 
				browserClient, browserSettings.url, 
				cefBrowserSettings, nullptr);

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
BrowserManager::Impl::DestroyBrowser(int browserIdentifier)
{
	if (browserMap.count(browserIdentifier) > 0) {
		CefRefPtr<CefBrowser> browser = browserMap[browserIdentifier];
		os_event_t *closeEvent;
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
BrowserManager::Impl::TickBrowser(int browserIdentifier)
{}

void BrowserManager::Impl::ExecuteOnBrowser(int browserIdentifier, 
		std::function<void(CefRefPtr<CefBrowser>)> f, 
		bool async)
{
	if (browserMap.count(browserIdentifier) > 0) {
		CefRefPtr<CefBrowser> browser = browserMap[browserIdentifier];
		if (async) {
			CefPostTask(TID_UI, BrowserTask::newTask([&] {
				f(browser);
			}));
		} else {
			os_event_t *finishedEvent;
			os_event_init(&finishedEvent, OS_EVENT_TYPE_AUTO);
			CefPostTask(TID_UI, BrowserTask::newTask([&] {
				f(browser);
				os_event_signal(finishedEvent);
			}));
			os_event_wait(finishedEvent);
			os_event_destroy(finishedEvent);
		}
	}
}

void BrowserManager::Impl::ExecuteOnAllBrowsers(
	std::function<void(CefRefPtr<CefBrowser>)> f, 
			bool async)
{
	for (auto& x: browserMap) {
		CefRefPtr<CefBrowser> browser = x.second;
		if (async) {
			CefPostTask(TID_UI, BrowserTask::newTask([&] {
				f(browser);
			}));
		} else {
			os_event_t *finishedEvent;
			os_event_init(&finishedEvent, OS_EVENT_TYPE_AUTO);
			CefPostTask(TID_UI, BrowserTask::newTask([&] {
				f(browser);
				os_event_signal(finishedEvent);
			}));
			os_event_wait(finishedEvent);
			os_event_destroy(finishedEvent);
		}
	}
}

void BrowserManager::Impl::SendMouseClick(int browserIdentifier,
	const struct obs_mouse_event *event, int32_t type,
	bool mouse_up, uint32_t click_count)
{
	ExecuteOnBrowser(browserIdentifier, [&](CefRefPtr<CefBrowser> b) 
	{
		CefMouseEvent e;
		e.modifiers = event->modifiers;
		e.x = event->x;
		e.y = event->y;
		CefBrowserHost::MouseButtonType buttonType =
			(CefBrowserHost::MouseButtonType)type;
		b->GetHost()->SendMouseClickEvent(e, buttonType, mouse_up, 
			click_count);
	});
}

void BrowserManager::Impl::SendMouseMove(int browserIdentifier,
	const struct obs_mouse_event *event, bool mouseLeave)
{
	ExecuteOnBrowser(browserIdentifier, [&](CefRefPtr<CefBrowser> b)
	{
		CefMouseEvent e;
		e.modifiers = event->modifiers;
		e.x = event->x;
		e.y = event->y;
		b->GetHost()->SendMouseMoveEvent(e, mouseLeave);
	});
}

void BrowserManager::Impl::SendMouseWheel(int browserIdentifier,
	const struct obs_mouse_event *event, int xDelta,
	int yDelta)
{
	ExecuteOnBrowser(browserIdentifier, [&](CefRefPtr<CefBrowser> b)
	{
		CefMouseEvent e;
		e.modifiers = event->modifiers;
		e.x = event->x;
		e.y = event->y;
		b->GetHost()->SendMouseWheelEvent(e, xDelta, yDelta);
	});
}

void BrowserManager::Impl::SendFocus(int browserIdentifier, bool focus)
{
	ExecuteOnBrowser(browserIdentifier, [&](CefRefPtr<CefBrowser> b)
	{
		b->GetHost()->SendFocusEvent(focus);
	});
}

void BrowserManager::Impl::SendKeyClick(int browserIdentifier,
	const struct obs_key_event *event, bool keyUp)
{
	ExecuteOnBrowser(browserIdentifier, [&](CefRefPtr<CefBrowser> b)
	{

		CefKeyEvent e;
		e.windows_key_code = event->native_vkey;
		e.native_key_code = 0;

		e.type = keyUp ? KEYEVENT_KEYUP : KEYEVENT_RAWKEYDOWN;
		
		if (event->text) {
			char16 *characters;
			os_utf8_to_wcs_ptr(event->text, 0, &characters);
			if (characters) {
				e.character = characters[0];
				bfree(characters);
			}
		}
		
		//e.native_key_code = event->native_vkey;
		e.modifiers = event->modifiers;
		
		b->GetHost()->SendKeyEvent(e);
		if (event->text && !keyUp) {
			e.type = KEYEVENT_CHAR;
			// Figure out why this works
			e.windows_key_code = e.character;
			e.character = 0;
			b->GetHost()->SendKeyEvent(e);
		}

	});
}

void BrowserManager::Impl::ExecuteVisiblityJSCallback(int browserIdentifier, bool visible)
{
	ExecuteOnBrowser(browserIdentifier, [&](CefRefPtr<CefBrowser> b)
	{
		CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("Visibility");
		CefRefPtr<CefListValue> args = msg->GetArgumentList();
		args->SetBool(0, visible);
		b->SendProcessMessage(PID_RENDERER, msg);
	});
}

void BrowserManager::Impl::ExecuteActiveJSCallback(int browserIdentifier, bool active)
{
	ExecuteOnBrowser(browserIdentifier, [&](CefRefPtr<CefBrowser> b)
	{
		CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("Active");
		CefRefPtr<CefListValue> args = msg->GetArgumentList();
		args->SetBool(0, active);
		b->SendProcessMessage(PID_RENDERER, msg);
	});
}

void BrowserManager::Impl::ExecuteSceneChangeJSCallback(const char *name)
{
	ExecuteOnAllBrowsers([&](CefRefPtr<CefBrowser> b)
	{
		CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("SceneChange");
		CefRefPtr<CefListValue> args = msg->GetArgumentList();
		args->SetString(0, name);
		b->SendProcessMessage(PID_RENDERER, msg);
	});
}

void BrowserManager::Impl::RefreshPageNoCache(int browserIdentifier)
{
	ExecuteOnBrowser(browserIdentifier, [&](CefRefPtr<CefBrowser> b)
	{
		b->ReloadIgnoreCache();
	});
}

void BrowserManager::Impl::DispatchJSEvent(const char *eventName, const char *jsonString)
{
	ExecuteOnAllBrowsers([&](CefRefPtr<CefBrowser> b)
	{
		CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("DispatchJSEvent");
		CefRefPtr<CefListValue> args = msg->GetArgumentList();
		args->SetString(0, eventName);
		args->SetString(1, jsonString);
		b->SendProcessMessage(PID_RENDERER, msg);
	});
}

void
BrowserManager::Impl::Startup() 
{
	pthread_mutex_lock(&dispatchLock);
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
	pthread_mutex_unlock(&dispatchLock);
		
	return;
}

void BrowserManager::Impl::Shutdown() 
{
	os_event_t *shutdown_event;
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

void *BrowserManager::Impl::browserManagerEntry(void* threadArgument)
{
	BrowserManager::Impl *browserManager =
			static_cast<BrowserManager::Impl *>(threadArgument);
	browserManager->BrowserManagerEntry();
	return nullptr;
}

void BrowserManager::Impl::PushEvent(std::function<void()> event)
{
	pthread_mutex_lock(&dispatchLock);
	queue.push_back(event);
	pthread_mutex_unlock(&dispatchLock);
	os_event_signal(dispatchEvent);
}

std::string getBootstrap()
{
	std::string modulePath(BrowserManager::Instance()->GetModulePath());
	std::string parentPath(modulePath.substr(0,
			modulePath.find_last_of('/') + 1));
#ifdef _WIN32	
	return parentPath + "/cef-bootstrap.exe";
#else
	return parentPath + "/cef-bootstrap";
#endif
}

void BrowserManager::Impl::BrowserManagerEntry()
{
	std::string bootstrapPath = getBootstrap();
	bool thread_exit = false;
	PushEvent([this] {
		CefMainArgs mainArgs;
		CefSettings settings;
		settings.log_severity = LOGSEVERITY_VERBOSE;
		settings.windowless_rendering_enabled = true;
		settings.no_sandbox = true;
		CefString(&settings.cache_path).FromASCII(obs_module_config_path(""));
		CefString(&settings.browser_subprocess_path) = getBootstrap();
		CefRefPtr<BrowserApp> app(new BrowserApp());
		CefExecuteProcess(mainArgs, app, nullptr);
		CefInitialize(mainArgs, settings, app, nullptr);
		CefRegisterSchemeHandlerFactory("http", "absolute", new BrowserSchemeHandlerFactory());
		os_event_signal(startupEvent);
		CefRunMessageLoop();
		CefShutdown();
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

BrowserManager *BrowserManager::Instance()
{
	if (instance == nullptr) {
		instance = new BrowserManager();
	}
	return instance;
}

void BrowserManager::DestroyInstance()
{
	if (instance != nullptr) {
		delete instance;
	}
	instance = nullptr;

}
