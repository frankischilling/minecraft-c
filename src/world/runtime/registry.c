#include "runtime_internal.h"

#include "mc/block/registry/runtime.h"
#include "mc/world/runtime/blocks.h"

mc_world_storage_status mc_world_runtime_call_enter(mc_world_runtime *runtime,
                                                    mc_world_storage_error *error)
{
    mc_world_storage_status status = runtime == NULL ? MC_WORLD_STORAGE_INVALID_ARGUMENT
                                     : runtime->construction.phase == MC_WORLD_RUNTIME_RETIRED
                                         ? MC_WORLD_STORAGE_UNSUPPORTED_STATE
                                     : runtime->active_calls == SIZE_MAX ? MC_WORLD_STORAGE_LIMIT
                                                                         : MC_WORLD_STORAGE_OK;
    if (status != MC_WORLD_STORAGE_OK) {
        if (error != NULL)
            *error = (mc_world_storage_error){status, "World operation admission", 0};
        return status;
    }
    ++runtime->active_calls;
    return MC_WORLD_STORAGE_OK;
}

void mc_world_runtime_call_leave(mc_world_runtime *runtime)
{
    --runtime->active_calls;
}

mc_world_storage_status mc_world_runtime_registry_status(mc_registry_status status)
{
    switch (status) {
        case MC_REGISTRY_OK:
            return MC_WORLD_STORAGE_OK;
        case MC_REGISTRY_INVALID_ARGUMENT:
            return MC_WORLD_STORAGE_INVALID_ARGUMENT;
        case MC_REGISTRY_LIMIT:
            return MC_WORLD_STORAGE_LIMIT;
        case MC_REGISTRY_ALLOCATION_FAILED:
            return MC_WORLD_STORAGE_ALLOCATION_FAILED;
        case MC_REGISTRY_BUSY:
            return MC_WORLD_STORAGE_BUSY;
        default:
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    }
}

mc_world_storage_status mc_world_runtime_registry_result(mc_world_runtime *runtime,
                                                         mc_registry_status status,
                                                         const mc_registry_error *cause,
                                                         const char *operation,
                                                         mc_world_storage_error *error)
{
    runtime->last_registry_error = *cause;
    runtime->last_error = (mc_world_storage_error){mc_world_runtime_registry_status(status),
                                                   status == MC_REGISTRY_OK ? NULL : operation, 0};
    if (error != NULL)
        *error = runtime->last_error;
    return runtime->last_error.status;
}

const mc_registry_error *mc_world_runtime_last_registry_error(const mc_world_runtime *runtime)
{
    return runtime == NULL ? NULL : &runtime->last_registry_error;
}

static mc_world_storage_status material_get(mc_world_runtime *runtime, int32_t x, int32_t y,
                                            int32_t z, mc_material **out,
                                            mc_world_storage_error *error)
{
    int32_t id;
    mc_world_storage_status status = mc_world_runtime_block_get(runtime, x, y, z, &id, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    mc_registry_classes *registry = mc_world_runtime_registry(runtime);
    mc_registry_error cause = {.status = MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY,
                               .message = "World material registry binding",
                               .initializer_pc = -1,
                               .block_id = -1,
                               .constructor_pc = -1,
                               .active_initializer_pc = -1,
                               .item_argument = -1};
    mc_material *material = NULL;
    mc_registry_status registry_status = cause.status;
    if (registry != NULL) {
        if (id == 0) {
            registry_status = mc_registry_material_get(registry, MC_MATERIAL_a, &material, &cause);
        } else {
            const mc_block *block = NULL;
            registry_status = mc_registry_block_get(registry, id, &block, &cause);
            if (registry_status == MC_REGISTRY_OK) {
                const mc_block_fields *fields = mc_block_fields_get(block);
                if (fields == NULL) {
                    registry_status = MC_REGISTRY_NULL_REFERENCE;
                    cause.status = registry_status;
                    cause.message = "World material Block receiver";
                } else {
                    material = fields->bA;
                }
            }
        }
    }
    status = mc_world_runtime_registry_result(runtime, registry_status, &cause,
                                              "World material registry", error);
    if (status == MC_WORLD_STORAGE_OK)
        *out = material;
    return status;
}

mc_world_storage_status mc_world_runtime_material_get(mc_world_runtime *runtime, int32_t x,
                                                      int32_t y, int32_t z, mc_material **out,
                                                      mc_world_storage_error *error)
{
    if (runtime == NULL || out == NULL) {
        if (error != NULL)
            *error = (mc_world_storage_error){MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                              "World material arguments", 0};
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    }
    mc_world_storage_status status = mc_world_runtime_call_enter(runtime, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    status = material_get(runtime, x, y, z, out, error);
    mc_world_runtime_call_leave(runtime);
    return status;
}
