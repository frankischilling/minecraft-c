#include "chunk_cache_render_adapter.h"

const ew_render_adapter *ew_render_adapter_get(void) {
    static const ew_render_adapter adapter = {mc_client_chunk_cache_brightness,
                                              mc_client_chunk_cache_min_brightness,
                                              mc_client_chunk_cache_material,
                                              mc_client_chunk_cache_opaque,
                                              mc_client_chunk_cache_normal_cube,
                                              mc_client_chunk_cache_manager,
                                              true,
                                              "public"};
    return &adapter;
}
