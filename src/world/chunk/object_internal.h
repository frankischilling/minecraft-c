#ifndef MC_WORLD_CHUNK_OBJECT_INTERNAL_H
#define MC_WORLD_CHUNK_OBJECT_INTERNAL_H

#include "mc/world/chunk/object.h"

/* Lower concrete storage, called only by the typed store factory/destructor.
 * A nonnull actual World and one exclusive malloc-compatible blocks allocation
 * of EXACTLY32768 bytes are required. No World/Registry method or callback runs.
 * One combined backing request provides the actual state/data/finite child
 * handles and disjoint packed/height spans; this is native allocation policy,
 * not Java object layout or per-object OOM timing. The budget includes that
 * backing and the transferred32768 bytes, excluding borrowed dependencies.
 * Perform the actual constructor field/zero stores in their source order;
 * never clear/copy blocks or call numerical data_init/finish. Success alone
 * consumes *blocks (sets NULL) and publishes out. Failure preserves both.
 * Caller holds World until successful store linkage or failed-child cleanup. */
mc_world_storage_status mc_world_chunk_storage_create(mc_world_runtime *world, int32_t x, int32_t z,
                                                      uint8_t **blocks, size_t length,
                                                      size_t max_allocation_bytes,
                                                      mc_world_chunk **out,
                                                      mc_world_storage_error *error);
void mc_world_chunk_storage_free(mc_world_chunk *chunk);

/* Construct finite known-empty loaded Chunk headers over the SAME stored
 * state/data/arrays. The store has already decoded the fixed shape and rejected
 * opaque entity/tile payloads. No borrowed cell is cleared, copied or rebound;
 * source NBT remains owned by the stored serializer. Header backing alone is
 * charged to this native budget. Failure preserves out and borrowed storage.
 * Concrete j/k stay final from binding onward; generic unbound stored owners
 * retain their separate coordinate-edit contract. Free this header before
 * retiring its borrowed state/data/arrays. */
mc_world_storage_status mc_world_chunk_storage_bind_loaded(mc_world_runtime *world,
                                                           mc_chunk_state *state,
                                                           size_t max_allocation_bytes,
                                                           mc_world_chunk **out,
                                                           mc_world_storage_error *error);

/* Validate the actual finite empty child/binding contract for serialization.
 * No class use, source World access, callback, propagation or state mutation. */
bool mc_world_chunk_storage_empty(const mc_world_chunk *chunk);

#endif
