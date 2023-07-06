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
    wyn_delegate_t* delegate;

    int exit_code;
};

static struct wyn_events_t g_events = {};

// -------------------------------------------------------------------------------------------------------------------------------- //

static bool wyn_init(struct wyn_events_t* events);
static void wyn_terminate(struct wyn_events_t* events);

static int wyn_run_native(struct wyn_events_t* events);

// ================================================================================================================================ //
// Public Definitions
// -------------------------------------------------------------------------------------------------------------------------------- //

extern int wyn_run(void)
{
    @autoreleasepool
    {
        int code = EXIT_FAILURE;

        if (wyn_init(&g_events))
        {
            // wyn_on_start(&g_events); // Will be called by NSApplicationDelegate.
            code = wyn_run_native(&g_events);
            wyn_on_stop(&g_events);
        }
        wyn_terminate(&g_events);

        return code;
    }
}

extern void wyn_quit(struct wyn_events_t* events, int code)
{
    [NSApp stop:nil];
    [NSEvent startPeriodicEventsAfterDelay: 0 withPeriod: 0.1];

    events->exit_code = code;
}

// ================================================================================================================================ //
// Private Definitions
// -------------------------------------------------------------------------------------------------------------------------------- //

static bool wyn_init(struct wyn_events_t* events)
{
    *events = (struct wyn_events_t){};
    
    [NSApplication sharedApplication];

    events->delegate = [[wyn_delegate_t new] autorelease];
    if (events->delegate == 0) return false;

    [NSApp setDelegate:events->delegate];
    [NSApp activateIgnoringOtherApps:TRUE];

    return true;
}

static void wyn_terminate(struct wyn_events_t* events [[maybe_unused]])
{
    void(^close_block)(NSWindow* _Nonnull, BOOL* _Nonnull) = ^void(NSWindow* _Nonnull window, BOOL* _Nonnull stop [[maybe_unused]])
    {
        [window close];
    };
    
    [NSApp enumerateWindowsWithOptions:NSWindowListOrderedFrontToBack usingBlock:close_block];
}

// -------------------------------------------------------------------------------------------------------------------------------- //

static int wyn_run_native(struct wyn_events_t* events)
{
    [NSApp run];
    return events->exit_code;
}

@implementation wyn_delegate_t
- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    struct wyn_events_t* const events = &g_events;

    wyn_on_start(events);
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender
{
    struct wyn_events_t* const events = &g_events;
    
    wyn_quit(events, 0);
    return NSTerminateCancel;
}

- (BOOL)windowShouldClose:(NSWindow*)sender
{
    struct wyn_events_t* const events = &g_events;
    const wyn_window_t window = (wyn_window_t)(sender);

    wyn_on_window_close(events, window);
    return TRUE;
}

- (void)windowWillClose:(NSNotification*)notification
{
    fputs("[WND WILL CLOSE]\n", stderr);
}
@end

// -------------------------------------------------------------------------------------------------------------------------------- //

extern wyn_window_t wyn_open_window(struct wyn_events_t* events)
{
    const NSRect rect = { .origin = { .x = 0.0, .y = 0.0 }, .size = { .width = 640.0, .height = 480.0 } };
    const NSWindowStyleMask style = NSWindowStyleMaskClosable | NSWindowStyleMaskTitled | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable;
    NSWindow* const nsWnd = [[NSWindow alloc] initWithContentRect:rect styleMask:style backing:NSBackingStoreBuffered defer:FALSE];
    if (nsWnd)
    {
        [nsWnd setDelegate:events->delegate];
        [nsWnd center];
    }
    return (wyn_window_t)(nsWnd);
}

extern void wyn_close_window(struct wyn_events_t* events [[maybe_unused]], wyn_window_t window)
{
    NSWindow* const nsWnd = (NSWindow*)(window);

    //[nsWnd close];
    [nsWnd performClose:nil];
}

extern void wyn_show_window(struct wyn_events_t* events [[maybe_unused]], wyn_window_t window)
{
    NSWindow* const nsWnd = (NSWindow*)(window);

    [nsWnd makeKeyAndOrderFront:nil];
}

extern void wyn_hide_window(struct wyn_events_t* events [[maybe_unused]], wyn_window_t window)
{
    NSWindow* const nsWnd = (NSWindow*)(window);

    [nsWnd orderOut:nil];
}

// ================================================================================================================================ //
