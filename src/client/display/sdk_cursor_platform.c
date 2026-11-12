#include "sdk_cursor_internal.h"

#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <X11/Xlib.h>
#endif

bool mc_client_sdk_cursor_platform_query(SDL_Window *window, mc_client_sdk_cursor_platform *out,
                                         mc_client_sdk_cursor_error *error) {
    const char *driver = SDL_GetCurrentVideoDriver();
    if (window == NULL || driver == NULL || SDL_GetWindowID(window) == 0)
        return false;
    const SDL_PropertiesID properties = SDL_GetWindowProperties(window);
#ifdef _WIN32
    if (strcmp(driver, "windows") == 0) {
        HWND hwnd = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
        if (hwnd == NULL || !IsWindow(hwnd)) {
            error->platform_error = GetLastError();
            return false;
        }
        const int width = GetSystemMetrics(SM_CXCURSOR), height = GetSystemMetrics(SM_CYCURSOR);
        if (width <= 0 || height <= 0)
            return false;
        /* Actual WindowsDisplay.getNativeCursorCapabilities returns1. The
         * current real HWND/system metrics establish this native attachment;
         * this does not fabricate source Mouse/Keyboard created state. */
        *out = (mc_client_sdk_cursor_platform){3, true, (unsigned)width, (unsigned)height};
        return true;
    }
#elif defined(__linux__)
    if (strcmp(driver, "x11") == 0) {
        Display *display =
            SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
        const Sint64 native_window =
            SDL_GetNumberProperty(properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
        unsigned width = 0, height = 0;
        if (display == NULL || native_window == 0 ||
            !XQueryBestCursor(display, (Drawable)native_window, 32, 32, &width, &height))
            return false;
        /* Only the ONE_BIT predicate consumed by Cursor.<init> is represented.
         * The real core X11 cursor query supplies native bounds, not a cached
         * warm flag or a claim about alpha/animation capability bits. */
        *out = (mc_client_sdk_cursor_platform){1, width != 0 && height != 0, width, height};
        return true;
    }
#endif
    (void)properties;
    error->status = MC_CLIENT_SDK_CURSOR_UNSUPPORTED_PROFILE;
    return false;
}
