/******************************************************************************
 Copyright (C) 2014 by John R. Bradley <jrb@turrettech.com>
 Copyright (C) 2018 by Hugh Bailey ("Jim") <jim@obsproject.com>

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

#include "cef-headers.hpp"
#include <string>
#include <fstream>

#if CHROME_VERSION_BUILD < 4638
#define ENABLE_LOCAL_FILE_URL_SCHEME 1
#else
#define ENABLE_LOCAL_FILE_URL_SCHEME 0
#endif

#if !ENABLE_LOCAL_FILE_URL_SCHEME
class BrowserSchemeHandlerFactory : public CefSchemeHandlerFactory {
public:
	virtual CefRefPtr<CefResourceHandler>
	Create(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame>,
	       const CefString &, CefRefPtr<CefRequest> request) override;

	IMPLEMENT_REFCOUNTING(BrowserSchemeHandlerFactory);
};
#endif
