#include "StreamElementsPerformanceHistoryTracker.hpp"

static const size_t BUF_SIZE = 60;

static uint64_t FromFileTime(const FILETIME& ft) {
	ULARGE_INTEGER uli = { 0 };
	uli.LowPart = ft.dwLowDateTime;
	uli.HighPart = ft.dwHighDateTime;
	return uli.QuadPart;
}

StreamElementsPerformanceHistoryTracker::StreamElementsPerformanceHistoryTracker()
{
	os_event_init(&m_quit_event, OS_EVENT_TYPE_AUTO);
	os_event_init(&m_done_event, OS_EVENT_TYPE_AUTO);

	std::thread thread([this]() {
		do {
			// CPU
			FILETIME idleTime;
			FILETIME kernelTime;
			FILETIME userTime;

			if (::GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
				static bool hasSavedStartingValues = false;
				static uint64_t savedIdleTime;
				static uint64_t savedKernelTime;
				static uint64_t savedUserTime;

				if (!hasSavedStartingValues) {
					savedIdleTime = FromFileTime(idleTime);
					savedKernelTime = FromFileTime(kernelTime);
					savedUserTime = FromFileTime(userTime);

					hasSavedStartingValues = true;
				}

				const uint64_t SECOND_PART = 10000000L;
				const uint64_t MS_PART = SECOND_PART / 1000L;

				uint64_t idleInt = FromFileTime(idleTime) - savedIdleTime;
				uint64_t kernelInt = FromFileTime(kernelTime) - savedKernelTime;
				uint64_t userInt = FromFileTime(userTime) - savedUserTime;

				uint64_t idleMs = idleInt / MS_PART;
				uint64_t kernelMs = kernelInt / MS_PART;
				uint64_t userMs = userInt / MS_PART;

				uint64_t idleSec = idleMs / (uint64_t)1000;
				uint64_t kernelSec = kernelMs / (uint64_t)1000;
				uint64_t userSec = userMs / (uint64_t)1000;

				uint64_t idleMod = idleMs % (uint64_t)1000;
				uint64_t kernelMod = kernelMs % (uint64_t)1000;
				uint64_t userMod = userMs % (uint64_t)1000;

				seconds_t idleRat = idleSec + ((seconds_t)idleMod / 1000.0);
				seconds_t kernelRat = kernelSec + ((seconds_t)kernelMod / 1000.0);
				seconds_t userRat = userSec + ((seconds_t)userMod / 1000.0);

				// https://msdn.microsoft.com/en-us/84f674e7-536b-4ae0-b523-6a17cb0a1c17
				// lpKernelTime [out, optional]
				// A pointer to a FILETIME structure that receives the amount of time that
				// the system has spent executing in Kernel mode (including all threads in
				// all processes, on all processors)
				//
				// >>> This time value also includes the amount of time the system has been idle.
				//

				cpu_usage_t item;

				item.idleSeconds = idleRat;
				item.totalSeconds =  kernelRat + userRat;
				item.busySeconds = kernelRat + userRat - idleRat;

				std::lock_guard<std::recursive_mutex> guard(m_mutex);
				m_cpu_usage.emplace_back(item);

				while (m_cpu_usage.size() > BUF_SIZE) {
					m_cpu_usage.erase(m_cpu_usage.begin());
				}
			}

			// Memory
			memory_usage_t mem;

			mem.dwLength = sizeof(mem);

			if (GlobalMemoryStatusEx(&mem)) {
				std::lock_guard<std::recursive_mutex> guard(m_mutex);

				m_memory_usage.emplace_back(mem);

				while (m_memory_usage.size() > BUF_SIZE) {
					m_memory_usage.erase(m_memory_usage.begin());
				}
			}
		} while (0 != os_event_timedwait(m_quit_event, 60000));

		os_event_signal(m_done_event);
	});

	thread.detach();
}

StreamElementsPerformanceHistoryTracker::~StreamElementsPerformanceHistoryTracker()
{
	os_event_signal(m_quit_event);
	os_event_wait(m_done_event);

	os_event_destroy(m_done_event);
	os_event_destroy(m_quit_event);
}

std::vector<StreamElementsPerformanceHistoryTracker::memory_usage_t> StreamElementsPerformanceHistoryTracker::getMemoryUsageSnapshot()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	return std::vector<memory_usage_t>(m_memory_usage);
}

std::vector<StreamElementsPerformanceHistoryTracker::cpu_usage_t> StreamElementsPerformanceHistoryTracker::getCpuUsageSnapshot()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	return std::vector<cpu_usage_t>(m_cpu_usage);
}
