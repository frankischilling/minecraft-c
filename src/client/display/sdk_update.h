#ifndef MC_CLIENT_DISPLAY_SDK_UPDATE_H
#define MC_CLIENT_DISPLAY_SDK_UPDATE_H

#include "display.h"
#include "events.h"
#include "sdk_input.h"

typedef struct mc_client_sdk_update mc_client_sdk_update;

typedef enum mc_client_sdk_update_status {
    MC_CLIENT_SDK_UPDATE_OK,
    MC_CLIENT_SDK_UPDATE_INVALID_ARGUMENT,
    MC_CLIENT_SDK_UPDATE_INVALID_PHASE,
    MC_CLIENT_SDK_UPDATE_BUSY,
    MC_CLIENT_SDK_UPDATE_ALLOCATION_FAILED,
    MC_CLIENT_SDK_UPDATE_SOURCE_ILLEGAL_STATE,
    MC_CLIENT_SDK_UPDATE_PLATFORM_FAILED,
    MC_CLIENT_SDK_UPDATE_INPUT_FAILED,
    MC_CLIENT_SDK_UPDATE_UNSUPPORTED_PROFILE
} mc_client_sdk_update_status;

typedef struct mc_client_sdk_update_error {
    mc_client_sdk_update_status status;
    const char *operation;
    mc_client_sdk_input_site site, callers[3];
    size_t caller_count;
    mc_client_display_error display;
    mc_client_events_error events;
    mc_client_events_error dispatch;
    mc_client_sdk_input_error input;
    bool swapped, messages_pumped, width_stored, height_stored;
} mc_client_sdk_update_error;

typedef struct mc_client_sdk_update_state {
    bool active, window_resized, failed;
    int32_t width, height, last_store_pc;
    const mc_client_sdk_update_error *failure;
} mc_client_sdk_update_state;

/* The canonical Display creates this child only for its real SDK profile.
 * It borrows Display/input/port until explicit native retirement, and owns no
 * second SDL queue or physical input cache. Source width/height are independent
 * of App.d/e, framebuffer pixels and the pending DisplayMode identity. */
mc_client_sdk_update_status mc_client_sdk_update_allocate(mc_client_display *,
                                                          mc_client_sdk_input *,
                                                          mc_client_event_port *, int32_t, int32_t,
                                                          mc_client_sdk_update **,
                                                          mc_client_sdk_update_error *);
/* Called once by the canonical createWindow prefix after the real port/input
 * children exist. A cold update owner has no devices and still executes the
 * original window_created guard before accessing either child. */
mc_client_sdk_update_status mc_client_sdk_update_attach(mc_client_sdk_update *,
                                                        mc_client_sdk_input *,
                                                        mc_client_event_port *,
                                                        mc_client_sdk_update_error *);
/* Canonical source createWindow width86/height95 stores, before initControls.
 * Private composition primitive, not a caller-controlled public field setter. */
void mc_client_sdk_update_create_dimensions(mc_client_sdk_update *, int32_t, int32_t);
mc_client_sdk_update_status mc_client_sdk_display_update(mc_client_sdk_update *,
                                                         mc_client_sdk_update_error *);
mc_client_sdk_update_status mc_client_sdk_display_update_messages(mc_client_sdk_update *, bool,
                                                                  mc_client_sdk_update_error *);
mc_client_sdk_update_status mc_client_sdk_display_process_messages(mc_client_sdk_update *,
                                                                   mc_client_sdk_update_error *);
mc_client_sdk_update_status mc_client_sdk_display_close_requested(mc_client_sdk_update *, bool *,
                                                                  mc_client_sdk_update_error *);
mc_client_sdk_update_status mc_client_sdk_display_active(mc_client_sdk_update *, bool *,
                                                         mc_client_sdk_update_error *);
mc_client_sdk_update_status mc_client_sdk_display_width(mc_client_sdk_update *, int32_t *,
                                                        mc_client_sdk_update_error *);
mc_client_sdk_update_status mc_client_sdk_display_height(mc_client_sdk_update *, int32_t *,
                                                         mc_client_sdk_update_error *);
bool mc_client_sdk_update_inspect(const mc_client_sdk_update *, mc_client_sdk_update_state *);
mc_client_sdk_update_status mc_client_sdk_update_retire(mc_client_sdk_update **,
                                                        mc_client_sdk_update_error *);

#endif
