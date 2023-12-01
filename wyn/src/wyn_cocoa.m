/**
 * @file wyn_cocoa.m
 * @brief Implementation of Wyn for the Cocoa backend.
 */

#include "wyn.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>

#import <Cocoa/Cocoa.h>

// ================================================================================================================================
//  Private Macros
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see BSD:
 * - https://www.manpagez.com/man/3/abort/
 */
#define WYN_ASSERT(expr) if (expr) {} else abort()

/**
 * @see C:
 * - https://en.cppreference.com/w/c/io/fprintf
 */
#define WYN_LOG(...) (void)fprintf(stderr, __VA_ARGS__)

// ================================================================================================================================
//  Private Declarations
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief Delegate Class for dispatching to callbacks.
 *
 * @see Obj-C:
 * - https://developer.apple.com/documentation/objectivec/nsobject?language=objc
 * @see AppKit:
 * - https://developer.apple.com/documentation/appkit/nsapplicationdelegate?language=objc
 * - https://developer.apple.com/documentation/appkit/nswindowdelegate?language=objc
 * - https://developer.apple.com/documentation/appkit/nsresponder?language=objc
 * - https://developer.apple.com/documentation/appkit/nsview?language=objc
 */
@interface wyn_delegate_t : NSView <NSApplicationDelegate, NSWindowDelegate>
@end

/**
 * @brief Internal structure for holding Wyn state.
 */
struct wyn_state_t 
{
    void* userdata; ///< The pointer provided by the user when the Event Loop was started.
    _Atomic(bool) quitting; ///< Flag to indicate the Event Loop is quitting.

    wyn_delegate_t* delegate; ///< Instance of the Delegate Class.
};

/**
 * @brief Static instance of all Wyn state.
 */
static struct wyn_state_t wyn_state;

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief Initializes all Wyn state.
 * @param[in] userdata [nullable] The pointer provided by the user when the Event Loop was started.
 * @return `true` if successful, `false` if there were errors.
 */
static bool wyn_reinit(void* userdata);

/**
 * @brief Cleans up all Wyn state.
 */
static void wyn_deinit(void);

/**
 * @brief Closes all remaining open Windows.
 * @param[in] arg [unused]
 */
static void wyn_close_callback(void* arg);

/**
 * @brief Stops the running NSApplication.
 * @param[in] arg [unused]
 */
static void wyn_stop_callback(void* arg);

/**
 * @brief Runs the platform-native Event Loop.
 */
static void wyn_run_native(void);

// ================================================================================================================================
//  Private Definitions
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Obj-C:
 * - https://developer.apple.com/documentation/objectivec/nsobject/1571948-new?language=objc
 * - https://developer.apple.com/documentation/objectivec/1418956-nsobject/1571951-autorelease?language=objc
 * @see AppKit:
 * - https://developer.apple.com/documentation/appkit/nsapplication/1428360-sharedapplication?language=objc
 * - https://developer.apple.com/documentation/appkit/nsapplication/1428621-setactivationpolicy?language=objc
 * - https://developer.apple.com/documentation/appkit/nsapplication/1428705-delegate?language=objc
 */
