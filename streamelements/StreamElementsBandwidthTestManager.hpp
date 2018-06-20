#pragma once

#include "StreamElementsBandwidthTestClient.hpp"

#include "../cef-headers.hpp"

#include <mutex>

class StreamElementsBandwidthTestManager
{
public:
	StreamElementsBandwidthTestManager();
	virtual ~StreamElementsBandwidthTestManager();

	bool BeginBandwidthTest(CefRefPtr<CefValue> settingsValue, CefRefPtr<CefValue> serversValue, CefRefPtr<CefBrowser> browser);
	CefRefPtr<CefDictionaryValue> EndBandwidthTest(CefRefPtr<CefValue> options);
	CefRefPtr<CefDictionaryValue> GetBandwidthTestStatus();

private:
	std::mutex m_mutex;

	bool m_isTestInProgress;
	StreamElementsBandwidthTestClient* m_client = new StreamElementsBandwidthTestClient();

	std::vector<StreamElementsBandwidthTestClient::Server> m_last_test_servers;
	std::vector<StreamElementsBandwidthTestClient::Result> m_last_test_results;
};
