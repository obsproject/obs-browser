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

class CEFisolationService;

@interface ServiceConnectionDelegate : NSObject
{
	id<CEFIsolationService> _cefIsolationService;
	NSString *uniqueClientName;
	NSConditionLock *lock;
	BOOL shutdown;
	NSThread *connectionThread;
}

@property (readonly) id<CEFIsolationService> cefIsolationService;

- (ServiceConnectionDelegate *)initWithUniqueName: (NSString *)name;
- (void)createConnectionThread;
- (void)shutdown;

@end
