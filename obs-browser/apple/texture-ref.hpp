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

#include <obs-module.h>
#import <Foundation/Foundation.h>
#import <IOSurface/IOSurfaceAPI.h>

class TextureRef {
public:
	TextureRef(IOSurfaceRef ioSurfaceRef, gs_texture_t *texture)
	: ioSurfaceRef(ioSurfaceRef), texture(texture)
	{
		IOSurfaceIncrementUseCount(ioSurfaceRef);
	}

	~TextureRef() {
		obs_enter_graphics();
		gs_texture_destroy(texture);
		obs_leave_graphics();
		IOSurfaceDecrementUseCount(ioSurfaceRef);
		CFRelease(ioSurfaceRef);
	}

public:
	gs_texture_t *GetTexture() const { return texture; }
	IOSurfaceRef GetSurfaceRef() const { return ioSurfaceRef; }

private:
	IOSurfaceRef ioSurfaceRef;
	gs_texture_t *texture;
};
