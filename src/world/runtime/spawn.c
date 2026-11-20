#include "spawn_internal.h"

bool mc_world_source_spawn_values(mc_world_runtime *world, int32_t values[3],
                                  mc_world_spawn_error *error) {
    *error = (mc_world_spawn_error){0};
    for (size_t n = 0; n < 3; ++n) {
        mc_world_runtime_fields fields;
        mc_world_info_fields info;
        error->pc = 8u + (unsigned int)n * 7u;
        if (world == NULL) {
            error->world = MC_WORLD_STORAGE_INVALID_ARGUMENT;
            error->fields_failed = true;
            return false;
        }
        error->world = mc_world_runtime_fields_get(world, &fields);
        if (error->world != MC_WORLD_STORAGE_OK) {
            error->fields_failed = true;
            return false;
        }
        error->info_was_null = fields.x == NULL;
        error->info_getter_entered = true;
        error->info = mc_world_info_fields_get(fields.x, &info);
        if (error->info != MC_WORLD_INFO_OK)
            return false;
        values[n] = n == 0 ? info.value.spawn_x : n == 1 ? info.value.spawn_y : info.value.spawn_z;
    }
    return true;
}
