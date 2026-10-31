#ifndef MC_CLIENT_RENDER_ENGINE_LIGHTING_H
#define MC_CLIENT_RENDER_ENGINE_LIGHTING_H

#include "ge.h"
#include "mc/core/math/classes.h"
#include "mc/entity/vector/vector.h"

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

typedef enum mc_client_lighting_status {
    MC_CLIENT_LIGHTING_OK,
    MC_CLIENT_LIGHTING_GRAPHICS_FAILED,
    MC_CLIENT_LIGHTING_VECTOR_FAILED,
    MC_CLIENT_LIGHTING_MATH_FAILED,
    MC_CLIENT_LIGHTING_NATIVE_FAILED
} mc_client_lighting_status;

typedef struct mc_client_lighting_error {
    mc_client_lighting_status status;
    const char *operation;
    mc_client_graphics_site caller, site, child, helper;
    bool native_policy, source_returned, cleanup_failed;
    size_t completed_display_calls, completed_buffer_fills, pooled_vectors;
    mc_client_graphics_error graphics;
    mc_entity_vector_error vector;
    mc_math_class_error math, cleanup_math;
    mc_entity_status native_entity_status, cleanup_entity_status;
    mc_client_render_engine_status native_graphics_status, cleanup_graphics_status;
} mc_client_lighting_error;

/* Complete u.b()V on the SAME U buffer, Entity vector pool, MathHelper namespace
 * and Display. Active use precedes Display access; BT starts at48 and MathHelper
 * at BT.c29 inside U51. Four successful pooled factories advance the actual
 * cursor, without a reset. The nine helper fills retain real cursor/data
 * prefixes on failure, and Display uploads borrow rather than consume them.
 *
 * Caller owns error storage and keeps all inputs live and exclusive. Native
 * occurrences are acquired only when reached and balanced before return.
 * site is the reached U.b operation; child/helper retain nested source sites,
 * and embedded errors retain their complete typed causes. Counters describe
 * completed effects even on failure. No state rollback or lit draw is implied. */
bool mc_client_lighting_enable(mc_client_render_engine_classes *, mc_entity_classes *,
                               mc_math_classes *, mc_client_display *, mc_client_graphics_site,
                               mc_client_lighting_error *);

#endif
