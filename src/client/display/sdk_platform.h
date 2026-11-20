#ifndef MC_CLIENT_DISPLAY_SDK_PLATFORM_H
#define MC_CLIENT_DISPLAY_SDK_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

typedef struct SDL_Window SDL_Window;

typedef enum mc_client_sdk_platform_status {
    MC_CLIENT_SDK_PLATFORM_OK,
    MC_CLIENT_SDK_PLATFORM_NOT_INITIALIZED,
    MC_CLIENT_SDK_PLATFORM_UNSUPPORTED,
    MC_CLIENT_SDK_PLATFORM_FAILED
} mc_client_sdk_platform_status;

typedef struct mc_client_sdk_platform {
    int32_t lwjgl_platform, button_count;
    bool has_wheel, physical_keyboard_present, physical_mouse_present;
} mc_client_sdk_platform;

typedef struct mc_client_sdk_desktop_mode {
    int32_t width, height, bits_per_pixel, frequency;
} mc_client_sdk_desktop_mode;

typedef struct mc_client_sdk_platform_error {
    mc_client_sdk_platform_status status;
    const char *operation;
    unsigned long native_error;
} mc_client_sdk_platform_error;

/* Uses actual linked/initialized SDL video, window and native platform handles.
 * Windows button count is min(5, GetSystemMetrics43), wheel metric75, matching
 * pinned WindowsMouse Code35/38/47. X11 uses actual XGetPointerMapping, matching
 * LinuxMouse's native count query; the concrete Linux hasWheel body is true.
 * No guessed32-bit-mask capacity is reported as a physical button count.
 * Other native window systems are explicit unsupported capability profiles. */
mc_client_sdk_platform_status mc_client_sdk_platform_query(SDL_Window *, mc_client_sdk_platform *);
/* Real primary desktop facts before any window exists. Windows uses actual
 * current DEVMODE fields (including32 storage bpp, not SDL RGB color bits).
 * X11 uses real current XRandR screen size/rate and XDefaultDepth, matching
 * the admitted original XRANDR path. The temporary query connection is owned
 * and closed here; it is not a fabricated warm device or source field. */
mc_client_sdk_platform_status mc_client_sdk_platform_initial_mode(mc_client_sdk_desktop_mode *,
                                                                  mc_client_sdk_platform_error *);

#endif
