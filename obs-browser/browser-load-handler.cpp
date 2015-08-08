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

#include "browser-load-handler.hpp"
#include "browser-listener.hpp"
#include "base64.hpp"

BrowserLoadHandler::BrowserLoadHandler(const std::string css)
	: css(css)
{
}

BrowserLoadHandler::~BrowserLoadHandler()
{
	
}

void BrowserLoadHandler::OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode)
{
	if (frame->IsMain())
	{
		std::string base64EncodedCSS = base64_encode(reinterpret_cast<const unsigned char*>(css.c_str()),css.length());
		std::string href = "data:text/css;charset=utf-8;base64," + base64EncodedCSS;

		std::string script = "";
		script += "var link = document.createElement('link');";
		script += "link.setAttribute('rel', 'stylesheet');";
		script += "link.setAttribute('type', 'text/css');";
		script += "link.setAttribute('href', '" + href + "');";
		script += "document.getElementsByTagName('head')[0].appendChild(link);";
		
		frame->ExecuteJavaScript(script, href, 0);
	}
}