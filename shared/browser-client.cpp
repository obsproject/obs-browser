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

#include <include/cef_render_handler.h>

#include "browser-client.hpp"

BrowserClient::BrowserClient(CefRenderHandler *renderHandler,
	CefLoadHandler *loadHandler)
	: renderHandler(renderHandler), loadHandler(loadHandler)
{
}

CefRefPtr<CefRenderHandler> BrowserClient::GetRenderHandler()
{
	return renderHandler;
}

CefRefPtr<CefLoadHandler> BrowserClient::GetLoadHandler()
{
	return loadHandler;
}

CefRefPtr<CefLifeSpanHandler> BrowserClient::GetLifeSpanHandler()
{
	return this;
}

CefRefPtr<CefContextMenuHandler> BrowserClient::GetContextMenuHandler()
{
	return this;
}

bool BrowserClient::OnBeforePopup(CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame, const CefString& target_url,
	const CefString& target_frame_name,
	WindowOpenDisposition target_disposition,
	bool user_gesture,
	const CefPopupFeatures& popupFeatures,
	CefWindowInfo& windowInfo, CefRefPtr<CefClient>& client,
	CefBrowserSettings& settings,
	bool* no_javascript_access)
{
	(void)browser;
	(void)frame;
	(void)target_url;
	(void)target_frame_name;
	(void)target_disposition;
	(void)user_gesture;
	(void)popupFeatures;
	(void)windowInfo;
	(void)client;
	(void)settings;
	(void)no_javascript_access;

	// block popups
	return true;
}

void BrowserClient::OnBeforeContextMenu(CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame, CefRefPtr<CefContextMenuParams> params,
	CefRefPtr<CefMenuModel> model)
{
	(void)browser;
	(void)frame;
	(void)params;

	// remove all context menu contributions
	model->Clear();
}