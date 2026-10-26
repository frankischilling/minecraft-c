#ifndef MC_WORLD_RUNTIME_ACQUISITION_ERROR_H
#define MC_WORLD_RUNTIME_ACQUISITION_ERROR_H

#include "mc/world/runtime/entity_errors.h"
#include "mc/world/runtime/population/context.h"
#include "mc/world/runtime/runtime.h"
#include "mc/world/runtime/tile_entities/errors.h"

/* Exact existing passive acquisition diagnostics; no layout or ownership change. */
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
} mc_world_runtime_acquisition_error;

#endif
