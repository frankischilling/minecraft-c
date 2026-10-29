#ifndef MC_WORLD_RUNTIME_BLOCKS_H
#define MC_WORLD_RUNTIME_BLOCKS_H

#include "mc/world/runtime/acquisition_error.h"
#include "mc/world/runtime/construction.h"

/* Actual GAME World block reads through its installed provider/cache. The
 * x domain is [-32000000,32000000), z is [-32000000,32000000], and y is
 * [0,127]. Outside returns air without acquisition. Inside, signed arithmetic
 * chunk shifts and local bit masks select the actual acquired chunk payload.
 * Missing generation/activation capabilities fail explicitly, retaining prior
 * acquisition effects; an absent cached chunk is never treated as air.
 * The owner may still be constructing or have a retained failed prefix, but
 * retirement rejects. Failure preserves out. Exclusive access and disjoint
 * output/error storage are required. No implicit save occurs. */
mc_world_storage_status mc_world_runtime_block_get(mc_world_runtime *runtime, int32_t x, int32_t y,
                                                   int32_t z, int32_t *out,
                                                   mc_world_storage_error *error);

/* SAME block-read implementation and source ordering, with the complete
 * acquisition child copied directly from its reached invocation. The required
 * acquisition frame is caller-owned/disjoint (use heap storage for enclosing
 * retained client calls). No discarded-cause reconstruction or second read.
 * Before acquisition it reports OK/unreached with fresh child diagnostics;
 * error retains ordinary block/storage failures, and out stays unchanged on
 * failure exactly as in block_get. Child source-loader views borrow the live
 * World/ld through their next mutation/retirement. No extra source operation. */
mc_world_storage_status
mc_world_runtime_block_get_detailed(mc_world_runtime *, int32_t x, int32_t y, int32_t z,
                                    int32_t *out, mc_world_storage_error *error,
                                    mc_world_runtime_acquisition_error *acquisition);

/* Same admission and real acquisition as block_get. Outside returns zero;
 * the actual blank-chunk getter also returns zero. A normal chunk reads its
 * packed metadata, with even Y in the low nibble and odd Y in the high.
 * Native failure preserves out and any earlier acquisition effects. */
mc_world_storage_status mc_world_runtime_metadata_get(mc_world_runtime *runtime, int32_t x,
                                                      int32_t y, int32_t z, int32_t *out,
                                                      mc_world_storage_error *error);

/* The original quiet metadata setter returns false outside the World domain
 * and true after an in-domain chunk call, including an equal-value write or
 * the actual blank chunk's no-op. The bool is an ordinary game result, separate
 * from native status. Acquisition occurs before any metadata inspection.
 *
 * After successful acquisition, a normal chunk is marked dirty before its
 * metadata owner is accessed; only the selected nibble receives metadata's low
 * four bits. Acquisition can reject a malformed native chunk before this
 * point; a later owner failure retains the dirty assignment. No block hook, WorldAccess,
 * neighbor, height/light change or lighting submission occurs in this quiet
 * path. The blank payload and its dirty flag remain unchanged. Native failure
 * preserves out and retains completed acquisition/dirty effects. The same
 * exclusive-access, lifetime and disjoint-output rules as block_get apply. */
mc_world_storage_status mc_world_runtime_metadata_set_quiet(mc_world_runtime *runtime, int32_t x,
                                                            int32_t y, int32_t z, int32_t metadata,
                                                            bool *out,
                                                            mc_world_storage_error *error);

/* Start at y=63, acquire/read y+1 until air, then read y at that same x/z.
 * This is the original top-block operation, not a height-map query or a
 * downward search. The bounded World getter supplies air above height127. */
mc_world_storage_status mc_world_runtime_top_block(mc_world_runtime *runtime, int32_t x, int32_t z,
                                                   int32_t *out, mc_world_storage_error *error);

/* Acquire the actual block first. Air returns the shared Material.a identity
 * without Block initialization; nonair performs Block active use, indexes the
 * actual m array, then reads the selected Block.bA. A NULL material field is a
 * successful nullable result. A NULL Block, failed initializer or absent class
 * binding fails explicitly, preserving out and prior acquisition effects.
 * Registry failures also retain their full cause in last_registry_error. */
mc_world_storage_status mc_world_runtime_material_get(mc_world_runtime *runtime, int32_t x,
                                                      int32_t y, int32_t z, mc_material **out,
                                                      mc_world_storage_error *error);

typedef struct mc_world_material_error {
    mc_world_storage_status status;
    const char *operation;
    unsigned int bytecode;
    mc_world_storage_error cause;
    mc_registry_error registry;
} mc_world_material_error;

/* The same actual material operation, with a fresh nested Registry frame and
 * separate source PCs for block query4, air Material14, Block.m active use18,
 * array element23 and bA24. A block-query failure does not copy stale World
 * Registry diagnostics. Ordinary output and ownership rules match material_get;
 * both interfaces execute one shared implementation. Success retains the
 * existing material operation's World Registry/storage diagnostic behavior. */
mc_world_storage_status mc_world_runtime_material_get_detailed(mc_world_runtime *runtime, int32_t x,
                                                               int32_t y, int32_t z,
                                                               mc_material **out,
                                                               mc_world_material_error *error);

typedef struct mc_world_solid_support_error {
    mc_world_storage_status status;
    const char *operation;
    unsigned int bytecode;
    mc_world_storage_error cause;
    mc_registry_error registry;
} mc_world_solid_support_error;

/* Actual fd.h/dj.e(III)Z. Capture live Block.m before acquiring the block,
 * then index that same array without narrowing or an air/bounds preflight.
 * A NULL Block returns false. Otherwise invoke its current Material.h and,
 * only when true, its actual virtual Block.d/client or b/server predicate.
 * The slab predicate reads its live own field; opacity is a different call.
 *
 * Hold the actual World and its bound Registry across synchronous acquisition.
 * GAME raw/constructing/failed prefixes are admitted; retirement rejects.
 * Require exclusive live owners and disjoint out/error/dependency storage.
 * Failure preserves out and every reached initialization/acquisition effect.
 * Getter failures retain their actual storage cause; they do not manufacture
 * a new Registry frame from earlier World diagnostics. Native admission leaves
 * earlier World diagnostics intact. error may be NULL; out is required. */
mc_world_storage_status mc_world_runtime_solid_support_get(mc_world_runtime *world, int32_t x,
                                                           int32_t y, int32_t z, bool *out,
                                                           mc_world_solid_support_error *error);

#endif
