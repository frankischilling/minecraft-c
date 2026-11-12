#include "mutation_internal.h"
#include "notifications_execution_internal.h"
#include "world_execution_internal.h"

#include "../../../block/registry/runtime_internal.h"
#include "../construction_internal.h"
#include "../execution_children_internal.h"
#include "../world_access_internal.h"
#include "mc/core/java/java.h"
#include "mc/world/runtime/blocks.h"

static bool world_access_failure(mc_world_runtime *world, mc_world_mutation_error *error,
                                 mc_world_storage_error cause) {
    error->status = cause.status;
    error->operation = cause.operation;
    error->cause = cause;
    world->last_error = cause;
    return false;
}

static bool notification_quiet(const mc_world_runtime_execution_result *owned) {
    if (owned->selection_ready || owned->root_pending || owned->math_failed)
        return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (owned->pending[i].used)
            return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (owned->closes[i].used)
            return false;
    return true;
}

static bool notification_note(mc_world_notification_execution *frame,
                              mc_world_storage_status status, const char *operation) {
    mc_world_runtime_execution_result_note(frame->owned, status, operation, frame->receiver_site);
    return false;
}

static bool notification_current(mc_world_notification_execution *frame, mc_world_runtime *world) {
    mc_entity_tick_execution_world_token current;
    if (!mc_entity_tick_execution_current_world(frame->execution, &current))
        return notification_note(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                 "World notification checked current scope");
    if (current.owner != frame->world.owner || current.slot != frame->world.slot ||
        current.generation != frame->world.generation)
        return notification_note(frame, MC_WORLD_STORAGE_BUSY,
                                 "World notification exact evaluated scope changed");
    mc_world_access_failure_lease *lease = NULL;
    const mc_world_storage_status status =
        mc_world_execution_child_lease(frame->execution, world, &lease);
    if (status != MC_WORLD_STORAGE_OK)
        return notification_note(frame, status, "World notification evaluated scope admission");
    if (lease != frame->lease)
        return notification_note(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                 "World notification exact producer lease changed");
    return true;
}

static void notification_callback(mc_world_notification_execution *frame,
                                  mc_world_storage_status status, mc_world_mutation_error *error) {
    if (frame == NULL)
        return;
    frame->callback_status = status;
    if (frame->callbacks_returned != SIZE_MAX)
        ++frame->callbacks_returned;
    if (status != MC_WORLD_STORAGE_OK) {
        const mc_world_storage_status captured = mc_entity_tick_execution_capture_receipt(
            frame->execution, &error->world_access_failure);
        mc_world_runtime_execution_result_note(frame->owned, captured,
                                               "World notification actual receipt capture",
                                               frame->receiver_site);
        if (captured != MC_WORLD_STORAGE_OK && frame->execution->capture_pending)
            error->world_access_failure = (mc_world_access_failure){0};
    }
}

static bool observe_cell(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                         mc_world_mutation_error *error,
                         mc_world_notification_execution *execution) {
    for (size_t index = 0;; ++index) {
        if (execution != NULL && !notification_current(execution, world))
            return false;
        mc_world_access *world_access = NULL;
        bool present = false;
        mc_world_storage_error cause = {0};
        if (!mc_world_access_next(world, index, 9, 23, 28, &world_access, &present,
                                  &error->world_access_bytecode, &cause))
            return world_access_failure(world, error, cause);
        if (!present) {
            error->world_access_bytecode = 0;
            return true;
        }
        error->world_access_bytecode = 34;
        const mc_world_storage_status status =
            execution == NULL ? mc_world_access_call_cell(world_access, world, x, y, z, &cause,
                                                          &error->world_access_failure)
                              : mc_world_access_call_cell_with_lease(&world_access, world, x, y, z,
                                                                     execution->lease, &cause,
                                                                     &error->world_access_failure);
        notification_callback(execution, status, error);
        mc_world_access_release_held(world_access);
        if (status != MC_WORLD_STORAGE_OK)
            return world_access_failure(world, error,
                                        mc_world_access_callback_error(status, cause));
        /* Source iinc39 follows a normally returned callback, even when it
         * removed this occurrence. The next size/get reload the current u. */
    }
}

