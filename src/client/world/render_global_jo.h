#ifndef MC_CLIENT_WORLD_RENDER_GLOBAL_JO_H
#define MC_CLIENT_WORLD_RENDER_GLOBAL_JO_H

#include "render_global_internal.h"

/* Actual temporary jo fields. Constructor captures source negated camera
 * coordinates once; compare reads live canonical DK centers. No MD flags,
 * float-distance narrowing or ordinal tie breaker participates. */
typedef struct mc_render_global_jo {
    double a, b, c;
} mc_render_global_jo;

bool mc_render_global_jo_construct(mc_render_global_jo *, const mc_entity *,
                                   mc_client_render_global_error *);
bool mc_render_global_jo_compare(const mc_render_global_jo *, const mc_client_source_dk *,
                                 const mc_client_source_dk *, int *,
                                 mc_client_render_global_error *);
/* Borrow the caller's admitted N guard/array/camera lifetime. Default caller
 * is terrain submission299; rebuild uses its actual Arrays.sort site691. */
bool mc_render_global_sort_entered(mc_client_render_global *, const mc_entity *,
                                   mc_client_render_global_error *);
bool mc_render_global_sort_at_entered(mc_client_render_global *, const mc_entity *,
                                      mc_client_graphics_site caller,
                                      mc_client_render_global_error *);

#endif
