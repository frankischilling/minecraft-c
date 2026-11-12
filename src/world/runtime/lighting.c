#include "mc/world/runtime/lighting.h"

#include "../../platform/text/stdout.h"
#include "chunk_provider_internal.h"
#include "construction_internal.h"
#include "execution_children_internal.h"
#include "lighting_execution_internal.h"
#include "lighting_internal.h"
#include "mc/core/java/java.h"
#include "runtime_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static mc_world_storage_error native_error(mc_world_storage_status status, const char *operation) {
    return (mc_world_storage_error){status, operation, 0};
}

static mc_world_storage_status storage_result(mc_world_storage_error *error,
                                              mc_world_storage_status status,
                                              const char *operation) {
    if (error != NULL)
        *error = native_error(status, operation);
    return status;
}

static mc_world_storage_status result(mc_world_light_error *error,
                                      const mc_world_storage_error *cause, unsigned int bytecode,
                                      unsigned int provider_bytecode,
                                      mc_chunk_acquire_status acquisition) {
    if (error != NULL)
        *error = (mc_world_light_error){.status = cause->status,
                                        .operation = cause->operation,
                                        .bytecode = bytecode,
                                        .provider_bytecode = provider_bytecode,
                                        .acquisition = acquisition,
                                        .cause = *cause};
    return cause->status;
}

static bool light_records_quiet(const mc_world_runtime_execution_result *owned) {
    if (owned->root_pending || owned->math_failed)
        return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (owned->pending[i].used)
            return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (owned->closes[i].used)
            return false;
    return true;
}

void mc_world_light_execution_init(mc_world_light_execution *frame,
                                   mc_entity_tick_execution *execution,
                                   mc_world_runtime_execution_code code,
                                   mc_entity_tick_execution_site receiver_site,
                                   mc_world_runtime_execution_result *owned) {
    if (frame != NULL)
        *frame = (mc_world_light_execution){
            .execution = execution, .code = code, .receiver_site = receiver_site, .owned = owned};
    mc_world_runtime_execution_result_init(owned);
}

bool mc_world_light_execution_enter(mc_world_light_execution *frame, mc_world_runtime *world,
                                    const void *out, const mc_world_light_error *error,
                                    const mc_world_storage_status *status) {
    if (frame == NULL)
        return false;
    if (frame->execution == NULL || frame->execution->identity != frame->execution ||
        frame->owned == NULL || out == NULL || error == NULL || status == NULL || frame->entered) {
        mc_world_runtime_execution_result_note(frame->owned, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                               "World light execution/fresh outputs required",
                                               frame->receiver_site);
        return false;
    }
    if (frame->owned->ownership_status != MC_WORLD_STORAGE_OK || frame->owned->selection_ready ||
        !light_records_quiet(frame->owned)) {
        mc_world_runtime_execution_result_note(frame->owned, MC_WORLD_STORAGE_BUSY,
                                               "World light fresh result required",
                                               frame->receiver_site);
        return false;
    }
    mc_world_access_failure_lease *unused = NULL;
    const mc_world_storage_status admitted =
        mc_world_execution_child_lease(frame->execution, world, &unused);
    if (admitted != MC_WORLD_STORAGE_OK) {
        mc_world_runtime_execution_result_note(
            frame->owned, admitted, "World light evaluated World ownership", frame->receiver_site);
        return false;
    }
    frame->entered = true;
    return true;
}

bool mc_world_light_execution_finish(mc_world_light_execution *frame,
                                     mc_world_storage_status actual,
                                     mc_world_storage_status *status) {
    *status = actual;
    frame->completed = actual == MC_WORLD_STORAGE_OK && !frame->native_stopped &&
                       frame->owned->ownership_status == MC_WORLD_STORAGE_OK;
    return frame->completed;
}

