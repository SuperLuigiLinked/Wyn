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
 * @see https://www.manpagez.com/man/3/abort/
 */
#define WYN_ASSERT(expr) if (expr) {} else abort()

/**
 * @see https://en.cppreference.com/w/c/io/fprintf
 */
#define WYN_LOG(...) (void)fprintf(stderr, __VA_ARGS__)

// #define WYN_LOG_OBJC

// ================================================================================================================================
//  Private Declarations
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
 * @brief Static instance of all Wyn state.
 * @details Because Wyn can only be used on the Main Thread, it is safe to have static-storage state.
 *          This state must be global so it can be reached by callbacks on certain platforms.
 */
struct wyn_state_t 
{
    void* userdata;             ///< The pointer provided by the user when the Event Loop was started.
    _Atomic(bool) quitting;     ///< Flag to indicate the Event Loop is quitting.

    wyn_delegate_t* delegate;   ///< Instance of the Delegate Class.
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
static bool wyn_init(void* userdata);

/**
 * @brief Cleans up all Wyn state.
 */
static void wyn_terminate(void);

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
 * @see https://developer.apple.com/documentation/objectivec/nsobject/1571948-new?language=objc
 * @see https://developer.apple.com/documentation/objectivec/1418956-nsobject/1571951-autorelease?language=objc
 * @see https://developer.apple.com/documentation/appkit/nsapplication/1428360-sharedapplication?language=objc
 * @see https://developer.apple.com/documentation/appkit/nsapplication/1428705-delegate?language=objc
 */
static bool wyn_init(void* userdata)
{
    wyn_state = (struct wyn_state_t){
        .userdata = userdata,
        .quitting = false,
        .delegate = NULL,
    };
    
    [NSApplication sharedApplication];
    [NSApp activateIgnoringOtherApps:YES];

    wyn_state.delegate = [[wyn_delegate_t new] autorelease];
    if (wyn_state.delegate == NULL) return false;
    [NSApp setDelegate:wyn_state.delegate];

    return true;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://developer.apple.com/documentation/appkit/nsapplication/1428631-run?language=objc
 * @see https://developer.apple.com/documentation/appkit/nsapplication/1428705-delegate?language=objc
 * @see https://developer.apple.com/documentation/dispatch/1452921-dispatch_get_main_queue
 * @see https://developer.apple.com/documentation/dispatch/1453057-dispatch_async
 */
static void wyn_terminate(void)
{
    const dispatch_queue_main_t main_queue = dispatch_get_main_queue();
    dispatch_async_f(main_queue, nil, wyn_close_callback);
    dispatch_async_f(main_queue, nil, wyn_stop_callback);
    [NSApp run];

    [NSApp setDelegate:nil];
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://developer.apple.com/documentation/appkit/nswindow/1419662-close?language=objc
 * @see https://developer.apple.com/documentation/appkit/nsapplication/1644472-enumeratewindowswithoptions?language=objc
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
 * @see https://developer.apple.com/documentation/appkit/nsapplication/1428473-stop?language=objc
 * @see https://developer.apple.com/documentation/appkit/nsevent/1533746-stopperiodicevents?language=objc
 * @see https://developer.apple.com/documentation/appkit/nsevent/1526044-startperiodiceventsafterdelay?language=objc
 */
static void wyn_stop_callback(void* arg [[maybe_unused]])
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
    wyn_quit();
}

// --------------------------------------------------------------------------------------------------------------------------------

@implementation wyn_delegate_t

#ifdef WYN_LOG_OBJC
+ (BOOL)resolveInstanceMethod:(SEL)sel
{
    WYN_LOG("[OBJC] %s\n", sel_getName(sel));
    return [super resolveInstanceMethod:sel];
}
#endif

/**
 * @see https://developer.apple.com/documentation/appkit/nsapplicationdelegate/1428642-applicationshouldterminate?language=objc
 */
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender
{
    WYN_LOG("[OBJC] applicationShouldTerminate:\n");

    wyn_quit();
    return NSTerminateCancel;
}

/**
 * @see https://developer.apple.com/documentation/appkit/nswindowdelegate/1419380-windowshouldclose?language=objc
 */
- (BOOL)windowShouldClose:(NSWindow*)sender
{
    const wyn_window_t window = (wyn_window_t)sender;

    wyn_on_window_close_request(wyn_state.userdata, window);
    return FALSE;
}

@end

// --------------------------------------------------------------------------------------------------------------------------------

#ifdef WYN_LOG_OBJC
#include <dlfcn.h>

[[maybe_unused]]
static void wyn_objc_log_toggle(const BOOL toggle)
{
    typedef void (*FP_instrumentObjcMessageSends)(BOOL);
    static FP_instrumentObjcMessageSends fp_instrumentObjcMessageSends = 0;
    
    if (fp_instrumentObjcMessageSends == 0)
    {
        void* dl_libobjc = dlopen("/usr/lib/libobjc.dylib", RTLD_LAZY);
        WYN_ASSERT(dl_libobjc != 0);

        fp_instrumentObjcMessageSends = (FP_instrumentObjcMessageSends)dlsym(dl_libobjc, "instrumentObjcMessageSends");
        WYN_ASSERT(fp_instrumentObjcMessageSends != 0);
    }

    fp_instrumentObjcMessageSends(toggle);
    NSLog(@"[OBJC] LOGGING (%d)\n", (int)toggle);
}
#endif

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
    #ifdef WYN_LOG_OBJC
        wyn_objc_log_toggle(TRUE);
    #endif

        if (wyn_init(userdata))
        {
            wyn_on_start(userdata);
            wyn_run_native();
            wyn_on_stop(userdata);
        }
        wyn_terminate();

    #ifdef WYN_LOG_OBJC
        wyn_objc_log_toggle(FALSE);
    #endif
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://en.cppreference.com/w/c/atomic/atomic_store
 * @see https://developer.apple.com/documentation/appkit/nsapplication/1428759-running?language=objc
 * @see https://developer.apple.com/documentation/appkit/nsapplication/1428473-stop?language=objc
 * @see https://developer.apple.com/documentation/appkit/nsevent/1533746-stopperiodicevents?language=objc
 * @see https://developer.apple.com/documentation/appkit/nsevent/1526044-startperiodiceventsafterdelay?language=objc
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
 * @see https://en.cppreference.com/w/c/atomic/atomic_load
 */
extern bool wyn_quitting(void)
{
    return atomic_load_explicit(&wyn_state.quitting, memory_order_relaxed);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://developer.apple.com/documentation/foundation/nsthread/1412704-ismainthread?language=objc
 */
extern bool wyn_is_this_thread(void)
{
    return [NSThread isMainThread];
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://en.cppreference.com/w/c/atomic/atomic_load
 * @see https://en.cppreference.com/w/c/atomic/atomic_store
 * @see https://developer.apple.com/documentation/dispatch/1452921-dispatch_get_main_queue
 * @see https://developer.apple.com/documentation/dispatch/3191901-dispatch_async_and_wait
 */
extern wyn_exec_t wyn_execute(void (*func)(void *), void *arg)
{
    if (wyn_is_this_thread())
    {
        func(arg);
        return wyn_exec_success;
    }
    else
    {
        __block _Atomic(wyn_exec_t) res = wyn_exec_canceled;

        dispatch_async_and_wait(dispatch_get_main_queue(), ^{
            if (wyn_quitting()) return;
            func(arg);
            atomic_store_explicit(&res, wyn_exec_success, memory_order_release);
        });

        return atomic_load_explicit(&res, memory_order_acquire);
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://developer.apple.com/documentation/dispatch/1452921-dispatch_get_main_queue
 * @see https://developer.apple.com/documentation/dispatch/1453057-dispatch_async
 */
extern wyn_exec_t wyn_execute_async(void (*func)(void *), void *arg)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        if (wyn_quitting()) return;
        func(arg);
    });

    return wyn_exec_success;
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
 * @see https://developer.apple.com/documentation/appkit/nswindow/1419662-close?language=objc
 */
extern void wyn_close_window(wyn_window_t window)
{
    NSWindow* const nsWnd = (NSWindow*)window;
    [nsWnd close];
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
