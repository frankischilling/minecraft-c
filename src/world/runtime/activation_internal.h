#ifndef MC_WORLD_RUNTIME_ACTIVATION_INTERNAL_H
#define MC_WORLD_RUNTIME_ACTIVATION_INTERNAL_H

#include "mc/world/chunk/object.h"
#include "mc/world/runtime/tiles/errors.h"

/* A reached Chunk invocation and World callee PC are separate. Zero denotes
 * native admission or an unreached World frame. These operations consume actual
 * actual child identities, never arbitrary caller collection projections.
 * They do not publish chunks or change active; cache/lifecycle own those stores.
 * Each World invocation holds the actual Chunk.d receiver, which may differ
 * from the callback coordinator. Empty Entity addAll validates only its reached actual
 * destination. The reached World receiver must be GAME and not RETIRED;
 * detach/maintenance precede World retirement. Tile addAll uses actual owned
 * c/G occurrences and retains its whole typed diagnostic. Native container
 * allocation policy does not claim Java object layout or OOM timing. */
typedef struct mc_world_activation_error {
    mc_world_storage_status status;
    const char *operation;
    unsigned int bytecode, world_bytecode;
    mc_world_storage_error cause;
    mc_live_tile_error tile;
} mc_world_activation_error;

mc_world_storage_status mc_world_runtime_register_chunk_tiles(mc_world_chunk *chunk,
                                                              mc_world_activation_error *error);
mc_world_storage_status mc_world_runtime_register_chunk_entities(mc_world_chunk *chunk,
                                                                 size_t slice,
                                                                 mc_world_activation_error *error);
mc_world_storage_status mc_world_runtime_invalidate_chunk_tiles(mc_world_chunk *chunk,
                                                                mc_world_activation_error *error);
mc_world_storage_status mc_world_runtime_defer_chunk_entities(mc_world_chunk *chunk, size_t slice,
                                                              mc_world_activation_error *error);

#endif
