#ifndef MC_BENCHMARK_CHUNK_CACHE_ADAPTER_H
#define MC_BENCHMARK_CHUNK_CACHE_ADAPTER_H
#include "mc/world/runtime/chunk_cache.h"
typedef mc_client_chunk_cache_status (*ew_cpu_getter)(mc_client_chunk_cache *, int32_t, int32_t,
                                                      int32_t, int32_t *,
                                                      mc_client_chunk_cache_error *);

typedef struct ew_cpu_adapter {
    ew_cpu_getter get;
    bool full_success_error;
    const char *name;
} ew_cpu_adapter;

const ew_cpu_adapter *ew_cpu_adapter_get(void);
#endif
