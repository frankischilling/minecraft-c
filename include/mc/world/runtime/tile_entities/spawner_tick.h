#ifndef MC_WORLD_RUNTIME_TILE_ENTITIES_SPAWNER_TICK_H
#define MC_WORLD_RUNTIME_TILE_ENTITIES_SPAWNER_TICK_H

#include "mc/entity/bounds/bounds.h"
#include "mc/entity/factory/factory.h"
#include "mc/entity/living/spawn_effect.h"
#include "mc/entity/mob/spawning.h"
#include "mc/world/runtime/collision/class_entities.h"
#include "mc/world/runtime/entity_errors.h"
#include "mc/world/runtime/presentation.h"
#include "mc/world/runtime/spawner_observation.h"
#include "mc/world/runtime/tile_entities/tile_entities.h"

typedef enum mc_world_spawner_tick_child {
    MC_WORLD_SPAWNER_TICK_CHILD_NONE,
    MC_WORLD_SPAWNER_TICK_CHILD_FACTORY,
    MC_WORLD_SPAWNER_TICK_CHILD_BOUNDS,
    MC_WORLD_SPAWNER_TICK_CHILD_QUERY,
    MC_WORLD_SPAWNER_TICK_CHILD_POSE,
    MC_WORLD_SPAWNER_TICK_CHILD_PREDICATE,
    MC_WORLD_SPAWNER_TICK_CHILD_PUBLICATION,
    MC_WORLD_SPAWNER_TICK_CHILD_REGISTRY,
    MC_WORLD_SPAWNER_TICK_CHILD_PARTICLE,
    MC_WORLD_SPAWNER_TICK_CHILD_EFFECT
} mc_world_spawner_tick_child;

typedef struct mc_world_spawner_tick_error {
    mc_world_spawner_tick_child child;
    bool source_exception;
    const char *exception_class;
    unsigned int delay_bytecode;
    mc_entity_status entity_status;
    mc_tile_entity_status tile_status;
    /* This temporary candidate occurrence survives a failed descendant. It
     * does not own, undo or replace the Chunk/World publication occurrences. */
    mc_entity *candidate;
    mc_world_runtime *evaluated_world;
    mc_world_access_failure captured_failure;
    bool capture_attempted;
    mc_world_storage_status capture_status;

    union {
        mc_entity_factory_error factory;
        mc_entity_bounds_error bounds;
        mc_world_nearby_error query;

        struct {
            mc_entity_status status;
            const char *operation;
            unsigned int bytecode, position_bytecode;
            bool source_null, native_policy;
        } pose;

        mc_mob_spawn_error predicate;
        mc_world_entity_error publication;
        mc_registry_error registry;
        mc_world_mutation_error particle;
        mc_living_spawn_effect_error effect;
    } detail;
} mc_world_spawner_tick_error;

/* End the failed candidate and only the explicitly captured callback receipt.
 * The caller-held defining namespaces and reached World graph outlive these
 * borrowed child diagnostics. Copying an active error transfers ownership. */
void mc_world_spawner_tick_error_retire(mc_world_spawner_tick_error *);

#endif
