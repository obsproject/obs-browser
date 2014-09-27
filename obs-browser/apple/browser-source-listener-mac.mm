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

#import <Foundation/Foundation.h>

#import "cef-logging.h"
#include "browser-source.hpp"
#include "browser-source-mac.h"
#include "browser-types.h"
#include "browser-source-listener-mac.h"
#include "texture-ref.hpp"

void BrowserSource::Impl::Listener::OnDraw(BrowserSurfaceHandle surfaceHandle,
		int width, int height)
{
	UNUSED_PARAMETER(width);
	UNUSED_PARAMETER(height);

	if (textureMap.count(surfaceHandle) > 0) {

		obs_enter_graphics();
		browserSource->GetParent()->LockTexture();
		if (gs_texture_rebind_iosurface(
			textureMap[surfaceHandle]->GetTexture(),
			textureMap[surfaceHandle]->GetSurfaceRef()))
		{
			browserSource->SetActiveTexture(
				textureMap[surfaceHandle].get());
		} else {
			browserSource->SetActiveTexture(nullptr);
		}
		browserSource->GetParent()->UnlockTexture();
		obs_leave_graphics();

	} else {
		CEFLogError(@"Asked to draw unknown surface with handle"
			"%d", surfaceHandle);
	}
}

bool BrowserSource::Impl::Listener::CreateSurface(int width,
		int height, BrowserSurfaceHandle *surfaceHandle)
{
	unsigned pixelFormat = 'BGRA';
	unsigned bytesPerElement = 4;

	unsigned long bytesPerRow = width * bytesPerElement;

	if (!bytesPerRow) {
		return 0;
	}

	unsigned long allocSize = height * bytesPerRow;

	if (!allocSize) {
		return 0;
	}

	CFDictionaryRef dictionaryRef =
			(CFDictionaryRef)
			[NSDictionary dictionaryWithObjectsAndKeys:
				[NSNumber numberWithInt: width],
				(id)kIOSurfaceWidth,
				[NSNumber numberWithInt: height],
				(id)kIOSurfaceHeight,
				[NSNumber numberWithInt:bytesPerElement],
				(id)kIOSurfaceBytesPerElement,
				[NSNumber numberWithBool:YES],
				(id)kIOSurfaceIsGlobal,
				[NSNumber numberWithInt: pixelFormat],
				(id)kIOSurfacePixelFormat,
				[NSNumber numberWithLong: bytesPerRow],
				(id)kIOSurfaceBytesPerRow,
				[NSNumber numberWithLong: allocSize],
				(id)kIOSurfaceAllocSize,
				nil];

	IOSurfaceRef ioSurfaceRef = IOSurfaceCreate(dictionaryRef);

	if (ioSurfaceRef == nullptr) {
		return false;
	}

	obs_enter_graphics();
	gs_texture_t *ioSurfaceTex =
		gs_texture_create_from_iosurface(ioSurfaceRef);

	obs_leave_graphics();

	if (ioSurfaceTex == nullptr) {
		CFRelease(ioSurfaceRef);
		return false;
	}

	int ioSurfaceHandle = IOSurfaceGetID(ioSurfaceRef);

	textureMap[ioSurfaceHandle].reset(
			new TextureRef(ioSurfaceRef, ioSurfaceTex));

	*surfaceHandle = ioSurfaceHandle;

	return true;
}

void BrowserSource::Impl::Listener::DestroySurface(
		BrowserSurfaceHandle surfaceHandle)
{
	if (textureMap.count(surfaceHandle) == 1) {
		obs_enter_graphics();
		browserSource->GetParent()->LockTexture();
		textureMap.erase(surfaceHandle);
		browserSource->activeTexture = nullptr;
		browserSource->GetParent()->UnlockTexture();
		obs_leave_graphics();
	}
}

void BrowserSource::Impl::Listener::Invalidated()
{
	obs_enter_graphics();
	browserSource->GetParent()->LockTexture();
	textureMap.clear();
	browserSource->activeTexture = nullptr;
	browserSource->GetParent()->browserIdentifier = 0;
	browserSource->GetParent()->UnlockTexture();
	obs_leave_graphics();

	browserSource->GetParent()->UpdateBrowser();
}