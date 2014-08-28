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

	void DestroyBrowser(const int browserIdentifier);
	void TickBrowser(const int browserIdentifier);

	void AddListener(const int browserIdentifier,
		std::shared_ptr<BrowserListener> browserListener);
	void RemoveListener(const int browserIdentifier);

	void PushEvent(std::function<void()> event);

private:
	bool threadAlive;
	os_event_t dispatchEvent;
	pthread_t managerThread;
	pthread_mutex_t dispatchLock;

	std::map<int, std::shared_ptr<BrowserListener>> listenerMap;
	std::map<int, CefRefPtr<CefBrowser> > browserMap;
	std::vector<std::function<void()>> queue;
};