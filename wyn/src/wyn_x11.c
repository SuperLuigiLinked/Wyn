/**
 * @file wyn_x11.c
 * @brief Implementation of Wyn for the X11 backend.
 */

#include "wyn.h"

#define _GNU_SOURCE

#include <stdatomic.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>
#include <fcntl.h> 
#include <poll.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include <linux/limits.h>

#if (defined(WYN_X11) + defined(WYN_XLIB) + defined(WYN_XCB)) != 1
    #error "Must specify exactly one Wyn X11 backend!"
#endif

#if defined(WYN_X11)
    #include <X11/Xlib-xcb.h>
#elif defined(WYN_XLIB)
    #include <X11/Xlib.h>
#elif defined(WYN_XCB)
    #include <xcb/xcb.h>
#endif

// ================================================================================================================================
//  Private Macros
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://man7.org/linux/man-pages/man3/abort.3.html
 */
#define WYN_ASSERT(expr) if (expr) {} else abort()

/**
 * @see https://en.cppreference.com/w/c/io/fprintf
 */
#define WYN_LOG(...) (void)fprintf(stderr, __VA_ARGS__)

// ================================================================================================================================
//  Private Declarations
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief Indices for Wyn X-Atoms.
 */
enum wyn_atom_t {
    wyn_atom_WM_PROTOCOLS,
    wyn_atom_WM_DELETE_WINDOW,
    wyn_atom_len,
};
typedef enum wyn_atom_t wyn_atom_t;

/**
 * @brief Names for Wyn X-Atoms.
 */
static const char* const wyn_atom_names[wyn_atom_len] = {
    [wyn_atom_WM_PROTOCOLS] = "WM_PROTOCOLS",
    [wyn_atom_WM_DELETE_WINDOW] = "WM_DELETE_WINDOW",
};

/**
 * @brief Internal structure for holding Wyn state.
 */
struct wyn_state_t
{
    void* userdata;             ///< The pointer provided by the user when the Event Loop was started.
    _Atomic(bool) quitting;     ///< Flag to indicate the Event Loop is quitting.

    pid_t tid_main;             ///< Thread ID of the Main Thread.

    int x11_fd;                 ///< File Descriptor for the X11 Connection.
    int read_pipe;              ///< File Descriptor for the Read-end of the Exec-Pipe.
    int write_pipe;             ///< File Descriptor for the Write-end of the Exec-Pipe.
    _Atomic(size_t) len_pipe;   ///< Count of pending Pipe events.

#if defined(WYN_X11) || defined(WYN_XLIB)
    Display* xlib_display;                  ///< The Xlib Connection to the X Window System.
#endif

#if defined(WYN_X11) || defined(WYN_XCB)
    xcb_connection_t* xcb_connection;       ///< The Xcb Connection to the X Window System.
#endif

#if defined(WYN_XLIB)
    Atom atoms[wyn_atom_len];               ///< List of cached X Atoms.
#elif defined(WYN_X11) || defined(WYN_XCB)
    xcb_atom_t atoms[wyn_atom_len];         ///< List of cached X Atoms.
#endif
};

/**
 * @brief Static instance of all Wyn state.
 * @details Because Wyn can only be used on the Main Thread, it is safe to have static-storage state.
 *          This state must be global so it can be reached by callbacks on certain platforms.
 */
static struct wyn_state_t wyn_state;

/**
 * @brief Struct for passing callbacks with arguments.
 */
struct wyn_callback_t
{
    void (*func)(void*);        ///< The function to call.
    void* arg;                  ///< The argument to pass to the function.
    _Atomic(uint32_t)* flag;    ///< Flag for controlling synchronous execution.
};

_Static_assert(sizeof(struct wyn_callback_t) <= PIPE_BUF, "Atomic pipe operations not possible.");

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
 * @brief Runs the platform-native Event Loop.
 */
static void wyn_run_native(void);

/**
 * @brief Runs all currently pending exec-callbacks.
 * @param cancel Flag to indicate that the callbacks should be cancelled instead of run.
 */
static void wyn_clear_exec_events(const bool cancel);

/**
 * @brief Responds to all pending X11 Events.
 */
