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
#include <iostream>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef USE_UI_LOOP
#include <util/base.h>
#include <util/platform.h>
#include <util/threading.h>
#ifdef WIN32
#include <QTimer>
#endif
#ifdef __APPLE__
#include "browser-mac.h"
#endif
#endif

using namespace json11;

CefRefPtr<CefRenderProcessHandler> BrowserApp::GetRenderProcessHandler()
{
	return this;
}

CefRefPtr<CefBrowserProcessHandler> BrowserApp::GetBrowserProcessHandler()
{
	return this;
}

void BrowserApp::OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar)
{
#if CHROME_VERSION_BUILD >= 3683
	registrar->AddCustomScheme("http",
				   CEF_SCHEME_OPTION_STANDARD |
					   CEF_SCHEME_OPTION_CORS_ENABLED);
#elif CHROME_VERSION_BUILD >= 3029
	registrar->AddCustomScheme("http", true, false, false, false, true,
				   false);
#else
	registrar->AddCustomScheme("http", true, false, false, false, true);
#endif
}

void BrowserApp::AddFlag(bool flag) 
{
    std::lock_guard<std::mutex> guard(flag_mutex);
    this->media_flags.push(flag);
}

void BrowserApp::OnBeforeChildProcessLaunch(
	CefRefPtr<CefCommandLine> command_line)
{
#ifdef _WIN32
	std::string pid = std::to_string(GetCurrentProcessId());
	command_line->AppendSwitchWithValue("parent_pid", pid);
#else
#endif

    std::lock_guard<std::mutex> guard(flag_mutex);
    if (this->media_flag != -1) {
        if (this->media_flag) {
            command_line->AppendSwitch("enable-media-stream");
        }
        this->media_flag = -1;
    }
    else if (this->media_flags.size()) {
        bool flag = media_flags.front();
        media_flags.pop();
        if (flag) {
            command_line->AppendSwitch("enable-media-stream");
        }
    }
}

void BrowserApp::OnBeforeCommandLineProcessing(
	const CefString &, CefRefPtr<CefCommandLine> command_line)
{
	if (!shared_texture_available) {
		bool enableGPU = command_line->HasSwitch("enable-gpu");
		CefString type = command_line->GetSwitchValue("type");

		if (!enableGPU && type.empty()) {
			command_line->AppendSwitch("disable-gpu");
			command_line->AppendSwitch("disable-gpu-compositing");
		}
	}

	if (command_line->HasSwitch("disable-features")) {
		// Don't override existing, as this can break OSR
		std::string disableFeatures =
			command_line->GetSwitchValue("disable-features");
		disableFeatures += ",HardwareMediaKeyHandling"
#ifdef __APPLE__
				   ",NetworkService"
#endif
			;
		command_line->AppendSwitchWithValue("disable-features",
						    disableFeatures);
	} else {
		command_line->AppendSwitchWithValue("disable-features",
						    "HardwareMediaKeyHandling"
#ifdef __APPLE__
						    ",NetworkService"
#endif
		);
	}

	command_line->AppendSwitchWithValue("autoplay-policy",
					    "no-user-gesture-required");
}

