/* The preceding sampler body, with only the intended getter call restored.
 * The observer and every remaining dependency are shared with the candidate. */
#include "random_display_adapter.h"

mc_world_storage_status mc_benchmark_rd_detailed_begin(mc_world_runtime *, int32_t, int32_t,
                                                       int32_t, mc_world_random_display_call **,
                                                       mc_world_random_display_error *);
bool mc_benchmark_rd_detailed_inspect(const mc_world_random_display_call *,
                                      mc_world_random_display_state *);
mc_world_storage_status mc_benchmark_rd_detailed_retire(mc_world_random_display_call **,
                                                        mc_world_random_display_error *);

#define mc_world_runtime_random_display_begin mc_benchmark_rd_detailed_begin
#define mc_world_runtime_random_display_inspect mc_benchmark_rd_detailed_inspect
#define mc_world_runtime_random_display_retire mc_benchmark_rd_detailed_retire
#define mc_world_runtime_block_get_failure mc_world_runtime_block_get_detailed
#include "../../src/world/runtime/random_display.c"
#undef mc_world_runtime_block_get_failure
#undef mc_world_runtime_random_display_begin
#undef mc_world_runtime_random_display_inspect
#undef mc_world_runtime_random_display_retire

const rd_cpu_adapter *rd_cpu_adapter_get(void) {
    static const rd_cpu_adapter adapter = {"detailed", mc_benchmark_rd_detailed_begin,
                                           mc_benchmark_rd_detailed_inspect,
                                           mc_benchmark_rd_detailed_retire};
    return &adapter;
}
