#ifndef MC_ENTITY_LIVING_VIEW_H
#define MC_ENTITY_LIVING_VIEW_H

#include "mc/core/math/classes.h"
#include "mc/entity/hit_result.h"
#include "mc/world/runtime/raytrace.h"

typedef struct mc_entity_living_ray_call mc_entity_living_ray_call;

typedef enum mc_entity_view_status {
    MC_ENTITY_VIEW_OK,
    MC_ENTITY_VIEW_ARGUMENT,
    MC_ENTITY_VIEW_BUSY,
    MC_ENTITY_VIEW_LIMIT,
    MC_ENTITY_VIEW_ALLOCATION,
    MC_ENTITY_VIEW_SOURCE_NULL_REFERENCE,
    MC_ENTITY_VIEW_UNSUPPORTED_SOURCE,
    MC_ENTITY_VIEW_UNSUPPORTED_FP,
    MC_ENTITY_VIEW_NATIVE_FAILED,
    MC_ENTITY_VIEW_MATH_FAILED,
    MC_ENTITY_VIEW_VECTOR_FAILED,
    MC_ENTITY_VIEW_WORLD_FAILED,
    MC_ENTITY_VIEW_HIT_FAILED,
    MC_ENTITY_VIEW_CLEANUP_FAILED
} mc_entity_view_status;

typedef struct mc_entity_view_site {
    const char *class_name, *method, *descriptor;
    int pc;
    bool entered;
} mc_entity_view_site;

typedef struct mc_entity_view_error {
    mc_entity_view_status status;
    const char *operation;
    bool native_policy, source_exception;
    const char *exception_class;
    mc_entity_view_site site, caller;
    mc_entity_status native_status;
    mc_math_class_error math;
    mc_entity_vector_error vector;
    mc_entity_hit_error hit;
    mc_world_raytrace_status world_status;
    const mc_world_raytrace_error *world; /* This exact retained child, never last_error. */
} mc_entity_view_error;

/* Actual client LS bodies inherited unchanged by GS/DC. Canonical LivingEntity
 * class identity is required; RAW/failed source prefixes add no READY gate.
 * Caller holds actual Entity/Math namespaces exclusively. Success transfers one
 * SAME pooled BT occurrence into a NULL output slot. Error storage is required
 * and disjoint. No added height, normalization, angle wrap or partial clamp.
 * Pool holds protect storage only: reset/reuse can overwrite held coordinates.
 * Look activates the supplied SAME MathHelper at lookup17/122, after argument
 * arithmetic; NULL Math namespace is a native dependency, not a Java null class. */
mc_entity_view_status mc_entity_living_position(mc_entity *, float partial, mc_entity_vector **out,
                                                mc_entity_view_error *);
mc_entity_view_status mc_entity_living_look(mc_entity *, mc_math_classes *, float partial,
                                            mc_entity_vector **out, mc_entity_view_error *);
mc_entity_view_status mc_entity_living_look_current(mc_entity *, mc_math_classes *,
                                                    mc_entity_vector **out, mc_entity_view_error *);

typedef struct mc_entity_living_ray_state {
    bool active, retiring, source_returned;
    unsigned int source_pc;
    mc_entity_view_status status;
    const mc_entity *receiver;
    const mc_math_classes *math;
    const mc_world_runtime *world;
    mc_entity_vector *eye, *look, *end;
    mc_entity_hit_result *result; /* Borrowed nullable normal result; retain before retire. */
    const mc_world_raytrace_call *world_call;
    const mc_entity_view_error *failure, *cleanup;
} mc_entity_living_ray_state;

/* Actual ls.a(DF): position2, look9, endpoint37, Entity.world43, fd.a(BT,BT)50.
 * A published call retains every reached prefix through failure/checked retire.
 * No retry/replay is exposed. NULL World faults at50 after all vector effects;
 * it does not enter a World child. The actual World DDA may mutate SAME eye.
 * Native preallocation admission failure leaves out NULL. Keep outputs/errors
 * disjoint; cleanup consumes only owned occurrences and preserves retry slots. */
mc_entity_view_status mc_entity_living_ray_begin(mc_entity *, mc_math_classes *, double distance,
                                                 float partial, mc_entity_living_ray_call **out);
bool mc_entity_living_ray_inspect(const mc_entity_living_ray_call *, mc_entity_living_ray_state *);
mc_entity_view_status mc_entity_living_ray_retire(mc_entity_living_ray_call **);

#endif
