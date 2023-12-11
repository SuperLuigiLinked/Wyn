/**
 * @file wyn_cocoa.m
 * @brief Implementation of Wyn for the Cocoa backend.
 */

#include "wyn.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>

// ================================================================================================================================
//  Private Macros
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see BSD:
 * - https://www.manpagez.com/man/3/abort/
 */
#define WYN_ASSERT(expr) if (expr) {} else abort()

#ifdef NDEBUG
    #define WYN_ASSUME(expr) ((void)0)
#else
    #define WYN_ASSUME(expr) WYN_ASSERT(expr)
#endif

/**
 * @see C:
 * - https://en.cppreference.com/w/c/io/fprintf
 */
#define WYN_LOG(...) (void)fprintf(stderr, __VA_ARGS__)

// --------------------------------------------------------------------------------------------------------------------------------

#define WYN_STYLE_BORDERED (NSWindowStyleMaskClosable | NSWindowStyleMaskTitled | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable)
#define WYN_STYLE_BORDERLESS (NSWindowStyleMaskBorderless)

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
struct wyn_cocoa_t 
{
    void* userdata; ///< The pointer provided by the user when the Event Loop was started.
    _Atomic(bool) quitting; ///< Flag to indicate the Event Loop is quitting.

    wyn_delegate_t* delegate; ///< Instance of the Delegate Class.
};

/**
 * @brief Static instance of all Wyn state.
 */
static struct wyn_cocoa_t wyn_cocoa;

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
    wyn_cocoa = (struct wyn_cocoa_t){
        .userdata = userdata,
        .quitting = false,
        .delegate = NULL,
    };
    
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    wyn_cocoa.delegate = [[wyn_delegate_t new] autorelease];
    if (wyn_cocoa.delegate == NULL) return false;
    [NSApp setDelegate:wyn_cocoa.delegate];

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
 * - https://developer.apple.com/documentation/appkit/nsapplicationdelegate/1428424-applicationdidchangescreenparame?language=objc
 */
- (void)applicationDidChangeScreenParameters:(NSNotification *)notification
{
    wyn_on_display_change(wyn_cocoa.userdata);
}

/**
 * @see AppKit:
 * - https://developer.apple.com/documentation/appkit/nswindowdelegate/1419380-windowshouldclose?language=objc
 */
- (BOOL)windowShouldClose:(NSWindow*)sender
{
    wyn_on_window_close(wyn_cocoa.userdata, (wyn_window_t)sender);
    return FALSE;
}

/**
 * @see AppKit:
 * - https://developer.apple.com/documentation/appkit/nswindowdelegate/1419258-windowdidexpose?language=objc
 */
- (void)windowDidExpose:(NSNotification*)notification
{
    NSWindow* const ns_window = [notification object];
    wyn_on_window_redraw(wyn_cocoa.userdata, (wyn_window_t)ns_window);
}
/**
 * @see Foundation:
 * - https://developer.apple.com/documentation/foundation/nsnotification/1414469-object?language=objc
 * @see AppKit:
 * - https://developer.apple.com/documentation/appkit/nswindowdelegate/1419737-windowdidbecomekey?language=objc
 */
- (void)windowDidBecomeKey:(NSNotification*)notification
{
    NSWindow* const ns_window = [notification object];
    wyn_on_window_focus(wyn_cocoa.userdata, (wyn_window_t)ns_window, true);
}

/**
 * @see Foundation:
 * - https://developer.apple.com/documentation/foundation/nsnotification/1414469-object?language=objc
 * @see AppKit:
 * - https://developer.apple.com/documentation/appkit/nswindowdelegate/1419711-windowdidresignkey?language=objc
 */
- (void)windowDidResignKey:(NSNotification*)notification
{
    NSWindow* const ns_window = [notification object];
    wyn_on_window_focus(wyn_cocoa.userdata, (wyn_window_t)ns_window, false);
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
    wyn_on_window_reposition(wyn_cocoa.userdata, window, wyn_window_position(window), wyn_window_scale(window));
}

/**
 * @see Foundation:
 * - https://developer.apple.com/documentation/foundation/nsnotification/1414469-object?language=objc
 * @see AppKit:
 * - https://developer.apple.com/documentation/appkit/nswindowdelegate/1419674-windowdidmove?language=objc
 */
