#include "canvas_platform.h"
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <X11/Xlib.h>
#endif

typedef struct canvas_native_graphics {
#ifdef _WIN32
    HWND hwnd;
    HDC dc;
#elif defined(__linux__)
    Display *display;
    Drawable drawable;
    GC gc;
    unsigned long opaque_black;
#else
    int unused;
#endif
} canvas_native_graphics;

static mc_client_canvas_status failure(mc_client_canvas_error *out, mc_client_canvas_status status,
                                       const char *operation, unsigned long system_error) {
    if (out != NULL)
        *out = (mc_client_canvas_error){status, operation, system_error,
                                        status != MC_CLIENT_CANVAS_OK};
    return status;
}

mc_client_canvas_status mc_client_canvas_peer_graphics_acquire(SDL_Window *window, void **out,
                                                               mc_client_canvas_error *error) {
    const char *driver = SDL_GetCurrentVideoDriver();
    if (window == NULL || out == NULL || *out != NULL || driver == NULL || !SDL_IsMainThread())
        return failure(error, MC_CLIENT_CANVAS_ARGUMENT, "Canvas native Graphics acquisition", 0);
    canvas_native_graphics *g = calloc(1, sizeof(*g));
    if (g == NULL)
        return failure(error, MC_CLIENT_CANVAS_ALLOCATION, "Canvas native Graphics context", 0);
    const SDL_PropertiesID properties = SDL_GetWindowProperties(window);
#ifdef _WIN32
    if (strcmp(driver, "windows") == 0) {
        g->hwnd = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
        if (g->hwnd != NULL && IsWindow(g->hwnd))
            g->dc = GetDC(g->hwnd);
        if (g->dc == NULL) {
            const DWORD code = GetLastError();
            free(g);
            return failure(error, MC_CLIENT_CANVAS_NATIVE_PLATFORM, "Canvas real GDI context",
                           code);
        }
    } else
#elif defined(__linux__)
    if (strcmp(driver, "x11") == 0) {
        g->display = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
        g->drawable =
            (Drawable)SDL_GetNumberProperty(properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
        XWindowAttributes a = {0};
        if (g->display == NULL || g->drawable == 0 ||
            XGetWindowAttributes(g->display, (Window)g->drawable, &a) == 0 || a.visual == NULL) {
            free(g);
            return failure(error, MC_CLIENT_CANVAS_NATIVE_PLATFORM, "Canvas real X11 attributes",
                           0);
        }
        if ((a.visual->class != TrueColor && a.visual->class != DirectColor) || a.depth <= 0 ||
            a.depth >= 64) {
            const unsigned long visual_class = (unsigned long)a.visual->class;
            free(g);
            return failure(error, MC_CLIENT_CANVAS_NATIVE_PROFILE, "Canvas RGB X11 visual",
                           visual_class);
        }
        const uint64_t available = (UINT64_C(1) << a.depth) - 1;
        const unsigned long rgb = a.visual->red_mask | a.visual->green_mask | a.visual->blue_mask;
        g->opaque_black = (unsigned long)available & ~rgb;
        /* SDL creates and initializes DirectColor component ramps as well as
         * TrueColor peers. Observe the actual colormap before using its RGB0
         * pixel; never replace or mutate the peer's colormap here. */
        XColor black = {.pixel = g->opaque_black};
        if (!XQueryColor(g->display, a.colormap, &black) || black.red != 0 || black.green != 0 ||
            black.blue != 0) {
            free(g);
            return failure(error, MC_CLIENT_CANVAS_NATIVE_PROFILE, "Canvas actual X11 black pixel",
                           0);
        }
        g->gc = XCreateGC(g->display, g->drawable, 0, NULL);
        if (g->gc == NULL) {
            free(g);
            return failure(error, MC_CLIENT_CANVAS_NATIVE_PLATFORM,
                           "Canvas real X11 Graphics context", 0);
        }
    } else
#endif
    {
        free(g);
        return failure(error, MC_CLIENT_CANVAS_NATIVE_PROFILE, "Canvas native peer platform", 0);
    }
    *out = g;
    return failure(error, MC_CLIENT_CANVAS_OK, NULL, 0);
}

mc_client_canvas_status mc_client_canvas_peer_graphics_black(void *context, int32_t x, int32_t y,
                                                             int32_t width, int32_t height,
                                                             mc_client_canvas_error *error) {
    canvas_native_graphics *g = context;
    if (g == NULL || !SDL_IsMainThread())
        return failure(error, MC_CLIENT_CANVAS_ARGUMENT, "Canvas native Graphics fill", 0);
#ifdef _WIN32
    const RECT rect = {x, y, x + width, y + height};
    if (FillRect(g->dc, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH)) == 0)
        return failure(error, MC_CLIENT_CANVAS_NATIVE_PLATFORM, "Canvas real GDI black fill",
                       GetLastError());
#elif defined(__linux__)
    XSetForeground(g->display, g->gc, g->opaque_black);
    XFillRectangle(g->display, g->drawable, g->gc, x, y, (unsigned int)width, (unsigned int)height);
    XFlush(g->display); /* Asynchronous Graphics submission, not a pixel assertion. */
#else
    return failure(error, MC_CLIENT_CANVAS_NATIVE_PROFILE, "Canvas native Graphics platform", 0);
#endif
    return failure(error, MC_CLIENT_CANVAS_OK, NULL, 0);
}

mc_client_canvas_status mc_client_canvas_peer_graphics_release(void **slot,
                                                               mc_client_canvas_error *error) {
    if (slot == NULL || !SDL_IsMainThread())
        return failure(error, MC_CLIENT_CANVAS_ARGUMENT, "Canvas native Graphics release", 0);
    canvas_native_graphics *g = *slot;
    if (g == NULL)
        return failure(error, MC_CLIENT_CANVAS_OK, NULL, 0);
#ifdef _WIN32
    if (ReleaseDC(g->hwnd, g->dc) == 0)
        return failure(error, MC_CLIENT_CANVAS_NATIVE_PLATFORM, "Canvas real GDI release",
                       GetLastError());
#elif defined(__linux__)
    XFreeGC(g->display, g->gc);
#endif
    free(g);
    *slot = NULL;
    return failure(error, MC_CLIENT_CANVAS_OK, NULL, 0);
}
