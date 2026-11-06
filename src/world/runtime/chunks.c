#include "mc/world/runtime/chunks.h"
#include "chunks_internal.h"

#include "../../block/registry/runtime_internal.h"
#include "../storage/generated_internal.h"
#include "mc/core/java/java.h"
#include "mc/world/dimension/world_provider.h"
#include "runtime_internal.h"

static mc_world_storage_error native_error(mc_world_storage_status status, const char *operation) {
    return (mc_world_storage_error){status, operation, 0};
}

static mc_registry_error registry_error(mc_registry_status status, const char *message) {
    return (mc_registry_error){.status = status,
                               .message = message,
                               .initializer_pc = -1,
                               .block_id = -1,
                               .constructor_pc = -1,
                               .active_initializer_pc = -1,
                               .item_argument = -1};
}

static mc_world_chunk_error empty_error(void) {
    return (mc_world_chunk_error){.registry = registry_error(MC_REGISTRY_OK, NULL)};
}

static mc_world_storage_status result(mc_world_chunk_error *out, mc_world_chunk_error *frame,
                                      mc_world_storage_error cause, unsigned int bytecode) {
    frame->status = cause.status;
    frame->operation = cause.operation;
    frame->cause = cause;
    frame->bytecode = bytecode;
    if (out != NULL)
        *out = *frame;
    return cause.status;
}

