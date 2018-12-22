#include "StreamElementsBrowserSourceApiMessageHandler.hpp"
#include "StreamElementsMessageBus.hpp"

#include <mutex>

static std::recursive_mutex s_sync_api_call_mutex;

#define API_HANDLER_BEGIN(name) RegisterIncomingApiCallHandler(name, [](StreamElementsApiMessageHandler*, CefRefPtr<CefProcessMessage> message, CefRefPtr<CefListValue> args, CefRefPtr<CefValue>& result, CefRefPtr<CefBrowser> browser, void (*complete_callback)(void*), void* complete_context) { std::lock_guard<std::recursive_mutex> _api_sync_guard(s_sync_api_call_mutex);
#define API_HANDLER_END() complete_callback(complete_context); });

StreamElementsBrowserSourceApiMessageHandler::StreamElementsBrowserSourceApiMessageHandler()
{

}

StreamElementsBrowserSourceApiMessageHandler::~StreamElementsBrowserSourceApiMessageHandler()
{

}

void StreamElementsBrowserSourceApiMessageHandler::RegisterIncomingApiCallHandlers()
{
	API_HANDLER_BEGIN("broadcastMessage")
		if (args->GetSize()) {
			StreamElementsMessageBus::GetInstance()->NotifyAllMessageListeners(
				StreamElementsMessageBus::DEST_ALL_LOCAL,
				StreamElementsMessageBus::SOURCE_WEB,
				browser->GetMainFrame()->GetURL().ToString(),
				args->GetValue(0));

			result->SetBool(true);
		}
	API_HANDLER_END()
}
