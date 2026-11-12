#include "power_internal.h"

#include "../../../block/registry/runtime_internal.h"
#include "../block_queries_internal.h"
#include "../collision/blocks_internal.h"
#include "../construction_internal.h"
#include "mc/core/java/java.h"

typedef enum power_method {
    POWER_WEAK_NEIGHBORS,
    POWER_WEAK_AT,
    POWER_STRONG_NEIGHBORS,
    POWER_STRONG_AT
} power_method;

mc_registry_error mc_world_power_empty_registry(void) {
    return (mc_registry_error){.initializer_pc = -1,
                               .block_id = -1,
                               .constructor_pc = -1,
                               .active_initializer_pc = -1,
                               .item_argument = -1};
}

void mc_world_power_support_error_init(mc_world_solid_support_error *error) {
    static const mc_world_solid_support_error zero = {0};
    *error = zero;
    error->registry = mc_world_power_empty_registry();
}

void mc_world_power_error_init(mc_world_power_error *error) {
    static const mc_world_power_error zero = {0};
    if (error == NULL)
        return;
    *error = zero;
    mc_world_power_support_error_init(&error->support);
    mc_world_nearby_error_init(&error->access);
    error->registry = mc_world_power_empty_registry();
}

mc_world_power_error mc_world_power_empty_error(void) {
    mc_world_power_error error;
    mc_world_power_error_init(&error);
    return error;
}

static mc_world_storage_status admission(mc_world_power_error *error,
                                         mc_world_storage_status status, const char *operation) {
    error->status = status;
    error->operation = operation;
    error->cause = (mc_world_storage_error){status, operation, 0};
    return status;
}

static mc_world_storage_status storage_failure(mc_world_runtime *world, mc_world_power_error *error,
                                               mc_world_storage_error cause) {
    error->status = cause.status;
    error->operation = cause.operation;
    error->cause = cause;
    world->last_error = cause;
    return cause.status;
}

static mc_world_storage_status
registry_failure(mc_world_runtime *world, mc_world_power_error *error, mc_registry_status status,
                 const mc_registry_error *cause, const char *operation) {
    error->registry = *cause;
    error->status =
        mc_world_runtime_registry_result(world, status, cause, operation, &error->cause);
    error->operation = error->cause.operation;
    return error->status;
}

static mc_world_power_site site(int32_t x, int32_t y, int32_t z, int32_t side) {
    return (mc_world_power_site){.entered = true, .x = x, .y = y, .z = z, .side = side};
}

static void clear_leaf(mc_world_power_error *error) {
    static const mc_world_block_power_site empty = {0};
    error->capability = empty;
    error->strong = empty;
    error->weak = empty;
    error->redstone_connectivity = empty;
    mc_world_nearby_error_init(&error->access);
    error->registry = mc_world_power_empty_registry();
}

/* Private kernels share their actual enclosing frame. Reset only the callee
 * subtree at a new attempt, so a later sibling cannot inherit an earlier
 * successful support/strong/Block path. No recursive error stack is implied. */
static void enter_strong_at(mc_world_power_error *error, int32_t x, int32_t y, int32_t z,
                            int32_t side) {
    error->strong_at = site(x, y, z, side);
    clear_leaf(error);
}

static void enter_strong_neighbors(mc_world_power_error *error, int32_t x, int32_t y, int32_t z) {
    error->strong_neighbors = site(x, y, z, -1);
    error->strong_at = (mc_world_power_site){0};
    clear_leaf(error);
}

static void enter_weak_at(mc_world_power_error *error, int32_t x, int32_t y, int32_t z,
                          int32_t side) {
    error->weak_at = site(x, y, z, side);
    error->strong_neighbors = (mc_world_power_site){0};
    error->strong_at = (mc_world_power_site){0};
    mc_world_power_support_error_init(&error->support);
    clear_leaf(error);
}

/* Block bodies receive a fresh child. Retain every current World caller while
 * carrying the complete actual concrete dispatch/metadata descendant back. */
static mc_world_storage_status block_failure(mc_world_runtime *world, mc_world_power_error *error,
                                             const mc_world_power_error *lower) {
    const char *const world_class = error->world_class;
    const mc_world_power_site weak_neighbors = error->weak_neighbors;
    const mc_world_power_site weak_at = error->weak_at;
    const mc_world_power_site strong_neighbors = error->strong_neighbors;
    const mc_world_power_site strong_at = error->strong_at;
    *error = *lower;
    error->world_class = world_class;
    error->weak_neighbors = weak_neighbors;
    error->weak_at = weak_at;
    error->strong_neighbors = strong_neighbors;
    error->strong_at = strong_at;
    world->last_error = error->cause;
    return error->status;
}

