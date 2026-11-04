#include "chunk_cache_metadata_adapter.h"

const ew_cpu_adapter *ew_cpu_adapter_get(void) {
    static const ew_cpu_adapter adapter = {mc_client_chunk_cache_metadata, true, "public"};
    return &adapter;
}
