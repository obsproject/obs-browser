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


#import "cef-logging.h"

#include "browser-render-handler.hpp"
#include "browser-handle.h"

BrowserRenderHandler::BrowserRenderHandler(
		std::shared_ptr<BrowserHandle> &browserHandle)
: browserHandle(browserHandle)
{
}

bool BrowserRenderHandler::GetViewRect(CefRefPtr<CefBrowser> browser,
		CefRect &rect)
{
	(void)(browser);

	rect.Set(0, 0, browserHandle->GetWidth(), browserHandle->GetHeight());
	return true;
}



void BrowserRenderHandler::OnPaint(
		CefRefPtr<CefBrowser> browser, PaintElementType type,
		const RectList &dirtyRects, const void *data, int width,
		int height)
{
	(void)(browser);
	(void)(type);
	(void)(dirtyRects);

	browserHandle->RenderToAvailableTexture(width, height, data);
}