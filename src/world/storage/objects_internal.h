#ifndef MC_WORLD_STORAGE_OBJECTS_INTERNAL_H
#define MC_WORLD_STORAGE_OBJECTS_INTERNAL_H

#include "mc/storage/nbt/tree.h"
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

typedef struct mc_world_store_game_stage mc_world_store_game_stage;
typedef enum mc_world_store_game_stage_phase {
    MC_WORLD_STORE_GAME_PREPARED,
    MC_WORLD_STORE_GAME_IMPORTING,
    MC_WORLD_STORE_GAME_NORMALIZED,
    MC_WORLD_STORE_GAME_COMMITTED,
    MC_WORLD_STORE_GAME_FAILED
} mc_world_store_game_stage_phase;

typedef struct mc_world_store_game_stage_view {
    mc_world_chunk *chunk;
    mc_chunk_state *state;
    const mc_nbt_tag *level;
    mc_world_store_game_stage_phase phase;
    mc_nbt_limits limits;
} mc_world_store_game_stage_view;

/* Concrete pre-normalization GAME transport, separate from the unchanged
 * known-empty load_game_chunk above. Reserve real Store capacity, decode once
 * with deferred list semantics/source q=false, bind the SAME state/data/arrays,
 * then attach that exact owner to the canonical Store chain. No source factory,
 * normalization, cache publication or activation occurs. Success retains ONE
 * actual coordinator lease; nested source wrappers own their separate leases.
 * The caller holds the actual World and its Store binding across the protocol.
 * Decoded/document and header allocation ceilings remain separate native
 * budgets, not a whole graph/Java heap accounting claim. Failure preserves out.
 * Private preattachment failure alone destroys preparation; every attached
 * stage remains counted and reachable after a reached failure. All output/error
 * storage is mutually disjoint and disjoint from owners, views and dependencies;
 * owner and dependency access remains exclusive through the protocol. */
mc_world_storage_status mc_world_store_game_stage_begin(mc_world_store *store,
                                                        mc_world_runtime *world, int32_t dimension,
                                                        int32_t x, int32_t z,
                                                        mc_world_store_game_stage **out,
                                                        mc_world_storage_error *error);

/* Passive borrowed view over the same retained document/header/state. No
 * semantic list getter runs. Valid handles, including committed/failed handles,
 * borrow Store ownership until retirement. Native source operations on failed
 * headers are terminally disabled independently of source d and retirement. */
mc_world_storage_status mc_world_store_game_stage_view_get(const mc_world_store *store,
                                                           const mc_world_store_game_stage *stage,
                                                           mc_world_store_game_stage_view *out,
                                                           mc_world_storage_error *error);
mc_world_storage_status mc_world_store_game_stage_imports_begin(mc_world_store *store,
                                                                mc_world_store_game_stage *stage,
                                                                mc_world_storage_error *error);

/* Only the concrete simulation coordinator calls finish after BOTH genuine
 * Entity and Tile traversals return normally. Normalize the SAME stored owner,
 * commit, balance only the coordinator lease and publish out. No fallible step,
 * allocation or callback follows normalization. Failure leaves out unchanged
 * and the coordinator lease held for fail; no implicit retry or cancellation. */
mc_world_storage_status mc_world_store_game_stage_finish(mc_world_store *store,
                                                         mc_world_store_game_stage *stage,
                                                         mc_chunk_state **out,
                                                         mc_world_storage_error *error);

/* Terminal failure preserves every escaped prefix, disables direct Chunk
 * source entry and releases ONLY the real coordinator lease. original_status
 * must be non-OK. error may be NULL to retain an earlier complete typed cause.
 * Repeated fail is passive/idempotent and releases no lease twice. */
mc_world_storage_status mc_world_store_game_stage_fail(mc_world_store *store,
                                                       mc_world_store_game_stage *stage,
                                                       mc_world_storage_status original_status,
                                                       mc_world_storage_error *error);

/* Pending/failed stages only, in canonical ownership order; no coordinate/key
 * lookup or source eligibility is introduced. count(NULL)==0. at requires both
 * outputs, preserves both on failure and borrows the handle/view until the
 * owner is retired. COMMITTED stage handles remain usable with view_get. */
size_t mc_world_store_game_stage_count(const mc_world_store *store);
mc_world_storage_status mc_world_store_game_stage_at(const mc_world_store *store, size_t ordinal,
                                                     mc_world_store_game_stage **out,
                                                     mc_world_store_game_stage_view *view,
                                                     mc_world_storage_error *error);
bool mc_world_store_chunk_source_enabled(const mc_world_store *store, const mc_chunk_state *state);

