/**
 * @file wyn_cocoa.m
 * @brief Implementation of Wyn for the Cocoa backend.
 */

#include <wyn.h>

#include <stdatomic.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>

#if __STDC_VERSION__ <= 201710L
    #define true ((wyn_bool_t)1)
    #define false ((wyn_bool_t)0)
#endif

// ================================================================================================================================
//  Private Macros
// --------------------------------------------------------------------------------------------------------------------------------

/// @see abort | <stdlib.h> [libc] (POSIX.1) | https://en.cppreference.com/w/c/program/abort | https://www.unix.com/man-page/mojave/3/abort/
#define WYN_ASSERT(expr) if (expr) {} else abort()

#ifdef NDEBUG
    #define WYN_ASSUME(expr) ((void)0)
#else
    #define WYN_ASSUME(expr) WYN_ASSERT(expr)
#endif

/// @see fprintf | <stdio.h> [libc] (POSIX.1) | https://en.cppreference.com/w/c/io/fprintf | https://www.unix.com/man-page/mojave/3/fprintf/
#define WYN_LOG(...) (void)fprintf(stderr, __VA_ARGS__)

#if __STDC_VERSION__ >= 201904L
    /// @see [[maybe_unused]] | (C23) | https://en.cppreference.com/w/c/language/attributes/maybe_unused
    #define WYN_UNUSED [[maybe_unused]]
#else
    #define WYN_UNUSED
#endif

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief Cocoa NSWindowStyleMask for Bordered Windows.
 */
#define WYN_COCOA_STYLE_BORDERED (NSWindowStyleMaskClosable | NSWindowStyleMaskTitled | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable)

/**
 * @brief Cocoa NSWindowStyleMask for Borderless Windows.
 */
#define WYN_COCOA_STYLE_BORDERLESS (NSWindowStyleMaskBorderless)

