#include "redstone_internal.h"

#include "../../block_queries_internal.h"
#include "../../collision/blocks_internal.h"

#include <stdlib.h>

mc_redstone_frame mc_redstone_frame_make(mc_world_runtime *world, const mc_block *block,
                                         mc_world_mutation_error *error,
                                         mc_redstone_method method) {
    /* Dispatch exposes a borrowed const view of the mutable Registry object. */
    return (mc_redstone_frame){world, (mc_block *)block, error, method};
}

void mc_redstone_at(const mc_redstone_frame *frame, unsigned int pc) {
    const bool client = frame->block->classes->options.side == MC_REGISTRY_CLIENT;
    mc_world_mutation_error *error = frame->error;
    error->hook_class = client ? "sm" : "lo";
    error->hook_bytecode = pc;
    switch (frame->method) {
        case MC_REDSTONE_ADDED:
            error->hook_method = client ? "c(Lfd;III)V" : "c(Ldj;III)V";
            error->redstone.added = pc;
            break;
        case MC_REDSTONE_REMOVED:
            error->hook_method = client ? "b(Lfd;III)V" : "b(Ldj;III)V";
            error->redstone.removed = pc;
            break;
        case MC_REDSTONE_NEIGHBOR:
            error->hook_method = client ? "b(Lfd;IIII)V" : "b(Ldj;IIII)V";
            error->redstone.neighbor = pc;
            break;
        case MC_REDSTONE_SUPPORT:
            error->hook_method = client ? "a(Lfd;III)Z" : "a(Ldj;III)Z";
            error->redstone.support = pc;
            break;
        case MC_REDSTONE_DRAIN:
            error->hook_method = client ? "h(Lfd;III)V" : "g(Ldj;III)V";
            error->redstone.drain = pc;
            break;
        case MC_REDSTONE_UPDATE:
            error->hook_method = client ? "a(Lfd;IIIIII)V" : "a(Ldj;IIIIII)V";
            error->redstone.update = pc;
            break;
        case MC_REDSTONE_NOTIFY:
            error->hook_method = client ? "i(Lfd;III)V" : "h(Ldj;III)V";
            error->redstone.notify = pc;
            break;
        case MC_REDSTONE_STRENGTH:
            error->hook_method = client ? "f(Lfd;IIII)I" : "f(Ldj;IIII)I";
            error->redstone.strength = pc;
            break;
    }
}

bool mc_redstone_done(const mc_redstone_frame *frame) {
    frame->error->hook_class = NULL;
    frame->error->hook_method = NULL;
    frame->error->hook_bytecode = 0;
    frame->error->redstone = (mc_world_redstone_sites){0};
    return true;
}

bool mc_redstone_fail(const mc_redstone_frame *frame, unsigned int pc,
                      mc_world_storage_status status, const char *operation) {
    mc_redstone_at(frame, pc);
    return mc_world_mutation_fail(frame->world, frame->error, status, operation);
}

static bool storage_fail(const mc_redstone_frame *frame, unsigned int pc,
                         const mc_world_storage_error *cause) {
    mc_redstone_at(frame, pc);
    frame->error->status = cause->status;
    frame->error->operation = cause->operation;
    frame->error->cause = *cause;
    if (frame->world != NULL)
        frame->world->last_error = *cause;
    return false;
}

bool mc_redstone_nested(const mc_redstone_frame *frame, unsigned int pc,
                        const mc_world_mutation_error *lower) {
    const mc_world_redstone_sites saved = frame->error->redstone;
    if (frame->error != lower)
        *frame->error = *lower;
#define KEEP(field)                                                                                \
    if (saved.field != 0)                                                                          \
    frame->error->redstone.field = saved.field
    KEEP(added);
    KEEP(removed);
    KEEP(neighbor);
    KEEP(support);
    KEEP(drain);
    KEEP(update);
    KEEP(notify);
    KEEP(strength);
#undef KEEP
    if (frame->error->cause_hook_class == NULL) {
        frame->error->cause_hook_class = frame->error->hook_class;
        frame->error->cause_hook_method = frame->error->hook_method;
        frame->error->cause_hook_bytecode = frame->error->hook_bytecode;
    }
    const mc_world_storage_error cause = frame->error->cause;
    mc_redstone_at(frame, pc);
    if (frame->world != NULL)
        frame->world->last_error = cause;
    return false;
}

bool mc_redstone_collection(const mc_redstone_frame *frame, unsigned int pc,
                            mc_registry_status status) {
    if (status == MC_REGISTRY_OK)
        return true;
    mc_redstone_at(frame, pc);
    mc_registry_error cause = mc_world_mutation_empty_registry();
    cause.status = status;
    cause.message = "Redstone position collection";
    return mc_world_mutation_registry_fail(frame->world, frame->error, &cause, cause.message);
}