- (void)windowDidMove:(NSNotification*)notification
{
    NSWindow* const ns_window = [notification object];
    wyn_window_t const window = (wyn_window_t)ns_window;
    wyn_on_window_reposition(wyn_cocoa.userdata, window, wyn_window_position(window), wyn_window_scale(window));
}

/**
 * @see Foundation:
 * - https://developer.apple.com/documentation/foundation/nsnotification/1414469-object?language=objc
 * @see AppKit:
 * - https://developer.apple.com/documentation/appkit/nswindowdelegate/1419517-windowdidchangebackingproperties?language=objc
 */
- (void)windowDidChangeBackingProperties:(NSNotification*)notification
{
    NSWindow* const ns_window = [notification object];
    wyn_window_t const window = (wyn_window_t)ns_window;
    wyn_on_window_reposition(wyn_cocoa.userdata, window, wyn_window_position(window), wyn_window_scale(window));
}

// https://developer.apple.com/documentation/appkit/nsresponder/1525114-mousemoved?language=objc
- (void)mouseMoved:(NSEvent*)event
{
    NSWindow* const ns_window = [event window];
    NSPoint const local_point = [event locationInWindow];
    wyn_on_cursor(wyn_cocoa.userdata, (wyn_window_t)ns_window, (wyn_coord_t)local_point.x, (wyn_coord_t)local_point.y);
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
    NSWindow* const ns_window = [event window];
    wyn_on_cursor_exit(wyn_cocoa.userdata, (wyn_window_t)ns_window);
}

// https://developer.apple.com/documentation/appkit/nsview/1483719-updatetrackingareas?language=objc
- (void)updateTrackingAreas
{
    NSArray<NSTrackingArea*>* const areas = [self trackingAreas];

    NSTrackingArea* track = areas.firstObject;
    if (track)
        [self removeTrackingArea:track];
    else
        track = [NSTrackingArea alloc];

    [self addTrackingArea:[track initWithRect:[self bounds] options:(NSTrackingActiveAlways | NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved) owner:self userInfo:nil]];

    [super updateTrackingAreas];
}

// https://developer.apple.com/documentation/appkit/nsresponder/1534192-scrollwheel?language=objc
- (void)scrollWheel:(NSEvent*)event
{
    NSWindow* const ns_window = [event window];
    CGFloat const dx = [event scrollingDeltaX];
    CGFloat const dy = [event scrollingDeltaY];
    wyn_on_scroll(wyn_cocoa.userdata, (wyn_window_t)ns_window, (wyn_coord_t)dx, (wyn_coord_t)dy);
}

// https://developer.apple.com/documentation/appkit/nsresponder/1524634-mousedown?language=objc
- (void)mouseDown:(NSEvent*)event
{
    NSWindow* const ns_window = [event window];
    NSInteger const ns_button = [event buttonNumber];
    wyn_on_mouse(wyn_cocoa.userdata, (wyn_window_t)ns_window, (wyn_button_t)ns_button, (wyn_bool_t)true);
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
    wyn_on_mouse(wyn_cocoa.userdata, (wyn_window_t)ns_window, (wyn_button_t)ns_button, (wyn_bool_t)false);
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
    wyn_on_keyboard(wyn_cocoa.userdata, (wyn_window_t)ns_window, (wyn_keycode_t)keycode, (wyn_bool_t)true);

    NSString* const ns_string = [event characters];
    const char* const text = [ns_string UTF8String];
    const size_t text_len = strlen(text);
    
    if (text_len > 0)
        wyn_on_text(wyn_cocoa.userdata, (wyn_window_t)ns_window, (const wyn_utf8_t*)text);
}

