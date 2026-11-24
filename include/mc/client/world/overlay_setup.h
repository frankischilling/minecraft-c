#ifndef MC_CLIENT_WORLD_OVERLAY_SETUP_H
#define MC_CLIENT_WORLD_OVERLAY_SETUP_H
#include "mc/client/gui/scaled_resolution.h"
#include "mc/client/world/entity_renderer.h"

typedef enum mc_client_overlay_setup_status {
    MC_CLIENT_OVERLAY_SETUP_OK,
    MC_CLIENT_OVERLAY_SETUP_ARGUMENT,
    MC_CLIENT_OVERLAY_SETUP_BUSY,
    MC_CLIENT_OVERLAY_SETUP_SOURCE_NULL,
    MC_CLIENT_OVERLAY_SETUP_NATIVE_FAILED,
    MC_CLIENT_OVERLAY_SETUP_UNSUPPORTED_FP
} mc_client_overlay_setup_status;

typedef struct mc_client_overlay_setup_error {
    mc_client_overlay_setup_status status;
    const char *operation;
    mc_client_graphics_site site, child;
    bool native_policy;
    mc_client_display_error display;
    mc_client_entity_renderer_error owner;
} mc_client_overlay_setup_error;

typedef struct mc_client_overlay_setup_state {
    bool returned, resolution_created, depth_cleared, projection_selected, projection_reset;
    bool ortho_applied, modelview_selected, modelview_reset, translated;
    unsigned int source_pc;
    int32_t physical_width, physical_height;
    mc_scaled_resolution resolution;
} mc_client_overlay_setup_state;

/* Actual px.b()V overlay projection. This synchronous child borrows the complete
 * live App graph, enters only its own PX guard, and publishes each source GL
 * effect immediately. Reached failure does not restore matrices or depth. Its
 * fixed value receipt contains no source-object/child views requiring retirement. */
bool entity_renderer_overlay_setup(mc_client_source_px *, mc_client_overlay_setup_state *,
                                   mc_client_overlay_setup_error *);
#endif
