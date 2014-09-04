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

// shared
#include "browser-listener.hpp"
#include "browser-settings.hpp"
#include "browser-manager.hpp"

// shared apple
#import "cef-logging.h"
#import "cef-isolation.h"
#import "browser-settings-bridge.h"

#import "cef-isolation-service.h"
#import "client-connection-delegate.h"

#include "cef-isolation-service-manager.h"



NSString *CreateUUID() {
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

void
CEFIsolationServiceManager::Startup()
{

	_uniqueClientName = CreateUUID();

	CEFLogDebug(@"Creating service %@", _uniqueClientName);

	NSString *launchPath = [NSString stringWithUTF8String:
		BrowserManager::Instance()->GetModulePath()];

	launchPath = [launchPath stringByDeletingLastPathComponent];
	launchPath = [launchPath stringByAppendingPathComponent:
		@"/CEF.app/Contents/MacOS/CEF"];

	CEFLogDebug(@"Launching child process %@", launchPath);
	isolatedClientProcess = [[[NSTask alloc] init] autorelease];
	[isolatedClientProcess setArguments: @[ _uniqueClientName ]];
	[isolatedClientProcess setLaunchPath: launchPath];

	[isolatedClientProcess launch];

	_cefIsolationService = [[CEFIsolationService alloc] init];

	delegate =
		[[ClientConnectionDelegate alloc]
			initWithManager: this];

	// Eventually move this to NSThread alloc/init so we can
	// attach lifetime observers before it starts
	[NSThread detachNewThreadSelector: @selector(createConnectionThread)
		toTarget: delegate withObject: nil];
}

void
CEFIsolationServiceManager::Shutdown()
{
	[delegate shutdown];
	_cefIsolationService = nil;
	[isolatedClientProcess terminate];
	isolatedClientProcess = nil;
	_uniqueClientName = nil;
	delegate = nil;
}


int
CEFIsolationServiceManager::CreateBrowser(
	const BrowserSettings &browserSettings,
	const std::shared_ptr<BrowserListener> &browserListener)
{
	id<CEFIsolatedClient> cefIsolatedClient = nil;
	int browserIdentifier = -1;

	@synchronized(cefIsolationServiceLock) {
		if ([_cefIsolationService client] != nil) {
			cefIsolatedClient = [_cefIsolationService client];
		}
	}
	@try {
		@autoreleasepool {
			BrowserSettingsBridge *browserSettingsBridge =
				[[BrowserSettingsBridge fromBrowserSettings:
					browserSettings] autorelease];

			browserIdentifier = [cefIsolatedClient createBrowser:
				browserSettingsBridge];
		}
		if (browserIdentifier > 0) {
			[_cefIsolationService addListener: browserListener
				browserIdentifier: browserIdentifier];
		}

	}
	@catch (NSException *exception) {
		[_cefIsolationService invalidateClient: cefIsolatedClient
			withException:exception];
	}
	@finally {
		return browserIdentifier;
	}
}

void
CEFIsolationServiceManager::DestroyBrowser(const int browserIdentifier)
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
	@catch (NSException *exception) {
		[_cefIsolationService invalidateClient: cefIsolatedClient
					 withException:exception];
	}
	@finally {
		[_cefIsolationService removeListener: browserIdentifier];
	}
}

void
CEFIsolationServiceManager::TickBrowser(const int browserIdentifier)
{
	id<CEFIsolatedClient> cefIsolatedClient =
	[_cefIsolationService client];

	@try {
		[cefIsolatedClient tickBrowser: browserIdentifier];
	}
	@catch (NSException *exception) {
		// swallow
	}
}