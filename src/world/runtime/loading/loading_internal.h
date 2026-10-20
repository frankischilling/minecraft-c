#ifndef MC_WORLD_RUNTIME_LOADING_INTERNAL_H
#define MC_WORLD_RUNTIME_LOADING_INTERNAL_H

#include "mc/world/runtime/loading/entity_errors.h"
#include "mc/world/runtime/runtime.h"
#include "mc/world/runtime/tiles/errors.h"

typedef struct mc_world_game_load_error {
    mc_world_storage_status status;
    mc_world_storage_error cause;
    unsigned int region_bytecode, builder_bytecode;
    mc_live_tile_error tile;
    mc_world_entity_load_error entity;
} mc_world_game_load_error;

/* Concrete GAME coordinator above persistence: the same Store-owned decoded
 * state/header remains leased through Entity then Tile import. Normalization
 * and ordinary loader output occur only after both loops finish. Failed stages
 * remain owned and inspectable, with their completed source prefixes intact.
 * Item records use the actual bound factory and direct Chunk insertion. Caller holds
 * exclusive live dependencies; output/error are disjoint from all owners. */
mc_world_storage_status mc_world_runtime_load_game(mc_world_runtime *world, int32_t x, int32_t z,
                                                   mc_chunk_state **out,
                                                   mc_world_game_load_error *error);

#endif
