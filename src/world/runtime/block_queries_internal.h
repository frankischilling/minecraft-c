#ifndef MC_WORLD_RUNTIME_BLOCK_QUERIES_INTERNAL_H
#define MC_WORLD_RUNTIME_BLOCK_QUERIES_INTERNAL_H

#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/collision/entities.h"

typedef struct mc_world_solid_support_detail {
    mc_world_solid_support_error error;
    mc_world_nearby_error access;
} mc_world_solid_support_detail;

/* Same concrete solid-support body and captured Block.m order as the public
 * query. Caller already holds/enters this live GAME World. The reached ID read
 * uses the held reader and retains its complete acquisition descendants. The
 * public query keeps its existing nested getter entry and storage-only cause.
 * No extra source read, retry, array snapshot or class preflight is introduced.
 * Failure preserves out; each call returns fresh detail, which may be NULL. */
mc_world_storage_status mc_world_solid_support_detailed_held(mc_world_runtime *world, int32_t x,
                                                             int32_t y, int32_t z, bool *out,
                                                             mc_world_solid_support_detail *detail);

#endif
