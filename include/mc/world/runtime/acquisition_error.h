#ifndef MC_WORLD_RUNTIME_ACQUISITION_ERROR_H
#define MC_WORLD_RUNTIME_ACQUISITION_ERROR_H

#include "mc/world/runtime/entity_errors.h"
#include "mc/world/runtime/population/context.h"
#include "mc/world/runtime/runtime.h"
#include "mc/world/runtime/tile_entities/errors.h"

typedef struct mc_world_source_chunk_error mc_world_source_chunk_error;

/* Passive diagnostics. source_chunk borrows the real ld retained failure and
 * remains live only while that World's source loader occurrence is live. */
typedef struct mc_world_runtime_acquisition_error {
    mc_chunk_acquire_status status;
    mc_world_storage_error cause;
    mc_registry_error registry;
    mc_live_tile_entity_error tile_entity;
    mc_world_entity_batch_error batch;
    unsigned int activation_bytecode, activation_world_bytecode;
    unsigned int load_bytecode, load_builder_bytecode;
    mc_world_entity_load_error load_entity;
    mc_world_population_context population;
    const mc_world_source_chunk_error *source_chunk;
} mc_world_runtime_acquisition_error;

#endif
