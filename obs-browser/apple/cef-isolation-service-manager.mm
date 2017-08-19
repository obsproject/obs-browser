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

#include <string>
#import <Foundation/Foundation.h>

// shared
#include "browser-listener.hpp"
#include "browser-settings.hpp"
#include "browser-manager.hpp"

// shared apple
#import "cef-logging.h"
#import "cef-isolation.h"
#import "browser-bridges.h"

#import "cef-isolation-service.h"
#import "client-connection-delegate.h"

#include "cef-isolation-service-manager.h"



NSString *CreateUUID()
{
	CFUUIDRef uuid = CFUUIDCreate(NULL);
	CFStringRef str = CFUUIDCreateString(NULL, uuid);
	CFRelease(uuid);
	return (NSString *)CFBridgingRelease(str);
}

CEFIsolationServiceManager::CEFIsolationServiceManager()
{
	cefIsolationServiceLock = [[NSObject alloc] init];
}

CEFIsolationServiceManager::~CEFIsolationServiceManager()
{
	[cefIsolationServiceLock release];
}

void CEFIsolationServiceManager::Startup()
{

	_uniqueClientName = CreateUUID();
	_cefIsolationService = [[CEFIsolationService alloc] init];
	delegate = [[ClientConnectionDelegate alloc]
			initWithManager: this];

	// Eventually move this to NSThread alloc/init so we can
	// attach lifetime observers before it starts
	[NSThread detachNewThreadSelector: @selector(createConnectionThread)
			toTarget: delegate withObject: nil];
}

void CEFIsolationServiceManager::Shutdown()
{
	[delegate shutdown];
	_cefIsolationService = nil;
	_uniqueClientName = nil;
	delegate = nil;
}

void CEFIsolationServiceManager::Restart()
{
	[delegate restart];
}

int CEFIsolationServiceManager::CreateBrowser(
		const BrowserSettings &browserSettings,
		const std::shared_ptr<BrowserListener> &browserListener)
{
	id<CEFIsolatedClient> cefIsolatedClient = nil;
	int browserIdentifier = 0;

	@synchronized(cefIsolationServiceLock) {
		if ([_cefIsolationService client] != nil) {
			cefIsolatedClient = [_cefIsolationService client];
		}
	}
	@try {
		@autoreleasepool {
			BrowserSettingsBridge *browserSettingsBridge =
					[[BrowserSettingsBridge
						fromBrowserSettings:
						browserSettings] autorelease];

			browserIdentifier = [cefIsolatedClient createBrowser:
					browserSettingsBridge];
		}
		if (browserIdentifier != 0) {
			[_cefIsolationService addListener: browserListener
					browserIdentifier: browserIdentifier];
		}

	}
	@catch (NSException *exception) {}
	@finally {
		return browserIdentifier;
	}
}

void CEFIsolationServiceManager::DestroyBrowser(int browserIdentifier)
{
	id<CEFIsolatedClient> cefIsolatedClient = nil;

	// Do we really need to synchronize on this?
	@synchronized(cefIsolationServiceLock) {
		if ([_cefIsolationService client] != nil) {
			cefIsolatedClient = [_cefIsolationService client];
		}
	}
	@try {
		[cefIsolatedClient destroyBrowser: browserIdentifier];

	}
	@catch (NSException *exception) {}
	@finally {
		[_cefIsolationService removeListener: browserIdentifier];
	}
}

void CEFIsolationServiceManager::TickBrowser(int browserIdentifier)
{
	id<CEFIsolatedClient> cefIsolatedClient =
		[_cefIsolationService client];

	@try {
		[cefIsolatedClient tickBrowser: browserIdentifier];
	}
	@catch (NSException *exception) {}
}


void CEFIsolationServiceManager::SendMouseClick(int browserIdentifier,
		const struct obs_mouse_event *event, int32_t type, bool mouseUp,
		uint32_t clickCount)
{
	id<CEFIsolatedClient> cefIsolatedClient =
		[_cefIsolationService client];
	@try {
		@autoreleasepool {
			ObsMouseEventBridge *obsMouseEventBridge =
					[[ObsMouseEventBridge fromObsMouseEvent:
						event]
					autorelease];

			[cefIsolatedClient sendMouseClick:browserIdentifier
					event:obsMouseEventBridge type:type
					mouseUp:mouseUp clickCount:clickCount];
		}
	}
	@catch (NSException *exception) {}
}

