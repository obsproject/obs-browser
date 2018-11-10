#include "StreamElementsMessageBus.hpp"

StreamElementsMessageBus* StreamElementsMessageBus::s_instance = nullptr;

const StreamElementsMessageBus::message_destination_filter_flags_t StreamElementsMessageBus::DEST_ALL = 0xFFFFFFFFUL;
const StreamElementsMessageBus::message_destination_filter_flags_t StreamElementsMessageBus::DEST_UI = 1;
const StreamElementsMessageBus::message_destination_filter_flags_t StreamElementsMessageBus::DEST_WORKER = 2;
const StreamElementsMessageBus::message_destination_filter_flags_t StreamElementsMessageBus::DEST_BROWSER_SOURCE = 4;

const char* const StreamElementsMessageBus::SOURCE_WEB = "web";

StreamElementsMessageBus::StreamElementsMessageBus()
{

}

StreamElementsMessageBus::~StreamElementsMessageBus()
{

}

StreamElementsMessageBus* StreamElementsMessageBus::GetInstance()
{
	if (!s_instance) {
		s_instance = new StreamElementsMessageBus();
	}

	return s_instance;
}

void StreamElementsMessageBus::AddBrowserListener(CefRefPtr<CefBrowser> browser, message_destination_filter_flags_t type)
{
	std::lock_guard<std::recursive_mutex> guard(m_browser_list_mutex);

	m_browser_list.emplace(browser, type);
}

void StreamElementsMessageBus::RemoveBrowserListener(CefRefPtr<CefBrowser> browser)
{
	std::lock_guard<std::recursive_mutex> guard(m_browser_list_mutex);

	m_browser_list.erase(browser);
}

void StreamElementsMessageBus::NotifyAllBrowserListeners(
	message_destination_filter_flags_t types,
	std::string source,
	std::string sourceAddress,
	std::string event,
	CefRefPtr<CefValue> payload)
{
	std::lock_guard<std::recursive_mutex> guard(m_browser_list_mutex);

	if (m_browser_list.empty()) {
		return;
	}

	CefRefPtr<CefValue> root = CefValue::Create();
	CefRefPtr<CefDictionaryValue> rootDict = CefDictionaryValue::Create();

	rootDict->SetString("scope", "broadcast");
	rootDict->SetString("source", source);
	rootDict->SetString("sourceAddress", sourceAddress);
	rootDict->SetValue("message", payload);

	root->SetDictionary(rootDict);

	CefString payloadJson = CefWriteJSON(root, JSON_WRITER_DEFAULT);

	for (auto kv : m_browser_list) {
		auto browser = kv.first;

		if (kv.second & types) {
			CefRefPtr<CefProcessMessage> msg =
				CefProcessMessage::Create("DispatchJSEvent");
			CefRefPtr<CefListValue> args = msg->GetArgumentList();

			args->SetString(0, event);
			args->SetString(1, payloadJson);

			browser->SendProcessMessage(PID_RENDERER, msg);
		}
	}
}
