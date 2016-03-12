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

#include <vector>

#include <include/cef_render_handler.h>

#include <obs-module.h>

class BrowserListener;

class BrowserRenderHandler : public CefRenderHandler
{
public:

	BrowserRenderHandler(const int width, const int height,
		const std::shared_ptr<BrowserListener> &browserListener);

	~BrowserRenderHandler();

public: /* CefRenderHandler overrides */

	virtual bool GetViewRect(CefRefPtr<CefBrowser> browser, CefRect &rect)
		OVERRIDE;

	CefRect GetPopupRectInWebView(const CefRect& originalRect);

	virtual void OnPaint(CefRefPtr<CefBrowser> browser,
		PaintElementType type, const RectList &dirtyRects,
		const void *buffer, int width, int height) OVERRIDE;

	const CefRect& popupRect_() const { return popupRect; }
	const CefRect& originalPopupRect_() const { return originalPopupRect; }

	// Forwarded from CefRenderHandler callbacks.
	void OnPopupShow(CefRefPtr<CefBrowser> browser,
		bool show);
	// |rect| must be in pixel coordinates.
	void OnPopupSize(CefRefPtr<CefBrowser> browser,
		const CefRect& rect);
	void ClearPopupRects();

	int GetViewWidth() const { return viewWidth; }
	int GetViewHeight() const { return viewHeight; }

private:
	const int width;
	const int height;
	const std::shared_ptr<BrowserListener> browserListener;
	std::vector<struct gs_texture *> surfaceHandles;
	int currentSurfaceHandle;
	int viewWidth;
	int viewHeight;
	CefRect popupRect;
	CefRect originalPopupRect;

private:
	IMPLEMENT_REFCOUNTING(BrowserRenderHandler);
	
};