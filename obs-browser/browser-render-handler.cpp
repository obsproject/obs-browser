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

#include "browser-render-handler.hpp"
#include "browser-listener.hpp"

BrowserRenderHandler::BrowserRenderHandler(int width, int height,
		const std::shared_ptr<BrowserListener> &browserListener)
: width(width), height(height), browserListener(browserListener),
	currentSurfaceHandle(0)
{
}

BrowserRenderHandler::~BrowserRenderHandler()
{
	for (auto i = surfaceHandles.begin(); i < surfaceHandles.end(); i++)
	{
		browserListener->DestroySurface(*i);
	}

	surfaceHandles.clear();
}

bool
BrowserRenderHandler::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect &rect)
{
	(void)browser;

	rect.Set(0, 0, width, height);
	return true;
}



void BrowserRenderHandler::OnPaint(CefRefPtr<CefBrowser> browser, 
		PaintElementType type, const RectList &dirtyRects,
		const void *data, int width, int height)
{
	(void)browser;
	(void)type;
	(void)dirtyRects;


	if (surfaceHandles.size() < 2) {
		BrowserSurfaceHandle newSurfaceHandle = 0;
		browserListener->CreateSurface(width, height, 
			&newSurfaceHandle);

		if (newSurfaceHandle != nullptr) {
			surfaceHandles.push_back(newSurfaceHandle);
		}
	}
	
	int previousSurfaceHandle = currentSurfaceHandle;

	currentSurfaceHandle = (++currentSurfaceHandle % surfaceHandles.size());

	if (currentSurfaceHandle != previousSurfaceHandle) {
		obs_enter_graphics();
		gs_texture_set_image(surfaceHandles[currentSurfaceHandle], 
				(const uint8_t*) data, width * 4, false);
		obs_leave_graphics();
	}
	
	browserListener->OnDraw(surfaceHandles[currentSurfaceHandle], width, height);
}