typedef struct mc_world_store_game_save mc_world_store_game_save;
typedef struct mc_world_store_game_save_view {
    mc_world_chunk *chunk;
    mc_chunk_state *state;
    mc_nbt_limits limits;
} mc_world_store_game_save_view;

/* Concrete synchronous GAME save transport. The caller holds the actual World,
 * its Store binding and selected READY Info (when nonnull) exclusively through
 * end. Begin admits the SAME committed/source-enabled concrete Chunk and owns
 * one actual Store identity lease, then checks the session. NULL account uses
 * existing Store metadata. No source field reset, Entity/Tile traversal, class
 * use or callback occurs. A nested begin for this entry is BUSY. Failure balances
 * its temporary lease and preserves both outputs. All outputs/errors must be
 * disjoint from each other, owners, input trees and borrowed dependencies. */
mc_world_storage_status
mc_world_store_game_save_begin(mc_world_store *store, mc_world_runtime *world,
                               mc_chunk_state *state, mc_world_info *account,
                               mc_world_store_game_save **out, mc_world_store_game_save_view *view,
                               mc_world_storage_error *error);

/* Only the concrete high coordinator calls write: after its second actual
 * shared-serializer session check, source prefix, q=false and genuine EMPTY
 * Entity traversal, then actual Tile builder success. It must dereference the
 * reached e/f/g descriptors before q in source order. No hidden session check,
 * source reset, Tile call, old-list replay or owner rebinding occurs here.
 * Compose actual b/e.a/f.a/g.a/h projections using the fixed native extents;
 * nibble descriptors must have exactly the packed extent, nonnull a/h spans
 * must be readable for those extents, and NULL array pointers are left for
 * codec failure after the reached q reset. No canonical array substitution.
 * tile_records is the actual builder's independent root Compound containing
 * only a Compound TileEntities List, borrowed unchanged through the call.
 * Caller retains/frees that tree. Explicit empty Entities and current Tiles
 * replace BOTH baseline lists; generic opaque builders remain unchanged.
 * The codec ceiling charges input tree + outer tree + overlapping scratch/raw
 * output. Retained Store payload/document and live World/Tile graphs retain
 * their separate existing budgets; region budgets remain separate as well.
 * Success alone publishes growth and adds Java-int-narrowed growth to selected
 * Info/metadata. Failure preserves growth and all earlier source effects. */
/* Optional disjoint composition/encoding outputs retain the complete actual
 * lower causes. Both start OK; native owner/buffer/region failures do not
 * manufacture codec errors for operations that were never reached. */
mc_world_storage_status mc_world_store_game_save_write(
    mc_world_store *store, mc_world_store_game_save *save, int64_t world_time, uint32_t timestamp,
    const mc_nbt_tree *tile_records, uint32_t *growth, mc_world_storage_error *error,
    mc_stored_chunk_error *composition_error, mc_nbt_error *encoding_error);

/* Balance ONLY this save's lease and invalidate its borrowed handle. No source
 * validation is repeated during cleanup; a reached nullable source projection
 * must not prevent cleanup. No source disable, retirement, rollback or implicit
 * retry occurs. Valid begin/end pairs are infallible under exclusive unchanged
 * native ownership. Always end exactly once after successful begin, including
 * source/build/write failure. A save handle is invalid after end. */
mc_world_storage_status mc_world_store_game_save_end(mc_world_store *store,
                                                     mc_world_store_game_save *save,
                                                     mc_world_storage_error *error);

/* Exact state identity lookup for generated and bound loaded Chunk objects.
 * Generic stored states have no concrete object. No state/data copy or cast. */
mc_world_chunk *mc_world_store_chunk_object(const mc_world_store *store,
                                            const mc_chunk_state *state);

/* Allocation-free actual d guard, including bound loaded objects. Retiring a
 * World is allowed first; every such Chunk must retire before freeing World. */
bool mc_world_store_has_chunks_for_world(const mc_world_store *store,
                                         const mc_world_runtime *world);

/* Whole-Store native teardown admission, followed only after complete World
 * admission by infallible per-occurrence Entity/Tile release. The actual embedded
 * Map is validated through its native inspector, independently of source l.
 * This does not save, unload,
 * remove cache identities or reset source flags. Retired containers cannot
 * return to gameplay/serialization; unknown stored NBT remains untouched. */
mc_world_storage_status mc_world_store_entity_containers_validate(const mc_world_store *store,
                                                                  const mc_world_runtime *world,
                                                                  mc_world_storage_error *error);
void mc_world_store_entity_containers_release(mc_world_store *store, const mc_world_runtime *world);

#endif
