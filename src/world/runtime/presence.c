#include "presence_internal.h"

#include "mc/core/java/java.h"
#include "provider_internal.h"

static mc_world_storage_status completed(bool value, bool *out, mc_world_presence_error *error) {
    *out = value;
    *error = (mc_world_presence_error){0};
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_runtime_region_present_held(mc_world_runtime *world, int32_t min_x,
                                                             int32_t min_y, int32_t min_z,
                                                             int32_t max_x, int32_t max_y,
                                                             int32_t max_z, bool *out,
                                                             mc_world_presence_error *error) {
    if (max_y < 0 || min_y >= 128)
        return completed(false, out, error);
    /* Preserve all six shifts, including the unused vertical endpoints.
     * Shifted X/Z endpoints cannot increment-wrap; reversed ranges stay empty. */
    const int32_t first_x = mc_java_i32_shr(min_x, 4);
    (void)mc_java_i32_shr(min_y, 4);
    const int32_t first_z = mc_java_i32_shr(min_z, 4);
    const int32_t last_x = mc_java_i32_shr(max_x, 4);
    (void)mc_java_i32_shr(max_y, 4);
    const int32_t last_z = mc_java_i32_shr(max_z, 4);
    for (int32_t chunk_x = first_x; chunk_x <= last_x; chunk_x = mc_java_i32_add(chunk_x, 1)) {
        for (int32_t chunk_z = first_z; chunk_z <= last_z; chunk_z = mc_java_i32_add(chunk_z, 1)) {
            bool present;
            *error = (mc_world_presence_error){.bytecode = 69, .provider_bytecode = 6};
            mc_world_storage_status status = mc_world_runtime_provider_contains(
                world, chunk_x, chunk_z, &present, &error->cause);
            if (status != MC_WORLD_STORAGE_OK)
                return status;
            if (!present)
                return completed(false, out, error);
        }
    }
    return completed(true, out, error);
}