static void light_acquisition_cause(mc_chunk_acquire_status acquired,
                                    const mc_world_runtime_acquisition_error *detail,
                                    mc_world_storage_error *cause,
                                    mc_world_population_context *population) {
    if (acquired != MC_CHUNK_ACQUIRE_OK) {
        *cause = detail->cause;
        *population = detail->population;
    }
}

static mc_chunk_acquire_status light_get_legacy(mc_world_runtime *world, int32_t x, int32_t z,
                                                mc_chunk_state **chunk,
                                                mc_world_storage_error *cause,
                                                mc_world_population_context *population) {
    mc_world_runtime_acquisition_error detail;
    const mc_chunk_acquire_status acquired =
        mc_world_runtime_get_detailed(world, x, z, chunk, &detail);
    light_acquisition_cause(acquired, &detail, cause, population);
    return acquired;
}

mc_chunk_acquire_status mc_world_light_get_chunk(mc_world_light_execution *frame,
                                                 mc_world_runtime *world, int32_t x, int32_t z,
                                                 mc_chunk_state **chunk,
                                                 mc_world_storage_error *cause,
                                                 mc_world_population_context *population) {
    if (frame == NULL)
        return light_get_legacy(world, x, z, chunk, cause, population);

    frame->get_entered = true;
    const mc_chunk_acquire_status acquired = mc_world_runtime_get_in_execution(
        frame->execution, world, frame->code, x, z, chunk, &frame->acquisition, frame->owned);
    frame->get_status = acquired;
    frame->get_returned = true;
    if (acquired != MC_CHUNK_ACQUIRE_OK) {
        frame->acquisition.source_chunk = NULL;
        if (frame->owned->selection_ready) {
            mc_entity_tick_execution_loader_view selected;
            if (mc_entity_tick_execution_loader_read(frame->execution, &frame->owned->selection,
                                                     &selected))
                frame->acquisition.source_chunk =
                    selected.status == MC_CHUNK_ACQUIRE_OK ? NULL : selected.failure;
            else
                mc_world_runtime_execution_result_note(
                    frame->owned, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "World light selected loader view", frame->receiver_site);
        }
        light_acquisition_cause(acquired, &frame->acquisition, cause, population);
    } else if (frame->owned->ownership_status == MC_WORLD_STORAGE_OK) {
        if (frame->owned->selection_ready &&
            frame->owned->selection.status == MC_CHUNK_ACQUIRE_OK &&
            light_records_quiet(frame->owned))
            frame->owned->selection_ready = false;
        else
            mc_world_runtime_execution_result_note(frame->owned, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                                   "World light completed GET outcome",
                                                   frame->receiver_site);
    }
    frame->native_stopped = frame->owned->ownership_status != MC_WORLD_STORAGE_OK;
    return acquired;
}

mc_world_storage_status mc_world_runtime_light_type_get(mc_world_runtime_classes *classes,
                                                        mc_light_kind kind,
                                                        const mc_world_light_type **out,
                                                        mc_world_storage_error *error) {
    if (classes == NULL || out == NULL || (kind != MC_LIGHT_SKY && kind != MC_LIGHT_BLOCK))
        return storage_result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                              "World Light class arguments");
    if (!classes->light_types_initialized) {
        classes->light_types[0] =
            (mc_world_light_type){.classes = classes, .fields = {.kind = MC_LIGHT_SKY, .c = 15}};
        classes->light_statics[0] = &classes->light_types[0];
        classes->light_types[1] =
            (mc_world_light_type){.classes = classes, .fields = {.kind = MC_LIGHT_BLOCK, .c = 0}};
        classes->light_statics[1] = &classes->light_types[1];
        classes->light_types_initialized = true;
    }
    *out = classes->light_statics[kind == MC_LIGHT_SKY ? 0u : 1u];
    return storage_result(error, MC_WORLD_STORAGE_OK, NULL);
}

const mc_world_light_type_fields *
mc_world_runtime_light_type_fields_get(const mc_world_light_type *type) {
    return type == NULL ? NULL : &type->fields;
}

