#ifndef MC_CLIENT_STARTUP_APPLICATION_LOGO_H
#define MC_CLIENT_STARTUP_APPLICATION_LOGO_H

#include "../gui/loading_screen.h"
#include "../render_engine/texture_lookup.h"
#include "application_quad.h"
#include "mc/client/gui/scaled_resolution.h"

typedef struct mc_client_startup_logo_call mc_client_startup_logo_call;

/* Startup owns this heap record. Its temporaries and passive diagnostics do
 * not replace Minecraft fields or own the shared Engine/device resources. */
struct mc_client_startup_logo_call {
    bool active, source_returned, background_drawn, logo_drawn, swapped, native_policy;
    int32_t pc;
    const char *operation;
    mc_client_graphics_site child;
    mc_scaled_resolution resolution;
    const mc_player_string *literal;
    mc_client_texture texture_id;
    mc_client_loading_error resolution_error;
    mc_client_graphics_error graphics;
    mc_client_texture_lookup_error texture;
    mc_client_application_quad_error quad;
    mc_client_display_error display;
    mc_player_status string;
};

#endif
