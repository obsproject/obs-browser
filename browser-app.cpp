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

#include "browser-app.hpp"
#include "browser-version.h"
#include <json11/json11.hpp>
#include <include/cef_parser.h>		// CefParseJSON, CefWriteJSON

#ifdef _WIN32
#include <windows.h>
#endif

using namespace json11;

static std::string StringReplaceAll(std::string str, const std::string& from, const std::string& to) {
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
	}
	return str;
}

CefRefPtr<CefRenderProcessHandler> BrowserApp::GetRenderProcessHandler()
{
	return this;
}

CefRefPtr<CefBrowserProcessHandler> BrowserApp::GetBrowserProcessHandler()
{
	return this;
}

void BrowserApp::OnRegisterCustomSchemes(
		CefRawPtr<CefSchemeRegistrar> registrar)
{
#if CHROME_VERSION_BUILD >= 3029
	registrar->AddCustomScheme("http", true, false, false, false, true,
			false);
#else
	registrar->AddCustomScheme("http", true, false, false, false, true);
#endif
}

void BrowserApp::OnBeforeChildProcessLaunch(
		CefRefPtr<CefCommandLine> command_line)
{
#ifdef _WIN32
	std::string pid = std::to_string(GetCurrentProcessId());
	command_line->AppendSwitchWithValue("parent_pid", pid);
#else
	(void)command_line;
#endif
}

void BrowserApp::OnBeforeCommandLineProcessing(
		const CefString &,
		CefRefPtr<CefCommandLine> command_line)
{
	if (!shared_texture_available) {
		bool enableGPU = command_line->HasSwitch("enable-gpu");
		CefString type = command_line->GetSwitchValue("type");

		if (!enableGPU && type.empty()) {
			command_line->AppendSwitch("disable-gpu");
			command_line->AppendSwitch("disable-gpu-compositing");
		}
	}

	command_line->AppendSwitch("enable-system-flash");

	command_line->AppendSwitchWithValue("autoplay-policy",
			"no-user-gesture-required");
	command_line->AppendSwitchWithValue("plugin-policy", "allow");
}

void BrowserApp::OnContextCreated(CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame>,
		CefRefPtr<CefV8Context> context)
{
	CefRefPtr<CefV8Value> globalObj = context->GetGlobal();

	CefRefPtr<CefV8Value> obsStudioObj = CefV8Value::CreateObject(0, 0);
	globalObj->SetValue("obsstudio",
			obsStudioObj, V8_PROPERTY_ATTRIBUTE_NONE);

	CefRefPtr<CefV8Value> pluginVersion =
		CefV8Value::CreateString(OBS_BROWSER_VERSION_STRING);
	obsStudioObj->SetValue("pluginVersion",
			pluginVersion, V8_PROPERTY_ATTRIBUTE_NONE);

	CefRefPtr<CefV8Value> func =
		CefV8Value::CreateFunction("getCurrentScene", this);
	obsStudioObj->SetValue("getCurrentScene",
			func, V8_PROPERTY_ATTRIBUTE_NONE);

	CefRefPtr<CefV8Value> getStatus =
		CefV8Value::CreateFunction("getStatus", this);
	obsStudioObj->SetValue("getStatus",
			getStatus, V8_PROPERTY_ATTRIBUTE_NONE);

	///
	// signal CefClient that render process context has been created
	CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("CefRenderProcessHandler::OnContextCreated");
	//CefRefPtr<CefBrowser> browser = CefV8Context::GetCurrentContext()->GetBrowser();
	browser->SendProcessMessage(PID_BROWSER, msg);
}

