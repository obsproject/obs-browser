#import <AppKit/AppKit.h>

void RemoveNSViewFromSuperview(void *view)
{
	if (!view)
		return;

	if (!*((bool *)view))
		return;

	// CefBrowserHostView is a subclass of NSView
	// and we just need the superclass here
	NSView *nsview = (NSView *)view;
	[nsview removeFromSuperview];
}
