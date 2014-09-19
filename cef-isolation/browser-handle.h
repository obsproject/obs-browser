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
 
#include <memory>
#include <vector>

#include <include/cef_browser.h>

#include "cef-isolation.h"
#include "browser-texture.hpp"

class BrowserTexture;
class BrowserHandle;

typedef std::shared_ptr<BrowserHandle> SharedBrowserHandle;

typedef int BrowserSurfaceHandle;

class BrowserHandle {
public:
	BrowserHandle(int width, int height,
			id<CEFIsolationService> cefIsolationService);

public:
	bool RenderToAvailableTexture(int width, int height, const void *data);
public:
	int GetWidth() const;
	int GetHeight() const;

	void Shutdown();

	void SetBrowser(CefRefPtr<CefBrowser> browser);
	CefRefPtr<CefBrowser> GetBrowser();

private:
	CefRefPtr<CefBrowser> browser;

	const int width;
	const int height;

	// This should be abstracted away so that both OSX and other platforms
	// can use this class
	id<CEFIsolationService> cefIsolationService;

	std::unique_ptr<BrowserTexture> browserTexture;
};

