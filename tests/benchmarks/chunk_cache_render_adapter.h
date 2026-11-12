#ifndef MC_BENCHMARK_CHUNK_CACHE_RENDER_ADAPTER_H
#define MC_BENCHMARK_CHUNK_CACHE_RENDER_ADAPTER_H
#include "mc/world/runtime/chunk_cache.h"

typedef mc_client_chunk_cache_status (*ew_render_float_getter)(mc_client_chunk_cache *, int32_t,
                                                               int32_t, int32_t, float *,
                                                               mc_client_chunk_cache_error *);
typedef mc_client_chunk_cache_status (*ew_render_min_getter)(mc_client_chunk_cache *, int32_t,
                                                             int32_t, int32_t, int32_t, float *,
                                                             mc_client_chunk_cache_error *);
typedef mc_client_chunk_cache_status (*ew_render_material_getter)(mc_client_chunk_cache *, int32_t,
                                                                  int32_t, int32_t, mc_material **,
                                                                  mc_client_chunk_cache_error *);
typedef mc_client_chunk_cache_status (*ew_render_bool_getter)(mc_client_chunk_cache *, int32_t,
                                                              int32_t, int32_t, bool *,
                                                              mc_client_chunk_cache_error *);
typedef mc_client_chunk_cache_status (*ew_render_manager_getter)(mc_client_chunk_cache *,
                                                                 mc_world_chunk_manager **,
                                                                 mc_client_chunk_cache_error *);

typedef struct ew_render_adapter {
    ew_render_float_getter brightness;
    ew_render_min_getter min_brightness;
    ew_render_material_getter material;
    ew_render_bool_getter opaque, normal_cube;
    ew_render_manager_getter manager;
    bool full_success_error;
    const char *name;
} ew_render_adapter;

const ew_render_adapter *ew_render_adapter_get(void);
#endif
