/**
 * @file wyn_xlib.c
 * @brief Implementation of Wyn for the Xlib backend.
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
#include <X11/Xlib.h>

// ================================================================================================================================
//  Private Macros
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://man7.org/linux/man-pages/man3/abort.3.html
 */
#define WYN_ASSERT(expr) if (expr) {} else abort()

#define WYN_LOG(...) (void)fprintf(stderr, __VA_ARGS__)

// ================================================================================================================================
//  Private Declarations
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @brief Indices for Wyn Atoms.
 */
enum wyn_atom {
    wyn_atom_WM_PROTOCOLS,
    wyn_atom_WM_DELETE_WINDOW,
    wyn_atom_len,
};

/**
 * @brief Names for Wyn Atoms.
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
    
    Display* display;           ///< The connection to the X Window System.
    Atom atoms[wyn_atom_len];   ///< List of cached X Atoms.

    pid_t tid_main;             ///< Thread ID of the Main Thread.

    int xlib_fd;                ///< File Descriptor for the Xlib Connection.
    int read_pipe;              ///< File Descriptor for the Read-end of the Exec-Pipe.
    int write_pipe;             ///< File Descriptor for the Write-end of the Exec-Pipe.
};

/**
 * @brief Static instance of all Wyn state.
 * @details Because Wyn can only be used on the Main Thread, it is safe to have static-storage state.
 *          This state must be global so it can be reached by callbacks on certain platforms.
 */
static struct wyn_state_t wyn_state = {};

/**
 * @brief Struct for passing callbacks with arguments.
 */
