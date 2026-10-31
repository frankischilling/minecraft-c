#ifndef MC_CLIENT_WORLD_RENDER_BATCH_INTERNAL_H
#define MC_CLIENT_WORLD_RENDER_BATCH_INTERNAL_H

#include "render_global_internal.h"

/* Whole actual TG methods on its SAME source fields and Ge IntBuffer. These
 * synchronous borrowed children require the enclosing N guard/graph to stay
 * live and exclusive. They add no class activation, owner, buffer or GL reset.
 * Outputs/errors are valid and disjoint; failure preserves reached stores and
 * Push/Translate/list prefix. A failed parent is terminal, never replayed. */
bool mc_render_batch_configure(mc_client_render_global *, mc_client_render_batch *, int32_t x,
                               int32_t y, int32_t z, double camera_x, double camera_y,
                               double camera_z, mc_client_render_global_error *);
bool mc_render_batch_matches(mc_client_render_global *, mc_client_render_batch *, int32_t x,
                             int32_t y, int32_t z, bool *out, mc_client_render_global_error *);
bool mc_render_batch_reset(mc_client_render_global *, mc_client_render_batch *,
                           mc_client_render_global_error *);
bool mc_render_batch_append(mc_client_render_global *, mc_client_render_batch *, int32_t list,
                            mc_client_render_global_error *);
bool mc_render_batch_draw(mc_client_render_global *, mc_client_render_batch *,
                          mc_client_render_global_error *);

#endif