void CEFIsolationServiceManager::SendMouseMove(int browserIdentifier,
		const struct obs_mouse_event *event, bool mouseLeave)
{
	id<CEFIsolatedClient> cefIsolatedClient =
		[_cefIsolationService client];
	@try {
		@autoreleasepool {
			ObsMouseEventBridge *obsMouseEventBridge =
					[[ObsMouseEventBridge fromObsMouseEvent:
						event]
					autorelease];

			[cefIsolatedClient sendMouseMove:browserIdentifier
					event:obsMouseEventBridge
					mouseLeave:mouseLeave];
		}
	}
	@catch (NSException *exception) {}
}

void CEFIsolationServiceManager::SendMouseWheel(int browserIdentifier,
		const struct obs_mouse_event *event, int xDelta, int yDelta)
{
	id<CEFIsolatedClient> cefIsolatedClient =
		[_cefIsolationService client];
	@try {
		@autoreleasepool {
			ObsMouseEventBridge *obsMouseEventBridge =
					[[ObsMouseEventBridge fromObsMouseEvent:
						event]
					autorelease];

			[cefIsolatedClient sendMouseWheel:browserIdentifier
				event:obsMouseEventBridge xDelta:xDelta
				yDelta:yDelta];
		}
	}
	@catch (NSException *exception) {}
}

void CEFIsolationServiceManager::SendFocus(int browserIdentifier, bool focus)
{
	id<CEFIsolatedClient> cefIsolatedClient =
		[_cefIsolationService client];
	@try {
		[cefIsolatedClient sendFocus:browserIdentifier
				focus:focus];
	}
	@catch (NSException *exception) {}
}

void CEFIsolationServiceManager::SendKeyClick(int browserIdentifier,
		const struct obs_key_event *event, bool keyUp)
{
	id<CEFIsolatedClient> cefIsolatedClient =
		[_cefIsolationService client];
	@try {
		@autoreleasepool {
			ObsKeyEventBridge *obsKeyEventBridge =
					[[ObsKeyEventBridge fromObsKeyEvent:
						event]
					autorelease];

			[cefIsolatedClient sendKeyClick:browserIdentifier
					event:obsKeyEventBridge keyUp:keyUp];
		}
	}
	@catch (NSException *exception) {}
}

void CEFIsolationServiceManager::ExecuteVisiblityJSCallback(int browserIdentifier, bool visible)
{
	id<CEFIsolatedClient> cefIsolatedClient =
		[_cefIsolationService client];
	@try {
		[cefIsolatedClient executeVisiblityJSCallback:browserIdentifier visible:visible];
	}
	@catch (NSException *exception) {}
}

void CEFIsolationServiceManager::ExecuteActiveJSCallback(int browserIdentifier, bool active)
{
	id<CEFIsolatedClient> cefIsolatedClient =
	[_cefIsolationService client];
	@try {
		[cefIsolatedClient executeActiveJSCallback:browserIdentifier active:active];
	}
	@catch (NSException *exception) {}
}

void CEFIsolationServiceManager::ExecuteSceneChangeJSCallback(const char *name)
{
    id<CEFIsolatedClient> cefIsolatedClient =
    [_cefIsolationService client];
    @try {
        [cefIsolatedClient executeSceneChangeJSCallback:name];
    }
    @catch (NSException *exception) {}
}

void CEFIsolationServiceManager::RefreshPageNoCache(int browserIdentifier)
{
	id<CEFIsolatedClient> cefIsolatedClient =
		[_cefIsolationService client];
	@try {
		[cefIsolatedClient refreshPageNoCache:browserIdentifier];
	}
	@catch (NSException *exception) {}
}

void CEFIsolationServiceManager::DispatchJSEvent(const char *eventName, const char *jsonData)
{
	id<CEFIsolatedClient> cefIsolatedClient =
		[_cefIsolationService client];
	@try {
		[cefIsolatedClient dispatchJSEvent:eventName data:jsonData];
	}
	@catch (NSException *exception) {}
}
