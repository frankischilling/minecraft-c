#ifndef MC_WORLD_RUNTIME_CHUNK_TICK_H
#define MC_WORLD_RUNTIME_CHUNK_TICK_H

#include "mc/world/runtime/lighting.h"

typedef struct mc_world_chunk_tick_capture mc_world_chunk_tick_capture;

typedef struct mc_world_chunk_tick_site {
    const char *class_name, *method, *descriptor;
    int32_t pc;
} mc_world_chunk_tick_site;

typedef struct mc_world_chunk_tick_error {
    mc_world_storage_status status;
    const char *operation, *exception_class;
    mc_world_chunk_tick_site site, child;
    bool native_policy, source_exception;
    int32_t packed_index, array_index;
    mc_world_storage_error cause;
} mc_world_chunk_tick_error;

typedef enum mc_world_chunk_tick_operation {
    MC_WORLD_CHUNK_TICK_NONE,
    MC_WORLD_CHUNK_TICK_BLOCK,
    MC_WORLD_CHUNK_TICK_METADATA,
    MC_WORLD_CHUNK_TICK_SAVED_LIGHT,
    MC_WORLD_CHUNK_TICK_RAW_BLOCK
} mc_world_chunk_tick_operation;

typedef struct mc_world_chunk_tick_state {
    mc_world_runtime *world;
    mc_chunk_state *captured;
    bool blank, world_held, chunk_held, ready, active;
    int32_t chunk_x, chunk_z, packed_index, array_index;
    size_t reads;
    mc_world_chunk_tick_operation operation;
    mc_world_chunk_tick_site site, child;
    const mc_world_chunk_tick_error *failure; /* Borrowed until capture retirement. */
} mc_world_chunk_tick_state;

/* Capture the exact acquired normal Chunk or this GAME World's constructed,
 * installed embedded Blank. No getter, requested-coordinate lookup, source
 * Chunk.d read, block/Nibble read, class use or Random operation occurs here.
 * Native provenance requires the supplying client class/store/context graph.
 * One World call and one actual references.retain occurrence protect the local
 * across callbacks, cache replacement and any number of later reads.
 *
 * Publish *out once native handle storage exists, including a later failed
 * World/Chunk hold. Such a partial capture remains inspectable and MUST be
 * retired. Earlier admission/allocation failure preserves *out. Source fields
 * are checked only by the operation that reaches them, including NULL b.
 * Owners, out, errors and operation outputs must be live/exclusive/disjoint. */
mc_world_storage_status mc_world_chunk_tick_capture_begin(mc_world_runtime *supplying_world,
                                                          mc_chunk_state *captured,
                                                          mc_world_chunk_tick_capture **out,
                                                          mc_world_chunk_tick_error *error);
bool mc_world_chunk_tick_capture_inspect(const mc_world_chunk_tick_capture *,
                                         mc_world_chunk_tick_state *out);

/* Actual client virtual LM/LI methods on the captured local. Coordinates are
 * source-local int operands: full Java shifts/OR/indexing, never clamps or
 * local masks. Normal methods reload current source fields on every call.
 * Blank overrides return literal zero even for invalid coordinates/NULL b or
 * Nibbles, and saved-light does not initialize Light for that override.
 * A nonnull Light operand must be an actual same-class handle; NULL is a valid
 * source operand and may equal a reached NULL shared Light static.
 * Failure preserves out and ownership; no source rollback or pool reset runs. */
mc_world_storage_status mc_world_chunk_tick_block(mc_world_chunk_tick_capture *, int32_t x,
                                                  int32_t y, int32_t z, int32_t *out,
                                                  mc_world_chunk_tick_error *error);
mc_world_storage_status mc_world_chunk_tick_metadata(mc_world_chunk_tick_capture *, int32_t x,
                                                     int32_t y, int32_t z, int32_t *out,
                                                     mc_world_chunk_tick_error *error);
mc_world_storage_status mc_world_chunk_tick_saved_light(mc_world_chunk_tick_capture *,
                                                        const mc_world_light_type *, int32_t x,
                                                        int32_t y, int32_t z, int32_t *out,
                                                        mc_world_chunk_tick_error *error);

/* FD.n's raw b field/load at927/944, followed by unsigned &255. This deliberately
 * does not dispatch LI.a: an installed Blank reads its actual current byte[]
 * too. NULL b and packed-index exceptions occur at944; no successful empty
 * fallback, virtual block getter, copied bytes or reacquisition is substituted. */
mc_world_storage_status mc_world_chunk_tick_raw_block(mc_world_chunk_tick_capture *, int32_t x,
                                                      int32_t y, int32_t z, int32_t *out,
                                                      mc_world_chunk_tick_error *error);

/* Native terminal cleanup only. Active/reentered access returns BUSY and keeps
 * the handle and every hold retryable. Success releases the exact captured
 * reference hooks, then the World call, frees/nulls this occurrence. NULL *slot
 * succeeds. No source field repair, reacquisition or failed read replay occurs.
 * Error is optional on every entry. */
mc_world_storage_status mc_world_chunk_tick_capture_retire(mc_world_chunk_tick_capture **slot,
                                                           mc_world_chunk_tick_error *error);

#endif
