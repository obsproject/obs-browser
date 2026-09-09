/******************************************************************************
    Copyright (C) 2026 by Matt Gajownik <matt@volunteer.obsproject.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#pragma once

#include "cef-headers.hpp"

class BrowserDummyClient : public CefClient,
			   public CefCommandHandler,
			   public CefRequestHandler,
			   public CefLifeSpanHandler {
public:
	virtual CefRefPtr<CefCommandHandler> GetCommandHandler() override;
	virtual CefRefPtr<CefRequestHandler> GetRequestHandler() override;
	virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override;

	/* CefCommandHandler */
	virtual bool OnChromeCommand(CefRefPtr<CefBrowser> browser, int command_id,
				     cef_window_open_disposition_t disposition) override;
	virtual bool IsChromeAppMenuItemVisible(CefRefPtr<CefBrowser> browser, int command_id) override;
	virtual bool IsChromeToolbarButtonVisible(cef_chrome_toolbar_button_type_t button_type) override;

	virtual bool IsChromePageActionIconVisible(cef_chrome_page_action_icon_type_t icon_type) override;

	virtual bool IsChromeAppMenuItemEnabled(CefRefPtr<CefBrowser> browser, int command_id) override;

	/* CefLifeSpanHandler */
	virtual bool OnBeforePopup(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
#if CHROME_VERSION_BUILD >= 6834
				   int,
#endif
				   const CefString &target_url, const CefString &target_frame_name,
				   cef_window_open_disposition_t target_disposition, bool user_gesture,
				   const CefPopupFeatures &popupFeatures, CefWindowInfo &windowInfo,
				   CefRefPtr<CefClient> &client, CefBrowserSettings &settings,
				   CefRefPtr<CefDictionaryValue> &extra_info, bool *no_javascript_access) override;

	virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;

	/* CefRequestHandler */
	virtual bool OnOpenURLFromTab(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
				      const CefString &target_url,
				      CefRequestHandler::WindowOpenDisposition target_disposition,
				      bool user_gesture) override;

	virtual bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
				    CefRefPtr<CefRequest> request, bool user_gesture, bool is_redirect) override;

	IMPLEMENT_REFCOUNTING(BrowserDummyClient);
};
