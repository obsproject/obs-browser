#pragma once

#include "../cef-headers.hpp"

class StreamElementsBrowserMessageHandler:
	public CefBaseRefCounted
{
public:
	virtual bool OnProcessMessageReceived(
		CefRefPtr<CefBrowser> browser,
#if CHROME_VERSION_BUILD >= 3770
		CefRefPtr<CefFrame> frame,
#endif
		CefProcessId source_process,
		CefRefPtr<CefProcessMessage> message,
		const long cefClientId) = 0;
};
