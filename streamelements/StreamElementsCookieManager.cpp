#include "StreamElementsCookieManager.hpp"
#include <obs.h>
#include <util/platform.h>

void flush_cookie_manager(CefRefPtr<CefCookieManager> cm);
void flush_cookie_managers();
void register_cookie_manager(CefRefPtr<CefCookieManager> cm);
void unregister_cookie_manager(CefRefPtr<CefCookieManager> cm);

#define persist_session_cookies false

StreamElementsCookieManager::StreamElementsCookieManager(std::string storagePath)
	: m_storagePath(storagePath),
	  m_cookieManager(nullptr),
	  m_requestContext(nullptr)
{
	//int os_mkdirs_ret = os_mkdirs(m_storagePath.c_str());

	//if (MKDIR_ERROR == os_mkdirs_ret) {
	if (!os_file_exists(m_storagePath.c_str())) {
		blog(LOG_ERROR,
		     "obs-browser: cookie storage path '%s' does not exist",
		     m_storagePath.c_str());

		return;
	}

#if CHROME_VERSION_BUILD < 3770
	m_cookieManager = CefCookieManager::CreateManager(
		m_storagePath, persist_session_cookies, nullptr);

	if (m_cookieManager) {
		class LocalCefRequestContextHandler
			: public CefRequestContextHandler {
		public:
			inline LocalCefRequestContextHandler(
				CefRefPtr<CefCookieManager> cookieManager)
				: m_cookieNManager(cookieManager)
			{
			}

			virtual CefRefPtr<CefCookieManager>
			GetCookieManager() override
			{
				return m_cookieNManager;
			}

			IMPLEMENT_REFCOUNTING(LocalCefRequestContextHandler);

		private:
			CefRefPtr<CefCookieManager> m_cookieNManager;
		};

		m_requestContext = CefRequestContext::CreateContext(
			CefRequestContext::GetGlobalContext(),
			new LocalCefRequestContextHandler(m_cookieManager));
	}
#else
	CefRequestContextSettings settings;
	CefString(&settings.cache_path) = m_storagePath;

	m_requestContext = CefRequestContext::CreateContext(
		settings, CefRefPtr<CefRequestContextHandler>());

	if (m_requestContext) {
		m_cookieManager = m_requestContext->GetCookieManager(nullptr);
	}
#endif

	if (m_cookieManager && m_requestContext) {
		blog(LOG_INFO,
		     "obs-browser: created request context and cookie manager with storage at '%s'",
		     m_storagePath.c_str());
	} else {
		blog(LOG_ERROR,
		     "obs-browser: failed creating request context and cookie manager with storage at '%s'",
		     m_storagePath.c_str());
	}

	register_cookie_manager(m_cookieManager);
}

StreamElementsCookieManager::~StreamElementsCookieManager()
{
	unregister_cookie_manager(m_cookieManager);
}

CefRefPtr<CefCookieManager> StreamElementsCookieManager::GetCefCookieManager()
{
	return m_cookieManager;
}

CefRefPtr<CefRequestContext> StreamElementsCookieManager::GetCefRequestContext()
{
	return m_requestContext;
}
