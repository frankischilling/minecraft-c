#include "random_blocks_adapter.h"
#include "world/runtime/mutation/mutation_internal.h"

const random_blocks_adapter *random_blocks_adapter_get(void) {
    static const random_blocks_adapter adapter = {.grass = mc_world_mutation_grass_tick,
                                                  .leaves = mc_world_mutation_leaves_tick,
                                                  .name = "failure"};
    return &adapter;
}
