#include "mutation_internal.h"

#include "../../../block/registry/runtime_internal.h"
#include "../chunks_internal.h"
#include "mc/core/java/java.h"

mc_world_mutation_error mc_world_mutation_empty_error(void) {
    const mc_registry_error registry = {.initializer_pc = -1,
                                        .block_id = -1,
                                        .constructor_pc = -1,
                                        .active_initializer_pc = -1,
                                        .item_argument = -1};
    return (mc_world_mutation_error){.tile = mc_live_tiles_empty_error(),
                                     .load_entity = mc_world_entity_load_empty_error(),
                                     .batch = {.index = SIZE_MAX},
                                     .registry = registry,
                                     .nearby = mc_world_nearby_empty_error(),
                                     .schedule = {.registry = registry}};
}

bool mc_world_mutation_fail(mc_world_runtime *world, mc_world_mutation_error *error,
                            mc_world_storage_status status, const char *operation) {
    error->status = status;
    error->operation = operation;
    error->cause = (mc_world_storage_error){status, operation, 0};
    if (world != NULL)
        world->last_error = error->cause;
    return false;
}

bool mc_world_mutation_registry_fail(mc_world_runtime *world, mc_world_mutation_error *error,
                                     const mc_registry_error *cause, const char *operation) {
    error->registry = *cause;
    error->status =
        mc_world_runtime_registry_result(world, cause->status, cause, operation, &error->cause);
    error->operation = error->cause.operation;
    return false;
}

static bool nibble_byte(const mc_world_chunk_nibble *nibble, int32_t x, int32_t y, int32_t z,
                        mc_world_chunk_nibble_fields *fields, size_t *index,
                        mc_world_mutation_error *error) {
    const int32_t cell = mc_java_i32_shl(x, 11) | mc_java_i32_shl(z, 7) | y;
    const int32_t packed = mc_java_i32_shr(cell, 1);
    if (mc_world_chunk_nibble_fields_get(nibble, fields) != MC_WORLD_STORAGE_OK ||
        fields->a == NULL)
        return mc_world_mutation_fail(NULL, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Chunk mutation Nibble backing is null");
    if (packed < 0 || (size_t)packed >= fields->length)
        return mc_world_mutation_fail(NULL, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Chunk mutation Nibble backing index");
    *index = (size_t)packed;
    return true;
}

bool mc_world_mutation_nibble_get(const mc_world_chunk_nibble *nibble, int32_t x, int32_t y,
                                  int32_t z, int32_t *out, mc_world_mutation_error *error) {
    mc_world_chunk_nibble_fields fields;
    size_t index;
    if (!nibble_byte(nibble, x, y, z, &fields, &index, error))
        return false;
    const unsigned int shift = ((uint32_t)y & 1u) * 4u;
    *out = (int32_t)(((uint32_t)fields.a[index] >> shift) & 15u);
    return true;
}

bool mc_world_mutation_nibble_set(const mc_world_chunk_nibble *nibble, int32_t x, int32_t y,
                                  int32_t z, int32_t value, mc_world_mutation_error *error) {
    mc_world_chunk_nibble_fields fields;
    size_t index;
    if (!nibble_byte(nibble, x, y, z, &fields, &index, error))
        return false;
    const unsigned int shift = ((uint32_t)y & 1u) * 4u;
    const uint32_t mask = 15u << shift;
    fields.a[index] =
        (uint8_t)(((uint32_t)fields.a[index] & ~mask) | (((uint32_t)value & 15u) << shift));
    return true;
}

bool mc_world_mutation_q(mc_world_runtime *world, int32_t id, unsigned int *bytecode,
                         unsigned int capture_pc, unsigned int element_pc, int32_t *out,
                         mc_world_mutation_error *error) {
    mc_registry_block_q_view array;
    mc_registry_error cause = mc_world_mutation_empty_error().registry;
    *bytecode = capture_pc;
    mc_registry_classes *classes = mc_world_runtime_registry(world);
    if (classes == NULL) {
        cause.status = MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY;
        cause.message = "Chunk mutation Registry binding";
        return mc_world_mutation_registry_fail(world, error, &cause, cause.message);
    }
    if (mc_registry_block_q_get(classes, &array, &cause) != MC_REGISTRY_OK)
        return mc_world_mutation_registry_fail(world, error, &cause,
                                               "Chunk mutation Block.q active use");
    *bytecode = element_pc;
    if (mc_registry_block_q_at(&array, id, out, &cause) != MC_REGISTRY_OK)
        return mc_world_mutation_registry_fail(world, error, &cause,
                                               "Chunk mutation Block.q element");
    return true;
}

bool mc_world_mutation_submit(mc_world_runtime *world, mc_light_kind kind,
                              const mc_light_bounds *bounds, unsigned int *bytecode,
                              unsigned int type_pc, unsigned int call_pc,
                              mc_world_mutation_error *error) {
    const mc_world_light_type *type;
    mc_world_storage_error cause = {0};
    *bytecode = type_pc;
    if (mc_world_runtime_light_type_get(world->classes, kind, &type, &cause) !=
        MC_WORLD_STORAGE_OK) {
        mc_world_mutation_fail(world, error, cause.status, cause.operation);
        error->cause = cause;
        world->last_error = cause;
        return false;
    }
    *bytecode = call_pc;
    mc_light_submit_result result;
    if (mc_world_runtime_light_submit(world, type, bounds, true, &result, &error->light) !=
        MC_WORLD_STORAGE_OK) {
        error->status = error->light.status;
        error->operation = error->light.operation;
        error->cause = error->light.cause;
        world->last_error = error->cause;
        return false;
    }
    return true;
}

bool mc_world_mutation_column(mc_world_chunk *chunk, int32_t x, int32_t z,
                              mc_world_mutation_error *error) {
    mc_world_chunk_error frame = {.registry = mc_world_mutation_empty_error().registry};
    if (mc_world_runtime_chunk_column(chunk, x, z, &frame)) {
        error->column_bytecode = 0;
        error->interval_bytecode = 0;
        return true;
    }
    error->column_bytecode = frame.column_bytecode;
    error->interval_bytecode = frame.interval_bytecode;
    error->status = frame.cause.status;
    error->operation = frame.cause.operation;
    error->cause = frame.cause;
    error->registry = frame.registry;
    error->light = frame.light;
    mc_world_chunk_fields fields;
    (void)mc_world_chunk_fields_get(chunk, &fields);
    fields.d->last_error = frame.cause;
    return false;
}
