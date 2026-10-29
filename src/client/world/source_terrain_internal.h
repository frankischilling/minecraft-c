#ifndef MC_CLIENT_WORLD_SOURCE_TERRAIN_INTERNAL_H
#define MC_CLIENT_WORLD_SOURCE_TERRAIN_INTERNAL_H

#include "mc/client/settings/game_settings.h"
#include "mc/client/world/render_global_submit.h"
#include "render_global_internal.h"
#include "source_chunk_renderer_internal.h"

/* Synchronous entered source children borrow the public call's one admitted N
 * guard. On failure the complete frame chain remains in this retained call. */
struct mc_client_render_global_submit_call {
    mc_client_render_global_submit_state state;
    mc_client_render_global_error failure, cleanup;
    mc_client_render_global_world_state rebuild;
    mc_client_graphics_site frames[20];
    size_t depth;
    bool camera_held, batch_camera_held, math_held;
    mc_math_class_error math_error, math_cleanup;
    struct mc_client_display_active_query *query;
};

void mc_terrain_pc(mc_client_render_global_submit_call *, unsigned int);
void mc_terrain_store(mc_client_render_global_submit_call *, unsigned int);
bool mc_terrain_push(mc_client_render_global_submit_call *, const char *class_name,
                     const char *method, const char *descriptor);
void mc_terrain_pop(mc_client_render_global_submit_call *);
bool mc_terrain_fail(mc_client_render_global_submit_call *, mc_client_render_global_status,
                     const char *, unsigned int, bool native_policy);
bool mc_terrain_fp(mc_client_render_global_submit_call *, unsigned int);
bool mc_terrain_display(mc_client_render_global_submit_call *, bool, unsigned int);
bool mc_terrain_graphics(mc_client_render_global_submit_call *, bool, unsigned int);
bool mc_terrain_child(mc_client_render_global_submit_call *, bool, unsigned int);
bool mc_terrain_array(mc_client_render_global_submit_call *, bool sorted, int32_t index,
                      unsigned int pc, mc_client_source_dk **);
bool mc_terrain_dk_receiver(mc_client_render_global_submit_call *, const mc_client_source_dk *,
                            unsigned int pc);
bool mc_terrain_settings(mc_client_render_global_submit_call *, unsigned int app_pc,
                         unsigned int settings_pc, unsigned int field_pc, mc_game_settings_state *);
bool mc_terrain_math(mc_client_render_global_submit_call *, unsigned int);
bool mc_terrain_distance(mc_client_render_global_submit_call *, mc_client_source_dk *, unsigned int,
                         float *);
bool mc_terrain_subset(mc_client_render_global_submit_call *, int32_t start, int32_t end,
                       unsigned int caller_pc, int32_t *result);
bool mc_terrain_occlusion(mc_client_render_global_submit_call *, int32_t *result);
bool mc_terrain_poll(mc_client_render_global_submit_call *, int32_t start, int32_t end,
                     unsigned int caller_pc);
void mc_terrain_freeze_failure(mc_client_render_global_submit_call *);

#endif
