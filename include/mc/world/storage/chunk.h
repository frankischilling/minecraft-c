#ifndef MC_WORLD_STORAGE_CHUNK_H
#define MC_WORLD_STORAGE_CHUNK_H

#include "mc/storage/nbt/nbt.h"
#include "mc/world/chunk/state.h"

typedef struct mc_stored_chunk mc_stored_chunk;

typedef enum mc_stored_chunk_status {
    MC_STORED_CHUNK_OK = 0,
    MC_STORED_CHUNK_INVALID_ARGUMENT,
    MC_STORED_CHUNK_MISSING_LEVEL,
    MC_STORED_CHUNK_MISSING_BLOCKS,
    MC_STORED_CHUNK_INVALID_FIELD,
    MC_STORED_CHUNK_INVALID_SHAPE,
    MC_STORED_CHUNK_UNSUPPORTED_EDIT,
    MC_STORED_CHUNK_LIMIT,
    MC_STORED_CHUNK_ALLOCATION_FAILED,
    MC_STORED_CHUNK_NBT_ERROR
} mc_stored_chunk_status;

typedef struct mc_stored_chunk_error {
    mc_stored_chunk_status status;
    const char *field; /* Static ASCII key or NULL. */
    mc_nbt_error nbt;
} mc_stored_chunk_error;

const char *mc_stored_chunk_status_string(mc_stored_chunk_status status);

/* Parse one uncompressed region NBT record and own all retained state. Requires
 * Level and Level.Blocks, exact original field types, and the supported fixed
 * arrays: Blocks32768, Data/SkyLight/BlockLight16384, HeightMap256. Rejection of
 * absent/short/oversized arrays is an explicit bounded C departure; the original
 * imports empty/malformed arrays without these checks. No repair or finishing
 * occurs. LastUpdate is ignored even if it has another tag type. Coordinates
 * come from Level and are never changed to a requested region slot.
 *
 * Loaded minimum, active, dirty, never_save and last_save_time start at zero;
 * has_entities is true when the serialized Entities list is nonempty. Nonempty
 * Entities/TileEntities must contain compounds and are retained opaquely,
 * including unknown IDs. No entity factory, attachment or simulation runs.
 * Registry normalization is a separate operation below.
 *
 * NULL limits selects NBT defaults. max_allocation_bytes includes this owner
 * and its source NBT document. Borrowed input is excluded. out is required;
 * optional consumed reports one record's end, leaving trailing bytes to the
 * caller. Failure leaves out/consumed unchanged. */
mc_stored_chunk_status mc_stored_chunk_decode(const uint8_t *input, size_t length,
                                              const mc_nbt_limits *limits, mc_stored_chunk **out,
                                              size_t *consumed, mc_stored_chunk_error *error);

/* Copy a normal caller-owned chunk into stable owned storage for a new record.
 * Requires valid fixed-shape data, blank == false and has_entities == false.
 * This operation cannot invent entity/TileEntity records. All transient flags are
 * copied but only coordinates, arrays and population are serialized. Limits
 * bound the owner allocation; NBT result limits are applied by build. */
mc_stored_chunk_status mc_stored_chunk_create(const mc_chunk_state *state,
                                              const mc_nbt_limits *limits, mc_stored_chunk **out,
                                              mc_stored_chunk_error *error);

/* Owner and embedded state/data/blocks remain at stable addresses until free.
 * The mutable view permits data and scalar changes under exclusive access.
 * Do not replace its data/blocks pointers or block capacity. Source NBT views
 * remain immutable and owned until free, independently of runtime edits.
 * NULL owner returns NULL. Retire all cache/list references before free. */
mc_chunk_state *mc_stored_chunk_state(mc_stored_chunk *chunk);
const mc_nbt_tag *mc_stored_chunk_root(const mc_stored_chunk *chunk);

/* Apply a caller-supplied frozen registry table, as the original postload step
 * does: map[i] must be i or zero for every first256 entries. A longer suffix is
 * ignored. map must be disjoint from storage owned by chunk. Failure performs
 * no mutation. Success changes only block bytes,
 * without rebuilding heights/lights, marking dirty or sampling a live registry. */
mc_stored_chunk_status mc_stored_chunk_normalize(mc_stored_chunk *chunk, const uint8_t *map,
                                                 size_t length);

/* Build an independent owned NBT document. LastUpdate is explicit world_time;
 * it does not change last_save_time or any other runtime field. The fresh
 * original schema is overlaid on retained root/Level state, preserving unknown
 * children/root name and opaque entity/TileEntity lists. New owners emit empty lists.
 * Normal NBT writer canonicalization applies; compound byte order is retained.
 *
 * Until entity/TileEntity persistence is implemented, nonempty opaque lists permit
 * only unchanged coordinates/arrays/population and a new LastUpdate. Other
 * edits return UNSUPPORTED_EDIT instead of silently invalidating unsupported
 * state. Changing has_entities inconsistently with retained records also fails.
 * An owner with no opaque records supports normal array/coordinate edits.
 *
 * max_allocation_bytes bounds peak new heap storage during this call, excluding
 * the preexisting owner. Result is independent of the owner and freed with
 * mc_nbt_document_free. out is required and unchanged on failure. All source,
 * limit and result storage must be disjoint. No compression, file access,
 * session check, save eligibility, dirty reset or cache time stamp occurs. */
mc_stored_chunk_status mc_stored_chunk_build(const mc_stored_chunk *chunk, int64_t world_time,
                                             const mc_nbt_limits *limits, mc_nbt_document **out,
                                             mc_stored_chunk_error *error);
void mc_stored_chunk_free(mc_stored_chunk *chunk);

#endif
