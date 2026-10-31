#include "mc/client/world/overlay_setup.h"
#include "../../application_state.h"
#include "../../display/camera_surface.h"
#include "../../display/world_frame_surface.h"
#include "internal.h"

#include <emmintrin.h>
#include <string.h>

static bool fail(mc_client_overlay_setup_state *state, mc_client_overlay_setup_error *error,
                 mc_client_overlay_setup_status status, const char *operation, unsigned int pc,
                 bool native) {
    error->status = status;
    error->operation = operation;
    error->site = (mc_client_graphics_site){"px", "b", "()V", (int32_t)pc};
    error->native_policy = native;
    state->source_pc = pc;
    return false;
}

static bool gl(mc_client_overlay_setup_state *state, mc_client_overlay_setup_error *error, bool ok,
               unsigned int pc) {
    return ok || fail(state, error,
                      error->display.status == MC_CLIENT_DISPLAY_UNSUPPORTED_FP
                          ? MC_CLIENT_OVERLAY_SETUP_UNSUPPORTED_FP
                          : MC_CLIENT_OVERLAY_SETUP_NATIVE_FAILED,
                      error->display.operation, pc, true);
}

static bool body(mc_client_source_px *owner, mc_client_overlay_setup_state *state,
                 mc_client_overlay_setup_error *error) {
    client_application *app = owner->fields.j;
    if (app == NULL)
        return fail(state, error, MC_CLIENT_OVERLAY_SETUP_SOURCE_NULL,
                    "actual overlay projection App receiver", 8, false);
    mc_game_settings *settings = app->settings; /* Source argument8 before dimensions15/22. */
    state->physical_width = owner->fields.j->d;
    state->physical_height = owner->fields.j->e;
    mc_game_settings_state options;
    if (!mc_game_settings_read_state(settings, &options)) {
        error->child = (mc_client_graphics_site){"qq", "<init>", "(Lkv;II)V", 20};
        return fail(state, error,
                    settings == NULL ? MC_CLIENT_OVERLAY_SETUP_SOURCE_NULL
                                     : MC_CLIENT_OVERLAY_SETUP_NATIVE_FAILED,
                    "actual overlay projection scaled-resolution Settings", 25, settings != NULL);
    }
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return fail(state, error, MC_CLIENT_OVERLAY_SETUP_UNSUPPORTED_FP,
                    "overlay projection scaled-resolution FP", 25, true);
    if (!mc_scaled_resolution_init(state->physical_width, state->physical_height,
                                   options.values.gui_scale, &state->resolution))
        return fail(state, error, MC_CLIENT_OVERLAY_SETUP_NATIVE_FAILED,
                    "overlay projection native physical dimensions", 25, true);
    state->resolution_created = true;
    mc_client_display *display = owner->services.display;
    if (!gl(state, error, mc_client_display_source_clear(display, 256, &error->display), 32))
        return false;
    state->depth_cleared = true;
    if (!gl(state, error, mc_client_display_source_matrix_mode(display, 5889, &error->display), 38))
        return false;
    state->projection_selected = true;
    if (!gl(state, error, mc_client_display_source_load_identity(display, &error->display), 41))
        return false;
    state->projection_reset = true;
    if (!gl(state, error,
            mc_client_display_source_ortho(display, 0, state->resolution.width_exact,
                                           state->resolution.height_exact, 0, 1000, 3000,
                                           &error->display),
            60))
        return false;
    state->ortho_applied = true;
    if (!gl(state, error, mc_client_display_source_matrix_mode(display, 5888, &error->display), 66))
        return false;
    state->modelview_selected = true;
    if (!gl(state, error, mc_client_display_source_load_identity(display, &error->display), 69))
        return false;
    state->modelview_reset = true;
    if (!gl(state, error, mc_client_display_source_translate(display, 0, 0, -2000, &error->display),
            76))
        return false;
    state->translated = state->returned = true;
    state->source_pc = 79;
    return true;
}

bool entity_renderer_overlay_setup(mc_client_source_px *owner, mc_client_overlay_setup_state *state,
                                   mc_client_overlay_setup_error *error) {
    if (state == NULL || error == NULL)
        return false;
    memset(state, 0, sizeof(*state));
    memset(error, 0, sizeof(*error));
    if (owner == NULL)
        return fail(state, error, MC_CLIENT_OVERLAY_SETUP_SOURCE_NULL,
                    "actual overlay projection PX receiver", 0, false);
    if (owner->active || owner->phase != MC_CLIENT_ENTITY_RENDERER_READY)
        return fail(state, error, MC_CLIENT_OVERLAY_SETUP_BUSY,
                    "overlay projection active/native PX phase", 0, true);
    owner->active = true;
    const bool ok = body(owner, state, error);
    owner->active = false;
    return ok;
}
