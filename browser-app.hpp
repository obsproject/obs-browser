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
#include <queue>
#include <unordered_map>
#include <functional>
#include "cef-headers.hpp"
#include <mutex>

typedef std::function<void(CefRefPtr<CefBrowser>)> BrowserFunc;

#if  defined(USE_UI_LOOP) && defined(WIN32)
#include <QObject>
#include <QTimer>
#include <deque>

typedef std::function<void()> MessageTask;

class MessageObject : public QObject {
	Q_OBJECT

	friend void QueueBrowserTask(CefRefPtr<CefBrowser> browser,
				     BrowserFunc func);

	struct Task {
		CefRefPtr<CefBrowser> browser;
		BrowserFunc func;

		inline Task() {}
		inline Task(CefRefPtr<CefBrowser> browser_, BrowserFunc func_)
			: browser(browser_), func(func_)
		{
		}
	};

	std::mutex browserTaskMutex;
	std::deque<Task> browserTasks;

public slots:
	bool ExecuteNextBrowserTask();
	void ExecuteTask(MessageTask task);
	void DoCefMessageLoop(int ms);
	void Process();
};

extern void QueueBrowserTask(CefRefPtr<CefBrowser> browser, BrowserFunc func);
#endif

class BrowserApp : public CefApp,
		   public CefRenderProcessHandler,
		   public CefBrowserProcessHandler,
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
		: shared_texture_available(shared_texture_available_), media_flag(-1)
	{
	}

	void AddFlag(bool flag);
	int media_flag;
	std::mutex flag_mutex;
	std::queue<bool> media_flags;
	virtual CefRefPtr<CefRenderProcessHandler>
	GetRenderProcessHandler() override;
	virtual CefRefPtr<CefBrowserProcessHandler>
	GetBrowserProcessHandler() override;
	virtual void OnBeforeChildProcessLaunch(
		CefRefPtr<CefCommandLine> command_line) override;
	virtual void OnRegisterCustomSchemes(
		CefRawPtr<CefSchemeRegistrar> registrar) override;
	virtual void OnBeforeCommandLineProcessing(
		const CefString &process_type,
		CefRefPtr<CefCommandLine> command_line) override;
	virtual void OnContextCreated(CefRefPtr<CefBrowser> browser,
				      CefRefPtr<CefFrame> frame,
				      CefRefPtr<CefV8Context> context) override;
	virtual bool
	OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
#if CHROME_VERSION_BUILD >= 3770
				 CefRefPtr<CefFrame> frame,
#endif
				 CefProcessId source_process,
				 CefRefPtr<CefProcessMessage> message) override;
	virtual bool Execute(const CefString &name,
			     CefRefPtr<CefV8Value> object,
			     const CefV8ValueList &arguments,
			     CefRefPtr<CefV8Value> &retval,
			     CefString &exception) override;

#ifdef USE_UI_LOOP
	virtual void OnScheduleMessagePumpWork(int64 delay_ms) override;
#ifdef WIN32
	QTimer frameTimer;
#endif
#endif

#if !ENABLE_WASHIDDEN
	std::unordered_map<int, bool> browserVis;

	void SetFrameDocumentVisibility(CefRefPtr<CefBrowser> browser,
					CefRefPtr<CefFrame> frame,
					bool isVisible);
	void SetDocumentVisibility(CefRefPtr<CefBrowser> browser,
				   bool isVisible);
#endif

	IMPLEMENT_REFCOUNTING(BrowserApp);
};
