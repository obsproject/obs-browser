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

#import <Cocoa/Cocoa.h>

#include <sys/event.h>

#include <include/cef_app.h>
#include <include/cef_browser.h>

#import "cef-logging.h"
#import "cef-isolation.h"

#import "cef-isolated-client.h"
#include "browser-app.hpp"
#include "browser-scheme.hpp"

#include "objc/runtime.h"

#import "service-connection-delegate.h"

void processTerminatedEvent(CFFileDescriptorRef fdref,
		CFOptionFlags callBackTypes, void* info)
{
	(void)callBackTypes;
	(void)info;

	CFFileDescriptorInvalidate(fdref);
	CFRelease(fdref);
	CEFLogError(@"Parent process died, exiting isolated CEF process.");
	exit(EXIT_SUCCESS);
}


void hookParentDeath()
{
	int fd = kqueue();
	struct kevent kev;
	EV_SET(&kev, getppid(), EVFILT_PROC, EV_ADD|EV_ENABLE, NOTE_EXIT,
		       0, NULL);
	kevent(fd, &kev, 1, NULL, 0, NULL);
	CFFileDescriptorRef fdref = CFFileDescriptorCreate(kCFAllocatorDefault,
			fd, true, processTerminatedEvent, NULL);
	CFFileDescriptorEnableCallBacks(fdref, kCFFileDescriptorReadCallBack);
	CFRunLoopSourceRef source =
			CFFileDescriptorCreateRunLoopSource(kCFAllocatorDefault,
			fdref, 0);
	CFRunLoopAddSource(CFRunLoopGetMain(), source, kCFRunLoopDefaultMode);
	CFRelease(source);
}

int main (int argc, const char * argv[])
{

	hookParentDeath();

	@autoreleasepool {
		if (argc != 2) {
			CEFLogError(@"Invalid arguments send to client");
			return -1;
		}

		NSString *uniqueClientName =
				[NSString stringWithUTF8String: argv[1]];

		CEFLogDebug(@"Process created with argument: %@",
				uniqueClientName);


		CefMainArgs mainArgs;
		CefSettings settings;
		
		settings.log_severity = LOGSEVERITY_DEFAULT;
		settings.windowless_rendering_enabled = true;

		CefRefPtr<BrowserApp> app(new BrowserApp());

		CEFLogDebug(@"Initializing CEF Runtime");
		if (CefInitialize(mainArgs, settings, app, nullptr) != 1) {
			CEFLogError(@"Could not initialize CEF Runtime");
		}

		ServiceConnectionDelegate *delegate =
				[[[ServiceConnectionDelegate alloc]
					 initWithUniqueName: uniqueClientName]
				 autorelease];


		// Eventually move this to NSThread alloc/init so we can
		// attach lifetime observers before it starts
		[NSThread detachNewThreadSelector:
				@selector(createConnectionThread)
				toTarget: delegate withObject: nil];

		CefRefPtr<BrowserSchemeHandlerFactory> factory =
				CefRefPtr<BrowserSchemeHandlerFactory>(
					new BrowserSchemeHandlerFactory());

		CefRegisterSchemeHandlerFactory("http", "absolute", factory);

		CefRunMessageLoop();

		CefShutdown();

		[delegate shutdown];
	}

	return 0;
}
