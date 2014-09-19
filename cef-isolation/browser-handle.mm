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

#include "browser-texture.hpp"
#include "browser-handle.h"

BrowserHandle::BrowserHandle(int width, int height,
		id<CEFIsolationService> cefIsolationService)
: width(width), height(height),	cefIsolationService(cefIsolationService)
{}

void BrowserHandle::SetBrowser(CefRefPtr<CefBrowser> browser)
{
	this->browser = browser;
}

CefRefPtr<CefBrowser> BrowserHandle::GetBrowser()
{
	return browser;
}

void BrowserHandle::Shutdown()
{
	if (browser == nullptr) {
		return;
	}

	browser->GetHost()->CloseBrowser(true);
}

int BrowserHandle::GetWidth() const
{
	return width;
}

int BrowserHandle::GetHeight() const
{
	return height;
}

bool BrowserHandle::RenderToAvailableTexture(int width, int height,
		const void *data)
{
	if (browser == nullptr) {
		return false;
	}

	bool needsOnDraw = false;
	if (browserTexture == nullptr) {
		int surfaceHandle = 0;
		if ([cefIsolationService createSurface: browser->GetIdentifier()
				width: width height: height
				surfaceHandle: &surfaceHandle])
		{
			std::unique_ptr<BrowserTexture> newTexture(
					new BrowserTexture(width, height,
						surfaceHandle));

			browserTexture = std::move(newTexture);

			needsOnDraw = true;
		}
	}

	if (browserTexture != nullptr) {
		browserTexture->Upload(data);

		if (needsOnDraw) {
			[cefIsolationService onDraw: browser->GetIdentifier()
					width: width height:height
					surfaceHandle:
						browserTexture->GetHandle()];
		}
	}

	return true;
}
