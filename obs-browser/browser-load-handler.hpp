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

#include <include/cef_load_handler.h>

class BrowserListener;

class BrowserLoadHandler : public CefLoadHandler
{
public:

	BrowserLoadHandler(const std::string css);

	~BrowserLoadHandler();

public: /* CefLoadHandler overrides */

	virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode) OVERRIDE;

private:
	const std::string css;

private:
	IMPLEMENT_REFCOUNTING(BrowserLoadHandler);

};