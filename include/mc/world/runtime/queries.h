#ifndef MC_WORLD_RUNTIME_QUERIES_H
#define MC_WORLD_RUNTIME_QUERIES_H

#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/lighting.h"

typedef struct mc_world_query_error {
    mc_world_storage_status status;
    const char *operation;
    unsigned int bytecode, provider_bytecode, chunk_bytecode;
    mc_chunk_acquire_status acquisition;
    mc_world_storage_error cause;
    mc_registry_error registry;
    mc_world_population_context population;
} mc_world_query_error;

/* Actual fd.e/dj.e(II)I: acquire one Chunk before masking coordinates, then
 * scan y=127 down through 1. Each nonair block selects the live Block.m entry
 * and its Material; air selects actual Material.a without Block active use.
 * Invoke Material.c, then d only when c is false. Return the first y+1, or -1.
 * There is no World border gate, height-map read or inspection of y=0.
 * The exact acquired normal Chunk is leased across Registry initialization
 * and synchronous reentry; subsequent iterations retain that same identity. */
mc_world_storage_status mc_world_runtime_top_solid_liquid(mc_world_runtime *world, int32_t x,
                                                          int32_t z, int32_t *out,
                                                          mc_world_query_error *error);

/* Actual fd.a(eb;III)/dj.a(co;III): clamp y to [0,127], then apply the World
 * asymmetric X/Z border. Outside, return the actual nullable type's c field
 * (NULL fails there). Inside, absence returns zero without acquisition or
 * Light active use. A present entry acquires the current Chunk. Normal Chunk
 * compares the actual live shared Sky, then Block identities and reads the
 * selected Nibble; NULL ordinarily matches neither and returns zero. Blank
 * returns zero without Light active use. type must be a same-class handle or
 * NULL; foreign handles are native admission errors, without dereferencing. */
mc_world_storage_status mc_world_runtime_saved_light(mc_world_runtime *world,
                                                     const mc_world_light_type *type, int32_t x,
                                                     int32_t y, int32_t z, int32_t *out,
                                                     mc_world_query_error *error);

/* Actual fd.m/dj.k(III)I used by plant survival. Negative y returns zero
 * before acquisition; y>=128 clamps to 127. There is no X/Z border gate.
 * Normal Chunk reads SkyLight, sets its real shared class field a=true when
 * positive, then reads BlockLight and returns the larger value. The class
 * write survives a later BlockLight failure. Blank returns zero. World.f
 * sky subtraction, slab/farmland neighbor sampling and float brightness are
 * different source methods and are not substituted for this operation. */
mc_world_storage_status mc_world_runtime_plant_light(mc_world_runtime *world, int32_t x, int32_t y,
                                                     int32_t z, int32_t *out,
                                                     mc_world_query_error *error);

/* Actual fd.n/dj.l(III)I and their a(IIIZ)I body. Apply the asymmetric X/Z
 * border first, returning 15 outside even for negative y. Inside, read the
 * target block before testing the live al/aB/aI/au statics in source order.
 * A match samples above,+X,-X,+Z,-Z independently with neighbor sampling off
 * and returns their maximum. Each sample repeats the border and Y gates.
 * Otherwise negative y returns zero; y>=128 clamps to 127 before acquisition.
 * Normal Chunk reads SkyLight, writes its shared a=true when raw sky is
 * positive, subtracts the World.f captured after acquisition with int wrapping,
 * then reads BlockLight and returns the larger value. Blank returns zero.
 * Native failure preserves out and all completed earlier sample effects.
 * bytecode identifies the reached a(IIIZ)I operation; a failed sample retains
 * its own reached site and nested provider/Chunk cause. No light propagation,
 * saved-light presence gate or plant-light substitute is performed. */
mc_world_storage_status mc_world_runtime_block_light(mc_world_runtime *world, int32_t x, int32_t y,
                                                     int32_t z, int32_t *out,
                                                     mc_world_query_error *error);

/* Actual fd.l/dj.j(III)Z. Acquire even for negative/large y or X/Z outside
 * the bounded block getter's domain. Normal Chunk compares y against its
 * current unsigned height byte at (localZ<<4)|localX; Blank returns false. */
mc_world_storage_status mc_world_runtime_sky_visible(mc_world_runtime *world, int32_t x, int32_t y,
                                                     int32_t z, bool *out,
                                                     mc_world_query_error *error);

/* Passive actual lm/hi.a view. This cell belongs to the shared class owner,
 * starts false once, and is not reset by World or Chunk construction. NULL
 * returns false. No class initialization, query or renderer reset occurs. */
bool mc_world_runtime_chunk_light_seen(const mc_world_runtime_classes *classes);

/* All query operations hold the live GAME World throughout the call, admitting
 * RAW/constructing/failed prefixes but rejecting retirement and COMPOSITION.
 * Keep owners and arguments/output/error storage live, exclusive and disjoint.
 * Native failure preserves out and all reached source effects. Error records
 * outer World, nested provider/Chunk and full Registry causes when reached;
 * admission changes only error. Success clears this call's error frame, not
 * prior World diagnostics. No implicit save, light propagation or population
 * substitute occurs. These operations use actual installed provider/cache
 * acquisition, including its real blank fallback and packed-key aliases. */

#endif
