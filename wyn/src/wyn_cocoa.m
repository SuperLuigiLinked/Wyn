/**
 * @file wyn_cocoa.m
 * @brief Implementation of Wyn for the Cocoa backend.
 */

#include "wyn.h"

#include <stdio.h>
#include <stdlib.h>

#import <Cocoa/Cocoa.h>

// ================================================================================================================================
//  Macros
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://www.manpagez.com/man/3/abort/
 */
#define WYN_ASSERT(expr) if (expr) {} else abort()

// ================================================================================================================================
//  Declarations
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief Delegate Class for dispatching to callbacks.
 *
 * @see https://developer.apple.com/documentation/objectivec/nsobject?language=objc
 * @see https://developer.apple.com/documentation/appkit/nsapplicationdelegate?language=objc
 * @see https://developer.apple.com/documentation/appkit/nswindowdelegate?language=objc
 */
@interface wyn_delegate_t : NSObject <NSApplicationDelegate, NSWindowDelegate>
@end

/**
 * @brief Internal structure for holding Wyn state.
 */
struct wyn_state_t 
{
    void* userdata;             ///< The pointer provided by the user when the Event Loop was started.
    wyn_delegate_t* delegate;   ///< Instance of the Delegate Class.
    bool clearing_events;       ///< Flag indicating that events are being cleared.
};

/**
 * @brief Static instance of all Wyn state.
 */
static struct wyn_state_t wyn_state = {};

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief Initializes all Wyn state.
 */
static bool wyn_init(void* userdata);

/**
 * @brief Cleans up all Wyn state.
 */
static void wyn_terminate(void);

static void wyn_async_close(void* arg);
static void wyn_async_stop(void* arg);

/**
 * @brief Runs the platform-native Event Loop.
 */
static void wyn_run_native(void);

// ================================================================================================================================
//  Public Definitions
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/MemoryMgmt/Articles/mmAutoreleasePools.html
 */
