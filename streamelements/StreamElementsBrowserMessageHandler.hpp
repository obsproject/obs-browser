#pragma once

#include "../cef-headers.hpp"

class StreamElementsBrowserMessageHandler:
	public CefBaseRefCounted
{
public:
	virtual bool OnProcessMessageReceived(
		CefRefPtr<CefBrowser> browser,
		CefProcessId source_process,
		CefRefPtr<CefProcessMessage> message) = 0;
};
