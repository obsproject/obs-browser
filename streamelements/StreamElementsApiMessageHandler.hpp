#pragma once

#include "StreamElementsBrowserMessageHandler.hpp"

class StreamElementsApiMessageHandler:
	public StreamElementsBrowserMessageHandler
{
public:
	StreamElementsApiMessageHandler() {}
	virtual ~StreamElementsApiMessageHandler() {}

public:
	virtual bool OnProcessMessageReceived(
		CefRefPtr<CefBrowser> browser,
#if CHROME_VERSION_BUILD >= 3770
		CefRefPtr<CefFrame> frame,
#endif
		CefProcessId source_process,
		CefRefPtr<CefProcessMessage> message,
		const long cefClientId) override;

	void setInitialHiddenState(bool isHidden) { m_initialHiddenState = isHidden; }

protected:
	virtual void RegisterIncomingApiCallHandlers();

	typedef void (*incoming_call_handler_t)(StreamElementsApiMessageHandler*, CefRefPtr<CefProcessMessage> message, CefRefPtr<CefListValue> args, CefRefPtr<CefValue>& result, CefRefPtr<CefBrowser> browser, void (*complete)(void*), void* context);

	void RegisterIncomingApiCallHandler(std::string id, incoming_call_handler_t handler);

private:
	std::map<std::string, incoming_call_handler_t> m_apiCallHandlers;
	bool m_initialHiddenState = false;

	void RegisterIncomingApiCallHandlersInternal(CefRefPtr<CefBrowser> browser);
	void RegisterApiPropsInternal(CefRefPtr<CefBrowser> browser);
	void DispatchHostReadyEventInternal(CefRefPtr<CefBrowser> browser);
	void DispatchEventInternal(CefRefPtr<CefBrowser> browser, std::string event, std::string eventArgsJson);

public:
	IMPLEMENT_REFCOUNTING(StreamElementsApiMessageHandler);
};
