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

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100)
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <include/cef_app.h>
#include <include/cef_base.h>
#include <include/cef_task.h>
#include <include/cef_client.h>
#include <include/cef_parser.h>
#include <include/cef_scheme.h>
#include <include/cef_version.h>
#include <include/cef_render_process_handler.h>
#include <include/cef_request_context_handler.h>
#if defined(__APPLE__) && !defined(BROWSER_LEGACY)
#include "include/wrapper/cef_library_loader.h"
#endif

#if CHROME_VERSION_BUILD < 3507
#define ENABLE_WASHIDDEN 1
#else
#define ENABLE_WASHIDDEN 0
#endif

#if CHROME_VERSION_BUILD >= 3770
#define SendBrowserProcessMessage(browser, pid, msg) \
	browser->GetMainFrame()->SendProcessMessage(pid, msg);
#else
#define SendBrowserProcessMessage(browser, pid, msg) \
	browser->SendProcessMessage(pid, msg);
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#else
#pragma GCC diagnostic pop
#endif
