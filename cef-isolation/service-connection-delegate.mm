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

#import "cef-logging.h"

#import "cef-isolation.h"
#import "cef-isolated-client.h"
#import "service-connection-delegate.h"



@implementation ServiceConnectionDelegate

#define T_START 0
#define T_FINISHED 1

@synthesize cefIsolationService = _cefIsolationService;


- (ServiceConnectionDelegate *)initWithUniqueName: (NSString *)name
{
	self = [super init];
	if (self) {
		lock = [[[NSConditionLock alloc] initWithCondition: T_START]
				autorelease];
		uniqueClientName = name;
	}
	return self;
}

- (void)createConnectionThread
{
	[[NSThread currentThread] setName: @"CEF Service IO Thread"];

	@autoreleasepool {

		CEFLogDebug(@"Connecting to host process");

		[lock lockWhenCondition: T_START];

		NSRunLoop *threadRunLoop = [NSRunLoop currentRunLoop];
		connectionThread = [NSThread currentThread];

		NSConnection *connection =
		[[[NSConnection alloc] init] autorelease];


		CEFIsolatedClient *cefIsolatedClient =
		[[[CEFIsolatedClient alloc] init] autorelease];

		while(nil == _cefIsolationService)
		{
			// This connection should be moved to its own thread
			// so that calls from OBS are not delayed by
			// CEF tasks in the message loop.
			NSConnection *connection = [NSConnection
					connectionWithRegisteredName:
						uniqueClientName
					host: nil];

			[connection setRootObject: cefIsolatedClient];
			CEFLogDebug(@"Checking for host process connection...");
			_cefIsolationService = (id)[connection rootProxy];
			[[NSRunLoop currentRunLoop] runUntilDate:[NSDate
					dateWithTimeIntervalSinceNow:.1]];
		}

		CEFLogDebug(@"Connected to host process and fetched proxy service");
		CEFLogDebug(@"Attempting to register client with service");
		[_cefIsolationService registerClient: cefIsolatedClient];

		[connection addRunLoop: threadRunLoop];

		BOOL threadAlive = YES;
		while (threadAlive) {
			@autoreleasepool {
				[threadRunLoop runMode:NSDefaultRunLoopMode
						beforeDate:
						[NSDate distantFuture]];
				threadAlive = !shutdown;
			}
		}

		// invalidate connection and proxy objects
		[connection invalidate];
	}

	connectionThread = nil;

	[lock unlockWithCondition: T_FINISHED];
}

// internally only called by shutdown as a means to wake up runloop
- (void)shutdownThread
{
	shutdown = YES;
}

- (void)shutdown
{
	[self performSelector:@selector(shutdownThread)
			onThread:connectionThread withObject:nil
			waitUntilDone:NO];

	// wait for thread to finish
	[lock lockWhenCondition: T_FINISHED];
	[lock unlockWithCondition: T_START];
}

@end
