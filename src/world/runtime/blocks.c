#include "mc/world/runtime/blocks.h"

#include "../../block/registry/runtime_internal.h"
#include "construction_internal.h"
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

static mc_world_storage_status acquire_chunk_detailed(mc_world_runtime *runtime, int32_t x,
                                                      int32_t z, mc_chunk_state **out,
                                                      mc_world_storage_error *error,
                                                      mc_world_runtime_acquisition_error *detail,
                                                      bool *acquisition_failed) {
    /* World.c reads v before invoking provider.b, including a cached hit. */
    if (runtime->fields.v == NULL)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World block provider receiver");
    mc_chunk_acquire_status status = mc_world_runtime_get_failure(
        runtime, mc_java_i32_shr(x, 4), mc_java_i32_shr(z, 4), out, detail);
    if (status == MC_CHUNK_ACQUIRE_OK)
        return MC_WORLD_STORAGE_OK;
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
    return acquire_chunk_detailed(runtime, x, z, out, error, &detail, NULL);
}

static mc_world_storage_status block_get(mc_world_runtime *runtime, int32_t x, int32_t y, int32_t z,
                                         int32_t *out, mc_world_storage_error *error,
                                         mc_world_runtime_acquisition_error *acquisition,
                                         bool *acquisition_failed) {
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
        acquisition == NULL
            ? acquire_chunk(runtime, x, z, &chunk, error)
            : acquire_chunk_detailed(runtime, x, z, &chunk, error, acquisition, acquisition_failed);
    if (status != MC_WORLD_STORAGE_OK)
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
    return block_get(runtime, x, y, z, out, error, NULL, NULL);
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
    return block_get(runtime, x, y, z, out, error, acquisition, NULL);
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
        block_get(runtime, x, y, z, out, error, acquisition, &acquisition_failed);
    if (status != MC_WORLD_STORAGE_OK && !acquisition_failed)
        acquisition_reset(acquisition);
    return status;
}

static mc_world_storage_status metadata_get(mc_world_runtime *runtime, int32_t x, int32_t y,
                                            int32_t z, int32_t *out, mc_world_storage_error *error,
                                            mc_world_runtime_acquisition_error *acquisition) {
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
        acquisition == NULL
            ? acquire_chunk(runtime, x, z, &chunk, error)
            : acquire_chunk_detailed(runtime, x, z, &chunk, error, acquisition, NULL);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (chunk->blank) {
        *out = 0;
        return result(error, MC_WORLD_STORAGE_OK, NULL);
    }
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
    return metadata_get(runtime, x, y, z, out, error, NULL);
}

mc_world_storage_status
mc_world_runtime_metadata_get_detailed(mc_world_runtime *runtime, int32_t x, int32_t y, int32_t z,
                                       int32_t *out, mc_world_storage_error *error,
                                       mc_world_runtime_acquisition_error *acquisition) {
    if (acquisition == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                      "World detailed metadata acquisition output");
    acquisition_reset(acquisition);
    return metadata_get(runtime, x, y, z, out, error, acquisition);
}

mc_world_storage_status mc_world_runtime_metadata_set_quiet(mc_world_runtime *runtime, int32_t x,
                                                            int32_t y, int32_t z, int32_t metadata,
                                                            bool *out,
                                                            mc_world_storage_error *error) {
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
    mc_world_storage_status status = acquire_chunk(runtime, x, z, &chunk, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (!chunk->blank) {
        chunk->dirty = true;
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
