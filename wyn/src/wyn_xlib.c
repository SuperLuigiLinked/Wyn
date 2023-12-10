/**
 * @file wyn_xlib.c
 * @brief Implementation of Wyn for the Xlib backend.
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

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/extensions/Xrandr.h>

// ================================================================================================================================
//  Private Macros
// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Linux:
 * - https://man7.org/linux/man-pages/man3/abort.3.html
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
struct wyn_xlib_t
{
    void* userdata; ///< The pointer provided by the user when the Event Loop was started.
    _Atomic(bool) quitting; ///< Flag to indicate the Event Loop is quitting.

    pid_t tid_main; ///< Thread ID of the Main Thread.

    int x11_fd; ///< File Descriptor for the X11 Connection.
    int evt_fd; ///< File Descriptor for the Event Signaler.

    Display* xlib_display; ///< The Xlib Connection to the X Window System.

    XIM xim; ///< X Input Manager

    Atom atoms[wyn_atom_len]; ///< List of cached X Atoms.
};

/**
 * @brief Static instance of all Wyn state.
 */
static struct wyn_xlib_t wyn_xlib;

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
    wyn_xlib = (struct wyn_xlib_t){
        .userdata = userdata,
        .quitting = false,
        .tid_main = 0,
        .x11_fd = -1,
        .evt_fd = -1,
        .xlib_display = NULL,
        .xim = NULL,
        .atoms = {},
    };
    {
        wyn_xlib.tid_main = gettid();
    }
    {
        wyn_xlib.xlib_display = XOpenDisplay(0);
        if (wyn_xlib.xlib_display == 0) return false;
    }
    {
        [[maybe_unused]] const XErrorHandler prev_error = XSetErrorHandler(wyn_xlib_error_handler);
        [[maybe_unused]] const XIOErrorHandler prev_io_error = XSetIOErrorHandler(wyn_xlib_io_error_handler);
        XSetIOErrorExitHandler(wyn_xlib.xlib_display, wyn_xlib_io_error_exit_handler, NULL);
    }
    {        
        wyn_xlib.x11_fd = ConnectionNumber(wyn_xlib.xlib_display);
        if (wyn_xlib.x11_fd == -1) return false;

        wyn_xlib.evt_fd = eventfd(0, EFD_SEMAPHORE);
        if (wyn_xlib.evt_fd == -1) return false;
    }
    {
        // https://www.x.org/releases/X11R7.5/doc/man/man3/XOpenIM.3.html
        wyn_xlib.xim = XOpenIM(wyn_xlib.xlib_display, NULL, NULL, NULL);
        if (wyn_xlib.xim == 0) return false;

        // https://linux.die.net/man/3/xkbsetdetectableautorepeat
        Bool res_repeat = XkbSetDetectableAutoRepeat(wyn_xlib.xlib_display, true, NULL);
        (void)res_repeat;
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
    if (wyn_xlib.xim != NULL)
    {
        // https://www.x.org/releases/X11R7.5/doc/man/man3/XOpenIM.3.html
        [[maybe_unused]] const int res = XCloseIM(wyn_xlib.xim);
    }
    if (wyn_xlib.xlib_display != NULL)
    {
        [[maybe_unused]] const int res = XCloseDisplay(wyn_xlib.xlib_display);
    }
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
    (void)XFlush(wyn_xlib.xlib_display);

    while (!wyn_quitting())
    {
        enum { evt_idx, x11_idx, nfds };

        struct pollfd fds[nfds] = {
            [evt_idx] = { .fd = wyn_xlib.evt_fd, .events = POLLIN, .revents = 0 },
            [x11_idx] = { .fd = wyn_xlib.x11_fd, .events = POLLIN, .revents = 0 },
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
        [[maybe_unused]] const int res = XSync(wyn_xlib.xlib_display, False);
    }

    while (XPending(wyn_xlib.xlib_display) > 0)
    {       
        XEvent event;
        (void)XNextEvent(wyn_xlib.xlib_display, &event);

        WYN_EVT_LOG("[X-EVENT] (%2d) %s\n", event.type, wyn_xevent_name(event.type));

        switch (event.type)
        {
            // https://www.x.org/releases/current/doc/man/man3/XClientMessageEvent.3.xhtml
            case ClientMessage:
            {
                const XClientMessageEvent* const xevt = &event.xclient;

                // https://tronche.com/gui/x/icccm/sec-4.html#WM_PROTOCOLS
                if (xevt->message_type == wyn_xlib.atoms[wyn_atom_WM_PROTOCOLS])
                {
                    WYN_ASSERT(xevt->format == 32);
                    const Atom atom = (Atom)xevt->data.l[0];
                    if (atom == wyn_xlib.atoms[wyn_atom_WM_DELETE_WINDOW])
                    {
                        WYN_EVT_LOG("* WM_PROTOCOLS/WM_DELETE_WINDOW\n");
                        wyn_on_window_close(wyn_xlib.userdata, (wyn_window_t)xevt->window);
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

            // https://www.x.org/releases/current/doc/man/man3/XExposeEvent.3.xhtml
            case Expose:
            {
                const XExposeEvent* const xevt = &event.xexpose;
                wyn_on_window_redraw(wyn_xlib.userdata, (wyn_window_t)xevt->window);
                break;
            }

            // https://www.x.org/releases/current/doc/man/man3/XFocusChangeEvent.3.xhtml
            case FocusIn:
            {
                const XFocusInEvent* const xevt = &event.xfocus;
                wyn_on_window_focus(wyn_xlib.userdata, (wyn_window_t)xevt->window, true);
                break;
            }

            // https://www.x.org/releases/current/doc/man/man3/XFocusChangeEvent.3.xhtml
            case FocusOut:
            {
                const XFocusInEvent* const xevt = &event.xfocus;
                wyn_on_window_focus(wyn_xlib.userdata, (wyn_window_t)xevt->window, false);
                break;
            }

            // https://www.x.org/releases/current/doc/man/man3/XConfigureEvent.3.xhtml
            case ConfigureNotify:
            {
                const XConfigureEvent* const xevt = &event.xconfigure;
                const wyn_rect_t content = {
                    .origin = { .x = (wyn_coord_t)xevt->x, .y = (wyn_coord_t)xevt->y },
                    .extent = { .w =(wyn_coord_t)xevt->width, .h = (wyn_coord_t)xevt->height }
                };
                wyn_on_window_reposition(wyn_xlib.userdata, (wyn_window_t)xevt->window, content, (wyn_coord_t)1.0);
                break;
            }

            // https://www.x.org/releases/current/doc/man/man3/XButtonEvent.3.xhtml
            case MotionNotify:
            {
                const XPointerMovedEvent* const xevt = &event.xmotion;
                wyn_on_cursor(wyn_xlib.userdata, (wyn_window_t)xevt->window, (wyn_coord_t)xevt->x, (wyn_coord_t)xevt->y);
                break;
            }
            
            // https://www.x.org/releases/current/doc/man/man3/XCrossingEvent.3.xhtml
            case EnterNotify:
            {
                const XEnterWindowEvent* const xevt = &event.xcrossing;
                (void)xevt;
                break;
            }
            
            // https://www.x.org/releases/current/doc/man/man3/XCrossingEvent.3.xhtml
            case LeaveNotify:
            {
                const XLeaveWindowEvent* const xevt = &event.xcrossing;
                wyn_on_cursor_exit(wyn_xlib.userdata, (wyn_window_t)xevt->window);
                break;
            }

            // https://www.x.org/releases/current/doc/man/man3/XButtonEvent.3.xhtml
            case ButtonPress:
            {
                const XButtonPressedEvent* const xevt = &event.xbutton;
                
                switch (xevt->button)
                {
                case 4:
                    wyn_on_scroll(wyn_xlib.userdata, (wyn_window_t)xevt->window, (wyn_coord_t)0.0, (wyn_coord_t)1.0);
                    break;
                case 5:
                    wyn_on_scroll(wyn_xlib.userdata, (wyn_window_t)xevt->window, (wyn_coord_t)0.0, (wyn_coord_t)-1.0);
                    break;
                case 6:
                    wyn_on_scroll(wyn_xlib.userdata, (wyn_window_t)xevt->window, (wyn_coord_t)-1.0, (wyn_coord_t)0.0);
                    break;
                case 7:
                    wyn_on_scroll(wyn_xlib.userdata, (wyn_window_t)xevt->window, (wyn_coord_t)1.0, (wyn_coord_t)0.0);
                    break;
                default:
                    wyn_on_mouse(wyn_xlib.userdata, (wyn_window_t)xevt->window, (wyn_button_t)xevt->button, true);
                    break;
                }

                break;
            }

            // https://www.x.org/releases/current/doc/man/man3/XButtonEvent.3.xhtml
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
                    wyn_on_mouse(wyn_xlib.userdata, (wyn_window_t)xevt->window, (wyn_button_t)xevt->button, false);
                    break;
                }

                break;
            }

            // https://www.x.org/releases/current/doc/man/man3/XButtonEvent.3.xhtml
            case KeyPress:
            {
                const XKeyPressedEvent* const xevt = &event.xkey;
                wyn_on_keyboard(wyn_xlib.userdata, (wyn_window_t)xevt->window, (wyn_keycode_t)xevt->keycode, true);
                // {
                //     const KeyCode keycode = (KeyCode)xevt->keycode;
                //     const KeySym keysym = XKeycodeToKeysym(wyn_xlib.xlib_display, keycode, 0);
                //     WYN_LOG("[WYN] %u -> %u\n", (unsigned)keycode, (unsigned)keysym);
                // }
                {
                    // https://www.x.org/releases/X11R7.5/doc/man/man3/XIMOfIC.3.html
                    const XIC xic = XCreateIC(wyn_xlib.xim,
                        XNInputStyle,   XIMPreeditNothing | XIMStatusNothing,
                        XNClientWindow, xevt->window,
                        XNFocusWindow,  xevt->window,
                        (void*)NULL
                    );
                    WYN_ASSERT(xic != NULL);

                    {
                        // https://linux.die.net/man/3/xutf8lookupstring
                        KeySym keysym = 0;
                        Status status = 0;
                        char buffer[5] = {};
                        const int len = Xutf8LookupString(xic, &event.xkey, buffer, sizeof(buffer) - 1, &keysym, &status);
                        
                        // WYN_LOG("[WYN] <%d> (%ld) [%d] \"%.4s\"\n", (int)status, (long)keysym, (int)len, (const char*)buffer);
                        // for (int i = 0; i < len; ++i)
                        // {
                        //     WYN_LOG("* %02X\n", (unsigned)(unsigned char)buffer[i]);
                        // }

                        if (len > 0)
                            wyn_on_text(wyn_xlib.userdata, (wyn_window_t)xevt->window, (const wyn_utf8_t*)buffer);
                    }

                    XDestroyIC(xic);
                }

                break;
            }

            // https://www.x.org/releases/current/doc/man/man3/XButtonEvent.3.xhtml
            case KeyRelease:
            {
                const XKeyReleasedEvent* const xevt = &event.xkey;
                wyn_on_keyboard(wyn_xlib.userdata, (wyn_window_t)xevt->window, (wyn_keycode_t)xevt->keycode, false);
                break;
            }
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------------------

static void wyn_dispatch_evt(void)
{
    uint64_t val = 0;
    const ssize_t res = read(wyn_xlib.evt_fd, &val, sizeof(val));
    WYN_ASSERT(res != -1);

    wyn_on_signal(wyn_xlib.userdata);
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
    atomic_store_explicit(&wyn_xlib.quitting, true, memory_order_relaxed);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see C:
 * - https://en.cppreference.com/w/c/atomic/atomic_load
 */
extern wyn_bool_t wyn_quitting(void)
{
    return (wyn_bool_t)atomic_load_explicit(&wyn_xlib.quitting, memory_order_relaxed);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Linux:
 * - https://man7.org/linux/man-pages/man2/gettid.2.html
 */
extern wyn_bool_t wyn_is_this_thread(void)
{
    return (wyn_bool_t)(gettid() == wyn_xlib.tid_main);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Linux:
 * - https://man7.org/linux/man-pages/man2/write.2.html
 */
extern void wyn_signal(void)
{
    const uint64_t val = 1;
    const ssize_t res = write(wyn_xlib.evt_fd, &val, sizeof(val));
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
    Screen* const screen = DefaultScreenOfDisplay(wyn_xlib.xlib_display);
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
        wyn_xlib.xlib_display, root,
        0, 0, 640, 480,
        0, CopyFromParent, InputOutput, CopyFromParent,
        mask, &attr
    );

    if (x11_window != 0)
    {
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wincompatible-pointer-types-discards-qualifiers"
        const Status res_atoms = XInternAtoms(wyn_xlib.xlib_display, wyn_atom_names, wyn_atom_len, true, wyn_xlib.atoms);
        #pragma GCC diagnostic pop
        WYN_ASSERT(res_atoms != 0);

        const Status res_proto = XSetWMProtocols(wyn_xlib.xlib_display, x11_window, wyn_xlib.atoms, wyn_atom_len);
        WYN_ASSERT(res_proto != 0);
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
    const Window x11_window = (Window)window;
    [[maybe_unused]] const int res = XDestroyWindow(wyn_xlib.xlib_display, x11_window);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Xlib:
 * - https://www.x.org/releases/current/doc/man/man3/XMapWindow.3.xhtml
 */
extern void wyn_window_show(wyn_window_t const window)
{
    const Window x11_window = (Window)window;
    [[maybe_unused]] const int res = XMapRaised(wyn_xlib.xlib_display, x11_window);
    wyn_dispatch_x11(true);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Xlib:
 * - https://www.x.org/releases/current/doc/man/man3/XUnmapWindow.3.xhtml
 */
extern void wyn_window_hide(wyn_window_t const window)
{
    const Window x11_window = (Window)window;
    [[maybe_unused]] const int res = XUnmapWindow(wyn_xlib.xlib_display, x11_window);
    wyn_dispatch_x11(true);
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
    const Window x11_window = (Window)window;
    
    XWindowAttributes attr;
    const Status res = XGetWindowAttributes(wyn_xlib.xlib_display, x11_window, &attr);
    WYN_ASSERT(res != 0);
    
    return (wyn_extent_t){ .w = (wyn_coord_t)(attr.width), .h = (wyn_coord_t)(attr.height) };
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Xlib:
 * - https://www.x.org/releases/current/doc/man/man3/XConfigureWindow.3.xhtml
 */
extern void wyn_window_resize(wyn_window_t const window, wyn_extent_t const size)
{
    const Window x11_window = (Window)window;
    const wyn_coord_t rounded_w = ceil(size.w);
    const wyn_coord_t rounded_h = ceil(size.h);

    [[maybe_unused]] const int res = XResizeWindow(wyn_xlib.xlib_display, x11_window, (unsigned int)rounded_w, (unsigned int)rounded_h);
    wyn_dispatch_x11(true);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Xlib:
 * - https://www.x.org/releases/current/doc/man/man3/XGetWindowAttributes.3.xhtml
 */
extern wyn_rect_t wyn_window_position(wyn_window_t const window)
{
    const Window x11_window = (Window)window;
    
    XWindowAttributes attr;
    const Status res = XGetWindowAttributes(wyn_xlib.xlib_display, x11_window, &attr);
    WYN_ASSERT(res != 0);
    
    return (wyn_rect_t){
        .origin = { .x = (wyn_coord_t)(attr.x), .y = (wyn_coord_t)(attr.y) },
        .extent = { .w = (wyn_coord_t)(attr.width), .h = (wyn_coord_t)(attr.height) }
    };
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Xlib:
 * - https://www.x.org/releases/current/doc/man/man3/XConfigureWindow.3.xhtml
 */
extern void wyn_window_reposition(wyn_window_t const window, const wyn_point_t* const origin, const wyn_extent_t* const extent, bool const borderless)
{
    const wyn_coord_t rounded_x = origin ? floor(origin->x) : 0.0;
    const wyn_coord_t rounded_y = origin ? floor(origin->y) : 0.0;
    const wyn_coord_t rounded_w = extent ? ceil(extent->w) : 0.0;
    const wyn_coord_t rounded_h = extent ? ceil(extent->h) : 0.0;

    const Window x11_window = (Window)window;
    
    if (origin && extent)
    {
        [[maybe_unused]] const int res = XMoveResizeWindow(wyn_xlib.xlib_display, x11_window, (int)rounded_x, (int)rounded_y, (unsigned int)rounded_w, (unsigned int)rounded_h);
    }
    else if (extent)
    {
        [[maybe_unused]] const int res = XResizeWindow(wyn_xlib.xlib_display, x11_window, (unsigned int)rounded_w, (unsigned int)rounded_h);
    }
    else if (origin)
    {
        [[maybe_unused]] const int res = XMoveWindow(wyn_xlib.xlib_display, x11_window, (int)rounded_x, (int)rounded_y);
    }

    wyn_dispatch_x11(true);
}

// --------------------------------------------------------------------------------------------------------------------------------

/**
 * @see Xlib:
 * - https://www.x.org/releases/current/doc/man/man3/XConfigureWindow.3.xhtml
 */
extern void wyn_window_retitle(wyn_window_t const window, const wyn_utf8_t* const title)
{
    [[maybe_unused]] const int res = XStoreName(wyn_xlib.xlib_display, (Window)window, (title ? (const char*)title : ""));
}

// ================================================================================================================================

extern void* wyn_native_context(wyn_window_t const window)
{
    (void)window;
    return wyn_xlib.xlib_display;
}

// ================================================================================================================================

extern const wyn_vb_mapping_t* wyn_vb_mapping(void)
{
    static const wyn_vb_mapping_t mapping = {
        [wyn_vb_left]   = Button1,
        [wyn_vb_right]  = Button3,
        [wyn_vb_middle] = Button2,
    };
    return &mapping;
}

// --------------------------------------------------------------------------------------------------------------------------------

static inline wyn_keycode_t wyn_map_keysym(const KeySym keysym)
{
    if (keysym == NoSymbol) return (wyn_keycode_t)~0;
    const KeyCode keycode = XKeysymToKeycode(wyn_xlib.xlib_display, keysym);
    return keycode == NoSymbol ? (wyn_keycode_t)~0 : (wyn_keycode_t)keycode; 
}

extern const wyn_vk_mapping_t* wyn_vk_mapping(void)
{
    #define WYN_MAP_VK(idx, keysym) \
        mapping[idx] = wyn_map_keysym(keysym)//; \
        //WYN_LOG("[WYN] %22s | %22s | %5u | = %3u\n", #idx, #keysym, (unsigned)keysym, (unsigned)mapping[idx])
    
    static wyn_vk_mapping_t mapping = {};
    WYN_MAP_VK(wyn_vk_0,              XK_0);
    WYN_MAP_VK(wyn_vk_1,              XK_1);
    WYN_MAP_VK(wyn_vk_2,              XK_2);
    WYN_MAP_VK(wyn_vk_3,              XK_3);
    WYN_MAP_VK(wyn_vk_4,              XK_4);
    WYN_MAP_VK(wyn_vk_5,              XK_5);
    WYN_MAP_VK(wyn_vk_6,              XK_6);
    WYN_MAP_VK(wyn_vk_7,              XK_7);
    WYN_MAP_VK(wyn_vk_8,              XK_8);
    WYN_MAP_VK(wyn_vk_9,              XK_9);
    WYN_MAP_VK(wyn_vk_A,              XK_A);
    WYN_MAP_VK(wyn_vk_B,              XK_B);
    WYN_MAP_VK(wyn_vk_C,              XK_C);
    WYN_MAP_VK(wyn_vk_D,              XK_D);
    WYN_MAP_VK(wyn_vk_E,              XK_E);
    WYN_MAP_VK(wyn_vk_F,              XK_F);
    WYN_MAP_VK(wyn_vk_G,              XK_G);
    WYN_MAP_VK(wyn_vk_H,              XK_H);
    WYN_MAP_VK(wyn_vk_I,              XK_I);
    WYN_MAP_VK(wyn_vk_J,              XK_J);
    WYN_MAP_VK(wyn_vk_K,              XK_K);
    WYN_MAP_VK(wyn_vk_L,              XK_L);
    WYN_MAP_VK(wyn_vk_M,              XK_M);
    WYN_MAP_VK(wyn_vk_N,              XK_N);
    WYN_MAP_VK(wyn_vk_O,              XK_O);
    WYN_MAP_VK(wyn_vk_P,              XK_P);
    WYN_MAP_VK(wyn_vk_Q,              XK_Q);
    WYN_MAP_VK(wyn_vk_R,              XK_R);
    WYN_MAP_VK(wyn_vk_S,              XK_S);
    WYN_MAP_VK(wyn_vk_T,              XK_T);
    WYN_MAP_VK(wyn_vk_U,              XK_U);
    WYN_MAP_VK(wyn_vk_V,              XK_V);
    WYN_MAP_VK(wyn_vk_W,              XK_W);
    WYN_MAP_VK(wyn_vk_X,              XK_X);
    WYN_MAP_VK(wyn_vk_Y,              XK_Y);
    WYN_MAP_VK(wyn_vk_Z,              XK_Z);
    WYN_MAP_VK(wyn_vk_Left,           XK_Left);
    WYN_MAP_VK(wyn_vk_Right,          XK_Right);
    WYN_MAP_VK(wyn_vk_Up,             XK_Up);
    WYN_MAP_VK(wyn_vk_Down,           XK_Down);
    WYN_MAP_VK(wyn_vk_Period,         XK_period);
    WYN_MAP_VK(wyn_vk_Comma,          XK_comma);
    WYN_MAP_VK(wyn_vk_Semicolon,      XK_semicolon);
    WYN_MAP_VK(wyn_vk_Quote,          XK_apostrophe);
    WYN_MAP_VK(wyn_vk_Slash,          XK_slash);
    WYN_MAP_VK(wyn_vk_Backslash,      XK_backslash);
    WYN_MAP_VK(wyn_vk_BracketL,       XK_bracketleft);
    WYN_MAP_VK(wyn_vk_BracketR,       XK_bracketright);
    WYN_MAP_VK(wyn_vk_Plus,           XK_plus);
    WYN_MAP_VK(wyn_vk_Minus,          XK_minus);
    WYN_MAP_VK(wyn_vk_Accent,         XK_grave);
    WYN_MAP_VK(wyn_vk_Control,        XK_Control_L);
    WYN_MAP_VK(wyn_vk_Start,          XK_Meta_L);
    WYN_MAP_VK(wyn_vk_Alt,            XK_Alt_L);
    WYN_MAP_VK(wyn_vk_Space,          XK_space);
    WYN_MAP_VK(wyn_vk_Backspace,      XK_BackSpace);
    WYN_MAP_VK(wyn_vk_Delete,         XK_Delete);
    WYN_MAP_VK(wyn_vk_Insert,         XK_Insert);
    WYN_MAP_VK(wyn_vk_Shift,          XK_Shift_L);
    WYN_MAP_VK(wyn_vk_CapsLock,       XK_Caps_Lock);
    WYN_MAP_VK(wyn_vk_Tab,            XK_Tab);
    WYN_MAP_VK(wyn_vk_Enter,          XK_Return);
    WYN_MAP_VK(wyn_vk_Escape,         XK_Escape);
    WYN_MAP_VK(wyn_vk_Home,           XK_Home);
    WYN_MAP_VK(wyn_vk_End,            XK_End);
    WYN_MAP_VK(wyn_vk_PageUp,         XK_Prior);
    WYN_MAP_VK(wyn_vk_PageDown,       XK_Next);
    WYN_MAP_VK(wyn_vk_F1,             XK_F1);
    WYN_MAP_VK(wyn_vk_F2,             XK_F2);
    WYN_MAP_VK(wyn_vk_F3,             XK_F3);
    WYN_MAP_VK(wyn_vk_F4,             XK_F4);
    WYN_MAP_VK(wyn_vk_F5,             XK_F5);
    WYN_MAP_VK(wyn_vk_F6,             XK_F6);
    WYN_MAP_VK(wyn_vk_F7,             XK_F7);
    WYN_MAP_VK(wyn_vk_F8,             XK_F8);
    WYN_MAP_VK(wyn_vk_F9,             XK_F9);
    WYN_MAP_VK(wyn_vk_F10,            XK_F10);
    WYN_MAP_VK(wyn_vk_F11,            XK_F11);
    WYN_MAP_VK(wyn_vk_F12,            XK_F12);
    WYN_MAP_VK(wyn_vk_PrintScreen,    XK_Print);
    WYN_MAP_VK(wyn_vk_ScrollLock,     XK_Scroll_Lock);
    WYN_MAP_VK(wyn_vk_NumLock,        XK_Num_Lock);
    WYN_MAP_VK(wyn_vk_Numpad0,        XK_KP_0);
    WYN_MAP_VK(wyn_vk_Numpad1,        XK_KP_1);
    WYN_MAP_VK(wyn_vk_Numpad2,        XK_KP_2);
    WYN_MAP_VK(wyn_vk_Numpad3,        XK_KP_3);
    WYN_MAP_VK(wyn_vk_Numpad4,        XK_KP_4);
    WYN_MAP_VK(wyn_vk_Numpad5,        XK_KP_5);
    WYN_MAP_VK(wyn_vk_Numpad6,        XK_KP_6);
    WYN_MAP_VK(wyn_vk_Numpad7,        XK_KP_7);
    WYN_MAP_VK(wyn_vk_Numpad8,        XK_KP_8);
    WYN_MAP_VK(wyn_vk_Numpad9,        XK_KP_9);
    WYN_MAP_VK(wyn_vk_NumpadAdd,      XK_KP_Add);
    WYN_MAP_VK(wyn_vk_NumpadSubtract, XK_KP_Subtract);
    WYN_MAP_VK(wyn_vk_NumpadMultiply, XK_KP_Multiply);
    WYN_MAP_VK(wyn_vk_NumpadDivide,   XK_KP_Divide);
    WYN_MAP_VK(wyn_vk_NumpadDecimal,  XK_KP_Decimal);
    return &mapping;
}

// ================================================================================================================================
