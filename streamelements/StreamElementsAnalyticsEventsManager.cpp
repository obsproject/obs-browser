#include "StreamElementsAnalyticsEventsManager.hpp"
#include "StreamElementsGlobalStateManager.hpp"
#include "StreamElementsUtils.hpp"

#include <obs.h>
#include <util/platform.h>
#include <string>
#include <codecvt>

// convert wstring to UTF-8 string
static std::string wstring_to_utf8(const std::wstring& str)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
	return myconv.to_bytes(str);
}

StreamElementsAnalyticsEventsManager::StreamElementsAnalyticsEventsManager(size_t numWorkers)
{
	uint64_t now = os_gettime_ns();
	m_startTime = now;
	m_prevEventTime = now;
	m_appId = StreamElementsConfig::GetInstance()->GetHeapAnalyticsAppId();
	m_sessionId = CreateGloballyUniqueIdString();
	m_identity = GetComputerSystemUniqueId();

	m_taskConsumersKeepRunning = true;

	for (size_t i = 0; i < numWorkers; ++i) {
		m_taskConsumers.emplace_back(std::thread([this]() {
			task_queue_item_t task;

			while (m_taskConsumersKeepRunning || m_taskQueue.size_approx()) {
				if (m_taskQueue.wait_dequeue_timed(task, std::chrono::milliseconds(100))) {
					task();
				}
			}
		}));
	}
}

StreamElementsAnalyticsEventsManager::~StreamElementsAnalyticsEventsManager()
{
	m_taskConsumersKeepRunning = false;

	for (size_t i = 0; i < m_taskConsumers.size(); ++i) {
		if (m_taskConsumers[i].joinable()) {
			m_taskConsumers[i].join();
		}
	}

	m_taskConsumers.clear();
}

void StreamElementsAnalyticsEventsManager::Enqueue(task_queue_item_t task)
{
	m_taskQueue.enqueue(task);
}

void StreamElementsAnalyticsEventsManager::AddRawEvent(const char* eventName, json11::Json::object propertiesJson)
{
	json11::Json::object props = propertiesJson;

	uint64_t now = os_gettime_ns();
	uint64_t secondsSinceStart = (now - m_startTime) / (uint64_t)1000000000L;
	uint64_t secondsSincePrevEvent = (now - m_prevEventTime) / (uint64_t)1000000000L;
	m_prevEventTime = now;

	props["appSessionId"] = m_sessionId;
	props["eventReportedBy"] = "obsPlugin";
	props["secondsSinceStart"] = (int)secondsSinceStart;
	props["secondsSincePreviousEvent"] = (int)secondsSincePrevEvent;
	props["cefVersion"] = GetCefVersionString();
	props["pluginVersion"] = GetStreamElementsPluginVersionString();
	props["obsVersion"] = obs_get_version_string();

	json11::Json json = json11::Json::object{
		{ "app_id", m_appId },
		{ "event", eventName },
		{ "identity", m_identity.c_str() },
		{ "properties", props }
	};

	std::string httpRequestBody = json.dump();

	Enqueue([=]() {
		bool result = HttpPost(
			"https://heapanalytics.com/api/track",
			"application/json",
			(void*)httpRequestBody.c_str(),
			httpRequestBody.size(),
			nullptr,
			nullptr);
	});
}
