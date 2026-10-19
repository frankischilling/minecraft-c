#include "mc/world/runtime/lighting.h"

#include "mc/core/java/java.h"
#include "provider_internal.h"
#include "runtime_internal.h"

static mc_world_storage_status result(mc_world_light_error *error,
                                      const mc_world_storage_error *cause, unsigned int bytecode,
                                      unsigned int provider_bytecode,
                                      mc_chunk_acquire_status acquisition)
{
    if (error != NULL)
        *error = (mc_world_light_error){.status = cause->status,
                                        .operation = cause->operation,
                                        .bytecode = bytecode,
                                        .provider_bytecode = provider_bytecode,
                                        .acquisition = acquisition,
                                        .cause = *cause};
    return cause->status;
}

static mc_world_storage_error native_error(mc_world_storage_status status, const char *operation)
{
    return (mc_world_storage_error){status, operation, 0};
}

mc_world_storage_status mc_world_runtime_height(mc_world_runtime *runtime, int32_t x, int32_t z,
                                                int32_t *out, mc_world_light_error *error)
{
    if (runtime == NULL || out == NULL) {
        const mc_world_storage_error cause =
            native_error(MC_WORLD_STORAGE_INVALID_ARGUMENT, "World height owner/output");
        return result(error, &cause, 0, 0, MC_CHUNK_ACQUIRE_OK);
    }
    if (runtime->construction.mode != MC_WORLD_RUNTIME_GAME ||
        runtime->construction.phase == MC_WORLD_RUNTIME_RETIRED) {
        const mc_world_storage_error cause =
            native_error(MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World height phase");
        return result(error, &cause, 0, 0, MC_CHUNK_ACQUIRE_OK);
    }
    mc_world_storage_error cause = {0};
    mc_world_storage_status status = mc_world_runtime_call_enter(runtime, &cause);
    if (status != MC_WORLD_STORAGE_OK)
        return result(error, &cause, 0, 0, MC_CHUNK_ACQUIRE_OK);

    unsigned int bytecode = 0, provider_bytecode = 0;
    mc_chunk_acquire_status acquisition = MC_CHUNK_ACQUIRE_OK;
    int32_t height = 0;
    if (x < -32000000 || x >= 32000000 || z < -32000000 || z > 32000000)
        goto success;

    const int32_t chunk_x = mc_java_i32_shr(x, 4);
    const int32_t chunk_z = mc_java_i32_shr(z, 4);
    bytecode = 33;
    provider_bytecode = 6;
    /* The source presence receiver is loaded separately from the later getter.
     * This is the current actual provider, not a detached World cache view. */
    bool present = false;
    status = mc_world_runtime_provider_contains(runtime, chunk_x, chunk_z, &present, &cause);
    if (status != MC_WORLD_STORAGE_OK)
        goto failed;
    if (!present)
        goto success;

    bytecode = 48;
    provider_bytecode = 6;
    if (runtime->fields.v == NULL) {
        cause = native_error(MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World height getter provider");
        goto failed;
    }
    mc_chunk_state *chunk = NULL;
    acquisition = mc_world_runtime_get(runtime, chunk_x, chunk_z, &chunk);
    if (acquisition != MC_CHUNK_ACQUIRE_OK) {
        if (acquisition == MC_CHUNK_ACQUIRE_LIMIT || acquisition == MC_CHUNK_ACQUIRE_PARTIAL_LIMIT)
            cause = native_error(MC_WORLD_STORAGE_LIMIT, "World height getter acquisition limit");
        else if (runtime->last_error.status != MC_WORLD_STORAGE_OK)
            cause = runtime->last_error;
        else
            cause =
                native_error(MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World height getter acquisition");
        goto failed;
    }
    bytecode = 61;
    provider_bytecode = 0;
    if (chunk == NULL || (!chunk->blank && chunk->data == NULL)) {
        cause = native_error(MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World height Chunk payload");
        goto failed;
    }
    if (!chunk->blank) {
        const size_t local_x = (uint32_t)x & 15u;
        const size_t local_z = (uint32_t)z & 15u;
        height = (int32_t)chunk->data->heightmap[local_z * 16u + local_x];
    }

success:
    *out = height;
    cause = native_error(MC_WORLD_STORAGE_OK, NULL);
    status = result(error, &cause, 0, 0, acquisition);
    goto leave;
failed:
    runtime->last_error = cause;
    status = result(error, &cause, bytecode, provider_bytecode, acquisition);
leave:
    mc_world_runtime_call_leave(runtime);
    return status;
}
