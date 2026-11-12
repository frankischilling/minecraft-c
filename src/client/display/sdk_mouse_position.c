/* Cursor positioning shares the existing class-closed native Input owner. */
#include "mc/core/java/java.h"
#include "sdk_input_internal.h"
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_properties.h>
#include <xmmintrin.h>
#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <X11/Xlib.h>
#endif

bool mc_client_sdk_native_cursor_capabilities(mc_client_sdk_input *, int32_t *,
                                              mc_client_sdk_input_error *);

bool mc_client_sdk_mouse_native_extent(mc_client_sdk_input *o, int32_t *width, int32_t *height,
                                       mc_client_sdk_input_error *e, const char *method,
                                       const char *descriptor, int32_t pc) {
    const SDL_PropertiesID properties = SDL_GetWindowProperties(o->window);
#ifdef _WIN32
    HWND window = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    RECT bounds;
    if (window != NULL && GetClientRect(window, &bounds)) {
        const int64_t h = (int64_t)bounds.bottom - bounds.top;
        const int64_t w = (int64_t)bounds.right - bounds.left;
        if (h >= 0 && h <= INT32_MAX && w >= 0 && w <= INT32_MAX) {
            if (width != NULL)
                *width = (int32_t)w;
            *height = (int32_t)h;
            return true;
        }
    }
#elif defined(__linux__)
    Display *display =
        SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
    Window window = (Window)SDL_GetNumberProperty(properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    XWindowAttributes bounds;
    if (display != NULL && window != 0 && XGetWindowAttributes(display, window, &bounds)) {
        if (width != NULL)
            *width = bounds.width;
        *height = bounds.height;
        return true;
    }
#endif
    mc_client_sdk_input_fail(e, MC_CLIENT_SDK_INPUT_PLATFORM_FAILED,
                             "Mouse actual native coordinate extent", "org/lwjgl/input/Mouse",
                             method, descriptor, pc, true);
    return false;
}

mc_client_sdk_input_status mc_client_sdk_mouse_set_cursor_position(mc_client_sdk_input *o,
                                                                   int32_t x, int32_t y,
                                                                   mc_client_sdk_input_error *out) {
    mc_client_sdk_input_error e = {0};
    if (!mc_client_sdk_input_enter(o, "org/lwjgl/input/Mouse", "setCursorPosition", "(II)V", &e)) {
        if (out != NULL)
            *out = e;
        return e.status;
    }
    if (!o->mouse_created) {
        mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_SOURCE_ILLEGAL_STATE,
                                 "actual uncreated Mouse.setCursorPosition",
                                 "org/lwjgl/input/Mouse", "setCursorPosition", "(II)V", 12, false);
        return mc_client_sdk_input_leave(o, &e, out);
    }
    o->current_mouse.x = x;
    o->last_mouse_store = 24;
    o->x = x;
    o->last_mouse_store = 27;
    o->current_mouse.y = y;
    o->last_mouse_store = 32;
    o->y = y;
    o->last_mouse_store = 35;
    bool warp = false;
    if (!o->grabbed) {
        int32_t capabilities;
        if (!mc_client_sdk_native_cursor_capabilities(o, &capabilities, &e)) {
            mc_client_sdk_input_caller(&e, "org/lwjgl/input/Mouse", "setCursorPosition", "(II)V",
                                       44);
            return mc_client_sdk_input_leave(o, &e, out);
        }
        warp = (capabilities & 1) != 0;
    }
    if (!warp) {
        o->grab_x = x;
        o->last_mouse_store = 70;
        o->grab_y = y;
        o->last_mouse_store = 74;
    } else {
        /* LinuxMouse.transformY reads the actual native height at each warp.
         * No Mouse poll or source dimension store precedes this conversion. */
        int32_t native_height;
        if (!mc_client_sdk_mouse_native_extent(o, NULL, &native_height, &e, "setCursorPosition",
                                               "(II)V", 61)) {
            return mc_client_sdk_input_leave(o, &e, out);
        }
        if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
            mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_UNSUPPORTED_FP,
                                     "Mouse reached native warp coordinate conversion",
                                     "org/lwjgl/input/Mouse", "setCursorPosition", "(II)V", 61,
                                     true);
            return mc_client_sdk_input_leave(o, &e, out);
        }
        SDL_WarpMouseInWindow(o->window, (float)x,
                              (float)mc_java_i32_sub(mc_java_i32_sub(native_height, 1), y));
    }
    /* No poll/reset/event drain belongs to this source method. In particular,
     * its x/y stores survive a later native failure, and deltas stay untouched. */
    return mc_client_sdk_input_leave(o, &e, out);
}
