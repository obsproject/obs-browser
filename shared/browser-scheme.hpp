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

#include <map>
#include <fstream>

#include <include/cef_scheme.h>

class BrowserSchemeHandlerFactory : public CefSchemeHandlerFactory {

public:
	BrowserSchemeHandlerFactory();

public: // CefSchemeHandlerFactory overrides
	virtual CefRefPtr<CefResourceHandler> Create(
			CefRefPtr<CefBrowser> browser,
			CefRefPtr<CefFrame> frame, const CefString& scheme_name,
			CefRefPtr<CefRequest> request);

public:
	IMPLEMENT_REFCOUNTING(BrowserSchemeHandlerFactory);

};

class BrowserSchemeHandler : public CefResourceHandler {
public:
	BrowserSchemeHandler();
	virtual bool ProcessRequest(CefRefPtr<CefRequest> request,
			CefRefPtr<CefCallback> callback);
	virtual void GetResponseHeaders(CefRefPtr<CefResponse> response,
			int64& response_length, CefString& redirectUrl);
	virtual bool ReadResponse(void* data_out, int bytes_to_read,
			int& bytes_read, CefRefPtr<CefCallback> callback);
	virtual void Cancel();


private:
	std::string fileName;
	std::ifstream inputStream;
	bool isComplete;
	int64 length;
	int64 remaining;

public:
	IMPLEMENT_REFCOUNTING(BrowserSchemeHandler);
};