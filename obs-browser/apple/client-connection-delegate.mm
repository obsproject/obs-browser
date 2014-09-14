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
#import "cef-isolation-service.h"
#import "cef-isolation-service-manager.h"
#import "client-connection-delegate.h"

@implementation ClientConnectionDelegate
{
	NSConditionLock *lock;
	BOOL shutdown;
	NSThread *connectionThread;
}


#define T_START 0
#define T_FINISHED 1


- (ClientConnectionDelegate *)initWithManager:
(CEFIsolationServiceManager *)manager
{
	self = [super init];
	if (self) {
		lock = [[[NSConditionLock alloc] initWithCondition: T_START]
			autorelease];
		_manager = manager;
	}
	return self;
}

- (void)createConnectionThread
{
	[[NSThread currentThread] setName: @"CEF Isolation IO Thread"];

	@autoreleasepool {

		CEFLogDebug(@"Creating connection");

		[lock lockWhenCondition: T_START];

		NSRunLoop *threadRunLoop = [NSRunLoop currentRunLoop];
		connectionThread = [NSThread currentThread];

		NSConnection *connection =
		[[[NSConnection alloc] init] autorelease];

		[connection registerName: _manager->GetUniqueClientName()];
		[connection setRootObject: _manager->GetCefIsolationService()];
		[connection addRunLoop: threadRunLoop];
		BOOL threadAlive = YES;
		while (threadAlive) {
			@autoreleasepool {
				[threadRunLoop runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantFuture]];
				threadAlive = !shutdown;
			}
		}

		// invalidate connection and proxy objects
		[connection invalidate];
	}

	connectionThread = nil;

	[lock unlockWithCondition: T_FINISHED];
}

- (void)shutdownThread
{
	shutdown = YES;
}

- (void)shutdown
{
	[self performSelector:@selector(shutdownThread)
		     onThread:connectionThread withObject:nil waitUntilDone:NO];

	// wait for thread to finish
	[lock lockWhenCondition: T_FINISHED];
	[lock unlockWithCondition: T_START];
}

@end
