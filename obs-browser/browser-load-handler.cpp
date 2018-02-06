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

BrowserLoadHandler::BrowserLoadHandler(const std::string css, const bool suspendElementsWhenInactive)
	: css(css), suspendElementsWhenInactive(suspendElementsWhenInactive)
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

	if (frame->IsMain() && suspendElementsWhenInactive)
	{
		///
		// The following JavaScript code will be executed on load, and will
		// pause and unpause video elements according to the main browser
		// frame visibility state.
		//
		// The effect of this is no CPU usage on invisible browser sources
		// which is very handy for multi-stage setups with multiple
		// browser sources.
		//
		// This functionality is guarded by the suspendElementsWhenInactive
		// configuration property.
		//

		std::string code;

		code += "(function() {\n";
		code += "var last = null;";

		code += "function react() {\n";
		code += "var visible = !window.document.hidden;\n";
		code += "var list = window.document.getElementsByTagName('video');\n";
		code += "for (var i = 0; i < list.length; ++i) {\n";
		code += "var e = list[i];\n";
		code += "if (!visible) {\n";
		code += "  if (e.currentSrc) {\n";
		code += "    if (!e.paused) {\n";
		code += "      e.__obs_auto_playpause_toggle_was_playing = true; \n";
		code += "      e.pause();\n";
		code += "    }\n";
		code += "  }\n";
		code += "} else {\n";
		code += "  if (e.paused) {\n";
		code += "    if (e.__obs_auto_playpause_toggle_was_playing) {\n";
		code += "      e.play();\n";
		code += "      e.__obs_auto_playpause_toggle_was_playing = false;\n";
		code += "    }\n";
		code += "  } else e.__obs_auto_playpause_toggle_was_playing = true;\n";
		code += "}\n";
		code += "}\n";
		code += "}\n";

		code += "function update() {\n";
		code += "if (last == null || last != window.document.hidden || window.document.hidden) {\n";
		code += "try { last = window.document.hidden;\n";
		code += "react(); } catch(e) {}\n";
		code += "}\n";
		code += "setTimeout(update, 100);\n";
		code += "}\n";

		code += "function setup() {\n";
		code += "window.document.addEventListener('visibilitychange', function() {\n";
		code += "react();\n";
		code += "});\n";
		code += "react();\n";
		code += "update();\n";
		code += "}\n";

		code += "function init() { try { setup(); } catch(e) { setTimeout(init, 100); } }\n";

		code += "init();\n";

		code += "})();\n";

		frame->ExecuteJavaScript(code, frame->GetURL(), 0);
	}
}
