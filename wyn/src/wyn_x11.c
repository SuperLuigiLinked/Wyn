/**
 * @file wyn_x11.c
 * @brief Implementation of Wyn for the X11 backend.
 */

#include "wyn.h"

#define _GNU_SOURCE

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <unistd.h>
#include <fcntl.h> 
#include <poll.h>
#include <sys/eventfd.h>

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
    void* userdata; ///< The pointer provided by the user when the Event Loop was started.
    _Atomic(bool) quitting; ///< Flag to indicate the Event Loop is quitting.

    pid_t tid_main; ///< Thread ID of the Main Thread.

    int x11_fd; ///< File Descriptor for the X11 Connection.
    int evt_fd; ///< File Descriptor for the Event Signaler.

#if defined(WYN_X11) || defined(WYN_XLIB)
    Display* xlib_display; ///< The Xlib Connection to the X Window System.
#endif

#if defined(WYN_X11) || defined(WYN_XCB)
    xcb_connection_t* xcb_connection; ///< The Xcb Connection to the X Window System.
#endif

#if defined(WYN_XLIB)
    Atom atoms[wyn_atom_len]; ///< List of cached X Atoms.
#elif defined(WYN_X11) || defined(WYN_XCB)
    xcb_atom_t atoms[wyn_atom_len]; ///< List of cached X Atoms.
#endif
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
 * @brief Runs the platform-native Event Loop.
 */
static void wyn_run_native(void);

/**
 * @brief Responds to all pending X11 Events.
 * @param sync If true, syncs with the X Server before polling events.
 */
static void wyn_dispatch_x11(bool sync);

/**
 * @brief Responds to all pending Signal Events.
 */
