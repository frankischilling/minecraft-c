#include "sdk_platform.h"

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#endif

mc_client_sdk_platform_status mc_client_sdk_platform_query(SDL_Window *window,
                                                           mc_client_sdk_platform *out) {
    if (window == NULL || out == NULL || !SDL_IsMainThread() ||
        (SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0 || SDL_GetWindowID(window) == 0)
        return MC_CLIENT_SDK_PLATFORM_NOT_INITIALIZED;
    mc_client_sdk_platform value = {0};
    value.physical_keyboard_present = SDL_HasKeyboard();
    value.physical_mouse_present = SDL_HasMouse();
    const char *driver = SDL_GetCurrentVideoDriver();
    if (driver == NULL)
        return MC_CLIENT_SDK_PLATFORM_NOT_INITIALIZED;
#ifdef _WIN32
    if (strcmp(driver, "windows") != 0)
        return MC_CLIENT_SDK_PLATFORM_UNSUPPORTED;
    const SDL_PropertiesID props = SDL_GetWindowProperties(window);
    if (props == 0 ||
        SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL) == NULL)
        return MC_CLIENT_SDK_PLATFORM_FAILED;
    value.lwjgl_platform = 3;
    const int count = GetSystemMetrics(SM_CMOUSEBUTTONS);
    value.button_count = count < 5 ? count : 5;
    value.has_wheel = GetSystemMetrics(SM_MOUSEWHEELPRESENT) != 0;
#elif defined(__linux__)
    if (strcmp(driver, "x11") != 0)
        return MC_CLIENT_SDK_PLATFORM_UNSUPPORTED;
    const SDL_PropertiesID props = SDL_GetWindowProperties(window);
    Display *display =
        props == 0 ? NULL
                   : SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
    if (display == NULL)
        return MC_CLIENT_SDK_PLATFORM_FAILED;
    unsigned char mapping[256];
    const int count = XGetPointerMapping(display, mapping, (int)sizeof(mapping));
    if (count < 0 || count > (int)sizeof(mapping))
        return MC_CLIENT_SDK_PLATFORM_FAILED;
    value.lwjgl_platform = 1;
    value.button_count = count;
    value.has_wheel = true;
#else
    return MC_CLIENT_SDK_PLATFORM_UNSUPPORTED;
#endif
    if (value.button_count < 0)
        return MC_CLIENT_SDK_PLATFORM_FAILED;
    *out = value;
    return MC_CLIENT_SDK_PLATFORM_OK;
}

mc_client_sdk_platform_status
mc_client_sdk_platform_initial_mode(mc_client_sdk_desktop_mode *out,
                                    mc_client_sdk_platform_error *error) {
    mc_client_sdk_platform_error e = {0};
    mc_client_sdk_desktop_mode value = {0};
    e.operation = "actual primary desktop mode";
    if (out == NULL || !SDL_IsMainThread() || (SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0) {
        e.status = MC_CLIENT_SDK_PLATFORM_NOT_INITIALIZED;
        goto done;
    }
    const char *driver = SDL_GetCurrentVideoDriver();
#ifdef _WIN32
    if (driver == NULL || strcmp(driver, "windows") != 0) {
        e.status = MC_CLIENT_SDK_PLATFORM_UNSUPPORTED;
        goto done;
    }
    DEVMODEW mode = {0};
    mode.dmSize = (WORD)sizeof(mode);
    if (!EnumDisplaySettingsW(NULL, ENUM_CURRENT_SETTINGS, &mode)) {
        e.status = MC_CLIENT_SDK_PLATFORM_FAILED;
        e.native_error = GetLastError();
        goto done;
    }
    /* Exact jint bit values from the native DWORD fields, without a signed
     * narrowing/overflow assumption or a guessed pixel-format replacement. */
    const uint32_t fields[] = {mode.dmPelsWidth, mode.dmPelsHeight, mode.dmBitsPerPel,
                               mode.dmDisplayFrequency};
    memcpy(&value.width, &fields[0], sizeof(value.width));
    memcpy(&value.height, &fields[1], sizeof(value.height));
    memcpy(&value.bits_per_pixel, &fields[2], sizeof(value.bits_per_pixel));
    memcpy(&value.frequency, &fields[3], sizeof(value.frequency));
#elif defined(__linux__)
    if (driver == NULL || strcmp(driver, "x11") != 0) {
        e.status = MC_CLIENT_SDK_PLATFORM_UNSUPPORTED;
        goto done;
    }
    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        e.status = MC_CLIENT_SDK_PLATFORM_FAILED;
        goto done;
    }
    const int screen = DefaultScreen(display);
    XRRScreenConfiguration *configuration = XRRGetScreenInfo(display, RootWindow(display, screen));
    if (configuration == NULL)
        e.status = MC_CLIENT_SDK_PLATFORM_UNSUPPORTED;
    else {
        Rotation rotation;
        const SizeID selected = XRRConfigCurrentConfiguration(configuration, &rotation);
        int count = 0;
        const XRRScreenSize *sizes = XRRConfigSizes(configuration, &count);
        if (sizes == NULL || selected >= count)
            e.status = MC_CLIENT_SDK_PLATFORM_FAILED;
        else {
            value.width = sizes[selected].width;
            value.height = sizes[selected].height;
            value.bits_per_pixel = XDefaultDepth(display, screen);
            value.frequency = XRRConfigCurrentRate(configuration);
        }
        XRRFreeScreenConfigInfo(configuration);
    }
    (void)XCloseDisplay(display);
    if (e.status != MC_CLIENT_SDK_PLATFORM_OK)
        goto done;
#else
    (void)driver;
    e.status = MC_CLIENT_SDK_PLATFORM_UNSUPPORTED;
    goto done;
#endif
    *out = value;
done:
    if (error != NULL)
        *error = e;
    return e.status;
}
