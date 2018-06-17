#pragma once

#include <obs.h>
#include <util/threading.h>
#include <vector>

///
// Asynchronous task queue processed by a worker thread
//
class StreamElementsAsyncTaskQueue
{
private:
	///
	// Internal class, describes a single task in the queue
	//
	class Task
	{
	public:
		///
		// Class constructor
		//
		// @param task_proc	Task callback
		// @param args		Task callback argument
		//
		Task(void(*task_proc)(void*), void* args) :
			task_proc(task_proc),
			args(args)
		{
		}

		// Task callback
		void(*task_proc)(void*);

		// Task callback arg
		void* args;
	};

private:
	// Task queue
	std::vector<Task> m_queue;

	// Task queue lock mutex
	pthread_mutex_t m_dispatchLock;

	// This event will be raised when an item is added to the queue
	os_event_t *m_dispatchEvent;

	// This event will be raised when the worker thread stops
	os_event_t *m_doneEvent;

	// Worker thread
	pthread_t m_worker_thread;

	// The worker thread will be running while the value of this variable is true
	bool m_continue_running;

	// Thread label
	std::string m_label;

	// Is currently busy?
	bool m_asyncBusy = false;

public:
	///
	// Class constructor
	//
	StreamElementsAsyncTaskQueue(const char* const label = nullptr);

	///
	// Class destructor
	//
	~StreamElementsAsyncTaskQueue();

	///
	// Add task to the processing queue
	//
	// @param task_proc	Task callback
	// @param args		Task argument
	//
	void Enqueue(void(*task_proc)(void*), void* args);

	///
	// Remove all pending tasks
	//
	void RemoveAll();

	///
	// Is busy?
	//
	bool IsBusy()
	{
		// Lock queue access mutex
		pthread_mutex_lock(&m_dispatchLock);

		bool result = m_asyncBusy;

		// Unlock queue access mutex
		pthread_mutex_unlock(&m_dispatchLock);

		return result;
	}
};
