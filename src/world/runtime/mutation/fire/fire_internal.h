#ifndef MC_WORLD_RUNTIME_MUTATION_FIRE_INTERNAL_H
#define MC_WORLD_RUNTIME_MUTATION_FIRE_INTERNAL_H

#include "mc/world/runtime/mutations.h"

/* Actual client YQ bodies. The enclosing ordinary call holds the World and
 * defining Registry graph through synchronous callbacks and descendants.
 * Failure preserves prior stores, queues, Random and actual child diagnostics. */
bool mc_world_mutation_fire_can_place(mc_world_runtime *, const mc_block *, int32_t, int32_t,
                                      int32_t, bool *, mc_world_mutation_error *);
bool mc_world_mutation_fire_added(mc_world_runtime *, const mc_block *, int32_t, int32_t, int32_t,
                                  mc_world_mutation_error *);
bool mc_world_mutation_fire_neighbor(mc_world_runtime *, const mc_block *, int32_t, int32_t,
                                     int32_t, int32_t, mc_world_mutation_error *);
bool mc_world_mutation_fire_tick(mc_world_runtime *, const mc_block *, int32_t, int32_t, int32_t,
                                 mc_java_random *, mc_world_mutation_error *);

/* The actual captured a/b array has 256 elements. Its reference is evaluated
 * before the World getter; this leaf performs only the later array operation.
 * Placement uses the same leaf with its own retained acquiring getter. */
mc_registry_status mc_world_fire_array_get(const int32_t *, int32_t, int32_t *,
                                           mc_registry_error *);

#endif
