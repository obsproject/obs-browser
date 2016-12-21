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

#include "browser-types.h"

class BrowserListenerBase
{
public:
	virtual ~BrowserListenerBase() {}

	virtual void OnDraw(BrowserSurfaceHandle surfaceHandle, int width,
			int height) = 0;
	virtual bool CreateSurface(int width, int height,
			BrowserSurfaceHandle *surfaceHandle) = 0;
	virtual void DestroySurface(BrowserSurfaceHandle surfaceHandle) = 0;
	virtual void Invalidated() = 0;
};

class BrowserListener : public BrowserListenerBase
{
public:
	virtual ~BrowserListener() {}

	virtual void OnDraw(BrowserSurfaceHandle surfaceHandle, int width,
			int height) override
	{
		(void)surfaceHandle;
		(void)width;
		(void)height;
	}

	virtual bool CreateSurface(int width, int height,
			BrowserSurfaceHandle *surfaceHandle) override
	{
		(void)width;
		(void)height;
		(void)surfaceHandle;

		return false;
	}

	virtual bool CreatePopupSurface(
		int width,
		int height,
		int x,
		int y,
		BrowserSurfaceHandle *surfaceHandle) 
	{
		(void)width;
		(void)height;
		(void)x;
		(void)y;
		(void)surfaceHandle;

		return false;
	}

	virtual void DestroyPopupSurface() 
	{
	}

	virtual void DestroySurface(BrowserSurfaceHandle surfaceHandle) override
	{
		(void)surfaceHandle;
	}

	virtual void Invalidated() override {}
};