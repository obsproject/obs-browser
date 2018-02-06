/******************************************************************************
 Copyright (C) 2014 by John R. Bradley <jrb@turrettech.com>

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

#include <include/cef_client.h>
#include <include/cef_display_handler.h>
#include "browser-obs-bridge.hpp"

class BrowserSource;
class BrowserRenderHandler;
class BrowserLoadHandler;

class BrowserClient :
		public CefClient,
		public CefLifeSpanHandler,
		public CefContextMenuHandler,
		public CefDisplayHandler
{
public:
	BrowserClient(CefRenderHandler *renderHandler,
		CefLoadHandler *loadHandler,
		BrowserOBSBridge *browserOBSBridge,

		CefClient *onProcessMessageReceivedHandler,	// used for passing unhandled OnProcessMessageReceived() calls to another class
								// do NOT call other methods of this class instance
								// do NOT delete this class instance

		bool allowPopupWindows				// allow opening pop-up windows with window.open() or target="_blank"
	);

public: /* CefClient overrides */
	virtual CefRefPtr<CefRenderHandler> GetRenderHandler() OVERRIDE;
	virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() OVERRIDE;
	virtual CefRefPtr<CefContextMenuHandler> GetContextMenuHandler() OVERRIDE;
	virtual CefRefPtr<CefLoadHandler> GetLoadHandler() OVERRIDE;
	virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() OVERRIDE;
	virtual bool OnProcessMessageReceived(
			CefRefPtr<CefBrowser> browser,
			CefProcessId source_process,
			CefRefPtr<CefProcessMessage> message) OVERRIDE;

public: /* CefLifeSpanHandler overrides */
	virtual bool OnBeforePopup(CefRefPtr<CefBrowser> browser,
			CefRefPtr<CefFrame> frame, const CefString& target_url,
			const CefString& target_frame_name,
			WindowOpenDisposition target_disposition,
			bool user_gesture,
			const CefPopupFeatures& popupFeatures,
			CefWindowInfo& windowInfo, CefRefPtr<CefClient>& client,
			CefBrowserSettings& settings,
			bool* no_javascript_access) OVERRIDE;
public: /* CefContextMenuHandler overrides */
	virtual void OnBeforeContextMenu(CefRefPtr<CefBrowser> browser,
			CefRefPtr<CefFrame> frame,
			CefRefPtr<CefContextMenuParams> params,
			CefRefPtr<CefMenuModel> model);

public: /* CefDisplayHandler overrides */

	///
	// Called when the page title changes.
	///
	/*--cef(optional_param=title)--*/
	virtual void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) OVERRIDE;

private:
	CefRefPtr<CefRenderHandler> renderHandler;
	CefRefPtr<CefLoadHandler> loadHandler;
	BrowserOBSBridge *browserOBSBridge;
	CefClient* onProcessMessageReceivedHandler;
	bool allowPopupWindows;

public:
	IMPLEMENT_REFCOUNTING(BrowserClient);
};
