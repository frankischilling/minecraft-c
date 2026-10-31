#include "sdk_icons_internal.h"

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

#ifdef _WIN32
#include <windows.h>

typedef struct native_icon {
    HICON handle;
    HBITMAP color, mask;
    HDC dc;
} native_icon;
#elif defined(__linux__)
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

struct mc_client_sdk_window_icons {
    SDL_Window *window;
    mc_client_sdk_icons *pixels;
    bool active, attempted, retiring, large_installed, small_installed;
    mc_client_sdk_icons_error failure;
#ifdef _WIN32
    HWND hwnd;
    native_icon large, small_icon;
    HICON previous_large, previous_small;
#elif defined(__linux__)
    Display *xdisplay;
    Window xwindow;
    Pixmap color, mask;
    XWMHints *previous_hints;
    bool previous_observed;
    XImage *image;
    GC gc;
#endif
};

static bool platform_failure(mc_client_sdk_window_icons *o, mc_client_sdk_icons_error *e,
                             const char *operation) {
    mc_client_sdk_icons_fail(e, MC_CLIENT_SDK_ICONS_PLATFORM_FAILED, operation);
#ifdef _WIN32
    e->platform_error = GetLastError();
#endif
    e->large_installed = o->large_installed;
    e->small_installed = o->small_installed;
    return false;
}

