#include "random_display_adapter.h"

const rd_cpu_adapter *rd_cpu_adapter_get(void) {
    static const rd_cpu_adapter adapter = {"failure", mc_world_runtime_random_display_begin,
                                           mc_world_runtime_random_display_inspect,
                                           mc_world_runtime_random_display_retire};
    return &adapter;
}
