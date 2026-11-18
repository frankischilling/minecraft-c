#include "chunk_execution_internal.h"
#include "mutation_internal.h"

#include "../../../block/registry/runtime_internal.h"
#include "../../../item/render/internal.h"
#include "../chunks_internal.h"
#include "../power/power_internal.h"
#include "mc/core/java/java.h"

#include <threads.h>

mc_registry_error mc_world_mutation_empty_registry(void) {
    return (mc_registry_error){.initializer_pc = -1,
                               .block_id = -1,
                               .constructor_pc = -1,
                               .active_initializer_pc = -1,
                               .item_argument = -1};
}

static once_flag empty_error_once = ONCE_FLAG_INIT;
static mc_world_mutation_error empty_error;

static void prepare_empty_error(void) {
    mc_world_mutation_error *error = &empty_error;
    const mc_registry_error registry = mc_world_mutation_empty_registry();
    mc_live_tile_entities_error_init(&error->tile_entity);
    mc_world_entity_load_error_init(&error->load_entity);
    error->batch.index = SIZE_MAX;
    error->registry = registry;
    mc_world_nearby_error_init(&error->nearby);
    error->schedule.registry = registry;
    error->plant.registry = registry;
    error->plant.query.registry = registry;
    error->plant.material.registry = registry;
    error->plant.support.registry = registry;
    mc_world_power_error_init(&error->power);
    mc_world_collision_error_init(&error->rail_access);
    error->rail_support.registry = registry;
}

void mc_world_mutation_error_init(mc_world_mutation_error *error) {
    if (error == NULL)
        return;
    /* Initialize the nested sentinels once and copy the complete diagnostic. */
    call_once(&empty_error_once, prepare_empty_error);
    *error = empty_error;
}

mc_world_mutation_error mc_world_mutation_empty_error(void) {
    mc_world_mutation_error error;
    mc_world_mutation_error_init(&error);
    return error;
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
    const mc_registry_error reached = *cause;
    error->registry_failure = (mc_world_registry_failure){0};
    error->registry = reached;
    error->status =
        mc_world_runtime_registry_result(world, reached.status, &reached, operation, &error->cause);
    error->operation = error->cause.operation;
    return false;
}

bool mc_world_registry_failure_matches(const mc_world_registry_failure *view,
                                       const mc_registry_error *cause) {
    if (view == NULL || cause == NULL ||
        (view->kind != MC_WORLD_REGISTRY_FAILURE_NATIVE &&
         view->kind != MC_WORLD_REGISTRY_FAILURE_SOURCE_EXCEPTION &&
         view->kind != MC_WORLD_REGISTRY_FAILURE_SOURCE_ERROR) ||
        ((view->exception_class == NULL) != (view->kind == MC_WORLD_REGISTRY_FAILURE_NATIVE)))
        return false;
    const mc_registry_error *a = &view->cause;
    const mc_registry_error *b = cause;
    return a->status == b->status && a->message == b->message &&
           a->initializer_pc == b->initializer_pc && a->block_id == b->block_id &&
           a->constructor_class == b->constructor_class && a->constructor_pc == b->constructor_pc &&
           a->profile_status == b->profile_status &&
           a->active_initializer_class == b->active_initializer_class &&
           a->active_initializer_pc == b->active_initializer_pc &&
           a->item_argument == b->item_argument &&
           a->format_error.status == b->format_error.status &&
           a->format_error.offset == b->format_error.offset &&
           a->format_error.length == b->format_error.length &&
           a->format_error.value == b->format_error.value &&
           a->format_error.flags == b->format_error.flags &&
           a->format_error.conversion == b->format_error.conversion &&
           a->format_error.date_time == b->format_error.date_time;
}

bool mc_world_mutation_registry_fail_in(mc_world_runtime *world,
                                        mc_registry_classes *producer_registry,
                                        mc_world_mutation_error *error,
                                        const mc_registry_error *cause, const char *operation) {
    (void)mc_world_mutation_registry_fail(world, error, cause, operation);
    mc_item_render_throwable kind = MC_ITEM_RENDER_NO_THROWABLE;
    const char *name = NULL;
    if (producer_registry != NULL)
        mc_item_render_registry_classification(producer_registry, &error->registry, &kind, &name);
    error->registry_failure = (mc_world_registry_failure){
        .kind = kind == MC_ITEM_RENDER_SOURCE_EXCEPTION ? MC_WORLD_REGISTRY_FAILURE_SOURCE_EXCEPTION
                : kind == MC_ITEM_RENDER_SOURCE_ERROR   ? MC_WORLD_REGISTRY_FAILURE_SOURCE_ERROR
                                                        : MC_WORLD_REGISTRY_FAILURE_NATIVE,
        .exception_class = name,
        .cause = error->registry};
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
    return mc_world_mutation_chunk_q(world, mc_world_runtime_registry(world), id, bytecode,
                                     capture_pc, element_pc, out, error);
}

