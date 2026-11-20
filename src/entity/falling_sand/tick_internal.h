#ifndef MC_ENTITY_FALLING_SAND_TICK_INTERNAL_H
#define MC_ENTITY_FALLING_SAND_TICK_INTERNAL_H

#include "../../world/runtime/mutation/mutation_internal.h"
#include "../living/travel_math_internal.h"
#include "../tick/tick_internal.h"
#include "mc/entity/falling_sand/tick.h"
#include "mc/world/runtime/entities.h"

struct mc_falling_entity_tick_work {
    mc_entity *entity;
    mc_entity_drop_call *drop;
    mc_registry_classes *registry;
    mc_world_runtime *worlds[8];
    size_t world_count;
    bool entity_held, registry_held, active;
    mc_entity_move_error movement;
    mc_world_storage_error block;
    mc_world_runtime_acquisition_error acquisition;
    mc_world_mutation_error mutation, predicate;
    mc_world_placement_error placement;
    mc_entity_effect_error effect;
    mc_registry_error registry_error;
    mc_world_access_failure mutation_receipt;
};

#endif
