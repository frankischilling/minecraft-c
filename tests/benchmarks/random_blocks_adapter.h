#ifndef MC_BENCHMARK_RANDOM_BLOCKS_ADAPTER_H
#define MC_BENCHMARK_RANDOM_BLOCKS_ADAPTER_H

#include "mc/core/java/java.h"
#include "mc/world/runtime/mutations.h"

typedef bool (*random_blocks_leaf)(mc_world_runtime *, const mc_block *, int32_t, int32_t, int32_t,
                                   mc_java_random *, mc_world_mutation_error *);

typedef struct random_blocks_adapter {
    random_blocks_leaf grass, leaves;
    const char *name;
} random_blocks_adapter;

const random_blocks_adapter *random_blocks_adapter_get(void);

#endif
