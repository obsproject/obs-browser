/******************************************************************************
    Copyright (C) 2016-2019 by Streamlabs (General Workings Inc)

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

#include <obs.h>
#include "browser-mac.hpp"
#include "obs-browser-objc-int.hpp"

@implementation MessageObject

BrowserObjCInt::BrowserObjCInt( void )
    : self( NULL )
{   }

BrowserObjCInt::~BrowserObjCInt( void )
{
    [(id)self dealloc];
}

void BrowserObjCInt::init( void )
{
    self = [[MessageObject alloc] init];
}

bool BrowserObjCInt::ExecuteNextBrowserTask()
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

void BrowserObjCInt::ExecuteTask(MessageTask task)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        task();
    });
}

void BrowserObjCInt::DoCefMessageLoop(int ms)
{
    dispatch_async(dispatch_get_main_queue(), ^{
    // if (ms)
	// 	QTimer::singleShot((int)ms + 2,
	// 			   []() { CefDoMessageLoopWork(); });
	// else
		CefDoMessageLoopWork();
    });
}

void BrowserObjCInt::Process()
{
    dispatch_async(dispatch_get_main_queue(), ^{
        CefDoMessageLoopWork();
    });
}

void BrowserObjCInt::QueueBrowserTask(CefRefPtr<CefBrowser> browser, BrowserFunc func)
{
	std::lock_guard<std::mutex> lock(browserTaskMutex);
	browserTasks.emplace_back(browser, func);

    dispatch_async(dispatch_get_main_queue(), ^{
        ExecuteNextBrowserTask();
    });
}

bool BrowserObjCInt::isMainThread()
{
    return [NSThread isMainThread];
}

@end
