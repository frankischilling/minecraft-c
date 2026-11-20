#include "mutation_internal.h"

#include "../../../block/registry/registry_internal.h"
#include "../collision/blocks_internal.h"
#include "mc/core/java/java.h"
#include "mc/world/runtime/blocks.h"

typedef enum pressure_lifecycle_method {
    PRESSURE_NEIGHBOR,
    PRESSURE_REMOVED
} pressure_lifecycle_method;

typedef struct pressure_lifecycle_frame {
    mc_world_runtime *world;
    const mc_block *block;
    mc_world_mutation_error *error;
    pressure_lifecycle_method method;
} pressure_lifecycle_frame;

static void pressure_at(const pressure_lifecycle_frame *frame, unsigned int pc) {
    const bool client = frame->block->classes->options.side == MC_REGISTRY_CLIENT;
    frame->error->hook_class = client ? "bv" : "bc";
    frame->error->hook_bytecode = pc;
    if (frame->method == PRESSURE_NEIGHBOR) {
        frame->error->hook_method = client ? "b(Lfd;IIII)V" : "b(Ldj;IIII)V";
        frame->error->pressure_neighbor_bytecode = pc;
    } else {
        frame->error->hook_method = client ? "b(Lfd;III)V" : "b(Ldj;III)V";
        frame->error->pressure_removed_bytecode = pc;
    }
}

static bool pressure_done(mc_world_mutation_error *error) {
    error->hook_class = NULL;
    error->hook_method = NULL;
    error->hook_bytecode = 0;
    error->pressure_contact_bytecode = 0;
    error->pressure_tick_bytecode = 0;
    error->pressure_bytecode = 0;
    error->pressure_neighbor_bytecode = 0;
    error->pressure_removed_bytecode = 0;
    return true;
}

static bool pressure_storage_fail(const pressure_lifecycle_frame *frame, unsigned int pc,
                                  mc_world_storage_error cause) {
    pressure_at(frame, pc);
    frame->error->status = cause.status;
    frame->error->operation = cause.operation;
    frame->error->cause = cause;
    if (frame->world != NULL)
        frame->world->last_error = cause;
    return false;
}

static bool pressure_metadata(const pressure_lifecycle_frame *frame, unsigned int pc, int32_t x,
                              int32_t y, int32_t z, int32_t *out) {
    mc_world_collision_error lower = mc_world_collision_empty_error();
    pressure_at(frame, pc);
    if (mc_world_collision_block_value_held(frame->world, x, y, z, true, out, &lower) ==
        MC_WORLD_STORAGE_OK)
        return true;
    frame->error->nearby = lower.nearby;
    frame->error->query = lower.nearby.query;
    frame->error->registry = lower.nearby.query.registry;
    frame->error->population = lower.nearby.query.population;
    return pressure_storage_fail(frame, pc, lower.nearby.query.cause);
}

/* Preserve the complete lower operation and its deepest concrete hook before
 * restoring this current caller. The lower error may alias the destination;
 * the finite projection introduces no source recursion or work limit. */
static bool pressure_nested(const pressure_lifecycle_frame *frame, unsigned int pc,
                            const mc_world_mutation_error *lower) {
    const mc_world_mutation_error caller = *frame->error;
    const mc_world_mutation_error captured = *lower;
    *frame->error = captured;
    if (caller.pressure_contact_bytecode != 0)
        frame->error->pressure_contact_bytecode = caller.pressure_contact_bytecode;
    if (caller.pressure_tick_bytecode != 0)
        frame->error->pressure_tick_bytecode = caller.pressure_tick_bytecode;
    if (caller.pressure_bytecode != 0)
        frame->error->pressure_bytecode = caller.pressure_bytecode;
    if (caller.pressure_neighbor_bytecode != 0)
        frame->error->pressure_neighbor_bytecode = caller.pressure_neighbor_bytecode;
    if (caller.pressure_removed_bytecode != 0)
        frame->error->pressure_removed_bytecode = caller.pressure_removed_bytecode;
    if (captured.cause_hook_class == NULL) {
        frame->error->cause_hook_class = captured.hook_class;
        frame->error->cause_hook_method = captured.hook_method;
        frame->error->cause_hook_bytecode = captured.hook_bytecode;
    }
    pressure_at(frame, pc);
    if (frame->world != NULL)
        frame->world->last_error = captured.cause;
    return false;
}

/* Caller holds the actual World and Block through all synchronous calls.
 * bv/bc.b(World,IIII) ignores the supplied source ID and reaches support10
 * directly, without a remote guard or the separate placement wrapper. */
bool mc_world_mutation_pressure_neighbor(mc_world_runtime *world, const mc_block *block, int32_t x,
                                         int32_t y, int32_t z, int32_t source_id,
                                         mc_world_mutation_error *error) {
    (void)source_id;
    const pressure_lifecycle_frame frame = {world, block, error, PRESSURE_NEIGHBOR};
    bool supported;
    mc_world_solid_support_error support;
    pressure_at(&frame, 10);
    if (mc_world_runtime_solid_support_get(world, x, mc_java_i32_sub(y, 1), z, &supported,
                                           &support) != MC_WORLD_STORAGE_OK) {
        error->support_bytecode = support.bytecode;
        error->registry = support.registry;
        return pressure_storage_fail(&frame, 10, support.cause);
    }
    if (supported)
        return pressure_done(error); /* return51 without reading metadata. */

    int32_t metadata;
    if (!pressure_metadata(&frame, 35, x, y, z, &metadata))
        return false;
    pressure_at(&frame, 38);
    if (!mc_world_mutation_drop(world, block, x, y, z, metadata, error))
        return pressure_nested(&frame, 38, error);

    bool ignored;
    mc_world_mutation_error lower = mc_world_mutation_empty_error();
    pressure_at(&frame, 47);
    if (mc_world_runtime_block_set(world, x, y, z, 0, &ignored, &lower) != MC_WORLD_STORAGE_OK)
        return pressure_nested(&frame, 47, &lower);
    return pressure_done(error); /* The notifying setter's bool is discarded. */
}

/* bv/bc.b(World,III) rereads metadata after the enclosing mutation has reached
 * removal. Positive metadata notifies twice; each bn read remains late across
 * the preceding synchronous notification and may observe its mutations. */
bool mc_world_mutation_pressure_removed(mc_world_runtime *world, const mc_block *block, int32_t x,
                                        int32_t y, int32_t z, mc_world_mutation_error *error) {
    const pressure_lifecycle_frame frame = {world, block, error, PRESSURE_REMOVED};
    int32_t metadata;
    if (!pressure_metadata(&frame, 5, x, y, z, &metadata))
        return false;
    if (metadata > 0) {
        pressure_at(&frame, 24);
        if (!mc_world_mutation_neighbors(world, x, y, z, block->fields.bn, error))
            return pressure_nested(&frame, 24, error);
        pressure_at(&frame, 38);
        if (!mc_world_mutation_neighbors(world, x, mc_java_i32_sub(y, 1), z, block->fields.bn,
                                         error))
            return pressure_nested(&frame, 38, error);
    }

    /* invokespecial47 selects the proved empty uu/na.b(World,III), rather
     * than redispatching this pressure override. That body returns at0. */
    pressure_at(&frame, 47);
    return pressure_done(error);
}
