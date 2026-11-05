#include "redstone_torch_internal.h"

#include "../../collision/blocks_internal.h"
#include "../../power/power_internal.h"

#include <stdlib.h>

void mc_redstone_torch_at(const mc_redstone_torch_frame *frame, unsigned int pc) {
    mc_world_mutation_error *error = frame->error;
    error->hook_class = "db";
    error->hook_bytecode = pc;
    switch (frame->method) {
        case MC_REDSTONE_TORCH_ADDED:
            error->hook_method = "c(Lfd;III)V";
            error->redstone_torch.added = pc;
            break;
        case MC_REDSTONE_TORCH_REMOVED:
            error->hook_method = "b(Lfd;III)V";
            error->redstone_torch.removed = pc;
            break;
        case MC_REDSTONE_TORCH_NEIGHBOR:
            error->hook_method = "b(Lfd;IIII)V";
            error->redstone_torch.neighbor = pc;
            break;
        case MC_REDSTONE_TORCH_TICK:
            error->hook_method = "a(Lfd;IIILjava/util/Random;)V";
            error->redstone_torch.tick = pc;
            break;
        case MC_REDSTONE_TORCH_POWERED:
            error->hook_method = "h(Lfd;III)Z";
            error->redstone_torch.powered = pc;
            break;
        case MC_REDSTONE_TORCH_BURNOUT:
            error->hook_method = "a(Lfd;IIIZ)Z";
            error->redstone_torch.burnout = pc;
            break;
    }
}

bool mc_redstone_torch_done(const mc_redstone_torch_frame *frame) {
    frame->error->hook_class = frame->error->hook_method = NULL;
    frame->error->hook_bytecode = 0;
    frame->error->redstone_torch = (mc_world_redstone_torch_sites){0};
    return true;
}

bool mc_redstone_torch_fail(const mc_redstone_torch_frame *frame, unsigned int pc,
                            mc_world_storage_status status, const char *operation) {
    mc_redstone_torch_at(frame, pc);
    return mc_world_mutation_fail(frame->world, frame->error, status, operation);
}

bool mc_redstone_torch_null(const mc_redstone_torch_frame *frame, unsigned int pc,
                            const char *operation) {
    frame->error->redstone_torch_source_null = true;
    return mc_redstone_torch_fail(frame, pc, MC_WORLD_STORAGE_UNSUPPORTED_STATE, operation);
}

bool mc_redstone_torch_storage(const mc_redstone_torch_frame *frame, unsigned int pc,
                               const mc_world_storage_error *cause) {
    mc_redstone_torch_at(frame, pc);
    frame->error->status = cause->status;
    frame->error->operation = cause->operation;
    frame->error->cause = *cause;
    if (frame->world != NULL)
        frame->world->last_error = *cause;
    return false;
}

bool mc_redstone_torch_nested(const mc_redstone_torch_frame *frame, unsigned int pc,
                              const mc_world_mutation_error *lower) {
    const mc_world_redstone_torch_sites saved = frame->error->redstone_torch;
    if (frame->error != lower)
        *frame->error = *lower;
#define KEEP(field)                                                                                \
    if (saved.field != 0)                                                                          \
    frame->error->redstone_torch.field = saved.field
    KEEP(added);
    KEEP(removed);
    KEEP(neighbor);
    KEEP(tick);
    KEEP(powered);
    KEEP(burnout);
#undef KEEP
    if (frame->error->cause_hook_class == NULL) {
        frame->error->cause_hook_class = frame->error->hook_class;
        frame->error->cause_hook_method = frame->error->hook_method;
        frame->error->cause_hook_bytecode = frame->error->hook_bytecode;
    }
    mc_redstone_torch_at(frame, pc);
    if (frame->world != NULL)
        frame->world->last_error = frame->error->cause;
    return false;
}

