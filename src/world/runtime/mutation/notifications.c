#include "mutation_internal.h"

#include "../../../block/registry/runtime_internal.h"
#include "../construction_internal.h"
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

bool mc_world_mutation_observe_cell(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                    mc_world_mutation_error *error) {
    for (size_t index = 0;; ++index) {
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
        mc_world_storage_status status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        if (world_access != NULL && world_access->callbacks.cell != NULL)
            status = world_access->callbacks.cell(world_access->context, world, x, y, z, &cause);
        else
            cause.operation = "World cell WorldAccess invocation";
        mc_world_access_release_held(world_access);
        if (status != MC_WORLD_STORAGE_OK)
            return world_access_failure(world, error,
                                        mc_world_access_callback_error(status, cause));
        /* Source iinc39 follows a normally returned callback, even when it
         * removed this occurrence. The next size/get reload the current u. */
    }
}

bool mc_world_mutation_observe_range(mc_world_runtime *world, int32_t min_x, int32_t min_y,
                                     int32_t min_z, int32_t max_x, int32_t max_y, int32_t max_z,
                                     mc_world_mutation_error *error) {
    /* fd/dj.b(IIIIII)V: pass the supplied bounds without sorting or clipping.
     * Equal endpoints do not select the separate fd.k/dj.i(III)V body. */
    for (size_t index = 0;; ++index) {
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
        mc_world_storage_status status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        if (world_access != NULL && world_access->callbacks.range != NULL)
            status = world_access->callbacks.range(world_access->context, world, min_x, min_y,
                                                   min_z, max_x, max_y, max_z, &cause);
        else
            cause.operation = "World range WorldAccess invocation";
        mc_world_access_release_held(world_access);
        if (status != MC_WORLD_STORAGE_OK)
            return world_access_failure(world, error,
                                        mc_world_access_callback_error(status, cause));
    }
}

bool mc_world_mutation_observe_column(mc_world_runtime *world, int32_t x, int32_t z, int32_t a,
                                      int32_t b, mc_world_mutation_error *error) {
    /* fd.h/dj.g(IIII)V: source arguments are x,z,yA,yB. In particular callers
     * in incremental Chunk.g supply LOCAL x/z; do not correct them here. */
    if (a > b) {
        const int32_t temporary = b;
        b = a;
        a = temporary;
    }
    return mc_world_mutation_observe_range(world, x, a, z, x, b, z, error);
}

static bool neighbor_success(mc_world_mutation_error *error) {
    error->neighbor_bytecode = 0;
    error->neighbor_call_bytecode = 0;
    return true;
}

static bool neighbor(mc_world_runtime *world, int32_t x, int32_t y, int32_t z, int32_t source_id,
                     unsigned int call_pc, mc_world_mutation_error *error) {
    error->neighbor_call_bytecode = call_pc;
    error->neighbor_bytecode = 1;
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
    mc_registry_classes *classes = mc_world_runtime_registry(world);
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
    mc_world_storage_error cause = {0};
    const mc_world_storage_status status = mc_world_runtime_block_get(world, x, y, z, &id, &cause);
    if (status != MC_WORLD_STORAGE_OK) {
        mc_world_mutation_fail(world, error, status, cause.operation);
        /* fail creates a fresh native diagnostic. Restore the actual returned
         * acquisition cause, including its native system_error. */
        error->cause = cause;
        world->last_error = cause;
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
    if (!mc_world_mutation_block_neighbor(world, block, x, y, z, source_id, error))
        return false;
    return neighbor_success(error);
}

bool mc_world_mutation_neighbors(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                 int32_t id, mc_world_mutation_error *error) {
    /* The source invokes each private operation synchronously. Each later
     * call rereads flags/registry/World and observes completed earlier effects. */
    return neighbor(world, mc_java_i32_sub(x, 1), y, z, id, 8, error) &&
           neighbor(world, mc_java_i32_add(x, 1), y, z, id, 19, error) &&
           neighbor(world, x, mc_java_i32_sub(y, 1), z, id, 30, error) &&
           neighbor(world, x, mc_java_i32_add(y, 1), z, id, 41, error) &&
           neighbor(world, x, y, mc_java_i32_sub(z, 1), id, 52, error) &&
           neighbor(world, x, y, mc_java_i32_add(z, 1), id, 63, error);
}
