#ifndef MC_WORLD_STORAGE_GENERATED_INTERNAL_H
#define MC_WORLD_STORAGE_GENERATED_INTERNAL_H

#include "mc/world/chunk/object.h"
#include "mc/world/storage/store.h"

/* Link a successfully constructed actual generated owner into this store.
 * Dimensions0/-1 use existing native admission. Link/count and payload budget
 * failures preserve out and caller blocks ownership. No reserve-token handoff,
 * copy, normalization, finishing, file/session write or cache publication.
 * On success the SAME owner/state/data/blocks are already final store storage.
 * Existing exact-identity leases/cache/active/retirement rules apply. */
mc_world_storage_status mc_world_store_create_generated(mc_world_store *store,
                                                        mc_world_runtime *world, int32_t dimension,
                                                        int32_t x, int32_t z, uint8_t **blocks,
                                                        size_t length, mc_world_chunk **out,
                                                        mc_world_storage_error *error);
mc_world_chunk *mc_world_store_generated_chunk(const mc_world_store *store,
                                               const mc_chunk_state *state);

/* Allocation-free exact-World reference guard. No World dereference/callback.
 * World free must return BUSY before any teardown while this is true. Retiring
 * World is allowed first, but chunks must retire before freeWorld/freeStore.
 * Unknown/null inputs return false/NULL. Store must outlive every World using it. */
bool mc_world_store_has_generated_for_world(const mc_world_store *store,
                                            const mc_world_runtime *world);

#endif
