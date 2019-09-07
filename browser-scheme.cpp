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
#include <include/wrapper/cef_stream_resource_handler.h>

#if !ENABLE_LOCAL_FILE_URL_SCHEME
CefRefPtr<CefResourceHandler>
BrowserSchemeHandlerFactory::Create(CefRefPtr<CefBrowser> browser,
				    CefRefPtr<CefFrame>, const CefString &,
				    CefRefPtr<CefRequest> request)
{
	if (!browser || !request)
		return nullptr;

	CefURLParts parts;
	CefParseURL(request->GetURL(), parts);

	std::string path = CefString(&parts.path);

	path = CefURIDecode(path, true, cef_uri_unescape_rule_t::UU_SPACES);
	path = CefURIDecode(
		path, true,
		cef_uri_unescape_rule_t::
			UU_URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);

	std::string fileExtension = path.substr(path.find_last_of(".") + 1);

	for (char &ch : fileExtension)
		ch = (char)tolower(ch);
	if (fileExtension.compare("woff2") == 0)
		fileExtension = "woff";

#ifdef _WIN32
	CefRefPtr<CefStreamReader> stream =
		CefStreamReader::CreateForFile(path.substr(1));
#else
	CefRefPtr<CefStreamReader> stream =
		CefStreamReader::CreateForFile(path);
#endif

	return new CefStreamResourceHandler(CefGetMimeType(fileExtension),
					    stream);
}
#endif
