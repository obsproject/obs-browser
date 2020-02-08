#pragma once

#include "cef-headers.hpp"
#include "browser-panel-internal.hpp"

#include <string>

class QCefBrowserClient : public CefClient,
			  public CefDisplayHandler,
			  public CefRequestHandler,
			  public CefLifeSpanHandler,
			  public CefLoadHandler,
			  public CefKeyboardHandler {

public:
	inline QCefBrowserClient(QCefWidgetInternal *widget_,
				 const std::string &script_,
				 bool allowAllPopups_)
		: widget(widget_),
		  script(script_),
		  allowAllPopups(allowAllPopups_)
	{
	}

	/* CefClient */
	virtual CefRefPtr<CefLoadHandler> GetLoadHandler() override;
	virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() override;
	virtual CefRefPtr<CefRequestHandler> GetRequestHandler() override;
	virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override;
	virtual CefRefPtr<CefKeyboardHandler> GetKeyboardHandler() override;

	/* CefDisplayHandler */
	virtual void OnTitleChange(CefRefPtr<CefBrowser> browser,
				   const CefString &title) override;

	/* CefRequestHandler */
	virtual bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
				    CefRefPtr<CefFrame> frame,
				    CefRefPtr<CefRequest> request,
				    bool user_gesture,
				    bool is_redirect) override;

	virtual void OnLoadError(CefRefPtr<CefBrowser> browser,
				 CefRefPtr<CefFrame> frame,
				 CefLoadHandler::ErrorCode errorCode,
				 const CefString &errorText,
				 const CefString &failedUrl) override;

	virtual bool OnOpenURLFromTab(
		CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
		const CefString &target_url,
		CefRequestHandler::WindowOpenDisposition target_disposition,
		bool user_gesture) override;

	/* CefLifeSpanHandler */
	virtual bool OnBeforePopup(
		CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
		const CefString &target_url, const CefString &target_frame_name,
		CefLifeSpanHandler::WindowOpenDisposition target_disposition,
		bool user_gesture, const CefPopupFeatures &popupFeatures,
		CefWindowInfo &windowInfo, CefRefPtr<CefClient> &client,
		CefBrowserSettings &settings,
#if CHROME_VERSION_BUILD >= 3770
		CefRefPtr<CefDictionaryValue> &extra_info,
#endif
		bool *no_javascript_access) override;

	/* CefLoadHandler */
	virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
			       CefRefPtr<CefFrame> frame,
			       int httpStatusCode) override;

	/* CefKeyboardHandler */
	virtual bool OnPreKeyEvent(CefRefPtr<CefBrowser> browser,
				   const CefKeyEvent &event,
				   CefEventHandle os_event,
				   bool *is_keyboard_shortcut) override;

	QCefWidgetInternal *widget = nullptr;
	std::string script;
	bool allowAllPopups;

	IMPLEMENT_REFCOUNTING(QCefBrowserClient);
};
