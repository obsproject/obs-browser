#include "wcui-async-task-queue.hpp"

WCUIAsyncTaskQueue::WCUIAsyncTaskQueue():
	m_queue(),
	m_continue_running(true)
{
	os_event_init(&m_dispatchEvent, OS_EVENT_TYPE_AUTO);
	pthread_mutex_init(&m_dispatchLock, nullptr);

	pthread_create(
		&m_worker_thread,
		nullptr,
		[](void* threadArgument)
		{
			WCUIAsyncTaskQueue* self = (WCUIAsyncTaskQueue*)threadArgument;

			while (self->m_continue_running)
			{
				if (os_event_timedwait(self->m_dispatchEvent, 10) != ETIMEDOUT) {
					pthread_mutex_lock(&self->m_dispatchLock);
					while (!self->m_queue.empty()) {
						auto event = self->m_queue[0];

						event();

						if (!self->m_queue.empty()) {
							self->m_queue.erase(self->m_queue.begin());
						}
					}

					pthread_mutex_unlock(&self->m_dispatchLock);
				}
			}

			return (void*)NULL;
		},
		this);
}

WCUIAsyncTaskQueue::~WCUIAsyncTaskQueue()
{
	pthread_mutex_lock(&m_dispatchLock);
	pthread_kill(m_worker_thread, 0);
	pthread_mutex_unlock(&m_dispatchLock);

	os_event_destroy(m_dispatchEvent);
	pthread_mutex_destroy(&m_dispatchLock);
}

void WCUIAsyncTaskQueue::Enqueue(std::function<void()> task)
{
	pthread_mutex_lock(&m_dispatchLock);
	m_queue.push_back(task);
	pthread_mutex_unlock(&m_dispatchLock);

	os_event_signal(m_dispatchEvent);
}
