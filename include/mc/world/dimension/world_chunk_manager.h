#ifndef MC_WORLD_CHUNK_MANAGER_RUNTIME_H
#define MC_WORLD_CHUNK_MANAGER_RUNTIME_H

#include "mc/world/biome/sampling.h"
#include "mc/world/dimension/world_provider.h"

typedef struct mc_world_provider_registry_binding {
    mc_registry_classes *classes;
    mc_registry_side side;
    mc_java_random_profile *profile;
} mc_world_provider_registry_binding;

typedef struct mc_world_chunk_manager_game_fields {
    mc_registry_classes *registry;
    const mc_biome_reference_array *d;
    const mc_biome *fixed_e;
} mc_world_chunk_manager_game_fields;

typedef struct mc_world_chunk_manager_error {
    mc_world_provider_status status;
    const char *operation;
    unsigned int bytecode;
    mc_biome_sampling_error sampling;
} mc_world_chunk_manager_error;

/* Concrete class-bound setup, separate from world_provider.h's numerical profile.
 * The application supplies its actual side/profile/class owner; a nonnull
 * World must have that same registry binding. Native admission is inert and
 * quiescent. Source bind publishes World.a2, allocates the actual manager at
 * setup1, then fixed setup actively loads Biome.l/m5 BEFORE its constructor
 * and Dimension.b publication. noise-backed setup has no Biome active use.
 *
 * Each manager owns an independent counted native registry borrow until its
 * final release, including aliases surviving Dimension rebind/World teardown.
 * Arena reference arrays never retain their own registry cyclically. Existing
 * numerical fields remain projection metadata; actual fixed e and cached d
 * are returned by game_fields_get. NULL fixed e is a real stored value.
 * Keep profile/material/resource owners live through all borrowers. Calls and
 * views require exclusive access and the existing strict FP environment.
 * Failure preserves reached World/class/resource/profile publications and the
 * preceding Dimension.b. Error keeps the full nested registry first cause. */
mc_world_provider_status
mc_world_provider_bind_game(mc_world_provider *dimension, mc_world_runtime *world,
                            const mc_world_provider_registry_binding *binding,
                            mc_world_provider_error *error);
mc_world_provider_status
mc_world_chunk_manager_game_fields_get(const mc_world_chunk_manager *biome,
                                       mc_world_chunk_manager_game_fields *out);

/* Actual noise-backed/fixed direct, cached, point and chunk methods. A numerical-only
 * manager has no actual reference capability and is rejected explicitly.
 * noise-backed direct retains its square domain; fixed direct permits rectangles.
 * Cached captures actual d3, invokes direct11, publishes d14 only on return.
 * Completed reused-array writes and climate/class effects survive failure.
 * Ordinary result outputs are unchanged on failure; error exposes the reached
 * direct prefix separately. Direct sampling leaves d untouched. Every call
 * holds the captured manager through synchronous resource callbacks. Rebinding
 * its containing Dimension may replace b; this call continues on its held
 * receiver. Ordinary GAME rebind during registry entry is BUSY; the explicit
 * numerical bind remains a separate operation. Fixed point/chunk overrides
 * return current e directly at4, without sampling or changing d. noise-backed point
 * calls cached1x1 before reading array[0].
 * Bound dimensions/products must fit INT32_MAX and the native spans. Output,
 * error, binding and owner storage must be disjoint and all owners live. */
mc_world_provider_status mc_world_chunk_manager_sample(mc_world_chunk_manager *biome, int32_t x,
                                                       int32_t z, size_t width, size_t height,
                                                       mc_biome_reference_array *input,
                                                       mc_biome_reference_array **out,
                                                       mc_world_chunk_manager_error *error);
mc_world_provider_status mc_world_chunk_manager_cached(mc_world_chunk_manager *biome, int32_t x,
                                                       int32_t z, size_t width, size_t height,
                                                       mc_biome_reference_array **out,
                                                       mc_world_chunk_manager_error *error);
mc_world_provider_status mc_world_chunk_manager_point(mc_world_chunk_manager *biome, int32_t x,
                                                      int32_t z, const mc_biome **out,
                                                      mc_world_chunk_manager_error *error);
mc_world_provider_status mc_world_chunk_manager_chunk(mc_world_chunk_manager *biome,
                                                      int32_t chunk_x, int32_t chunk_z,
                                                      const mc_biome **out,
                                                      mc_world_chunk_manager_error *error);

#endif