void BrowserApp::OnContextCreated(CefRefPtr<CefBrowser> browser,
				  CefRefPtr<CefFrame>,
				  CefRefPtr<CefV8Context> context)
{
	CefRefPtr<CefV8Value> globalObj = context->GetGlobal();

	CefRefPtr<CefV8Value> obsStudioObj = CefV8Value::CreateObject(0, 0);
	globalObj->SetValue("obsstudio", obsStudioObj,
			    V8_PROPERTY_ATTRIBUTE_NONE);

	CefRefPtr<CefV8Value> pluginVersion =
		CefV8Value::CreateString(OBS_BROWSER_VERSION_STRING);
	obsStudioObj->SetValue("pluginVersion", pluginVersion,
			       V8_PROPERTY_ATTRIBUTE_NONE);

	CefRefPtr<CefV8Value> getCurrentScene =
		CefV8Value::CreateFunction("getCurrentScene", this);
	obsStudioObj->SetValue("getCurrentScene", getCurrentScene,
			       V8_PROPERTY_ATTRIBUTE_NONE);

	CefRefPtr<CefV8Value> getStatus =
		CefV8Value::CreateFunction("getStatus", this);
	obsStudioObj->SetValue("getStatus", getStatus,
			       V8_PROPERTY_ATTRIBUTE_NONE);

	CefRefPtr<CefV8Value> saveReplayBuffer =
		CefV8Value::CreateFunction("saveReplayBuffer", this);
	obsStudioObj->SetValue("saveReplayBuffer", saveReplayBuffer,
			       V8_PROPERTY_ATTRIBUTE_NONE);

#if !ENABLE_WASHIDDEN
	int id = browser->GetIdentifier();
	if (browserVis.find(id) != browserVis.end()) {
		SetDocumentVisibility(browser, browserVis[id]);
	}
#endif
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

#if !ENABLE_WASHIDDEN
void BrowserApp::SetFrameDocumentVisibility(CefRefPtr<CefBrowser> browser,
					    CefRefPtr<CefFrame> frame,
					    bool isVisible)
{
	CefRefPtr<CefV8Context> context = frame->GetV8Context();

	context->Enter();

	CefRefPtr<CefV8Value> globalObj = context->GetGlobal();

	CefRefPtr<CefV8Value> documentObject = globalObj->GetValue("document");

	if (!!documentObject) {
		documentObject->SetValue("hidden",
					 CefV8Value::CreateBool(!isVisible),
					 V8_PROPERTY_ATTRIBUTE_READONLY);

		documentObject->SetValue(
			"visibilityState",
			CefV8Value::CreateString(isVisible ? "visible"
							   : "hidden"),
			V8_PROPERTY_ATTRIBUTE_READONLY);

		std::string script = "new CustomEvent('visibilitychange', {});";

		CefRefPtr<CefV8Value> returnValue;
		CefRefPtr<CefV8Exception> exception;

		/* Create the CustomEvent object
		 * We have to use eval to invoke the new operator */
		bool success = context->Eval(script, frame->GetURL(), 0,
					     returnValue, exception);

		if (success) {
			CefV8ValueList arguments;
			arguments.push_back(returnValue);

			CefRefPtr<CefV8Value> dispatchEvent =
				documentObject->GetValue("dispatchEvent");

			/* Dispatch visibilitychange event on the document
			 * object */
			dispatchEvent->ExecuteFunction(documentObject,
						       arguments);
		}
	}

	context->Exit();
}

void BrowserApp::SetDocumentVisibility(CefRefPtr<CefBrowser> browser,
				       bool isVisible)
{
	/* This method might be called before OnContextCreated
	 * call is made. We'll save the requested visibility
	 * state here, and use it later in OnContextCreated to
	 * set initial page visibility state. */
	browserVis[browser->GetIdentifier()] = isVisible;

	std::vector<int64> frameIdentifiers;
	/* Set visibility state for every frame in the browser
	 *
	 * According to the Page Visibility API documentation:
	 * https://developer.mozilla.org/en-US/docs/Web/API/Page_Visibility_API
	 *
	 * "Visibility states of an <iframe> are the same as
	 * the parent document. Hiding an <iframe> using CSS
	 * properties (such as display: none;) doesn't trigger
	 * visibility events or change the state of the document
	 * contained within the frame."
	 *
	 * Thus, we set the same visibility state for every frame of the browser.
	 */
	browser->GetFrameIdentifiers(frameIdentifiers);

	for (int64 frameId : frameIdentifiers) {
		CefRefPtr<CefFrame> frame = browser->GetFrame(frameId);

		SetFrameDocumentVisibility(browser, frame, isVisible);
	}
}
#endif

bool BrowserApp::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
#if CHROME_VERSION_BUILD >= 3770
					  CefRefPtr<CefFrame> frame,
#endif
					  CefProcessId source_process,
					  CefRefPtr<CefProcessMessage> message)
{
	DCHECK(source_process == PID_BROWSER);

	CefRefPtr<CefListValue> args = message->GetArgumentList();

	if (message->GetName() == "Visibility") {
		CefV8ValueList arguments;
		arguments.push_back(CefV8Value::CreateBool(args->GetBool(0)));

		ExecuteJSFunction(browser, "onVisibilityChange", arguments);

#if !ENABLE_WASHIDDEN
		SetDocumentVisibility(browser, args->GetBool(0));
#endif

	} else if (message->GetName() == "Active") {
		CefV8ValueList arguments;
		arguments.push_back(CefV8Value::CreateBool(args->GetBool(0)));

		ExecuteJSFunction(browser, "onActiveChange", arguments);

	} else if (message->GetName() == "DispatchJSEvent") {
		CefRefPtr<CefV8Context> context =
			browser->GetMainFrame()->GetV8Context();

		context->Enter();

		CefRefPtr<CefV8Value> globalObj = context->GetGlobal();

		std::string err;
		auto payloadJson =
			Json::parse(args->GetString(1).ToString(), err);

		Json::object wrapperJson;
		if (args->GetSize() > 1)
			wrapperJson["detail"] = payloadJson;
		std::string wrapperJsonString = Json(wrapperJson).dump();
		std::string script;

		script += "new CustomEvent('";
		script += args->GetString(0).ToString();
		script += "', ";
		script += wrapperJsonString;
		script += ");";

		CefRefPtr<CefV8Value> returnValue;
		CefRefPtr<CefV8Exception> exception;

		/* Create the CustomEvent object
		 * We have to use eval to invoke the new operator */
		context->Eval(script, browser->GetMainFrame()->GetURL(), 0,
			      returnValue, exception);

		CefV8ValueList arguments;
		arguments.push_back(returnValue);

		CefRefPtr<CefV8Value> dispatchEvent =
			globalObj->GetValue("dispatchEvent");
		dispatchEvent->ExecuteFunction(NULL, arguments);

		context->Exit();

	} else if (message->GetName() == "executeCallback") {
		CefRefPtr<CefV8Context> context =
			browser->GetMainFrame()->GetV8Context();
		CefRefPtr<CefV8Value> retval;
		CefRefPtr<CefV8Exception> exception;

		context->Enter();

		CefRefPtr<CefListValue> arguments = message->GetArgumentList();
		int callbackID = arguments->GetInt(0);
		CefString jsonString = arguments->GetString(1);

		std::string script;
		script += "JSON.parse('";
		script += arguments->GetString(1).ToString();
		script += "');";

		CefRefPtr<CefV8Value> callback = callbackMap[callbackID];
		CefV8ValueList args;

		context->Eval(script, browser->GetMainFrame()->GetURL(), 0,
			      retval, exception);

		args.push_back(retval);

		if (callback)
			callback->ExecuteFunction(NULL, args);

		context->Exit();

		callbackMap.erase(callbackID);

	} else {
		return false;
	}

	return true;
}

