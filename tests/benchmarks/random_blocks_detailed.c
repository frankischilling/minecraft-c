#include "random_blocks_adapter.h"

/* The optional baseline object compiles the production bodies under these
 * names and maps only their two failure-only getter calls to detailed getters. */
bool mc_benchmark_detailed_grass_tick(mc_world_runtime *, const mc_block *, int32_t, int32_t,
                                      int32_t, mc_java_random *, mc_world_mutation_error *);
bool mc_benchmark_detailed_leaves_tick(mc_world_runtime *, const mc_block *, int32_t, int32_t,
                                       int32_t, mc_java_random *, mc_world_mutation_error *);

const random_blocks_adapter *random_blocks_adapter_get(void) {
    static const random_blocks_adapter adapter = {.grass = mc_benchmark_detailed_grass_tick,
                                                  .leaves = mc_benchmark_detailed_leaves_tick,
                                                  .name = "detailed"};
    return &adapter;
}
