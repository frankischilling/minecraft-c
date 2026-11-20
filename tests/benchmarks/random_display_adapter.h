#ifndef MC_BENCHMARK_RANDOM_DISPLAY_ADAPTER_H
#define MC_BENCHMARK_RANDOM_DISPLAY_ADAPTER_H

#include "mc/world/runtime/random_display.h"

typedef struct rd_cpu_adapter {
    const char *name;
    mc_world_storage_status (*begin)(mc_world_runtime *, int32_t, int32_t, int32_t,
                                     mc_world_random_display_call **,
                                     mc_world_random_display_error *);
    bool (*inspect)(const mc_world_random_display_call *, mc_world_random_display_state *);
    mc_world_storage_status (*retire)(mc_world_random_display_call **,
                                      mc_world_random_display_error *);
} rd_cpu_adapter;

const rd_cpu_adapter *rd_cpu_adapter_get(void);

#endif