static mc_world_storage_status block_id(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                        int32_t *out, mc_world_power_error *error) {
    mc_world_collision_error lower;
    mc_world_collision_error_init(&lower);
    const mc_world_storage_status status =
        mc_world_collision_block_value_held(world, x, y, z, false, out, &lower);
    if (status == MC_WORLD_STORAGE_OK)
        return status;
    error->access = lower.nearby;
    error->registry = lower.nearby.query.registry;
    return storage_failure(world, error, lower.nearby.query.cause);
}

/* Capture m only after the source ID getter, independently of support's
 * opposite m-before-ID order. Full signed ID indexing is left to m_at. */
static mc_world_storage_status selected_block(mc_world_runtime *world, int32_t id,
                                              unsigned int capture_pc, unsigned int element_pc,
                                              mc_world_power_site *caller, const mc_block **out,
                                              mc_world_power_error *error) {
    caller->bytecode = capture_pc;
    mc_registry_classes *const classes = mc_world_runtime_registry(world);
    mc_registry_error cause = mc_world_power_empty_registry();
    if (classes == NULL) {
        cause.status = MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY;
        cause.message = "World power Registry binding";
        return registry_failure(world, error, cause.status, &cause, cause.message);
    }
    mc_registry_block_m_view array;
    mc_registry_status status = mc_registry_block_m_get(classes, &array, &cause);
    if (status != MC_REGISTRY_OK)
        return registry_failure(world, error, status, &cause, "World power Block.m active use");
    caller->bytecode = element_pc;
    status = mc_registry_block_m_at(&array, id, out, &cause);
    if (status != MC_REGISTRY_OK)
        return registry_failure(world, error, status, &cause,
                                "World power captured Block.m element");
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status strong_at(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                         int32_t side, bool *out, mc_world_power_error *error) {
    enter_strong_at(error, x, y, z, side);
    error->strong_at.bytecode = 4;
    int32_t id;
    mc_world_storage_status status = block_id(world, x, y, z, &id, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (id == 0) {
        *out = false; /* Actual ireturn15, before any Block active use. */
        return MC_WORLD_STORAGE_OK;
    }
    const mc_block *block;
    status = selected_block(world, id, 16, 21, &error->strong_at, &block, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    error->strong_at.bytecode = 28;
    mc_world_power_error lower;
    mc_world_power_error_init(&lower);
    bool value;
    status = mc_world_block_strong_power_held(world, block, x, y, z, side, &value, &lower);
    if (status != MC_WORLD_STORAGE_OK)
        return block_failure(world, error, &lower);
    *out = value;
    return MC_WORLD_STORAGE_OK;
}

/* Evaluate only the reached neighbor's Java arithmetic. The six iterations
 * are the six literal source calls, not a work/recursion budget. */
static void neighbor_coordinates(int32_t x, int32_t y, int32_t z, int32_t side, int32_t *nx,
                                 int32_t *ny, int32_t *nz) {
    *nx = x;
    *ny = y;
    *nz = z;
    switch (side) {
        case 0:
            *ny = mc_java_i32_sub(y, 1);
            break;
        case 1:
            *ny = mc_java_i32_add(y, 1);
            break;
        case 2:
            *nz = mc_java_i32_sub(z, 1);
            break;
        case 3:
            *nz = mc_java_i32_add(z, 1);
            break;
        case 4:
            *nx = mc_java_i32_sub(x, 1);
            break;
        case 5:
            *nx = mc_java_i32_add(x, 1);
            break;
        default:
            break;
    }
}

static mc_world_storage_status strong_neighbors(mc_world_runtime *world, int32_t x, int32_t y,
                                                int32_t z, bool *out, mc_world_power_error *error) {
    enter_strong_neighbors(error, x, y, z);
    for (int32_t side = 0; side < 6; ++side) {
        int32_t nx, ny, nz;
        neighbor_coordinates(x, y, z, side, &nx, &ny, &nz);
        error->strong_neighbors.bytecode = 7u + 15u * (unsigned int)side;
        bool powered;
        const mc_world_storage_status status = strong_at(world, nx, ny, nz, side, &powered, error);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
        if (powered) {
            *out = true;
            return MC_WORLD_STORAGE_OK;
        }
    }
    *out = false;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status weak_at(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                       int32_t side, bool *out, mc_world_power_error *error) {
    enter_weak_at(error, x, y, z, side);
    error->weak_at.bytecode = 4;
    mc_world_solid_support_detail support;
    mc_world_power_support_error_init(&support.error);
    mc_world_nearby_error_init(&support.access);
    bool solid;
    mc_world_storage_status status =
        mc_world_solid_support_detailed_held(world, x, y, z, &solid, &support);
    if (status != MC_WORLD_STORAGE_OK) {
        error->support = support.error;
        error->access = support.access;
        error->registry = support.error.registry;
        return storage_failure(world, error, support.error.cause);
    }
    if (solid) {
        error->weak_at.bytecode = 14;
        return strong_neighbors(world, x, y, z, out, error);
    }
    error->weak_at.bytecode = 22;
    int32_t id;
    status = block_id(world, x, y, z, &id, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (id == 0) {
        *out = false; /* Actual ireturn33. */
        return MC_WORLD_STORAGE_OK;
    }
    const mc_block *block;
    status = selected_block(world, id, 34, 39, &error->weak_at, &block, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    error->weak_at.bytecode = 46;
    mc_world_power_error lower;
    mc_world_power_error_init(&lower);
    bool value;
    status = mc_world_block_weak_power_held(world, block, x, y, z, side, &value, &lower);
    if (status != MC_WORLD_STORAGE_OK)
        return block_failure(world, error, &lower);
    *out = value;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status weak_neighbors(mc_world_runtime *world, int32_t x, int32_t y,
                                              int32_t z, bool *out, mc_world_power_error *error) {
    error->weak_neighbors = site(x, y, z, -1);
    for (int32_t side = 0; side < 6; ++side) {
        int32_t nx, ny, nz;
        neighbor_coordinates(x, y, z, side, &nx, &ny, &nz);
        error->weak_neighbors.bytecode = 7u + 15u * (unsigned int)side;
        bool powered;
        const mc_world_storage_status status = weak_at(world, nx, ny, nz, side, &powered, error);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
        if (powered) {
            *out = true;
            return MC_WORLD_STORAGE_OK;
        }
    }
    *out = false;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status held(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                    int32_t side, bool *out, mc_world_power_error *error,
                                    power_method method) {
    mc_world_power_error frame;
    mc_world_power_error_init(&frame);
    mc_world_storage_status status;
    if (world == NULL || out == NULL)
        status = admission(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World power held arguments");
    else if (world->classes == NULL)
        status = admission(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "World power defining classes binding");
    else {
        frame.world_class = world->classes->side == MC_WORLD_INFO_CLIENT ? "fd" : "dj";
        bool value = false;
        switch (method) {
            case POWER_WEAK_NEIGHBORS:
                status = weak_neighbors(world, x, y, z, &value, &frame);
                break;
            case POWER_WEAK_AT:
                status = weak_at(world, x, y, z, side, &value, &frame);
                break;
            case POWER_STRONG_NEIGHBORS:
                status = strong_neighbors(world, x, y, z, &value, &frame);
                break;
            case POWER_STRONG_AT:
                status = strong_at(world, x, y, z, side, &value, &frame);
                break;
            default:
                status = admission(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                   "World power native method selector");
                break;
        }
        if (status == MC_WORLD_STORAGE_OK) {
            *out = value;
            mc_world_power_error_init(&frame);
        }
    }
    if (error != NULL)
        *error = frame;
    return status;
}

mc_world_storage_status mc_world_power_neighbors_held(mc_world_runtime *world, int32_t x, int32_t y,
                                                      int32_t z, bool *out,
                                                      mc_world_power_error *error) {
    return held(world, x, y, z, -1, out, error, POWER_WEAK_NEIGHBORS);
}

mc_world_storage_status mc_world_power_at_held(mc_world_runtime *world, int32_t x, int32_t y,
                                               int32_t z, int32_t side, bool *out,
                                               mc_world_power_error *error) {
    return held(world, x, y, z, side, out, error, POWER_WEAK_AT);
}

mc_world_storage_status mc_world_power_strong_neighbors_held(mc_world_runtime *world, int32_t x,
                                                             int32_t y, int32_t z, bool *out,
                                                             mc_world_power_error *error) {
    return held(world, x, y, z, -1, out, error, POWER_STRONG_NEIGHBORS);
}

mc_world_storage_status mc_world_power_strong_at_held(mc_world_runtime *world, int32_t x, int32_t y,
                                                      int32_t z, int32_t side, bool *out,
                                                      mc_world_power_error *error) {
    return held(world, x, y, z, side, out, error, POWER_STRONG_AT);
}

mc_world_storage_status mc_world_runtime_is_powered(mc_world_runtime *world, int32_t x, int32_t y,
                                                    int32_t z, bool *out,
                                                    mc_world_power_error *error) {
    mc_world_power_error frame;
    mc_world_power_error_init(&frame);
    mc_world_storage_status status;
    if (world == NULL || out == NULL)
        status = admission(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World power arguments");
    else if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
             world->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        status = admission(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World power phase");
    else {
        status = mc_world_runtime_call_enter(world, &frame.cause);
        if (status != MC_WORLD_STORAGE_OK) {
            frame.status = status;
            frame.operation = frame.cause.operation;
        } else {
            status = mc_world_power_neighbors_held(world, x, y, z, out, &frame);
            mc_world_runtime_call_leave(world);
        }
    }
    if (error != NULL)
        *error = frame;
    return status;
}
