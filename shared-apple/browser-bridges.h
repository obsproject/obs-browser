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

struct BrowserSettings;
struct obs_mouse_event;
struct obs_key_event;

@interface BrowserSettingsBridge : NSObject

@property (readonly)NSString *url;
@property (readonly)unsigned int width;
@property (readonly)unsigned int height;
@property (readonly)unsigned int fps;
@property (readonly)NSString *css;

+ (id)fromBrowserSettings: (const BrowserSettings &)browserSettings;

@end

@interface ObsMouseEventBridge : NSObject

@property (readonly)unsigned int modifiers;
@property (readonly)int x;
@property (readonly)int y;

+ (id)fromObsMouseEvent: (const obs_mouse_event *)event;

@end

@interface ObsKeyEventBridge : NSObject

@property (readonly)unsigned int modifiers;
@property (readonly)NSString *text;
@property (readonly)unsigned int nativeModifiers;
@property (readonly)unsigned int nativeScanCode;
@property (readonly)unsigned int nativeVirtualKey;

+ (id)fromObsKeyEvent: (const obs_key_event *)event;

@end