mc_world_mutation_error *mc_redstone_workspace(const mc_redstone_frame *frame, unsigned int pc) {
    mc_world_mutation_error *lower = malloc(sizeof(*lower));
    if (lower == NULL)
        (void)mc_redstone_fail(frame, pc, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                               "Redstone child diagnostic allocation");
    else
        mc_world_mutation_error_init(lower);
    return lower;
}

bool mc_redstone_child(const mc_redstone_frame *frame, unsigned int pc,
                       mc_world_mutation_error *lower, bool complete) {
    if (!complete)
        (void)mc_redstone_nested(frame, pc, lower);
    free(lower);
    return complete;
}

bool mc_redstone_remote(const mc_redstone_frame *frame, unsigned int pc, bool *out) {
    mc_redstone_at(frame, pc);
    if (frame->world == NULL)
        return mc_redstone_fail(frame, pc, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                "Redstone World remote receiver is null");
    *out = frame->world->fields.B;
    return true;
}

bool mc_redstone_value(const mc_redstone_frame *frame, unsigned int pc, int32_t x, int32_t y,
                       int32_t z, bool metadata, int32_t *out) {
    mc_redstone_at(frame, pc);
    mc_world_collision_error *lower = malloc(sizeof(*lower));
    if (lower == NULL)
        return mc_redstone_fail(frame, pc, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                "Redstone value diagnostic allocation");
    *lower = mc_world_collision_empty_error();
    const mc_world_storage_status status =
        mc_world_collision_block_value_held(frame->world, x, y, z, metadata, out, lower);
    if (status != MC_WORLD_STORAGE_OK) {
        frame->error->nearby = lower->nearby;
        frame->error->query = lower->nearby.query;
        frame->error->registry = lower->nearby.query.registry;
        frame->error->population = lower->nearby.query.population;
        frame->error->redstone_value_failed = true;
        frame->error->redstone_x = lower->x;
        frame->error->redstone_y = lower->y;
        frame->error->redstone_z = lower->z;
        (void)storage_fail(frame, pc, &lower->nearby.query.cause);
    }
    free(lower);
    return status == MC_WORLD_STORAGE_OK;
}

bool mc_redstone_solid(const mc_redstone_frame *frame, unsigned int pc, int32_t x, int32_t y,
                       int32_t z, bool *out) {
    mc_redstone_at(frame, pc);
    mc_world_solid_support_detail *lower = malloc(sizeof(*lower));
    if (lower == NULL)
        return mc_redstone_fail(frame, pc, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                "Redstone support diagnostic allocation");
    const mc_world_storage_status status =
        mc_world_solid_support_detailed_held(frame->world, x, y, z, out, lower);
    if (status != MC_WORLD_STORAGE_OK) {
        frame->error->support_bytecode = lower->error.bytecode;
        frame->error->nearby = lower->access;
        frame->error->query = lower->access.query;
        frame->error->registry = lower->error.registry;
        frame->error->population = lower->access.query.population;
        (void)storage_fail(frame, pc, &lower->error.cause);
    }
    free(lower);
    return status == MC_WORLD_STORAGE_OK;
}

bool mc_redstone_neighbors(const mc_redstone_frame *frame, unsigned int pc, int32_t x, int32_t y,
                           int32_t z) {
    mc_redstone_at(frame, pc);
    mc_world_mutation_error *lower = mc_redstone_workspace(frame, pc);
    if (lower == NULL)
        return false;
    const bool complete =
        mc_world_mutation_neighbors(frame->world, x, y, z, frame->block->fields.bn, lower);
    return mc_redstone_child(frame, pc, lower, complete);
}

bool mc_redstone_strength(const mc_redstone_frame *caller, unsigned int pc, int32_t x, int32_t y,
                          int32_t z, int32_t maximum, int32_t *out) {
    mc_redstone_at(caller, pc);
    mc_redstone_frame frame = *caller;
    frame.method = MC_REDSTONE_STRENGTH;
    int32_t id, metadata;
    if (!mc_redstone_value(&frame, 5, x, y, z, false, &id))
        return mc_redstone_nested(caller, pc, frame.error);
    if (id == frame.block->fields.bn) {
        if (!mc_redstone_value(&frame, 23, x, y, z, true, &metadata))
            return mc_redstone_nested(caller, pc, frame.error);
        if (metadata > maximum)
            maximum = metadata;
    }
    *out = maximum;
    return mc_redstone_done(&frame);
}

void mc_redstone_horizontal(int32_t x, int32_t z, unsigned int side, int32_t *out_x,
                            int32_t *out_z) {
    *out_x = side == 0 ? mc_java_i32_sub(x, 1) : side == 1 ? mc_java_i32_add(x, 1) : x;
    *out_z = side == 2 ? mc_java_i32_sub(z, 1) : side == 3 ? mc_java_i32_add(z, 1) : z;
}
