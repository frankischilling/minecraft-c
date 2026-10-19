#ifndef MC_WORLD_RUNTIME_ACTIVATION_INTERNAL_H
#define MC_WORLD_RUNTIME_ACTIVATION_INTERNAL_H

#include "mc/world/chunk/object.h"

/* A reached Chunk invocation and World callee PC are separate. Zero denotes
 * native admission or an unreached World frame. These operations consume actual
 * known-empty child identities, never arbitrary caller collection projections.
 * They do not publish chunks or change active; cache/lifecycle own those stores.
 * Each World invocation holds the actual Chunk.d receiver, which may differ
 * from the callback coordinator. Empty addAll validates only its reached actual
 * destination. The reached World receiver must be GAME and not RETIRED;
 * detach/maintenance precede World retirement. No Java collection layout,
 * allocation or modCount is modeled. */
typedef struct mc_world_activation_error {
    mc_world_storage_status status;
    const char *operation;
    unsigned int bytecode, world_bytecode;
    mc_world_storage_error cause;
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
