#pragma once

#include <obs-module.h>

#include <include/cef_app.h>
#include <include/cef_task.h>

#include <vector>
#include <functional>
#include <util/threading.h>

#include "browser-manager.hpp"

class BrowserManager::Impl
{

public:
	Impl();
	~Impl();

private:

	static void *browserManagerEntry(void* threadArguments);
	void BrowserManagerEntry();


public:

	void Startup();
	void Shutdown();
	
	int CreateBrowser(
		const BrowserSettings &browserSettings,
		const std::shared_ptr<BrowserListener> &browserListener);

	void DestroyBrowser(int browserIdentifier);
	void TickBrowser(int browserIdentifier);

	void SendMouseClick(int browserIdentifier,
		const struct obs_mouse_event *event, int32_t type,
		bool mouse_up, uint32_t click_count);
	void SendMouseMove(int browserIdentifier,
		const struct obs_mouse_event *event, bool mouseLeave);
	void SendMouseWheel(int browserIdentifier,
		const struct obs_mouse_event *event, int xDelta,
		int yDelta);
	void SendFocus(int browserIdentifier, bool focus);
	void SendKeyClick(int browserIdentifier,
		const struct obs_key_event *event, bool keyUp);

	void AddListener(const int browserIdentifier,
		std::shared_ptr<BrowserListener> browserListener);
	void RemoveListener(const int browserIdentifier);

	void PushEvent(std::function<void()> event);

	void ExecuteVisiblityJSCallback(int browserIdentifier, bool visible);

	void ExecuteActiveJSCallback(int browserIdentifier, bool active);

	void ExecuteSceneChangeJSCallback(const char *name);

	void RefreshPageNoCache(int browserIdentifier);
	
	void DispatchJSEvent(const char *eventName, const char *jsonString);

private: 
	void ExecuteOnBrowser(int browserIdentifier, 
			std::function<void(CefRefPtr<CefBrowser>)> f, 
			bool async = false);

	void ExecuteOnAllBrowsers(std::function<void(CefRefPtr<CefBrowser>)> f, 
			bool async = false);

private:
	bool threadAlive;
	os_event_t *dispatchEvent;
	os_event_t *startupEvent;
	pthread_t managerThread;
	pthread_mutex_t dispatchLock;

	std::map<int, std::shared_ptr<BrowserListener>> listenerMap;
	std::map<int, CefRefPtr<CefBrowser> > browserMap;
	std::vector<std::function<void()>> queue;
};
