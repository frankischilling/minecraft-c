#ifndef MC_ENTITY_MOVEMENT_ENVIRONMENT_INTERNAL_H
#define MC_ENTITY_MOVEMENT_ENVIRONMENT_INTERNAL_H

#include "../../core/math/execution_internal.h"
#include "mc/entity/movement/environment.h"

/* Selected held READY client Arrow only. Caller owns actor and defining
 * Registry lifetimes/exclusivity and fresh disjoint outputs/errors. No extra
 * actor/Registry retain or eager code-owner check precedes pooled bounds.
 * The required execution storage may name a NULL Math dependency until use.
 * Defining Registry is checked only at the reached Material static access;
 * source World remains nullable until its actual invocation. */
mc_entity_status mc_entity_environment_water_held_in_execution(
    mc_entity *, mc_registry_classes *, mc_math_execution *,
    const mc_world_material_movement_limits *, bool *, mc_entity_environment_error *);
mc_entity_status mc_entity_environment_lava_held_in_execution(
    mc_entity *, mc_registry_classes *, mc_math_execution *,
    const mc_world_material_movement_limits *, bool *, mc_entity_environment_error *);

#endif