const mc_world_light_region_fields *
mc_world_runtime_light_region_fields_get(const mc_world_light_region *region) {
    return region == NULL ? NULL : &region->fields;
}

static bool list_metadata_valid(const mc_world_reference_list *list) {
    return list != NULL && list->count <= list->capacity && list->capacity <= (size_t)INT32_MAX &&
           list->capacity <= SIZE_MAX / sizeof(*list->items) &&
           (list->capacity == 0 || list->items != NULL);
}

mc_world_storage_status mc_world_runtime_light_list_get(const mc_world_runtime *runtime,
                                                        mc_world_light_list_view *out,
                                                        mc_world_storage_error *error) {
    if (runtime == NULL || out == NULL)
        return storage_result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                              "World light list arguments");
    if (runtime->construction.mode != MC_WORLD_RUNTIME_GAME)
        return storage_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World light list mode");
    const mc_world_reference_list *list = runtime->fields.C;
    if (list != NULL && !list_metadata_valid(list))
        return storage_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "World light list metadata");
    *out = (mc_world_light_list_view){.identity = list,
                                      .count = list == NULL ? 0u : list->count,
                                      .capacity = list == NULL ? 0u : list->capacity};
    return storage_result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_runtime_light_at(const mc_world_runtime *runtime, size_t index,
                                                  const mc_world_light_region **out,
                                                  mc_world_storage_error *error) {
    if (runtime == NULL || out == NULL)
        return storage_result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                              "World light index arguments");
    if (runtime->construction.mode != MC_WORLD_RUNTIME_GAME)
        return storage_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World light index mode");
    const mc_world_reference_list *list = runtime->fields.C;
    if (!list_metadata_valid(list))
        return storage_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World light index list");
    if (index >= list->count)
        return storage_result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World light index range");
    *out = list->items[index];
    return storage_result(error, MC_WORLD_STORAGE_OK, NULL);
}

