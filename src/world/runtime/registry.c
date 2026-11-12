#include "registry_execution_internal.h"
#include "runtime_internal.h"

#include "construction_internal.h"
#include "execution_children_internal.h"

#include "../../block/registry/runtime_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/world/runtime/blocks.h"

mc_world_storage_status mc_world_runtime_call_enter(mc_world_runtime *runtime,
                                                    mc_world_storage_error *error) {
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

void mc_world_runtime_call_leave(mc_world_runtime *runtime) {
    --runtime->active_calls;
}

mc_world_storage_status mc_world_runtime_registry_status(mc_registry_status status) {
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
                                                         mc_world_storage_error *error) {
    runtime->last_registry_error = *cause;
    runtime->last_error = (mc_world_storage_error){mc_world_runtime_registry_status(status),
                                                   status == MC_REGISTRY_OK ? NULL : operation, 0};
    if (error != NULL)
        *error = runtime->last_error;
    return runtime->last_error.status;
}

const mc_registry_error *mc_world_runtime_last_registry_error(const mc_world_runtime *runtime) {
    return runtime == NULL ? NULL : &runtime->last_registry_error;
}

static mc_world_material_error material_empty_error(void) {
    return (mc_world_material_error){.registry = {.initializer_pc = -1,
                                                  .block_id = -1,
                                                  .constructor_pc = -1,
                                                  .active_initializer_pc = -1,
                                                  .item_argument = -1}};
}

/* Borrowed descriptor only; all result/typed query storage belongs to the
 * retained caller frame. No Source outcome or pending VALUE is stored here. */
typedef struct material_execution {
    mc_entity_tick_execution *execution;
    mc_world_runtime_classes *code_classes;
    mc_world_runtime_execution_code code;
    mc_world_collision_error *query_error;
    mc_world_storage_status *query_status;
    mc_world_runtime_execution_result *owned;
} material_execution;

static mc_world_storage_status material_get(mc_world_runtime *runtime, int32_t x, int32_t y,
                                            int32_t z, mc_material **out,
                                            mc_world_material_error *error,
                                            const material_execution *query) {
    int32_t id;
    error->bytecode = 4;
    mc_world_storage_status status;
    if (query == NULL) {
        status = mc_world_runtime_block_get(runtime, x, y, z, &id, &error->cause);
    } else {
        status = mc_world_collision_block_value_in_execution(query->execution, runtime, query->code,
                                                             x, y, z, false, &id,
                                                             query->query_error, query->owned);
        *query->query_status = status;
        /* The lower return is not the native ownership channel. A real GET
         * failure remains in the required lower frame even when ownership also
         * refuses; do not overwrite it with an invented material Source cause.
         * Actual GET OK/native refusal likewise cannot reach Registry or out. */
        if (query->owned->ownership_status != MC_WORLD_STORAGE_OK)
            return status;
        error->cause = query->query_error->nearby.query.cause;
    }
    if (status != MC_WORLD_STORAGE_OK) {
        error->status = status;
        error->operation = error->cause.operation;
        return status;
    }
    mc_registry_classes *registry =
        query == NULL ? mc_world_runtime_registry(runtime) : query->code_classes->registry;
    mc_registry_error cause = {.status = MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY,
                               .message = "World material registry binding",
                               .initializer_pc = -1,
                               .block_id = -1,
                               .constructor_pc = -1,
                               .active_initializer_pc = -1,
                               .item_argument = -1};
    mc_material *material = NULL;
    mc_registry_status registry_status = cause.status;
    error->bytecode = id == 0 ? 14u : 18u;
    if (registry != NULL) {
        if (id == 0) {
            registry_status = mc_registry_material_get(registry, MC_MATERIAL_a, &material, &cause);
        } else {
            const mc_block *block = NULL;
            mc_registry_block_m_view array;
            registry_status = mc_registry_block_m_get(registry, &array, &cause);
            if (registry_status == MC_REGISTRY_OK) {
                error->bytecode = 23;
                registry_status = mc_registry_block_m_at(&array, id, &block, &cause);
            }
            if (registry_status == MC_REGISTRY_OK) {
                error->bytecode = 24;
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
                                              "World material registry", &error->cause);
    error->status = status;
    error->operation = error->cause.operation;
    error->registry = cause;
    if (status == MC_WORLD_STORAGE_OK) {
        *out = material;
        *error = material_empty_error();
    }
    return status;
}

mc_world_storage_status mc_world_runtime_material_get_detailed(mc_world_runtime *runtime, int32_t x,
                                                               int32_t y, int32_t z,
                                                               mc_material **out,
                                                               mc_world_material_error *error) {
    mc_world_material_error frame = material_empty_error();
    if (runtime == NULL || out == NULL) {
        frame.status = MC_WORLD_STORAGE_INVALID_ARGUMENT;
        frame.operation = "World material arguments";
        frame.cause = (mc_world_storage_error){frame.status, frame.operation, 0};
        if (error != NULL)
            *error = frame;
        return frame.status;
    }
    mc_world_storage_status status = mc_world_runtime_call_enter(runtime, &frame.cause);
    if (status != MC_WORLD_STORAGE_OK) {
        frame.status = status;
        frame.operation = frame.cause.operation;
    } else {
        status = material_get(runtime, x, y, z, out, &frame, NULL);
        mc_world_runtime_call_leave(runtime);
    }
    if (error != NULL)
        *error = frame;
    return status;
}

mc_world_storage_status mc_world_runtime_material_get(mc_world_runtime *runtime, int32_t x,
                                                      int32_t y, int32_t z, mc_material **out,
                                                      mc_world_storage_error *error) {
    mc_world_material_error frame;
    const mc_world_storage_status status =
        mc_world_runtime_material_get_detailed(runtime, x, y, z, out, &frame);
    if (error != NULL)
        *error = frame.cause;
    return status;
}

/* Native interface refusal is not a Source material/query outcome. Only the
 * required fresh owner result is written; caller Source/query/out storage stays
 * untouched until the same material body actually enters. */
static bool material_execution_refused(mc_world_runtime_execution_result *owned,
                                       mc_world_storage_status status, const char *operation,
                                       mc_entity_tick_execution_site site) {
    mc_world_runtime_execution_result_note(owned, status, operation, site);
    return false;
}

bool mc_world_runtime_material_get_in_execution(
    mc_entity_tick_execution *execution, mc_world_runtime_classes *code_classes,
    mc_world_runtime *runtime, mc_world_runtime_execution_code code, int32_t x, int32_t y,
    int32_t z, mc_material **out, mc_world_material_error *error,
    mc_world_collision_error *query_error, mc_world_storage_status *query_status,
    mc_world_runtime_execution_result *owned) {
    if (owned != NULL)
        mc_world_runtime_execution_result_init(owned);
    const mc_entity_tick_execution_site site =
        execution != NULL && execution->identity == execution
            ? execution->site
            : (mc_entity_tick_execution_site){NULL, NULL, NULL, -1};
    if (execution == NULL || code_classes == NULL || runtime == NULL || out == NULL ||
        error == NULL || query_error == NULL || query_status == NULL || owned == NULL)
        return material_execution_refused(owned, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                          "World material execution/code/outputs required", site);
    mc_world_access_failure_lease *unused = NULL;
    const mc_world_storage_status admitted =
        mc_world_execution_child_lease(execution, runtime, &unused);
    if (admitted != MC_WORLD_STORAGE_OK)
        return material_execution_refused(owned, admitted,
                                          "World material evaluated World ownership", site);
    /* A code borrow is supplied explicitly, never inferred from the receiver.
     * The joined scalar domain currently supports only this same-code graph. */
    if (runtime->classes != code_classes)
        return material_execution_refused(owned, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "World material same defining code namespace", site);
    mc_world_storage_error native = {0};
    const mc_world_storage_status entered = mc_world_runtime_call_enter(runtime, &native);
    if (entered != MC_WORLD_STORAGE_OK)
        return material_execution_refused(owned, entered, "World material World call admission",
                                          site);
    mc_world_material_error frame = material_empty_error();
    const material_execution query = {execution,   code_classes, code,
                                      query_error, query_status, owned};
    const mc_world_storage_status status = material_get(runtime, x, y, z, out, &frame, &query);
    /* Pair only this exact entered World. A refused descendant unwind may have
     * left another neutral scope current; never pop or infer from that scope. */
    mc_world_runtime_call_leave(runtime);
    *error = frame;
    return status == MC_WORLD_STORAGE_OK && owned->ownership_status == MC_WORLD_STORAGE_OK;
}