static bool observe_range(mc_world_runtime *world, int32_t min_x, int32_t min_y, int32_t min_z,
                          int32_t max_x, int32_t max_y, int32_t max_z,
                          mc_world_mutation_error *error,
                          mc_world_notification_execution *execution) {
    /* fd/dj.b(IIIIII)V: pass the supplied bounds without sorting or clipping.
     * Equal endpoints do not select the separate fd.k/dj.i(III)V body. */
    for (size_t index = 0;; ++index) {
        if (execution != NULL && !notification_current(execution, world))
            return false;
        mc_world_access *world_access = NULL;
        bool present = false;
        mc_world_storage_error cause = {0};
        if (!mc_world_access_next(world, index, 9, 23, 28, &world_access, &present,
                                  &error->world_access_bytecode, &cause))
            return world_access_failure(world, error, cause);
        if (!present) {
            error->world_access_bytecode = 0;
            return true;
        }
        error->world_access_bytecode = 40;
        const mc_world_storage_status status =
            execution == NULL
                ? mc_world_access_call_range(world_access, world, min_x, min_y, min_z, max_x, max_y,
                                             max_z, &cause, &error->world_access_failure)
                : mc_world_access_call_range_with_lease(&world_access, world, min_x, min_y, min_z,
                                                        max_x, max_y, max_z, execution->lease,
                                                        &cause, &error->world_access_failure);
        notification_callback(execution, status, error);
        mc_world_access_release_held(world_access);
        if (status != MC_WORLD_STORAGE_OK)
            return world_access_failure(world, error,
                                        mc_world_access_callback_error(status, cause));
    }
}

bool mc_world_mutation_observe_cell(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                    mc_world_mutation_error *error) {
    return observe_cell(world, x, y, z, error, NULL);
}

bool mc_world_mutation_observe_range(mc_world_runtime *world, int32_t min_x, int32_t min_y,
                                     int32_t min_z, int32_t max_x, int32_t max_y, int32_t max_z,
                                     mc_world_mutation_error *error) {
    return observe_range(world, min_x, min_y, min_z, max_x, max_y, max_z, error, NULL);
}

void mc_world_notification_execution_init_borrowed(mc_world_notification_execution *frame,
                                                   mc_entity_tick_execution *execution,
                                                   mc_entity_tick_execution_site receiver_site,
                                                   mc_world_runtime_execution_result *owned) {
    if (frame != NULL)
        *frame = (mc_world_notification_execution){
            .execution = execution, .owned = owned, .receiver_site = receiver_site};
}

void mc_world_notification_execution_init(mc_world_notification_execution *frame,
                                          mc_entity_tick_execution *execution,
                                          mc_entity_tick_execution_site receiver_site,
                                          mc_world_runtime_execution_result *owned) {
    mc_world_notification_execution_init_borrowed(frame, execution, receiver_site, owned);
    mc_world_runtime_execution_result_init(owned);
}

static bool notification_enter(mc_world_notification_execution *frame, mc_world_runtime *world,
                               mc_world_mutation_error *error, mc_world_storage_status *status) {
    if (frame == NULL)
        return false;
    if (frame->execution == NULL || frame->execution->identity != frame->execution ||
        frame->owned == NULL || world == NULL || error == NULL || status == NULL || frame->entered)
        return notification_note(frame, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                 "World notification execution/fresh outputs required");
    if (frame->owned->ownership_status != MC_WORLD_STORAGE_OK || !notification_quiet(frame->owned))
        return notification_note(frame, MC_WORLD_STORAGE_BUSY,
                                 "World notification fresh result required");
    if (!mc_entity_tick_execution_current_world(frame->execution, &frame->world))
        return notification_note(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                 "World notification evaluated scope required");
    const mc_world_storage_status admitted =
        mc_world_execution_child_lease(frame->execution, world, &frame->lease);
    if (admitted != MC_WORLD_STORAGE_OK)
        return notification_note(frame, admitted, "World notification evaluated scope admission");
    frame->entered = true;
    return true;
}