static void wyn_dispatch_evt(void);

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
static bool wyn_reinit(void* userdata)
{
    wyn_state = (struct wyn_state_t){
        .userdata = userdata,
        .quitting = false,
        .tid_main = 0,
        .x11_fd = -1,
        .evt_fd = -1,
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
        wyn_state.evt_fd = eventfd(0, EFD_SEMAPHORE);
        if (wyn_state.evt_fd == -1) return false;
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://man7.org/linux/man-pages/man2/close.2.html
 * @see https://www.x.org/releases/current/doc/man/man3/XOpenDisplay.3.xhtml
 */
static void wyn_deinit(void)
{
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
        enum { evt_idx, x11_idx, nfds };

        struct pollfd fds[nfds] = {
            [evt_idx] = { .fd = wyn_state.evt_fd, .events = POLLIN, .revents = 0 },
            [x11_idx] = { .fd = wyn_state.x11_fd, .events = POLLIN, .revents = 0 },
        };
        const int res_poll = poll(fds, nfds, -1);
        WYN_ASSERT((res_poll != -1) && (res_poll != 0));

        const short evt_events = fds[evt_idx].revents;
        const short x11_events = fds[x11_idx].revents;

        if (evt_events != 0)
        {
            if (evt_events != POLLIN) WYN_LOG("[POLL-EVT] %04hX\n", evt_events);
            WYN_ASSERT(evt_events == POLLIN);
            wyn_dispatch_evt();
        }
        
        if (x11_events != 0)
        {
            if (x11_events != POLLIN) WYN_LOG("[POLL-X11] %04hX\n", x11_events);
            WYN_ASSERT(x11_events == POLLIN);
            wyn_dispatch_x11(false);
        }
    }

    wyn_quit();
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
static void wyn_dispatch_x11(bool const sync)
{
    #define WYN_EVT_LOG(...) // WYN_LOG(__VA_ARGS__)

    if (sync)
    {
        [[maybe_unused]] const int res = XSync(wyn_state.xlib_display, False);
    }

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
                        wyn_on_window_close(wyn_state.userdata, (wyn_window_t)xevt->window);
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

static void wyn_dispatch_evt(void)
{
    uint64_t val = 0;
    const ssize_t res = read(wyn_state.evt_fd, &val, sizeof(val));
    WYN_ASSERT(res != -1);

    wyn_on_signal(wyn_state.userdata);
}

// --------------------------------------------------------------------------------------------------------------------------------

[[maybe_unused]]
static const char* wyn_xevent_name(const int type)
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
static int wyn_xlib_error_handler(Display* const display [[maybe_unused]], XErrorEvent* const error)
{
    WYN_LOG("[XLIB ERROR] <%d> %hhu (%hhu.%hhu)\n",
        error->type, error->error_code, error->request_code, error->minor_code
    );
    return 0;
}

/**
 * @see https://www.x.org/releases/current/doc/man/man3/XSetErrorHandler.3.xhtml
 */
static int wyn_xlib_io_error_handler(Display* const display [[maybe_unused]])
{
    WYN_LOG("[XLIB IO ERROR]\n");
    return 0;
}

/**
 * @see https://www.x.org/releases/current/doc/man/man3/XSetErrorHandler.3.xhtml
 */
static void wyn_xlib_io_error_exit_handler(Display* const display [[maybe_unused]], void* const userdata [[maybe_unused]])
{
    WYN_LOG("[XLIB IO ERROR EXIT]\n");
    wyn_quit();
}

#endif

// ================================================================================================================================
//  Public Definitions
// --------------------------------------------------------------------------------------------------------------------------------

extern void wyn_run(void* const userdata)
{
    if (wyn_reinit(userdata))
    {
        wyn_on_start(userdata);
        wyn_run_native();
        wyn_on_stop(userdata);
    }
    wyn_deinit();
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
extern void wyn_signal(void)
{
    const uint64_t val = 1;
    const ssize_t res = write(wyn_state.evt_fd, &val, sizeof(val));
    WYN_ASSERT(res != -1);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://www.x.org/releases/current/doc/man/man3/AllPlanes.3.xhtml
 * @see https://www.x.org/releases/current/doc/man/man3/XCreateWindow.3.xhtml
 * @see https://www.x.org/releases/current/doc/libX11/libX11/libX11.html#Event_Masks
 * @see https://www.x.org/releases/current/doc/man/man3/XInternAtom.3.xhtml
 * @see https://www.x.org/releases/current/doc/man/man3/XSetWMProtocols.3.xhtml
 */
extern wyn_window_t wyn_window_open(void)
{
#if defined(WYN_XLIB)

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
#elif defined(WYN_X11) || defined(WYN_XCB)
    #error "Unimplemented"
#endif

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
extern void wyn_window_close(wyn_window_t const window)
{
#if defined(WYN_XLIB)
    const Window xWnd = (Window)window;
    [[maybe_unused]] const int res = XDestroyWindow(wyn_state.xlib_display, xWnd);
#elif defined(WYN_X11) || defined(WYN_XCB)
    #error "Unimplemented"
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://www.x.org/releases/current/doc/man/man3/XMapWindow.3.xhtml
 */
extern void wyn_window_show(wyn_window_t const window)
{
#if defined(WYN_XLIB)
    const Window xWnd = (Window)window;
    [[maybe_unused]] const int res = XMapRaised(wyn_state.xlib_display, xWnd);
    wyn_dispatch_x11(true);
#elif defined(WYN_X11) || defined(WYN_XCB)
    #error "Unimplemented"
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see https://www.x.org/releases/current/doc/man/man3/XUnmapWindow.3.xhtml
 */
extern void wyn_window_hide(wyn_window_t const window)
{
#if defined(WYN_XLIB)
    const Window xWnd = (Window)window;
    [[maybe_unused]] const int res = XUnmapWindow(wyn_state.xlib_display, xWnd);
    wyn_dispatch_x11(true);
#elif defined(WYN_X11) || defined(WYN_XCB)
    #error "Unimplemented"
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Xlib:
 * - https://www.x.org/releases/current/doc/man/man3/XGetWindowAttributes.3.xhtml
 */
extern wyn_size_t wyn_window_size(wyn_window_t const window)
{
#if defined(WYN_XLIB)
    const Window xWnd = (Window)window;
    
    XWindowAttributes attr;
    const Status res = XGetWindowAttributes(wyn_state.xlib_display, xWnd, &attr);
    WYN_ASSERT(res != 0);
    
    return (wyn_size_t){ .w = (wyn_coord_t)(attr.width), .h = (wyn_coord_t)(attr.height) };
#else
    #error "Unimplemented"
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Xlib:
 * - https://www.x.org/releases/current/doc/man/man3/XConfigureWindow.3.xhtml
 */
extern void wyn_window_resize(wyn_window_t const window, wyn_size_t const size)
{
#if defined(WYN_XLIB)
    const Window xWnd = (Window)window;
    const wyn_coord_t rounded_w = ceil(size.w);
    const wyn_coord_t rounded_h = ceil(size.h);

    [[maybe_unused]] const int res = XResizeWindow(wyn_state.xlib_display, xWnd, (unsigned int)rounded_w, (unsigned int)rounded_h);
    wyn_dispatch_x11(true);
#else
    #error "Unimplemented"
#endif
}

// ================================================================================================================================

extern void* wyn_native_context(wyn_window_t const window)
{
    (void)window;
#if defined(WYN_X11)
    return wyn_state.xcb_connection;
#elif defined(WYN_XLIB)
    return wyn_state.xlib_display;
#elif defined(WYN_XCB)
    return wyn_state.xcb_connection;
#endif
}

// ================================================================================================================================
