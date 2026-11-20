#ifndef MC_ENTITY_MOB_WOLF_DAMAGE_H
#define MC_ENTITY_MOB_WOLF_DAMAGE_H

#include "mc/entity/bounds/bounds.h"
#include "mc/world/runtime/collision/class_entities.h"

struct mc_entity_effect_error;

typedef struct mc_wolf_damage_site {
    const char *class_name, *method, *descriptor;
    uint16_t pc;
} mc_wolf_damage_site;

/* Passive borrowed view of one actual GI.a(SN,I) invocation. Pending work
 * belongs to the same Wolf, not this value. receiver/error/nearby/bounds and
 * arbitrary reached exception names expire when checked cleanup consumes it.
 * Completed values retain scalar/identity observations only. */
typedef struct mc_wolf_damage_state {
    bool entered, pending, active, returned;
    bool native_policy, source_exception, source_error;
    mc_entity_status status, cleanup_status;
    const char *operation, *cleanup_operation, *exception_class;
    mc_wolf_damage_site site, child;
    mc_entity *receiver;
    mc_world_runtime *world;
    const mc_entity_bounds *bounds, *expanded;
    const mc_world_class_entity_list *wolves;
    uintptr_t receiver_identity, argument_source_identity, source_identity, world_identity;
    int32_t amount, parent_amount;
    bool parent_entered, parent_returned, parent_result, parent_cleanup_owned;
    bool sitting_cleared, query_entered, query_returned;
    size_t wolves_examined, wolves_alerted;
    const struct mc_entity_effect_error *parent_error;
    const mc_world_nearby_error *nearby;
    const mc_entity_bounds_error *bounds_error;
} mc_wolf_damage_state;

#endif
