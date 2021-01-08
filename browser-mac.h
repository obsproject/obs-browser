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

#include <functional>
#include <mutex>
#include <deque>

#include "cef-headers.hpp"
#include <string>

typedef std::function<void()> MessageTask;
typedef std::function<void(CefRefPtr<CefBrowser>)> BrowserFunc;

struct Task {
    CefRefPtr<CefBrowser> browser;
    BrowserFunc func;

    inline Task() {}
    inline Task(CefRefPtr<CefBrowser> browser_, BrowserFunc func_)
        : browser(browser_), func(func_)
    {
    }
};

extern std::mutex browserTaskMutex;
extern std::deque<Task> browserTasks;

bool ExecuteNextBrowserTask();
void ExecuteTask(MessageTask task);
void ExecuteSyncTask(MessageTask task);
void DoCefMessageLoop(int ms);
void Process();
void QueueBrowserTask(CefRefPtr<CefBrowser> browser,
                    BrowserFunc func);
bool isMainThread();
std::string getExecutablePath();
bool isHighThanBigSur();