mc_client_sdk_icons_status mc_client_sdk_window_icons_allocate(SDL_Window *window,
                                                               mc_client_sdk_icons *pixels,
                                                               mc_client_sdk_window_icons **out,
                                                               mc_client_sdk_icons_error *error) {
    mc_client_sdk_icons_error e = {0};
    if (window == NULL || out == NULL || !SDL_IsMainThread() || SDL_GetWindowID(window) == 0)
        mc_client_sdk_icons_fail(&e, MC_CLIENT_SDK_ICONS_INVALID_ARGUMENT,
                                 "native window-icon candidate");
    else if (mc_client_sdk_icons_retain(pixels, &e) == MC_CLIENT_SDK_ICONS_OK) {
        mc_client_sdk_window_icons *o = calloc(1, sizeof(*o));
        if (o == NULL) {
            mc_client_sdk_icons_error cleanup = {0};
            (void)mc_client_sdk_icons_release(&pixels, &cleanup); /* nonfinal admitted occurrence */
            mc_client_sdk_icons_fail(&e, MC_CLIENT_SDK_ICONS_ALLOCATION_FAILED,
                                     "native window-icon owner");
        } else {
            o->window = window;
            o->pixels = pixels;
            *out = o;
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

#ifdef _WIN32
static bool make_icon(mc_client_sdk_window_icons *o, native_icon *icon, const uint8_t *rgba,
                      int edge, mc_client_sdk_icons_error *e) {
    BITMAPV5HEADER format = {0};
    format.bV5Size = (DWORD)sizeof(format);
    format.bV5Width = edge;
    format.bV5Height = -edge;
    format.bV5Planes = 1;
    format.bV5BitCount = 32;
    format.bV5Compression = BI_BITFIELDS;
    format.bV5RedMask = 0x00ff0000u;
    format.bV5GreenMask = 0x0000ff00u;
    format.bV5BlueMask = 0x000000ffu;
    format.bV5AlphaMask = 0xff000000u;
    icon->dc = GetDC(NULL);
    if (icon->dc == NULL)
        return platform_failure(o, e, "native icon screen DC");
    void *storage = NULL;
    icon->color =
        CreateDIBSection(icon->dc, (const BITMAPINFO *)&format, DIB_RGB_COLORS, &storage, NULL, 0);
    if (icon->color == NULL || storage == NULL)
        return platform_failure(o, e, "native icon top-down BGRA bitmap");
    if (ReleaseDC(NULL, icon->dc) == 0)
        return platform_failure(o, e, "release native icon DC");
    icon->dc = NULL;
    uint8_t *bgra = storage;
    for (size_t i = 0; i < (size_t)edge * (size_t)edge; ++i) {
        bgra[4 * i] = rgba[4 * i + 2];
        bgra[4 * i + 1] = rgba[4 * i + 1];
        bgra[4 * i + 2] = rgba[4 * i];
        bgra[4 * i + 3] = rgba[4 * i + 3];
    }
    /* The actual Windows backend uses an all-one WORD-aligned AND mask with
     * its alpha-channel bitmap. Both admitted dimensions are multiples of16. */
    uint8_t mask[32 * 32 / 8];
    memset(mask, 0xff, sizeof(mask));
    icon->mask = CreateBitmap(edge, edge, 1, 1, mask);
    if (icon->mask == NULL)
        return platform_failure(o, e, "native icon AND bitmap");
    ICONINFO info = {0};
    info.fIcon = TRUE;
    info.hbmColor = icon->color;
    info.hbmMask = icon->mask;
    icon->handle = CreateIconIndirect(&info);
    if (icon->handle == NULL)
        return platform_failure(o, e, "native actual HICON");
    if (!DeleteObject(icon->color))
        return platform_failure(o, e, "release native icon color temporary");
    icon->color = NULL;
    if (!DeleteObject(icon->mask))
        return platform_failure(o, e, "release native icon mask temporary");
    icon->mask = NULL;
    return true;
}

static bool cleanup_icon(mc_client_sdk_window_icons *o, native_icon *icon,
                         mc_client_sdk_icons_error *e) {
    if (icon->handle != NULL) {
        if (!DestroyIcon(icon->handle))
            return platform_failure(o, e, "retire native HICON");
        icon->handle = NULL;
    }
    if (icon->color != NULL) {
        if (!DeleteObject(icon->color))
            return platform_failure(o, e, "retire native color bitmap");
        icon->color = NULL;
    }
    if (icon->mask != NULL) {
        if (!DeleteObject(icon->mask))
            return platform_failure(o, e, "retire native mask bitmap");
        icon->mask = NULL;
    }
    if (icon->dc != NULL) {
        if (ReleaseDC(NULL, icon->dc) == 0)
            return platform_failure(o, e, "retire native icon DC");
        icon->dc = NULL;
    }
    return true;
}
#elif defined(__linux__)
static bool make_pixmap(mc_client_sdk_window_icons *o, bool mask, mc_client_sdk_icons_error *e) {
    XWindowAttributes attributes;
    if (!XGetWindowAttributes(o->xdisplay, o->xwindow, &attributes))
        return platform_failure(o, e, "native icon window visual");
    /* Matches the pinned x86 little-endian SDK's BGR0 / LSB alpha-bit buffers;
     * a different visual must not be reported as the same converted image. */
    if (ImageByteOrder(o->xdisplay) != LSBFirst || BitmapBitOrder(o->xdisplay) != LSBFirst ||
        attributes.visual->red_mask != 0xff0000ul || attributes.visual->green_mask != 0xff00ul ||
        attributes.visual->blue_mask != 0xfful || attributes.depth != 24) {
        mc_client_sdk_icons_fail(e, MC_CLIENT_SDK_ICONS_UNSUPPORTED_PLATFORM,
                                 "native icon visual/byte-order profile");
        return false;
    }
    const size_t bytes = mask ? 32u * 32u / 8u : 32u * 32u * 4u;
    char *copy = calloc(bytes, 1);
    if (copy == NULL) {
        mc_client_sdk_icons_fail(e, MC_CLIENT_SDK_ICONS_ALLOCATION_FAILED,
                                 "native icon image storage");
        return false;
    }
    const uint8_t *rgba = o->pixels->rgba32;
    for (size_t i = 0; i < 32u * 32u; ++i)
        if (mask) {
            if (rgba[4 * i + 3] >= 127)
                copy[i / 8] = (char)((uint8_t)copy[i / 8] | (uint8_t)(1u << (i % 8)));
        } else {
            copy[4 * i] = (char)rgba[4 * i + 2];
            copy[4 * i + 1] = (char)rgba[4 * i + 1];
            copy[4 * i + 2] = (char)rgba[4 * i];
        }
    const unsigned depth = mask ? 1u : (unsigned)attributes.depth;
    Pixmap *pixmap = mask ? &o->mask : &o->color;
    *pixmap = XCreatePixmap(o->xdisplay, o->xwindow, 32, 32, depth);
    if (*pixmap == None) {
        free(copy);
        return platform_failure(o, e, "native icon Pixmap");
    }
    o->image = XCreateImage(o->xdisplay, attributes.visual, depth, mask ? XYPixmap : ZPixmap, 0,
                            copy, 32, 32, 32, 0);
    if (o->image == NULL) {
        free(copy);
        return platform_failure(o, e, "native icon XImage");
    }
    o->gc = XCreateGC(o->xdisplay, *pixmap, 0, NULL);
    if (o->gc == NULL)
        return platform_failure(o, e, "native icon GC");
    (void)XPutImage(o->xdisplay, *pixmap, o->gc, o->image, 0, 0, 0, 0, 32, 32);
    (void)XFreeGC(o->xdisplay, o->gc);
    o->gc = NULL;
    (void)XDestroyImage(o->image); /* consumes the actual copied bytes */
    o->image = NULL;
    return true;
}
#endif

mc_client_sdk_icons_status mc_client_sdk_window_icons_install(mc_client_sdk_window_icons *o,
                                                              mc_client_sdk_icons_error *error) {
    mc_client_sdk_icons_error e = {0};
    if (o == NULL || !SDL_IsMainThread())
        mc_client_sdk_icons_fail(&e, MC_CLIENT_SDK_ICONS_INVALID_ARGUMENT,
                                 "native icon install owner/thread");
    else if (o->active || o->attempted || o->retiring)
        mc_client_sdk_icons_fail(
            &e, o->active ? MC_CLIENT_SDK_ICONS_BUSY : MC_CLIENT_SDK_ICONS_INVALID_PHASE,
            "native icon once-only install");
    else {
        o->active = true;
        o->attempted = true;
        const SDL_PropertiesID props = SDL_GetWindowProperties(o->window);
#ifdef _WIN32
        o->hwnd = props == 0
                      ? NULL
                      : SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
        e.class_name = "org/lwjgl/opengl/WindowsDisplay";
        e.method = "setIcon";
        e.pc = 110; /* reached i2d before big32 Math.sqrt111 */
        if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
            mc_client_sdk_icons_fail(&e, MC_CLIENT_SDK_ICONS_UNSUPPORTED_FP,
                                     "reached native big-icon dimension arithmetic");
        else if (o->hwnd == NULL)
            platform_failure(o, &e, "native icon HWND");
        else {
            e.pc = 133;
            if (make_icon(o, &o->large, o->pixels->rgba32, 32, &e)) {
                o->previous_large =
                    (HICON)SendMessageW(o->hwnd, WM_SETICON, ICON_BIG, (LPARAM)o->large.handle);
                o->large_installed = true;
                e.pc = 51; /* second array element, reached i2d before small16 sqrt52 */
                if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
                    mc_client_sdk_icons_fail(&e, MC_CLIENT_SDK_ICONS_UNSUPPORTED_FP,
                                             "reached native small-icon dimension arithmetic");
                else {
                    e.pc = 74;
                    if (make_icon(o, &o->small_icon, o->pixels->rgba16, 16, &e)) {
                        o->previous_small = (HICON)SendMessageW(o->hwnd, WM_SETICON, ICON_SMALL,
                                                                (LPARAM)o->small_icon.handle);
                        o->small_installed = true;
                    }
                }
            }
        }
#elif defined(__linux__)
        o->xdisplay =
            props == 0 ? NULL
                       : SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
        o->xwindow =
            props == 0 ? 0
                       : (Window)SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
        e.class_name = "org/lwjgl/opengl/LinuxDisplay";
        e.method = "setIcon";
        e.pc = 37;
        if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
            mc_client_sdk_icons_fail(&e, MC_CLIENT_SDK_ICONS_UNSUPPORTED_FP,
                                     "reached native icon dimension arithmetic");
        else if (o->xdisplay == NULL || o->xwindow == None)
            platform_failure(o, &e, "native icon X11 attachment");
        else {
            e.pc = 95;
            if (make_pixmap(o, false, &e) && make_pixmap(o, true, &e)) {
                o->previous_hints = XGetWMHints(o->xdisplay, o->xwindow);
                o->previous_observed = true;
                XWMHints hints = {0};
                hints.flags = InputHint | IconPixmapHint | IconMaskHint;
                hints.input = True;
                hints.icon_pixmap = o->color;
                hints.icon_mask = o->mask;
                if (!XSetWMHints(o->xdisplay, o->xwindow, &hints))
                    platform_failure(o, &e, "native icon WM_HINTS");
                else {
                    o->large_installed = true;
                    (void)XFlush(o->xdisplay);
                }
            }
        }
#else
        (void)props;
        mc_client_sdk_icons_fail(&e, MC_CLIENT_SDK_ICONS_UNSUPPORTED_PLATFORM,
                                 "native SDK icon platform");
#endif
        e.large_installed = o->large_installed;
        e.small_installed = o->small_installed;
        if (e.status != MC_CLIENT_SDK_ICONS_OK)
            o->failure = e;
        o->active = false;
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_client_sdk_icons_status mc_client_sdk_window_icons_retire(mc_client_sdk_window_icons **slot,
                                                             mc_client_sdk_icons_error *error) {
    mc_client_sdk_icons_error e = {0};
    if (slot == NULL || !SDL_IsMainThread())
        mc_client_sdk_icons_fail(&e, MC_CLIENT_SDK_ICONS_INVALID_ARGUMENT,
                                 "native icon retire slot/thread");
    else if (*slot != NULL) {
        mc_client_sdk_window_icons *o = *slot;
        if (o->active)
            mc_client_sdk_icons_fail(&e, MC_CLIENT_SDK_ICONS_BUSY, "native active icon retirement");
        else {
            o->active = true;
            o->retiring = true;
#ifdef _WIN32
            if (o->large_installed) {
                (void)SendMessageW(o->hwnd, WM_SETICON, ICON_BIG, (LPARAM)o->previous_large);
                o->large_installed = false;
            }
            if (o->small_installed) {
                (void)SendMessageW(o->hwnd, WM_SETICON, ICON_SMALL, (LPARAM)o->previous_small);
                o->small_installed = false;
            }
            if (!cleanup_icon(o, &o->large, &e) || !cleanup_icon(o, &o->small_icon, &e))
                goto retained;
#elif defined(__linux__)
            if (o->large_installed) {
                if (o->previous_hints != NULL) {
                    if (!XSetWMHints(o->xdisplay, o->xwindow, o->previous_hints)) {
                        platform_failure(o, &e,
                                         "restore native window hints before Pixmap retirement");
                        goto retained;
                    }
                } else if (o->previous_observed)
                    (void)XDeleteProperty(o->xdisplay, o->xwindow, XA_WM_HINTS);
                o->large_installed = false;
            }
            if (o->gc != NULL) {
                (void)XFreeGC(o->xdisplay, o->gc);
                o->gc = NULL;
            }
            if (o->image != NULL) {
                (void)XDestroyImage(o->image);
                o->image = NULL;
            }
            if (o->color != None) {
                (void)XFreePixmap(o->xdisplay, o->color);
                o->color = None;
            }
            if (o->mask != None) {
                (void)XFreePixmap(o->xdisplay, o->mask);
                o->mask = None;
            }
            if (o->previous_hints != NULL) {
                (void)XFree(o->previous_hints);
                o->previous_hints = NULL;
            }
            if (o->xdisplay != NULL)
                (void)XFlush(o->xdisplay);
#endif
            if (mc_client_sdk_icons_release(&o->pixels, &e) != MC_CLIENT_SDK_ICONS_OK)
                goto retained;
            free(o);
            *slot = NULL;
            goto done;
        retained:
            o->failure = e;
            o->active = false;
        }
    }
done:
    if (error != NULL)
        *error = e;
    return e.status;
}