bool mc_world_mutation_chunk_q(mc_world_runtime *world, mc_registry_classes *classes, int32_t id,
                               unsigned int *bytecode, unsigned int capture_pc,
                               unsigned int element_pc, int32_t *out,
                               mc_world_mutation_error *error) {
    mc_registry_block_q_view array;
    mc_registry_error cause = mc_world_mutation_empty_registry();
    *bytecode = capture_pc;
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

bool mc_world_mutation_chunk_submit(mc_world_mutation_chunk_execution *execution,
                                    mc_world_runtime *world, mc_light_kind kind,
                                    const mc_light_bounds *bounds,
                                    mc_world_mutation_chunk_body body, unsigned int *bytecode,
                                    unsigned int type_pc, unsigned int call_pc,
                                    mc_world_light_error *error, mc_world_storage_error *cause) {
    const mc_world_light_type *type;
    mc_world_runtime_classes *classes =
        execution == NULL ? world->classes : execution->code_classes;
    *bytecode = type_pc;
    if (mc_world_runtime_light_type_get(classes, kind, &type, cause) != MC_WORLD_STORAGE_OK) {
        if (world != NULL)
            world->last_error = *cause;
        return execution == NULL ? false : mc_world_mutation_chunk_close(execution, false);
    }
    *bytecode = call_pc;
    mc_light_submit_result submitted;
    if (execution == NULL) {
        if (mc_world_runtime_light_submit(world, type, bounds, true, &submitted, error) ==
            MC_WORLD_STORAGE_OK)
            return true;
        *cause = error->cause;
        return false;
    }

    if (world == NULL) {
        mc_world_mutation_chunk_null(execution, body, call_pc);
        *cause = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "Chunk light invocation World receiver is null", 0};
        return mc_world_mutation_chunk_close(execution, false);
    }
    mc_world_light_execution_init(&execution->light, execution->execution, execution->code,
                                  execution->scope.site, execution->owned);
    mc_world_storage_status raw = MC_WORLD_STORAGE_OK;
    const bool completed = mc_world_runtime_light_submit_in_execution(
        &execution->light, world, type, bounds, true, &submitted, error, &raw);
    if (raw != MC_WORLD_STORAGE_OK)
        *cause = error->cause;
    return mc_world_mutation_chunk_close(execution, completed);
}

bool mc_world_mutation_submit(mc_world_runtime *world, mc_light_kind kind,
                              const mc_light_bounds *bounds, unsigned int *bytecode,
                              unsigned int type_pc, unsigned int call_pc,
                              mc_world_mutation_error *error) {
    return mc_world_mutation_submit_scoped(NULL, world, kind, bounds, MC_WORLD_MUTATION_CHUNK_ID,
                                           bytecode, type_pc, call_pc, error);
}

bool mc_world_mutation_submit_scoped(mc_world_mutation_chunk_execution *execution,
                                     mc_world_runtime *world, mc_light_kind kind,
                                     const mc_light_bounds *bounds,
                                     mc_world_mutation_chunk_body body, unsigned int *bytecode,
                                     unsigned int type_pc, unsigned int call_pc,
                                     mc_world_mutation_error *error) {
    mc_world_storage_error cause = {0};
    if (!mc_world_mutation_chunk_submit(execution, world, kind, bounds, body, bytecode, type_pc,
                                        call_pc, &error->light, &cause)) {
        error->status = cause.status;
        error->operation = cause.operation;
        error->cause = cause;
        if (execution == NULL)
            world->last_error = cause;
        return false;
    }
    return true;
}

static bool column_result(mc_world_mutation_error *error, const mc_world_chunk_error *frame,
                          bool completed) {
    if (completed) {
        error->column_bytecode = 0;
        error->interval_bytecode = 0;
        return true;
    }
    error->column_bytecode = frame->column_bytecode;
    error->interval_bytecode = frame->interval_bytecode;
    error->status = frame->cause.status;
    error->operation = frame->cause.operation;
    error->cause = frame->cause;
    error->registry = frame->registry;
    error->registry_failure = (mc_world_registry_failure){0};
    error->light = frame->light;
    return false;
}

bool mc_world_mutation_column(mc_world_chunk *chunk, int32_t x, int32_t z,
                              mc_world_mutation_error *error) {
    mc_world_chunk_error frame = {.registry = mc_world_mutation_empty_registry()};
    const bool completed = mc_world_runtime_chunk_column(chunk, x, z, &frame);
    if (column_result(error, &frame, completed))
        return true;
    mc_world_chunk_fields fields;
    (void)mc_world_chunk_fields_get(chunk, &fields);
    fields.d->last_error = frame.cause;
    return false;
}

bool mc_world_mutation_column_in_execution(mc_world_mutation_chunk_execution *execution,
                                           mc_world_chunk *chunk, int32_t x, int32_t z,
                                           mc_world_mutation_error *error) {
    execution->column = (mc_world_chunk_error){.registry = mc_world_mutation_empty_registry()};
    const bool completed =
        mc_world_runtime_chunk_column_in_execution(execution, chunk, x, z, &execution->column);
    return column_result(error, &execution->column, completed);
}