// https://developer.apple.com/documentation/appkit/nsresponder/1527436-keyup?language=objc
- (void)keyUp:(NSEvent *)event
{
    NSWindow* const ns_window = [event window];

    unsigned short keycode = [event keyCode];
    wyn_on_keyboard(wyn_cocoa.userdata, (wyn_window_t)ns_window, (wyn_keycode_t)keycode, (wyn_bool_t)false);
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
    const bool was_quitting = atomic_exchange_explicit(&wyn_cocoa.quitting, true, memory_order_relaxed);

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
extern wyn_bool_t wyn_quitting(void)
{
    return (wyn_bool_t)atomic_load_explicit(&wyn_cocoa.quitting, memory_order_relaxed);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Foundation:
 * - https://developer.apple.com/documentation/foundation/nsthread/1412704-ismainthread?language=objc
 */
extern wyn_bool_t wyn_is_this_thread(void)
{
    return (wyn_bool_t)[NSThread isMainThread];
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
        wyn_on_signal(wyn_cocoa.userdata);
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
    NSWindow* const ns_window = [[NSWindow alloc] initWithContentRect:rect styleMask:WYN_STYLE_BORDERED backing:NSBackingStoreBuffered defer:FALSE];
    if (ns_window)
    {
        [ns_window setDelegate:wyn_cocoa.delegate];
        [ns_window setContentView:wyn_cocoa.delegate];
        // [ns_window setAcceptsMouseMovedEvents:YES];
        [ns_window setInitialFirstResponder:wyn_cocoa.delegate];
        const BOOL res = [ns_window makeFirstResponder:wyn_cocoa.delegate];
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
extern wyn_coord_t wyn_window_scale(wyn_window_t const window)
{
    NSWindow* const ns_window = (NSWindow*)window;
    const CGFloat scale = [ns_window backingScaleFactor];
    return (wyn_coord_t)scale;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see AppKit:
 * - https://developer.apple.com/documentation/appkit/nswindow/1419697-frame?language=objc
 * - https://developer.apple.com/documentation/appkit/nswindow/1419108-contentrectforframerect?language=objc
 * - https://developer.apple.com/documentation/appkit/nswindow/1419260-convertrecttobacking?language=objc
 */
extern wyn_extent_t wyn_window_size(wyn_window_t const window)
{
    NSWindow* const ns_window = (NSWindow*)window;
    const NSRect frame = [ns_window frame];
    const NSRect content = [ns_window contentRectForFrameRect:frame];
    const NSRect backing = [ns_window convertRectToBacking:content];
    return (wyn_extent_t){ .w = (wyn_coord_t)(backing.size.width), .h = (wyn_coord_t)(backing.size.height) };
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see AppKit:
 * - https://developer.apple.com/documentation/appkit/nswindow/1419273-convertrectfrombacking?language=objc
 * - https://developer.apple.com/documentation/appkit/nswindow/1419100-setcontentsize?language=objc
 */
extern void wyn_window_resize(wyn_window_t const window, wyn_extent_t const extent)
{
    NSWindow* const ns_window = (NSWindow*)window;
    const NSRect backing = { .size = { .width = (CGFloat)(extent.w), .height = (CGFloat)(extent.h) } };
    const NSRect content = [ns_window convertRectFromBacking:backing];
    [ns_window setContentSize:content.size];
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyn_rect_t wyn_window_position(wyn_window_t const window)
{
    NSWindow* const ns_window = (NSWindow*)window;
    NSRect const frame = [ns_window frame];
    NSRect const content = [ns_window contentRectForFrameRect:frame];
    
    return (wyn_rect_t){
        .origin = { .x = (wyn_coord_t)content.origin.x, .y = (wyn_coord_t)content.origin.y },
        .extent = { .w = (wyn_coord_t)content.size.width, .h = (wyn_coord_t)content.size.height }
    };
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_window_reposition(wyn_window_t const window, const wyn_point_t* const origin, const wyn_extent_t* const extent)
{
    NSWindow* const ns_window = (NSWindow*)window;

    NSWindowStyleMask const old_style = [ns_window styleMask];
    const bool was_fullscreen = (old_style & NSWindowStyleMaskFullScreen) != 0;
    if (was_fullscreen) return;

    NSRect const old_frame = [ns_window frame];
    NSRect const old_content = [ns_window contentRectForFrameRect:old_frame];

    NSPoint const new_origin = (origin) ? (NSPoint){ .x = (CGFloat)origin->x, .y = (CGFloat)origin->y } : old_content.origin;
    NSSize const new_size = (extent) ? (NSSize){ .width = (CGFloat)extent->w, .height = (CGFloat)extent->h } : old_content.size;
    NSRect const new_content = { .origin = new_origin, .size = new_size };

    NSRect const new_frame = [ns_window frameRectForContentRect:new_content];
    [ns_window setFrame:new_frame display:YES];
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyn_bool_t wyn_window_is_fullscreen(wyn_window_t const window)
{
    NSWindow* const ns_window = (NSWindow*)window;

    NSWindowStyleMask const style = [ns_window styleMask];
    return (wyn_bool_t)((style & NSWindowStyleMaskFullScreen) != 0);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_window_fullscreen(wyn_window_t const window, wyn_bool_t const status)
{
    NSWindow* const ns_window = (NSWindow*)window;

    NSWindowStyleMask const old_style = [ns_window styleMask];
    const bool was_fullscreen = (old_style & NSWindowStyleMaskFullScreen) != 0;
    if (was_fullscreen == status) return;

    [ns_window toggleFullScreen:nil];
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

extern unsigned int wyn_enumerate_displays(wyn_display_callback const callback, void* const userdata)
{
    NSArray<NSScreen*>* const ns_screens = [NSScreen screens];
    const NSUInteger screen_count = [ns_screens count];
    if (!callback) return (unsigned int)screen_count;
    
    NSUInteger counter;
    for (counter = 0; counter < screen_count; ++counter)
    {
        NSScreen* const ns_screen = [ns_screens objectAtIndex:counter];

        wyn_display_t const display = (wyn_display_t)ns_screen;
        if (!callback(userdata, display)) return (unsigned int)(counter + 1);
    }
    return (unsigned int)counter;
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyn_rect_t wyn_display_position(wyn_display_t const display)
{
    WYN_ASSUME(display != NULL);

    NSScreen* const ns_screen = (NSScreen*)display;
    NSRect frame = [ns_screen frame];
    
    return (wyn_rect_t){
        .origin = { .x = (wyn_coord_t)frame.origin.x, .y = (wyn_coord_t)frame.origin.y },
        .extent = { .w = (wyn_coord_t)frame.size.width, .h = (wyn_coord_t)frame.size.height }
    };
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

extern const wyn_vb_mapping_t* wyn_vb_mapping(void)
{
    static const wyn_vb_mapping_t mapping = {
        [wyn_vb_left]   = kCGMouseButtonLeft, 
        [wyn_vb_right]  = kCGMouseButtonRight,
        [wyn_vb_middle] = kCGMouseButtonCenter,
    };
    return &mapping;
}

// --------------------------------------------------------------------------------------------------------------------------------

extern const wyn_vk_mapping_t* wyn_vk_mapping(void)
{
    static const wyn_vk_mapping_t mapping = {
        [wyn_vk_0]              = kVK_ANSI_0,
        [wyn_vk_1]              = kVK_ANSI_1,
        [wyn_vk_2]              = kVK_ANSI_2,
        [wyn_vk_3]              = kVK_ANSI_3,
        [wyn_vk_4]              = kVK_ANSI_4,
        [wyn_vk_5]              = kVK_ANSI_5,
        [wyn_vk_6]              = kVK_ANSI_6,
        [wyn_vk_7]              = kVK_ANSI_7,
        [wyn_vk_8]              = kVK_ANSI_8,
        [wyn_vk_9]              = kVK_ANSI_9,
        [wyn_vk_A]              = kVK_ANSI_A,
        [wyn_vk_B]              = kVK_ANSI_B,
        [wyn_vk_C]              = kVK_ANSI_C,
        [wyn_vk_D]              = kVK_ANSI_D,
        [wyn_vk_E]              = kVK_ANSI_E,
        [wyn_vk_F]              = kVK_ANSI_F,
        [wyn_vk_G]              = kVK_ANSI_G,
        [wyn_vk_H]              = kVK_ANSI_H,
        [wyn_vk_I]              = kVK_ANSI_I,
        [wyn_vk_J]              = kVK_ANSI_J,
        [wyn_vk_K]              = kVK_ANSI_K,
        [wyn_vk_L]              = kVK_ANSI_L,
        [wyn_vk_M]              = kVK_ANSI_M,
        [wyn_vk_N]              = kVK_ANSI_N,
        [wyn_vk_O]              = kVK_ANSI_O,
        [wyn_vk_P]              = kVK_ANSI_P,
        [wyn_vk_Q]              = kVK_ANSI_Q,
        [wyn_vk_R]              = kVK_ANSI_R,
        [wyn_vk_S]              = kVK_ANSI_S,
        [wyn_vk_T]              = kVK_ANSI_T,
        [wyn_vk_U]              = kVK_ANSI_U,
        [wyn_vk_V]              = kVK_ANSI_V,
        [wyn_vk_W]              = kVK_ANSI_W,
        [wyn_vk_X]              = kVK_ANSI_X,
        [wyn_vk_Y]              = kVK_ANSI_Y,
        [wyn_vk_Z]              = kVK_ANSI_Z,
        [wyn_vk_Left]           = kVK_LeftArrow,
        [wyn_vk_Right]          = kVK_RightArrow,
        [wyn_vk_Up]             = kVK_UpArrow,
        [wyn_vk_Down]           = kVK_DownArrow,
        [wyn_vk_Period]         = kVK_ANSI_Period,
        [wyn_vk_Comma]          = kVK_ANSI_Comma,
        [wyn_vk_Semicolon]      = kVK_ANSI_Semicolon,
        [wyn_vk_Quote]          = kVK_ANSI_Quote,
        [wyn_vk_Slash]          = kVK_ANSI_Slash,
        [wyn_vk_Backslash]      = kVK_ANSI_Backslash,
        [wyn_vk_BracketL]       = kVK_ANSI_LeftBracket,
        [wyn_vk_BracketR]       = kVK_ANSI_RightBracket,
        [wyn_vk_Plus]           = kVK_ANSI_Equal,
        [wyn_vk_Minus]          = kVK_ANSI_Minus,
        [wyn_vk_Accent]         = kVK_ANSI_Grave,
        [wyn_vk_Control]        = kVK_Control,
        [wyn_vk_Start]          = kVK_Command,
        [wyn_vk_Alt]            = kVK_Option,
        [wyn_vk_Space]          = kVK_Space,
        [wyn_vk_Backspace]      = kVK_Delete,
        [wyn_vk_Delete]         = kVK_ForwardDelete,
        [wyn_vk_Insert]         = (wyn_keycode_t)~0,
        [wyn_vk_Shift]          = kVK_Shift,
        [wyn_vk_CapsLock]       = kVK_CapsLock,
        [wyn_vk_Tab]            = kVK_Tab,
        [wyn_vk_Enter]          = kVK_Return,
        [wyn_vk_Escape]         = kVK_Escape,
        [wyn_vk_Home]           = kVK_Home,
        [wyn_vk_End]            = kVK_End,
        [wyn_vk_PageUp]         = kVK_PageUp,
        [wyn_vk_PageDown]       = kVK_PageDown,
        [wyn_vk_F1]             = kVK_F1,
        [wyn_vk_F2]             = kVK_F2,
        [wyn_vk_F3]             = kVK_F3,
        [wyn_vk_F4]             = kVK_F4,
        [wyn_vk_F5]             = kVK_F5,
        [wyn_vk_F6]             = kVK_F6,
        [wyn_vk_F7]             = kVK_F7,
        [wyn_vk_F8]             = kVK_F8,
        [wyn_vk_F9]             = kVK_F9,
        [wyn_vk_F10]            = kVK_F10,
        [wyn_vk_F11]            = kVK_F11,
        [wyn_vk_F12]            = kVK_F12,
        [wyn_vk_PrintScreen]    = (wyn_keycode_t)~0,
        [wyn_vk_ScrollLock]     = (wyn_keycode_t)~0,
        [wyn_vk_NumLock]        = (wyn_keycode_t)~0,
        [wyn_vk_Numpad0]        = kVK_ANSI_Keypad0,
        [wyn_vk_Numpad1]        = kVK_ANSI_Keypad1,
        [wyn_vk_Numpad2]        = kVK_ANSI_Keypad2,
        [wyn_vk_Numpad3]        = kVK_ANSI_Keypad3,
        [wyn_vk_Numpad4]        = kVK_ANSI_Keypad4,
        [wyn_vk_Numpad5]        = kVK_ANSI_Keypad5,
        [wyn_vk_Numpad6]        = kVK_ANSI_Keypad6,
        [wyn_vk_Numpad7]        = kVK_ANSI_Keypad7,
        [wyn_vk_Numpad8]        = kVK_ANSI_Keypad8,
        [wyn_vk_Numpad9]        = kVK_ANSI_Keypad9,
        [wyn_vk_NumpadAdd]      = kVK_ANSI_KeypadPlus,
        [wyn_vk_NumpadSubtract] = kVK_ANSI_KeypadMinus,
        [wyn_vk_NumpadMultiply] = kVK_ANSI_KeypadMultiply,
        [wyn_vk_NumpadDivide]   = kVK_ANSI_KeypadDivide,
        [wyn_vk_NumpadDecimal]  = kVK_ANSI_KeypadDecimal,
    };
    return &mapping;
}

// ================================================================================================================================
