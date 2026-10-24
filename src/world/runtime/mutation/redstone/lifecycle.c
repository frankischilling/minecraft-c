#include "redstone_internal.h"

bool mc_redstone_notify(const mc_redstone_frame *caller, unsigned int pc, int32_t x, int32_t y,
                        int32_t z) {
    mc_redstone_at(caller, pc);
    mc_redstone_frame frame = *caller;
    frame.method = MC_REDSTONE_NOTIFY;
    int32_t id;
    if (!mc_redstone_value(&frame, 5, x, y, z, false, &id))
        return mc_redstone_nested(caller, pc, frame.error);
    if (id == frame.block->fields.bn &&
        (!mc_redstone_neighbors(&frame, 25, x, y, z) ||
         !mc_redstone_neighbors(&frame, 39, mc_java_i32_sub(x, 1), y, z) ||
         !mc_redstone_neighbors(&frame, 53, mc_java_i32_add(x, 1), y, z) ||
         !mc_redstone_neighbors(&frame, 67, x, y, mc_java_i32_sub(z, 1)) ||
         !mc_redstone_neighbors(&frame, 81, x, y, mc_java_i32_add(z, 1)) ||
         !mc_redstone_neighbors(&frame, 95, x, mc_java_i32_sub(y, 1), z) ||
         !mc_redstone_neighbors(&frame, 109, x, mc_java_i32_add(y, 1), z)))
        return mc_redstone_nested(caller, pc, frame.error);
    return mc_redstone_done(&frame);
}

static bool vertical_neighbors(const mc_redstone_frame *frame, int32_t x, int32_t y, int32_t z) {
    for (unsigned int side = 0; side < 4; ++side) {
        int32_t nx, nz;
        mc_redstone_horizontal(x, z, side, &nx, &nz);
        if (!mc_redstone_notify(frame, 62u + 11u * side, nx, y, nz))
            return false;
    }
    for (unsigned int side = 0; side < 4; ++side) {
        int32_t nx, nz;
        mc_redstone_horizontal(x, z, side, &nx, &nz);
        bool solid;
        if (!mc_redstone_solid(frame, 105u + 42u * side, nx, y, nz, &solid))
            return false;
        const int32_t ny = solid ? mc_java_i32_add(y, 1) : mc_java_i32_sub(y, 1);
        if (!mc_redstone_notify(frame, (solid ? 121u : 137u) + 42u * side, nx, ny, nz))
            return false;
    }
    return true;
}

static bool lifecycle(mc_world_runtime *world, const mc_block *block, int32_t x, int32_t y,
                      int32_t z, bool added, mc_world_mutation_error *error) {
    const mc_redstone_frame frame = mc_redstone_frame_make(
        world, block, error, added ? MC_REDSTONE_ADDED : MC_REDSTONE_REMOVED);
    /* invokespecial6 selects the proven empty uu/na base body, not virtual
     * redispatch. Its return precedes the actual remote field read at10. */
    mc_redstone_at(&frame, 6);
    bool remote;
    if (!mc_redstone_remote(&frame, 10, &remote))
        return false;
    if (remote)
        return mc_redstone_done(&frame);
    if (added && !mc_redstone_drain(&frame, 23, x, y, z))
        return false;
    if (!mc_redstone_neighbors(&frame, added ? 37u : 28u, x, mc_java_i32_add(y, 1), z) ||
        !mc_redstone_neighbors(&frame, added ? 51u : 42u, x, mc_java_i32_sub(y, 1), z))
        return false;
    if (!added && !mc_redstone_drain(&frame, 51, x, y, z))
        return false;
    if (!vertical_neighbors(&frame, x, y, z))
        return false;
    return mc_redstone_done(&frame);
}

bool mc_world_mutation_redstone_added(mc_world_runtime *world, const mc_block *block, int32_t x,
                                      int32_t y, int32_t z, mc_world_mutation_error *error) {
    return lifecycle(world, block, x, y, z, true, error);
}

bool mc_world_mutation_redstone_removed(mc_world_runtime *world, const mc_block *block, int32_t x,
                                        int32_t y, int32_t z, mc_world_mutation_error *error) {
    return lifecycle(world, block, x, y, z, false, error);
}

bool mc_world_mutation_redstone_neighbor(mc_world_runtime *world, const mc_block *block, int32_t x,
                                         int32_t y, int32_t z, int32_t source_id,
                                         mc_world_mutation_error *error) {
    (void)source_id;
    const mc_redstone_frame frame =
        mc_redstone_frame_make(world, block, error, MC_REDSTONE_NEIGHBOR);
    bool remote;
    if (!mc_redstone_remote(&frame, 1, &remote))
        return false;
    if (remote)
        return mc_redstone_done(&frame);
    int32_t metadata;
    if (!mc_redstone_value(&frame, 13, x, y, z, true, &metadata))
        return false;
    mc_redstone_frame support = frame;
    support.method = MC_REDSTONE_SUPPORT;
    bool supported;
    mc_redstone_at(&frame, 24);
    if (!mc_redstone_solid(&support, 7, x, mc_java_i32_sub(y, 1), z, &supported))
        return mc_redstone_nested(&frame, 24, error);
    if (!supported) {
        mc_redstone_at(&frame, 42);
        mc_world_mutation_error *lower = mc_redstone_workspace(&frame, 42);
        if (lower == NULL)
            return false;
        const bool dropped = mc_world_mutation_drop(world, block, x, y, z, metadata, lower);
        if (!mc_redstone_child(&frame, 42, lower, dropped))
            return false;
        mc_redstone_at(&frame, 51);
        lower = mc_redstone_workspace(&frame, 51);
        if (lower == NULL)
            return false;
        bool ignored;
        const bool complete =
            mc_world_runtime_block_set(world, x, y, z, 0, &ignored, lower) == MC_WORLD_STORAGE_OK;
        if (!mc_redstone_child(&frame, 51, lower, complete))
            return false;
    } else if (!mc_redstone_drain(&frame, 64, x, y, z))
        return false;
    /* invokespecial75 reaches the empty inherited neighbor body. */
    mc_redstone_at(&frame, 75);
    return mc_redstone_done(&frame);
}
