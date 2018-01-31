#pragma once

#include <util/threading.h>
#include <vector>
#include <functional>

class WCUIAsyncTaskQueue
{
private:
	std::vector<std::function<void()>> m_queue;
	pthread_mutex_t m_dispatchLock;
	os_event_t *m_dispatchEvent;
	pthread_t m_worker_thread;
	bool m_continue_running;

public:
	WCUIAsyncTaskQueue();
	~WCUIAsyncTaskQueue();

	void Enqueue(std::function<void()> event);
};
