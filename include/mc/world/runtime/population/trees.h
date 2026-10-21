#ifndef MC_WORLD_RUNTIME_POPULATION_TREES_H
#define MC_WORLD_RUNTIME_POPULATION_TREES_H

#include "mc/core/java/java.h"
#include "mc/world/runtime/mutations.h"
#include <stddef.h>

/* These entries call the actual acquiring World and quiet mutation operations.
 * Native status and the source boolean are independent. Failure preserves out
 * and all reached owner/Random/World effects; there is no rollback or retry.
 * Require exclusive live owners, disjoint arguments/results/errors, and an
 * unchanged strict floating environment through synchronous descendants.
 * Recursive population may use another feature owner and the live Random;
 * it must not reenter, scale, dispose, or modify this big-oak owner.
 * NULL Random is diagnosed only at its first reached source invocation.
 * GAME raw/failed prefixes are admitted; native COMPOSITION/RETIRED are not. */
typedef struct mc_world_tree_error {
    mc_world_storage_status status;
    const char *operation;
    const char *feature_class;
    const char *method;
    unsigned int bytecode;
    /* Big-oak placement phase call site, distinct from its deepest helper. */
    unsigned int entry_bytecode;
    bool native_boundary;
    mc_world_storage_error cause;
    mc_registry_error registry;
    mc_world_mutation_error mutation;
    mc_java_random_profile_status profile_status;
} mc_world_tree_error;

mc_world_storage_status mc_world_tree_oak(mc_world_runtime *, mc_java_random *, int32_t x,
                                          int32_t y, int32_t z, bool *out,
                                          mc_world_tree_error *error);
mc_world_storage_status mc_world_tree_birch(mc_world_runtime *, mc_java_random *, int32_t x,
                                            int32_t y, int32_t z, bool *out,
                                            mc_world_tree_error *error);
mc_world_storage_status mc_world_tree_taiga_tall(mc_world_runtime *, mc_java_random *, int32_t x,
                                                 int32_t y, int32_t z, bool *out,
                                                 mc_world_tree_error *error);
mc_world_storage_status mc_world_tree_taiga_dense(mc_world_runtime *, mc_java_random *, int32_t x,
                                                  int32_t y, int32_t z, bool *out,
                                                  mc_world_tree_error *error);

typedef struct mc_world_tree_big_oak mc_world_tree_big_oak;

typedef struct mc_world_tree_limits {
    /* Actual native owner and simultaneously live coordinate rows. NULL
     * limits selects 1 MiB. Explicit zero admits no allocation. No scan cap. */
    size_t max_allocation_bytes;
} mc_world_tree_limits;

void mc_world_tree_limits_default(mc_world_tree_limits *out);

/* constructor_world provides its actual application DefaultRandom service;
 * it is not the source c field, which remains NULL until placement. Keep that
 * World class owner alive until dispose, including the unplaced interval.
 * Placement requires the same class owner, stores/retains source c before the
 * provider nextLong, and retains it through rejection/failure/reuse/dispose.
 * Create before scale and the separate acquiring height query in population.
 * No convenience entry constructs after the height query. */
mc_world_storage_status mc_world_tree_big_oak_create(mc_world_runtime *constructor_world,
                                                     const mc_world_tree_limits *limits,
                                                     mc_world_tree_big_oak **out,
                                                     mc_world_tree_error *error);
mc_world_storage_status mc_world_tree_big_oak_scale(mc_world_tree_big_oak *, double height,
                                                    double width, double density,
                                                    mc_world_tree_error *error);
mc_world_storage_status mc_world_tree_big_oak_place(mc_world_tree_big_oak *, mc_world_runtime *,
                                                    mc_java_random *, int32_t x, int32_t y,
                                                    int32_t z, bool *out,
                                                    mc_world_tree_error *error);
void mc_world_tree_big_oak_dispose(mc_world_tree_big_oak *);

/* Big-oak libm calls require separate reference qualification. Strict binary
 * representation/control checks do not prove pow/sqrt/sin/cos equivalence.
 * Native bounded allocations model reached failures, not Java heap OOME. */
#endif
