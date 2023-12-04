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
 * @see Linux:
 * - https://man7.org/linux/man-pages/man3/abort.3.html
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
 * @see Linux:
 * - https://man7.org/linux/man-pages/man2/close.2.html
 * @see Xlib:
 * - https://www.x.org/releases/current/doc/man/man3/XOpenDisplay.3.xhtml
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
 * @see Linux:
 * - https://man7.org/linux/man-pages/man2/poll.2.html
 * @see Xlib:
 * - https://www.x.org/releases/current/doc/man/man3/XFlush.3.xhtml
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
            // https://www.x.org/releases/current/doc/man/man3/XClientMessageEvent.3.xhtml
            case ClientMessage:
            {
                const XClientMessageEvent* const xevt = &event.xclient;

                // https://tronche.com/gui/x/icccm/sec-4.html#WM_PROTOCOLS
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

            // https://www.x.org/releases/current/doc/man/man3/XClientMessageEvent.3.xhtml
            case Expose:
            {
                const XExposeEvent* const xevt = &event.xexpose;
                wyn_on_window_redraw(wyn_state.userdata, (wyn_window_t)xevt->window);
                break;
            }

            case ConfigureNotify:
            {
                const XConfigureEvent* const xevt = &event.xconfigure;
                const wyn_rect_t content = {
                    .origin = { .x = (wyn_coord_t)xevt->x, .y = (wyn_coord_t)xevt->y },
                    .extent = { .w =(wyn_coord_t)xevt->width, .h = (wyn_coord_t)xevt->height }
                };
                wyn_on_window_reposition(wyn_state.userdata, (wyn_window_t)xevt->window, content, (wyn_coord_t)1.0);
                break;
            }

            case MotionNotify:
            {
                const XPointerMovedEvent* const xevt = &event.xmotion;
                wyn_on_cursor(wyn_state.userdata, (wyn_window_t)xevt->window, (wyn_coord_t)xevt->x, (wyn_coord_t)xevt->y);
                break;
            }
            
            case EnterNotify:
            {
                const XEnterWindowEvent* const xevt = &event.xcrossing;
                (void)xevt;
                break;
            }
            
            case LeaveNotify:
            {
                const XLeaveWindowEvent* const xevt = &event.xcrossing;
                (void)xevt;
                break;
            }

            case ButtonPress:
            {
                const XButtonPressedEvent* const xevt = &event.xbutton;
                
                switch (xevt->button)
                {
                case 4:
                    wyn_on_scroll(wyn_state.userdata, (wyn_window_t)xevt->window, (wyn_coord_t)0.0, (wyn_coord_t)1.0);
                    break;
                case 5:
                    wyn_on_scroll(wyn_state.userdata, (wyn_window_t)xevt->window, (wyn_coord_t)0.0, (wyn_coord_t)-1.0);
                    break;
                case 6:
                    wyn_on_scroll(wyn_state.userdata, (wyn_window_t)xevt->window, (wyn_coord_t)-1.0, (wyn_coord_t)0.0);
                    break;
                case 7:
                    wyn_on_scroll(wyn_state.userdata, (wyn_window_t)xevt->window, (wyn_coord_t)1.0, (wyn_coord_t)0.0);
                    break;
                default:
                    wyn_on_mouse(wyn_state.userdata, (wyn_window_t)xevt->window, (wyn_button_t)xevt->button, true);
                    break;
                }

                break;
            }

            case ButtonRelease:
            {
                const XButtonReleasedEvent* const xevt = &event.xbutton;
                
                switch (xevt->button)
                {
                case 4:
                    break;
                case 5:
                    break;
                case 6:
                    break;
                case 7:
                    break;
                default:
                    wyn_on_mouse(wyn_state.userdata, (wyn_window_t)xevt->window, (wyn_button_t)xevt->button, true);
                    break;
                }

                break;
            }

            case KeyPress:
            {
                const XKeyPressedEvent* const xevt = &event.xkey;
                wyn_on_keyboard(wyn_state.userdata, (wyn_window_t)xevt->window, (wyn_keycode_t)xevt->keycode, true);

                {
                    // https://www.x.org/releases/X11R7.5/doc/man/man3/XOpenIM.3.html
                    const XIM xim = XOpenIM(wyn_state.xlib_display, NULL, NULL, NULL);
                    WYN_ASSERT(xim != NULL);
                    {
                        // https://www.x.org/releases/X11R7.5/doc/man/man3/XIMOfIC.3.html
                        const XIC xic = XCreateIC(xim,
                            XNClientWindow, xevt->window,
                            XNFocusWindow,  xevt->window,
                            XNInputStyle,   XIMPreeditNothing  | XIMStatusNothing,
                            (void*)NULL
                        );
                        WYN_ASSERT(xic != NULL);
                        {
                            // https://linux.die.net/man/3/xutf8lookupstring
                            KeySym keysym = 0;
                            Status status = 0;
                            char buffer[5] = {};
                            const int res = Xutf8LookupString(xic, &event.xkey, buffer, sizeof(buffer) - 1, &keysym, &status);
                            WYN_LOG("[WYN] <%d> (%ld) [%d] \"%.4s\"\n", (int)status, (long)keysym, (int)res, (const char*)buffer);

                            if (res > 0)
                                wyn_on_text(wyn_state.userdata, (wyn_window_t)xevt->window, (const wyn_utf8_t*)buffer);
                        }
                        XDestroyIC(xic);
                    }
                    const int res = XCloseIM(xim);
                    (void)res;
                }

                break;
            }

            case KeyRelease:
            {
                const XKeyReleasedEvent* const xevt = &event.xkey;
                wyn_on_keyboard(wyn_state.userdata, (wyn_window_t)xevt->window, (wyn_keycode_t)xevt->keycode, false);
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
 * @see Xlib:
 * - https://www.x.org/releases/current/doc/man/man3/XSetErrorHandler.3.xhtml
 */
static int wyn_xlib_error_handler(Display* const display [[maybe_unused]], XErrorEvent* const error)
{
    WYN_LOG("[XLIB ERROR] <%d> %hhu (%hhu.%hhu)\n",
        error->type, error->error_code, error->request_code, error->minor_code
    );
    return 0;
}

/**
 * @see Xlib:
 * - https://www.x.org/releases/current/doc/man/man3/XSetErrorHandler.3.xhtml
 */
static int wyn_xlib_io_error_handler(Display* const display [[maybe_unused]])
{
    WYN_LOG("[XLIB IO ERROR]\n");
    return 0;
}

/**
 * @see Xlib:
 * - https://www.x.org/releases/current/doc/man/man3/XSetErrorHandler.3.xhtml
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
 * @see C:
 * - https://en.cppreference.com/w/c/atomic/atomic_store
 */
extern void wyn_quit(void)
{
    atomic_store_explicit(&wyn_state.quitting, true, memory_order_relaxed);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see C:
 * - https://en.cppreference.com/w/c/atomic/atomic_load
 */
extern wyn_bool_t wyn_quitting(void)
{
    return (wyn_bool_t)atomic_load_explicit(&wyn_state.quitting, memory_order_relaxed);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Linux:
 * - https://man7.org/linux/man-pages/man2/gettid.2.html
 */
extern wyn_bool_t wyn_is_this_thread(void)
{
    return (wyn_bool_t)(gettid() == wyn_state.tid_main);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Linux:
 * - https://man7.org/linux/man-pages/man2/write.2.html
 */
extern void wyn_signal(void)
{
    const uint64_t val = 1;
    const ssize_t res = write(wyn_state.evt_fd, &val, sizeof(val));
    WYN_ASSERT(res != -1);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Xlib:
 * - https://www.x.org/releases/current/doc/libX11/libX11/libX11.html#Event_Masks
 * - https://www.x.org/releases/current/doc/man/man3/AllPlanes.3.xhtml
 * - https://www.x.org/releases/current/doc/man/man3/XCreateWindow.3.xhtml
 * - https://www.x.org/releases/current/doc/man/man3/XInternAtom.3.xhtml
 * - https://www.x.org/releases/current/doc/man/man3/XSetWMProtocols.3.xhtml
 */
extern wyn_window_t wyn_window_open(void)
{
#if defined(WYN_XLIB)

    Screen* const screen = DefaultScreenOfDisplay(wyn_state.xlib_display);
    const Window root = RootWindowOfScreen(screen);

    const unsigned long mask = CWEventMask;
    XSetWindowAttributes attr = {
        .event_mask = NoEventMask
            | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask
            | EnterWindowMask | LeaveWindowMask
            // | PointerMotionHintMask
            | PointerMotionMask | Button1MotionMask | Button2MotionMask | Button3MotionMask | Button4MotionMask | Button5MotionMask | ButtonMotionMask
            | KeymapStateMask
            | ExposureMask
            | VisibilityChangeMask
            | StructureNotifyMask
            // | ResizeRedirectMask
            | SubstructureNotifyMask
            // | SubstructureRedirectMask
            | FocusChangeMask
            | PropertyChangeMask
            | ColormapChangeMask
            | OwnerGrabButtonMask
    };

    const Window x11_window = XCreateWindow(
        wyn_state.xlib_display, root,
        0, 0, 640, 480,
        0, CopyFromParent, InputOutput, CopyFromParent,
        mask, &attr
    );
#elif defined(WYN_X11) || defined(WYN_XCB)
    #error "Unimplemented"
#endif

    if (x11_window != 0)
    {
    #if defined(WYN_XLIB)
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wincompatible-pointer-types-discards-qualifiers"
        const Status res_atoms = XInternAtoms(wyn_state.xlib_display, wyn_atom_names, wyn_atom_len, true, wyn_state.atoms);
        #pragma GCC diagnostic pop
        WYN_ASSERT(res_atoms != 0);

        const Status res_proto = XSetWMProtocols(wyn_state.xlib_display, x11_window, wyn_state.atoms, wyn_atom_len);
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

    return (wyn_window_t)x11_window;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Xlib:
 * - https://www.x.org/releases/current/doc/man/man3/XDestroyWindow.3.xhtml
 */
extern void wyn_window_close(wyn_window_t const window)
{
#if defined(WYN_XLIB)
    const Window x11_window = (Window)window;
    [[maybe_unused]] const int res = XDestroyWindow(wyn_state.xlib_display, x11_window);
#elif defined(WYN_X11) || defined(WYN_XCB)
    #error "Unimplemented"
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Xlib:
 * - https://www.x.org/releases/current/doc/man/man3/XMapWindow.3.xhtml
 */
extern void wyn_window_show(wyn_window_t const window)
{
#if defined(WYN_XLIB)
    const Window x11_window = (Window)window;
    [[maybe_unused]] const int res = XMapRaised(wyn_state.xlib_display, x11_window);
    wyn_dispatch_x11(true);
#elif defined(WYN_X11) || defined(WYN_XCB)
    #error "Unimplemented"
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Xlib:
 * - https://www.x.org/releases/current/doc/man/man3/XUnmapWindow.3.xhtml
 */
extern void wyn_window_hide(wyn_window_t const window)
{
#if defined(WYN_XLIB)
    const Window x11_window = (Window)window;
    [[maybe_unused]] const int res = XUnmapWindow(wyn_state.xlib_display, x11_window);
    wyn_dispatch_x11(true);
#elif defined(WYN_X11) || defined(WYN_XCB)
    #error "Unimplemented"
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------

extern wyn_coord_t wyn_window_scale(wyn_window_t const window)
{
    (void)window;
    return (wyn_coord_t)1.0;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Xlib:
 * - https://www.x.org/releases/current/doc/man/man3/XGetWindowAttributes.3.xhtml
 */
extern wyn_extent_t wyn_window_size(wyn_window_t const window)
{
#if defined(WYN_XLIB)
    const Window x11_window = (Window)window;
    
    XWindowAttributes attr;
    const Status res = XGetWindowAttributes(wyn_state.xlib_display, x11_window, &attr);
    WYN_ASSERT(res != 0);
    
    return (wyn_extent_t){ .w = (wyn_coord_t)(attr.width), .h = (wyn_coord_t)(attr.height) };
#else
    #error "Unimplemented"
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Xlib:
 * - https://www.x.org/releases/current/doc/man/man3/XConfigureWindow.3.xhtml
 */
extern void wyn_window_resize(wyn_window_t const window, wyn_extent_t const size)
{
#if defined(WYN_XLIB)
    const Window x11_window = (Window)window;
    const wyn_coord_t rounded_w = ceil(size.w);
    const wyn_coord_t rounded_h = ceil(size.h);

    [[maybe_unused]] const int res = XResizeWindow(wyn_state.xlib_display, x11_window, (unsigned int)rounded_w, (unsigned int)rounded_h);
    wyn_dispatch_x11(true);
#else
    #error "Unimplemented"
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Xlib:
 * - https://www.x.org/releases/current/doc/man/man3/XGetWindowAttributes.3.xhtml
 */
extern wyn_rect_t wyn_window_position(wyn_window_t const window)
{
#if defined(WYN_XLIB)
    const Window x11_window = (Window)window;
    
    XWindowAttributes attr;
    const Status res = XGetWindowAttributes(wyn_state.xlib_display, x11_window, &attr);
    WYN_ASSERT(res != 0);
    
    return (wyn_rect_t){
        .origin = { .x = (wyn_coord_t)(attr.x), .y = (wyn_coord_t)(attr.y) },
        .extent = { .w = (wyn_coord_t)(attr.width), .h = (wyn_coord_t)(attr.height) }
    };
#else
    #error "Unimplemented"
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Xlib:
 * - https://www.x.org/releases/current/doc/man/man3/XConfigureWindow.3.xhtml
 */
extern void wyn_window_reposition(wyn_window_t const window, const wyn_point_t* const origin, const wyn_extent_t* const extent)
{
    const wyn_coord_t rounded_x = origin ? floor(origin->x) : 0.0;
    const wyn_coord_t rounded_y = origin ? floor(origin->y) : 0.0;
    const wyn_coord_t rounded_w = extent ? ceil(extent->w) : 0.0;
    const wyn_coord_t rounded_h = extent ? ceil(extent->h) : 0.0;

#if defined(WYN_XLIB)
    const Window x11_window = (Window)window;
    
    if (origin && extent)
    {
        [[maybe_unused]] const int res = XMoveResizeWindow(wyn_state.xlib_display, x11_window, (int)rounded_x, (int)rounded_y, (unsigned int)rounded_w, (unsigned int)rounded_h);
    }
    else if (extent)
    {
        [[maybe_unused]] const int res = XResizeWindow(wyn_state.xlib_display, x11_window, (unsigned int)rounded_w, (unsigned int)rounded_h);
    }
    else if (origin)
    {
        [[maybe_unused]] const int res = XMoveWindow(wyn_state.xlib_display, x11_window, (int)rounded_x, (int)rounded_y);
    }

    wyn_dispatch_x11(true);
#else
    #error "Unimplemented"
#endif
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Xlib:
 * - https://www.x.org/releases/current/doc/man/man3/XConfigureWindow.3.xhtml
 */
extern void wyn_window_retitle(wyn_window_t const window, const wyn_utf8_t* const title)
{
#if defined(WYN_XLIB)
    [[maybe_unused]] const int res = XStoreName(wyn_state.xlib_display, (Window)window, (title ? (const char*)title : ""));
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

/**
 * @see -:
 * -
 */
extern const wyn_vb_mapping_t* wyn_vb_mapping(void)
{
    static const wyn_vb_mapping_t mapping = {
        [wyn_vb_left]   = (wyn_button_t)~0, 
        [wyn_vb_right]  = (wyn_button_t)~0,
        [wyn_vb_middle] = (wyn_button_t)~0,
    };
    return &mapping;
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see -:
 * - 
 */
extern const wyn_vk_mapping_t* wyn_vk_mapping(void)
{
    static const wyn_vk_mapping_t mapping = {
        [wyn_vk_0]              = (wyn_keycode_t)~0,
        [wyn_vk_1]              = (wyn_keycode_t)~0,
        [wyn_vk_2]              = (wyn_keycode_t)~0,
        [wyn_vk_3]              = (wyn_keycode_t)~0,
        [wyn_vk_4]              = (wyn_keycode_t)~0,
        [wyn_vk_5]              = (wyn_keycode_t)~0,
        [wyn_vk_6]              = (wyn_keycode_t)~0,
        [wyn_vk_7]              = (wyn_keycode_t)~0,
        [wyn_vk_8]              = (wyn_keycode_t)~0,
        [wyn_vk_9]              = (wyn_keycode_t)~0,
        [wyn_vk_A]              = (wyn_keycode_t)~0,
        [wyn_vk_B]              = (wyn_keycode_t)~0,
        [wyn_vk_C]              = (wyn_keycode_t)~0,
        [wyn_vk_D]              = (wyn_keycode_t)~0,
        [wyn_vk_E]              = (wyn_keycode_t)~0,
        [wyn_vk_F]              = (wyn_keycode_t)~0,
        [wyn_vk_G]              = (wyn_keycode_t)~0,
        [wyn_vk_H]              = (wyn_keycode_t)~0,
        [wyn_vk_I]              = (wyn_keycode_t)~0,
        [wyn_vk_J]              = (wyn_keycode_t)~0,
        [wyn_vk_K]              = (wyn_keycode_t)~0,
        [wyn_vk_L]              = (wyn_keycode_t)~0,
        [wyn_vk_M]              = (wyn_keycode_t)~0,
        [wyn_vk_N]              = (wyn_keycode_t)~0,
        [wyn_vk_O]              = (wyn_keycode_t)~0,
        [wyn_vk_P]              = (wyn_keycode_t)~0,
        [wyn_vk_Q]              = (wyn_keycode_t)~0,
        [wyn_vk_R]              = (wyn_keycode_t)~0,
        [wyn_vk_S]              = (wyn_keycode_t)~0,
        [wyn_vk_T]              = (wyn_keycode_t)~0,
        [wyn_vk_U]              = (wyn_keycode_t)~0,
        [wyn_vk_V]              = (wyn_keycode_t)~0,
        [wyn_vk_W]              = (wyn_keycode_t)~0,
        [wyn_vk_X]              = (wyn_keycode_t)~0,
        [wyn_vk_Y]              = (wyn_keycode_t)~0,
        [wyn_vk_Z]              = (wyn_keycode_t)~0,
        [wyn_vk_Left]           = (wyn_keycode_t)~0,
        [wyn_vk_Right]          = (wyn_keycode_t)~0,
        [wyn_vk_Up]             = (wyn_keycode_t)~0,
        [wyn_vk_Down]           = (wyn_keycode_t)~0,
        [wyn_vk_Period]         = (wyn_keycode_t)~0,
        [wyn_vk_Comma]          = (wyn_keycode_t)~0,
        [wyn_vk_Semicolon]      = (wyn_keycode_t)~0,
        [wyn_vk_Quote]          = (wyn_keycode_t)~0,
        [wyn_vk_Slash]          = (wyn_keycode_t)~0,
        [wyn_vk_Backslash]      = (wyn_keycode_t)~0,
        [wyn_vk_BracketL]       = (wyn_keycode_t)~0,
        [wyn_vk_BracketR]       = (wyn_keycode_t)~0,
        [wyn_vk_Plus]           = (wyn_keycode_t)~0,
        [wyn_vk_Minus]          = (wyn_keycode_t)~0,
        [wyn_vk_Accent]         = (wyn_keycode_t)~0,
        [wyn_vk_Control]        = (wyn_keycode_t)~0,
        [wyn_vk_Start]          = (wyn_keycode_t)~0,
        [wyn_vk_Alt]            = (wyn_keycode_t)~0,
        [wyn_vk_Space]          = (wyn_keycode_t)~0,
        [wyn_vk_Backspace]      = (wyn_keycode_t)~0,
        [wyn_vk_Delete]         = (wyn_keycode_t)~0,
        [wyn_vk_Insert]         = (wyn_keycode_t)~0,
        [wyn_vk_Shift]          = (wyn_keycode_t)~0,
        [wyn_vk_CapsLock]       = (wyn_keycode_t)~0,
        [wyn_vk_Tab]            = (wyn_keycode_t)~0,
        [wyn_vk_Enter]          = (wyn_keycode_t)~0,
        [wyn_vk_Escape]         = (wyn_keycode_t)~0,
        [wyn_vk_Home]           = (wyn_keycode_t)~0,
        [wyn_vk_End]            = (wyn_keycode_t)~0,
        [wyn_vk_PageUp]         = (wyn_keycode_t)~0,
        [wyn_vk_PageDown]       = (wyn_keycode_t)~0,
        [wyn_vk_F1]             = (wyn_keycode_t)~0,
        [wyn_vk_F2]             = (wyn_keycode_t)~0,
        [wyn_vk_F3]             = (wyn_keycode_t)~0,
        [wyn_vk_F4]             = (wyn_keycode_t)~0,
        [wyn_vk_F5]             = (wyn_keycode_t)~0,
        [wyn_vk_F6]             = (wyn_keycode_t)~0,
        [wyn_vk_F7]             = (wyn_keycode_t)~0,
        [wyn_vk_F8]             = (wyn_keycode_t)~0,
        [wyn_vk_F9]             = (wyn_keycode_t)~0,
        [wyn_vk_F10]            = (wyn_keycode_t)~0,
        [wyn_vk_F11]            = (wyn_keycode_t)~0,
        [wyn_vk_F12]            = (wyn_keycode_t)~0,
        [wyn_vk_PrintScreen]    = (wyn_keycode_t)~0,
        [wyn_vk_ScrollLock]     = (wyn_keycode_t)~0,
        [wyn_vk_NumLock]        = (wyn_keycode_t)~0,
        [wyn_vk_Numpad0]        = (wyn_keycode_t)~0,
        [wyn_vk_Numpad1]        = (wyn_keycode_t)~0,
        [wyn_vk_Numpad2]        = (wyn_keycode_t)~0,
        [wyn_vk_Numpad3]        = (wyn_keycode_t)~0,
        [wyn_vk_Numpad4]        = (wyn_keycode_t)~0,
        [wyn_vk_Numpad5]        = (wyn_keycode_t)~0,
        [wyn_vk_Numpad6]        = (wyn_keycode_t)~0,
        [wyn_vk_Numpad7]        = (wyn_keycode_t)~0,
        [wyn_vk_Numpad8]        = (wyn_keycode_t)~0,
        [wyn_vk_Numpad9]        = (wyn_keycode_t)~0,
        [wyn_vk_NumpadPlus]     = (wyn_keycode_t)~0,
        [wyn_vk_NumpadMinus]    = (wyn_keycode_t)~0,
        [wyn_vk_NumpadMultiply] = (wyn_keycode_t)~0,
        [wyn_vk_NumpadDivide]   = (wyn_keycode_t)~0,
        [wyn_vk_NumpadPeriod]   = (wyn_keycode_t)~0,
    };
    return &mapping;
}

// ================================================================================================================================
