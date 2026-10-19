#ifndef MC_WORLD_RUNTIME_BLOCKS_H
#define MC_WORLD_RUNTIME_BLOCKS_H

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
 * point; a later owner failure retains the dirty assignment. No block hook, observer,
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

#endif
