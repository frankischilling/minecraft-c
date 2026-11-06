#ifndef MC_CLIENT_STARTUP_RENDERING_CALL_H
#define MC_CLIENT_STARTUP_RENDERING_CALL_H

#include "../world/render_global_internal.h"
#include "mc/client/particles/effect_renderer.h"

struct mc_client_startup_rendering_call {
    bool active, entered, g_stored, j_stored, viewport_returned, calls_completed, retired;
    bool global_allocated, global_constructor_returned, particle_attempted, particle_allocated;
    bool particle_constructor_returned, world_held;
    unsigned int last_pc, failed_pc;
    mc_client_render_global *global;
    mc_client_render_engine *global_engine, *particle_engine;
    mc_client_effect_renderer *particle; /* Same actual App retained j occurrence. */
    mc_world_runtime *particle_world;
    int32_t viewport_width, viewport_height;
    unsigned int global_last_store, global_constructor_pc, global_star_vertices;
    unsigned int particle_constructor_pc;
    mc_client_render_global_error global_error, global_cleanup;
    mc_effect_renderer_error particle_error;
    mc_effect_renderer_status particle_cleanup;
    mc_client_render_engine_status global_hold, particle_hold, global_release, particle_release;
    mc_client_display_error viewport_error;
};

#endif
