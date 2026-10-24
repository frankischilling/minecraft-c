#include "power_internal.h"

#include "../../../block/registry/horizontal_directions_internal.h"
#include "../../../block/registry/registry_internal.h"
#include "../../../block/registry/runtime_internal.h"
#include "../block_queries_internal.h"
#include "../collision/blocks_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"

typedef enum block_power_method {
    BLOCK_CAPABILITY,
    BLOCK_STRONG,
    BLOCK_WEAK,
    REDSTONE_CONNECTIVITY
} block_power_method;

static mc_world_storage_status capability_kernel(const mc_block *block, bool *out,
                                                 mc_world_power_error *frame);

static mc_world_block_power_site *site(mc_world_power_error *error, block_power_method method) {
    switch (method) {
        case BLOCK_CAPABILITY:
            return &error->capability;
        case BLOCK_STRONG:
            return &error->strong;
        case BLOCK_WEAK:
            return &error->weak;
        case REDSTONE_CONNECTIVITY:
            return &error->redstone_connectivity;
    }
    return &error->weak;
}

/* Reset only this callee and its descendants. Explicit World and strong/weak
 * callers survive a delegated output or static connectivity failure. */
static void clear(mc_world_power_error *error, block_power_method method) {
    const mc_world_power_error empty = mc_world_power_empty_error();
    error->status = empty.status;
    error->operation = empty.operation;
    error->cause = empty.cause;
    error->registry = empty.registry;
    error->support = empty.support;
    error->access = empty.access;
    error->capability = empty.capability;
    if (method == BLOCK_STRONG)
        error->strong = empty.strong;
    if (method == BLOCK_STRONG || method == BLOCK_WEAK)
        error->weak = empty.weak;
    if (method != BLOCK_CAPABILITY)
        error->redstone_connectivity = empty.redstone_connectivity;
}

static mc_world_storage_status fail(mc_world_runtime *world, mc_world_power_error *error,
                                    mc_world_storage_status status, const char *operation) {
    error->status = status;
    error->operation = operation;
    error->cause = (mc_world_storage_error){status, operation, 0};
    if (world != NULL)
        world->last_error = error->cause;
    return status;
}