mc_world_storage_status mc_world_runtime_chunk_construct(mc_world_runtime *world, int32_t x,
                                                         int32_t z, uint8_t **blocks, size_t length,
                                                         mc_world_chunk **out,
                                                         mc_world_chunk_error *error) {
    mc_world_chunk_error frame = empty_error();
    if (world == NULL || blocks == NULL || *blocks == NULL || length != (size_t)MC_CHUNK_BLOCKS ||
        out == NULL)
        return result(
            error, &frame,
            native_error(MC_WORLD_STORAGE_INVALID_ARGUMENT, "Chunk constructor arguments"), 0);
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        return result(
            error, &frame,
            native_error(MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Chunk constructor World phase"), 0);

    /* Catalog selection is an explicit native admission requirement, not an
     * extra source World method, Block active use or provider preflight. */
    mc_world_provider_state dimension;
    if (world->fields.w == NULL ||
        mc_world_provider_state_get(world->fields.t, &dimension) != MC_WORLD_PROVIDER_OK ||
        (dimension.kind != MC_WORLD_PROVIDER_OVERWORLD &&
         dimension.kind != MC_WORLD_PROVIDER_NETHER))
        return result(error, &frame,
                      native_error(MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Chunk constructor catalog"),
                      0);

    mc_world_storage_error cause = {0};
    mc_world_storage_status status = mc_world_runtime_call_enter(world, &cause);
    if (status != MC_WORLD_STORAGE_OK)
        return result(error, &frame, cause, 0);
    mc_world_chunk *chunk = NULL;
    status = mc_world_store_create_generated(world->fields.w, world, (int32_t)dimension.kind, x, z,
                                             blocks, length, &chunk, &cause);
    /* Successful linkage already protects d against native World free. */
    if (status == MC_WORLD_STORAGE_OK)
        *out = chunk;
    mc_world_runtime_call_leave(world);
    return result(error, &frame, cause, 0);
}

static bool opacity(mc_world_chunk *chunk, mc_world_runtime *world, size_t block_index,
                    unsigned int capture_pc, unsigned int byte_pc, unsigned int element_pc,
                    int32_t *out, mc_world_chunk_error *frame) {
    mc_registry_classes *classes = mc_world_runtime_registry(world);
    mc_registry_error cause =
        registry_error(MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY, "Chunk opacity registry binding");
    mc_registry_block_q_view array;
    mc_registry_status status =
        classes == NULL ? cause.status : mc_registry_block_q_get(classes, &array, &cause);
    frame->bytecode = capture_pc;
    if (status == MC_REGISTRY_OK) {
        /* getstatic has completed, including any initializer callbacks. Only
         * now evaluate the current source b and its unsigned block byte. */
        mc_world_chunk_fields fields;
        (void)mc_world_chunk_fields_get(chunk, &fields);
        if (fields.b == NULL) {
            frame->bytecode = byte_pc;
            frame->cause =
                native_error(MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Chunk block array is null");
            world->last_error = frame->cause;
            return false;
        }
        const int32_t id = (int32_t)fields.b[block_index];
        frame->bytecode = element_pc;
        status = mc_registry_block_q_at(&array, id, out, &cause);
    }
    if (status != MC_REGISTRY_OK) {
        frame->registry = cause;
        (void)mc_world_runtime_registry_result(world, status, &cause, "Chunk opacity read",
                                               &frame->cause);
        return false;
    }
    return true;
}

static bool interval(mc_world_chunk *chunk, int32_t x, int32_t z, int32_t local_height,
                     unsigned int column_pc, mc_world_chunk_error *frame) {
    mc_world_chunk_fields fields;
    (void)mc_world_chunk_fields_get(chunk, &fields);
    mc_world_runtime *world = fields.d;
    int32_t height;
    frame->column_bytecode = column_pc;
    frame->interval_bytecode = 6;
    if (mc_world_runtime_height(world, x, z, &height, &frame->light) != MC_WORLD_STORAGE_OK) {
        frame->cause = frame->light.cause;
        return false;
    }
    if (height == local_height)
        return true;

    /* Each branch reloads d and reaches the same class's actual Sky identity
     * after the height comparison, before the six-coordinate World call. */
    (void)mc_world_chunk_fields_get(chunk, &fields);
    world = fields.d;
    const mc_world_light_type *sky = NULL;
    frame->interval_bytecode = height > local_height ? 21u : 52u;
    if (mc_world_runtime_light_type_get(world->classes, MC_LIGHT_SKY, &sky, &frame->cause) !=
        MC_WORLD_STORAGE_OK) {
        world->last_error = frame->cause;
        return false;
    }
    const mc_light_bounds bounds = {.min_x = x,
                                    .min_y = height > local_height ? local_height : height,
                                    .min_z = z,
                                    .max_x = x,
                                    .max_y = height > local_height ? height : local_height,
                                    .max_z = z};
    mc_light_submit_result submitted;
    frame->interval_bytecode = height > local_height ? 31u : 62u;
    if (mc_world_runtime_light_submit(world, sky, &bounds, true, &submitted, &frame->light) !=
        MC_WORLD_STORAGE_OK) {
        frame->cause = frame->light.cause;
        return false;
    }
    /* A normal void return reaches o36/67 even when enqueue skipped. */
    mc_world_chunk_state(chunk)->dirty = true;
    return true;
}

bool mc_world_runtime_chunk_column(mc_world_chunk *chunk, int32_t x, int32_t z,
                                   mc_world_chunk_error *frame) {
    mc_world_chunk_fields fields;
    (void)mc_world_chunk_fields_get(chunk, &fields);
    const int32_t index = mc_java_i32_shl(z, 4) | x;
    frame->column_bytecode = 3;
    if (fields.h == NULL || index < 0 || (size_t)index >= MC_CHUNK_HEIGHTMAP_BYTES) {
        frame->cause =
            native_error(MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Chunk column height element");
        return false;
    }
    const int32_t local = (int32_t)fields.h[(size_t)index];
    const int32_t world_x = mc_java_i32_add(mc_java_i32_mul(fields.j, 16), x);
    const int32_t world_z = mc_java_i32_add(mc_java_i32_mul(fields.k, 16), z);
    return interval(chunk, mc_java_i32_sub(world_x, 1), world_z, local, 37, frame) &&
           interval(chunk, mc_java_i32_add(world_x, 1), world_z, local, 48, frame) &&
           interval(chunk, world_x, mc_java_i32_sub(world_z, 1), local, 59, frame) &&
           interval(chunk, world_x, mc_java_i32_add(world_z, 1), local, 70, frame);
}

mc_world_storage_status mc_world_runtime_chunk_finish(mc_world_chunk *chunk,
                                                      mc_world_chunk_error *error) {
    mc_world_chunk_error frame = empty_error();
    if (chunk == NULL)
        return result(error, &frame,
                      native_error(MC_WORLD_STORAGE_INVALID_ARGUMENT, "Chunk finalizer owner"), 0);
    mc_world_chunk_fields fields;
    (void)mc_world_chunk_fields_get(chunk, &fields);
    mc_world_runtime *world = fields.d;
    mc_chunk_state *state = mc_world_chunk_state(chunk);
    if (world == NULL || world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED || world->fields.w == NULL ||
        mc_world_store_generated_chunk(world->fields.w, state) != chunk)
        return result(error, &frame,
                      native_error(MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Chunk finalizer binding"),
                      0);

    mc_world_storage_error cause = {0};
    mc_world_storage_status status = mc_world_runtime_call_enter(world, &cause);
    if (status != MC_WORLD_STORAGE_OK)
        return result(error, &frame, cause, 0);
    mc_world_store *store = world->fields.w;
    status = mc_world_store_lease_chunk(store, state, &cause);
    if (status != MC_WORLD_STORAGE_OK) {
        mc_world_runtime_call_leave(world);
        return result(error, &frame, cause, 0);
    }

    int32_t minimum = 127;
    for (int32_t x = 0; x < 16; ++x) {
        for (int32_t z = 0; z < 16; ++z) {
            const size_t base = ((size_t)x << 11u) | ((size_t)z << 7u);
            int32_t height = 127;
            while (height > 0) {
                int32_t value;
                if (!opacity(chunk, world, base + (size_t)(height - 1), 39, 53, 58, &value, &frame))
                    goto failed;
                if (value != 0)
                    break;
                --height;
            }
            (void)mc_world_chunk_fields_get(chunk, &fields);
            fields.h[((size_t)z << 4u) | (size_t)x] = (uint8_t)height;
            if (height < minimum)
                minimum = height;

            /* h80 has already changed before current d91/t94/e97 is read. */
            mc_world_provider_fields dimension;
            frame.bytecode = 97;
            if (mc_world_provider_fields_get(fields.d->fields.t, &dimension) !=
                MC_WORLD_PROVIDER_OK) {
                frame.cause = native_error(MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                           "Chunk finalizer current Dimension");
                goto failed;
            }
            if (!dimension.e) {
                int32_t light = 15, y = 127;
                do {
                    int32_t value;
                    if (!opacity(chunk, world, base + (size_t)y, 113, 125, 130, &value, &frame))
                        goto failed;
                    light = mc_java_i32_sub(light, value);
                    if (light > 0) {
                        (void)mc_world_chunk_fields_get(chunk, &fields);
                        mc_world_chunk_nibble_fields sky;
                        frame.bytecode = 149;
                        if (mc_world_chunk_nibble_fields_get(fields.f, &sky) !=
                                MC_WORLD_STORAGE_OK ||
                            !mc_chunk_nibbles_set(sky.a, sky.length, x, y, z, light)) {
                            frame.cause = native_error(MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                                       "Chunk finalizer sky Nibble");
                            goto failed;
                        }
                    }
                    --y;
                } while (y > 0 && light > 0);
            }
        }
    }
    state->data->minimum_height = minimum; /* i179, after all local columns. */

    for (int32_t x = 0; x < 16; ++x) {
        for (int32_t z = 0; z < 16; ++z) {
            frame.bytecode = 201;
            if (!mc_world_runtime_chunk_column(chunk, x, z, &frame))
                goto failed;
        }
    }
    state->dirty = true; /* o218 only after all neighbor operations. */
    frame = empty_error();
    cause = native_error(MC_WORLD_STORAGE_OK, NULL);
    goto leave;

failed:
    cause = frame.cause;
    world->last_error = cause;
leave:
    /* These exact leases cannot be retired by synchronous callbacks. Keep
     * the first source failure and its entire frame through native cleanup. */
    (void)mc_world_store_release_chunk(store, state, NULL);
    mc_world_runtime_call_leave(world);
    return result(error, &frame, cause, frame.bytecode);
}
