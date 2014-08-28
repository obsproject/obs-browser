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

	virtual void OnDraw(const BrowserSurfaceHandle surfaceHandle, const int width,
		const int height) = 0;

	virtual bool CreateSurface(const int width, const int height,
		BrowserSurfaceHandle * const surfaceHandle) = 0;
	virtual void DestroySurface(const BrowserSurfaceHandle surfaceHandle) 
		= 0;

};

class BrowserListener : public BrowserListenerBase
{
public:
	virtual ~BrowserListener() {}

	virtual void OnDraw(const BrowserSurfaceHandle surfaceHandle, const int width,
		       const int height) override
	{
		(void)surfaceHandle;
		(void)width;
		(void)height;
	}

	virtual bool CreateSurface(const int width, const int height,
		BrowserSurfaceHandle * const surfaceHandle) override
	{
		(void)width;
		(void)height;
		(void)surfaceHandle;

		return false;
	}

	virtual void DestroySurface(const BrowserSurfaceHandle surfaceHandle) 
		override
	{
		(void)surfaceHandle;
	}
};