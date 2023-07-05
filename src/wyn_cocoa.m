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

struct wyn_events_t
{
    char dummy;
};

static struct wyn_events_t g_events = {};

// -------------------------------------------------------------------------------------------------------------------------------- //

static bool wyn_init(struct wyn_events_t *events);
static void wyn_terminate(struct wyn_events_t *events);

static int wyn_run_native(struct wyn_events_t *events);

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
            wyn_on_start(&g_events);
            code = wyn_run_native(&g_events);
            wyn_on_stop(&g_events);
        }
        wyn_terminate(&g_events);

        return code;
    }
}

extern void wyn_quit(struct wyn_events_t* events [[maybe_unused]], int code [[maybe_unused]])
{
    [NSApp stop:nil];
}

// ================================================================================================================================ //
// Private Definitions
// -------------------------------------------------------------------------------------------------------------------------------- //

static bool wyn_init(struct wyn_events_t* events [[maybe_unused]])
{
    *events = (struct wyn_events_t){};
    return true;
}

static void wyn_terminate(struct wyn_events_t* events [[maybe_unused]])
{
}

// -------------------------------------------------------------------------------------------------------------------------------- //

static int wyn_run_native(struct wyn_events_t* events [[maybe_unused]])
{
    [NSApplication sharedApplication];
    [NSApp run];
    return 0;
}

// -------------------------------------------------------------------------------------------------------------------------------- //

extern wyn_window_t wyn_open_window(struct wyn_events_t* events [[maybe_unused]])
{
    const NSRect rect = { .origin = { .x = 0.0, .y = 0.0 }, .size = { .width = 640.0, .height = 480.0 }};
    const NSWindowStyleMask style = NSWindowStyleMaskClosable | NSWindowStyleMaskTitled | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable;
    NSWindow* const nsWnd = [[NSWindow alloc] initWithContentRect:rect styleMask:style backing:NSBackingStoreBuffered defer:FALSE];
    return (wyn_window_t)(nsWnd);
}

extern void wyn_close_window(struct wyn_events_t* events [[maybe_unused]], wyn_window_t window [[maybe_unused]])
{
    NSWindow* const nsWnd = (NSWindow*)(window);

    [nsWnd close];
}

extern void wyn_show_window(struct wyn_events_t* events [[maybe_unused]], wyn_window_t window [[maybe_unused]])
{
    NSWindow* const nsWnd = (NSWindow*)(window);

    [nsWnd makeKeyAndOrderFront:nil];
}

extern void wyn_hide_window(struct wyn_events_t* events [[maybe_unused]], wyn_window_t window [[maybe_unused]])
{
    NSWindow* const nsWnd = (NSWindow*)(window);

    [nsWnd orderOut:nil];
}

// ================================================================================================================================ //
