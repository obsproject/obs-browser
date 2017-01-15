#include <util/platform.h>

#include "browser-manager-base.hpp"
#include "browser-task.hpp"
#include "browser-app.hpp"
#include "browser-settings.hpp"
#include "browser-scheme.hpp"
#include "browser-client.hpp"
#include "browser-render-handler.hpp"
#include "browser-load-handler.hpp"
#include "browser-obs-bridge-base.hpp"
#include <chrono>
#include <thread>

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

//////////////////////////////////////////////////////////////////////////
// BrowserManager::Impl
BrowserManager::Impl::Impl() {}

BrowserManager::Impl::~Impl() {}

int BrowserManager::Impl::CreateBrowser(
		const BrowserSettings &browserSettings,
		const std::shared_ptr<BrowserListener> &browserListener)
{
	int browserIdentifier = 0;
	std::mutex createMutex;
	std::condition_variable createEvent;

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
		windowInfo.transparent_painting_enabled = true;
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
		createEvent.notify_all();
	}));

	std::unique_lock<std::mutex> createLock(createMutex);
	createEvent.wait(createLock);
	return browserIdentifier;
}

void 
BrowserManager::Impl::DestroyBrowser(int browserIdentifier)
{
	if (browserMap.count(browserIdentifier) > 0) {
		CefRefPtr<CefBrowser> browser = browserMap[browserIdentifier];
		std::mutex closeMutex;
		std::condition_variable closeEvent;
		CefPostTask(TID_UI, BrowserTask::newTask([&, browser]
		{
			browser->GetHost()->CloseBrowser(true);
			closeEvent.notify_all();
		}));
		closeEvent.wait(std::unique_lock<std::mutex>(closeMutex));
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
			std::mutex finishedMutex;
			std::condition_variable finishedEvent;
			CefPostTask(TID_UI, BrowserTask::newTask([&] {
				f(browser);
				finishedEvent.notify_all();
			}));
			finishedEvent.wait(std::unique_lock<std::mutex>(finishedMutex));
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
			std::mutex finishedMutex;
			std::condition_variable finishedEvent;
			CefPostTask(TID_UI, BrowserTask::newTask([&] {
				f(browser);
				finishedEvent.notify_all();
			}));
			finishedEvent.wait(std::unique_lock<std::mutex>(finishedMutex));
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

void BrowserManager::Impl::Startup()
{
	// Initialize CEF
	CefMainArgs mainArgs;
	CefSettings settings;
	settings.log_severity = LOGSEVERITY_VERBOSE;
	settings.windowless_rendering_enabled = true;
	settings.no_sandbox = false;
	settings.multi_threaded_message_loop = true;
	CefString(&settings.cache_path).FromASCII(obs_module_config_path(""));
	CefString(&settings.browser_subprocess_path) = getBootstrap();
	CefRefPtr<BrowserApp> app(new BrowserApp());
	CefExecuteProcess(mainArgs, app, nullptr);
	CefInitialize(mainArgs, settings, app, nullptr);
	CefRegisterSchemeHandlerFactory("http", "absolute", new BrowserSchemeHandlerFactory());

	// Initialize event queue.
	{
		std::lock_guard<std::mutex> lock(events.mutex);
		events.bQuit = false;
		events.thread = std::thread(QueueThreadMain, this);
	}


	return;
}

void BrowserManager::Impl::Shutdown()
{
	// Quit Event Thread
	events.bQuit = true;
	events.condvar.notify_all();
	events.thread.join();

	// Shut down CEF
	CefShutdown();
	return;
}

void BrowserManager::Impl::PushEvent(std::function<void()> event)
{
	std::lock_guard<std::mutex> lock(events.mutex);
	events.queue.push(event);
	events.condvar.notify_one();
}

void BrowserManager::Impl::QueueThreadMain(void* data)
{
	auto obj = static_cast<BrowserManager::Impl*>(data);
	obj->QueueThreadLocalMain();
}

void BrowserManager::Impl::QueueThreadLocalMain()
{
	std::unique_lock<std::mutex> ulock(events.mutex);
	do {
		events.condvar.wait(ulock);
		while (events.queue.size() > 0) {
			auto v = events.queue.front();
			v();
			events.queue.pop();
		}
	} while (events.bQuit == false);
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
