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
#include <include/cef_version.h>

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
		CefRawPtr<CefSchemeRegistrar> registrar)
{
#if CHROME_VERSION_BUILD >= 3029
	registrar->AddCustomScheme("http", true, false, false, false, true, false);
#else
	registrar->AddCustomScheme("http", true, false, false, false, true);
#endif
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

	CefRefPtr<CefV8Value> func = CefV8Value::CreateFunction("getCurrentScene", this);
  	obsStudioObj->SetValue("getCurrentScene", func, V8_PROPERTY_ATTRIBUTE_NONE);
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
	else if (message->GetName() == "Active") {
		CefV8ValueList arguments;
		arguments.push_back(CefV8Value::CreateBool(args->GetBool(0)));

		ExecuteJSFunction(browser, "onActiveChange", arguments);
		return true;
	}
	else if (message->GetName() == "DispatchJSEvent") {       
		CefRefPtr<CefV8Context> context = browser->GetMainFrame()->GetV8Context();

		context->Enter();

		CefRefPtr<CefV8Value> globalObj = context->GetGlobal();

		// Build up a new json object to store the CustomEvent data in.
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
	else if (message->GetName() == "executeCallback") {
        CefRefPtr<CefV8Context> context = browser->GetMainFrame()->GetV8Context();
        
        context->Enter();

		int callbackID = message->GetArgumentList()->GetInt(0);
		CefString jsonString = message->GetArgumentList()->GetString(1);

		CefRefPtr<CefV8Value> callback = callbackMap[callbackID];
		CefV8ValueList args;
		args.push_back(CefV8Value::CreateString(jsonString));

		CefRefPtr<CefV8Value> retval;
		CefRefPtr<CefV8Exception> exception;
        callback->ExecuteFunction(NULL, args);
        
        context->Exit();

		callbackMap.erase(callbackID);

		
		return true;
	}

	return false;
}

// CefV8Handler::Execute
bool BrowserApp::Execute(const CefString& name,
		CefRefPtr<CefV8Value> object,
		const CefV8ValueList& arguments,
		CefRefPtr<CefV8Value>& retval,
		CefString& exception)
{
	if (name == "getCurrentScene") {

		if (arguments.size() == 1 && arguments[0]->IsFunction()) {
			callbackId++;
			callbackMap[callbackId] = arguments[0];
		}

		CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("getCurrentScene");
		CefRefPtr<CefListValue> args = msg->GetArgumentList();
		args->SetInt(0, callbackId);

		CefRefPtr<CefBrowser> browser = 
                CefV8Context::GetCurrentContext()->GetBrowser();
		browser->SendProcessMessage(PID_BROWSER, msg);

		return true;
	}

	// Function does not exist.
	return false;
}
