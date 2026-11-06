#ifndef MC_WORLD_RUNTIME_RANDOM_DISPLAY_H
#define MC_WORLD_RUNTIME_RANDOM_DISPLAY_H

#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/world_access_error.h"

typedef struct mc_world_random_display_call mc_world_random_display_call;
typedef struct mc_world_random_display_block_error mc_world_random_display_block_error;

typedef enum mc_world_random_display_child {
    MC_WORLD_RANDOM_DISPLAY_NONE,
    MC_WORLD_RANDOM_DISPLAY_DEFAULT_RANDOM,
    MC_WORLD_RANDOM_DISPLAY_ACQUISITION,
    MC_WORLD_RANDOM_DISPLAY_REGISTRY,
    MC_WORLD_RANDOM_DISPLAY_BLOCK
} mc_world_random_display_child;

typedef struct mc_world_random_display_error {
    mc_world_storage_status status;
    const char *operation, *exception_class;
    unsigned int bytecode, child_bytecode;
    const char *child_class, *child_method, *child_descriptor;
    bool native_policy;
    mc_world_random_display_child child;
    mc_world_storage_error cause;
    mc_java_random_profile_status profile;
    const mc_world_runtime_acquisition_error *acquisition;
    const mc_registry_error *registry;
    const mc_world_random_display_block_error *block;
    mc_world_access_failure world_access;
} mc_world_random_display_error;

typedef struct mc_world_random_display_state {
    bool active, returned, local_random_allocated, local_random_constructed;
    bool coordinates_stored[3], block_read, block_selected;
    unsigned int source_pc;
    int32_t argument_x, argument_y, argument_z, sample, draw_index;
    int32_t x, y, z, block_id, draws[6];
    size_t samples_started, samples_returned, world_random_calls;
    size_t display_calls_started, display_calls_returned;
    mc_java_random local_random_initial, local_random;
    const mc_java_random *source_random;
    const mc_block *selected_block;
    mc_world_runtime *world;
    const mc_world_random_display_error *error;
} mc_world_random_display_state;

/* Actual client fd.q(III)V. Construct one distinct ordinary Random at8 on the
 * supplying World's existing profile, including for all-air calls. Each of
 * exactly1000 samples reloads World.r six times and draws nextInt16 before
 * the real acquiring getter100. Positive IDs alone reach Block.m110/115 and
 * the actual display virtual125, passing the same local Random throughout.
 * Sample increment128 and return134 occur only after their real predecessors.
 * No Math owner, simulation tick, Block.n query, source repair or work cap is
 * added. Child methods own their reached floating-point admission and effects.
 *
 * A published call holds the actual client GAME World/class/profile graph and
 * the exact failed WorldAccess receipt until native retirement. The getter's
 * own Chunk use ends on its return; no captured Chunk crosses display callbacks.
 * Failed source fields and consumed entropy remain unchanged by retirement.
 * source_random is the fixed local identity; its initialized value is valid
 * only when local_random_constructed. Inspect copies both current and initial
 * values without rebinding any source stream. Child diagnostic pointers borrow
 * the call and remain valid through failed cleanup. out begins NULL; native
 * admission preserves it and the World's prior diagnostics. All access is
 * exclusive, and owners/arguments/results/errors must be live and disjoint. */
mc_world_storage_status
mc_world_runtime_random_display_begin(mc_world_runtime *, int32_t x, int32_t y, int32_t z,
                                      mc_world_random_display_call **,
                                      mc_world_random_display_error *admission);
bool mc_world_runtime_random_display_inspect(const mc_world_random_display_call *,
                                             mc_world_random_display_state *);
mc_world_storage_status
mc_world_runtime_random_display_retire(mc_world_random_display_call **,
                                       mc_world_random_display_error *cleanup);

#endif