mc_world_mutation_error *mc_redstone_torch_workspace(const mc_redstone_torch_frame *frame,
                                                     unsigned int pc) {
    mc_world_mutation_error *lower = malloc(sizeof(*lower));
    if (lower == NULL)
        (void)mc_redstone_torch_fail(frame, pc, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                     "Redstone Torch child diagnostic allocation");
    else
        mc_world_mutation_error_init(lower);
    return lower;
}

bool mc_redstone_torch_child(const mc_redstone_torch_frame *frame, unsigned int pc,
                             mc_world_mutation_error *lower, bool complete) {
    if (!complete)
        (void)mc_redstone_torch_nested(frame, pc, lower);
    free(lower);
    return complete;
}

bool mc_redstone_torch_metadata(const mc_redstone_torch_frame *frame, unsigned int pc, int32_t x,
                                int32_t y, int32_t z, int32_t *out) {
    mc_redstone_torch_at(frame, pc);
    if (frame->world == NULL)
        return mc_redstone_torch_null(frame, pc, "Redstone Torch actual null World metadata call");
    mc_world_collision_error *lower = malloc(sizeof(*lower));
    if (lower == NULL)
        return mc_redstone_torch_fail(frame, pc, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                      "Redstone Torch value diagnostic allocation");
    *lower = mc_world_collision_empty_error();
    const mc_world_storage_status status =
        mc_world_collision_block_value_held(frame->world, x, y, z, true, out, lower);
    if (status != MC_WORLD_STORAGE_OK) {
        frame->error->nearby = lower->nearby;
        frame->error->query = lower->nearby.query;
        frame->error->registry = lower->nearby.query.registry;
        frame->error->population = lower->nearby.query.population;
        frame->error->entity = lower->nearby.entity;
        frame->error->tile_entity = lower->nearby.tile_entity;
        frame->error->batch = lower->nearby.batch;
        frame->error->load_entity = lower->nearby.load_entity;
        frame->error->activation_bytecode = lower->nearby.activation_bytecode;
        frame->error->activation_world_bytecode = lower->nearby.activation_world_bytecode;
        frame->error->load_bytecode = lower->nearby.load_bytecode;
        frame->error->load_builder_bytecode = lower->nearby.load_builder_bytecode;
        (void)mc_redstone_torch_storage(frame, pc, &lower->nearby.query.cause);
    }
    free(lower);
    return status == MC_WORLD_STORAGE_OK;
}

bool mc_redstone_torch_power_at(const mc_redstone_torch_frame *frame, unsigned int pc, int32_t x,
                                int32_t y, int32_t z, int32_t side, bool *out) {
    mc_redstone_torch_at(frame, pc);
    if (frame->world == NULL)
        return mc_redstone_torch_null(frame, pc, "Redstone Torch actual null World power call");
    mc_world_power_error *lower = malloc(sizeof(*lower));
    if (lower == NULL)
        return mc_redstone_torch_fail(frame, pc, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                      "Redstone Torch power diagnostic allocation");
    mc_world_power_error_init(lower);
    const mc_world_storage_status status =
        mc_world_power_at_held(frame->world, x, y, z, side, out, lower);
    if (status != MC_WORLD_STORAGE_OK) {
        frame->error->power = *lower;
        frame->error->registry = lower->registry;
        (void)mc_redstone_torch_storage(frame, pc, &lower->cause);
    }
    free(lower);
    return status == MC_WORLD_STORAGE_OK;
}

bool mc_redstone_torch_notify(const mc_redstone_torch_frame *frame, unsigned int id_pc,
                              unsigned int call_pc, int32_t x, int32_t y, int32_t z) {
    mc_redstone_torch_at(frame, id_pc);
    const int32_t id = frame->block->fields.bn;
    mc_redstone_torch_at(frame, call_pc);
    if (frame->world == NULL)
        return mc_redstone_torch_null(frame, call_pc,
                                      "Redstone Torch actual null World notify call");
    mc_world_mutation_error *lower = mc_redstone_torch_workspace(frame, call_pc);
    if (lower == NULL)
        return false;
    return mc_redstone_torch_child(frame, call_pc, lower,
                                   mc_world_mutation_neighbors(frame->world, x, y, z, id, lower));
}

