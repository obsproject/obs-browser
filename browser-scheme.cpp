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

#include "browser-scheme.hpp"
#include "wide-string.hpp"

/* ------------------------------------------------------------------------- */

CefRefPtr<CefResourceHandler> BrowserSchemeHandlerFactory::Create(
		CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame>,
		const CefString &,
		CefRefPtr<CefRequest> request)
{
	if (!browser || !request)
		return nullptr;

	return CefRefPtr<BrowserSchemeHandler>(new BrowserSchemeHandler());
}

/* ------------------------------------------------------------------------- */

bool BrowserSchemeHandler::ProcessRequest(
		CefRefPtr<CefRequest> request,
		CefRefPtr<CefCallback> callback)
{
	CefURLParts parts;
	CefParseURL(request->GetURL(), parts);

	std::string path = CefString(&parts.path);

	path = CefURIDecode(path, true, cef_uri_unescape_rule_t::UU_SPACES);
	path = CefURIDecode(path, true, cef_uri_unescape_rule_t::UU_URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);

#ifdef WIN32
	inputStream.open(to_wide(path.erase(0,1)), std::ifstream::binary);
#else
	inputStream.open(path, std::ifstream::binary);
#endif

	/* Set fileName for use in GetResponseHeaders */
	fileName = path;

	if (!inputStream.is_open()) {
		callback->Cancel();
		return false;
	}

	inputStream.seekg(0, std::ifstream::end);
	length = remaining = inputStream.tellg();
	inputStream.seekg(0, std::ifstream::beg);
	callback->Continue();
	return true;
}

void BrowserSchemeHandler::GetResponseHeaders(
		CefRefPtr<CefResponse> response,
		int64 &response_length,
		CefString &redirectUrl)
{
	if (!response) {
		response_length = -1;
		redirectUrl = "";
		return;
	}

	std::string fileExtension =
		fileName.substr(fileName.find_last_of(".") + 1);

	for (char &ch : fileExtension)
		ch = (char)tolower(ch);
	if (fileExtension.compare("woff2") == 0)
		fileExtension = "woff";

	response->SetStatus(200);
	response->SetMimeType(CefGetMimeType(fileExtension));
	response->SetStatusText("OK");
	response_length = length;
	redirectUrl = "";
}

bool BrowserSchemeHandler::ReadResponse(
		void *data_out,
		int bytes_to_read,
		int &bytes_read,
		CefRefPtr<CefCallback>)
{
	if (!data_out || !inputStream.is_open()) {
		bytes_read = 0;
		inputStream.close();
		return false;
	}

	if (isComplete) {
		bytes_read = 0;
		return false;
	}

	inputStream.read((char *)data_out, bytes_to_read);
	bytes_read = (int)inputStream.gcount();
	remaining -= bytes_read;

	if (remaining == 0) {
		isComplete = true;
		inputStream.close();
	}

	return true;
}

void BrowserSchemeHandler::Cancel()
{
	if (inputStream.is_open())
		inputStream.close();
}
