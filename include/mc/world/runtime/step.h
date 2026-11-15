#ifndef MC_WORLD_RUNTIME_STEP_H
#define MC_WORLD_RUNTIME_STEP_H

#include "mc/world/runtime/mutations.h"

typedef struct mc_world_step_limits {
    size_t max_wrapped_calls;
} mc_world_step_limits;

#define MC_WORLD_STEP_DEFAULT_WRAPPED_CALLS ((size_t)1048576)

typedef enum mc_world_step_source_failure_kind {
    MC_WORLD_STEP_SOURCE_NONE,
    MC_WORLD_STEP_SOURCE_NULL_WORLD,
    MC_WORLD_STEP_SOURCE_NULL_RANDOM,
    MC_WORLD_STEP_SOURCE_REGISTRY,
    MC_WORLD_STEP_SOURCE_QUERY,
    MC_WORLD_STEP_SOURCE_PRESENTATION,
    MC_WORLD_STEP_SOURCE_NULL_STATIC,
    MC_WORLD_STEP_SOURCE_MUTATION
} mc_world_step_source_failure_kind;

/* Actual selected Redstone Ore child, stored when that child fails. The kind
 * names the reached operation; it does not turn every native failure of that
 * operation into a Source exception. Inspect the corresponding retained query,
 * Registry or mutation diagnostic before classifying it. NULL_STATIC is a
 * positive nullable bn-receiver witness from the activation body. */
typedef struct mc_world_step_source_failure {
    mc_world_step_source_failure_kind kind;
    mc_registry_status registry_status;
} mc_world_step_source_failure;

typedef struct mc_world_step_error {
    mc_world_storage_status status;
    const char *operation, *block_class;
    const mc_block *block;
    unsigned int bytecode, activation_bytecode, particle_bytecode, opaque_bytecode;
    size_t wrapped_calls;
    int32_t particle_face;
    mc_registry_error registry;
    mc_world_storage_error cause;
    mc_world_collision_error block_read;
    mc_world_mutation_error mutation;
    mc_world_step_source_failure source;
} mc_world_step_error;

void mc_world_step_error_init(mc_world_step_error *error);

/* Actual Block.b(World,III,Entity)V virtual, distinct from contact a(...).
 * Complete current 69-kind inheritance includes Leaves' base passthrough,
 * actual Stairs wrapped dispatch, Redstone Ore particles/activation and the
 * existing Farmland body. All other kinds inherit the real empty base body.
 * World and Entity can be null until the selected source body reads them.
 * Owners remain held/exclusive through synchronous calls; no pool resets,
 * rollback, scheduling drain or successful unknown-kind fallback is supplied.
 * A wrapping limit stops at the pending delegate with its completed prefix.
 * error is optional and disjoint from every owner, input and dependency. */
mc_world_storage_status mc_world_runtime_entity_step(mc_world_runtime *world, const mc_block *block,
                                                     int32_t x, int32_t y, int32_t z,
                                                     mc_entity *entity,
                                                     const mc_world_step_limits *limits,
                                                     mc_world_step_error *error);

#endif
