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

#import <OpenGL/OpenGL.h>
#import <OpenGL/glu.h>
#import <AppKit/AppKit.h>

#include "cef-logging.h"

#include "browser-texture-mac.h"

BrowserTexture::BrowserTexture(const int width, const int height,
		const BrowserSurfaceHandle surfaceHandle)
: pimpl(new Impl(width, height, surfaceHandle))
{}

BrowserTexture::~BrowserTexture()
{}

int BrowserTexture::GetWidth() const
{
	return pimpl->GetWidth();
}

int BrowserTexture::GetHeight() const
{
	return pimpl->GetHeight();
}

BrowserSurfaceHandle BrowserTexture::GetHandle() const
{
	return pimpl->GetHandle();
}

bool BrowserTexture::Upload(const void *data)
{
	return pimpl->Upload(data);
}

BrowserTexture::Impl::Impl(int width, int height,
		BrowserSurfaceHandle surfaceHandle)
: width(width), height(height), surfaceHandle(surfaceHandle)
{
	ioSurfaceRef = IOSurfaceLookup(surfaceHandle);
	IOSurfaceIncrementUseCount(ioSurfaceRef);
}

BrowserTexture::Impl::~Impl()
{
	IOSurfaceDecrementUseCount(ioSurfaceRef);
	CFRelease(ioSurfaceRef);
}

int BrowserTexture::Impl::GetWidth() const
{
	return width;
}

int BrowserTexture::Impl::GetHeight() const
{
	return height;
}

BrowserSurfaceHandle BrowserTexture::Impl::GetHandle() const
{
	return surfaceHandle;
}

bool BrowserTexture::Impl::Upload(const void *data)
{
	if (ioSurfaceRef) {
		IOSurfaceLock(ioSurfaceRef, 0, nullptr);
		void *surfaceData = IOSurfaceGetBaseAddress(ioSurfaceRef);
		memcpy(surfaceData, data, width * height * 4);
		IOSurfaceUnlock(ioSurfaceRef, 0, nullptr);
		return true;
	}
	return false;

}