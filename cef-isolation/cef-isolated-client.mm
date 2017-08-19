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
#import <Foundation/Foundation.h>

#include <include/cef_app.h>
#include <include/cef_browser.h>
#include <include/cef_version.h>

// shared apple
#import "cef-isolation.h"
#import "cef-logging.h"
#import "browser-bridges.h"

// shared
#include "browser-task.hpp"
#include "browser-client.hpp"

#include "browser-handle.h"
#include "browser-render-handler.hpp"
#include "obs-browser/browser-load-handler.hpp"

#import "cef-isolated-client.h"

#import "browser-obs-bridge-mac.hpp"

@implementation CEFIsolatedClient

- (id)init
{
	if (self = [super init]) {
		_server = nil;
	}
	return self;
}

- (oneway void)registerServer:(id<CEFIsolationService>)server
{
	CEFLogDebug(@"Registered server");
	_server = server;
}

void sync_on_cef_ui(dispatch_block_t block)
{
	if ([NSThread isMainThread]) {
		block();
	} else {
		dispatch_sync(dispatch_get_main_queue(), block);
	}
}

- (void)update:(CefRefPtr<CefBrowser>)browser
{
	browser->GetHost()->WasResized();
}

- (int)createBrowser:(BrowserSettingsBridge *)browserSettings
{
	__block CefRefPtr<CefBrowser> browser;

	__block std::shared_ptr<BrowserHandle> browserHandle(
		new BrowserHandle(browserSettings.width,
			browserSettings.height, _server));

	BrowserOBSBridge *browserOBSBridge = new BrowserOBSBridgeMac(_server);

	sync_on_cef_ui(^{

		CefRefPtr<BrowserRenderHandler> browserRenderHandler =
				new BrowserRenderHandler(browserHandle);
				
		CefRefPtr<BrowserLoadHandler> loadHandler =
				new BrowserLoadHandler(std::string([browserSettings.css UTF8String]));

		CefRefPtr<BrowserClient> browserClient =
				new BrowserClient(browserRenderHandler.get(),loadHandler, browserOBSBridge);

		CefWindowInfo windowInfo;
		windowInfo.view = nullptr;
		windowInfo.parent_view = nullptr;
#if CHROME_VERSION_BUILD < 3071
		windowInfo.transparent_painting_enabled = true;
#endif
		windowInfo.width = browserSettings.width;
		windowInfo.height = browserSettings.height;
		windowInfo.windowless_rendering_enabled = true;

		
		CefBrowserSettings cefBrowserSettings;
		cefBrowserSettings.windowless_frame_rate = browserSettings.fps;

		//TODO Switch to using async browser creation
		// Less chance of freezes here taking the whole
		// process down
		browser = CefBrowserHost::CreateBrowserSync(windowInfo,
				browserClient.get(),
				[browserSettings.url UTF8String],
				cefBrowserSettings, nil);
	});

	if (browser != nil) {
		browserHandle->SetBrowser(browser.get());
		map[browser->GetIdentifier()] = browserHandle;
		return browser->GetIdentifier();
	} else {
		return 0;
	}
}

- (oneway void)tickBrowser: (const int)browserIdentifier {
	(void)browserIdentifier;
	// Not implemented
}

typedef void(^event_block_t)(std::shared_ptr<BrowserHandle>);

- (void)sendEvent:(const int)browserIdentifier
	event:(event_block_t)event
{
	if (map.count(browserIdentifier) == 1) {
		SharedBrowserHandle browserHandle =
			map[browserIdentifier];
		sync_on_cef_ui(^{
			event(browserHandle);
		});
	}
}

- (void)sendEventToAllBrowsers:(event_block_t)event
{
    for (auto& x: map) {
		SharedBrowserHandle browserHandle = x.second;
		sync_on_cef_ui(^{
			event(browserHandle);
		});
	}
}

- (void)sendMouseClick:(const int)browserIdentifier
	event:(ObsMouseEventBridge *)event type:(const int)type
	mouseUp:(BOOL)mouseUp clickCount:(const int)clickCount
{
	[self sendEvent:browserIdentifier
		event:^(SharedBrowserHandle browserHandle)
	{
		CefMouseEvent mouseEvent;
		mouseEvent.x = event.x;
		mouseEvent.y = event.y;
		mouseEvent.modifiers = event.modifiers;
		CefRefPtr<CefBrowserHost> host =
				browserHandle->GetBrowser()->GetHost();

		CefBrowserHost::MouseButtonType buttonType =
				(CefBrowserHost::MouseButtonType)type;

		host->SendMouseClickEvent(mouseEvent, buttonType,
				mouseUp, clickCount);
	}];
}


- (oneway void)sendMouseMove:(const int)browserIdentifier
	event:(ObsMouseEventBridge *)event mouseLeave:(BOOL)mouseLeave
{
	[self sendEvent:browserIdentifier
		event:^(SharedBrowserHandle browserHandle)
	{
		CefMouseEvent mouseEvent;
		mouseEvent.x = event.x;
		mouseEvent.y = event.y;
		mouseEvent.modifiers = event.modifiers;

		CefRefPtr<CefBrowserHost> host =
				browserHandle->GetBrowser()->GetHost();
		host->SendMouseMoveEvent(mouseEvent, mouseLeave);
	}];
}

- (void)sendMouseWheel:(const int)browserIdentifier
	event:(ObsMouseEventBridge *)event xDelta:(int)xDelta
	yDelta:(int)yDelta
{
	[self sendEvent:browserIdentifier
		event:^(SharedBrowserHandle browserHandle)
	 {
		CefMouseEvent mouseEvent;
		mouseEvent.x = event.x;
		mouseEvent.y = event.y;
		mouseEvent.modifiers = event.modifiers;

		CefRefPtr<CefBrowserHost> host =
				browserHandle->GetBrowser()->GetHost();
		host->SendMouseWheelEvent(mouseEvent, xDelta, yDelta);
	}];
}