void BrowserApp::ExecuteJSFunction(CefRefPtr<CefBrowser> browser,
		const char *functionName,
		CefV8ValueList arguments)
{
	CefRefPtr<CefV8Context> context =
		browser->GetMainFrame()->GetV8Context();

	context->Enter();

	CefRefPtr<CefV8Value> globalObj = context->GetGlobal();
	CefRefPtr<CefV8Value> obsStudioObj = globalObj->GetValue("obsstudio");
	CefRefPtr<CefV8Value> jsFunction = obsStudioObj->GetValue(functionName);

	if (jsFunction && jsFunction->IsFunction())
		jsFunction->ExecuteFunction(NULL, arguments);

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

	} else if (message->GetName() == "Active") {
		CefV8ValueList arguments;
		arguments.push_back(CefV8Value::CreateBool(args->GetBool(0)));

		ExecuteJSFunction(browser, "onActiveChange", arguments);

	} else if (message->GetName() == "DispatchJSEvent") {
		CefRefPtr<CefV8Context> context =
			browser->GetMainFrame()->GetV8Context();

		context->Enter();

		CefRefPtr<CefV8Value> globalObj = context->GetGlobal();
		
		Json::object json;
		if (args->GetSize() > 1) {
			std::string err;
			json["detail"] = Json::parse(args->GetString(1).ToString(), err);
		}
		std::string jsonString = Json(json).dump();

		jsonString = StringReplaceAll(jsonString, "'", "\\u0027");
		jsonString = StringReplaceAll(jsonString, "\\", "\\\\");

		std::string script;

		script += "new CustomEvent('";
		script += args->GetString(0).ToString();
		script += "', ";
		script += "window.JSON.parse('" + jsonString + "')";
		script += ");";

		CefRefPtr<CefV8Value> returnValue;
		CefRefPtr<CefV8Exception> exception;

		/* Create the CustomEvent object
		 * We have to use eval to invoke the new operator */
		context->Eval(script, browser->GetMainFrame()->GetURL(),
				0, returnValue, exception);

		CefV8ValueList arguments;
		arguments.push_back(returnValue);

		CefRefPtr<CefV8Value> dispatchEvent =
			globalObj->GetValue("dispatchEvent");
		dispatchEvent->ExecuteFunction(NULL, arguments);

		context->Exit();

	}
	else if (message->GetName() == "executeCallback") {
		CefRefPtr<CefV8Context> context =
			browser->GetMainFrame()->GetV8Context();
		CefRefPtr<CefV8Value> retval;
		CefRefPtr<CefV8Exception> exception;

		context->Enter();

		CefRefPtr<CefListValue> arguments = message->GetArgumentList();

		if (arguments->GetSize()) {
			int callbackID = arguments->GetInt(0);

			CefRefPtr<CefV8Value> callback = callbackMap[callbackID];

			CefV8ValueList args;

			if (arguments->GetSize() > 1) {
				std::string json = arguments->GetString(1).ToString();

				json = StringReplaceAll(json, "'", "\\u0027");
				json = StringReplaceAll(json, "\\", "\\\\");

				std::string script = "JSON.parse('" + json + "');";

				context->Eval(script, browser->GetMainFrame()->GetURL(),
					0, retval, exception);

				args.push_back(retval);
			}

			callback->ExecuteFunction(NULL, args);

			callbackMap.erase(callbackID);
		}

		context->Exit();
	}
	else if (message->GetName() == "CefRenderProcessHandler::BindJavaScriptProperties") {
		CefRefPtr<CefV8Context> context =
			browser->GetMainFrame()->GetV8Context();

		context->Enter();

		CefRefPtr<CefV8Value> globalObj = context->GetGlobal();

		CefString containerName = args->GetValue(0)->GetString();
		CefString root_json_string = args->GetValue(1)->GetString();
		CefRefPtr<CefDictionaryValue> root =
			CefParseJSON(root_json_string, JSON_PARSER_ALLOW_TRAILING_COMMAS)->GetDictionary();

		CefDictionaryValue::KeyList propsList;
		if (root->GetKeys(propsList)) {
			for (auto propName : propsList) {
				// Get/create function container
				CefRefPtr<CefV8Value> containerObj = nullptr;

				if (!globalObj->HasValue(containerName)) {
					containerObj = CefV8Value::CreateObject(0, 0);

					globalObj->SetValue(containerName,
						containerObj, V8_PROPERTY_ATTRIBUTE_NONE);
				}
				else {
					containerObj = globalObj->GetValue(containerName);
				}

				std::string propFullName = "window.";
				propFullName.append(containerName);
				propFullName.append(".");
				propFullName.append(propName);

				CefRefPtr<CefV8Value> propValue;

				switch (root->GetType(propName)) {
				case VTYPE_NULL:
					propValue = CefV8Value::CreateNull();
					break;
				case VTYPE_BOOL:
					propValue = CefV8Value::CreateBool(root->GetBool(propName));
					break;
				case VTYPE_INT:
					propValue = CefV8Value::CreateInt(root->GetInt(propName));
					break;
				case VTYPE_DOUBLE:
					propValue = CefV8Value::CreateDouble(root->GetDouble(propName));
					break;
				case VTYPE_STRING:
					propValue = CefV8Value::CreateString(root->GetString(propName));
					break;
				// case VTYPE_BINARY:
				// case VTYPE_DICTIONARY:
				// case VTYPE_LIST:
				// case VTYPE_INVALID:
				default:
					propValue = CefV8Value::CreateUndefined();
					break;
				}

				// Create function
				containerObj->SetValue(propName,
					propValue,
					V8_PROPERTY_ATTRIBUTE_NONE);
			}
		}

		context->Exit();
	}
	else if (message->GetName() == "CefRenderProcessHandler::BindJavaScriptFunctions") {
		CefRefPtr<CefV8Context> context =
			browser->GetMainFrame()->GetV8Context();

		context->Enter();

		CefRefPtr<CefV8Value> globalObj = context->GetGlobal();

		CefString containerName = args->GetValue(0)->GetString();
		CefString root_json_string = args->GetValue(1)->GetString();
		CefRefPtr<CefDictionaryValue> root =
			CefParseJSON(root_json_string, JSON_PARSER_ALLOW_TRAILING_COMMAS)->GetDictionary();

		CefDictionaryValue::KeyList functionsList;
		if (root->GetKeys(functionsList)) {
			for (auto functionName : functionsList) {
				CefRefPtr<CefDictionaryValue> function =
					root->GetDictionary(functionName);

				auto messageName = function->GetString("message");

				if (!messageName.empty()) {
					// Get/create function container
					CefRefPtr<CefV8Value> containerObj = nullptr;

					if (!globalObj->HasValue(containerName)) {
						containerObj = CefV8Value::CreateObject(0, 0);

						globalObj->SetValue(containerName,
							containerObj, V8_PROPERTY_ATTRIBUTE_NONE);
					}
					else {
						containerObj = globalObj->GetValue(containerName);
					}

					std::string functionFullName = "window.";
					functionFullName.append(containerName);
					functionFullName.append(".");
					functionFullName.append(functionName);

					// Create function
					containerObj->SetValue(functionName,
						CefV8Value::CreateFunction(functionFullName, this),
						V8_PROPERTY_ATTRIBUTE_NONE);


					// Add function name -> metadata map
					APIFunctionItem item;

					item.message = messageName;
					item.fullName = functionFullName;

					cefClientFunctions[functionFullName] = item;
				}
			}
		}

		context->Exit();
	} else {
		return false;
	}

	return true;
}

