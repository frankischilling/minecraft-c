#include "mutation_internal.h"

#include "../../../block/registry/runtime_internal.h"
#include "mc/core/java/java.h"

static int32_t or_bits(int32_t a, int32_t b) {
    return mc_java_i32_narrow((int64_t)((uint32_t)a | (uint32_t)b));
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

/* Capture is deliberately separate from byte/getter evaluation. Passing an
 * already evaluated ID to a combined helper would move that read before the
 * source getstatic and its actual initializer callbacks. */
static bool capture_q(mc_world_runtime *world, unsigned int pc, mc_registry_block_q_view *out,
                      mc_world_mutation_error *error) {
    error->height_bytecode = pc;
    mc_registry_classes *classes = mc_world_runtime_registry(world);
    mc_registry_error cause = registry_error(MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY,
                                             "Chunk incremental height Registry binding");
    const mc_registry_status status =
        classes == NULL ? cause.status : mc_registry_block_q_get(classes, out, &cause);
    if (status != MC_REGISTRY_OK)
        return mc_world_mutation_registry_fail(world, error, &cause,
                                               "Chunk incremental height opacity capture");
    return true;
}

static bool opacity_at(mc_world_runtime *world, const mc_registry_block_q_view *array, int32_t id,
                       unsigned int pc, int32_t *out, mc_world_mutation_error *error) {
    error->height_bytecode = pc;
    mc_registry_error cause = registry_error(MC_REGISTRY_OK, NULL);
    if (mc_registry_block_q_at(array, id, out, &cause) != MC_REGISTRY_OK)
        return mc_world_mutation_registry_fail(world, error, &cause,
                                               "Chunk incremental height opacity element");
    return true;
}

static bool height_byte(mc_world_chunk *chunk, mc_world_runtime *world, int32_t index,
                        unsigned int field_pc, unsigned int access_pc, int32_t *out,
                        mc_world_mutation_error *error) {
    mc_world_chunk_fields fields;
    error->height_bytecode = field_pc;
    (void)mc_world_chunk_fields_get(chunk, &fields);
    error->height_bytecode = access_pc;
    if (fields.h == NULL)
        return mc_world_mutation_fail(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Chunk incremental height array is null");
    if (index < 0 || (uint32_t)index >= MC_CHUNK_HEIGHTMAP_BYTES)
        return mc_world_mutation_fail(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Chunk incremental height array index");
    *out = (int32_t)fields.h[(size_t)index]; /* Source baload followed by &255. */
    return true;
}

static bool block_byte(mc_world_chunk *chunk, mc_world_runtime *world, int32_t index,
                       unsigned int field_pc, unsigned int access_pc, int32_t *out,
                       mc_world_mutation_error *error) {
    mc_world_chunk_fields fields;
    error->height_bytecode = field_pc;
    (void)mc_world_chunk_fields_get(chunk, &fields);
    error->height_bytecode = access_pc;
    if (fields.b == NULL)
        return mc_world_mutation_fail(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Chunk incremental block array is null");
    if (index < 0 || (uint32_t)index >= MC_CHUNK_BLOCKS)
        return mc_world_mutation_fail(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Chunk incremental block array index");
    *out = (int32_t)fields.b[(size_t)index]; /* Source baload followed by &255. */
    return true;
}

/* The actual a(III) getter has OR-composed Java indexing and no numerical XYZ
 * gate. Its failure belongs to the invoking g PC349/409 in this error frame. */
static bool block_get(mc_world_chunk *chunk, mc_world_runtime *world, int32_t x, int32_t y,
                      int32_t z, unsigned int pc, int32_t *out, mc_world_mutation_error *error) {
    const int32_t index = or_bits(or_bits(mc_java_i32_shl(x, 11), mc_java_i32_shl(z, 7)), y);
    return block_byte(chunk, world, index, pc, pc, out, error);
}

bool mc_world_mutation_height(mc_world_chunk *chunk, int32_t x, int32_t y, int32_t z,
                              mc_world_mutation_error *error) {
    /* Enclosing concrete setter already holds this World and exact Chunk.
     * This passive binding supplies the native Registry/error context only;
     * each reached source field below is still evaluated at its own point. */
    mc_world_chunk_fields fields;
    (void)mc_world_chunk_fields_get(chunk, &fields);
    mc_world_runtime *world = fields.d;
    const int32_t column = or_bits(mc_java_i32_shl(z, 4), x);
    int32_t old_height;
    if (!height_byte(chunk, world, column, 1, 9, &old_height, error))
        return false;
    int32_t height = old_height;
    if (y > old_height)
        height = y;
    const int32_t base = or_bits(mc_java_i32_shl(x, 11), mc_java_i32_shl(z, 7));

    while (height > 0) {
        mc_registry_block_q_view array;
        if (!capture_q(world, 45, &array, error))
            return false;
        int32_t id, opacity;
        const int32_t index = mc_java_i32_sub(mc_java_i32_add(base, height), 1);
        if (!block_byte(chunk, world, index, 49, 59, &id, error) ||
            !opacity_at(world, &array, id, 64, &opacity, error))
            return false;
        if (opacity != 0)
            break;
        height = mc_java_i32_sub(height, 1);
    }
    if (height == old_height) {
        error->height_bytecode = 0; /* Ordinary early return81. */
        return true;
    }

    error->height_bytecode = 83;
    (void)mc_world_chunk_fields_get(chunk, &fields);
    world = fields.d;
    error->height_bytecode = 92;
    if (!mc_world_mutation_observe_column(world, x, z, height, old_height, error))
        return false;

    error->height_bytecode = 96;
    (void)mc_world_chunk_fields_get(chunk, &fields);
    error->height_bytecode = 107;
    if (fields.h == NULL)
        return mc_world_mutation_fail(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Chunk incremental height store array is null");
    if (column < 0 || (uint32_t)column >= MC_CHUNK_HEIGHTMAP_BYTES)
        return mc_world_mutation_fail(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Chunk incremental height store index");
    fields.h[(size_t)column] = (uint8_t)mc_java_i8_narrow(height);

    error->height_bytecode = 111;
    (void)mc_world_chunk_fields_get(chunk, &fields);
    if (height < fields.i) {
        error->height_bytecode = 120;
        mc_world_chunk_state(chunk)->data->minimum_height = height;
    } else {
        int32_t minimum = 127;
        for (int32_t local_x = 0; local_x < 16; local_x = mc_java_i32_add(local_x, 1)) {
            for (int32_t local_z = 0; local_z < 16; local_z = mc_java_i32_add(local_z, 1)) {
                const int32_t index = or_bits(mc_java_i32_shl(local_z, 4), local_x);
                int32_t value;
                if (!height_byte(chunk, world, index, 151, 161, &value, error))
                    return false;
                if (value < minimum) {
                    if (!height_byte(chunk, world, index, 172, 182, &value, error))
                        return false;
                    minimum = value;
                }
            }
        }
        error->height_bytecode = 204;
        mc_world_chunk_state(chunk)->data->minimum_height = minimum;
    }

    error->height_bytecode = 208;
    (void)mc_world_chunk_fields_get(chunk, &fields);
    const int32_t world_x = mc_java_i32_add(mc_java_i32_mul(fields.j, 16), x);
    error->height_bytecode = 219;
    (void)mc_world_chunk_fields_get(chunk, &fields);
    const int32_t world_z = mc_java_i32_add(mc_java_i32_mul(fields.k, 16), z);

    if (height < old_height) {
        for (int32_t fill_y = height; fill_y < old_height; fill_y = mc_java_i32_add(fill_y, 1)) {
            error->height_bytecode = 248;
            (void)mc_world_chunk_fields_get(chunk, &fields);
            error->height_bytecode = 257;
            if (!mc_world_mutation_nibble_set(fields.f, x, fill_y, z, 15, error))
                return false;
        }
    } else {
        error->height_bytecode = 270;
        (void)mc_world_chunk_fields_get(chunk, &fields);
        world = fields.d;
        const mc_light_bounds bounds = {.min_x = world_x,
                                        .min_y = old_height,
                                        .min_z = world_z,
                                        .max_x = world_x,
                                        .max_y = height,
                                        .max_z = world_z};
        if (!mc_world_mutation_submit(world, MC_LIGHT_SKY, &bounds, &error->height_bytecode, 273,
                                      288, error))
            return false;
        for (int32_t fill_y = old_height; fill_y < height; fill_y = mc_java_i32_add(fill_y, 1)) {
            error->height_bytecode = 303;
            (void)mc_world_chunk_fields_get(chunk, &fields);
            error->height_bytecode = 311;
            if (!mc_world_mutation_nibble_set(fields.f, x, fill_y, z, 0, error))
                return false;
        }
    }

    int32_t light = 15;
    const int32_t saved_height = height;
    while (height > 0 && light > 0) {
        height = mc_java_i32_sub(height, 1); /* Source338 precedes q capture. */
        mc_registry_block_q_view array;
        if (!capture_q(world, 341, &array, error))
            return false;
        int32_t id, opacity;
        if (!block_get(chunk, world, x, height, z, 349, &id, error) ||
            !opacity_at(world, &array, id, 352, &opacity, error))
            return false;
        if (opacity == 0)
            opacity = 1;
        light = mc_java_i32_sub(light, opacity);
        if (light < 0)
            light = 0;
        error->height_bytecode = 379;
        (void)mc_world_chunk_fields_get(chunk, &fields);
        error->height_bytecode = 388;
        if (!mc_world_mutation_nibble_set(fields.f, x, height, z, light, error))
            return false;
    }
    while (height > 0) {
        mc_registry_block_q_view array;
        if (!capture_q(world, 399, &array, error))
            return false;
        int32_t id, opacity;
        if (!block_get(chunk, world, x, mc_java_i32_sub(height, 1), z, 409, &id, error) ||
            !opacity_at(world, &array, id, 412, &opacity, error))
            return false;
        if (opacity != 0)
            break;
        height = mc_java_i32_sub(height, 1);
    }
    if (height != saved_height) {
        error->height_bytecode = 430;
        (void)mc_world_chunk_fields_get(chunk, &fields);
        world = fields.d;
        const mc_light_bounds bounds = {.min_x = mc_java_i32_sub(world_x, 1),
                                        .min_y = height,
                                        .min_z = mc_java_i32_sub(world_z, 1),
                                        .max_x = mc_java_i32_add(world_x, 1),
                                        .max_y = saved_height,
                                        .max_z = mc_java_i32_add(world_z, 1)};
        if (!mc_world_mutation_submit(world, MC_LIGHT_SKY, &bounds, &error->height_bytecode, 433,
                                      456, error))
            return false;
    }
    error->height_bytecode = 461;
    mc_world_chunk_state(chunk)->dirty = true;
    error->height_bytecode = 0; /* Ordinary return464; caller's frame is retained. */
    return true;
}
