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

#include <obs-module.h>
#import "browser-bridges.h"
#include "browser-settings.hpp"

@implementation BrowserSettingsBridge

- (id)initWithBrowserSettings:(const BrowserSettings &)browserSettings
{
	if (self = [super init]) {
		_url = [NSString stringWithUTF8String:
				browserSettings.url.c_str()];
		_width = browserSettings.width;
		_height = browserSettings.height;
		_fps = browserSettings.fps;
		_css = [NSString stringWithUTF8String:
				browserSettings.css.c_str()];
	}
	return self;
}

+ (id)fromBrowserSettings:(const BrowserSettings &)browserSettings
{
	return [[BrowserSettingsBridge alloc]
		initWithBrowserSettings: browserSettings];
}
@end

@implementation ObsMouseEventBridge

- (id)initWithObsMouseEvent:(const struct obs_mouse_event *)event
{
	if (self = [super init]) {
		_x = event->x;
		_y = event->y;
		_modifiers = event->modifiers;
	}

	return self;
}

+ (id)fromObsMouseEvent:(const obs_mouse_event *)event
{
	return [[ObsMouseEventBridge alloc] initWithObsMouseEvent:event];
}

@end

@implementation ObsKeyEventBridge

- (id)initWithObsKeyEvent:(const struct obs_key_event *)event
{
	if (self = [super init]) {
		_modifiers = event->modifiers;
		NSString *__text = [NSString stringWithUTF8String: event->text];
		_text = [__text copy];
		_nativeModifiers = event->native_modifiers;
		_nativeScanCode = event->native_scancode;
		_nativeVirtualKey = event->native_vkey;
	}

	return self;
}

+ (id)fromObsKeyEvent:(const obs_key_event *)event
{
	return [[ObsKeyEventBridge alloc] initWithObsKeyEvent:event];
}

@end