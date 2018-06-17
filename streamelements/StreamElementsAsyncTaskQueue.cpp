#include "StreamElementsAsyncTaskQueue.hpp"

StreamElementsAsyncTaskQueue::StreamElementsAsyncTaskQueue(const char* const label):
	m_label(label),
	m_queue(),
	m_continue_running(true)
{
	// Init events
	os_event_init(&m_dispatchEvent, OS_EVENT_TYPE_AUTO);
	os_event_init(&m_doneEvent, OS_EVENT_TYPE_AUTO);

	// Init queue access mutex
	pthread_mutex_init(&m_dispatchLock, nullptr);

	// Create worker thread
	pthread_create(
		&m_worker_thread,
		nullptr,
		[](void* threadArgument)
	{
		StreamElementsAsyncTaskQueue* self = (StreamElementsAsyncTaskQueue*)threadArgument;

		if (self->m_label.empty())
		{
			// Set worker thread name for debugging
			os_set_thread_name("StreamElementsAsyncTaskQueue: worker");
		}
		else {
			// Set worker thread name for debugging
			os_set_thread_name(self->m_label.c_str());
		}

		// While we should continue running
		while (self->m_continue_running)
		{
			// Wait for queue task submitted event
			if (os_event_timedwait(self->m_dispatchEvent, 10) != ETIMEDOUT) {
				// Check if we should continue running again
				if (self->m_continue_running)
				{
					// Lock queue access mutex
					pthread_mutex_lock(&self->m_dispatchLock);

					bool has_task = false;
					StreamElementsAsyncTaskQueue::Task task(NULL, NULL);

					// If there are tasks in the queue
					if (!self->m_queue.empty()) {
						self->m_asyncBusy = true;

						// Get first task in the queue
						task = self->m_queue[0];

						// Remove first task from the queue
						self->m_queue.erase(self->m_queue.begin());

						// Mark that we have a task for execution
						has_task = true;
					}

					// Unlock queue access mutex
					pthread_mutex_unlock(&self->m_dispatchLock);

					// Execute task callback with task args
					if (has_task) {
						task.task_proc(task.args);
					}

					// Lock queue access mutex
					pthread_mutex_lock(&self->m_dispatchLock);

					self->m_asyncBusy = false;

					// Unlock queue access mutex
					pthread_mutex_unlock(&self->m_dispatchLock);
				}
			}
		}

		// Signal the worked thread has stopped running
		os_event_signal(self->m_doneEvent);

		return (void*)NULL;
	},
		this);
}

StreamElementsAsyncTaskQueue::~StreamElementsAsyncTaskQueue()
{
	if (m_continue_running)
	{
		// Signal worker thread should stop running
		m_continue_running = false;

		// Signal worker thread should wake up
		os_event_signal(m_dispatchEvent);
	}

	// Unlock queue access mutex
	// pthread_mutex_unlock(&m_dispatchLock);

	// Wait until worker thread has stopped running
	os_event_wait(m_doneEvent);

	// Destroy events
	os_event_destroy(m_doneEvent);
	os_event_destroy(m_dispatchEvent);

	// Destroy queue access mutex
	pthread_mutex_destroy(&m_dispatchLock);
}

void StreamElementsAsyncTaskQueue::Enqueue(void(*task_proc)(void*), void* args)
{
	// Lock queue access mutex
	pthread_mutex_lock(&m_dispatchLock);

	// Add task item to the queue
	m_queue.push_back(Task(task_proc, args));

	// Unlock queue access mutex
	pthread_mutex_unlock(&m_dispatchLock);

	// Signal the worker thread to wake up
	os_event_signal(m_dispatchEvent);
}

void StreamElementsAsyncTaskQueue::RemoveAll()
{
	// Lock queue access mutex
	pthread_mutex_lock(&m_dispatchLock);

	// Clear pending tasks
	m_queue.clear();

	// Unlock queue access mutex
	pthread_mutex_unlock(&m_dispatchLock);
}
