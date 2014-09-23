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
#include "browser-manager.hpp"

@implementation ClientConnectionDelegate
{
	NSConditionLock *lock;
	BOOL shutdown;
	BOOL newConnection;
	NSThread *connectionThread;
	NSTask *clientProcess;
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

- (void)connectionDidDie:(NSNotification *)aNotification
{
	(void)aNotification;

	if (!shutdown && !newConnection) {
		CEFLogError(@"Lost connection to isolated client process");
		[self restart];
	}
}

- (NSConnection *)createConnectionWithRunLoop:(NSRunLoop *)runLoop
{
	CEFLogDebug(@"Creating service %@", _manager->GetUniqueClientName());

	NSString *launchPath = [NSString stringWithUTF8String:
			BrowserManager::Instance()->GetModulePath()];

	launchPath = [launchPath stringByDeletingLastPathComponent];
	launchPath = [launchPath stringByAppendingPathComponent:
			@"/CEF.app/Contents/MacOS/CEF"];

	CEFLogDebug(@"Launching child process %@", launchPath);
	if (clientProcess != nil && clientProcess.isRunning) {
		CEFLogDebug(@"Existing process %d found; terminating",
				clientProcess.processIdentifier);
		[clientProcess terminate];
	}

	clientProcess = [[[NSTask alloc] init] autorelease];
	[clientProcess setArguments: @[ _manager->GetUniqueClientName() ]];
	[clientProcess setLaunchPath: launchPath];

	[clientProcess launch];
	
	NSConnection *connection = [[[NSConnection alloc] init] autorelease];

	[connection registerName: _manager->GetUniqueClientName()];
	[connection setRootObject: _manager->GetCefIsolationService()];
	[connection addRunLoop: runLoop];



	return connection;

}

- (void)createConnectionThread
{
	[[NSThread currentThread] setName: @"CEF Isolation IO Thread"];

	@autoreleasepool {

		CEFLogDebug(@"Creating connection");

		[lock lockWhenCondition: T_START];

		NSRunLoop *threadRunLoop = [NSRunLoop currentRunLoop];
		connectionThread = [NSThread currentThread];

		[[NSNotificationCenter defaultCenter] addObserver:self
				selector:@selector(connectionDidDie:)
				name:NSConnectionDidDieNotification
				object:nil];

		NSConnection *connection = nullptr;
		shutdown = NO;
		newConnection = YES;
		while (!shutdown) {
			if (newConnection) {
				if (connection != nil) {
					// it should already be invalidated
					connection = nil;
				}
				[_manager->GetCefIsolationService()
						invalidateClient: nil
						withException: nil];
				connection = [self createConnectionWithRunLoop:
						threadRunLoop];
				newConnection = NO;
			}
			@autoreleasepool {
				[threadRunLoop runMode:NSDefaultRunLoopMode
						beforeDate:
							[NSDate distantFuture]];
			}
		}

		[[NSNotificationCenter defaultCenter] removeObserver:self
				name:NSConnectionDidDieNotification
				object:nil];

		[connection invalidate];
		connection = nil;
		if (clientProcess != nullptr) {
			[clientProcess terminate];
			clientProcess = nil;
		}

	}

	connectionThread = nil;

	[lock unlockWithCondition: T_FINISHED];
}

- (void)restartConnection
{
	newConnection = YES;
}

- (void)shutdownThread
{
	shutdown = YES;
}

- (void)restart
{
	[self performSelector:@selector(restartConnection)
			onThread:connectionThread withObject:nil
			waitUntilDone:NO];
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
