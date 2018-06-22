#pragma once

#include <obs-frontend-api.h>

#include "cef-headers.hpp"

#include "StreamElementsBrowserMessageHandler.hpp"

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
	StreamElementsCefClient(std::string executeJavaScriptCodeOnLoad, CefRefPtr<StreamElementsBrowserMessageHandler> messageHandler) :
		m_executeJavaScriptCodeOnLoad(executeJavaScriptCodeOnLoad),
		m_messageHandler(messageHandler)
	{
	}

	inline ~StreamElementsCefClient()
	{
	}

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

		client = new StreamElementsCefClient("", nullptr);

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

public:
	IMPLEMENT_REFCOUNTING(StreamElementsCefClient)
};
