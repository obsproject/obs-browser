#pragma once

#include "cef-headers.hpp"
#include "browser-panel-internal.hpp"

#include <mutex>
#include <string>

class QCefBrowserClient : public CefClient,
			  public CefDisplayHandler,
			  public CefRequestHandler,
			  public CefLifeSpanHandler,
			  public CefContextMenuHandler,
			  public CefLoadHandler,
			  public CefKeyboardHandler,
			  public CefFocusHandler,
			  public CefJSDialogHandler {

public:
	inline QCefBrowserClient(QCefWidgetInternal *widget_, const std::string &script_, bool allowAllPopups_)
		: widget(widget_),
		  script(script_),
		  allowAllPopups(allowAllPopups_)
	{
	}

	/* Called by the widget while it is still alive. Blocks until any callback
	 * currently holding the back-pointer has finished, so once it returns no
	 * CEF thread can be inside one. */
	void detachWidget()
	{
		std::lock_guard<std::recursive_mutex> lock(widgetMutex);
		widget = nullptr;
	}

	/* Publish the browser created by the queued task in Init(). If the widget
	 * went away while the browser was being created then nothing owns it, so
	 * close it here rather than leak it. */
	void attachBrowser(CefRefPtr<CefBrowser> browser)
	{
		{
			std::lock_guard<std::recursive_mutex> lock(widgetMutex);
			if (widget) {
				widget->cefBrowser = browser;
				return;
			}
		}

		if (browser)
			browser->GetHost()->CloseBrowser(true);
	}

#ifdef __linux__
	void unsetToplevelXdndProxy()
	{
		std::lock_guard<std::recursive_mutex> lock(widgetMutex);
		if (widget)
			widget->unsetToplevelXdndProxy();
	}
#endif

	/* CefClient */
	virtual CefRefPtr<CefLoadHandler> GetLoadHandler() override;
	virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() override;
	virtual CefRefPtr<CefRequestHandler> GetRequestHandler() override;
	virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override;
	virtual CefRefPtr<CefKeyboardHandler> GetKeyboardHandler() override;
	virtual CefRefPtr<CefFocusHandler> GetFocusHandler() override;
	virtual CefRefPtr<CefContextMenuHandler> GetContextMenuHandler() override;
	virtual CefRefPtr<CefJSDialogHandler> GetJSDialogHandler() override;

	/* CefDisplayHandler */
	virtual void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString &title) override;

	/* CefRequestHandler */
	virtual bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
				    CefRefPtr<CefRequest> request, bool user_gesture, bool is_redirect) override;

	virtual void OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
				 CefLoadHandler::ErrorCode errorCode, const CefString &errorText,
				 const CefString &failedUrl) override;

	virtual bool OnOpenURLFromTab(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
				      const CefString &target_url,
				      CefRequestHandler::WindowOpenDisposition target_disposition,
				      bool user_gesture) override;

	/* CefLifeSpanHandler */
	virtual bool OnBeforePopup(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
#if CHROME_VERSION_BUILD >= 6834
				   int popup_id,
#endif
				   const CefString &target_url, const CefString &target_frame_name,
				   CefLifeSpanHandler::WindowOpenDisposition target_disposition, bool user_gesture,
				   const CefPopupFeatures &popupFeatures, CefWindowInfo &windowInfo,
				   CefRefPtr<CefClient> &client, CefBrowserSettings &settings,
				   CefRefPtr<CefDictionaryValue> &extra_info, bool *no_javascript_access) override;

	virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

	/* CefFocusHandler */
	virtual bool OnSetFocus(CefRefPtr<CefBrowser> browser, CefFocusHandler::FocusSource source) override;

	/* CefContextMenuHandler */
	virtual void OnBeforeContextMenu(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
					 CefRefPtr<CefContextMenuParams> params,
					 CefRefPtr<CefMenuModel> model) override;

#if defined(_WIN32)
	virtual bool RunContextMenu(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
				    CefRefPtr<CefContextMenuParams> params, CefRefPtr<CefMenuModel> model,
				    CefRefPtr<CefRunContextMenuCallback> callback) override;
#endif

	virtual bool OnContextMenuCommand(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
					  CefRefPtr<CefContextMenuParams> params, int command_id,
					  CefContextMenuHandler::EventFlags event_flags) override;

	/* CefLoadHandler */
	virtual void OnLoadStart(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
				 TransitionType transition_type) override;

	virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode) override;

	/* CefKeyboardHandler */
	virtual bool OnPreKeyEvent(CefRefPtr<CefBrowser> browser, const CefKeyEvent &event, CefEventHandle os_event,
				   bool *is_keyboard_shortcut) override;

	/* CefJSDialogHandler */
	virtual bool OnJSDialog(CefRefPtr<CefBrowser> browser, const CefString &origin_url,
				CefJSDialogHandler::JSDialogType dialog_type, const CefString &message_text,
				const CefString &default_prompt_text, CefRefPtr<CefJSDialogCallback> callback,
				bool &suppress_message) override;

	std::string script;
	bool allowAllPopups;

private:
	/* Private on purpose. Every use has to go through widgetMutex, and making
	 * the compiler enforce that is what guarantees no unguarded one is left
	 * behind. */
	std::recursive_mutex widgetMutex;
	QCefWidgetInternal *widget = nullptr;

	IMPLEMENT_REFCOUNTING(QCefBrowserClient);
};
