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

void BrowserRenderHandler::OnPopupSize(CefRefPtr<CefBrowser> browser,
	const CefRect& rect)
{
	if (rect.width <= 0 || rect.height <= 0)
		return;

	originalPopupRect = rect;
	popupRect = GetPopupRectInWebView(originalPopupRect);
}

void BrowserRenderHandler::OnPopupShow(CefRefPtr<CefBrowser> browser,
	bool show)
{
	if (!show)
	{
		ClearPopupRects();
		browserListener->DestroyPopupSurface();
	}
}

void BrowserRenderHandler::ClearPopupRects() {
	popupRect.Set(0, 0, 0, 0);
	originalPopupRect.Set(0, 0, 0, 0);
}

bool
BrowserRenderHandler::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect &rect)
{
	(void)browser;

	rect.Set(0, 0, width, height);
	return true;
}

CefRect BrowserRenderHandler::GetPopupRectInWebView(const CefRect& originalRect)
{
	CefRect rc(originalRect);
	// if x or y are negative, move them to 0.
	if (rc.x < 0)
		rc.x = 0;
	if (rc.y < 0)
		rc.y = 0;
	// if popup goes outside the view, try to reposition origin
	if (rc.x + rc.width > width)
		rc.x = width - rc.width;
	if (rc.y + rc.height > height)
		rc.y = height - rc.height;
	// if x or y became negative, move them to 0 again.
	if (rc.x < 0)
	{
		rc.x = 0;
		rc.width = width;
	}
	if (rc.y < 0)
	{
		rc.y = 0;
		rc.height = height;
	}
	return rc;
}

void BrowserRenderHandler::OnPaint(CefRefPtr<CefBrowser> browser,
		PaintElementType type, const RectList &dirtyRects,
		const void *data, int width, int height)
{
	(void)browser;

	LockPreventDraw();

	if (!IsDrawPrevented())
	{
		viewWidth = width;
		viewHeight = height;
		if (surfaceHandles.size() < 1) {
			BrowserSurfaceHandle newSurfaceHandle = 0;
			browserListener->CreateSurface(width, height,
				&newSurfaceHandle);

			if (newSurfaceHandle != nullptr) {
				surfaceHandles.push_back(newSurfaceHandle);
			}
		}
		else if (type == PET_POPUP && popupRect.width > 0 &&
			popupRect.height > 0)
		{
			int x = popupRect.x;
			int y = popupRect.y;
			int w = width;
			int h = height;

			if (x < 0)
				x = 0;

		obs_enter_graphics();
		gs_texture_set_image(surfaceHandles[currentSurfaceHandle],
			(const uint8_t*)data, width * 4, false);
		obs_leave_graphics();
	}

	browserListener->OnDraw(surfaceHandles[currentSurfaceHandle], viewWidth, viewHeight);

	UnlockPreventDraw();
}

long BrowserRenderHandler::s_preventDraw = 0;
pthread_mutex_t BrowserRenderHandler::s_preventDrawLock = NULL;

// initializer static class to init BrowserRenderHandler::s_preventDrawLock above
BrowserRenderHandler::_InitPreventDraw BrowserRenderHandler::s_initPreventDraw;

void BrowserRenderHandler::LockPreventDraw()
{
	pthread_mutex_lock(&s_preventDrawLock);
}

void BrowserRenderHandler::UnlockPreventDraw()
{
	pthread_mutex_unlock(&s_preventDrawLock);
}

void BrowserRenderHandler::SetPreventDraw(bool preventDraw)
{
	BrowserRenderHandler::LockPreventDraw();

	if (preventDraw)
		++s_preventDraw;
	else
		--s_preventDraw;

	BrowserRenderHandler::UnlockPreventDraw();
}

bool BrowserRenderHandler::IsDrawPrevented()
{
	return s_preventDraw > 0;
}