static void wyn_clear_x11_events(void);

/**
 * @brief Returns the name for the given XEvent type.
 */
static const char* wyn_xevent_name(int type);

#if defined(WYN_XLIB)
    /**
    * @brief Xlib Error Handler.
    */
    static int wyn_xlib_error_handler(Display* display, XErrorEvent* error);

    /**
    * @brief Xlib IO Error Handler.
    */
    static int wyn_xlib_io_error_handler(Display* display);

    /**
    * @brief Xlib IO Error Exit Handler.
    */
    static void wyn_xlib_io_error_exit_handler(Display* display, void* userdata);
#endif

/**
 * @brief Waits until the value at `addr` is no longer equal to `val`.
 */
static uint32_t wyn_futex_wait(const _Atomic(uint32_t)* addr, uint32_t val);

/**
 * @brief Atomically sets the value at `addr` to `val` and wakes up to 1 thread waiting on it.
 */
static void wyn_futex_wake(_Atomic(uint32_t)* addr, uint32_t val);

// ================================================================================================================================
//  Private Definitions
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Linux:
 * - https://man7.org/linux/man-pages/man2/gettid.2.html
 * - https://man7.org/linux/man-pages/man2/pipe.2.html
 * @see Xlib:
 * - https://www.x.org/releases/current/doc/man/man3/XOpenDisplay.3.xhtml
 * - https://www.x.org/releases/current/doc/man/man3/XSetErrorHandler.3.xhtml
 * - https://manpages.debian.org/unstable/libx11-doc/XSetErrorHandler.3.en.html
 * - https://tronche.com/gui/x/xlib/display/display-macros.html#ConnectionNumber
 * @see Xcb:
 * - 
 */
