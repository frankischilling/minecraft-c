#ifndef MC_CLIENT_WORLD_RENDER_GLOBAL_CLOUDS_H
#define MC_CLIENT_WORLD_RENDER_GLOBAL_CLOUDS_H

#include "mc/client/world/render_global_sky.h"

typedef struct mc_client_render_global_clouds_call mc_client_render_global_clouds_call;
typedef mc_client_render_global_sky_error mc_client_render_global_clouds_error;

typedef struct mc_client_render_global_clouds_state {
    bool active, retiring, returned, nether_skipped, fancy;
    bool color_returned, anaglyph, texture_bound, scaled, state_restored;
    unsigned int source_pc, gl_calls, vertices, draws, passes, completed_cells;
    float partial, camera_y, height, rgb[3], offset_u, offset_v, fraction_x, fraction_z;
    double scroll_x, scroll_z;
    mc_client_render_global *owner;
    const mc_entity_vector *color_vector;
    const mc_client_render_global_clouds_error *failure, *cleanup;
} mc_client_render_global_clouds_state;

/* Actual n.b(F)V, including its n.c(F)V fancy child. The caller keeps the
 * complete source App/World/Settings/Engine graph live and exclusive through
 * retirement. Reads use actual current fields, namespace statics and owners.
 * Failed allocated calls remain terminal and inspectable. Neither retirement
 * nor failure restores source GL, NW, matrices, texture or pooled-vector state. */
mc_client_render_global_status
mc_client_render_global_clouds_begin(mc_client_render_global *, mc_math_classes *, float partial,
                                     mc_client_render_global_clouds_call **owned_out,
                                     mc_client_render_global_clouds_error *);
bool mc_client_render_global_clouds_inspect(const mc_client_render_global_clouds_call *,
                                            mc_client_render_global_clouds_state *);
mc_client_render_global_status
mc_client_render_global_clouds_retire(mc_client_render_global_clouds_call **,
                                      mc_client_render_global_clouds_error *);

#endif
