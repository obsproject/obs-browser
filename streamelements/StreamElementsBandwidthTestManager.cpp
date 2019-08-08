#include "StreamElementsBandwidthTestManager.hpp"
#include "StreamElementsUtils.hpp"

static void DispatchJSEvent(CefRefPtr<CefBrowser> browser, const char *eventName, const char *jsonString)
{
	CefRefPtr<CefProcessMessage> msg =
		CefProcessMessage::Create("DispatchJSEvent");
	CefRefPtr<CefListValue> args = msg->GetArgumentList();

	args->SetString(0, eventName);
	args->SetString(1, jsonString ? jsonString : "null");
	SendBrowserProcessMessage(browser, PID_RENDERER, msg);
}

StreamElementsBandwidthTestManager::StreamElementsBandwidthTestManager()
{
	m_isTestInProgress = false;
	m_client = new StreamElementsBandwidthTestClient();
}

StreamElementsBandwidthTestManager::~StreamElementsBandwidthTestManager()
{
	delete m_client;
}

bool StreamElementsBandwidthTestManager::BeginBandwidthTest(CefRefPtr<CefValue> settingsValue, CefRefPtr<CefValue> serversValue, CefRefPtr<CefBrowser> browser)
{
	std::lock_guard<std::mutex> guard(m_mutex);

	if (m_isTestInProgress) {
		return false;
	}

	m_last_test_results.clear();

	CefRefPtr<CefDictionaryValue> settings = settingsValue->GetDictionary();
	CefRefPtr<CefListValue> servers = serversValue->GetList();

	if (settings.get() && servers.get() && servers->GetSize()) {
		if (settings->HasKey("maxBitsPerSecond") && settings->HasKey("serverTestDurationSeconds")) {
			int maxBitsPerSecond = settings->GetInt("maxBitsPerSecond");
			int serverTestDurationSeconds = settings->GetInt("serverTestDurationSeconds");

			m_last_test_servers.clear();

			for (int i = 0; i < servers->GetSize(); ++i) {
				CefRefPtr<CefDictionaryValue> server =
					servers->GetValue(i)->GetDictionary();

				if (server.get() && server->HasKey("url") && server->HasKey("streamKey")) {
					StreamElementsBandwidthTestClient::Server item;

					item.url = server->GetString("url");
					item.streamKey = server->GetString("streamKey");

					if (server->HasKey("useAuth") && server->GetBool("useAuth")) {
						item.useAuth = true;

						if (server->HasKey("authUsername")) {
							item.authUsername = server->GetString("authUsername");
						}

						if (server->HasKey("authPassword")) {
							item.authPassword = server->GetString("authPassword");
						}
					}

					m_last_test_servers.push_back(item);
				}
			}

			if (m_last_test_servers.size()) {
				m_isTestInProgress = true;

				// Signal test started
				DispatchJSEvent(browser, "hostBandwidthTestStarted", nullptr);

				struct local_context {
					StreamElementsBandwidthTestManager* self;
					CefRefPtr<CefBrowser> browser;
				};

				local_context* context = new local_context();

				context->self = this;
				context->browser = browser;

				m_client->TestMultipleServersBitsPerSecondAsync(
					m_last_test_servers,
					maxBitsPerSecond,
					nullptr,
					serverTestDurationSeconds,
					[](std::vector<StreamElementsBandwidthTestClient::Result>* results, void* data) {
						local_context* context = (local_context*)data;

						std::lock_guard<std::mutex> guard(context->self->m_mutex);

						// Copy
						context->self->m_last_test_results = *results;

						// Signal test progress
						DispatchJSEvent(context->browser, "hostBandwidthTestProgress", nullptr);
					},
					[](std::vector<StreamElementsBandwidthTestClient::Result>* results, void* data) {
						local_context* context = (local_context*)data;

						std::lock_guard<std::mutex> guard(context->self->m_mutex);

						// Copy
						context->self->m_last_test_results = *results;

						context->self->m_isTestInProgress = false;

						// Signal test completed
						DispatchJSEvent(context->browser, "hostBandwidthTestCompleted", nullptr);

						delete context;
					},
					context);
			}
		}
	}

	return m_isTestInProgress;
}

CefRefPtr<CefDictionaryValue> StreamElementsBandwidthTestManager::EndBandwidthTest(CefRefPtr<CefValue> options)
{
	bool stopIfRunning = false;

	if (options.get()) {
		stopIfRunning = options->GetBool();
	}

	if (stopIfRunning) {
		m_client->CancelAll();
	}

	return GetBandwidthTestStatus();
}

CefRefPtr<CefDictionaryValue> StreamElementsBandwidthTestManager::GetBandwidthTestStatus()
{
	std::lock_guard<std::mutex> guard(m_mutex);

	CefRefPtr<CefDictionaryValue> resultDictionary = CefDictionaryValue::Create();

	resultDictionary->SetBool("isRunning", m_isTestInProgress);

	// Build servers list
	{
		CefRefPtr<CefValue> value = CefValue::Create();
		CefRefPtr<CefListValue> list = CefListValue::Create();
		value->SetList(list);

		for (auto server : m_last_test_servers) {
			CefRefPtr<CefValue> itemValue = CefValue::Create();
			CefRefPtr<CefDictionaryValue> item = CefDictionaryValue::Create();
			itemValue->SetDictionary(item);

			item->SetString("url", server.url);
			item->SetString("streamKey", server.streamKey);
			item->SetBool("useAuth", server.useAuth);
			item->SetString("authUsername", server.authUsername);

			list->SetValue(list->GetSize(), itemValue);
		}

		resultDictionary->SetValue("servers", value);
	}


	// Build results list
	{
		CefRefPtr<CefValue> value = CefValue::Create();
		CefRefPtr<CefListValue> list = CefListValue::Create();
		value->SetList(list);

		for (auto testResult : m_last_test_results) {
			CefRefPtr<CefValue> itemValue = CefValue::Create();
			CefRefPtr<CefDictionaryValue> item = CefDictionaryValue::Create();
			itemValue->SetDictionary(item);

			item->SetBool("success", testResult.success);
			item->SetBool("wasCancelled", testResult.cancelled);
			item->SetString("serverUrl", testResult.serverUrl);
			item->SetString("streamKey", testResult.streamKey);
			item->SetInt("connectTimeMilliseconds", testResult.connectTimeMilliseconds);

			list->SetValue(list->GetSize(), itemValue);
		}

		resultDictionary->SetValue("results", value);
	}

	return resultDictionary;
}

