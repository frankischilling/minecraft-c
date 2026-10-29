#ifndef MC_CLIENT_RENDER_ENGINE_LIGHTING_H
#define MC_CLIENT_RENDER_ENGINE_LIGHTING_H

#include "ge.h"

typedef struct mc_client_lighting_state {
    mc_client_render_engine_phase phase;
    const mc_client_direct_float_buffer *a;
    const mc_client_graphics_error *failure;
} mc_client_lighting_state;

/* Actual u active use allocates ge.e(16) into the SAME class arena, then stores
 * u.a5. No Display or GL is needed. Failure is terminal for this actual class;
 * any completed ge initialization/allocation remains owned by that namespace.
 * Caller keeps the classes live and exclusive. Views expire at class release. */
bool mc_client_lighting_active_use(mc_client_render_engine_classes *, mc_client_graphics_site,
                                   mc_client_graphics_error *);
bool mc_client_lighting_inspect(const mc_client_render_engine_classes *,
                                mc_client_lighting_state *);

/* Complete u.a()V: active use precedes ordered disables3/9/15/21. Display is
 * dereferenced only by the reached GL child. The same classes/Display stay live
 * exclusively through this synchronous call. Graphics error carries caller,
 * exact u/ge failure site, and the complete reached Display cause. retained_count
 * reports the number of completed GL disables, including a failed prefix. */
bool mc_client_lighting_disable(mc_client_render_engine_classes *, mc_client_display *,
                                mc_client_graphics_site, mc_client_graphics_error *);

#endif
