#ifndef MC_WORLD_STORAGE_OBJECTS_INTERNAL_H
#define MC_WORLD_STORAGE_OBJECTS_INTERNAL_H

#include "mc/world/chunk/object.h"
#include "mc/world/storage/store.h"

/* GAME load keeps the existing stored state/data and retained NBT document.
 * It rejects opaque entities/tiles and attaches actual known-empty Chunk child
 * headers before publishing the store link. Header allocation is native policy,
 * not original decoder allocation/partial-failure timing. Failure preserves out.
 * World and store must remain live, and store must be this actual World's w;
 * no cache publication or activation occurs. Caller holds the World across
 * this operation and until successful linkage or failed-child cleanup. */
mc_world_storage_status mc_world_store_load_game_chunk(mc_world_store *store,
                                                       mc_world_runtime *world, int32_t dimension,
                                                       int32_t x, int32_t z, mc_chunk_state **out,
                                                       mc_world_storage_error *error);

/* Exact state identity lookup for generated and bound loaded Chunk objects.
 * Generic stored states have no concrete object. No state/data copy or cast. */
mc_world_chunk *mc_world_store_chunk_object(const mc_world_store *store,
                                            const mc_chunk_state *state);

/* Allocation-free actual d guard, including bound loaded objects. Retiring a
 * World is allowed first; every such Chunk must retire before freeing World. */
bool mc_world_store_has_chunks_for_world(const mc_world_store *store,
                                         const mc_world_runtime *world);

#endif
