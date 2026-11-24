#include "chunk_cache_render_adapter.h"
#include "world/runtime/chunk_cache_internal.h"

const ew_render_adapter *ew_render_adapter_get(void) {
    static const ew_render_adapter adapter = {mc_client_chunk_cache_brightness_failure,
                                              mc_client_chunk_cache_min_brightness_failure,
                                              mc_client_chunk_cache_material_failure,
                                              mc_client_chunk_cache_opaque_failure,
                                              mc_client_chunk_cache_normal_cube_failure,
                                              mc_client_chunk_cache_manager_failure,
                                              false,
                                              "failure"};
    return &adapter;
}
