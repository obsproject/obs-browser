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
#include <map>
#include "browser-listener.hpp"
#include "browser-types.h"

class Impl;

class BrowserSource::Impl::Listener : public BrowserListener
{
public:
	Listener(BrowserSource::Impl * const browserSource)
	: browserSource(browserSource)
	{}

	~Listener()
	{}

	void OnDraw(const BrowserSurfaceHandle surfaceHandle,
			int width, int height);
	bool CreateSurface(int width, int height,
			BrowserSurfaceHandle *surfaceHandle);
	void DestroySurface(BrowserSurfaceHandle surfaceHandle);
	void Invalidated();

private:
	BrowserSource::Impl *browserSource;
	std::map<int, std::shared_ptr<TextureRef>> textureMap;
};
