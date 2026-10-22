#ifndef MC_WORLD_RUNTIME_POWER_INTERNAL_H
#define MC_WORLD_RUNTIME_POWER_INTERNAL_H

#include "../runtime_internal.h"
#include "mc/world/runtime/power/power.h"

mc_world_power_error mc_world_power_empty_error(void);

/* Actual World bodies. Caller holds/enters the same live GAME World; no new
 * entry or hold is acquired. Each held entry returns a fresh diagnostic frame;
 * private nested kernels preserve their explicit enclosing World call sites.
 * out/error are disjoint from owners and input views; failure preserves out.
 * error may be NULL. No source flag, Registry or future-neighbor preflight. */
mc_world_storage_status mc_world_power_neighbors_held(mc_world_runtime *world, int32_t x, int32_t y,
                                                      int32_t z, bool *out,
                                                      mc_world_power_error *error);
mc_world_storage_status mc_world_power_at_held(mc_world_runtime *world, int32_t x, int32_t y,
                                               int32_t z, int32_t side, bool *out,
                                               mc_world_power_error *error);
mc_world_storage_status mc_world_power_strong_neighbors_held(mc_world_runtime *world, int32_t x,
                                                             int32_t y, int32_t z, bool *out,
                                                             mc_world_power_error *error);
mc_world_storage_status mc_world_power_strong_at_held(mc_world_runtime *world, int32_t x, int32_t y,
                                                      int32_t z, int32_t side, bool *out,
                                                      mc_world_power_error *error);

/* Concrete Block bodies use the receiver's actual defining class namespace.
 * The caller keeps Block/classes live/exclusive and holds/enters a nonnull
 * World through any reached source getter. World is used only at that getter;
 * guarded bodies admit NULL without any World entry. No constructor/READY/class active
 * use is implied by an instance invocation. NULL receiver fails with the body
 * unentered. Each entry returns a fresh frame; private strong-to-weak and wire
 * helper calls preserve their current enclosing Block sites. The World caller
 * merges a failed Block child with its own retained World sites. */
mc_world_storage_status mc_world_block_power_capability_held(const mc_block *block, bool *out,
                                                             mc_world_power_error *error);
mc_world_storage_status mc_world_block_strong_power_held(mc_world_runtime *world,
                                                         const mc_block *block, int32_t x,
                                                         int32_t y, int32_t z, int32_t side,
                                                         bool *out, mc_world_power_error *error);
mc_world_storage_status mc_world_block_weak_power_held(mc_world_runtime *world,
                                                       const mc_block *block, int32_t x, int32_t y,
                                                       int32_t z, int32_t side, bool *out,
                                                       mc_world_power_error *error);

#endif
