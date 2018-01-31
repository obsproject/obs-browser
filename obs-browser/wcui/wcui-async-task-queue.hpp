#pragma once

#include <util/threading.h>
#include <vector>

class WCUIAsyncTaskQueue
{
private:
	class Task
	{
	public:
		Task(void(*task_proc)(void*), void* args):
			task_proc(task_proc),
			args(args)
		{
		}

		void(*task_proc)(void*);
		void* args;
	};

private:
	std::vector<Task> m_queue;
	pthread_mutex_t m_dispatchLock;
	os_event_t *m_dispatchEvent;
	os_event_t *m_doneEvent;
	pthread_t m_worker_thread;
	bool m_continue_running;

public:
	WCUIAsyncTaskQueue();
	~WCUIAsyncTaskQueue();

	void Enqueue(void(*task_proc)(void*), void* args);
};