bool BrowserApp::Execute(const CefString &name,
		CefRefPtr<CefV8Value>,
		const CefV8ValueList &arguments,
		CefRefPtr<CefV8Value> &,
		CefString &)
{
	if (name == "getCurrentScene") {
		if (arguments.size() == 1 && arguments[0]->IsFunction()) {
			callbackId++;
			callbackMap[callbackId] = arguments[0];
		}

		CefRefPtr<CefProcessMessage> msg =
			CefProcessMessage::Create("getCurrentScene");
		CefRefPtr<CefListValue> args = msg->GetArgumentList();
		args->SetInt(0, callbackId);

		CefRefPtr<CefBrowser> browser =
			CefV8Context::GetCurrentContext()->GetBrowser();
		browser->SendProcessMessage(PID_BROWSER, msg);

	}
	else if (name == "getStatus") {
		if (arguments.size() == 1 && arguments[0]->IsFunction()) {
			callbackId++;
			callbackMap[callbackId] = arguments[0];
		}

		CefRefPtr<CefProcessMessage> msg =
			CefProcessMessage::Create("getStatus");
		CefRefPtr<CefListValue> args = msg->GetArgumentList();
		args->SetInt(0, callbackId);

		CefRefPtr<CefBrowser> browser =
			CefV8Context::GetCurrentContext()->GetBrowser();
		browser->SendProcessMessage(PID_BROWSER, msg);
	} else if (cefClientFunctions.count(name)) {
		/* dynamic API function binding from CefClient, see "CefRenderProcessHandler::BindJavaScriptFunctions"
		   message for more details */

		CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
		CefRefPtr<CefBrowser> browser = context->GetBrowser();

		context->Enter();

		APIFunctionItem item = cefClientFunctions[name];

		CefRefPtr<CefProcessMessage> msg =
			CefProcessMessage::Create(item.message);

		CefRefPtr<CefListValue> args = msg->GetArgumentList();

		// Set header
		args->SetInt(args->GetSize(), 4); // header length, including first arg
		args->SetString(args->GetSize(), item.message);
		args->SetString(args->GetSize(), item.fullName);
		args->SetString(args->GetSize(), name);

		// Convert args except functions to JSON
		for (int i = 0; i < arguments.size() && !arguments[i]->IsFunction(); ++i) {
			CefV8ValueList JSON_value_list;
			JSON_value_list.push_back(arguments[i]);

			// Call global JSON.stringify JS function to stringify JSON value
			CefRefPtr<CefV8Value> JSON_string_value =
				context->GetGlobal()->GetValue("JSON")->GetValue("stringify")->ExecuteFunction(NULL, JSON_value_list);

			args->SetString(args->GetSize(), JSON_string_value->GetStringValue());
		}

		if (arguments.size() > 0 && arguments[arguments.size() - 1]->IsFunction()) {
			callbackId++;
			callbackMap[callbackId] = arguments[arguments.size() - 1];

			args->SetInt(args->GetSize(), callbackId);
		} else {
			args->SetInt(args->GetSize(), -1); // invalid callback ID
		}

		// Send message to CefClient
		browser->SendProcessMessage(PID_BROWSER, msg);

		context->Exit();

		return true;
	} else {
		/* Function does not exist. */
		return false;
	}

	return true;
}
