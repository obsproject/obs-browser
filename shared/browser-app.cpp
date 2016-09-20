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

#include "browser-app.hpp"

#include <iostream>
#include <string>
#include <jansson.h>

#include "fmt/format.h"
#include "include/cef_browser.h"
#include "include/cef_command_line.h"
#include "include/wrapper/cef_helpers.h"
#include "browser-version.h"

BrowserApp::BrowserApp(){
}

CefRefPtr<CefRenderProcessHandler> BrowserApp::GetRenderProcessHandler()
{
	return this;
}

void BrowserApp::OnRegisterCustomSchemes(
		CefRefPtr<CefSchemeRegistrar> registrar)
{
	registrar->AddCustomScheme("http", true, true, false);
}

void BrowserApp::OnBeforeCommandLineProcessing(
	const CefString& process_type,
	CefRefPtr<CefCommandLine> command_line)
{
	bool enableGPU = command_line->HasSwitch("enable-gpu");

	CefString type = command_line->GetSwitchValue("type");

	if (!enableGPU && type.empty())
	{
		command_line->AppendSwitch("disable-gpu");
		command_line->AppendSwitch("disable-gpu-compositing");
	}
	command_line->AppendSwitch("enable-begin-frame-scheduling");

	command_line->AppendSwitch("enable-system-flash");
}

void BrowserApp::OnContextCreated(CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		CefRefPtr<CefV8Context> context)
{
	CefRefPtr<CefV8Value> globalObj = context->GetGlobal();

	CefRefPtr<CefV8Value> obsStudioObj = CefV8Value::CreateObject(0, 0);
	globalObj->SetValue("obsstudio", obsStudioObj, V8_PROPERTY_ATTRIBUTE_NONE);

	CefRefPtr<CefV8Value> pluginVersion = CefV8Value::CreateString(OBS_BROWSER_VERSION);
	obsStudioObj->SetValue("pluginVersion", pluginVersion, V8_PROPERTY_ATTRIBUTE_NONE);
}

void BrowserApp::ExecuteJSFunction(CefRefPtr<CefBrowser> browser,
		const char *functionName,
		CefV8ValueList arguments)
{
	CefRefPtr<CefV8Context> context = browser->GetMainFrame()->GetV8Context();

	context->Enter();
	
	CefRefPtr<CefV8Value> globalObj = context->GetGlobal();

	CefRefPtr<CefV8Value> obsStudioObj = globalObj->GetValue("obsstudio");

	CefRefPtr<CefV8Value> jsFunction = obsStudioObj->GetValue(functionName);
	if (jsFunction && jsFunction->IsFunction()) {
		jsFunction->ExecuteFunction(NULL, arguments);
	}

	context->Exit();
}

bool BrowserApp::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
		CefProcessId source_process,
		CefRefPtr<CefProcessMessage> message)
{
	DCHECK(source_process == PID_BROWSER);

	CefRefPtr<CefListValue> args = message->GetArgumentList();

	if (message->GetName() == "Visibility") {
		CefV8ValueList arguments;
		arguments.push_back(CefV8Value::CreateBool(args->GetBool(0)));

		ExecuteJSFunction(browser, "onVisibilityChange", arguments);
		return true;
	}
	else if (message->GetName() == "DispatchJSEvent") {       
		CefRefPtr<CefV8Context> context = browser->GetMainFrame()->GetV8Context();

		context->Enter();

		CefRefPtr<CefV8Value> globalObj = context->GetGlobal();

		json_t *json = json_object();
        
		if (args->GetSize() > 1) {
			json_error_t error;

			json_object_set_new(json, "detail", json_loads(args->GetString(1).ToString().c_str(), 0, &error));
		}

		char *jsonString = json_dumps(json, 0);

		std::string script = fmt::format(
			"new CustomEvent('{}', {});", 
			args->GetString(0).ToString(),
			jsonString);

		free(jsonString);

		CefRefPtr<CefV8Value> returnValue;
		CefRefPtr<CefV8Exception> exception;

		// Create the CustomEvent object
		// We have to use eval to invoke the new operator
		context->Eval(script, browser->GetMainFrame()->GetURL(), 0, returnValue, exception);

		CefV8ValueList arguments;
		arguments.push_back(returnValue);

		CefRefPtr<CefV8Value> dispatchEvent = globalObj->GetValue("dispatchEvent");
		dispatchEvent->ExecuteFunction(NULL, arguments);

		context->Exit();

		return true;
	}

	return false;
}
