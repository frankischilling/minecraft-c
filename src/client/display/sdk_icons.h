#ifndef MC_CLIENT_DISPLAY_SDK_ICONS_H
#define MC_CLIENT_DISPLAY_SDK_ICONS_H

#include "mc/resources/translation/translation.h"

typedef struct mc_client_sdk_icons mc_client_sdk_icons;
typedef struct mc_client_sdk_window_icons mc_client_sdk_window_icons;
typedef struct SDL_Window SDL_Window;

typedef enum mc_client_sdk_icons_status {
    MC_CLIENT_SDK_ICONS_OK,
    MC_CLIENT_SDK_ICONS_INVALID_ARGUMENT,
    MC_CLIENT_SDK_ICONS_INVALID_PHASE,
    MC_CLIENT_SDK_ICONS_BUSY,
    MC_CLIENT_SDK_ICONS_ALLOCATION_FAILED,
    MC_CLIENT_SDK_ICONS_LIMIT,
    MC_CLIENT_SDK_ICONS_RESOURCE_FAILED,
    MC_CLIENT_SDK_ICONS_WRONG_EXTENT,
    MC_CLIENT_SDK_ICONS_PLATFORM_FAILED,
    MC_CLIENT_SDK_ICONS_UNSUPPORTED_PLATFORM,
    MC_CLIENT_SDK_ICONS_UNSUPPORTED_FP
} mc_client_sdk_icons_status;

typedef enum mc_client_sdk_icons_phase {
    MC_CLIENT_SDK_ICONS_RAW,
    MC_CLIENT_SDK_ICONS_LOADING,
    MC_CLIENT_SDK_ICONS_READY,
    MC_CLIENT_SDK_ICONS_FAILED,
    MC_CLIENT_SDK_ICONS_RETIRING
} mc_client_sdk_icons_phase;

typedef struct mc_client_sdk_icons_error {
    mc_client_sdk_icons_status status;
    const char *operation;
    mc_registry_status open, close;
    mc_java_properties_status read;
    size_t bytes_read;
    int asset_edge;
    unsigned long platform_error;
    bool stream_open, large_installed, small_installed;
    const char *class_name, *method;
    int32_t pc;
} mc_client_sdk_icons_error;

typedef struct mc_client_sdk_icons_state {
    mc_client_sdk_icons_phase phase;
    size_t holds;
    bool active, stream_open;
    const uint8_t *rgba16, *rgba32;
    const mc_client_sdk_icons_error *failure;
} mc_client_sdk_icons_state;

/* Actual imported files only: pass the real resource_files source rooted at
 * the asset import root (not its jar/ child). Allocate is I/O-free. Construct
 * reads exactly1024/4096 bytes from /sdk/lwjgl-icon-{16,32}.rgba, checks EOF,
 * then closes each actual stream. It never opens a JAR/class/launcher/JVM.
 * The resource source/context remains borrowed through successful final
 * release, including every failed-close retry. Out/error/slots are disjoint.
 * Provider callbacks may inspect, but may not replace slots/dependencies. */
mc_client_sdk_icons_status mc_client_sdk_icons_allocate(const mc_registry_resources *,
                                                        mc_client_sdk_icons **,
                                                        mc_client_sdk_icons_error *);
mc_client_sdk_icons_status mc_client_sdk_icons_construct(mc_client_sdk_icons *,
                                                         mc_client_sdk_icons_error *);
mc_client_sdk_icons_status mc_client_sdk_icons_retain(mc_client_sdk_icons *,
                                                      mc_client_sdk_icons_error *);
/* Success consumes/nulls exactly this occurrence. A last occurrence retires
 * under its active guard; pending native close failure preserves it for retry.
 * RETIRING cannot gain new holds. BUSY never consumes or frees an occurrence. */
mc_client_sdk_icons_status mc_client_sdk_icons_release(mc_client_sdk_icons **,
                                                       mc_client_sdk_icons_error *);
bool mc_client_sdk_icons_inspect(const mc_client_sdk_icons *, mc_client_sdk_icons_state *);

/* Real per-window native icon handles borrow the live window and retain the
 * same imported pixel owner. Allocate publishes a candidate before install;
 * partial installation stays typed and owned until explicit retirement.
 * Windows installs actual32 big then16 small HICONs. The pinned Linux body
 * uses the first valid default32 buffer, real color/mask Pixmaps and WM_HINTS.
 * No single SDL-rescaled icon is credited as both platform effects. */
mc_client_sdk_icons_status mc_client_sdk_window_icons_allocate(SDL_Window *, mc_client_sdk_icons *,
                                                               mc_client_sdk_window_icons **,
                                                               mc_client_sdk_icons_error *);
mc_client_sdk_icons_status mc_client_sdk_window_icons_install(mc_client_sdk_window_icons *,
                                                              mc_client_sdk_icons_error *);
mc_client_sdk_icons_status mc_client_sdk_window_icons_retire(mc_client_sdk_window_icons **,
                                                             mc_client_sdk_icons_error *);

#endif
