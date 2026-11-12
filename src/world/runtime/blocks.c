#include "mc/world/runtime/blocks.h"

#include "../../block/registry/runtime_internal.h"
#include "../../world/chunk/object_state_internal.h"
#include "../../world/storage/objects_internal.h"
#include "acquisition_internal.h"
#include "blocks_execution_internal.h"
#include "construction_internal.h"
#include "execution_children_internal.h"
#include "mc/block/registry/rendering.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "runtime_internal.h"

static mc_world_storage_status result(mc_world_storage_error *error, mc_world_storage_status status,
                                      const char *operation) {
    if (error != NULL)
        *error = (mc_world_storage_error){status, operation, 0};
    return status;
}

static bool in_domain(int32_t x, int32_t y, int32_t z) {
    return x >= -32000000 && x < 32000000 && z >= -32000000 && z <= 32000000 && y >= 0 && y < 128;
}

static bool scalar_records_quiet(const mc_world_runtime_execution_result *owned) {
    if (owned->root_pending)
        return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (owned->pending[i].used)
            return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (owned->closes[i].used)
            return false;
    return true;
}

static bool scalar_result_quiet(const mc_world_runtime_execution_result *owned) {
    return !owned->selection_ready && scalar_records_quiet(owned);
}

static void scalar_selected_view(mc_world_block_execution *execution) {
    if (!execution->owned->selection_ready)
        return;
    mc_entity_tick_execution_loader_view selected;
    if (mc_entity_tick_execution_loader_read(execution->execution, &execution->owned->selection,
                                             &selected))
        execution->acquisition.source_chunk =
            selected.status == MC_CHUNK_ACQUIRE_OK ? NULL : selected.failure;
    else {
        execution->acquisition.source_chunk = NULL;
        mc_world_runtime_execution_result_note(execution->owned, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                               "World scalar selected loader view",
                                               execution->receiver_site);
    }
}

static mc_world_storage_status
acquire_chunk_detailed(mc_world_runtime *runtime, int32_t x, int32_t z, mc_chunk_state **out,
                       mc_world_storage_error *error, mc_world_runtime_acquisition_error *detail,
                       bool *acquisition_failed, mc_world_block_execution *execution) {
    /* World.c reads v before invoking provider.b, including a cached hit. */
    if (runtime->fields.v == NULL)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World block provider receiver");
    mc_chunk_acquire_status status;
    if (execution == NULL)
        status = mc_world_runtime_get_failure(runtime, mc_java_i32_shr(x, 4), mc_java_i32_shr(z, 4),
                                              out, detail);
    else {
        execution->get_entered = true;
        status = mc_world_runtime_get_failure_in_execution(
            execution->execution, runtime, execution->code, mc_java_i32_shr(x, 4),
            mc_java_i32_shr(z, 4), out, detail, execution->owned);
        execution->get_status = status;
        execution->get_returned = true;
        /* Only an actual failed GET supplies failure-only diagnostic storage.
         * The kernel already retained its real selected/pending outcome. */
        if (status != MC_CHUNK_ACQUIRE_OK)
            scalar_selected_view(execution);
        execution->native_stopped = execution->owned->ownership_status != MC_WORLD_STORAGE_OK;
    }
    if (status == MC_CHUNK_ACQUIRE_OK) {
        if (execution != NULL && !execution->native_stopped) {
            if (execution->owned->selection_ready &&
                execution->owned->selection.status == MC_CHUNK_ACQUIRE_OK &&
                scalar_records_quiet(execution->owned))
                execution->owned->selection_ready = false;
            else {
                mc_world_runtime_execution_result_note(
                    execution->owned, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "World scalar completed GET outcome", execution->receiver_site);
                execution->native_stopped = true;
            }
        }
        /* Raw Source success remains OK even when native completion refuses.
         * Every shared scalar body checks native_stopped before payload work. */
        return MC_WORLD_STORAGE_OK;
    }
    if (acquisition_failed != NULL)
        *acquisition_failed = true;
    if (detail->cause.status != MC_WORLD_STORAGE_OK) {
        if (error != NULL)
            *error = detail->cause;
        return detail->cause.status;
    }
    if (status == MC_CHUNK_ACQUIRE_LIMIT || status == MC_CHUNK_ACQUIRE_PARTIAL_LIMIT)
        return result(error, MC_WORLD_STORAGE_LIMIT, "World block acquisition limit");
    return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World block acquisition");
}

