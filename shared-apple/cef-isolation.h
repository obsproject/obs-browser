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

#pragma once

#import <Foundation/Foundation.h>

@protocol CEFIsolatedClient;
@class BrowserSettingsBridge;
@class ObsMouseEventBridge;
@class ObsKeyEventBridge;

@protocol CEFIsolationService
- (oneway void)registerClient:(id<CEFIsolatedClient>)client;
- (BOOL)createSurface:(const int)browserIdentifier width:(const int)width
	height:(const int)height surfaceHandle:(int *)surfaceHandle;
- (void)destroySurface:(const int)browserIdentifier
	surfaceHandle:(const int)surfaceHandle;
- (void)onDraw:(const int)browserIdentifier width:(const int)width
	height:(const int)height surfaceHandle:(const int)surfaceHandle;
- (void)invalidateClient:(id)client withException:(NSException *)exception;
@end

@protocol CEFIsolatedClient
- (oneway void)registerServer:(id<CEFIsolationService>)server;
- (int)createBrowser:(BrowserSettingsBridge *)browserSettings;
- (void)destroyBrowser:(const int)browserIdentifier;
- (oneway void)tickBrowser:(const int)browserIdentifier;
- (void)sendMouseClick:(const int) browserIdentifier
	event:(ObsMouseEventBridge *)event type:(const int)type
	mouseUp:(BOOL)mouseUp clickCount:(const int)clickCount;
- (oneway void)sendMouseMove:(const int) browserIdentifier
	event:(ObsMouseEventBridge *)event mouseLeave:(BOOL)mouseLeave;
- (void)sendMouseWheel:(const int)browserIdentifier
	event:(ObsMouseEventBridge *)event xDelta:(int)xDelta
	yDelta:(int)yDelta;
- (oneway void)sendFocus:(const int)browserIdentifier focus:(BOOL)focus;
- (void)sendKeyClick:(const int) browserIdentifier
	event:(bycopy ObsKeyEventBridge *)event keyUp:(BOOL)keyUp;
@end


