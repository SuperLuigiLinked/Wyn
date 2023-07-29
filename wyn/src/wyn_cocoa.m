/*
    Wyn - Cocoa.m
*/

#include "wyn.h"

#include <stdio.h>
#include <stdlib.h>

#import <Cocoa/Cocoa.h>

// ================================================================================================================================ //
// Macros
// -------------------------------------------------------------------------------------------------------------------------------- //

// #ifdef NDEBUG
// #   define WYN_ASSERT(expr) if (expr) {} else abort()
// #else
// #   define WYN_ASSERT(expr) if (expr) {} else abort()
// #endif

// ================================================================================================================================ //
// Declarations
// -------------------------------------------------------------------------------------------------------------------------------- //

@interface wyn_delegate_t : NSObject <NSApplicationDelegate, NSWindowDelegate>
@end

struct wyn_events_t 
{
    void* userdata;

    wyn_delegate_t* delegate;

    int exit_code;
};

static struct wyn_events_t g_events = {};

// -------------------------------------------------------------------------------------------------------------------------------- //

static bool wyn_init(void* userdata);
static void wyn_terminate(void);

static int wyn_run_native(void);

// ================================================================================================================================ //
// Public Definitions
// -------------------------------------------------------------------------------------------------------------------------------- //

extern int wyn_run(void* userdata)
{
    @autoreleasepool
    {
        int code = EXIT_FAILURE;

        if (wyn_init(userdata))
        {
            // wyn_on_start(userdata); // Will be called by NSApplicationDelegate.
            code = wyn_run_native();
            wyn_on_stop(userdata);
        }
        wyn_terminate();

        return code;
    }
}

extern void wyn_quit(int code)
{
    [NSApp stop:nil];
    [NSEvent startPeriodicEventsAfterDelay: 0 withPeriod: 0.1];

    g_events.exit_code = code;
}

// ================================================================================================================================ //
// Private Definitions
// -------------------------------------------------------------------------------------------------------------------------------- //

static bool wyn_init(void* userdata)
{
    g_events = (struct wyn_events_t){
        .userdata = userdata,
        .delegate = NULL,
        .exit_code = 0,
    };
    
    [NSApplication sharedApplication];

    g_events.delegate = [[wyn_delegate_t new] autorelease];
    if (g_events.delegate == NULL) return false;

    [NSApp setDelegate:g_events.delegate];
    [NSApp activateIgnoringOtherApps:TRUE];

    return true;
}

static void wyn_terminate(void)
{
    void(^close_block)(NSWindow* _Nonnull, BOOL* _Nonnull) = ^void(NSWindow* _Nonnull window, BOOL* _Nonnull stop [[maybe_unused]])
    {
        [window close];
    };
    
    [NSApp enumerateWindowsWithOptions:NSWindowListOrderedFrontToBack usingBlock:close_block];
}

// -------------------------------------------------------------------------------------------------------------------------------- //

static int wyn_run_native(void)
{
    [NSApp run];
    return g_events.exit_code;
}

@implementation wyn_delegate_t
- (void)applicationDidFinishLaunching:(NSNotification*)notification
{
    wyn_on_start(g_events.userdata);
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender
{
    wyn_quit(0);
    return NSTerminateCancel;
}

- (BOOL)windowShouldClose:(NSWindow*)sender
{
    const wyn_window_t window = (wyn_window_t)sender;

    wyn_on_window_close(g_events.userdata, window);
    return TRUE;
}

- (void)windowWillClose:(NSNotification*)notification
{
    fputs("[WND WILL CLOSE]\n", stderr);
}
@end

// -------------------------------------------------------------------------------------------------------------------------------- //

extern wyn_window_t wyn_open_window(void)
{
    const NSRect rect = { .origin = { .x = 0.0, .y = 0.0 }, .size = { .width = 640.0, .height = 480.0 } };
    const NSWindowStyleMask style = NSWindowStyleMaskClosable | NSWindowStyleMaskTitled | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable;
    NSWindow* const nsWnd = [[NSWindow alloc] initWithContentRect:rect styleMask:style backing:NSBackingStoreBuffered defer:FALSE];
    if (nsWnd)
    {
        [nsWnd setDelegate:g_events.delegate];
        // [nsWnd center];
    }
    return (wyn_window_t)(nsWnd);
}

extern void wyn_close_window(wyn_window_t window)
{
    NSWindow* const nsWnd = (NSWindow*)window;

    //[nsWnd close];
    [nsWnd performClose:nil];
}

extern void wyn_show_window(wyn_window_t window)
{
    NSWindow* const nsWnd = (NSWindow*)window;

    [nsWnd makeKeyAndOrderFront:nil];
}

extern void wyn_hide_window(wyn_window_t window)
{
    NSWindow* const nsWnd = (NSWindow*)window;

    [nsWnd orderOut:nil];
}

// ================================================================================================================================ //