static bool notification_finish(mc_world_notification_execution *frame, bool completed,
                                mc_world_mutation_error *error, mc_world_storage_status *status) {
    *status = completed ? MC_WORLD_STORAGE_OK : error->status;
    frame->completed = completed && frame->owned->ownership_status == MC_WORLD_STORAGE_OK;
    return frame->completed;
}

bool mc_world_mutation_observe_cell_in_execution(mc_world_notification_execution *frame,
                                                 mc_world_runtime *world, int32_t x, int32_t y,
                                                 int32_t z, mc_world_mutation_error *error,
                                                 mc_world_storage_status *status) {
    if (!notification_enter(frame, world, error, status))
        return false;
    const bool completed = observe_cell(world, x, y, z, error, frame);
    return notification_finish(frame, completed, error, status);
}

bool mc_world_mutation_observe_range_in_execution(mc_world_notification_execution *frame,
                                                  mc_world_runtime *world, int32_t min_x,
                                                  int32_t min_y, int32_t min_z, int32_t max_x,
                                                  int32_t max_y, int32_t max_z,
                                                  mc_world_mutation_error *error,
                                                  mc_world_storage_status *status) {
    if (!notification_enter(frame, world, error, status))
        return false;
    frame->range = true;
    const bool completed =
        observe_range(world, min_x, min_y, min_z, max_x, max_y, max_z, error, frame);
    return notification_finish(frame, completed, error, status);
}

static bool observe_column(mc_world_runtime *world, int32_t x, int32_t z, int32_t a, int32_t b,
                           mc_world_mutation_error *error, mc_world_notification_execution *frame,
                           mc_world_storage_status *raw) {
    /* fd.h/dj.g(IIII)V: source arguments are x,z,yA,yB. In particular callers
     * in incremental Chunk.g supply LOCAL x/z; do not correct them here. */
    if (a > b) {
        const int32_t temporary = b;
        b = a;
        a = temporary;
    }
    return frame == NULL ? mc_world_mutation_observe_range(world, x, a, z, x, b, z, error)
                         : mc_world_mutation_observe_range_in_execution(frame, world, x, a, z, x, b,
                                                                        z, error, raw);
}

bool mc_world_mutation_observe_column(mc_world_runtime *world, int32_t x, int32_t z, int32_t a,
                                      int32_t b, mc_world_mutation_error *error) {
    return observe_column(world, x, z, a, b, error, NULL, NULL);
}

bool mc_world_mutation_observe_column_in_execution(mc_world_notification_execution *frame,
                                                   mc_world_runtime *world, int32_t x, int32_t z,
                                                   int32_t a, int32_t b,
                                                   mc_world_mutation_error *error,
                                                   mc_world_storage_status *raw) {
    return frame != NULL && observe_column(world, x, z, a, b, error, frame, raw);
}

static bool neighbor_success(mc_world_mutation_error *error) {
    error->neighbor_bytecode = 0;
    error->neighbor_call_bytecode = 0;
    return true;
}

