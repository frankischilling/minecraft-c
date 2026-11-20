#include "frame_platform.h"
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <X11/Xlib.h>
#endif
static bool failed(mc_client_frame_error *e, const char *operation, unsigned long code) {
    e->status = MC_CLIENT_CANVAS_NATIVE_PLATFORM;
    e->operation = operation;
    e->system_error = code;
    return false;
}

bool mc_client_frame_native_embed(SDL_Window *frame, SDL_Window *canvas, mc_client_frame_error *e) {
    const char *driver = SDL_GetCurrentVideoDriver();
    const SDL_PropertiesID f = SDL_GetWindowProperties(frame), c = SDL_GetWindowProperties(canvas);
#ifdef _WIN32
    if (driver != NULL && strcmp(driver, "windows") == 0) {
        HWND parent = SDL_GetPointerProperty(f, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
        HWND child = SDL_GetPointerProperty(c, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
        if (parent == NULL || child == NULL || parent == child)
            return failed(e, "Frame/Canvas distinct HWNDs", GetLastError());
        SetLastError(0);
        LONG_PTR style = GetWindowLongPtrW(child, GWL_STYLE);
        if (style == 0 && GetLastError() != 0)
            return failed(e, "Canvas original window style", GetLastError());
        style &=
            ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
        style |= WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
        SetLastError(0);
        if (SetWindowLongPtrW(child, GWL_STYLE, style) == 0 && GetLastError() != 0)
            return failed(e, "Canvas child style", GetLastError());
        SetLastError(0);
        if (SetParent(child, parent) == NULL && GetLastError() != 0)
            return failed(e, "Canvas real SetParent", GetLastError());
        if (GetParent(child) != parent ||
            !SetWindowPos(child, NULL, 0, 0, 0, 0,
                          SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED))
            return failed(e, "Canvas real child parent/layout", GetLastError());
        return true;
    }
#elif defined(__linux__)
    if (driver != NULL && strcmp(driver, "x11") == 0) {
        Display *display = SDL_GetPointerProperty(f, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
        Display *other = SDL_GetPointerProperty(c, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
        Window parent = (Window)SDL_GetNumberProperty(f, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
        Window child = (Window)SDL_GetNumberProperty(c, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
        if (display == NULL || display != other || parent == 0 || child == 0 || parent == child)
            return failed(e, "Frame/Canvas distinct same-Display X11 windows", 0);
        XReparentWindow(display, child, parent, 0, 0);
        /* Query actual server state, rather than equating submission with
         * embedding success. No process-global X error-handler replacement. */
        Window root, actual_parent, *children = NULL;
        unsigned int count = 0;
        const Status queried = XQueryTree(display, child, &root, &actual_parent, &children, &count);
        if (children != NULL)
            XFree(children);
        if (!queried || actual_parent != parent)
            return failed(e, "Canvas actual X11 parent", 0);
        return true;
    }
#endif
    return failed(e, "Frame embedding requires actual Win32/X11 peers", 0);
}

bool mc_client_frame_native_focus(SDL_Window *canvas, mc_client_frame_error *e) {
#ifdef _WIN32
    HWND child = SDL_GetPointerProperty(SDL_GetWindowProperties(canvas),
                                        SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    if (child == NULL)
        return failed(e, "Canvas focus real HWND", 0);
    SetFocus(child);
    return GetFocus() == child || failed(e, "Canvas native focus", GetLastError());
#elif defined(__linux__)
    const SDL_PropertiesID p = SDL_GetWindowProperties(canvas);
    Display *d = SDL_GetPointerProperty(p, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
    Window w = (Window)SDL_GetNumberProperty(p, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    XWindowAttributes a;
    if (d == NULL || w == 0 || !XGetWindowAttributes(d, w, &a) || a.map_state != IsViewable)
        return failed(e, "Canvas focus actual viewable X11 peer", 0);
    XSetInputFocus(d, w, RevertToParent, CurrentTime);
    XFlush(d);
    return true; /* Request submission; window manager focus timing is native. */
#else
    return failed(e, "Canvas focus platform", 0);
#endif
}