static bool wyn_reinit(void* userdata)
{
    wyn_state = (struct wyn_state_t){
        .userdata = userdata,
        .quitting = false,
        .delegate = NULL,
    };
    
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    wyn_state.delegate = [[wyn_delegate_t new] autorelease];
    if (wyn_state.delegate == NULL) return false;
    [NSApp setDelegate:wyn_state.delegate];

    return true;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see AppKit:
 * - https://developer.apple.com/documentation/appkit/nsapplication/1428631-run?language=objc
 * - https://developer.apple.com/documentation/appkit/nsapplication/1428705-delegate?language=objc
 * @see GCD:
 * - https://developer.apple.com/documentation/dispatch/1452921-dispatch_get_main_queue
 * - https://developer.apple.com/documentation/dispatch/1453057-dispatch_async
 */
static void wyn_deinit(void)
{
    const dispatch_queue_main_t main_queue = dispatch_get_main_queue();
    dispatch_async_f(main_queue, nil, wyn_close_callback);
    dispatch_async_f(main_queue, nil, wyn_stop_callback);
    [NSApp run];

    [NSApp setDelegate:nil];
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see AppKit:
 * - https://developer.apple.com/documentation/appkit/nswindow/1419662-close?language=objc
 * - https://developer.apple.com/documentation/appkit/nsapplication/1644472-enumeratewindowswithoptions?language=objc
 */
static void wyn_close_callback(void* arg [[maybe_unused]])
{
    [NSApp
        enumerateWindowsWithOptions:NSWindowListOrderedFrontToBack
        usingBlock: ^void(NSWindow* _Nonnull window, BOOL* _Nonnull stop [[maybe_unused]])
        {
            [window close];
        }
    ];
}

/**
 * @see AppKit:
 * - https://developer.apple.com/documentation/appkit/nsapplication/1428473-stop?language=objc
 * - https://developer.apple.com/documentation/appkit/nsevent/1533746-stopperiodicevents?language=objc
 * - https://developer.apple.com/documentation/appkit/nsevent/1526044-startperiodiceventsafterdelay?language=objc
 */
static void wyn_stop_callback(void* arg [[maybe_unused]])
{
    [NSApp stop:nil];
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see AppKit:
 * - https://developer.apple.com/documentation/appkit/nsapplication/1428631-run?language=objc
 */
static void wyn_run_native(void)
{
    [NSApp run];
    wyn_quit();
}

// --------------------------------------------------------------------------------------------------------------------------------

@implementation wyn_delegate_t

/**
 * @see AppKit:
 * - https://developer.apple.com/documentation/appkit/nsapplicationdelegate/1428642-applicationshouldterminate?language=objc
 */
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender
{
    wyn_quit();
    return NSTerminateCancel;
}

/**
 * @see AppKit:
 * - https://developer.apple.com/documentation/appkit/nswindowdelegate/1419380-windowshouldclose?language=objc
 */
- (BOOL)windowShouldClose:(NSWindow*)sender
{
    wyn_on_window_close(wyn_state.userdata, (wyn_window_t)sender);
    return FALSE;
}

/**
 * @see AppKit:
 * - https://developer.apple.com/documentation/appkit/nswindowdelegate/1419258-windowdidexpose?language=objc
 */
- (void)windowDidExpose:(NSNotification*)notification
{
    NSWindow* const ns_window = [notification object];
    wyn_on_window_redraw(wyn_state.userdata, (wyn_window_t)ns_window);
}

/**
 * @see Foundation:
 * - https://developer.apple.com/documentation/foundation/nsnotification/1414469-object?language=objc
 * @see AppKit:
 * - https://developer.apple.com/documentation/appkit/nswindowdelegate/1419567-windowdidresize?language=objc
 */
- (void)windowDidResize:(NSNotification*)notification
{
    NSWindow* const ns_window = [notification object];
    wyn_window_t const window = (wyn_window_t)ns_window;

    const wyn_size_t size = wyn_window_size(window);
    wyn_on_window_resize(wyn_state.userdata, window, size.w, size.h);
}

// https://developer.apple.com/documentation/appkit/nsresponder/1525114-mousemoved?language=objc
- (void)mouseMoved:(NSEvent*)event
{
    NSWindow* const ns_window = [event window];
    NSPoint const ns_point = [NSEvent mouseLocation];
    NSRect const frame_rect = [ns_window frame];
    NSRect const content_rect  = [ns_window contentRectForFrameRect:frame_rect];
    if ([wyn_state.delegate mouse:ns_point inRect:content_rect])
    {
        NSPoint const sc_point = [ns_window convertPointFromScreen:ns_point];
        NSPoint const px_point = [ns_window convertPointToBacking:sc_point];
        wyn_on_cursor(wyn_state.userdata, (wyn_window_t)ns_window, (wyn_coord_t)px_point.x, (wyn_coord_t)px_point.y);
    }
}

// https://developer.apple.com/documentation/appkit/nsresponder/1527420-mousedragged?language=objc
- (void)mouseDragged:(NSEvent*)event
{
    [self mouseMoved:event];
}

// https://developer.apple.com/documentation/appkit/nsresponder/1529135-rightmousedragged?language=objc
- (void)rightMouseDragged:(NSEvent*)event
{
    [self mouseMoved:event];
}

// https://developer.apple.com/documentation/appkit/nsresponder/1529804-othermousedragged?language=objc
- (void)otherMouseDragged:(NSEvent*)event
{
    [self mouseMoved:event];
}

// https://developer.apple.com/documentation/appkit/nsresponder/1529306-mouseentered?language=objc
- (void)mouseEntered:(NSEvent*)event
{
    // WYN_LOG("[WYN] mouseEntered\n");
}

// https://developer.apple.com/documentation/appkit/nsresponder/1527561-mouseexited?language=objc
- (void)mouseExited:(NSEvent*)event
{
    // WYN_LOG("[WYN] mouseExited\n");
}

// https://developer.apple.com/documentation/appkit/nsresponder/1534192-scrollwheel?language=objc
- (void)scrollWheel:(NSEvent*)event
{
    NSWindow* const ns_window = [event window];
    CGFloat const dx = [event scrollingDeltaX];
    CGFloat const dy = [event scrollingDeltaY];
    wyn_on_scroll(wyn_state.userdata, (wyn_window_t)ns_window, (double)dx, (double)dy);
}

// https://developer.apple.com/documentation/appkit/nsresponder/1524634-mousedown?language=objc
- (void)mouseDown:(NSEvent*)event
{
    NSWindow* const ns_window = [event window];
    NSInteger const ns_button = [event buttonNumber];
    wyn_on_mouse(wyn_state.userdata, (wyn_window_t)ns_window, (wyn_button_t)ns_button, true);
}

// https://developer.apple.com/documentation/appkit/nsresponder/1524727-rightmousedown?language=objc
- (void)rightMouseDown:(NSEvent*)event
{
    [self mouseDown:event];
}

// https://developer.apple.com/documentation/appkit/nsresponder/1525719-othermousedown?language=objc
- (void)otherMouseDown:(NSEvent*)event
{
    [self mouseDown:event];
}

// https://developer.apple.com/documentation/appkit/nsresponder/1535349-mouseup?language=objc
- (void)mouseUp:(NSEvent*)event
{
    NSWindow* const ns_window = [event window];
    NSInteger const ns_button = [event buttonNumber];
    wyn_on_mouse(wyn_state.userdata, (wyn_window_t)ns_window, (wyn_button_t)ns_button, false);
}

// https://developer.apple.com/documentation/appkit/nsresponder/1526309-rightmouseup?language=objc
- (void)rightMouseUp:(NSEvent*)event
{
    [self mouseUp:event];
}

// https://developer.apple.com/documentation/appkit/nsresponder/1531343-othermouseup?language=objc
- (void)otherMouseUp:(NSEvent*)event
{
    [self mouseUp:event];
}

// https://developer.apple.com/documentation/appkit/nsresponder/1525805-keydown?language=objc
- (void)keyDown:(NSEvent*)event
{
    NSWindow* const ns_window = [event window];

    unsigned short keycode = [event keyCode];
    wyn_on_keyboard(wyn_state.userdata, (wyn_window_t)ns_window, (wyn_keycode_t)keycode, true);

    NSString* const ns_string = [event characters];
    const char* const text = [ns_string UTF8String];
    wyn_on_text(wyn_state.userdata, (wyn_window_t)ns_window, (const wyn_utf8_t*)text);
}

// https://developer.apple.com/documentation/appkit/nsresponder/1527436-keyup?language=objc
- (void)keyUp:(NSEvent *)event
{
    NSWindow* const ns_window = [event window];

    unsigned short keycode = [event keyCode];
    wyn_on_keyboard(wyn_state.userdata, (wyn_window_t)ns_window, (wyn_keycode_t)keycode, false);
}

@end

// ================================================================================================================================
//  Public Definitions
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Cocoa:
 * - https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/MemoryMgmt/Articles/mmAutoreleasePools.html
 */
extern void wyn_run(void* const userdata)
{
    @autoreleasepool
    {
        if (wyn_reinit(userdata))
        {
            wyn_on_start(userdata);
            wyn_run_native();
            wyn_on_stop(userdata);
        }
        wyn_deinit();
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see C:
 * - https://en.cppreference.com/w/c/atomic/atomic_store
 * @see AppKit:
 * - https://developer.apple.com/documentation/appkit/nsapplication/1428759-running?language=objc
 * - https://developer.apple.com/documentation/appkit/nsapplication/1428473-stop?language=objc
 * - https://developer.apple.com/documentation/appkit/nsevent/1533746-stopperiodicevents?language=objc
 * - https://developer.apple.com/documentation/appkit/nsevent/1526044-startperiodiceventsafterdelay?language=objc
 */
extern void wyn_quit(void)
{
    const bool was_quitting = atomic_exchange_explicit(&wyn_state.quitting, true, memory_order_relaxed);

    if (!was_quitting && [NSApp isRunning])
    {
        [NSApp stop:nil];
        [NSEvent stopPeriodicEvents];
        [NSEvent startPeriodicEventsAfterDelay:0.0 withPeriod:0.1];
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see C:
 * - https://en.cppreference.com/w/c/atomic/atomic_load
 */
extern bool wyn_quitting(void)
{
    return atomic_load_explicit(&wyn_state.quitting, memory_order_relaxed);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Foundation:
 * - https://developer.apple.com/documentation/foundation/nsthread/1412704-ismainthread?language=objc
 */
extern bool wyn_is_this_thread(void)
{
    return [NSThread isMainThread];
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see GCD:
 * - https://developer.apple.com/documentation/dispatch/1452921-dispatch_get_main_queue
 * - https://developer.apple.com/documentation/dispatch/1453057-dispatch_async
 */
extern void wyn_signal(void)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        wyn_on_signal(wyn_state.userdata);
    });
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Obj-C:
 * - https://developer.apple.com/documentation/objectivec/nsobject/1571958-alloc/
 * @see AppKit:
 * - https://developer.apple.com/documentation/appkit/nswindow/1419477-initwithcontentrect?language=objc
 * - https://developer.apple.com/documentation/appkit/nswindow/1419060-delegate?language=objc
 * - https://developer.apple.com/documentation/appkit/nswindow/1419160-contentview?language=objc
 * - https://developer.apple.com/documentation/appkit/nswindow/1419340-acceptsmousemovedevents?language=objc
 * - https://developer.apple.com/documentation/appkit/nswindow/1419479-initialfirstresponder?language=objc
 * - https://developer.apple.com/documentation/appkit/nswindow/1419366-makefirstresponder?language=objc
 * - https://developer.apple.com/documentation/appkit/nswindow/1419090-center?language=objc
 */
extern wyn_window_t wyn_window_open(void)
{
    const NSRect rect = { .origin = { .x = 0.0, .y = 0.0 }, .size = { .width = 640.0, .height = 480.0 } };
    const NSWindowStyleMask style = NSWindowStyleMaskClosable | NSWindowStyleMaskTitled | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable;
    NSWindow* const ns_window = [[NSWindow alloc] initWithContentRect:rect styleMask:style backing:NSBackingStoreBuffered defer:FALSE];
    if (ns_window)
    {
        [ns_window setDelegate:wyn_state.delegate];
        [ns_window setContentView:wyn_state.delegate];
        [ns_window setAcceptsMouseMovedEvents:YES];
        [ns_window setInitialFirstResponder:wyn_state.delegate];
        const BOOL res = [ns_window makeFirstResponder:wyn_state.delegate];
        WYN_ASSERT(res == YES);

        [ns_window center];
    }
    return (wyn_window_t)ns_window;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see AppKit:
 * - https://developer.apple.com/documentation/appkit/nswindow/1419662-close?language=objc
 */
extern void wyn_window_close(wyn_window_t const window)
{
    NSWindow* const ns_window = (NSWindow*)window;
    [ns_window close];
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see AppKit:
 * - https://developer.apple.com/documentation/appkit/nswindow/1419208-makekeyandorderfront?language=objc
 */
extern void wyn_window_show(wyn_window_t const window)
{
    NSWindow* const ns_window = (NSWindow*)window;
    [ns_window makeKeyAndOrderFront:nil];
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see AppKit:
 * - https://developer.apple.com/documentation/appkit/nswindow/1419660-orderout?language=objc
 */
extern void wyn_window_hide(wyn_window_t const window)
{
    NSWindow* const ns_window = (NSWindow*)window;
    [ns_window orderOut:nil];
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see AppKit:
 * - https://developer.apple.com/documentation/appkit/nswindow/1419459-backingscalefactor?language=objc
 */
extern double wyn_window_scale(wyn_window_t const window)
{
    NSWindow* const ns_window = (NSWindow*)window;
    const CGFloat scale = [ns_window backingScaleFactor];
    return (double)scale;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see AppKit:
 * - https://developer.apple.com/documentation/appkit/nswindow/1419697-frame?language=objc
 * - https://developer.apple.com/documentation/appkit/nswindow/1419108-contentrectforframerect?language=objc
 * - https://developer.apple.com/documentation/appkit/nswindow/1419260-convertrecttobacking?language=objc
 */
extern wyn_size_t wyn_window_size(wyn_window_t const window)
{
    NSWindow* const ns_window = (NSWindow*)window;
    const NSRect frame = [ns_window frame];
    const NSRect content = [ns_window contentRectForFrameRect:frame];
    const NSRect backing = [ns_window convertRectToBacking:content];
    return (wyn_size_t){ .w = (wyn_coord_t)(backing.size.width), .h = (wyn_coord_t)(backing.size.height) };
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see AppKit:
 * - https://developer.apple.com/documentation/appkit/nswindow/1419273-convertrectfrombacking?language=objc
 * - https://developer.apple.com/documentation/appkit/nswindow/1419100-setcontentsize?language=objc
 */
extern void wyn_window_resize(wyn_window_t const window, wyn_size_t const size)
{
    NSWindow* const ns_window = (NSWindow*)window;
    const NSRect backing = { .size = { .width = (CGFloat)(size.w), .height = (CGFloat)(size.h) } };
    const NSRect content = [ns_window convertRectFromBacking:backing];
    [ns_window setContentSize:content.size];
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Foundation:
 * - https://developer.apple.com/documentation/foundation/nsstring/1412128-initwithutf8string?language=objc
 * - https://developer.apple.com/documentation/foundation/nsstring/1497312-string?language=objc
 * @see AppKit:
 * - https://developer.apple.com/documentation/appkit/nswindow/1419404-title?language=objc
 */
extern void wyn_window_retitle(wyn_window_t const window, const wyn_utf8_t* const title)
{
    NSWindow* const ns_window = (NSWindow*)window;
    NSString* const ns_string = (title ? [NSString stringWithUTF8String:(const char*)title] : [NSString string]);
    WYN_ASSERT(ns_string != nil);

    [ns_window setTitle:ns_string];
}

// ================================================================================================================================

/**
 * @see AppKit:
 * - https://developer.apple.com/documentation/appkit/nswindow/1419160-contentview?language=objc
 */
extern void* wyn_native_context(wyn_window_t const window)
{
    NSWindow* const ns_window = (NSWindow*)window;
    return [ns_window contentView];
}

// ================================================================================================================================