extern void wyn_run(void* userdata)
{
    @autoreleasepool
    {
        if (wyn_init(userdata))
        {
            // wyn_on_start(userdata); // Will be called by NSApplicationDelegate.
            wyn_run_native();
            wyn_on_stop(userdata);
        }
        wyn_terminate();
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://developer.apple.com/documentation/appkit/nsapplication/1428473-stop?language=objc
 */
extern void wyn_quit(void)
{
    [NSApp stop:nil];
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://developer.apple.com/documentation/dispatch/1452921-dispatch_get_main_queue
 * @see https://developer.apple.com/documentation/dispatch/3191902-dispatch_async_and_wait_f
 */
extern void wyn_execute(void (*func)(void *), void *arg)
{
    const dispatch_queue_main_t queue = dispatch_get_main_queue();
    dispatch_async_and_wait_f(queue, arg, func);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://developer.apple.com/documentation/dispatch/1452921-dispatch_get_main_queue
 * @see https://developer.apple.com/documentation/dispatch/1452834-dispatch_async_f
 */
extern void wyn_execute_async(void (*func)(void *), void *arg)
{
    const dispatch_queue_main_t queue = dispatch_get_main_queue();
    dispatch_async_f(queue, arg, func);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://developer.apple.com/documentation/objectivec/nsobject/1571958-alloc/
 * @see https://developer.apple.com/documentation/appkit/nswindow/1419477-initwithcontentrect?language=objc
 * @see https://developer.apple.com/documentation/appkit/nswindow/1419060-delegate?language=objc
 * @see https://developer.apple.com/documentation/appkit/nswindow/1419090-center?language=objc
 */
extern wyn_window_t wyn_open_window(void)
{
    const NSRect rect = { .origin = { .x = 0.0, .y = 0.0 }, .size = { .width = 640.0, .height = 480.0 } };
    const NSWindowStyleMask style = NSWindowStyleMaskClosable | NSWindowStyleMaskTitled | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable;
    NSWindow* const nsWnd = [[NSWindow alloc] initWithContentRect:rect styleMask:style backing:NSBackingStoreBuffered defer:FALSE];
    if (nsWnd)
    {
        [nsWnd setDelegate:wyn_state.delegate];
        [nsWnd center];
    }
    return (wyn_window_t)nsWnd;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://developer.apple.com/documentation/appkit/nswindow/1419288-performclose?language=objc
 */
extern void wyn_close_window(wyn_window_t window)
{
    NSWindow* const nsWnd = (NSWindow*)window;
    [nsWnd performClose:nil];
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://developer.apple.com/documentation/appkit/nswindow/1419208-makekeyandorderfront?language=objc
 */
extern void wyn_show_window(wyn_window_t window)
{
    NSWindow* const nsWnd = (NSWindow*)window;
    [nsWnd makeKeyAndOrderFront:nil];
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://developer.apple.com/documentation/appkit/nswindow/1419660-orderout?language=objc
 */
extern void wyn_hide_window(wyn_window_t window)
{
    NSWindow* const nsWnd = (NSWindow*)window;
    [nsWnd orderOut:nil];
}

// ================================================================================================================================
//  Private Definitions
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://developer.apple.com/documentation/objectivec/nsobject/1571948-new?language=objc
 * @see https://developer.apple.com/documentation/objectivec/1418956-nsobject/1571951-autorelease?language=objc
 * @see https://developer.apple.com/documentation/appkit/nsapplication/1428360-sharedapplication?language=objc
 * @see https://developer.apple.com/documentation/appkit/nsapplication/1428705-delegate?language=objc
 */
static bool wyn_init(void* userdata)
{
    wyn_state = (struct wyn_state_t){
        .userdata = userdata,
        .delegate = NULL,
        .clearing_events = false,
    };
    
    [NSApplication sharedApplication];

    wyn_state.delegate = [[wyn_delegate_t new] autorelease];
    if (wyn_state.delegate == NULL) return false;
    [NSApp setDelegate:wyn_state.delegate];

    return true;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://developer.apple.com/documentation/appkit/nsapplication/1428631-run?language=objc
 */
static void wyn_terminate(void)
{
    wyn_state.clearing_events = true;

    wyn_execute_async(wyn_async_close, nil);
    wyn_execute_async(wyn_async_stop, nil);
    [NSApp run];
    
    wyn_state.clearing_events = false;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://developer.apple.com/documentation/appkit/nswindow/1419662-close?language=objc
 * @see https://developer.apple.com/documentation/appkit/nsapplication/1644472-enumeratewindowswithoptions?language=objc
 */
static void wyn_async_close(void* arg [[maybe_unused]])
{
    void(^close_block)(NSWindow* _Nonnull, BOOL* _Nonnull) = ^void(NSWindow* _Nonnull window, BOOL* _Nonnull stop [[maybe_unused]])
    {
        [window close];
    };
    [NSApp enumerateWindowsWithOptions:NSWindowListOrderedFrontToBack usingBlock:close_block];
}

/**
 * @see https://developer.apple.com/documentation/appkit/nsapplication/1428473-stop?language=objc
 */
static void wyn_async_stop(void* arg [[maybe_unused]])
{
    [NSApp stop:nil];
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://developer.apple.com/documentation/appkit/nsapplication/1428631-run?language=objc
 */
static void wyn_run_native(void)
{
    [NSApp run];
}

// --------------------------------------------------------------------------------------------------------------------------------

@implementation wyn_delegate_t

/**
 * @see https://developer.apple.com/documentation/appkit/nsapplicationdelegate/1428385-applicationdidfinishlaunching?language=objc
 */
- (void)applicationDidFinishLaunching:(NSNotification*)notification
{
    if (!wyn_state.clearing_events)
    {
        wyn_on_start(wyn_state.userdata);
    }
}

/**
 * @see https://developer.apple.com/documentation/appkit/nsapplicationdelegate/1428642-applicationshouldterminate?language=objc
 */
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender
{
    if (!wyn_state.clearing_events)
    {
        wyn_quit();
    }
    return NSTerminateCancel;
}

/**
 * @see https://developer.apple.com/documentation/appkit/nswindowdelegate/1419380-windowshouldclose?language=objc
 */
- (BOOL)windowShouldClose:(NSWindow*)sender
{
    const wyn_window_t window = (wyn_window_t)sender;

    wyn_on_window_close(wyn_state.userdata, window);
    return TRUE;
}

@end

// ================================================================================================================================
