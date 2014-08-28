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

#include <include/cef_app.h>
#include <include/cef_browser.h>

// shared apple
#import "cef-isolation.h"
#import "cef-logging.h"
#import "browser-settings-bridge.h"

// shared
#include "browser-task.hpp"
#include "browser-client.hpp"

#include "browser-handle.h"
#include "browser-render-handler.hpp"

#import "cef-isolated-client.h"


@implementation CEFIsolatedClient

- (id)init {
	if (self = [super init]) {
		_server = nil;
	}
	return self;
}

- (oneway void)registerServer:(id<CEFIsolationService>)server {
	CEFLogDebug(@"Registered server");
	_server = server;
}
void
sync_on_cef_ui(dispatch_block_t block)
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

- (int)createBrowser:(BrowserSettingsBridge *)browserSettings{
	__block CefRefPtr<CefBrowser> browser;

	__block std::shared_ptr<BrowserHandle> browserHandle(
		new BrowserHandle(browserSettings.width,
			browserSettings.height, _server));

	sync_on_cef_ui(^{

		CefRefPtr<BrowserRenderHandler> browserRenderHandler =
			new BrowserRenderHandler(browserHandle);

		CefRefPtr<BrowserClient> browserClient =
			new BrowserClient(browserRenderHandler.get());

		CefWindowInfo windowInfo;
		windowInfo.view = nullptr;
		windowInfo.parent_view = nullptr;
		windowInfo.transparent_painting_enabled = true;
		windowInfo.width = browserSettings.width;
		windowInfo.height = browserSettings.height;
		windowInfo.windowless_rendering_enabled = true;

		
		CefBrowserSettings cefBrowserSettings;
		cefBrowserSettings.windowless_frame_rate = browserSettings.fps;

		//TODO Switch to using async browser creation
		// Less chance of freezes here taking the whole
		// process down
		browser = CefBrowserHost::CreateBrowserSync(windowInfo,
			browserClient.get(), [browserSettings.url UTF8String],
			cefBrowserSettings, nil);
	});



	if (browser != nil) {
		browserHandle->SetBrowser(browser.get());
		map[browser->GetIdentifier()] = browserHandle;
		return browser->GetIdentifier();
	} else {
		return -1;
	}
}

- (oneway void)tickBrowser: (const int)browserIdentifier {
	(void)browserIdentifier;
	// Not implemented
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