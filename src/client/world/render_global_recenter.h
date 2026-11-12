#ifndef MC_CLIENT_WORLD_RENDER_GLOBAL_RECENTER_H
#define MC_CLIENT_WORLD_RENDER_GLOBAL_RECENTER_H

#include "render_global_internal.h"

/* Whole actual n.b(III)V and n.a()V on the SAME admitted N. Caller owns its
 * active guard and all App/World/Display/class dependencies. These functions
 * neither admit another call nor rebind WorldAccess/CV. Failure preserves all
 * reached source fields, DK/list mutations and exact nested diagnostics. */
bool mc_render_global_recenter_entered(mc_client_render_global *, int32_t, int32_t, int32_t,
                                       mc_client_render_global_error *);
bool mc_render_global_rebuild_entered(mc_client_render_global *, mc_math_classes *,
                                      mc_client_render_global_world_state *,
                                      mc_client_render_global_error *);

#endif
