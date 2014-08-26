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

#include <map>

#include <obs-module.h>

#import "cef-isolation.h"
#import "cef-logging.h"

#import "cef-isolation-service.h"

#include "browser-listener.hpp"

@implementation CEFIsolationService {
	std::map<int, std::shared_ptr<BrowserListener> > browserListenerMap;
}

- (void)addListener: (const std::shared_ptr<BrowserListener> &)browserListener
  browserIdentifier: (const int)browserIdentifier
{
	browserListenerMap[browserIdentifier] = browserListener;
}

- (void)removeListener: (const int)browserIdentifier
{
	browserListenerMap.erase(browserIdentifier);
}

/* Protocol methods */

- (oneway void)registerClient:(id<CEFIsolatedClient>)client {
	@try {
		CEFLogDebug(@"Registered client");
		_client = client;
		CEFLogDebug(@"Attempting to register server with client");
		[_client registerServer: self];
	}
	@catch (NSException *exception) {
		[self invalidateClient:client withException:exception];
	}	
}

- (BOOL)createSurface: (const int)browserIdentifier width:(int)width
	height:(int)height surfaceHandle:(int * const)surfaceHandle
{
	if (browserListenerMap.count(browserIdentifier) == 1) {
		BrowserListener *listener =
			browserListenerMap[browserIdentifier].get();
		return listener->CreateSurface(width, height, surfaceHandle);
	}
	return false;
}
- (void)destroySurface:(const int)browserIdentifier
	surfaceHandle:(const int)surfaceHandle
{
	if (browserListenerMap.count(browserIdentifier) == 1) {
		BrowserListener *listener =
			browserListenerMap[browserIdentifier].get();
		listener->DestroySurface(surfaceHandle);
	}
}

- (void)onDraw:(const int)browserIdentifier width:(const int)width
	height:(const int)height surfaceHandle:(const int)surfaceHandle
{
	if (browserListenerMap.count(browserIdentifier) == 1) {
		BrowserListener *listener =
		browserListenerMap[browserIdentifier].get();
		listener->OnDraw(surfaceHandle, width, height);
	}
}

- (void)invalidateClient:(id)client withException:(NSException *)exception {
	UNUSED_PARAMETER(client);
	if (exception) CEFLogError(@"Client Exception: %@", exception);
	_client = nil;
}

@end