#include "mc/world/runtime/blocks.h"

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

static mc_world_storage_status acquire_chunk(mc_world_runtime *runtime, int32_t x, int32_t z,
                                             mc_chunk_state **out, mc_world_storage_error *error) {
    /* World.c reads v before invoking provider.b, including a cached hit. */
    if (runtime->fields.v == NULL)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World block provider receiver");
    mc_world_runtime_acquisition_error detail;
    mc_chunk_acquire_status status = mc_world_runtime_get_detailed(
        runtime, mc_java_i32_shr(x, 4), mc_java_i32_shr(z, 4), out, &detail);
    if (status == MC_CHUNK_ACQUIRE_OK)
        return MC_WORLD_STORAGE_OK;
    if (detail.cause.status != MC_WORLD_STORAGE_OK) {
        if (error != NULL)
            *error = detail.cause;
        return detail.cause.status;
    }
    if (status == MC_CHUNK_ACQUIRE_LIMIT || status == MC_CHUNK_ACQUIRE_PARTIAL_LIMIT)
        return result(error, MC_WORLD_STORAGE_LIMIT, "World block acquisition limit");
    return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World block acquisition");
}

mc_world_storage_status mc_world_runtime_block_get(mc_world_runtime *runtime, int32_t x, int32_t y,
                                                   int32_t z, int32_t *out,
                                                   mc_world_storage_error *error) {
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
    mc_world_storage_status status = acquire_chunk(runtime, x, z, &chunk, error);
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

mc_world_storage_status mc_world_runtime_metadata_get(mc_world_runtime *runtime, int32_t x,
                                                      int32_t y, int32_t z, int32_t *out,
                                                      mc_world_storage_error *error) {
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
    mc_world_storage_status status = acquire_chunk(runtime, x, z, &chunk, error);
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