static bool neighbor(mc_world_runtime *world, int32_t x, int32_t y, int32_t z, int32_t source_id,
                     unsigned int call_pc, mc_world_mutation_error *error,
                     mc_world_mutation_execution_call *owned) {
    error->neighbor_call_bytecode = call_pc;
    error->neighbor_bytecode = 1;
    if (owned != NULL &&
        !mc_world_mutation_execution_current(
            owned, world, (mc_entity_tick_execution_site){"fd", "l", "(IIII)V", 1}))
        return false;
    if (world->fields.o)
        return neighbor_success(error);
    error->neighbor_bytecode = 8;
    if (world->fields.B)
        return neighbor_success(error);

    /* Source getstatic15 precedes virtual World.a22. Keep this exact nullable
     * m across acquisition/reentry; do not mask its later full ID index. The
     * enclosing entered World keeps its bound Registry owner alive. */
    error->neighbor_bytecode = 15;
    mc_registry_block_m_view array;
    mc_registry_error registry = mc_world_mutation_empty_registry();
    mc_registry_classes *classes = owned == NULL ? mc_world_runtime_registry(world)
                                                 : mc_world_mutation_execution_registry(owned);
    if (classes == NULL) {
        registry.status = MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY;
        registry.message = "World neighbor Registry binding";
        return mc_world_mutation_registry_fail(world, error, &registry, registry.message);
    }
    if (mc_registry_block_m_get(classes, &array, &registry) != MC_REGISTRY_OK)
        return mc_world_mutation_registry_fail(world, error, &registry,
                                               "World neighbor Block.m active use");

    error->neighbor_bytecode = 22;
    int32_t id;
    if (owned == NULL) {
        mc_world_storage_error cause = {0};
        const mc_world_storage_status status =
            mc_world_runtime_block_get(world, x, y, z, &id, &cause);
        if (status != MC_WORLD_STORAGE_OK) {
            mc_world_mutation_fail(world, error, status, cause.operation);
            /* Preserve the actual returned acquisition cause and system error. */
            error->cause = cause;
            world->last_error = cause;
            return false;
        }
    } else {
        const mc_entity_tick_execution_site site = {"fd", "l", "(IIII)V", 22};
        if (!mc_world_mutation_execution_get(owned, world, x, y, z, false, &id, error, site))
            return false;
    }

    error->neighbor_bytecode = 25;
    const mc_block *block;
    if (mc_registry_block_m_at(&array, id, &block, &registry) != MC_REGISTRY_OK)
        return mc_world_mutation_registry_fail(world, error, &registry,
                                               "World neighbor captured Block.m element");
    error->neighbor_bytecode = 30;
    if (block == NULL)
        return neighbor_success(error);

    error->neighbor_bytecode = 41;
    const bool completed =
        owned == NULL ? mc_world_mutation_block_neighbor(world, block, x, y, z, source_id, error)
                      : mc_world_mutation_block_neighbor_in_execution(owned, world, block, x, y, z,
                                                                      source_id, error);
    if (!completed)
        return false;
    return neighbor_success(error);
}

static bool neighbors(mc_world_runtime *world, int32_t x, int32_t y, int32_t z, int32_t id,
                      mc_world_mutation_error *error, mc_world_mutation_execution_call *owned) {
    /* The source invokes each private operation synchronously. Each later
     * call rereads flags/registry/World and observes completed earlier effects. */
    return neighbor(world, mc_java_i32_sub(x, 1), y, z, id, 8, error, owned) &&
           neighbor(world, mc_java_i32_add(x, 1), y, z, id, 19, error, owned) &&
           neighbor(world, x, mc_java_i32_sub(y, 1), z, id, 30, error, owned) &&
           neighbor(world, x, mc_java_i32_add(y, 1), z, id, 41, error, owned) &&
           neighbor(world, x, y, mc_java_i32_sub(z, 1), id, 52, error, owned) &&
           neighbor(world, x, y, mc_java_i32_add(z, 1), id, 63, error, owned);
}

bool mc_world_mutation_neighbors(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                 int32_t id, mc_world_mutation_error *error) {
    return neighbors(world, x, y, z, id, error, NULL);
}

bool mc_world_mutation_neighbors_in_execution(mc_world_mutation_execution_call *owned,
                                              mc_world_runtime *world, int32_t x, int32_t y,
                                              int32_t z, int32_t id,
                                              mc_world_mutation_error *error) {
    return owned != NULL && neighbors(world, x, y, z, id, error, owned);
}