static bool admit(const mc_block *block, const bool *out, mc_world_power_error *error) {
    if (out == NULL) {
        (void)fail(NULL, error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Block power output argument");
        return false;
    }
    if (block == NULL) {
        (void)fail(NULL, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                   "Block power invocation receiver is null");
        return false;
    }
    if (block->classes == NULL || (unsigned int)block->kind >= (unsigned int)MC_BLOCK_KIND_COUNT) {
        (void)fail(NULL, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                   "Block power native receiver owner or kind");
        return false;
    }
    return true;
}

static void begin(const mc_block *block, block_power_method method, mc_world_power_error *error) {
    const bool client = block->classes->options.side == MC_REGISTRY_CLIENT;
    const char *declaring_class;
    switch (block->kind) {
        case MC_BLOCK_PRESSURE_PLATE:
            declaring_class = client ? "bv" : "bc";
            break;
        case MC_BLOCK_DETECTOR_RAIL:
            declaring_class = client ? "ph" : "jn";
            break;
        case MC_BLOCK_BUTTON:
            declaring_class = client ? "oi" : "iz";
            break;
        case MC_BLOCK_LEVER:
            declaring_class = client ? "xr" : "pd";
            break;
        case MC_BLOCK_REDSTONE_TORCH:
            declaring_class = client ? "db" : "by";
            break;
        case MC_BLOCK_REDSTONE:
            declaring_class = client ? "sm" : "lo";
            break;
        case MC_BLOCK_REPEATER:
            declaring_class = client ? "wo" : "oi";
            break;
        default:
            declaring_class = client ? "uu" : "na";
            break;
    }
    const char *descriptor;
    switch (method) {
        case BLOCK_CAPABILITY:
            descriptor = client ? "f()Z" : "d()Z";
            break;
        case BLOCK_STRONG:
            descriptor = client ? "d(Lfd;IIII)Z" : "d(Ldj;IIII)Z";
            break;
        case BLOCK_WEAK:
            descriptor = client ? "c(Lxp;IIII)Z" : "a(Lpb;IIII)Z";
            break;
        case REDSTONE_CONNECTIVITY:
            descriptor = client ? "e(Lxp;IIII)Z" : "c(Lpb;IIII)Z";
            break;
        default:
            descriptor = NULL;
            break;
    }
    *site(error, method) = (mc_world_block_power_site){
        .entered = true, .declaring_class = declaring_class, .method = descriptor};
}

static void at(mc_world_power_error *error, block_power_method method, unsigned int pc) {
    site(error, method)->bytecode = pc;
}

static bool read_value(mc_world_runtime *world, int32_t x, int32_t y, int32_t z, bool metadata,
                       int32_t *out, block_power_method method, unsigned int pc,
                       mc_world_power_error *error) {
    at(error, method, pc);
    mc_world_collision_error lower = mc_world_collision_empty_error();
    const mc_world_storage_status status =
        mc_world_collision_block_value_held(world, x, y, z, metadata, out, &lower);
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    error->access = lower.nearby;
    error->registry = lower.nearby.query.registry;
    error->status = status;
    error->operation = lower.nearby.query.operation;
    error->cause = lower.nearby.query.cause;
    return false;
}

static bool support(mc_world_runtime *world, int32_t x, int32_t y, int32_t z, bool *out,
                    unsigned int pc, mc_world_power_error *error) {
    at(error, BLOCK_WEAK, pc);
    mc_world_solid_support_detail lower;
    const mc_world_storage_status status =
        mc_world_solid_support_detailed_held(world, x, y, z, out, &lower);
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    error->support = lower.error;
    error->access = lower.access;
    error->registry = lower.error.registry;
    error->status = status;
    error->operation = lower.error.operation;
    error->cause = lower.error.cause;
    return false;
}

static bool registry_failure(mc_world_runtime *world, const mc_registry_error *lower,
                             const char *operation, mc_world_power_error *error) {
    error->registry = *lower;
    error->status =
        mc_world_runtime_registry_result(world, lower->status, lower, operation, &error->cause);
    error->operation = error->cause.operation;
    return false;
}

static bool static_id(mc_world_runtime *world, const mc_block *redstone, const char *field,
                      unsigned int static_pc, unsigned int field_pc, int32_t *out,
                      mc_world_power_error *error) {
    at(error, REDSTONE_CONNECTIVITY, static_pc);
    mc_registry_error lower = mc_world_power_empty_error().registry;
    const mc_block *selected = NULL;
    if (mc_registry_block_static_get(redstone->classes, field, &selected, &lower) != MC_REGISTRY_OK)
        return registry_failure(world, &lower, "Redstone connectivity Block static active use",
                                error);
    at(error, REDSTONE_CONNECTIVITY, field_pc);
    if (selected == NULL) {
        (void)fail(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                   "Redstone connectivity static Block receiver is null");
        return false;
    }
    *out = selected->fields.bn;
    return true;
}

/* Paired sm.e/lo.c(BlockAccess,IIII), distinct from either directional
 * output. Source ID4 precedes every declaring-namespace static active use.
 * aw can match even ID0; bi/bj and the orientation class remain late. */
static bool connectivity(mc_world_runtime *world, const mc_block *redstone, int32_t x, int32_t y,
                         int32_t z, int32_t direction, bool *out, mc_world_power_error *error) {
    clear(error, REDSTONE_CONNECTIVITY);
    begin(redstone, REDSTONE_CONNECTIVITY, error);
    int32_t id, expected;
    if (!read_value(world, x, y, z, false, &id, REDSTONE_CONNECTIVITY, 4, error) ||
        !static_id(world, redstone, "aw", 13, 16, &expected, error))
        return false;
    bool value = id == expected;
    if (!value && id != 0) {
        at(error, REDSTONE_CONNECTIVITY, 31);
        mc_registry_block_m_view array;
        mc_registry_error lower = mc_world_power_empty_error().registry;
        if (mc_registry_block_m_get(redstone->classes, &array, &lower) != MC_REGISTRY_OK)
            return registry_failure(world, &lower, "Redstone connectivity Block.m active use",
                                    error);
        at(error, REDSTONE_CONNECTIVITY, 36);
        const mc_block *selected = NULL;
        if (mc_registry_block_m_at(&array, id, &selected, &lower) != MC_REGISTRY_OK)
            return registry_failure(world, &lower, "Redstone connectivity Block.m element", error);
        at(error, REDSTONE_CONNECTIVITY, 37);
        if (capability_kernel(selected, &value, error) != MC_WORLD_STORAGE_OK) {
            if (world != NULL)
                world->last_error = error->cause;
            return false;
        }
        if (!value) {
            if (!static_id(world, redstone, "bi", 47, 50, &expected, error))
                return false;
            bool diode = id == expected;
            if (!diode) {
                if (!static_id(world, redstone, "bj", 58, 61, &expected, error))
                    return false;
                diode = id == expected;
            }
            if (diode) {
                int32_t metadata;
                if (!read_value(world, x, y, z, true, &metadata, REDSTONE_CONNECTIVITY, 71, error))
                    return false;
                /* The actual jj/ge.b capture and iaload are supplied by the
                 * independent four-direction class, not wj/od or a constant. */
                at(error, REDSTONE_CONNECTIVITY, 80);
                mc_registry_horizontal_view orientation;
                lower = mc_world_power_empty_error().registry;
                if (mc_registry_horizontal_b_get(redstone->classes, &orientation, &lower) !=
                    MC_REGISTRY_OK)
                    return registry_failure(
                        world, &lower, "Redstone connectivity orientation b active use", error);
                at(error, REDSTONE_CONNECTIVITY, 87);
                if (mc_registry_horizontal_at(&orientation, metadata & 3, &expected, &lower) !=
                    MC_REGISTRY_OK)
                    return registry_failure(world, &lower,
                                            "Redstone connectivity orientation b index", error);
                value = direction == expected;
            }
        }
    }
    *out = value;
    clear(error, REDSTONE_CONNECTIVITY);
    return true;
}

static bool connected(mc_world_runtime *world, const mc_block *redstone, int32_t x, int32_t y,
                      int32_t z, int32_t direction, unsigned int pc, bool *out,
                      mc_world_power_error *error) {
    at(error, BLOCK_WEAK, pc);
    return connectivity(world, redstone, x, y, z, direction, out, error);
}

/* Complete paired sm/lo weak body after the a/metadata/side1 prefix. The four
 * first horizontal tests and repeated elevated support tests remain separate.
 * Existing true locals do not skip later source support/connectivity reads. */
static bool redstone_output(mc_world_runtime *world, const mc_block *redstone, int32_t x, int32_t y,
                            int32_t z, int32_t side, bool *out, mc_world_power_error *error) {
    const int32_t west_x = mc_java_i32_sub(x, 1), east_x = mc_java_i32_add(x, 1);
    const int32_t north_z = mc_java_i32_sub(z, 1), south_z = mc_java_i32_add(z, 1);
    const int32_t below = mc_java_i32_sub(y, 1), above = mc_java_i32_add(y, 1);
    bool west, east, north, south, solid, extra;
    if (!connected(world, redstone, west_x, y, z, 1, 40, &west, error))
        return false;
    if (!west) {
        if (!support(world, west_x, y, z, &solid, 53, error))
            return false;
        if (!solid && !connected(world, redstone, west_x, below, z, -1, 71, &west, error))
            return false;
    }
    if (!connected(world, redstone, east_x, y, z, 3, 92, &east, error))
        return false;
    if (!east) {
        if (!support(world, east_x, y, z, &solid, 105, error))
            return false;
        if (!solid && !connected(world, redstone, east_x, below, z, -1, 123, &east, error))
            return false;
    }
    if (!connected(world, redstone, x, y, north_z, 2, 144, &north, error))
        return false;
    if (!north) {
        if (!support(world, x, y, north_z, &solid, 157, error))
            return false;
        if (!solid && !connected(world, redstone, x, below, north_z, -1, 175, &north, error))
            return false;
    }
    if (!connected(world, redstone, x, y, south_z, 0, 196, &south, error))
        return false;
    if (!south) {
        if (!support(world, x, y, south_z, &solid, 209, error))
            return false;
        if (!solid && !connected(world, redstone, x, below, south_z, -1, 227, &south, error))
            return false;
    }
    if (!support(world, x, above, z, &solid, 247, error))
        return false;
    if (!solid) {
        if (!support(world, west_x, y, z, &solid, 262, error))
            return false;
        if (solid) {
            if (!connected(world, redstone, west_x, above, z, -1, 280, &extra, error))
                return false;
            if (extra)
                west = true;
        }
        if (!support(world, east_x, y, z, &solid, 296, error))
            return false;
        if (solid) {
            if (!connected(world, redstone, east_x, above, z, -1, 314, &extra, error))
                return false;
            if (extra)
                east = true;
        }
        if (!support(world, x, y, north_z, &solid, 330, error))
            return false;
        if (solid) {
            if (!connected(world, redstone, x, above, north_z, -1, 348, &extra, error))
                return false;
            if (extra)
                north = true;
        }
        if (!support(world, x, y, south_z, &solid, 364, error))
            return false;
        if (solid) {
            if (!connected(world, redstone, x, above, south_z, -1, 382, &extra, error))
                return false;
            if (extra)
                south = true;
        }
    }
    *out = (!north && !east && !west && !south && side >= 2 && side <= 5) ||
           (side == 2 && north && !west && !east) || (side == 3 && south && !west && !east) ||
           (side == 4 && west && !north && !south) || (side == 5 && east && !north && !south);
    return true;
}

/* All admitted kinds resolve the complete seven declaring overrides or the
 * uu/na constant-false base. Native unknown kinds never become false outputs. */
static mc_world_storage_status capability_kernel(const mc_block *block, bool *out,
                                                 mc_world_power_error *frame) {
    clear(frame, BLOCK_CAPABILITY);
    if (!admit(block, out, frame))
        return frame->status;
    begin(block, BLOCK_CAPABILITY, frame);
    bool value;
    switch (block->kind) {
        case MC_BLOCK_PRESSURE_PLATE:
        case MC_BLOCK_DETECTOR_RAIL:
        case MC_BLOCK_BUTTON:
        case MC_BLOCK_LEVER:
        case MC_BLOCK_REDSTONE_TORCH:
            value = true;
            break;
        case MC_BLOCK_REDSTONE:
            at(frame, BLOCK_CAPABILITY, 1);
            value = block->subtype.sm.a;
            break;
        default:
            value = false; /* wo/oi also declares its own false0/1. */
            break;
    }
    *out = value;
    clear(frame, BLOCK_CAPABILITY);
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status weak_kernel(mc_world_runtime *world, const mc_block *block,
                                           int32_t x, int32_t y, int32_t z, int32_t side, bool *out,
                                           mc_world_power_error *frame) {
    clear(frame, BLOCK_WEAK);
    if (!admit(block, out, frame))
        return frame->status;
    begin(block, BLOCK_WEAK, frame);
    bool value = false;
    int32_t metadata;
    switch (block->kind) {
        case MC_BLOCK_PRESSURE_PLATE:
        case MC_BLOCK_DETECTOR_RAIL:
        case MC_BLOCK_BUTTON:
        case MC_BLOCK_LEVER:
            if (!read_value(world, x, y, z, true, &metadata, BLOCK_WEAK, 5, frame))
                return frame->status;
            value = block->kind == MC_BLOCK_PRESSURE_PLATE ? metadata > 0 : (metadata & 8) != 0;
            break;
        case MC_BLOCK_REDSTONE_TORCH:
            at(frame, BLOCK_WEAK, 1);
            if (!block->subtype.db.a)
                break;
            if (!read_value(world, x, y, z, true, &metadata, BLOCK_WEAK, 14, frame))
                return frame->status;
            /* Redstone torch: exact raw metadata equality, with no mask. */
            value = !((metadata == 5 && side == 1) || (metadata == 3 && side == 3) ||
                      (metadata == 4 && side == 2) || (metadata == 1 && side == 5) ||
                      (metadata == 2 && side == 4));
            break;
        case MC_BLOCK_REPEATER:
            at(frame, BLOCK_WEAK, 1);
            if (!block->subtype.wo.c)
                break;
            if (!read_value(world, x, y, z, true, &metadata, BLOCK_WEAK, 14, frame))
                return frame->status;
            metadata &= 3;
            value = (metadata == 0 && side == 3) || (metadata == 1 && side == 4) ||
                    (metadata == 2 && side == 2) || (metadata == 3 && side == 5);
            break;
        case MC_BLOCK_REDSTONE:
            at(frame, BLOCK_WEAK, 1);
            if (!block->subtype.sm.a)
                break;
            if (!read_value(world, x, y, z, true, &metadata, BLOCK_WEAK, 14, frame))
                return frame->status;
            if (metadata == 0)
                break;
            if (side == 1)
                value = true;
            else if (!redstone_output(world, block, x, y, z, side, &value, frame))
                return frame->status;
            break;
        default:
            break; /* uu/na.c(xp,...)/a(pb,...) is false0/1. */
    }
    *out = value;
    clear(frame, BLOCK_WEAK);
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status strong_kernel(mc_world_runtime *world, const mc_block *block,
                                             int32_t x, int32_t y, int32_t z, int32_t side,
                                             bool *out, mc_world_power_error *frame) {
    clear(frame, BLOCK_STRONG);
    if (!admit(block, out, frame))
        return frame->status;
    begin(block, BLOCK_STRONG, frame);
    bool value = false;
    int32_t metadata;
    switch (block->kind) {
        case MC_BLOCK_PRESSURE_PLATE:
        case MC_BLOCK_DETECTOR_RAIL:
        case MC_BLOCK_BUTTON:
        case MC_BLOCK_LEVER:
            if (!read_value(world, x, y, z, true, &metadata, BLOCK_STRONG, 5, frame))
                return frame->status;
            if (block->kind == MC_BLOCK_PRESSURE_PLATE)
                value = metadata != 0 && side == 1;
            else if (block->kind == MC_BLOCK_DETECTOR_RAIL)
                value = (metadata & 8) != 0 && side == 1;
            else if ((metadata & 8) != 0) {
                const int32_t orientation = metadata & 7;
                value = (block->kind == MC_BLOCK_LEVER && orientation == 6 && side == 1) ||
                        (orientation == 5 && side == 1) || (orientation == 4 && side == 2) ||
                        (orientation == 3 && side == 3) || (orientation == 2 && side == 4) ||
                        (orientation == 1 && side == 5);
            }
            break;
        case MC_BLOCK_REDSTONE_TORCH:
            if (side != 0)
                break;
            at(frame, BLOCK_STRONG, 13);
            if (weak_kernel(world, block, x, y, z, side, &value, frame) != MC_WORLD_STORAGE_OK)
                return frame->status;
            break;
        case MC_BLOCK_REPEATER:
            at(frame, BLOCK_STRONG, 8);
            if (weak_kernel(world, block, x, y, z, side, &value, frame) != MC_WORLD_STORAGE_OK)
                return frame->status;
            break;
        case MC_BLOCK_REDSTONE:
            at(frame, BLOCK_STRONG, 1);
            if (!block->subtype.sm.a)
                break;
            at(frame, BLOCK_STRONG, 17);
            if (weak_kernel(world, block, x, y, z, side, &value, frame) != MC_WORLD_STORAGE_OK)
                return frame->status;
            break;
        default:
            break; /* uu/na.d(World,...) is false0/1 without World use. */
    }
    *out = value;
    clear(frame, BLOCK_STRONG);
    return MC_WORLD_STORAGE_OK;
}

/* Callable held entries always replace caller error storage with a fresh
 * frame. Only the private kernels above share their current source callers. */
mc_world_storage_status mc_world_block_power_capability_held(const mc_block *block, bool *out,
                                                             mc_world_power_error *error) {
    mc_world_power_error frame = mc_world_power_empty_error();
    const mc_world_storage_status status = capability_kernel(block, out, &frame);
    if (error != NULL)
        *error = frame;
    return status;
}

mc_world_storage_status mc_world_block_weak_power_held(mc_world_runtime *world,
                                                       const mc_block *block, int32_t x, int32_t y,
                                                       int32_t z, int32_t side, bool *out,
                                                       mc_world_power_error *error) {
    mc_world_power_error frame = mc_world_power_empty_error();
    const mc_world_storage_status status = weak_kernel(world, block, x, y, z, side, out, &frame);
    if (error != NULL)
        *error = frame;
    return status;
}

mc_world_storage_status mc_world_block_strong_power_held(mc_world_runtime *world,
                                                         const mc_block *block, int32_t x,
                                                         int32_t y, int32_t z, int32_t side,
                                                         bool *out, mc_world_power_error *error) {
    mc_world_power_error frame = mc_world_power_empty_error();
    const mc_world_storage_status status = strong_kernel(world, block, x, y, z, side, out, &frame);
    if (error != NULL)
        *error = frame;
    return status;
}
