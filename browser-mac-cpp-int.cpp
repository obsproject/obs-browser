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

#include "obs-browser-objc-int.hpp"
#include "browser-mac-cpp-int.hpp"
#include <obs.h>

BrowserCppInt::BrowserCppInt(void)
    : _impl ( nullptr )
{   }

void BrowserCppInt::init(void)
{
    _impl = new BrowserObjCInt();
}

BrowserCppInt::~BrowserCppInt( void )
{
    if ( _impl ) { delete _impl; _impl = nullptr; }
}

bool BrowserCppInt::ExecuteNextBrowserTask(void)
{
    return _impl->ExecuteNextBrowserTask();
}

void BrowserCppInt::ExecuteTask(MessageTask task)
{
    _impl->ExecuteTask(task);
}

void BrowserCppInt::DoCefMessageLoop(int ms)
{
    _impl->DoCefMessageLoop(ms);
}

void BrowserCppInt::Process(void)
{
    _impl->Process();
}

void BrowserCppInt::QueueBrowserTask(CefRefPtr<CefBrowser> browser,
				     BrowserFunc func)
{
    _impl->QueueBrowserTask(browser, func);
}

bool BrowserCppInt::isMainThread(void)
{
    return _impl->isMainThread();
}

std::string BrowserCppInt::getExecutablePath(void)
{
    return _impl->getExecutablePath();
}