bool BrowserApp::Execute(const CefString &name, CefRefPtr<CefV8Value>,
			 const CefV8ValueList &arguments,
			 CefRefPtr<CefV8Value> &, CefString &)
{
	if (name == "getCurrentScene" || name == "getStatus" ||
	    name == "saveReplayBuffer") {
		if (arguments.size() == 1 && arguments[0]->IsFunction()) {
			callbackId++;
			callbackMap[callbackId] = arguments[0];
		}

		CefRefPtr<CefProcessMessage> msg =
			CefProcessMessage::Create(name);
		CefRefPtr<CefListValue> args = msg->GetArgumentList();
		args->SetInt(0, callbackId);

		CefRefPtr<CefBrowser> browser =
			CefV8Context::GetCurrentContext()->GetBrowser();
		SendBrowserProcessMessage(browser, PID_BROWSER, msg);

	} else {
		/* Function does not exist. */
		return false;
	}

	return true;
}

#if defined(USE_UI_LOOP) && defined(WIN32)
Q_DECLARE_METATYPE(MessageTask);
MessageObject messageObject;

void QueueBrowserTask(CefRefPtr<CefBrowser> browser, BrowserFunc func)
{
	std::lock_guard<std::mutex> lock(messageObject.browserTaskMutex);
	messageObject.browserTasks.emplace_back(browser, func);

	QMetaObject::invokeMethod(&messageObject, "ExecuteNextBrowserTask",
				  Qt::QueuedConnection);
}

bool MessageObject::ExecuteNextBrowserTask()
{
	Task nextTask;
	{
		std::lock_guard<std::mutex> lock(browserTaskMutex);
		if (!browserTasks.size())
			return false;

		nextTask = browserTasks[0];
		browserTasks.pop_front();
	}

	nextTask.func(nextTask.browser);
	return true;
}

void MessageObject::ExecuteTask(MessageTask task)
{
	task();
}

void MessageObject::DoCefMessageLoop(int ms)
{
	if (ms)
		QTimer::singleShot((int)ms + 2,
				   []() { CefDoMessageLoopWork(); });
	else
		CefDoMessageLoopWork();
}

void MessageObject::Process()
{
	CefDoMessageLoopWork();
}

void ProcessCef()
{
	QMetaObject::invokeMethod(&messageObject, "DoCefMessageLoop",
				  Qt::QueuedConnection, Q_ARG(int, (int)0));
}
#endif

#ifdef USE_UI_LOOP
#define MAX_DELAY (1000 / 30)

void BrowserApp::OnScheduleMessagePumpWork(int64 delay_ms)
{
	if (delay_ms < 0)
		delay_ms = 0;
	else if (delay_ms > MAX_DELAY)
		delay_ms = MAX_DELAY;

#ifdef WIN32

	if (!frameTimer.isActive()) {
		QObject::connect(&frameTimer, &QTimer::timeout, &messageObject,
				 &MessageObject::Process);
		frameTimer.setSingleShot(false);
		frameTimer.start(33);
	}

	QMetaObject::invokeMethod(&messageObject, "DoCefMessageLoop",
				  Qt::QueuedConnection,
				  Q_ARG(int, (int)delay_ms));
#elif __APPLE__
	DoCefMessageLoop((int)delay_ms);
#endif
}
#endif
