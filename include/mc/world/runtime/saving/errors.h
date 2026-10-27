#ifndef MC_WORLD_RUNTIME_SAVING_ERRORS_H
#define MC_WORLD_RUNTIME_SAVING_ERRORS_H

#include "mc/world/cache/lifecycle.h"
#include "mc/world/info/info.h"
#include "mc/world/runtime/entity_errors.h"
#include "mc/world/runtime/tile_entities/errors.h"
#include "mc/world/storage/chunk.h"

typedef struct mc_world_player_metadata_error {
    mc_world_storage_status status;
    const char *operation;
    unsigned int wrapper_bytecode, list_bytecode, info_writer_bytecode;
    size_t player_index;
    mc_entity_nbt_error entity;
    mc_world_info_error info;
} mc_world_player_metadata_error;

void mc_world_player_metadata_error_init(mc_world_player_metadata_error *);

/* Passive per-attempt diagnostics. Region and shared serializer positions are
 * distinct from the complete reached TileEntity/Stack/NBT child. Zero denotes native
 * admission or an unreached source frame. No owner or result is retained. */
typedef struct mc_world_game_save_error {
    mc_world_storage_status status;
    mc_world_storage_error cause;
    unsigned int region_bytecode, serializer_bytecode;
    size_t entity_slice, entity_index;
    mc_entity_nbt_error entity;
    /* Native collection/tree assembly, separate from an actual Entity call. */
    mc_nbt_error entity_records;
    mc_live_tile_entity_error tile_entity;
    mc_stored_chunk_error composition;
    mc_nbt_error encoding;
    mc_world_player_metadata_error metadata;
} mc_world_game_save_error;

typedef struct mc_world_runtime_lifecycle_error {
    mc_chunk_lifecycle_status status;
    mc_world_storage_error cause;
    mc_world_game_save_error save;
    /* Actual maintenance activation/deferral child, independent of save.
     * Fresh batch.index is SIZE_MAX; caller/callee PCs remain distinct. */
    mc_live_tile_entity_error tile_entity;
    mc_world_entity_batch_error batch;
    unsigned int activation_bytecode, activation_world_bytecode;
} mc_world_runtime_lifecycle_error;

#endif
