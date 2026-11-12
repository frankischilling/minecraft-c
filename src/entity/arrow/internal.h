#ifndef MC_ENTITY_ARROW_INTERNAL_H
#define MC_ENTITY_ARROW_INTERNAL_H

#include "../entity_internal.h"
#include "mc/entity/arrow/construction.h"

struct mc_arrow_tick_call;

typedef struct mc_arrow_entity_state {
    mc_arrow_fields fields;
    /* Independent counted occurrence backing actual source SL.c. */
    mc_entity *shooter_hold;
    /* Borrowed backlink; the independently owned call keeps this Arrow alive. */
    struct mc_arrow_tick_call *tick_call;
    unsigned int constructor_pc, heading_pc;
    bool constructor_active, heading_active;
    mc_arrow_error failure;
} mc_arrow_entity_state;

typedef struct mc_arrow_entity_object {
    mc_entity entity;
    mc_arrow_entity_state state;
} mc_arrow_entity_object;

bool mc_entity_has_arrow(const mc_entity *);

/* Optional direct-component observation, outside public errors/Arrow state.
 * Initialized on every new attempt. Values are valid only when the returned
 * mc_arrow_error.heading_entered is true. No ownership, source getter, draw or
 * callback is added; all arguments/error/owners must be live and disjoint. */
typedef struct mc_arrow_heading_arguments {
    double x, y, z;
    float speed, spread;
} mc_arrow_heading_arguments;

_Static_assert(sizeof(mc_arrow_heading_arguments) == 32, "Private heading arguments are 32 bytes");

/* Same public admission, holds, cleanup and one entered implementation.
 * NULL arguments selects the unobserved path; NULL error retains its existing
 * independent semantics. No invocation or observation is resumed/replayed. */
mc_entity_status mc_arrow_construct_living_observed(mc_entity *, mc_world_runtime *, mc_entity *,
                                                    mc_math_classes *, mc_arrow_heading_arguments *,
                                                    mc_arrow_error *);
mc_entity_status mc_arrow_heading_observed(mc_entity *, mc_math_classes *, double x, double y,
                                           double z, float speed, float spread,
                                           mc_arrow_heading_arguments *, mc_arrow_error *);

#endif
