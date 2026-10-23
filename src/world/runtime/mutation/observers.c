#include "mutation_internal.h"

#include "../../../block/registry/runtime_internal.h"
#include "../construction_internal.h"
#include "mc/core/java/java.h"
#include "mc/world/runtime/blocks.h"

static bool list_metadata(const mc_world_reference_list *list) {
    return list != NULL && list->count <= list->capacity &&
           list->capacity <= SIZE_MAX / sizeof(*list->items) &&
           (list->capacity == 0 || list->items != NULL);
}

/* The current finite World has no concrete observer registration owner. Empty
 * u still reaches size9. On nonempty input, preserve get23/null/invoke order;
 * an untyped nonnull element cannot establish the source pm/jp cast at28.
 * No successful stand-in observer or Java collection internals are supplied. */
static bool observe(mc_world_runtime *world, unsigned int invocation_pc,
                    mc_world_mutation_error *error) {
    error->observer_bytecode = 9;
    const mc_world_reference_list *list = world->fields.u;
    if (!list_metadata(list))
        return mc_world_mutation_fail(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "World observer size receiver metadata");
    if (list->count == 0) {
        error->observer_bytecode = 0;
        return true;
    }

    /* This is a separate actual u reload at18, not a saved loop receiver. The
     * first index is0; no later iteration is reached without a real observer. */
    error->observer_bytecode = 23;
    list = world->fields.u;
    if (!list_metadata(list) || list->count == 0)
        return mc_world_mutation_fail(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "World observer get receiver or index");
    const void *observer = list->items[0];
    if (observer != NULL) {
        error->observer_bytecode = 28;
        return mc_world_mutation_fail(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "World concrete observer cast unavailable");
    }

    /* checkcast accepts NULL; its actual interface invocation then fails. */
    error->observer_bytecode = invocation_pc;
    return mc_world_mutation_fail(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                  "World observer invocation receiver is null");
}

bool mc_world_mutation_observe_cell(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                    mc_world_mutation_error *error) {
    /* Coordinates would be passed unchanged at34. No observer body is reached
     * successfully in the supported empty/cast-unavailable domain. */
    (void)x;
    (void)y;
    (void)z;
    return observe(world, 34, error);
}

bool mc_world_mutation_observe_range(mc_world_runtime *world, int32_t min_x, int32_t min_y,
                                     int32_t min_z, int32_t max_x, int32_t max_y, int32_t max_z,
                                     mc_world_mutation_error *error) {
    /* The full range interface invocation is40, not the separate same-cell
     * range observer method's37. No coordinate clipping is performed. */
    (void)min_x;
    (void)min_y;
    (void)min_z;
    (void)max_x;
    (void)max_y;
    (void)max_z;
    return observe(world, 40, error);
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