static void *request(mc_world_runtime *runtime, size_t bytes, mc_world_storage_error *cause,
                     const char *operation) {
    const size_t used = runtime->construction.direct_allocation_bytes;
    const size_t limit = runtime->construction_limits.max_direct_allocation_bytes;
    if (used > limit) {
        *cause =
            native_error(MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World light allocation accounting");
        return NULL;
    }
    if (bytes > limit - used) {
        *cause = native_error(MC_WORLD_STORAGE_LIMIT, operation);
        return NULL;
    }
    void *allocation = calloc(1u, bytes);
    if (allocation == NULL) {
        *cause = native_error(MC_WORLD_STORAGE_ALLOCATION_FAILED, operation);
        return NULL;
    }
    runtime->construction.direct_allocation_bytes += bytes;
    return allocation;
}

static void retire(mc_world_runtime *runtime, void *allocation, size_t bytes) {
    if (allocation != NULL) {
        free(allocation);
        runtime->construction.direct_allocation_bytes -= bytes;
    }
}

static bool list_load(mc_world_runtime *runtime, mc_world_reference_list **out,
                      mc_world_storage_error *cause) {
    mc_world_reference_list *list = runtime->fields.C;
    if (list == NULL) {
        *cause = native_error(MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World light C receiver is null");
        return false;
    }
    if (!list_metadata_valid(list)) {
        *cause = native_error(MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World light C metadata");
        return false;
    }
    *out = list;
    return true;
}

static bool list_grow(mc_world_runtime *runtime, mc_world_reference_list *list,
                      mc_world_storage_error *cause) {
    if (list->count < list->capacity)
        return true;
    if (list->count == (size_t)INT32_MAX) {
        *cause = native_error(MC_WORLD_STORAGE_LIMIT, "World light C entry limit");
        return false;
    }
    /* Bounded native pointer storage; charge old + new at peak. This growth
     * policy is not a Java per-object/JDK allocation-failure model. */
    const size_t maximum = (size_t)INT32_MAX < SIZE_MAX / sizeof(*list->items)
                               ? (size_t)INT32_MAX
                               : SIZE_MAX / sizeof(*list->items);
    const size_t needed = list->count + 1u;
    size_t capacity = list->capacity == 0 ? 10u : list->capacity;
    if (list->capacity != 0) {
        const size_t step = list->capacity / 2u;
        capacity = step > maximum - list->capacity ? maximum : list->capacity + step;
    }
    if (capacity < needed)
        capacity = needed;
    if (capacity > maximum) {
        *cause = native_error(MC_WORLD_STORAGE_LIMIT, "World light C pointer span");
        return false;
    }
    void **replacement = request(runtime, capacity * sizeof(*replacement), cause,
                                 "World light C pointer allocation");
    if (replacement == NULL)
        return false;
    if (list->count != 0)
        memcpy(replacement, list->items, list->count * sizeof(*replacement));
    void **previous = list->items;
    const size_t previous_bytes = list->capacity * sizeof(*previous);
    list->items = replacement;
    list->capacity = capacity;
    retire(runtime, previous, previous_bytes);
    return true;
}

static void list_clear(mc_world_runtime *runtime, mc_world_reference_list *list) {
    for (size_t i = 0; i < list->count; ++i) {
        retire(runtime, list->items[i], sizeof(mc_world_light_region));
        list->items[i] = NULL;
    }
    list->count = 0;
}

mc_world_storage_status mc_world_runtime_light_list_clear(mc_world_runtime *runtime,
                                                          mc_world_storage_error *error) {
    if (runtime == NULL)
        return storage_result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                              "World light clear arguments");
    if (runtime->construction.mode != MC_WORLD_RUNTIME_GAME ||
        runtime->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        return storage_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World light clear phase");
    mc_world_storage_error cause = {0};
    mc_world_storage_status status = mc_world_runtime_call_enter(runtime, &cause);
    if (status == MC_WORLD_STORAGE_OK) {
        mc_world_reference_list *list = NULL;
        if (list_load(runtime, &list, &cause))
            list_clear(runtime, list);
        else
            status = cause.status;
        mc_world_runtime_call_leave(runtime);
    }
    if (error != NULL)
        *error = cause;
    return status;
}

void mc_world_runtime_lighting_free(mc_world_runtime *runtime) {
    if (runtime == NULL || runtime->fields.C == NULL)
        return;
    mc_world_reference_list *list = runtime->fields.C;
    list_clear(runtime, list);
    retire(runtime, list->items, list->capacity * sizeof(*list->items));
    list->items = NULL;
    list->capacity = 0;
}

static bool same_class_type(const mc_world_runtime_classes *classes,
                            const mc_world_light_type *type) {
    if (type == NULL)
        return true;
    if (!classes->light_types_initialized ||
        (type != &classes->light_types[0] && type != &classes->light_types[1]))
        return false;
    return type->classes == classes;
}

static mc_world_storage_status
light_submit(mc_world_runtime *runtime, const mc_world_light_type *type,
             const mc_light_bounds *bounds, bool coalesce, mc_light_submit_result *out,
             mc_world_light_error *error, mc_world_light_execution *execution) {
    if (runtime == NULL || bounds == NULL || out == NULL || runtime->classes == NULL) {
        const mc_world_storage_error cause =
            native_error(MC_WORLD_STORAGE_INVALID_ARGUMENT, "World light submit arguments");
        return result(error, &cause, 0, 0, MC_CHUNK_ACQUIRE_OK);
    }
    if (runtime->construction.mode != MC_WORLD_RUNTIME_GAME ||
        runtime->construction.phase == MC_WORLD_RUNTIME_RETIRED) {
        const mc_world_storage_error cause =
            native_error(MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World light submit phase");
        return result(error, &cause, 0, 0, MC_CHUNK_ACQUIRE_OK);
    }
    if (!same_class_type(runtime->classes, type)) {
        const mc_world_storage_error cause =
            native_error(MC_WORLD_STORAGE_INVALID_ARGUMENT, "World light type class");
        return result(error, &cause, 0, 0, MC_CHUNK_ACQUIRE_OK);
    }
    const mc_light_bounds requested = *bounds;
    mc_world_storage_error cause = {0};
    mc_world_storage_status status = mc_world_runtime_call_enter(runtime, &cause);
    if (status != MC_WORLD_STORAGE_OK)
        return result(error, &cause, 0, 0, MC_CHUNK_ACQUIRE_OK);

    unsigned int bytecode = 4, provider_bytecode = 0;
    mc_chunk_acquire_status acquisition = MC_CHUNK_ACQUIRE_OK;
    mc_world_population_context population = {0};
    mc_light_submit_result submitted = MC_LIGHT_SUBMIT_SKIPPED;
    bool depth_entered = false;
    mc_world_light_region *unpublished = NULL;
    mc_world_provider_fields dimension;
    if (mc_world_provider_fields_get(runtime->fields.t, &dimension) != MC_WORLD_PROVIDER_OK) {
        cause = native_error(MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World light Dimension receiver");
        goto failed;
    }
    if (dimension.e) {
        bytecode = 11;
        const mc_world_light_type *sky = NULL;
        status = mc_world_runtime_light_type_get(runtime->classes, MC_LIGHT_SKY, &sky, &cause);
        if (status != MC_WORLD_STORAGE_OK)
            goto failed;
        if (type == sky)
            goto success;
    }

    runtime->classes->enqueue_depth = mc_java_i32_add(runtime->classes->enqueue_depth, 1);
    depth_entered = true;
    if (runtime->classes->enqueue_depth == 50)
        goto success;

    const int32_t midpoint_x = mc_java_i32_add(requested.max_x, requested.min_x) / 2;
    const int32_t midpoint_z = mc_java_i32_add(requested.max_z, requested.min_z) / 2;
    const int32_t chunk_x = mc_java_i32_shr(midpoint_x, 4);
    const int32_t chunk_z = mc_java_i32_shr(midpoint_z, 4);
    bool present = false;
    bytecode = 67;
    provider_bytecode = 6;
    status = mc_world_runtime_chunk_provider_contains(runtime, chunk_x, chunk_z, &present, &cause);
    if (status != MC_WORLD_STORAGE_OK)
        goto failed;
    if (!present)
        goto success;

    /* The getter evaluates the current provider independently of presence. */
    bytecode = 87;
    if (runtime->fields.v == NULL) {
        cause = native_error(MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World light getter provider");
        goto failed;
    }
    mc_chunk_state *chunk = NULL;
    acquisition =
        mc_world_light_get_chunk(execution, runtime, chunk_x, chunk_z, &chunk, &cause, &population);
    if (acquisition != MC_CHUNK_ACQUIRE_OK) {
        if (cause.status == MC_WORLD_STORAGE_OK)
            cause =
                acquisition == MC_CHUNK_ACQUIRE_LIMIT ||
                        acquisition == MC_CHUNK_ACQUIRE_PARTIAL_LIMIT
                    ? native_error(MC_WORLD_STORAGE_LIMIT, "World light getter acquisition limit")
                    : native_error(MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                   "World light getter acquisition");
        goto failed;
    }
    if (execution != NULL && execution->native_stopped) {
        status = result(error, &cause, bytecode, provider_bytecode, acquisition);
        goto leave;
    }
    bytecode = 90;
    provider_bytecode = 0;
    if (chunk == NULL) {
        cause = native_error(MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World light Chunk receiver");
        goto failed;
    }
    if (chunk->blank)
        goto success;

    mc_world_reference_list *list = NULL;
    bytecode = 109;
    if (!list_load(runtime, &list, &cause))
        goto failed;
    const size_t examined = list->count < 5u ? list->count : 5u;
    if (coalesce) {
        for (size_t i = 0; i < examined; ++i) {
            bytecode = 153;
            mc_world_reference_list *current = NULL;
            if (!list_load(runtime, &current, &cause))
                goto failed;
            bytecode = 163;
            if (current->count <= i) {
                cause = native_error(MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World light C get index");
                goto failed;
            }
            mc_world_light_region *region = current->items[current->count - i - 1u];
            bytecode = 175;
            if (region == NULL) {
                cause =
                    native_error(MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World light region receiver");
                goto failed;
            }
            if (region->fields.a == type) {
                bytecode = 194;
                if (mc_light_bounds_merge(&region->fields.bounds, &requested)) {
                    submitted = MC_LIGHT_SUBMIT_MERGED;
                    goto success;
                }
            }
        }
    }

    /* The source has evaluated C216 before new219/constructor234/add237. */
    list = runtime->fields.C;
    bytecode = 219;
    unpublished = request(runtime, sizeof(*unpublished), &cause, "World light region allocation");
    if (unpublished == NULL)
        goto failed;
    unpublished->fields = (mc_world_light_region_fields){.a = type, .bounds = requested};
    bytecode = 237;
    if (!list_metadata_valid(list)) {
        cause = native_error(MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World light C add receiver");
        goto failed;
    }
    if (!list_grow(runtime, list, &cause))
        goto failed;
    list->items[list->count++] = unpublished;
    unpublished = NULL;
    submitted = MC_LIGHT_SUBMIT_APPENDED;

    bytecode = 251;
    if (!list_load(runtime, &list, &cause))
        goto failed;
    if (list->count > (size_t)MC_LIGHT_QUEUE_LIMIT) {
        bytecode = 289;
        static const char diagnostic[] = "More than 1000000 updates, aborting lighting updates";
        mc_text_stdout output;
        bool io_failed = false;
        mc_text_stdout_status printed = mc_text_stdout_capture(&output);
        if (printed == MC_TEXT_STDOUT_OK)
            printed =
                mc_text_stdout_println(&output, diagnostic, sizeof(diagnostic) - 1u, &io_failed);
        if (printed != MC_TEXT_STDOUT_OK || io_failed) {
            cause = native_error(MC_WORLD_STORAGE_IO_ERROR, "World light abort diagnostic");
            goto failed;
        }
        bytecode = 296;
        if (!list_load(runtime, &list, &cause))
            goto failed;
        list_clear(runtime, list);
        submitted = MC_LIGHT_SUBMIT_CLEARED;
    }

success:
    cause = native_error(MC_WORLD_STORAGE_OK, NULL);
    *out = submitted;
    status = result(error, &cause, 0, 0, acquisition);
    goto leave;
failed:
    runtime->last_error = cause;
    status = result(error, &cause, bytecode, provider_bytecode, acquisition);
    if (error != NULL)
        error->population = population;
leave:
    retire(runtime, unpublished, sizeof(*unpublished));
    if (depth_entered)
        runtime->classes->enqueue_depth = mc_java_i32_sub(runtime->classes->enqueue_depth, 1);
    mc_world_runtime_call_leave(runtime);
    return status;
}

mc_world_storage_status mc_world_runtime_light_submit(mc_world_runtime *runtime,
                                                      const mc_world_light_type *type,
                                                      const mc_light_bounds *bounds, bool coalesce,
                                                      mc_light_submit_result *out,
                                                      mc_world_light_error *error) {
    return light_submit(runtime, type, bounds, coalesce, out, error, NULL);
}

bool mc_world_runtime_light_submit_in_execution(
    mc_world_light_execution *frame, mc_world_runtime *world, const mc_world_light_type *type,
    const mc_light_bounds *bounds, bool coalesce, mc_light_submit_result *out,
    mc_world_light_error *error, mc_world_storage_status *status) {
    if (!mc_world_light_execution_enter(frame, world, out, error, status))
        return false;
    const mc_world_storage_status actual =
        light_submit(world, type, bounds, coalesce, out, error, frame);
    return mc_world_light_execution_finish(frame, actual, status);
}
