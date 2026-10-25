#ifndef MC_WORLD_RUNTIME_PROPAGATION_H
#define MC_WORLD_RUNTIME_PROPAGATION_H

#include "mc/world/runtime/entity_errors.h"
#include "mc/world/runtime/queries.h"
#include "mc/world/runtime/tile_entities/errors.h"

typedef struct mc_world_light_acquisition_error {
    mc_chunk_acquire_status status;
    mc_world_storage_error cause;
    mc_registry_error registry;
    mc_live_tile_entity_error tile_entity;
    mc_world_entity_batch_error batch;
    unsigned int activation_bytecode, activation_world_bytecode;
    unsigned int load_bytecode, load_builder_bytecode;
    mc_world_entity_load_error load_entity;
    mc_world_population_context population;
} mc_world_light_acquisition_error;

/* Fresh nested source sites; zero means that frame was not reached. World
 * methods/PCs are the pinned client fd bodies; region is st.a(fd). Existing
 * query/submit/provider/Registry frames retain their original details. */
typedef struct mc_world_light_operation_error {
    mc_world_storage_status status;
    const char *operation;
    unsigned int drain_bytecode, region_bytecode, point_bytecode;
    unsigned int setter_bytecode, sky_bytecode, presence_bytecode;
    unsigned int provider_bytecode, chunk_bytecode, world_access_bytecode;
    mc_world_storage_error cause;
    mc_world_query_error query;
    mc_world_light_error submit;
    mc_registry_error registry;
    mc_world_light_acquisition_error acquisition;
} mc_world_light_operation_error;

/* Actual fd.o(III)Z, distinct from the acquiring fd.l sky_visible query.
 * Border then Y gates precede presence; an absent cache entry returns false.
 * Y>=128 returns true without provider access, after the border gate. */
mc_world_storage_status mc_world_runtime_sky_exposed(mc_world_runtime *world, int32_t x, int32_t y,
                                                     int32_t z, bool *out,
                                                     mc_world_light_operation_error *error);

/* Actual fd.b(Light,IIII)V. A present normal Chunk becomes dirty before Light
 * active use and the masked nibble write. Then notify the live u List, even
 * for a blank Chunk or a nullable unmatched Light. A missing callback fails.
 * Every completed write/callback remains on failure; no Block-ID hooks run. */
mc_world_storage_status mc_world_runtime_saved_light_set(mc_world_runtime *world,
                                                         const mc_world_light_type *type, int32_t x,
                                                         int32_t y, int32_t z, int32_t value,
                                                         mc_world_light_operation_error *error);

/* Actual fd.a(Light,IIII)V: no-sky gate, point presence, intrinsic Sky/Block
 * hint adjustment, saved-light comparison, then coalescing one-cell submit. */
mc_world_storage_status mc_world_runtime_light_recheck(mc_world_runtime *world,
                                                       const mc_world_light_type *type, int32_t x,
                                                       int32_t y, int32_t z, int32_t hint,
                                                       mc_world_light_operation_error *error);

/* Actual fd.j()Z and concrete st propagation. Pop newest before invoking;
 * process at most 499 records; return true when its source budget expires
 * with work still queued. M>=50 returns false before changing M. Entered M
 * is decremented from its current value on every exit, including failures.
 *
 * max_steps is an explicit native per-call work budget, counting loop X iterations,
 * columns and cells (including skipped columns). SIZE_MAX selects its maximum bound. A
 * reached exhausted budget fails with LIMIT and retains prior effects and the
 * already-popped record's removal; it is not successful propagation. Source
 * volume diagnostics, wrapped bounds, live queue mutation and WorldAccess
 * reentry retain their order. The detached popped record survives queue clear
 * and is retired after its invocation, including failure.
 *
 * All four operations admit live GAME RAW/constructing/failed prefixes, reject
 * COMPOSITION/retirement, and hold the exact World through synchronous reentry.
 * Nullable same-class Light is real source input; foreign handles are native
 * admission failures. Keep owners/dependencies exclusive and arguments/output/
 * error disjoint. Failure preserves ordinary output and earlier source effects.
 * Success clears this call's frame, without clearing prior World diagnostics.
 * No startup grid acquisition, renderer remesh or World.p simulation is implied. */
mc_world_storage_status mc_world_runtime_light_drain(mc_world_runtime *world, size_t max_steps,
                                                     bool *out,
                                                     mc_world_light_operation_error *error);

#endif