static bool wyn_init(void* userdata)
{
    wyn_state = (struct wyn_state_t){
        .userdata = userdata,
        .quitting = false,
        .tid_main = 0,
        .x11_fd = -1,
        .read_pipe = -1,
        .write_pipe = -1,
        .len_pipe = 0,
    #if defined(WYN_X11) || defined(WYN_XLIB)
        .xlib_display = NULL,
    #endif
    #if defined(WYN_X11) || defined(WYN_XCB)
        .xcb_connection = NULL,
    #endif
        .atoms = {},
    };

    wyn_state.tid_main = gettid();

#if defined(WYN_X11) || defined(WYN_XLIB)
    {
        wyn_state.xlib_display = XOpenDisplay(0);
        if (wyn_state.xlib_display == 0) return false;
    }
#endif

#if defined(WYN_X11)
    {
        wyn_state.xcb_connection = XGetXCBConnection(wyn_state.xlib_display);
        if (wyn_state.xcb_connection == 0) return false;
        if (xcb_connection_has_error(wyn_state.xcb_connection)) return false;

        XSetEventQueueOwner(wyn_state.xlib_display, XCBOwnsEventQueue);
    }
#elif defined(WYN_XCB)
    {
        wyn_state.xcb_connection = xcb_connect(0, 0);
        if (wyn_state.xcb_connection == 0) return false;
        if (xcb_connection_has_error(wyn_state.xcb_connection)) return false;
    }
#endif

#if defined(WYN_XLIB)
    {
        [[maybe_unused]] const XErrorHandler prev_error = XSetErrorHandler(wyn_xlib_error_handler);
        [[maybe_unused]] const XIOErrorHandler prev_io_error = XSetIOErrorHandler(wyn_xlib_io_error_handler);
        XSetIOErrorExitHandler(wyn_state.xlib_display, wyn_xlib_io_error_exit_handler, NULL);
    }

    {        
        wyn_state.x11_fd = ConnectionNumber(wyn_state.xlib_display);
        if (wyn_state.x11_fd == -1) return false;
    }
#endif

    {
        int pipe_fds[2];
        const int res = pipe2(pipe_fds, O_CLOEXEC | O_DIRECT | O_NONBLOCK);
        if (res == -1) return false;

        wyn_state.read_pipe = pipe_fds[0];
        wyn_state.write_pipe = pipe_fds[1];
    }    

    return true;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://man7.org/linux/man-pages/man2/close.2.html
 * @see https://www.x.org/releases/current/doc/man/man3/XOpenDisplay.3.xhtml
 */
static void wyn_terminate(void)
{
    wyn_clear_exec_events(true);

    if (wyn_state.write_pipe != -1)
    {
        [[maybe_unused]] const int res = close(wyn_state.write_pipe);
    }

    if (wyn_state.read_pipe != -1)
    {
        [[maybe_unused]] const int res = close(wyn_state.read_pipe);
    }

#if defined(WYN_XCB)
    if (wyn_state.xcb_connection != NULL)
    {
        xcb_disconnect(wyn_state.xcb_connection);
    }
#endif

#if defined(WYN_X11) || defined(WYN_XLIB)
    if (wyn_state.xlib_display != NULL)
    {
        [[maybe_unused]] const int res = XCloseDisplay(wyn_state.xlib_display);
    }
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://man7.org/linux/man-pages/man2/poll.2.html
 * @see https://www.x.org/releases/current/doc/man/man3/XFlush.3.xhtml
 */
static void wyn_run_native(void)
{
#if defined(WYN_XLIB)
    (void)XFlush(wyn_state.xlib_display);
#endif

    while (!wyn_quitting())
    {
        enum { pipe_idx, x11_idx, nfds };

        struct pollfd fds[nfds] = {
            [pipe_idx] = { .fd = wyn_state.read_pipe, .events = POLLIN, .revents = 0 },
            [x11_idx] = { .fd = wyn_state.x11_fd, .events = POLLIN, .revents = 0 },
        };
        const int res_poll = poll(fds, nfds, -1);
        WYN_ASSERT((res_poll != -1) && (res_poll != 0));

        const short pipe_events = fds[pipe_idx].revents;
        const short x11_events = fds[x11_idx].revents;

        if (pipe_events != 0)
        {
            if (pipe_events != POLLIN) WYN_LOG("[POLL-PIPE] %04hX\n", pipe_events);
            WYN_ASSERT(pipe_events == POLLIN);
            wyn_clear_exec_events(false);
        }
        
        if (x11_events != 0)
        {
            if (x11_events != POLLIN) WYN_LOG("[POLL-XLIB] %04hX\n", x11_events);
            WYN_ASSERT(x11_events == POLLIN);
            wyn_clear_x11_events();
        }
    }

    wyn_quit();
    wyn_clear_exec_events(true);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see C:
 * - https://en.cppreference.com/w/c/atomic/atomic_load
 * - https://en.cppreference.com/w/c/atomic/atomic_fetch_sub
 * @see Linux:
 * - https://man7.org/linux/man-pages/man2/read.2.html
 */
static void wyn_clear_exec_events(const bool cancel)
{
    const size_t pending = atomic_load_explicit(&wyn_state.len_pipe, memory_order_relaxed);

    for (size_t removed = 0; removed < pending; ++removed)
    {
        struct wyn_callback_t callback;
        {
            const ssize_t bytes_read = read(wyn_state.read_pipe, &callback, sizeof(callback));
            WYN_ASSERT(bytes_read == sizeof(callback));
            WYN_ASSERT(callback.func != NULL);
        }

        if (!cancel) callback.func(callback.arg);

        if (callback.flag != NULL)
        {
            const uint32_t wake_val = (uint32_t)(cancel ? wyn_exec_canceled : wyn_exec_success);
            wyn_futex_wake(callback.flag, wake_val);
        }
    }

    atomic_fetch_sub_explicit(&wyn_state.len_pipe, pending, memory_order_relaxed);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Xlib:
 * - https://www.x.org/releases/current/doc/man/man3/XFlush.3.xhtml
 * - https://www.x.org/releases/current/doc/man/man3/XNextEvent.3.xhtml
 * - https://www.x.org/releases/current/doc/man/man3/XAnyEvent.3.xhtml
 * - https://www.x.org/releases/current/doc/man/man3/XClientMessageEvent.3.xhtml
 * - https://tronche.com/gui/x/icccm/sec-4.html#WM_PROTOCOLS
 * - https://www.x.org/releases/current/doc/man/man3/XDestroyWindow.3.xhtml
 */
static void wyn_clear_x11_events(void)
{
    #define WYN_EVT_LOG(...) // WYN_LOG(__VA_ARGS__)

    while (XPending(wyn_state.xlib_display) > 0)
    {       
        XEvent event;
        (void)XNextEvent(wyn_state.xlib_display, &event);

        WYN_EVT_LOG("[X-EVENT] (%2d) %s\n", event.type, wyn_xevent_name(event.type));

        switch (event.type)
        {
            case ClientMessage:
            {
                const XClientMessageEvent* const xevt = &event.xclient;

                if (xevt->message_type == wyn_state.atoms[wyn_atom_WM_PROTOCOLS])
                {
                    WYN_ASSERT(xevt->format == 32);
                    const Atom atom = (Atom)xevt->data.l[0];
                    if (atom == wyn_state.atoms[wyn_atom_WM_DELETE_WINDOW])
                    {
                        WYN_EVT_LOG("* WM_PROTOCOLS/WM_DELETE_WINDOW\n");
                        wyn_on_window_close_request(wyn_state.userdata, (wyn_window_t)xevt->window);
                    }
                    else
                    {
                        WYN_EVT_LOG("* WM_PROTOCOLS/(%lu)\n", (unsigned long)atom);
                    }
                }
                else
                {
                    WYN_EVT_LOG("* ClientMessage/(%lu)\n", (unsigned long)event.xclient.message_type);
                }

                break;
            }

            case Expose:
            {
                WYN_EVT_LOG("* Expose\n");

                const XExposeEvent* const xevt = &event.xexpose;

                wyn_on_window_redraw(wyn_state.userdata, (wyn_window_t)xevt->window);
                
                break;
            }
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

[[maybe_unused]]
static const char* wyn_xevent_name(int type)
{
    switch (type)
    {
        case 0:                return "<0>";
        case 1:                return "<1>";
        case KeyPress:         return "KeyPress";
        case KeyRelease:       return "KeyRelease";
        case ButtonPress:      return "ButtonPress";
        case ButtonRelease:    return "ButtonRelease";
        case MotionNotify:     return "MotionNotify";
        case EnterNotify:      return "EnterNotify";
        case LeaveNotify:      return "LeaveNotify";
        case FocusIn:          return "FocusIn";
        case FocusOut:         return "FocusOut";
        case KeymapNotify:     return "KeymapNotify";
        case Expose:           return "Expose";
        case GraphicsExpose:   return "GraphicsExpose";
        case NoExpose:         return "NoExpose";
        case VisibilityNotify: return "VisibilityNotify";
        case CreateNotify:     return "CreateNotify";
        case DestroyNotify:    return "DestroyNotify";
        case UnmapNotify:      return "UnmapNotify";
        case MapNotify:        return "MapNotify";
        case MapRequest:       return "MapRequest";
        case ReparentNotify:   return "ReparentNotify";
        case ConfigureNotify:  return "ConfigureNotify";
        case ConfigureRequest: return "ConfigureRequest";
        case GravityNotify:    return "GravityNotify";
        case ResizeRequest:    return "ResizeRequest";
        case CirculateNotify:  return "CirculateNotify";
        case CirculateRequest: return "CirculateRequest";
        case PropertyNotify:   return "PropertyNotify";
        case SelectionClear:   return "SelectionClear";
        case SelectionRequest: return "SelectionRequest";
        case SelectionNotify:  return "SelectionNotify";
        case ColormapNotify:   return "ColormapNotify";
        case ClientMessage:    return "ClientMessage";
        case MappingNotify:    return "MappingNotify";
        case GenericEvent:     return "GenericEvent";
        default:               return "<?>";
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

#if defined(WYN_XLIB)

/**
 * @see https://www.x.org/releases/current/doc/man/man3/XSetErrorHandler.3.xhtml
 */
static int wyn_xlib_error_handler(Display* display [[maybe_unused]], XErrorEvent* error)
{
    WYN_LOG("[XLIB ERROR] <%d> %hhu (%hhu.%hhu)\n",
        error->type, error->error_code, error->request_code, error->minor_code
    );
    return 0;
}

/**
 * @see https://www.x.org/releases/current/doc/man/man3/XSetErrorHandler.3.xhtml
 */
static int wyn_xlib_io_error_handler(Display* display [[maybe_unused]])
{
    WYN_LOG("[XLIB IO ERROR]\n");
    return 0;
}

/**
 * @see https://www.x.org/releases/current/doc/man/man3/XSetErrorHandler.3.xhtml
 */
static void wyn_xlib_io_error_exit_handler(Display* display [[maybe_unused]], void* userdata [[maybe_unused]])
{
    WYN_LOG("[XLIB IO ERROR EXIT]\n");
    wyn_quit();
}

#endif

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://en.cppreference.com/w/c/atomic/atomic_load
 * @see https://man7.org/linux/man-pages/man2/futex.2.html
 * @see https://man7.org/linux/man-pages/man2/syscall.2.html
 */
static uint32_t wyn_futex_wait(const _Atomic(uint32_t)* addr, uint32_t old_val)
{
    for (;;)
    {
        const uint32_t new_val = atomic_load_explicit(addr, memory_order_acquire);
        if (new_val != old_val) return new_val;

        const long res = syscall(SYS_futex, addr, FUTEX_WAIT_PRIVATE, old_val, NULL);
        WYN_ASSERT((res == 0) || (errno == EAGAIN));
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://en.cppreference.com/w/c/atomic/atomic_store
 * @see https://man7.org/linux/man-pages/man2/syscall.2.html
 * @see https://man7.org/linux/man-pages/man2/futex.2.html
 */
static void wyn_futex_wake(_Atomic(uint32_t)* addr, uint32_t new_val)
{
    atomic_store_explicit(addr, new_val, memory_order_release);
    const long res = syscall(SYS_futex, addr, FUTEX_WAKE_PRIVATE, 1);
    WYN_ASSERT(res != -1);
}

// ================================================================================================================================
//  Public Definitions
// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_run(void* userdata)
{
    if (wyn_init(userdata))
    {
        wyn_on_start(userdata);
        wyn_run_native();
        wyn_on_stop(userdata);
    }
    wyn_terminate();
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://en.cppreference.com/w/c/atomic/atomic_store
 */
extern void wyn_quit(void)
{
    atomic_store_explicit(&wyn_state.quitting, true, memory_order_relaxed);
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
 * @see https://man7.org/linux/man-pages/man2/gettid.2.html
 */
extern bool wyn_is_this_thread(void)
{
    return gettid() == wyn_state.tid_main;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://man7.org/linux/man-pages/man2/write.2.html
 */
extern wyn_exec_t wyn_execute(void (*func)(void*), void* arg)
{
    if (wyn_is_this_thread())
    {
        func(arg);
        return wyn_exec_success;
    }
    else
    {
        if (wyn_quitting()) return wyn_exec_canceled;

        _Atomic(uint32_t) flag = 0;
        const struct wyn_callback_t callback = { .func = func, .arg = arg, .flag = &flag };

        {
            const ssize_t bytes_written = write(wyn_state.write_pipe, &callback, sizeof(callback));
            if (bytes_written == -1) return wyn_exec_failed;
            
            WYN_ASSERT(bytes_written == sizeof(callback));
            atomic_fetch_add_explicit(&wyn_state.len_pipe, 1, memory_order_relaxed);
        }

        const uint32_t wait_val = (uint32_t)wyn_exec_failed;
        const uint32_t retval = wyn_futex_wait(&flag, wait_val);
        return (wyn_exec_t)retval;
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://en.cppreference.com/w/c/atomic/atomic_fetch_add
 * @see https://man7.org/linux/man-pages/man2/write.2.html
 */
extern wyn_exec_t wyn_execute_async(void (*func)(void*), void* arg)
{
    if (wyn_quitting()) return wyn_exec_canceled;

    const struct wyn_callback_t callback = { .func = func, .arg = arg, .flag = NULL };

    const ssize_t bytes_written = write(wyn_state.write_pipe, &callback, sizeof(callback));
    if (bytes_written == -1) return wyn_exec_failed;

    WYN_ASSERT(bytes_written == sizeof(callback));
    atomic_fetch_add_explicit(&wyn_state.len_pipe, 1, memory_order_relaxed);

    return wyn_exec_success;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://www.x.org/releases/current/doc/man/man3/AllPlanes.3.xhtml
 * @see https://www.x.org/releases/current/doc/man/man3/XCreateWindow.3.xhtml
 * @see https://www.x.org/releases/current/doc/libX11/libX11/libX11.html#Event_Masks
 * @see https://www.x.org/releases/current/doc/man/man3/XInternAtom.3.xhtml
 * @see https://www.x.org/releases/current/doc/man/man3/XSetWMProtocols.3.xhtml
 */
extern wyn_window_t wyn_open_window(void)
{
    Screen* const screen = DefaultScreenOfDisplay(wyn_state.xlib_display);
    const Window root = RootWindowOfScreen(screen);

    const unsigned long mask = CWEventMask;
    XSetWindowAttributes attr = {
        .event_mask = NoEventMask
            | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | EnterWindowMask | LeaveWindowMask
            | PointerMotionMask | PointerMotionHintMask | Button1MotionMask | Button2MotionMask | Button3MotionMask | Button4MotionMask | Button5MotionMask | ButtonMotionMask
            | KeymapStateMask
            | ExposureMask
            | VisibilityChangeMask
            | StructureNotifyMask
            //| ResizeRedirectMask
            | SubstructureNotifyMask
            //| SubstructureRedirectMask
            | FocusChangeMask
            | PropertyChangeMask
            | ColormapChangeMask
            | OwnerGrabButtonMask
    };

    const Window xWnd = XCreateWindow(
        wyn_state.xlib_display, root,
        0, 0, 640, 480,
        0, CopyFromParent, InputOutput, CopyFromParent,
        mask, &attr
    );

    if (xWnd != 0)
    {
    #if defined(WYN_XLIB)
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wincompatible-pointer-types-discards-qualifiers"
        const Status res_atoms = XInternAtoms(wyn_state.xlib_display, wyn_atom_names, wyn_atom_len, true, wyn_state.atoms);
        #pragma GCC diagnostic pop
        WYN_ASSERT(res_atoms != 0);

        const Status res_proto = XSetWMProtocols(wyn_state.xlib_display, xWnd, wyn_state.atoms, wyn_atom_len);
        WYN_ASSERT(res_proto != 0);
    #elif defined(WYN_X11) || defined(WYN_XCB)
        for (size_t idx = 0; idx < wyn_atom_len; ++idx)
        {
            const char* const dat = wyn_atom_names[idx];
            const size_t len = strlen(dat);

            xcb_intern_atom_cookie_t cookie = xcb_intern_atom(wyn_state.xcb_connection, true, (uint16_t)len, dat);

            xcb_generic_error_t* error = NULL;
            xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(wyn_state.xcb_connection, cookie, &error);
            
            WYN_ASSERT(error == NULL);
            WYN_ASSERT(reply != NULL);

            {
                wyn_state.atoms[idx] = reply->atom;
            }

            free(reply);
        }
    #endif
    }

    return (wyn_window_t)xWnd;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://www.x.org/releases/current/doc/man/man3/XDestroyWindow.3.xhtml
 */
extern void wyn_close_window(wyn_window_t window)
{
    const Window xWnd = (Window)window;

    [[maybe_unused]] const int res = XDestroyWindow(wyn_state.xlib_display, xWnd);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://www.x.org/releases/current/doc/man/man3/XMapWindow.3.xhtml
 */
extern void wyn_show_window(wyn_window_t window)
{
    const Window xWnd = (Window)window;
    
    [[maybe_unused]] const int res = XMapRaised(wyn_state.xlib_display, xWnd);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://www.x.org/releases/current/doc/man/man3/XUnmapWindow.3.xhtml
 */
extern void wyn_hide_window(wyn_window_t window)
{
    const Window xWnd = (Window)window;

    [[maybe_unused]] const int res = XUnmapWindow(wyn_state.xlib_display, xWnd);
}

// ================================================================================================================================
