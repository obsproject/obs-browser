#import <Foundation/Foundation.h>

bool atLeast10_15(void)
{
	NSProcessInfo *processInfo = [[NSProcessInfo alloc] init];
	bool atLeast = [processInfo
		isOperatingSystemAtLeastVersion:(NSOperatingSystemVersion){
							10, 15, 0}];
	[processInfo release];

	return atLeast;
}