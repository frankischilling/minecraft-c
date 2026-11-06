#ifndef MC_ENTITY_DROP_INTERNAL_H
#define MC_ENTITY_DROP_INTERNAL_H

#include "mc/entity/drop.h"
#include "mc/world/runtime/entities.h"
#include "mc/world/runtime/world_access.h"

struct mc_entity_drop_call {
    mc_entity_drop_state state;
    mc_entity_drop_error error, cleanup;
    mc_registry_classes *registry;
    mc_world_entity_error publication;
    mc_world_access_failure receipt;
    bool actor_held, stack_held, registry_held;
    bool constructor_world_held, publication_world_held;
};

#endif
