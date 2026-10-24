#ifndef MC_WORLD_STORAGE_CHUNK_BUILD_INTERNAL_H
#define MC_WORLD_STORAGE_CHUNK_BUILD_INTERNAL_H

#include "mc/storage/nbt/tree.h"
#include "mc/world/chunk/object.h"
#include "mc/world/storage/chunk.h"

/* Same actual decoded owner/document/fixed arrays as the generic decoder, but
 * defer ALL Entities/TileEntities semantic getter/cast checks to their reached
 * GAME coordinator phases. Source q starts false; opaque list provenance is
 * not a successful Entity factory or Chunk insertion. Generic decode remains
 * unchanged. This retains the existing fixed-array/native codec allocation
 * policy, not original missing-array repair or per-allocation failure timing. */
mc_stored_chunk_status mc_stored_chunk_decode_game(const uint8_t *input, size_t length,
                                                   const mc_nbt_limits *limits,
                                                   mc_stored_chunk **out, size_t *consumed,
                                                   mc_stored_chunk_error *error);

/* Passive ownership-shape admission. No normalization, allocation or source
 * access; useful before checked teardown of the actual retained payload. */
bool mc_stored_chunk_storage_valid(const mc_stored_chunk *chunk);

/* Build the existing eleven-field fresh schema directly from ACTUAL live
 * state/data, without constructing/copying a mc_stored_chunk owner. Requires
 * normal fixed data and no entity/TileEntity records; actual generated children are
 * checked by the store dispatcher. Preserve existing scratch/result budgets,
 * exact field types/order and output-failure contracts. No state mutation. */
mc_stored_chunk_status mc_stored_chunk_build_fresh(const mc_chunk_state *state, int64_t world_time,
                                                   const mc_nbt_limits *limits,
                                                   mc_nbt_document **out,
                                                   mc_stored_chunk_error *error);

/* Explicit GAME composition after genuine Entity traversal and live TileEntity
 * serialization. stored is the SAME payload owning state, or NULL for generated
 * state; chunk is the exact Store-held concrete header borrowing that state.
 * Current TileEntities replace the baseline TileEntity list. For Entities, baseline_entities
 * is the SAME retained nonempty List and consumed has exactly one completed
 * factory/adoption result per baseline occurrence. Replace consumed positions
 * with current serialized records in order, drop surplus consumed positions,
 * append surplus current records, and retain false/unknown records in relative
 * order. Empty/missing baseline requires NULL witness/consumed and count zero.
 * This is native opaque storage preservation, not original source replay or
 * live Entity substitution. Preserve other root/Level unknown fields.
 * Read actual b/e.a/f.a/g.a/h projections, not canonical replacement arrays.
 * Caller has reached e/f/g dereferences before q and holds every nonnull span
 * readable for fixed native extents. Packed descriptor lengths must be exact;
 * NULL a/h reaches normal tree/codec failure. No source mutation/normalization,
 * old-list semantic replay or generic opaque-policy relaxation occurs.
 * limits bounds ALL pointer scratch plus new independent result; caller deducts
 * BOTH simultaneously live input trees first. Failure preserves out. */
mc_stored_chunk_status mc_stored_chunk_build_game_records(
    const mc_stored_chunk *stored, const mc_chunk_state *state, const mc_world_chunk *chunk,
    int64_t world_time, const mc_nbt_tag *entity_records, const mc_nbt_tag *tile_entity_records,
    const mc_nbt_tag *baseline_entities, const bool *consumed, size_t consumed_count,
    const mc_nbt_limits *limits, mc_nbt_tree **out, mc_stored_chunk_error *error);

#endif
