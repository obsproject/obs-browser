#include "wcui-async-task-queue.hpp"

WCUIAsyncTaskQueue::WCUIAsyncTaskQueue():
	m_queue(),
	m_continue_running(true)
{
	os_event_init(&m_dispatchEvent, OS_EVENT_TYPE_AUTO);
	os_event_init(&m_doneEvent, OS_EVENT_TYPE_AUTO);

	pthread_mutex_init(&m_dispatchLock, nullptr);

	pthread_create(
		&m_worker_thread,
		nullptr,
		[](void* threadArgument)
		{
			WCUIAsyncTaskQueue* self = (WCUIAsyncTaskQueue*)threadArgument;

			os_set_thread_name("WCUIAsyncTaskQueue: worker");

			while (self->m_continue_running)
			{
				if (os_event_timedwait(self->m_dispatchEvent, 10) != ETIMEDOUT) {
					if (self->m_continue_running)
					{
						pthread_mutex_lock(&self->m_dispatchLock);
						while (!self->m_queue.empty()) {
							auto task = self->m_queue[0];

							if (!self->m_queue.empty()) {
								self->m_queue.erase(self->m_queue.begin());
							}

							task.task_proc(task.args);
						}

						pthread_mutex_unlock(&self->m_dispatchLock);
					}
				}
			}

			os_event_signal(self->m_doneEvent);

			return (void*)NULL;
		},
		this);
}

WCUIAsyncTaskQueue::~WCUIAsyncTaskQueue()
{
	pthread_mutex_lock(&m_dispatchLock);

	m_continue_running = false;

	os_event_signal(m_dispatchEvent);

	pthread_mutex_unlock(&m_dispatchLock);

	os_event_wait(m_doneEvent);

	os_event_destroy(m_doneEvent);
	os_event_destroy(m_dispatchEvent);
	pthread_mutex_destroy(&m_dispatchLock);
}

void WCUIAsyncTaskQueue::Enqueue(void(*task_proc)(void*), void* args)
{
	pthread_mutex_lock(&m_dispatchLock);
	m_queue.push_back(Task(task_proc, args));
	pthread_mutex_unlock(&m_dispatchLock);

	os_event_signal(m_dispatchEvent);
}
