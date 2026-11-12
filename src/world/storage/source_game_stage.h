#ifndef MC_WORLD_STORAGE_SOURCE_GAME_STAGE_H
#define MC_WORLD_STORAGE_SOURCE_GAME_STAGE_H
#include "mc/world/storage/source_chunk_loader.h"
mc_world_storage_status mc_world_store_source_adopt_game(mc_world_store *, mc_world_runtime *,
                                                         int32_t, mc_nbt_document **,
                                                         mc_world_store_game_stage **,
                                                         mc_world_storage_error *);
const uint8_t *mc_world_store_source_normalization_map(const mc_world_store *);

typedef struct mc_world_source_normalizer {
    uint8_t *map;
    bool *ready, *failed;
    mc_world_source_error *failure;
    mc_registry_error *registry;
} mc_world_source_normalizer;

bool mc_world_store_source_normalizer(mc_world_store *, mc_world_source_normalizer *);
#endif
