#include "redstone_torch_internal.h"

#include "mc/world/runtime/scheduling.h"

#include <stdlib.h>

static bool notify_six(const mc_redstone_torch_frame *frame, int32_t x, int32_t y, int32_t z,
                       const unsigned int reads[6], const unsigned int calls[6]) {
    const int32_t xs[6] = {x, x, mc_java_i32_sub(x, 1), mc_java_i32_add(x, 1), x, x};
    const int32_t ys[6] = {mc_java_i32_sub(y, 1), mc_java_i32_add(y, 1), y, y, y, y};
    const int32_t zs[6] = {z, z, z, z, mc_java_i32_sub(z, 1), mc_java_i32_add(z, 1)};
    for (size_t i = 0; i < 6; ++i)
        if (!mc_redstone_torch_notify(frame, reads[i], calls[i], xs[i], ys[i], zs[i]))
            return false;
    return true;
}

bool mc_world_mutation_redstone_torch_added(mc_world_runtime *world, const mc_block *block,
                                            int32_t x, int32_t y, int32_t z,
                                            mc_world_mutation_error *error) {
    const mc_redstone_torch_frame frame = {world, block, error, MC_REDSTONE_TORCH_ADDED};
    int32_t metadata;
    if (!mc_redstone_torch_metadata(&frame, 5, x, y, z, &metadata))
        return false;
    if (metadata == 0) {
        mc_redstone_torch_at(&frame, 17);
        mc_world_mutation_error *lower = mc_redstone_torch_workspace(&frame, 17);
        if (lower == NULL)
            return false;
        if (!mc_redstone_torch_child(&frame, 17, lower,
                                     mc_world_mutation_torch_added(world, block, x, y, z, lower)))
            return false;
    }
    mc_redstone_torch_at(&frame, 21);
    if (block->subtype.db.a) {
        static const unsigned int reads[6] = {35, 49, 63, 77, 91, 105};
        static const unsigned int calls[6] = {38, 52, 66, 80, 94, 108};
        if (!notify_six(&frame, x, y, z, reads, calls))
            return false;
    }
    return mc_redstone_torch_done(&frame);
}

bool mc_world_mutation_redstone_torch_removed(mc_world_runtime *world, const mc_block *block,
                                              int32_t x, int32_t y, int32_t z,
                                              mc_world_mutation_error *error) {
    const mc_redstone_torch_frame frame = {world, block, error, MC_REDSTONE_TORCH_REMOVED};
    mc_redstone_torch_at(&frame, 1);
    if (block->subtype.db.a) {
        static const unsigned int reads[6] = {15, 29, 43, 57, 71, 85};
        static const unsigned int calls[6] = {18, 32, 46, 60, 74, 88};
        if (!notify_six(&frame, x, y, z, reads, calls))
            return false;
    }
    return mc_redstone_torch_done(&frame);
}

bool mc_world_mutation_redstone_torch_neighbor(mc_world_runtime *world, const mc_block *block,
                                               int32_t x, int32_t y, int32_t z, int32_t source_id,
                                               mc_world_mutation_error *error) {
    const mc_redstone_torch_frame frame = {world, block, error, MC_REDSTONE_TORCH_NEIGHBOR};
    mc_redstone_torch_at(&frame, 8);
    mc_world_mutation_error *lower = mc_redstone_torch_workspace(&frame, 8);
    if (lower == NULL)
        return false;
    if (!mc_redstone_torch_child(
            &frame, 8, lower,
            mc_world_mutation_torch_neighbor(world, block, x, y, z, source_id, lower)))
        return false;
    mc_redstone_torch_at(&frame, 17);
    const int32_t id = block->fields.bn;
    mc_redstone_torch_at(&frame, 21);
    const int32_t delay = 2; /* Complete DB.e()I: iconst2; ireturn1. */
    mc_redstone_torch_at(&frame, 24);
    if (world == NULL)
        return mc_redstone_torch_null(&frame, 24, "Redstone Torch actual null World schedule call");
    mc_world_schedule_error *scheduled = malloc(sizeof(*scheduled));
    if (scheduled == NULL)
        return mc_redstone_torch_fail(&frame, 24, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                      "Redstone Torch schedule diagnostic allocation");
    const mc_world_storage_status status =
        mc_world_runtime_schedule(world, x, y, z, id, delay, scheduled);
    if (status == MC_WORLD_STORAGE_OK) {
        free(scheduled);
        return mc_redstone_torch_done(&frame);
    }
    lower = &scheduled->tick;
    lower->schedule = (mc_world_schedule_prefix){.status = scheduled->status,
                                                 .operation = scheduled->operation,
                                                 .bytecode = scheduled->bytecode,
                                                 .presence_bytecode = scheduled->presence_bytecode,
                                                 .provider_bytecode = scheduled->provider_bytecode,
                                                 .cause = scheduled->cause,
                                                 .record = scheduled->record,
                                                 .membership = scheduled->membership,
                                                 .ordered = scheduled->ordered,
                                                 .registry = scheduled->registry};
    lower->status = scheduled->status;
    lower->operation = scheduled->operation;
    lower->cause = scheduled->cause;
    lower->registry = scheduled->registry;
    lower->schedule_bytecode = scheduled->bytecode;
    lower->record = scheduled->record;
    lower->membership = scheduled->membership;
    lower->ordered = scheduled->ordered;
    if (scheduled->presence_bytecode != 0)
        lower->presence_bytecode = scheduled->presence_bytecode;
    if (scheduled->provider_bytecode != 0)
        lower->provider_bytecode = scheduled->provider_bytecode;
    (void)mc_redstone_torch_nested(&frame, 24, lower);
    free(scheduled);
    return false;
}

bool mc_world_mutation_redstone_torch_powered(mc_world_runtime *world, const mc_block *block,
                                              int32_t x, int32_t y, int32_t z, bool *out,
                                              mc_world_mutation_error *error) {
    const mc_redstone_torch_frame frame = {world, block, error, MC_REDSTONE_TORCH_POWERED};
    int32_t metadata;
    if (!mc_redstone_torch_metadata(&frame, 5, x, y, z, &metadata))
        return false;
    /* Source compares5,3,4,1,2 in that order, invoking at most one directional
     * query. Its metadata local cannot change during a reached child call. */
    static const int32_t values[5] = {5, 3, 4, 1, 2};
    static const int32_t sides[5] = {0, 2, 3, 4, 5};
    static const unsigned int pcs[5] = {24, 46, 68, 90, 112};
    const int32_t xs[5] = {x, x, x, mc_java_i32_sub(x, 1), mc_java_i32_add(x, 1)};
    const int32_t ys[5] = {mc_java_i32_sub(y, 1), y, y, y, y};
    const int32_t zs[5] = {z, mc_java_i32_sub(z, 1), mc_java_i32_add(z, 1), z, z};
    bool value = false;
    for (size_t i = 0; i < 5; ++i)
        if (metadata == values[i]) {
            if (!mc_redstone_torch_power_at(&frame, pcs[i], xs[i], ys[i], zs[i], sides[i], &value))
                return false;
            break;
        }
    *out = value;
    return mc_redstone_torch_done(&frame);
}
