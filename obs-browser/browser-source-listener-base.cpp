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

#include "browser-source.hpp"
#include "browser-source-base.hpp"
#include "browser-types.h"
#include "browser-source-listener-base.hpp"

void BrowserSource::Impl::Listener::OnDraw( BrowserSurfaceHandle surfaceHandle,
		int width, int height)
{
	UNUSED_PARAMETER(width);
	UNUSED_PARAMETER(height);

	if (textureSet.count(surfaceHandle) > 0) {

		obs_enter_graphics();
		browserSource->GetParent()->LockTexture();
		browserSource->SetActiveTexture(surfaceHandle);
		browserSource->GetParent()->UnlockTexture();
		obs_leave_graphics();

	}
	else {
		blog(LOG_ERROR, "Asked to draw unknown surface with handle"
			"%d", surfaceHandle);
	}
}

bool BrowserSource::Impl::Listener::CreateSurface(int width, int height, 
		BrowserSurfaceHandle *surfaceHandle)
{
	//TODO: We can't lock graphics on CEF thread
	// as there may be a synchronous call
	obs_enter_graphics();

	gs_texture_t *newTexture =  gs_texture_create(width, height, GS_BGRA, 1, 
			nullptr, GS_DYNAMIC);
	
	obs_leave_graphics();

	textureSet.insert(newTexture);

	*surfaceHandle = newTexture;

	return true;
}

void BrowserSource::Impl::Listener::DestroySurface(
		BrowserSurfaceHandle surfaceHandle)
{
	if (textureSet.count(surfaceHandle) == 1) {
		obs_enter_graphics();
		textureSet.erase(surfaceHandle);
		gs_texture_destroy(surfaceHandle);
		browserSource->activeTexture = nullptr;
		obs_leave_graphics();
	}
}