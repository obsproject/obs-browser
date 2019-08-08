#pragma once

#include "cef-headers.hpp"

#include <string>

class StreamElementsCookieManager {
public:
	StreamElementsCookieManager(std::string storagePath);
	virtual ~StreamElementsCookieManager();

	CefRefPtr<CefCookieManager> GetCefCookieManager();
	CefRefPtr<CefRequestContext> GetCefRequestContext();

private:
	std::string m_storagePath;
	CefRefPtr<CefCookieManager> m_cookieManager;
	CefRefPtr<CefRequestContext> m_requestContext;
};
