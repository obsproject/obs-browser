#pragma once

#include <obs-frontend-api.h>

#include "cef-headers.hpp"

#include "StreamElementsBrowserMessageHandler.hpp"

class StreamElementsCefClientEventHandler :
	public CefBaseRefCounted
{
public:
	virtual void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
		bool isLoading,
		bool canGoBack,
		bool canGoForward) { }


public:
	IMPLEMENT_REFCOUNTING(StreamElementsCefClientEventHandler)
};

class StreamElementsCefClient :
	public CefClient,
	public CefLifeSpanHandler,
	public CefContextMenuHandler,
	public CefLoadHandler,
	public CefDisplayHandler
{
private:
	std::string m_containerId = "";
	std::string m_locationArea = "unknown";

public:
	StreamElementsCefClient(std::string executeJavaScriptCodeOnLoad, CefRefPtr<StreamElementsBrowserMessageHandler> messageHandler, CefRefPtr<StreamElementsCefClientEventHandler> eventHandler);
	virtual ~StreamElementsCefClient();

public:
	/* Own */
	std::string GetContainerId() { return m_containerId; }
	void SetContainerId(std::string id) { m_containerId = id; }

	std::string GetLocationArea() { return m_locationArea; }
	void SetLocationArea(std::string area) { m_locationArea = area; }

	/* CefClient */
	virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
	virtual CefRefPtr<CefContextMenuHandler> GetContextMenuHandler() override { return this; }
	virtual CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
	virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }

	virtual bool OnProcessMessageReceived(
		CefRefPtr<CefBrowser> browser,
		CefProcessId source_process,
		CefRefPtr<CefProcessMessage> message) override;

	/* CefLifeSpanHandler */
	virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;

	///
	// Called just before a browser is destroyed. Release all references to the
	// browser object and do not attempt to execute any methods on the browser
	// object after this callback returns. This callback will be the last
	// notification that references |browser|. See DoClose() documentation for
	// additional usage information.
	///
	/*--cef()--*/
	virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

	virtual bool OnBeforePopup(
		CefRefPtr<CefBrowser> /*browser*/,
		CefRefPtr<CefFrame> /*frame*/,
		const CefString& /*target_url*/,
		const CefString& /*target_frame_name*/,
		WindowOpenDisposition /*target_disposition*/,
		bool /*user_gesture*/,
		const CefPopupFeatures& /*popupFeatures*/,
		CefWindowInfo& windowInfo,
		CefRefPtr<CefClient>& client,
		CefBrowserSettings& /*settings*/,
		bool* /*no_javascript_access*/) override
	{
		windowInfo.parent_window = (cef_window_handle_t)obs_frontend_get_main_window_handle();

		client = new StreamElementsCefClient("", nullptr, nullptr);

		// Allow pop-ups
		return false;
	}

	/* CefContextMenuHandler */
	virtual void OnBeforeContextMenu(
		CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		CefRefPtr<CefContextMenuParams> params,
		CefRefPtr<CefMenuModel> model) override
	{
		// Remove all context menu contributions
		model->Clear();
	}

	/* CefLoadHandler */
	virtual void OnLoadEnd(
		CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		int httpStatusCode) override;

	virtual void OnLoadError(CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		ErrorCode errorCode,
		const CefString& errorText,
		const CefString& failedUrl) override;

	///
	// Called when the loading state has changed. This callback will be executed
	// twice -- once when loading is initiated either programmatically or by user
	// action, and once when loading is terminated due to completion, cancellation
	// of failure. It will be called before any calls to OnLoadStart and after all
	// calls to OnLoadError and/or OnLoadEnd.
	///
	/*--cef()--*/
	virtual void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
		bool isLoading,
		bool canGoBack,
		bool canGoForward) override;

	/* CefDisplayHandler */
	virtual void OnTitleChange(CefRefPtr<CefBrowser> browser,
		const CefString& title) override;

	virtual void OnFaviconURLChange(CefRefPtr<CefBrowser> browser,
		const std::vector<CefString>& icon_urls) override;

public:
	std::string GetExecuteJavaScriptCodeOnLoad()
	{
		return m_executeJavaScriptCodeOnLoad;
	}

private:
	std::string m_executeJavaScriptCodeOnLoad;
	CefRefPtr<StreamElementsBrowserMessageHandler> m_messageHandler;
	CefRefPtr<StreamElementsCefClientEventHandler> m_eventHandler;

public:
	static void DispatchJSEvent(std::string event, std::string eventArgsJson);

public:
	IMPLEMENT_REFCOUNTING(StreamElementsCefClient)
};