// ================================================================================================================================
//  Private Declarations
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief Delegate Class for dispatching to callbacks.
 *
 * @see NSObject | <Cocoa/Cocoa.h> <objc/NSObject.h> (macOS 10.0) | https://developer.apple.com/documentation/objectivec/nsobject?language=objc
 * @see NSApplicationDelegate | <Cocoa/Cocoa.h> <AppKit/NSApplication.h> (macOS 10.6) | https://developer.apple.com/documentation/appkit/nsapplicationdelegate?language=objc
 * @see NSWindowDelegate | <Cocoa/Cocoa.h> <AppKit/NSWindow.h> (macOS 10.6) | https://developer.apple.com/documentation/appkit/nswindowdelegate?language=objc
 * @see NSResponder | <Cocoa/Cocoa.h> <AppKit/NSResponder.h> (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsresponder?language=objc
 * @see NSView | <Cocoa/Cocoa.h> <AppKit/NSView.h> (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsview?language=objc
 */
@interface wyn_cocoa_delegate_t : NSView <NSApplicationDelegate, NSWindowDelegate>
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender;
- (void)applicationDidChangeScreenParameters:(NSNotification*)notification;
- (BOOL)windowShouldClose:(NSWindow*)sender;
- (void)windowDidExpose:(NSNotification*)notification;
- (void)windowDidBecomeKey:(NSNotification*)notification;
- (void)windowDidResignKey:(NSNotification*)notification;
- (void)windowDidResize:(NSNotification*)notification;
- (void)windowDidMove:(NSNotification*)notification;
- (void)windowDidChangeBackingProperties:(NSNotification*)notification;
- (void)mouseMoved:(NSEvent*)event;
- (void)mouseDragged:(NSEvent*)event;
- (void)rightMouseDragged:(NSEvent*)event;
- (void)otherMouseDragged:(NSEvent*)event;
- (void)mouseEntered:(NSEvent*)event;
- (void)mouseExited:(NSEvent*)event;
- (void)updateTrackingAreas;
- (void)scrollWheel:(NSEvent*)event;
- (void)mouseDown:(NSEvent*)event;
- (void)rightMouseDown:(NSEvent*)event;
- (void)otherMouseDown:(NSEvent*)event;
- (void)mouseUp:(NSEvent*)event;
- (void)rightMouseUp:(NSEvent*)event;
- (void)otherMouseUp:(NSEvent*)event;
- (void)keyDown:(NSEvent*)event;
- (void)keyUp:(NSEvent*)event;
@end

/**
 * @brief Cocoa backend state.
 */
struct wyn_cocoa_t 
{
    void* userdata; ///< The pointer provided by the user when the Event Loop was started.
    wyn_cocoa_delegate_t* delegate; ///< Instance of the Delegate Class.
    _Atomic(wyn_bool_t) quitting; ///< Flag to indicate the Event Loop is quitting.
};

/**
 * @brief Static instance of Cocoa backend.
 */
static struct wyn_cocoa_t wyn_cocoa;

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief Initializes all Wyn state.
 * @param[in] userdata [nullable] The pointer provided by the user when the Event Loop was started.
 * @return `true` if successful, `false` if there were errors.
 */
static wyn_bool_t wyn_cocoa_reinit(void* userdata);

/**
 * @brief Cleans up all Wyn state.
 */
static void wyn_cocoa_deinit(void);

/**
 * @brief Closes all remaining open Windows.
 * @param[in] arg [unused]
 */
static void wyn_cocoa_close_callback(void* arg);

/**
 * @brief Stops the running NSApplication.
 * @param[in] arg [unused]
 */
static void wyn_cocoa_stop_callback(void* arg);

/**
 * @brief Runs the platform-native Event Loop.
 */
static void wyn_cocoa_run_native(void);

// ================================================================================================================================
//  Private Definitions
// --------------------------------------------------------------------------------------------------------------------------------

static wyn_bool_t wyn_cocoa_reinit(void* const userdata)
{
    wyn_cocoa = (struct wyn_cocoa_t){
        .userdata = userdata,
        .delegate = NULL,
        .quitting = false,
    };
    {
        /// @see new | <Cocoa/Cocoa.h> <objc/NSObject.h> [objc] (macOS 10.0) | https://developer.apple.com/documentation/objectivec/nsobject/1571948-new?language=objc
        /// @see autorelease | <Cocoa/Cocoa.h> <objc/NSObject.h> [objc] (macOS 10.0) | https://developer.apple.com/documentation/objectivec/1418956-nsobject/1571951-autorelease?language=objc
        wyn_cocoa.delegate = [[wyn_cocoa_delegate_t new] autorelease];
        if (wyn_cocoa.delegate == NULL) return false;
    }
    {
        /// @see NSApplication | <Cocoa/Cocoa.h> <AppKit/NSApplication.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsapplication?language=objc
        /// @see sharedApplication | <Cocoa/Cocoa.h> <AppKit/NSApplication.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsapplication/1428360-sharedapplication?language=objc
        __kindof NSApplication* _Nonnull const ns_app = [NSApplication sharedApplication];
        
        /// @see delegate | <Cocoa/Cocoa.h> <AppKit/NSApplication.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsapplication/1428705-delegate?language=objc
        [ns_app setDelegate:wyn_cocoa.delegate];

        NSApplicationActivationPolicy const policy = [ns_app activationPolicy];
        /// @see setActivationPolicy | <Cocoa/Cocoa.h> <AppKit/NSApplication.h> [AppKit] (macOS 10.6) | https://developer.apple.com/documentation/appkit/nsapplication/1428621-setactivationpolicy?language=objc
        BOOL const res_act = [ns_app setActivationPolicy:NSApplicationActivationPolicyRegular];
        (void)(res_act == NO);
    }
    return true;
}

// --------------------------------------------------------------------------------------------------------------------------------

static void wyn_cocoa_deinit(void)
{
    /// @see dispatch_get_main_queue | <Cocoa/Cocoa.h> <dispatch/dispatch.h> [libdispatch] (macOS 10.10) | https://developer.apple.com/documentation/dispatch/1452921-dispatch_get_main_queue
    const dispatch_queue_main_t main_queue = dispatch_get_main_queue();
    /// @see dispatch_async_f | <Cocoa/Cocoa.h> <dispatch/dispatch.h> [libdispatch] (macOS 10.10) | https://developer.apple.com/documentation/dispatch/1452834-dispatch_async_f
    dispatch_async_f(main_queue, nil, wyn_cocoa_close_callback);
    dispatch_async_f(main_queue, nil, wyn_cocoa_stop_callback);
    /// @see run | <Cocoa/Cocoa.h> <AppKit/NSApplication.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsapplication/1428631-run?language=objc
    [NSApp run];

    /// @see setDelegate | <Cocoa/Cocoa.h> <AppKit/NSApplication.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsapplication/1428705-delegate?language=objc
    [NSApp setDelegate:nil];
}

// --------------------------------------------------------------------------------------------------------------------------------

static void wyn_cocoa_close_callback(void* const arg WYN_UNUSED)
{
    /// @see enumerateWindowsWithOptions | <Cocoa/Cocoa.h> <AppKit/NSApplication.h> [AppKit] (macOS 10.12) | https://developer.apple.com/documentation/appkit/nsapplication/1644472-enumeratewindowswithoptions?language=objc
    [NSApp
        enumerateWindowsWithOptions:NSWindowListOrderedFrontToBack
        usingBlock: ^void(NSWindow* const _Nonnull window, BOOL* const _Nonnull stop WYN_UNUSED)
        {
            /// @see close | <Cocoa/Cocoa.h> <AppKit/NSWindow.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nswindow/1419662-close?language=objc
            [window close];
        }
    ];
}

// --------------------------------------------------------------------------------------------------------------------------------

static void wyn_cocoa_stop_callback(void* const arg WYN_UNUSED)
{
    /// @see stop | <Cocoa/Cocoa.h> <AppKit/NSApplication.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsapplication/1428473-stop?language=objc
    [NSApp stop:nil];
}

// --------------------------------------------------------------------------------------------------------------------------------

static void wyn_cocoa_run_native(void)
{
    /// @see run | <Cocoa/Cocoa.h> <AppKit/NSApplication.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsapplication/1428631-run?language=objc
    [NSApp run];
    wyn_quit();
}

// --------------------------------------------------------------------------------------------------------------------------------

@implementation wyn_cocoa_delegate_t

/// @see applicationShouldTerminate | (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsapplicationdelegate/1428642-applicationshouldterminate?language=objc
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication* const)sender
{
    wyn_quit();
    return NSTerminateCancel;
}

/// @see applicationDidChangeScreenParameters | (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsapplicationdelegate/1428424-applicationdidchangescreenparame?language=objc
- (void)applicationDidChangeScreenParameters:(NSNotification* const)notification
{
    wyn_on_display_change(wyn_cocoa.userdata);
}

/// @see windowShouldClose | (macOS 10.0) | https://developer.apple.com/documentation/appkit/nswindowdelegate/1419380-windowshouldclose?language=objc
- (BOOL)windowShouldClose:(NSWindow* const)sender
{
    wyn_on_window_close(wyn_cocoa.userdata, (wyn_window_t)sender);
    return FALSE;
}

/// @see windowDidExpose | (macOS 10.0) | https://developer.apple.com/documentation/appkit/nswindowdelegate/1419258-windowdidexpose?language=objc
- (void)windowDidExpose:(NSNotification* const)notification
{
    /// @see object | <Cocoa/Cocoa.h> <Foundation/NSNotification.h> [Foundation] (macOS 10.0) | https://developer.apple.com/documentation/foundation/nsnotification/1414469-object?language=objc
    NSWindow* const ns_window = [notification object];
    wyn_on_window_redraw(wyn_cocoa.userdata, (wyn_window_t)ns_window);
}

/// @see windowDidBecomeKey | (macOS 10.0) | https://developer.apple.com/documentation/appkit/nswindowdelegate/1419737-windowdidbecomekey?language=objc
- (void)windowDidBecomeKey:(NSNotification* const)notification
{
    /// @see object | <Cocoa/Cocoa.h> <Foundation/NSNotification.h> [Foundation] (macOS 10.0) | https://developer.apple.com/documentation/foundation/nsnotification/1414469-object?language=objc
    NSWindow* const ns_window = [notification object];
    wyn_on_window_focus(wyn_cocoa.userdata, (wyn_window_t)ns_window, true);
}

/// @see windowDidResignKey | (macOS 10.0) | https://developer.apple.com/documentation/appkit/nswindowdelegate/1419711-windowdidresignkey?language=objc
- (void)windowDidResignKey:(NSNotification* const)notification
{
    /// @see object | <Cocoa/Cocoa.h> <Foundation/NSNotification.h> [Foundation] (macOS 10.0) | https://developer.apple.com/documentation/foundation/nsnotification/1414469-object?language=objc
    NSWindow* const ns_window = [notification object];
    wyn_on_window_focus(wyn_cocoa.userdata, (wyn_window_t)ns_window, false);
}

/// @see windowDidResize | (macOS 10.0) | https://developer.apple.com/documentation/appkit/nswindowdelegate/1419567-windowdidresize?language=objc
- (void)windowDidResize:(NSNotification* const)notification
{
    /// @see object | <Cocoa/Cocoa.h> <Foundation/NSNotification.h> [Foundation] (macOS 10.0) | https://developer.apple.com/documentation/foundation/nsnotification/1414469-object?language=objc
    NSWindow* const ns_window = [notification object];
    wyn_window_t const window = (wyn_window_t)ns_window;
    wyn_on_window_reposition(wyn_cocoa.userdata, window, wyn_window_position(window), wyn_window_scale(window));
}

/// @see windowDidMove | (macOS 10.0) | https://developer.apple.com/documentation/appkit/nswindowdelegate/1419674-windowdidmove?language=objc
- (void)windowDidMove:(NSNotification* const)notification
{
    /// @see object | <Cocoa/Cocoa.h> <Foundation/NSNotification.h> [Foundation] (macOS 10.0) | https://developer.apple.com/documentation/foundation/nsnotification/1414469-object?language=objc
    NSWindow* const ns_window = [notification object];
    wyn_window_t const window = (wyn_window_t)ns_window;
    wyn_on_window_reposition(wyn_cocoa.userdata, window, wyn_window_position(window), wyn_window_scale(window));
}

/// @see windowDidChangeBackingProperties | (macOS 10.7) | https://developer.apple.com/documentation/appkit/nswindowdelegate/1419517-windowdidchangebackingproperties?language=objc
- (void)windowDidChangeBackingProperties:(NSNotification* const)notification
{
    /// @see object | <Cocoa/Cocoa.h> <Foundation/NSNotification.h> [Foundation] (macOS 10.0) | https://developer.apple.com/documentation/foundation/nsnotification/1414469-object?language=objc
    NSWindow* const ns_window = [notification object];
    wyn_window_t const window = (wyn_window_t)ns_window;
    wyn_on_window_reposition(wyn_cocoa.userdata, window, wyn_window_position(window), wyn_window_scale(window));
}

/// @see mouseMoved | (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsresponder/1525114-mousemoved?language=objc
- (void)mouseMoved:(NSEvent* const)event
{
    /// @see window | <Cocoa/Cocoa.h> <AppKit/NSEvent.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsevent/1530808-window?language=objc
    NSWindow* const ns_window = [event window];
    /// @see locationInWindow | <Cocoa/Cocoa.h> <AppKit/NSEvent.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsevent/1529068-locationinwindow?language=objc
    NSPoint const local_point = [event locationInWindow];
    wyn_on_cursor(wyn_cocoa.userdata, (wyn_window_t)ns_window, (wyn_coord_t)local_point.x, (wyn_coord_t)local_point.y);
}

/// @see mouseDragged | (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsresponder/1527420-mousedragged?language=objc
- (void)mouseDragged:(NSEvent* const)event
{
    [self mouseMoved:event];
}

/// @see rightMouseDragged | (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsresponder/1529135-rightmousedragged?language=objc
- (void)rightMouseDragged:(NSEvent* const)event
{
    [self mouseMoved:event];
}

/// @see otherMouseDragged | (macOS 10.1) | https://developer.apple.com/documentation/appkit/nsresponder/1529804-othermousedragged?language=objc
- (void)otherMouseDragged:(NSEvent* const)event
{
    [self mouseMoved:event];
}

/// @see mouseEntered | (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsresponder/1529306-mouseentered?language=objc
- (void)mouseEntered:(NSEvent* const)event
{
    // WYN_LOG("[WYN] mouseEntered\n");
}

/// @see mouseExited | (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsresponder/1527561-mouseexited?language=objc
- (void)mouseExited:(NSEvent* const)event
{
    /// @see window | <Cocoa/Cocoa.h> <AppKit/NSEvent.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsevent/1530808-window?language=objc
    NSWindow* const ns_window = [event window];
    wyn_on_cursor_exit(wyn_cocoa.userdata, (wyn_window_t)ns_window);
}

/// @see updateTrackingAreas | (macOS 10.5) | https://developer.apple.com/documentation/appkit/nsview/1483719-updatetrackingareas?language=objc
- (void)updateTrackingAreas
{
    /// @see NSArray | <Cocoa/Cocoa.h> <AppKit/NSArray.h> (macOS 10.0) | https://developer.apple.com/documentation/foundation/nsarray?language=objc
    /// @see NSTrackingArea | <Cocoa/Cocoa.h> <AppKit/NSTrackingArea.h> (macOS 10.5) | https://developer.apple.com/documentation/appkit/nstrackingarea?language=objc
    /// @see trackingAreas | <Cocoa/Cocoa.h> <AppKit/NSView.h> [AppKit] (macOS 10.5) | https://developer.apple.com/documentation/appkit/nsview/1483333-trackingareas?language=objc
    NSArray<NSTrackingArea*>* const areas = [self trackingAreas];

    /// @see firstObject | <Cocoa/Cocoa.h> <Foundation/NSArray.h> [Foundation] (macOS 10.6) | https://developer.apple.com/documentation/foundation/nsarray/1412852-firstobject?language=objc
    NSTrackingArea* track = [areas firstObject];
    if (track == nil)
    {
        /// @see alloc | <Cocoa/Cocoa.h> <objc/NSObject.h> [objc] (macOS 10.0) | https://developer.apple.com/documentation/objectivec/nsobject/1571958-alloc?language=objc
        track = [NSTrackingArea alloc];
    }
    else
    {
        /// @see removeTrackingArea | <Cocoa/Cocoa.h> <AppKit/NSView.h> [AppKit] (macOS 10.5) | https://developer.apple.com/documentation/appkit/nsview/1483634-removetrackingarea?language=objc
        [self removeTrackingArea:track];
    }
    /// @see initWithRect | <Cocoa/Cocoa.h> <AppKit/NSTrackingArea.h> [AppKit] (macOS 10.5) | https://developer.apple.com/documentation/appkit/nstrackingarea/1524488-initwithrect?language=objc
    track = [track initWithRect:[self bounds] options:(NSTrackingActiveAlways | NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved) owner:self userInfo:nil];

    /// @see addTrackingArea | <Cocoa/Cocoa.h> <AppKit/NSView.h> [AppKit] (macOS 10.5) | https://developer.apple.com/documentation/appkit/nsview/1483489-addtrackingarea?language=objc
    [self addTrackingArea:track];
    /// @see updateTrackingAreas | <Cocoa/Cocoa.h> <AppKit/NSView.h> [AppKit] (macOS 10.5) | https://developer.apple.com/documentation/appkit/nsview/1483719-updatetrackingareas?language=objc
    [super updateTrackingAreas];
}

/// @see scrollWheel | (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsresponder/1534192-scrollwheel?language=objc
- (void)scrollWheel:(NSEvent* const)event
{
    /// @see window | <Cocoa/Cocoa.h> <AppKit/NSEvent.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsevent/1530808-window?language=objc
    NSWindow* const ns_window = [event window];
    /// @see scrollingDeltaX | <Cocoa/Cocoa.h> <AppKit/NSEvent.h> [AppKit] (macOS 10.7) | https://developer.apple.com/documentation/appkit/nsevent/1524505-scrollingdeltax?language=objc
    CGFloat const dx = [event scrollingDeltaX];
    /// @see scrollingDeltaY | <Cocoa/Cocoa.h> <AppKit/NSEvent.h> [AppKit] (macOS 10.7) | https://developer.apple.com/documentation/appkit/nsevent/1535387-scrollingdeltay?language=objc
    CGFloat const dy = [event scrollingDeltaY];
    wyn_on_scroll(wyn_cocoa.userdata, (wyn_window_t)ns_window, (wyn_coord_t)dx, (wyn_coord_t)dy);
}

/// @see mouseDown | (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsresponder/1524634-mousedown?language=objc
- (void)mouseDown:(NSEvent* const)event
{
    /// @see window | <Cocoa/Cocoa.h> <AppKit/NSEvent.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsevent/1530808-window?language=objc
    NSWindow* const ns_window = [event window];
    /// @see buttonNumber | <Cocoa/Cocoa.h> <AppKit/NSEvent.h> [AppKit] (macOS 10.1) | https://developer.apple.com/documentation/appkit/nsevent/1527828-buttonnumber?language=objc
    NSInteger const ns_button = [event buttonNumber];
    wyn_on_mouse(wyn_cocoa.userdata, (wyn_window_t)ns_window, (wyn_button_t)ns_button, true);
}

/// @see rightMouseDown | (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsresponder/1524727-rightmousedown?language=objc
- (void)rightMouseDown:(NSEvent* const)event
{
    [self mouseDown:event];
}

/// @see otherMouseDown | (macOS 10.1) | https://developer.apple.com/documentation/appkit/nsresponder/1525719-othermousedown?language=objc
- (void)otherMouseDown:(NSEvent* const)event
{
    [self mouseDown:event];
}

/// @see mouseUp | (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsresponder/1535349-mouseup?language=objc
- (void)mouseUp:(NSEvent* const)event
{
    /// @see window | <Cocoa/Cocoa.h> <AppKit/NSEvent.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsevent/1530808-window?language=objc
    NSWindow* const ns_window = [event window];
    /// @see buttonNumber | <Cocoa/Cocoa.h> <AppKit/NSEvent.h> [AppKit] (macOS 10.1) | https://developer.apple.com/documentation/appkit/nsevent/1527828-buttonnumber?language=objc
    NSInteger const ns_button = [event buttonNumber];
    wyn_on_mouse(wyn_cocoa.userdata, (wyn_window_t)ns_window, (wyn_button_t)ns_button, false);
}

/// @see rightMouseUp | (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsresponder/1526309-rightmouseup?language=objc
- (void)rightMouseUp:(NSEvent* const)event
{
    [self mouseUp:event];
}

/// @see otherMouseUp | (macOS 10.1) | https://developer.apple.com/documentation/appkit/nsresponder/1531343-othermouseup?language=objc
- (void)otherMouseUp:(NSEvent* const)event
{
    [self mouseUp:event];
}

/// @see keyDown | (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsresponder/1525805-keydown?language=objc
- (void)keyDown:(NSEvent* const)event
{
    /// @see window | <Cocoa/Cocoa.h> <AppKit/NSEvent.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsevent/1530808-window?language=objc
    NSWindow* const ns_window = [event window];
    /// @see keyCode | <Cocoa/Cocoa.h> <AppKit/NSEvent.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsevent/1534513-keycode?language=objc
    unsigned short keycode = [event keyCode];
    wyn_on_keyboard(wyn_cocoa.userdata, (wyn_window_t)ns_window, (wyn_keycode_t)keycode, true);

    /// @see characters | <Cocoa/Cocoa.h> <AppKit/NSEvent.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsevent/1534183-characters?language=objc
    NSString* const ns_string = [event characters];
    /// @see UTF8String | <Cocoa/Cocoa.h> <Foundation/NSString.h> [Foundation] (macOS 10.0) | https://developer.apple.com/documentation/foundation/nsstring/1411189-utf8string?language=objc
    const char* const text = [ns_string UTF8String];
    /// @see strlen | <string.h> [libc] (POSIX.1) | https://en.cppreference.com/w/c/string/byte/strlen | https://www.unix.com/man-page/mojave/3/strlen/
    const size_t text_len = strlen(text);
    
    if (text_len > 0)
        wyn_on_text(wyn_cocoa.userdata, (wyn_window_t)ns_window, (const wyn_utf8_t*)text);
}

/// @see keyUp | (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsresponder/1527436-keyup?language=objc
- (void)keyUp:(NSEvent* const)event
{
    /// @see window | <Cocoa/Cocoa.h> <AppKit/NSEvent.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsevent/1530808-window?language=objc
    NSWindow* const ns_window = [event window];
    /// @see keyCode | <Cocoa/Cocoa.h> <AppKit/NSEvent.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsevent/1534513-keycode?language=objc
    unsigned short keycode = [event keyCode];
    wyn_on_keyboard(wyn_cocoa.userdata, (wyn_window_t)ns_window, (wyn_keycode_t)keycode, false);
}

@end

// ================================================================================================================================
//  Public Definitions
// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_run(void* const userdata)
{
    /// @see autoreleasepool | https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/MemoryMgmt/Articles/mmAutoreleasePools.html
    @autoreleasepool
    {
        if (wyn_cocoa_reinit(userdata))
        {
            wyn_on_start(userdata);
            wyn_cocoa_run_native();
            wyn_on_stop(userdata);
        }
        wyn_cocoa_deinit();
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_quit(void)
{
    /// @see atomic_exchange_explicit | <stdatomic.h> (C11) | https://en.cppreference.com/w/c/atomic/atomic_exchange | https://www.unix.com/man-page/mojave/3/atomic_exchange_explicit/
    const wyn_bool_t was_quitting = atomic_exchange_explicit(&wyn_cocoa.quitting, true, memory_order_relaxed);

    /// @see isRunning | <Cocoa/Cocoa.h> <AppKit/NSApplication.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsapplication/1428759-running?language=objc
    if (!was_quitting && [NSApp isRunning])
    {
        /// @see stop | <Cocoa/Cocoa.h> <AppKit/NSApplication.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsapplication/1428473-stop?language=objc
        [NSApp stop:nil];
        /// @see stopPeriodicEvents | <Cocoa/Cocoa.h> <AppKit/NSEvent.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsevent/1533746-stopperiodicevents?language=objc
        [NSEvent stopPeriodicEvents];
        /// @see startPeriodicEventsAfterDelay | <Cocoa/Cocoa.h> <AppKit/NSEvent.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsevent/1526044-startperiodiceventsafterdelay?language=objc
        [NSEvent startPeriodicEventsAfterDelay:0.0 withPeriod:0.1];
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyn_bool_t wyn_quitting(void)
{
    /// @see atomic_load_explicit | <stdatomic.h> (C11) | https://en.cppreference.com/w/c/atomic/atomic_load | https://www.unix.com/man-page/mojave/3/atomic_load_explicit/
    return atomic_load_explicit(&wyn_cocoa.quitting, memory_order_relaxed);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyn_bool_t wyn_is_this_thread(void)
{
    /// @see isMainThread | <Cocoa/Cocoa.h> <Foundation/NSThread.h> [Foundation] (macOS 10.5) | https://developer.apple.com/documentation/foundation/nsthread/1412704-ismainthread?language=objc
    return (wyn_bool_t)[NSThread isMainThread];
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_signal(void)
{
    /// @see dispatch_get_main_queue | <Cocoa/Cocoa.h> <dispatch/dispatch.h> [libdispatch] (macOS 10.10) | https://developer.apple.com/documentation/dispatch/1452921-dispatch_get_main_queue
    /// @see dispatch_async_f | <Cocoa/Cocoa.h> <dispatch/dispatch.h> [libdispatch] (macOS 10.6) | https://developer.apple.com/documentation/dispatch/1452834-dispatch_async_f
    dispatch_async_f(dispatch_get_main_queue(), wyn_on_signal, wyn_cocoa.userdata);
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyn_window_t wyn_window_open(void)
{
    const NSRect rect = {
        .origin = { .x = 0.0, .y = 0.0 },
        .size = { .width = 640.0, .height = 480.0 }
    };

    /// @see NSWindow | <Cocoa/Cocoa.h> <AppKit/NSWindow.h> (macOS 10.0) | https://developer.apple.com/documentation/appkit/nswindow?language=objc
    /// @see alloc | <Cocoa/Cocoa.h> <objc/NSObject.h> [objc] (macOS 10.0) | https://developer.apple.com/documentation/objectivec/nsobject/1571958-alloc?language=objc
    /// @see initWithContentRect | <Cocoa/Cocoa.h> <AppKit/NSWindow.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nswindow/1419477-initwithcontentrect?language=objc
    NSWindow* const ns_window = [[NSWindow alloc] initWithContentRect:rect styleMask:WYN_COCOA_STYLE_BORDERED backing:NSBackingStoreBuffered defer:FALSE];
    if (ns_window)
    {
        /// @see setDelegate | <Cocoa/Cocoa.h> <AppKit/NSWindow.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nswindow/1419060-delegate?language=objc
        [ns_window setDelegate:wyn_cocoa.delegate];
        /// @see setContentView | <Cocoa/Cocoa.h> <AppKit/NSWindow.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nswindow/1419160-contentview?language=objc
        [ns_window setContentView:wyn_cocoa.delegate];
        /// @see setAcceptsMouseMovedEvents | <Cocoa/Cocoa.h> <AppKit/NSWindow.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nswindow/1419340-acceptsmousemovedevents?language=objc
        // [ns_window setAcceptsMouseMovedEvents:YES];
        /// @see setInitialFirstResponder | <Cocoa/Cocoa.h> <AppKit/NSWindow.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nswindow/1419479-initialfirstresponder?language=objc
        [ns_window setInitialFirstResponder:wyn_cocoa.delegate];
        /// @see makeFirstResponder | <Cocoa/Cocoa.h> <AppKit/NSWindow.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nswindow/1419366-makefirstresponder?language=objc
        const BOOL res_first = [ns_window makeFirstResponder:wyn_cocoa.delegate];
        WYN_ASSERT(res_first == YES);

        /// @see center | <Cocoa/Cocoa.h> <AppKit/NSWindow.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nswindow/1419090-center?language=objc
        [ns_window center];
    }
    return (wyn_window_t)ns_window;
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_window_close(wyn_window_t const window)
{
    WYN_ASSUME(window != NULL);
    NSWindow* const ns_window = (NSWindow*)window;

    /// @see close | <Cocoa/Cocoa.h> <AppKit/NSWindow.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nswindow/1419662-close?language=objc
    [ns_window close];
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_window_show(wyn_window_t const window)
{
    WYN_ASSUME(window != NULL);
    NSWindow* const ns_window = (NSWindow*)window;

    /// @see makeKeyAndOrderFront | <Cocoa/Cocoa.h> <AppKit/NSWindow.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nswindow/1419208-makekeyandorderfront?language=objc
    [ns_window makeKeyAndOrderFront:nil];
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_window_hide(wyn_window_t const window)
{
    WYN_ASSUME(window != NULL);
    NSWindow* const ns_window = (NSWindow*)window;

    /// @see orderOut | <Cocoa/Cocoa.h> <AppKit/NSWindow.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nswindow/1419660-orderout?language=objc
    [ns_window orderOut:nil];
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyn_coord_t wyn_window_scale(wyn_window_t const window)
{
    WYN_ASSUME(window != NULL);
    NSWindow* const ns_window = (NSWindow*)window;

    /// @see backingScaleFactor | <Cocoa/Cocoa.h> <AppKit/NSWindow.h> [AppKit] (macOS 10.7) | https://developer.apple.com/documentation/appkit/nswindow/1419459-backingscalefactor?language=objc
    const CGFloat scale = [ns_window backingScaleFactor];
    return (wyn_coord_t)scale;
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyn_rect_t wyn_window_position(wyn_window_t const window)
{
    WYN_ASSUME(window != NULL);
    NSWindow* const ns_window = (NSWindow*)window;
    
    /// @see frame | <Cocoa/Cocoa.h> <AppKit/NSWindow.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nswindow/1419697-frame?language=objc
    NSRect const frame = [ns_window frame];
    /// @see content | <Cocoa/Cocoa.h> <AppKit/NSWindow.h> [AppKit] (macOS 10.3) | https://developer.apple.com/documentation/appkit/nswindow/1419108-contentrectforframerect?language=objc
    NSRect const content = [ns_window contentRectForFrameRect:frame];
    
    return (wyn_rect_t){
        .origin = { .x = (wyn_coord_t)content.origin.x, .y = (wyn_coord_t)content.origin.y },
        .extent = { .w = (wyn_coord_t)content.size.width, .h = (wyn_coord_t)content.size.height }
    };
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_window_reposition(wyn_window_t const window, const wyn_point_t* const origin, const wyn_extent_t* const extent)
{
    WYN_ASSUME(window != NULL);
    NSWindow* const ns_window = (NSWindow*)window;

    /// @see styleMask | <Cocoa/Cocoa.h> <AppKit/NSWindow.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nswindow/1419078-stylemask?language=objc
    NSWindowStyleMask const old_style = [ns_window styleMask];
    const wyn_bool_t was_fullscreen = (old_style & NSWindowStyleMaskFullScreen) != 0;
    if (was_fullscreen) return;

    /// @see frame | <Cocoa/Cocoa.h> <AppKit/NSWindow.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nswindow/1419697-frame?language=objc
    NSRect const old_frame = [ns_window frame];
    /// @see content | <Cocoa/Cocoa.h> <AppKit/NSWindow.h> [AppKit] (macOS 10.3) | https://developer.apple.com/documentation/appkit/nswindow/1419108-contentrectforframerect?language=objc
    NSRect const old_content = [ns_window contentRectForFrameRect:old_frame];

    NSPoint const new_origin = (origin) ? (NSPoint){ .x = (CGFloat)origin->x, .y = (CGFloat)origin->y } : old_content.origin;
    NSSize const new_size = (extent) ? (NSSize){ .width = (CGFloat)extent->w, .height = (CGFloat)extent->h } : old_content.size;
    NSRect const new_content = { .origin = new_origin, .size = new_size };

    /// @see frameRectForContentRect | <Cocoa/Cocoa.h> <AppKit/NSWindow.h> [AppKit] (macOS 10.3) | https://developer.apple.com/documentation/appkit/nswindow/1419134-framerectforcontentrect?language=objc
    NSRect const new_frame = [ns_window frameRectForContentRect:new_content];
    /// @see setFrame | <Cocoa/Cocoa.h> <AppKit/NSWindow.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nswindow/1419753-setframe?language=objc
    [ns_window setFrame:new_frame display:YES];
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyn_bool_t wyn_window_is_fullscreen(wyn_window_t const window)
{
    WYN_ASSUME(window != NULL);
    NSWindow* const ns_window = (NSWindow*)window;

    /// @see styleMask | <Cocoa/Cocoa.h> <AppKit/NSWindow.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nswindow/1419078-stylemask?language=objc
    NSWindowStyleMask const style = [ns_window styleMask];
    return (style & NSWindowStyleMaskFullScreen) != 0;
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_window_fullscreen(wyn_window_t const window, wyn_bool_t const status)
{
    WYN_ASSUME(window != NULL);
    NSWindow* const ns_window = (NSWindow*)window;

    /// @see styleMask | <Cocoa/Cocoa.h> <AppKit/NSWindow.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nswindow/1419078-stylemask?language=objc
    NSWindowStyleMask const old_style = [ns_window styleMask];
    const wyn_bool_t was_fullscreen = (old_style & NSWindowStyleMaskFullScreen) != 0;
    if (was_fullscreen == status) return;

    /// @see toggleFullScreen | <Cocoa/Cocoa.h> <AppKit/NSWindow.h> [AppKit] (macOS 10.7) | https://developer.apple.com/documentation/appkit/nswindow/1419527-togglefullscreen?language=objc
    [ns_window toggleFullScreen:nil];
}

// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_window_retitle(wyn_window_t const window, const wyn_utf8_t* const title)
{
    WYN_ASSUME(window != NULL);
    NSWindow* const ns_window = (NSWindow*)window;

    /// @see NSString | <Cocoa/Cocoa.h> <Foundation/NSString.h> (macOS 10.0) | https://developer.apple.com/documentation/foundation/nsstring?language=objc
    /// @see stringWithUTF8String | <Cocoa/Cocoa.h> <Foundation/NSString.h> [Foundation] (macOS 10.0) | https://developer.apple.com/documentation/foundation/nsstring/1497379-stringwithutf8string?language=objc
    /// @see string | <Cocoa/Cocoa.h> <Foundation/NSString.h> [Foundation] (macOS 10.0) | https://developer.apple.com/documentation/foundation/nsstring/1497312-string?language=objc
    NSString* const ns_string = (title ? [NSString stringWithUTF8String:(const char*)title] : [NSString string]);
    WYN_ASSERT(ns_string != nil);

    /// @see setTitle | <Cocoa/Cocoa.h> <AppKit/NSWindow> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nswindow/1419404-title?language=objc
    [ns_window setTitle:ns_string];
}

// ================================================================================================================================

extern unsigned int wyn_enumerate_displays(wyn_display_callback const callback, void* const userdata)
{
    /// @see NSScreen | <Cocoa/Cocoa.h> <AppKit/NSScreen.h> (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsscreen?language=objc
    /// @see screens | <Cocoa/Cocoa.h> <AppKit/NSScreen.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nsscreen/1388393-screens?language=objc
    NSArray<NSScreen*>* const ns_screens = [NSScreen screens];
    /// @see count | <Cocoa/Cocoa.h> <Foundation/NSArray.h> [Foundation] (macOS 10.0) | https://developer.apple.com/documentation/foundation/nsarray/1409982-count?language=objc
    const NSUInteger screen_count = [ns_screens count];
    if (!callback) return (unsigned int)screen_count;
    
    NSUInteger counter;
    for (counter = 0; counter < screen_count; ++counter)
    {
        /// @see objectAtIndex | <Cocoa/Cocoa.h> <Foundation/NSArray.h> [Foundation] (macOS 10.0) | https://developer.apple.com/documentation/foundation/nsarray/1417555-objectatindex?language=objc
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

    /// @see frame | <Cocoa/Cocoa.h> <AppKit/NSWindow.h> [AppKit] (macOS 10.0) | https://developer.apple.com/documentation/appkit/nswindow/1419697-frame?language=objc
    NSRect const frame = [ns_screen frame];
    
    return (wyn_rect_t){
        .origin = { .x = (wyn_coord_t)frame.origin.x, .y = (wyn_coord_t)frame.origin.y },
        .extent = { .w = (wyn_coord_t)frame.size.width, .h = (wyn_coord_t)frame.size.height }
    };
}

// ================================================================================================================================

extern void* wyn_native_context(wyn_window_t const window)
{
    WYN_ASSUME(window != NULL);
    NSWindow* const ns_window = (NSWindow*)window;

    /// @see contentView | <Cocoa/Cocoa.h> <AppKit/NSWindow.h> [AppKit] (macOS 10.0)| https://developer.apple.com/documentation/appkit/nswindow/1419160-contentview?language=objc
    return [ns_window contentView];
}

// ================================================================================================================================

extern const wyn_vb_mapping_t* wyn_vb_mapping(void)
{
    /// @see CGMouseButton | <Cocoa/Cocoa.h> <CoreGraphics/CGEventTypes.h> (macOS 10.4) | https://developer.apple.com/documentation/coregraphics/cgmousebutton?changes=_7&language=objc
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
    /// @see CGKeyCode | <Carbon/Carbon.h> <HIToolbox/Events.h> (macOS 10.0) | https://developer.apple.com/documentation/coregraphics/cgkeycode?changes=_7&language=objc
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
