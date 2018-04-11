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

#include <map>
#include "cef-headers.hpp"

class BrowserApp : public CefApp,
                   public CefRenderProcessHandler,
                   public CefV8Handler {

	void ExecuteJSFunction(CefRefPtr<CefBrowser> browser,
			const char *functionName,
			CefV8ValueList arguments);

	typedef std::map<int, CefRefPtr<CefV8Value>> CallbackMap;

	bool shared_texture_available;
	CallbackMap callbackMap;
	int callbackId;

public:
	inline BrowserApp(bool shared_texture_available_ = false)
		: shared_texture_available(shared_texture_available_)
	{
	}

	virtual CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override;
	virtual void OnRegisterCustomSchemes(
			CefRawPtr<CefSchemeRegistrar> registrar) override;
	virtual void OnBeforeCommandLineProcessing(
			const CefString &process_type,
			CefRefPtr<CefCommandLine> command_line) override;
	virtual void OnContextCreated(CefRefPtr<CefBrowser> browser,
			CefRefPtr<CefFrame> frame,
			CefRefPtr<CefV8Context> context) override;
	virtual bool OnProcessMessageReceived(
			CefRefPtr<CefBrowser> browser,
			CefProcessId source_process,
			CefRefPtr<CefProcessMessage> message) override;
	virtual bool Execute(const CefString &name,
			CefRefPtr<CefV8Value> object,
			const CefV8ValueList &arguments,
			CefRefPtr<CefV8Value> &retval,
			CefString &exception) override;

	IMPLEMENT_REFCOUNTING(BrowserApp);
};
