#ifndef MC_WORLD_CHUNK_OBJECT_INTERNAL_H
#define MC_WORLD_CHUNK_OBJECT_INTERNAL_H

#include "mc/entity/entity.h"
#include "mc/world/chunk/entity_errors.h"
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

/* Construct initially empty loaded Chunk headers over the SAME stored
 * state/data/arrays. The store has decoded the fixed transport shape; its load
 * protocol owns semantic imports and normalization. No borrowed cell is cleared, copied or rebound;
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

/* Actual normal lm/hi.a(Entity)V only. Caller holds the exact concrete Chunk,
 * supplying Store and live Entity (when nonnull) across this synchronous call;
 * a World entry wrapper owns World/Registry/native retirement admission.
 * No READY, Entity ID/side/world, finite coordinate or current cache-key gate
 * is added here. Terminal native container retirement rejects at admission0,
 * before q. A NULL Entity is reached at field6 AFTER q=true2.
 * Blank li/hg.a(Entity)V is a different virtual body: its dispatch returns
 * its literal no-op without resolving a normal header or reading Entity.
 *
 * Mismatch reaches the unimplemented System.out/StringBuilder diagnostic45
 * and stops explicitly; no pretend println/dumpStack or continuation occurs.
 * Each successful duplicate append owns a separate real Entity hold. A late
 * failure preserves q and all completed membership stores. No World callback,
 * RNG, class initialization, collection equality or serializer runs.
 * error may be NULL and must be disjoint from every owner/input view. */
mc_world_storage_status mc_world_chunk_insert_entity(mc_world_chunk *chunk, mc_entity *entity,
                                                     mc_world_chunk_entity_error *error);

mc_world_storage_status mc_world_chunk_remove_entity(mc_world_chunk *chunk, mc_entity *entity,
                                                     int32_t slice, size_t *work, size_t max_work,
                                                     mc_world_chunk_entity_error *error);

/* Borrow an actual indexed occurrence without an extra hold. Valid only while
 * the same live, exclusive List owner remains unchanged. Failure preserves out.
 * This neither merges duplicates nor searches by Entity equality/ID. */
mc_world_storage_status mc_world_chunk_entity_at(const mc_world_chunk_entity_list *list,
                                                 size_t index, mc_entity **out);

/* Native quiescent retirement, NOT game removal/List.clear. The application
 * first validates ALL World/Store/Chunk containers and leases before releasing
 * ANY occurrence. Validation is read-only and verifies actual eight List
 * embedded identities, descriptor coherence, nonnull held entries and exact
 * byte ledger. It never dereferences replaceable source m or l; reached source
 * operations validate those projections independently.
 * Live typed provenance is an owner contract; this cannot detect stale or
 * forged pointers. Validated metadata may not change before release.
 *
 * release is terminal and infallible under that precondition: latch the native
 * marker before releasing even an empty container, release one hold per duplicate
 * occurrence, free capacities, update ledger/reset descriptor storage. It leaves
 * stable List handles, q, Entity bF/bG/bH/bI and every other source field alone;
 * no source WorldAccess, removal or World/container callback runs. The registered
 * native finalizer releases each real hold. Serialization refuses the marker
 * even after count0;
 * native cleanup NEVER enables ordinary maintenance/save of erased game state.
 * Already-retired empty coherent storage can validate/release for final native
 * destruction; it cannot be reused for insertion, activation or saving. NULL
 * release is safe. */
bool mc_world_chunk_entities_validate(const mc_world_chunk *chunk);
void mc_world_chunk_entities_release(mc_world_chunk *chunk);
/* Joint native finalization after the same complete quiescent admission:
 * Entity slices followed by the actual embedded Map, independent of source l.
 * This remains infallible and terminal; no gameplay or source callback runs. */
void mc_world_chunk_containers_release(mc_world_chunk *chunk);

bool mc_world_chunk_entity_containers_retired(const mc_world_chunk *chunk);
bool mc_world_chunk_source_enabled(const mc_world_chunk *chunk);
void mc_world_chunk_source_disable(mc_world_chunk *chunk);

#endif
