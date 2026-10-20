#ifndef MC_WORLD_RUNTIME_ACQUISITION_INTERNAL_H
#define MC_WORLD_RUNTIME_ACQUISITION_INTERNAL_H

#include "mc/world/runtime/entity_errors.h"
#include "mc/world/runtime/runtime.h"
#include "mc/world/runtime/tiles/errors.h"

/* One synchronous acquisition attempt owns this value on its caller's stack.
 * Callback adapters write only the currently installed attempt; nested calls
 * install independent values and restore their predecessor before returning.
 * No ambient last-error lookup, operation-string decoding or owner hold occurs.
 * Outputs/errors are disjoint from owners and remain readable through the call. */
typedef struct mc_world_runtime_acquisition_error {
    mc_chunk_acquire_status status;
    mc_world_storage_error cause;
    mc_registry_error registry;
    mc_live_tile_error tile;
    mc_world_entity_batch_error batch;
    unsigned int activation_bytecode, activation_world_bytecode;
    unsigned int load_bytecode, load_builder_bytecode;
} mc_world_runtime_acquisition_error;

mc_chunk_acquire_status mc_world_runtime_get_detailed(mc_world_runtime *runtime, int32_t x,
                                                      int32_t z, mc_chunk_state **out,
                                                      mc_world_runtime_acquisition_error *error);

#endif
