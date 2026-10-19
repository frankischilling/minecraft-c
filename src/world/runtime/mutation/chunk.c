#include "mutation_internal.h"

#include "../../../block/registry/runtime_internal.h"
#include "mc/core/java/java.h"
#include "mc/world/dimension/dimension.h"

static bool initial(mc_world_chunk *chunk, int32_t x, int32_t y, int32_t z, int32_t *height,
                    int32_t *old, mc_world_mutation_error *error)
{
    mc_world_chunk_fields fields;
    (void)mc_world_chunk_fields_get(chunk, &fields);
    const int32_t column = mc_java_i32_shl(z, 4) | x;
    error->chunk_bytecode = 14;
    if (fields.h == NULL || column < 0 || (uint32_t)column >= MC_CHUNK_HEIGHTMAP_BYTES)
        return mc_world_mutation_fail(fields.d, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Chunk setter old height element");
    *height = (int32_t)fields.h[(size_t)column];

    (void)mc_world_chunk_fields_get(chunk, &fields);
    const int32_t cell = mc_java_i32_shl(x, 11) | mc_java_i32_shl(z, 7) | y;
    error->chunk_bytecode = 36;
    if (fields.b == NULL || cell < 0 || (uint32_t)cell >= MC_CHUNK_BLOCKS)
        return mc_world_mutation_fail(fields.d, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Chunk setter old block element");
    *old = (int32_t)fields.b[(size_t)cell];
    return true;
}

static bool put(mc_world_chunk *chunk, int32_t x, int32_t y, int32_t z, int32_t id, unsigned int pc,
                mc_world_mutation_error *error)
{
    mc_world_chunk_fields fields;
    (void)mc_world_chunk_fields_get(chunk, &fields);
    const int32_t cell = mc_java_i32_shl(x, 11) | mc_java_i32_shl(z, 7) | y;
    error->chunk_bytecode = pc;
    if (fields.b == NULL || cell < 0 || (uint32_t)cell >= MC_CHUNK_BLOCKS)
        return mc_world_mutation_fail(fields.d, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Chunk setter block store");
    fields.b[(size_t)cell] = (uint8_t)((uint32_t)id & 255u);
    return true;
}

static bool metadata(mc_world_chunk *chunk, int32_t x, int32_t y, int32_t z, int32_t value,
                     unsigned int pc, mc_world_mutation_error *error)
{
    mc_world_chunk_fields fields;
    (void)mc_world_chunk_fields_get(chunk, &fields);
    error->chunk_bytecode = pc;
    if (!mc_world_mutation_nibble_set(fields.e, x, y, z, value, error)) {
        fields.d->last_error = error->cause;
        return false;
    }
    return true;
}

static bool hook(mc_world_chunk *chunk, int32_t id, int32_t x, int32_t y, int32_t z, bool added,
                 unsigned int capture_pc, unsigned int element_pc, unsigned int invoke_pc,
                 mc_world_mutation_error *error)
{
    mc_world_chunk_fields fields;
    (void)mc_world_chunk_fields_get(chunk, &fields);
    mc_world_runtime *world = fields.d;
    mc_registry_classes *classes = mc_world_runtime_registry(world);
    mc_registry_error cause = mc_world_mutation_empty_error().registry;
    mc_registry_block_m_view array;
    error->chunk_bytecode = capture_pc;
    if (classes == NULL) {
        cause.status = MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY;
        cause.message = "Chunk setter Registry binding";
        return mc_world_mutation_registry_fail(world, error, &cause, cause.message);
    }
    if (mc_registry_block_m_get(classes, &array, &cause) != MC_REGISTRY_OK)
        return mc_world_mutation_registry_fail(world, error, &cause,
                                               "Chunk setter Block.m active use");
    const mc_block *block;
    error->chunk_bytecode = element_pc;
    if (mc_registry_block_m_at(&array, id, &block, &cause) != MC_REGISTRY_OK)
        return mc_world_mutation_registry_fail(world, error, &cause,
                                               "Chunk setter Block.m element");
    (void)mc_world_chunk_fields_get(chunk, &fields);
    error->chunk_bytecode = invoke_pc;
    return added ? mc_world_mutation_block_added(fields.d, block, x, y, z, error)
                 : mc_world_mutation_block_removed(fields.d, block, x, y, z, error);
}

static bool height(mc_world_chunk *chunk, int32_t x, int32_t y, int32_t z, int32_t id,
                   int32_t old_height, unsigned int capture_pc, unsigned int element_pc,
                   unsigned int raise_pc, unsigned int lower_pc, mc_world_mutation_error *error)
{
    mc_world_chunk_fields fields;
    (void)mc_world_chunk_fields_get(chunk, &fields);
    int32_t opacity;
    if (!mc_world_mutation_q(fields.d, (int32_t)((uint32_t)id & 255u), &error->chunk_bytecode,
                             capture_pc, element_pc, &opacity, error))
        return false;
    if (opacity != 0 && y >= old_height) {
        error->chunk_bytecode = raise_pc;
        return mc_world_mutation_height(chunk, x, mc_java_i32_add(y, 1), z, error);
    }
    if (opacity == 0 && y == mc_java_i32_sub(old_height, 1)) {
        error->chunk_bytecode = lower_pc;
        return mc_world_mutation_height(chunk, x, y, z, error);
    }
    return true;
}

static bool submit(mc_world_chunk *chunk, mc_light_kind kind, int32_t x, int32_t y, int32_t z,
                   unsigned int type_pc, unsigned int call_pc, mc_world_mutation_error *error)
{
    mc_world_chunk_fields fields;
    (void)mc_world_chunk_fields_get(chunk, &fields);
    const mc_light_bounds bounds = {x, y, z, x, y, z};
    return mc_world_mutation_submit(fields.d, kind, &bounds, &error->chunk_bytecode, type_pc,
                                    call_pc, error);
}

bool mc_world_mutation_chunk_block(mc_world_chunk *chunk, int32_t x, int32_t y, int32_t z,
                                   int32_t id, bool *out, mc_world_mutation_error *error)
{
    int32_t old_height, old;
    if (!initial(chunk, x, y, z, &old_height, &old, error))
        return false;
    if (old == id) {
        error->chunk_bytecode = 0;
        *out = false;
        return true;
    }
    mc_world_chunk_fields fields;
    (void)mc_world_chunk_fields_get(chunk, &fields);
    const int32_t world_x = mc_java_i32_add(mc_java_i32_mul(fields.j, 16), x);
    const int32_t world_z = mc_java_i32_add(mc_java_i32_mul(fields.k, 16), z);
    if (!put(chunk, x, y, z, id, 96, error))
        return false;
    if (old != 0 && !hook(chunk, old, world_x, y, world_z, false, 102, 107, 117, error))
        return false;
    if (!metadata(chunk, x, y, z, 0, 128, error) ||
        !height(chunk, x, y, z, id, old_height, 131, 140, 156, 174, error) ||
        !submit(chunk, MC_LIGHT_SKY, world_x, y, world_z, 181, 194, error) ||
        !submit(chunk, MC_LIGHT_BLOCK, world_x, y, world_z, 201, 214, error))
        return false;
    error->chunk_bytecode = 220;
    if (!mc_world_mutation_column(chunk, x, z, error))
        return false;
    if (id != 0) {
        (void)mc_world_chunk_fields_get(chunk, &fields);
        error->chunk_bytecode = 232;
        if (!fields.d->fields.B &&
            !hook(chunk, id, world_x, y, world_z, true, 238, 243, 253, error))
            return false;
    }
    mc_world_chunk_state(chunk)->dirty = true; /* o258 */
    error->chunk_bytecode = 0;
    *out = true;
    return true;
}

bool mc_world_mutation_chunk_block_metadata(mc_world_chunk *chunk, int32_t x, int32_t y, int32_t z,
                                            int32_t id, int32_t value, bool *out,
                                            mc_world_mutation_error *error)
{
    int32_t old_height, old;
    if (!initial(chunk, x, y, z, &old_height, &old, error))
        return false;
    mc_world_chunk_fields fields;
    if (old == id) {
        int32_t previous;
        (void)mc_world_chunk_fields_get(chunk, &fields);
        error->chunk_bytecode = 57;
        if (!mc_world_mutation_nibble_get(fields.e, x, y, z, &previous, error)) {
            fields.d->last_error = error->cause;
            return false;
        }
        if (previous == value) {
            error->chunk_bytecode = 0;
            *out = false;
            return true;
        }
    }
    (void)mc_world_chunk_fields_get(chunk, &fields);
    const int32_t world_x = mc_java_i32_add(mc_java_i32_mul(fields.j, 16), x);
    const int32_t world_z = mc_java_i32_add(mc_java_i32_mul(fields.k, 16), z);
    if (!put(chunk, x, y, z, id, 111, error))
        return false;
    if (old != 0) {
        (void)mc_world_chunk_fields_get(chunk, &fields);
        error->chunk_bytecode = 121;
        if (!fields.d->fields.B &&
            !hook(chunk, old, world_x, y, world_z, false, 127, 132, 142, error))
            return false;
    }
    if (!metadata(chunk, x, y, z, value, 154, error))
        return false;
    (void)mc_world_chunk_fields_get(chunk, &fields);
    mc_world_dimension_fields dimension;
    error->chunk_bytecode = 164;
    if (mc_world_dimension_fields_get(fields.d->fields.t, &dimension) != MC_WORLD_DIMENSION_OK)
        return mc_world_mutation_fail(fields.d, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Chunk setter current Dimension");
    if (!dimension.e && (!height(chunk, x, y, z, id, old_height, 170, 179, 195, 213, error) ||
                         !submit(chunk, MC_LIGHT_SKY, world_x, y, world_z, 220, 233, error)))
        return false;
    if (!submit(chunk, MC_LIGHT_BLOCK, world_x, y, world_z, 240, 253, error))
        return false;
    error->chunk_bytecode = 259;
    if (!mc_world_mutation_column(chunk, x, z, error) ||
        !metadata(chunk, x, y, z, value, 271, error))
        return false;
    if (id != 0 && !hook(chunk, id, world_x, y, world_z, true, 279, 284, 294, error))
        return false;
    mc_world_chunk_state(chunk)->dirty = true; /* o299 */
    error->chunk_bytecode = 0;
    *out = true;
    return true;
}