bool mc_redstone_torch_registry(const mc_redstone_torch_frame *frame, unsigned int pc,
                                mc_registry_status status, const mc_registry_error *cause,
                                const char *operation) {
    mc_redstone_torch_at(frame, pc);
    frame->error->registry = *cause;
    if (frame->world != NULL) {
        frame->error->status = mc_world_runtime_registry_result(frame->world, status, cause,
                                                                operation, &frame->error->cause);
        frame->error->operation = frame->error->cause.operation;
    } else {
        frame->error->status = mc_world_runtime_registry_status(status);
        frame->error->operation = operation;
        frame->error->cause = (mc_world_storage_error){frame->error->status, operation, 0};
    }
    return false;
}

bool mc_redstone_torch_collection(const mc_redstone_torch_frame *frame, unsigned int pc,
                                  const mc_registry_redstone_torch_error *cause,
                                  const char *operation) {
    if (cause->status == MC_REGISTRY_OK)
        return true;
    frame->error->redstone_torch_collection_status = cause->status;
    frame->error->redstone_torch_mod_count_stored = cause->mod_count_stored;
    switch (cause->failure) {
        case MC_REGISTRY_REDSTONE_TORCH_NONE:
            frame->error->redstone_torch_collection_failure =
                MC_WORLD_REDSTONE_TORCH_COLLECTION_NONE;
            break;
        case MC_REGISTRY_REDSTONE_TORCH_LIST_NULL:
            frame->error->redstone_torch_collection_failure =
                MC_WORLD_REDSTONE_TORCH_COLLECTION_NULL;
            break;
        case MC_REGISTRY_REDSTONE_TORCH_LIST_INDEX:
            frame->error->redstone_torch_collection_failure =
                MC_WORLD_REDSTONE_TORCH_COLLECTION_INDEX;
            break;
        case MC_REGISTRY_REDSTONE_TORCH_NATIVE:
            frame->error->redstone_torch_collection_failure =
                MC_WORLD_REDSTONE_TORCH_COLLECTION_NATIVE;
            break;
    }
    return mc_redstone_torch_fail(frame, pc, mc_world_runtime_registry_status(cause->status),
                                  operation);
}

bool mc_redstone_torch_list(const mc_redstone_torch_frame *frame, unsigned int pc,
                            mc_registry_redstone_torch_list_ref *out) {
    mc_redstone_torch_at(frame, pc);
    const mc_registry_status status =
        mc_registry_redstone_torch_list_capture(frame->block->classes, out);
    if (status == MC_REGISTRY_OK)
        return true;
    return mc_redstone_torch_fail(frame, pc, mc_world_runtime_registry_status(status),
                                  "Redstone Torch actual static List capture");
}

bool mc_redstone_torch_time(const mc_redstone_torch_frame *frame, unsigned int pc, int64_t *out) {
    mc_redstone_torch_at(frame, pc);
    if (frame->world == NULL)
        return mc_redstone_torch_null(frame, pc, "Redstone Torch actual null World time call");
    const mc_world_info_status status = mc_world_info_time_get(frame->world->fields.x, out);
    if (status == MC_WORLD_INFO_OK)
        return true;
    frame->error->redstone_torch_info = status;
    frame->error->cause_hook_class = "fd";
    frame->error->cause_hook_method = "t()J";
    frame->error->cause_hook_bytecode = 4;
    if (frame->world->fields.x == NULL)
        return mc_redstone_torch_null(frame, pc, "Redstone Torch actual null WorldInfo time call");
    return mc_redstone_torch_fail(frame, pc, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                  "Redstone Torch reached WorldInfo time getter");
}
