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
#include "browser-mac.h"
#include <mach-o/dyld.h>
#include <sys/types.h>
#include <sys/sysctl.h>

#import "Foundation/Foundation.h"
#import <Cocoa/Cocoa.h>

#include "obs.h"

std::mutex browserTaskMutex;
std::deque<Task> browserTasks;

bool ExecuteNextBrowserTask()
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

void ExecuteTask(MessageTask task)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        task();
    });
}

void ExecuteSyncTask(MessageTask task)
{
    dispatch_sync(dispatch_get_main_queue(), ^{
        task();
    });
}

void DoCefMessageLoop(int ms)
{
    dispatch_async(dispatch_get_main_queue(), ^{
		CefDoMessageLoopWork();
    });
}

void Process()
{
    dispatch_async(dispatch_get_main_queue(), ^{
        CefDoMessageLoopWork();
    });
}

void QueueBrowserTask(CefRefPtr<CefBrowser> browser, BrowserFunc func)
{
	std::lock_guard<std::mutex> lock(browserTaskMutex);
	browserTasks.emplace_back(browser, func);

    dispatch_async(dispatch_get_main_queue(), ^{
        ExecuteNextBrowserTask();
    });
}

bool isMainThread()
{
    return [NSThread isMainThread];
}

std::string getExecutablePath()
{
    char path[1024];
    uint32_t size = sizeof(path);
    _NSGetExecutablePath(path, &size);
    return path;
}

bool isHighThanBigSur()
{
    char buf[100];
    size_t buflen = 100;
    if (sysctlbyname("machdep.cpu.brand_string", &buf, &buflen, NULL, 0) < 0)
        return false;

    NSOperatingSystemVersion OSversion = [NSProcessInfo processInfo].operatingSystemVersion;
    return ((OSversion.majorVersion >= 10 && OSversion.minorVersion >= 16) ||
        OSversion.majorVersion >= 11) && strcmp("Apple M1", buf) != 0;
}