- (oneway void)sendFocus:(const int)browserIdentifier focus:(BOOL)focus
{
	[self sendEvent:browserIdentifier
		event:^(SharedBrowserHandle browserHandle)
	{
		CefRefPtr<CefBrowserHost> host =
				browserHandle->GetBrowser()->GetHost();
		host->SendFocusEvent(focus);
		[[host->GetWindowHandle() window]
				makeFirstResponder: host->GetWindowHandle()];
	}];
}

enum AppleVirtualKey {
	kVK_Return     = 0x24,
	kVK_Command    = 0x37,
	kVK_Shift      = 0x38,
	kVK_Option     = 0x3A,
	kVK_Control    = 0x3B,
};

void getUnmodifiedCharacter(ObsKeyEventBridge *event, UniChar &character)
{
	@autoreleasepool {
		CGEventRef cgEvent = CGEventCreateKeyboardEvent(NULL,
			event.nativeVirtualKey, false);

		NSEvent *e = [NSEvent eventWithCGEvent: cgEvent];
		NSString *s = [e charactersIgnoringModifiers];
		if (s && s.length)
			character = [s characterAtIndex: 0];

		CFRelease(cgEvent);
	}
}

- (void)sendKeyClick:(const int)browserIdentifier
	       event:(bycopy ObsKeyEventBridge *)event keyUp:(BOOL)keyUp
{
	[self sendEvent:browserIdentifier
		  event:^(SharedBrowserHandle browserHandle)
	{
		CefRefPtr<CefBrowserHost> host =
			 browserHandle->GetBrowser()->GetHost();



		NSEvent *nsEvent =
			 [NSEvent keyEventWithType: keyUp ? NSKeyUp : NSKeyDown
					  location: NSMakePoint(0,0)
				     modifierFlags: [event nativeModifiers]
					 timestamp: 0
				      windowNumber: 0
					   context: nil
					characters: [event text]
		       charactersIgnoringModifiers: nil
					 isARepeat: false
					   keyCode: event.nativeVirtualKey];


		CefKeyEvent keyEvent;
		keyEvent.modifiers = [event modifiers];

		keyEvent.native_key_code = event.nativeVirtualKey;

		if (event.text.length) {
			keyEvent.character =
			[event.text characterAtIndex: 0];

			getUnmodifiedCharacter(event,
					keyEvent.unmodified_character);
		}


		if (keyUp) {
			keyEvent.type = KEYEVENT_KEYUP;
			host->SendKeyEvent(keyEvent);
		} else {
			// Somehow return is not handled correctly
			if ([event nativeVirtualKey] == kVK_Return) {
				keyEvent.type = KEYEVENT_CHAR;
				host->SendKeyEvent(keyEvent);
				return;
			}

			host->SendKeyEvent(keyEvent);
		}
	 }];
}

- (void)executeVisiblityJSCallback:(const int)browserIdentifier visible:(BOOL)visible
{
	[self sendEvent:browserIdentifier
		  event:^(SharedBrowserHandle browserHandle)
	{
		CefRefPtr<CefBrowser> browser = browserHandle->GetBrowser();

		CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("Visibility");
		CefRefPtr<CefListValue> args = msg->GetArgumentList();
		args->SetBool(0, visible);
		browser->SendProcessMessage(PID_RENDERER, msg);
	}];
}

- (void)executeActiveJSCallback:(const int)browserIdentifier active:(BOOL)active
{
	[self sendEvent:browserIdentifier
			event:^(SharedBrowserHandle browserHandle)
	{
		CefRefPtr<CefBrowser> browser = browserHandle->GetBrowser();

		CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("Active");
		CefRefPtr<CefListValue> args = msg->GetArgumentList();
		args->SetBool(0, active);
		browser->SendProcessMessage(PID_RENDERER, msg);
	}];
}

- (void)executeSceneChangeJSCallback:(const char *)name
{
	[self sendEventToAllBrowsers:^(SharedBrowserHandle browserHandle)
	{
		CefRefPtr<CefBrowser> browser = browserHandle->GetBrowser();

		CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("SceneChange");
		CefRefPtr<CefListValue> args = msg->GetArgumentList();
		args->SetString(0, name);
		browser->SendProcessMessage(PID_RENDERER, msg);
	}];
}

- (void)refreshPageNoCache:(const int)browserIdentifier
{
    [self sendEvent:browserIdentifier
              event:^(SharedBrowserHandle browserHandle)
     {
         CefRefPtr<CefBrowser> browser = browserHandle->GetBrowser();
         
         browser->ReloadIgnoreCache();
     }];
}

-(void)dispatchJSEvent:(const char *)eventName data:(const char *)jsonString
{
	[self sendEventToAllBrowsers:^(SharedBrowserHandle browserHandle)
	{
		CefRefPtr<CefBrowser> browser = browserHandle->GetBrowser();

		CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("DispatchJSEvent");
		CefRefPtr<CefListValue> args = msg->GetArgumentList();
		args->SetString(0, eventName);
		args->SetString(1, jsonString);

		browser->SendProcessMessage(PID_RENDERER, msg);
	}];
}

- (void)destroyBrowser:(const int)browserIdentifier {
	if (map.count(browserIdentifier) == 1) {
		std::shared_ptr<BrowserHandle> browserHandle =
				map[browserIdentifier];
		sync_on_cef_ui(^{
			browserHandle->Shutdown();
		});
		map.erase(browserIdentifier);
	}
}

@end