static mc_world_storage_status acquire_chunk(mc_world_runtime *runtime, int32_t x, int32_t z,
                                             mc_chunk_state **out, mc_world_storage_error *error) {
    mc_world_runtime_acquisition_error detail;
    return acquire_chunk_detailed(runtime, x, z, out, error, &detail, NULL, NULL);
}

static mc_world_storage_status block_get(mc_world_runtime *runtime, int32_t x, int32_t y, int32_t z,
                                         int32_t *out, mc_world_storage_error *error,
                                         mc_world_runtime_acquisition_error *acquisition,
                                         bool *acquisition_failed,
                                         mc_world_block_execution *execution) {
    if (runtime == NULL || out == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World block arguments");
    if (runtime->construction.mode != MC_WORLD_RUNTIME_GAME ||
        runtime->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World block phase");
    if (!in_domain(x, y, z)) {
        *out = 0;
        return result(error, MC_WORLD_STORAGE_OK, NULL);
    }
    mc_chunk_state *chunk = NULL;
    mc_world_storage_status status =
        acquisition == NULL ? acquire_chunk(runtime, x, z, &chunk, error)
                            : acquire_chunk_detailed(runtime, x, z, &chunk, error, acquisition,
                                                     acquisition_failed, execution);
    if (status != MC_WORLD_STORAGE_OK || (execution != NULL && execution->native_stopped))
        return status;
    if (chunk->blank) {
        *out = 0;
        return result(error, MC_WORLD_STORAGE_OK, NULL);
    }
    if (chunk->data == NULL || chunk->data->blocks == NULL ||
        chunk->data->blocks_capacity < MC_CHUNK_BLOCKS)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World block chunk payload");
    size_t index = ((size_t)((uint32_t)x & 15u) * 16u + ((uint32_t)z & 15u)) * 128u + (size_t)y;
    *out = (int32_t)chunk->data->blocks[index];
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_runtime_block_get(mc_world_runtime *runtime, int32_t x, int32_t y,
                                                   int32_t z, int32_t *out,
                                                   mc_world_storage_error *error) {
    return block_get(runtime, x, y, z, out, error, NULL, NULL, NULL);
}

static void acquisition_reset(mc_world_runtime_acquisition_error *acquisition) {
    static const mc_world_runtime_acquisition_error zero = {.status = MC_CHUNK_ACQUIRE_OK};
    *acquisition = zero;
    mc_live_tile_entities_error_init(&acquisition->tile_entity);
    acquisition->registry = acquisition->tile_entity.registry;
    mc_world_entity_load_error_init(&acquisition->load_entity);
    mc_world_entity_batch_error_init(&acquisition->batch);
}

mc_world_storage_status
mc_world_runtime_block_get_detailed(mc_world_runtime *runtime, int32_t x, int32_t y, int32_t z,
                                    int32_t *out, mc_world_storage_error *error,
                                    mc_world_runtime_acquisition_error *acquisition) {
    if (acquisition == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                      "World detailed block acquisition output");
    acquisition_reset(acquisition);
    return block_get(runtime, x, y, z, out, error, acquisition, NULL, NULL);
}

mc_world_storage_status
mc_world_runtime_block_get_failure(mc_world_runtime *runtime, int32_t x, int32_t y, int32_t z,
                                   int32_t *out, mc_world_storage_error *error,
                                   mc_world_runtime_acquisition_error *acquisition) {
    if (acquisition == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                      "World detailed block acquisition output");
    bool acquisition_failed = false;
    const mc_world_storage_status status =
        block_get(runtime, x, y, z, out, error, acquisition, &acquisition_failed, NULL);
    if (status != MC_WORLD_STORAGE_OK && !acquisition_failed)
        acquisition_reset(acquisition);
    return status;
}

static mc_world_storage_status metadata_get(mc_world_runtime *runtime, int32_t x, int32_t y,
                                            int32_t z, int32_t *out, mc_world_storage_error *error,
                                            mc_world_runtime_acquisition_error *acquisition,
                                            bool *acquisition_failed,
                                            mc_world_metadata_source_error *source,
                                            mc_world_block_execution *execution) {
    if (source != NULL)
        *source = (mc_world_metadata_source_error){0};
    if (runtime == NULL || out == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World metadata arguments");
    if (runtime->construction.mode != MC_WORLD_RUNTIME_GAME ||
        runtime->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World metadata phase");
    if (!in_domain(x, y, z)) {
        *out = 0;
        return result(error, MC_WORLD_STORAGE_OK, NULL);
    }
    mc_chunk_state *chunk = NULL;
    mc_world_storage_status status =
        acquisition == NULL ? acquire_chunk(runtime, x, z, &chunk, error)
                            : acquire_chunk_detailed(runtime, x, z, &chunk, error, acquisition,
                                                     acquisition_failed, execution);
    if (status != MC_WORLD_STORAGE_OK || (execution != NULL && execution->native_stopped))
        return status;
    if (chunk->blank) {
        *out = 0;
        return result(error, MC_WORLD_STORAGE_OK, NULL);
    }
    mc_world_chunk *object = mc_world_store_chunk_object(runtime->fields.w, chunk);
    if (object != NULL) {
        const mc_world_chunk_nibble *nibble = object->e;
        const char *exception = NULL;
        const char *operation = NULL;
        unsigned int nibble_pc = 0;
        if (nibble == NULL) {
            exception = "java/lang/NullPointerException";
            operation = "World metadata actual Chunk.e null receiver";
        } else {
            const int32_t cell = mc_java_i32_shl(x & 15, 11) | mc_java_i32_shl(z & 15, 7) | y;
            const int32_t index = mc_java_i32_shr(cell, 1);
            nibble_pc = ((uint32_t)cell & 1u) == 0 ? 36u : 47u;
            if (nibble->a == NULL) {
                exception = "java/lang/NullPointerException";
                operation = "World metadata actual NibbleArray.a null backing";
            } else if (index < 0 || (size_t)index >= nibble->length) {
                exception = "java/lang/ArrayIndexOutOfBoundsException";
                operation = "World metadata actual NibbleArray.a index";
            } else {
                *out =
                    (int32_t)(((uint32_t)nibble->a[index] >> (((uint32_t)cell & 1u) * 4u)) & 15u);
                return result(error, MC_WORLD_STORAGE_OK, NULL);
            }
        }
        if (source != NULL)
            *source = (mc_world_metadata_source_error){true, exception, 68, 7, nibble_pc};
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, operation);
    }
    /* Generic stored states have no source Chunk object. Preserve their
     * preexisting native representation; bound GAME objects above read e. */
    if (chunk->data == NULL)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World metadata owner");
    uint8_t metadata;
    if (!mc_chunk_nibbles_get(chunk->data->metadata, MC_CHUNK_NIBBLE_BYTES,
                              (int32_t)((uint32_t)x & 15u), y, (int32_t)((uint32_t)z & 15u),
                              &metadata))
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World metadata payload");
    *out = metadata;
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_runtime_metadata_get(mc_world_runtime *runtime, int32_t x,
                                                      int32_t y, int32_t z, int32_t *out,
                                                      mc_world_storage_error *error) {
    return metadata_get(runtime, x, y, z, out, error, NULL, NULL, NULL, NULL);
}

mc_world_storage_status
mc_world_runtime_metadata_get_detailed(mc_world_runtime *runtime, int32_t x, int32_t y, int32_t z,
                                       int32_t *out, mc_world_storage_error *error,
                                       mc_world_runtime_acquisition_error *acquisition) {
    if (acquisition == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                      "World detailed metadata acquisition output");
    acquisition_reset(acquisition);
    return metadata_get(runtime, x, y, z, out, error, acquisition, NULL, NULL, NULL);
}

mc_world_storage_status
mc_world_runtime_metadata_get_failure(mc_world_runtime *runtime, int32_t x, int32_t y, int32_t z,
                                      int32_t *out, mc_world_storage_error *error,
                                      mc_world_runtime_acquisition_error *acquisition) {
    if (acquisition == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                      "World detailed metadata acquisition output");
    bool acquisition_failed = false;
    const mc_world_storage_status status =
        metadata_get(runtime, x, y, z, out, error, acquisition, &acquisition_failed, NULL, NULL);
    if (status != MC_WORLD_STORAGE_OK && !acquisition_failed)
        acquisition_reset(acquisition);
    return status;
}

mc_world_storage_status
mc_world_runtime_metadata_get_source_failure(mc_world_runtime *runtime, int32_t x, int32_t y,
                                             int32_t z, int32_t *out, mc_world_storage_error *error,
                                             mc_world_runtime_acquisition_error *acquisition,
                                             mc_world_metadata_source_error *source) {
    if (acquisition == NULL || source == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                      "World metadata source diagnostic outputs");
    bool acquisition_failed = false;
    const mc_world_storage_status status =
        metadata_get(runtime, x, y, z, out, error, acquisition, &acquisition_failed, source, NULL);
    if (status != MC_WORLD_STORAGE_OK && !acquisition_failed)
        acquisition_reset(acquisition);
    return status;
}

static mc_world_storage_status
metadata_set_quiet(mc_world_runtime *runtime, int32_t x, int32_t y, int32_t z, int32_t metadata,
                   bool *out, mc_world_storage_error *error,
                   mc_world_runtime_acquisition_error *acquisition, bool *acquisition_failed,
                   mc_world_metadata_source_error *source, mc_world_block_execution *execution) {
    if (source != NULL)
        *source = (mc_world_metadata_source_error){0};
    if (runtime == NULL || out == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World metadata arguments");
    if (runtime->construction.mode != MC_WORLD_RUNTIME_GAME ||
        runtime->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World metadata phase");
    if (!in_domain(x, y, z)) {
        *out = false;
        return result(error, MC_WORLD_STORAGE_OK, NULL);
    }
    mc_chunk_state *chunk = NULL;
    mc_world_storage_status status =
        acquisition == NULL ? acquire_chunk(runtime, x, z, &chunk, error)
                            : acquire_chunk_detailed(runtime, x, z, &chunk, error, acquisition,
                                                     acquisition_failed, execution);
    if (status != MC_WORLD_STORAGE_OK || (execution != NULL && execution->native_stopped))
        return status;
    if (!chunk->blank) {
        chunk->dirty = true;
        mc_world_chunk *object = mc_world_store_chunk_object(runtime->fields.w, chunk);
        if (object != NULL) {
            const mc_world_chunk_nibble *nibble = object->e;
            const char *exception = NULL;
            const char *operation = NULL;
            unsigned int nibble_pc = 0;
            if (nibble == NULL) {
                exception = "java/lang/NullPointerException";
                operation = "World metadata setter actual Chunk.e null receiver";
            } else {
                const int32_t cell = mc_java_i32_shl(x & 15, 11) | mc_java_i32_shl(z & 15, 7) | y;
                const int32_t index = mc_java_i32_shr(cell, 1);
                const bool odd = ((uint32_t)cell & 1u) != 0;
                nibble_pc = odd ? 70u : 42u;
                if (nibble->a == NULL) {
                    exception = "java/lang/NullPointerException";
                    operation = "World metadata setter actual NibbleArray.a null backing";
                } else if (index < 0 || (size_t)index >= nibble->length) {
                    exception = "java/lang/ArrayIndexOutOfBoundsException";
                    operation = "World metadata setter actual NibbleArray.a index";
                } else {
                    const uint32_t old = nibble->a[index];
                    const uint32_t value = (uint32_t)metadata & 15u;
                    nibble->a[index] =
                        (uint8_t)(odd ? (old & 15u) | (value << 4) : (old & 240u) | value);
                    *out = true;
                    return result(error, MC_WORLD_STORAGE_OK, NULL);
                }
            }
            if (source != NULL)
                *source = (mc_world_metadata_source_error){true, exception, 70, 14, nibble_pc};
            return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, operation);
        }
        /* Generic stored states have no source Chunk object. Preserve their
         * existing fixed-plane representation, as in the metadata getter. */
        if (chunk->data == NULL)
            return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World metadata owner");
        if (!mc_chunk_nibbles_set(chunk->data->metadata, MC_CHUNK_NIBBLE_BYTES,
                                  (int32_t)((uint32_t)x & 15u), y, (int32_t)((uint32_t)z & 15u),
                                  metadata))
            return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World metadata payload");
    }
    *out = true;
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_runtime_metadata_set_quiet(mc_world_runtime *runtime, int32_t x,
                                                            int32_t y, int32_t z, int32_t metadata,
                                                            bool *out,
                                                            mc_world_storage_error *error) {
    return metadata_set_quiet(runtime, x, y, z, metadata, out, error, NULL, NULL, NULL, NULL);
}

mc_world_storage_status mc_world_runtime_metadata_set_quiet_source_failure(
    mc_world_runtime *runtime, int32_t x, int32_t y, int32_t z, int32_t metadata, bool *out,
    mc_world_storage_error *error, mc_world_runtime_acquisition_error *acquisition,
    mc_world_metadata_source_error *source) {
    if (acquisition == NULL || source == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                      "World metadata setter source diagnostic outputs");
    bool acquisition_failed = false;
    const mc_world_storage_status status = metadata_set_quiet(
        runtime, x, y, z, metadata, out, error, acquisition, &acquisition_failed, source, NULL);
    if (status != MC_WORLD_STORAGE_OK && !acquisition_failed)
        acquisition_reset(acquisition);
    return status;
}

void mc_world_block_execution_init(mc_world_block_execution *frame,
                                   mc_entity_tick_execution *execution,
                                   mc_world_runtime_execution_code code,
                                   mc_entity_tick_execution_site receiver_site,
                                   mc_world_runtime_execution_result *owned) {
    if (frame != NULL)
        *frame = (mc_world_block_execution){
            .execution = execution, .code = code, .receiver_site = receiver_site, .owned = owned};
    mc_world_runtime_execution_result_init(owned);
}

static bool scalar_enter(mc_world_block_execution *frame, mc_world_runtime *world, void *out,
                         mc_world_storage_error *error, mc_world_storage_status *status) {
    if (frame == NULL)
        return false;
    if (frame->execution == NULL || frame->execution->identity != frame->execution ||
        frame->owned == NULL || out == NULL || error == NULL || status == NULL || frame->entered) {
        mc_world_runtime_execution_result_note(frame->owned, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                               "World scalar execution/fresh outputs required",
                                               frame->receiver_site);
        return false;
    }
    if (frame->owned->ownership_status != MC_WORLD_STORAGE_OK ||
        !scalar_result_quiet(frame->owned)) {
        mc_world_runtime_execution_result_note(frame->owned, MC_WORLD_STORAGE_BUSY,
                                               "World scalar fresh result required",
                                               frame->receiver_site);
        return false;
    }
    mc_world_access_failure_lease *unused = NULL;
    const mc_world_storage_status admitted =
        mc_world_execution_child_lease(frame->execution, world, &unused);
    if (admitted != MC_WORLD_STORAGE_OK) {
        mc_world_runtime_execution_result_note(
            frame->owned, admitted, "World scalar evaluated World ownership", frame->receiver_site);
        return false;
    }
    /* The caller's exact scope already holds World. The shared GET enters its
     * own actual call only when domain guards reach it; no unused call is added. */
    frame->entered = true;
    return true;
}

static bool scalar_finish(mc_world_block_execution *frame, mc_world_storage_status actual,
                          mc_world_storage_status *status) {
    *status = actual;
    /* Any participating native refusal was checked immediately after the one
     * GET, before Source payload work. Nothing after it invokes another child. */
    frame->completed = actual == MC_WORLD_STORAGE_OK && !frame->native_stopped &&
                       frame->owned->ownership_status == MC_WORLD_STORAGE_OK;
    return frame->completed;
}

bool mc_world_runtime_block_get_in_execution(mc_world_block_execution *frame,
                                             mc_world_runtime *world, int32_t x, int32_t y,
                                             int32_t z, int32_t *out, mc_world_storage_error *error,
                                             mc_world_storage_status *status) {
    if (!scalar_enter(frame, world, out, error, status))
        return false;
    const mc_world_storage_status actual =
        block_get(world, x, y, z, out, error, &frame->acquisition, NULL, frame);
    return scalar_finish(frame, actual, status);
}

bool mc_world_runtime_metadata_get_in_execution(mc_world_block_execution *frame,
                                                mc_world_runtime *world, int32_t x, int32_t y,
                                                int32_t z, int32_t *out,
                                                mc_world_storage_error *error,
                                                mc_world_storage_status *status) {
    if (!scalar_enter(frame, world, out, error, status))
        return false;
    const mc_world_storage_status actual =
        metadata_get(world, x, y, z, out, error, &frame->acquisition, NULL, &frame->source, frame);
    return scalar_finish(frame, actual, status);
}

bool mc_world_runtime_metadata_set_quiet_in_execution(mc_world_block_execution *frame,
                                                      mc_world_runtime *world, int32_t x, int32_t y,
                                                      int32_t z, int32_t metadata, bool *out,
                                                      mc_world_storage_error *error,
                                                      mc_world_storage_status *status) {
    if (!scalar_enter(frame, world, out, error, status))
        return false;
    const mc_world_storage_status actual = metadata_set_quiet(
        world, x, y, z, metadata, out, error, &frame->acquisition, NULL, &frame->source, frame);
    return scalar_finish(frame, actual, status);
}

mc_world_storage_status mc_world_runtime_top_block(mc_world_runtime *runtime, int32_t x, int32_t z,
                                                   int32_t *out, mc_world_storage_error *error) {
    if (out == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World top-block output");
    int32_t y = 63;
    int32_t block;
    for (;;) {
        mc_world_storage_status status =
            mc_world_runtime_block_get(runtime, x, y + 1, z, &block, error);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
        if (block == 0)
            break;
        ++y;
    }
    return mc_world_runtime_block_get(runtime, x, y, z, out, error);
}

mc_world_storage_status mc_world_runtime_opaque_get(mc_world_runtime *world, int32_t x, int32_t y,
                                                    int32_t z, bool *out,
                                                    mc_world_opaque_error *error) {
    mc_world_opaque_error frame = {0};
    frame.registry = (mc_registry_error){.initializer_pc = -1,
                                         .block_id = -1,
                                         .constructor_pc = -1,
                                         .active_initializer_pc = -1,
                                         .item_argument = -1};
    frame.native_policy = true;
    if (world == NULL || out == NULL) {
        frame.status = MC_WORLD_STORAGE_INVALID_ARGUMENT;
        frame.operation = "World opacity arguments";
        goto publish;
    }
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED ||
        world->classes->side != MC_WORLD_INFO_CLIENT) {
        frame.status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        frame.operation = "World opacity client GAME admission";
        goto publish;
    }
    frame.status = mc_world_runtime_call_enter(world, &frame.cause);
    if (frame.status != MC_WORLD_STORAGE_OK) {
        frame.operation = frame.cause.operation;
        goto publish;
    }
    mc_registry_classes *classes = mc_world_runtime_registry(world);
    mc_registry_block_m_view array;
    const mc_block *block;
    if (mc_registry_block_m_get(classes, &array, &frame.registry) != MC_REGISTRY_OK) {
        frame.status =
            mc_world_runtime_registry_result(world, frame.registry.status, &frame.registry,
                                             "World opacity Block.m active use", &frame.cause);
        frame.operation = frame.cause.operation;
        goto leave;
    }
    frame.bytecode = 7;
    int32_t id;
    frame.status = mc_world_runtime_block_get(world, x, y, z, &id, &frame.cause);
    if (frame.status != MC_WORLD_STORAGE_OK) {
        frame.operation = frame.cause.operation;
        goto leave;
    }
    frame.bytecode = 10;
    if (mc_registry_block_m_at(&array, id, &block, &frame.registry) != MC_REGISTRY_OK) {
        frame.native_policy = frame.registry.status != MC_REGISTRY_NULL_REFERENCE &&
                              frame.registry.status != MC_REGISTRY_INDEX_OUT_OF_RANGE;
        frame.status = mc_world_runtime_registry_result(
            world, frame.registry.status, &frame.registry, "World opacity captured Block.m element",
            &frame.cause);
        frame.operation = frame.cause.operation;
        goto leave;
    }
    if (block == NULL) {
        *out = false;
        frame.bytecode = 19;
    } else {
        frame.bytecode = 22;
        *out = mc_block_render_opaque(block);
        frame.bytecode = 25;
    }
    frame.native_policy = false;
leave:
    if (frame.status != MC_WORLD_STORAGE_OK) {
        if (frame.cause.operation == NULL)
            frame.cause = (mc_world_storage_error){frame.status, frame.operation, 0};
        world->last_error = frame.cause;
    }
    mc_world_runtime_call_leave(world);
publish:
    if (frame.status != MC_WORLD_STORAGE_OK && frame.cause.operation == NULL)
        frame.cause = (mc_world_storage_error){frame.status, frame.operation, 0};
    if (error != NULL)
        *error = frame;
    return frame.status;
}

mc_world_storage_status mc_world_runtime_chunk_manager(mc_world_runtime *world,
                                                       mc_world_chunk_manager **out,
                                                       mc_world_manager_error *error) {
    mc_world_manager_error frame = {0};
    frame.native_policy = true;
    if (world == NULL || out == NULL) {
        frame.status = MC_WORLD_STORAGE_INVALID_ARGUMENT;
        frame.operation = "World manager arguments";
        goto publish;
    }
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED ||
        world->classes->side != MC_WORLD_INFO_CLIENT) {
        frame.status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        frame.operation = "World manager client GAME admission";
        goto publish;
    }
    frame.status = mc_world_runtime_call_enter(world, &frame.cause);
    if (frame.status != MC_WORLD_STORAGE_OK) {
        frame.operation = frame.cause.operation;
        goto publish;
    }
    frame.bytecode = 4;
    mc_world_provider *dimension = world->fields.t;
    if (dimension == NULL) {
        frame.status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        frame.operation = "actual World manager null Dimension receiver";
        frame.native_policy = false;
    } else {
        mc_world_provider_fields fields;
        frame.provider = mc_world_provider_fields_get(dimension, &fields);
        if (frame.provider != MC_WORLD_PROVIDER_OK) {
            frame.status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
            frame.operation = "World manager actual Dimension fields";
        } else {
            *out = fields.b;
            frame.bytecode = 7;
            frame.native_policy = false;
        }
    }
    if (frame.status != MC_WORLD_STORAGE_OK) {
        if (frame.cause.operation == NULL)
            frame.cause = (mc_world_storage_error){frame.status, frame.operation, 0};
        world->last_error = frame.cause;
    }
    mc_world_runtime_call_leave(world);
publish:
    if (frame.status != MC_WORLD_STORAGE_OK && frame.cause.operation == NULL)
        frame.cause = (mc_world_storage_error){frame.status, frame.operation, 0};
    if (error != NULL)
        *error = frame;
    return frame.status;
}
