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
#include <jansson.h>

#include "browser-types.h"

@protocol CEFIsolatedClient;
@class BrowserSettingsBridge;
@class ObsMouseEventBridge;
@class ObsKeyEventBridge;

@protocol CEFIsolationService
- (oneway void)registerClient:(id<CEFIsolatedClient>)client;
- (BOOL)createSurface:(int)browserIdentifier width:(int)width height:(int)height
		surfaceHandle:(BrowserSurfaceHandle *)surfaceHandle;
- (void)destroySurface:(int)browserIdentifier
		surfaceHandle:(BrowserSurfaceHandle)surfaceHandle;
- (void)onDraw:(int)browserIdentifier width:(int)width
		height:(int)height
		surfaceHandle:(BrowserSurfaceHandle)surfaceHandle;
- (void)invalidateClient:(id)client withException:(NSException *)exception;
- (const char*)getCurrentSceneJSONData;
@end

@protocol CEFIsolatedClient
- (oneway void)registerServer:(id<CEFIsolationService>)server;
- (int)createBrowser:(BrowserSettingsBridge *)browserSettings;
- (void)destroyBrowser:(int)browserIdentifier;
- (oneway void)tickBrowser:(int)browserIdentifier;
- (void)sendMouseClick:(int) browserIdentifier
		event:(ObsMouseEventBridge *)event type:(int)type
		mouseUp:(BOOL)mouseUp clickCount:(int)clickCount;
- (oneway void)sendMouseMove:(int)browserIdentifier
		event:(ObsMouseEventBridge *)event mouseLeave:(BOOL)mouseLeave;
- (void)sendMouseWheel:(int)browserIdentifier event:(ObsMouseEventBridge *)event
		xDelta:(int)xDelta yDelta:(int)yDelta;
- (oneway void)sendFocus:(int)browserIdentifier focus:(BOOL)focus;
- (void)sendKeyClick:(int) browserIdentifier
		event:(bycopy ObsKeyEventBridge *)event keyUp:(BOOL)keyUp;
- (void)refreshPageNoCache:(const int)browserIdentifier;
- (void)executeVisiblityJSCallback:(const int)browserIdentifier
		visible:(BOOL)visible;
- (void)executeActiveJSCallback:(const int)browserIdentifier
		active:(BOOL)active;
- (void)executeSceneChangeJSCallback:(const char *)name;
- (void)dispatchJSEvent:(const char *)eventName data:(const char*) jsonString;
@end


