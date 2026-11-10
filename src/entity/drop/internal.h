#ifndef MC_ENTITY_DROP_INTERNAL_H
#define MC_ENTITY_DROP_INTERNAL_H

#include "execution_internal.h"
#include "mc/entity/drop.h"
#include "mc/world/runtime/entities.h"
#include "mc/world/runtime/world_access.h"

typedef struct mc_entity_drop_execution {
    mc_entity_tick_execution *execution;
    mc_world_runtime_classes *code_classes;
    mc_world_runtime_execution_code code;
} mc_entity_drop_execution;

struct mc_entity_drop_call {
    mc_entity_drop_state state;
    mc_entity_drop_error error, cleanup;
    mc_registry_classes *registry;
    mc_world_entity_error publication;
    mc_world_access_failure receipt;
    bool actor_held, stack_held, registry_held;
    bool constructor_world_held, publication_world_held;
    bool execution_owned, owned_completed, publication_execution_returned;
    mc_entity_drop_execution execution;
    mc_world_runtime_execution_result owned;
    mc_world_entity_insert_execution insertion;
    mc_world_storage_status publication_status;
    mc_entity_drop_world_execution_view constructor_scope, publication_scope;
};

#endif