struct wyn_callback_t
{
    void (*func)(void*);    ///< The function to call.
    void* arg;              ///< The argument to pass to the function.
    _Atomic uint32_t* flag; ///< Flag for controlling synchronous execution.
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
 * @brief Runs all pending exec-callbacks.
 * @return `true` if the Event Loop should quit, `false` otherwise.
 */
static bool wyn_clear_exec_events(void);

/**
 * @brief Responds to all pending Xlib Events.
 */
static void wyn_clear_xlib_events(void);

/**
 * @brief Returns the name for the given XEvent type.
 */
static const char* wyn_xevent_name(int type);

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

/**
 * @brief Waits until the value at `addr` is no longer equal to `val`.
 */
static void wyn_futex_wait(const _Atomic uint32_t* addr, uint32_t val);

/**
 * @brief Atomically sets the value at `addr` to `val` and wakes up to 1 thread waiting on it.
 */
static void wyn_futex_wake(_Atomic uint32_t* addr, uint32_t val);

// ================================================================================================================================
//  Private Definitions
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://man7.org/linux/man-pages/man2/gettid.2.html
 * @see https://man7.org/linux/man-pages/man2/pipe.2.html
 * @see https://man7.org/linux/man-pages/man2/fcntl.2.html
 * @see https://www.x.org/releases/current/doc/man/man3/XOpenDisplay.3.xhtml
 * @see https://www.x.org/releases/current/doc/man/man3/XSetErrorHandler.3.xhtml
 * @see https://manpages.debian.org/unstable/libx11-doc/XSetErrorHandler.3.en.html
 * @see https://tronche.com/gui/x/xlib/display/display-macros.html#ConnectionNumber
 */
static bool wyn_init(void* userdata)
{
    wyn_state = (struct wyn_state_t){
        .userdata = userdata,
        .display = NULL,
        .tid_main = 0,
        .xlib_fd = -1,
        .read_pipe = -1,
        .write_pipe = -1,
    };

    wyn_state.tid_main = gettid();

    {
        wyn_state.display = XOpenDisplay(0);
        if (wyn_state.display == 0) return false;
    }

    {
        [[maybe_unused]] const XErrorHandler prev_error = XSetErrorHandler(wyn_xlib_error_handler);
        [[maybe_unused]] const XIOErrorHandler prev_io_error = XSetIOErrorHandler(wyn_xlib_io_error_handler);
        XSetIOErrorExitHandler(wyn_state.display, wyn_xlib_io_error_exit_handler, NULL);
    }

    {        
        wyn_state.xlib_fd = ConnectionNumber(wyn_state.display);
        if (wyn_state.xlib_fd == -1) return false;
    }

    {
        int pipe_fds[2];
        const int res = pipe2(pipe_fds, O_CLOEXEC | O_DIRECT);
        if (res == -1) return false;

        wyn_state.read_pipe = pipe_fds[0];
        wyn_state.write_pipe = pipe_fds[1];

        fcntl(wyn_state.read_pipe, F_SETFL, O_NONBLOCK);
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
    (void)wyn_clear_exec_events();

    if (wyn_state.write_pipe != -1)
    {
        [[maybe_unused]] const int res = close(wyn_state.write_pipe);
    }

    if (wyn_state.read_pipe != -1)
    {
        [[maybe_unused]] const int res = close(wyn_state.read_pipe);
    }

    if (wyn_state.display != NULL)
    {
        [[maybe_unused]] const int res = XCloseDisplay(wyn_state.display);
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://man7.org/linux/man-pages/man2/poll.2.html
 * @see https://www.x.org/releases/current/doc/man/man3/XFlush.3.xhtml
 */
static void wyn_run_native(void)
{
    (void)XFlush(wyn_state.display);
    
    for (;;)
    {
        struct pollfd fds[] = {
            [0] = { .fd = wyn_state.read_pipe, .events = POLLIN, .revents = 0 },
            [1] = { .fd = wyn_state.xlib_fd, .events = POLLIN, .revents = 0 },
        };
        const nfds_t nfds = sizeof(fds) / sizeof(*fds);
        const int res_poll = poll(fds, nfds, -1);
        WYN_ASSERT((res_poll != -1) && (res_poll != 0));

        const short pipe_events = fds[0].revents;
        const short xlib_events = fds[1].revents;

        if (pipe_events != 0)
        {
            if (pipe_events != POLLIN)
                WYN_LOG("[POLL-PIPE] %04hX\n", pipe_events);

            WYN_ASSERT(pipe_events == POLLIN);

            const bool quit = wyn_clear_exec_events();
            if (quit) return;
        }
        
        if (xlib_events != 0)
        {
            if (xlib_events != POLLIN)
                WYN_LOG("[POLL-XLIB] %04hX\n", xlib_events);

            if (xlib_events & POLLIN)
                wyn_clear_xlib_events();

            if (xlib_events & POLLHUP) return;

            WYN_ASSERT(xlib_events == POLLIN);
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://man7.org/linux/man-pages/man2/read.2.html
 * @see https://man7.org/linux/man-pages/man3/memcpy.3.html
 */
static bool wyn_clear_exec_events(void)
{
    _Alignas(struct wyn_callback_t) char buf[sizeof(struct wyn_callback_t)];

    for (;;)
    {
        const ssize_t res = read(wyn_state.read_pipe, buf, sizeof(buf));
        
        if (res == -1)
        {
            WYN_ASSERT(errno == EAGAIN);
            return false;
        }
        else if (res == sizeof(struct wyn_callback_t))
        {
            struct wyn_callback_t callback;
            (void)memcpy(&callback, buf, sizeof(callback));
            
            WYN_ASSERT(callback.func != NULL);
            callback.func(callback.arg);

            if (callback.flag != NULL)
                wyn_futex_wake(callback.flag, 1);
        }
        else
        {
            WYN_ASSERT(res != 0);
            return true;
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://www.x.org/releases/current/doc/man/man3/XFlush.3.xhtml
 * @see https://www.x.org/releases/current/doc/man/man3/XNextEvent.3.xhtml
 * @see https://www.x.org/releases/current/doc/man/man3/XAnyEvent.3.xhtml
 * @see https://www.x.org/releases/current/doc/man/man3/XClientMessageEvent.3.xhtml
 * @see https://tronche.com/gui/x/icccm/sec-4.html#WM_PROTOCOLS
 * @see https://www.x.org/releases/current/doc/man/man3/XDestroyWindow.3.xhtml
 */
static void wyn_clear_xlib_events(void)
{
    #define WYN_EVT_LOG(...)
    //#define WYN_EVT_LOG(...) WYN_LOG(__VA_ARGS__)

    while (XPending(wyn_state.display) > 0)
    {       
        XEvent event;
        (void)XNextEvent(wyn_state.display, &event);

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
                        wyn_on_window_close(wyn_state.userdata, (wyn_window_t)xevt->window);
                        (void)XDestroyWindow(wyn_state.display, xevt->window);
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
            }
            break;
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
        default: return "<?>";
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://www.x.org/releases/current/doc/man/man3/XSetErrorHandler.3.xhtml
 */
static int wyn_xlib_error_handler(Display* display [[maybe_unused]], XErrorEvent* error)
{
    fprintf(stderr, "[XLIB ERROR] <%d> %hhu (%hhu.%hhu)\n",
        error->type, error->error_code, error->request_code, error->minor_code
    );
    return 0;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://www.x.org/releases/current/doc/man/man3/XSetErrorHandler.3.xhtml
 */
static int wyn_xlib_io_error_handler(Display* display [[maybe_unused]])
{
    fputs("[XLIB IO ERROR]\n", stderr);
    return 0;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://www.x.org/releases/current/doc/man/man3/XSetErrorHandler.3.xhtml
 */
static void wyn_xlib_io_error_exit_handler(Display* display [[maybe_unused]], void* userdata [[maybe_unused]])
{
    fputs("[XLIB IO ERROR EXIT]\n", stderr);
    wyn_quit();
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://en.cppreference.com/w/c/atomic/atomic_load
 * @see https://man7.org/linux/man-pages/man2/futex.2.html
 * @see https://man7.org/linux/man-pages/man2/syscall.2.html
 */
static void wyn_futex_wait(const _Atomic uint32_t* addr, uint32_t val)
{
    while (atomic_load_explicit(addr, memory_order_acquire) == val)
    {
        const long res = syscall(SYS_futex, addr, FUTEX_WAIT_PRIVATE, val, NULL);
        WYN_ASSERT((res == 0) || (errno == EAGAIN));
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://en.cppreference.com/w/c/atomic/atomic_store
 * @see https://man7.org/linux/man-pages/man2/syscall.2.html
 * @see https://man7.org/linux/man-pages/man2/futex.2.html
 */
static void wyn_futex_wake(_Atomic uint32_t* addr, uint32_t val)
{
    (void)atomic_store_explicit(addr, val, memory_order_release);
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
 * @see https://man7.org/linux/man-pages/man2/write.2.html
 */
extern void wyn_quit(void)
{
    const char buf[] = { 0 };
    const ssize_t res = write(wyn_state.write_pipe, buf, sizeof(buf));
    WYN_ASSERT(res == sizeof(buf));
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://man7.org/linux/man-pages/man2/gettid.2.html
 * @see https://man7.org/linux/man-pages/man2/write.2.html
 */
extern void wyn_execute(void (*func)(void*), void* arg)
{
    if (gettid() == wyn_state.tid_main)
    {
        func(arg);
    }
    else
    {
        _Atomic uint32_t flag = 0;

        const struct wyn_callback_t callback = { .func = func, .arg = arg, .flag = &flag };
        const ssize_t res = write(wyn_state.write_pipe, &callback, sizeof(callback));
        WYN_ASSERT(res == sizeof(callback));

        wyn_futex_wait(&flag, 0);
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://man7.org/linux/man-pages/man2/write.2.html
 */
extern void wyn_execute_async(void (*func)(void*), void* arg)
{
    const struct wyn_callback_t callback = { .func = func, .arg = arg, .flag = NULL };
    const ssize_t res = write(wyn_state.write_pipe, &callback, sizeof(callback));
    WYN_ASSERT(res == sizeof(callback));
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
    Screen* const screen = DefaultScreenOfDisplay(wyn_state.display);
    const Window root = RootWindowOfScreen(screen);

    const unsigned long mask = CWEventMask;
    XSetWindowAttributes attr = {
        .event_mask = NoEventMask
            | KeyPressMask
            | KeyReleaseMask
            | ButtonPressMask
            | ButtonReleaseMask
            | EnterWindowMask
            | LeaveWindowMask
            | PointerMotionMask
            | PointerMotionHintMask
            | Button1MotionMask
            | Button2MotionMask
            | Button3MotionMask
            | Button4MotionMask
            | Button5MotionMask
            | ButtonMotionMask
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
        wyn_state.display, root,
        0, 0, 640, 480,
        0, CopyFromParent, InputOutput, CopyFromParent,
        mask, &attr
    );

    if (xWnd != 0)
    {
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wincompatible-pointer-types-discards-qualifiers"
        const Status res_atoms = XInternAtoms(wyn_state.display, wyn_atom_names, wyn_atom_len, true, wyn_state.atoms);
        #pragma GCC diagnostic pop
        WYN_ASSERT(res_atoms != 0);

        const Status res_proto = XSetWMProtocols(wyn_state.display, xWnd, wyn_state.atoms, wyn_atom_len);
        WYN_ASSERT(res_proto != 0);
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

    [[maybe_unused]] const int res = XDestroyWindow(wyn_state.display, xWnd);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://www.x.org/releases/current/doc/man/man3/XMapWindow.3.xhtml
 */
extern void wyn_show_window(wyn_window_t window)
{
    const Window xWnd = (Window)window;
    
    [[maybe_unused]] const int res = XMapRaised(wyn_state.display, xWnd);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://www.x.org/releases/current/doc/man/man3/XUnmapWindow.3.xhtml
 */
extern void wyn_hide_window(wyn_window_t window)
{
    const Window xWnd = (Window)window;

    [[maybe_unused]] const int res = XUnmapWindow(wyn_state.display, xWnd);
}

// ================================================================================================================================
