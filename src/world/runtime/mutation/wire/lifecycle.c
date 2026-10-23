#include "wire_internal.h"

bool mc_wire_notify(const mc_wire_frame *caller, unsigned int pc, int32_t x, int32_t y, int32_t z) {
    mc_wire_at(caller, pc);
    mc_wire_frame frame = *caller;
    frame.method = MC_WIRE_NOTIFY;
    int32_t id;
    if (!mc_wire_value(&frame, 5, x, y, z, false, &id))
        return mc_wire_nested(caller, pc, frame.error);
    if (id == frame.block->fields.bn &&
        (!mc_wire_neighbors(&frame, 25, x, y, z) ||
         !mc_wire_neighbors(&frame, 39, mc_java_i32_sub(x, 1), y, z) ||
         !mc_wire_neighbors(&frame, 53, mc_java_i32_add(x, 1), y, z) ||
         !mc_wire_neighbors(&frame, 67, x, y, mc_java_i32_sub(z, 1)) ||
         !mc_wire_neighbors(&frame, 81, x, y, mc_java_i32_add(z, 1)) ||
         !mc_wire_neighbors(&frame, 95, x, mc_java_i32_sub(y, 1), z) ||
         !mc_wire_neighbors(&frame, 109, x, mc_java_i32_add(y, 1), z)))
        return mc_wire_nested(caller, pc, frame.error);
    return mc_wire_done(&frame);
}

static bool vertical_neighbors(const mc_wire_frame *frame, int32_t x, int32_t y, int32_t z) {
    for (unsigned int side = 0; side < 4; ++side) {
        int32_t nx, nz;
        mc_wire_horizontal(x, z, side, &nx, &nz);
        if (!mc_wire_notify(frame, 62u + 11u * side, nx, y, nz))
            return false;
    }
    for (unsigned int side = 0; side < 4; ++side) {
        int32_t nx, nz;
        mc_wire_horizontal(x, z, side, &nx, &nz);
        bool solid;
        if (!mc_wire_solid(frame, 105u + 42u * side, nx, y, nz, &solid))
            return false;
        const int32_t ny = solid ? mc_java_i32_add(y, 1) : mc_java_i32_sub(y, 1);
        if (!mc_wire_notify(frame, (solid ? 121u : 137u) + 42u * side, nx, ny, nz))
            return false;
    }
    return true;
}

static bool lifecycle(mc_world_runtime *world, const mc_block *block, int32_t x, int32_t y,
                      int32_t z, bool added, mc_world_mutation_error *error) {
    const mc_wire_frame frame =
        mc_wire_frame_make(world, block, error, added ? MC_WIRE_ADDED : MC_WIRE_REMOVED);
    /* invokespecial6 selects the proven empty uu/na base body, not virtual
     * redispatch. Its return precedes the actual remote field read at10. */
    mc_wire_at(&frame, 6);
    bool remote;
    if (!mc_wire_remote(&frame, 10, &remote))
        return false;
    if (remote)
        return mc_wire_done(&frame);
    if (added && !mc_wire_drain(&frame, 23, x, y, z))
        return false;
    if (!mc_wire_neighbors(&frame, added ? 37u : 28u, x, mc_java_i32_add(y, 1), z) ||
        !mc_wire_neighbors(&frame, added ? 51u : 42u, x, mc_java_i32_sub(y, 1), z))
        return false;
    if (!added && !mc_wire_drain(&frame, 51, x, y, z))
        return false;
    if (!vertical_neighbors(&frame, x, y, z))
        return false;
    return mc_wire_done(&frame);
}

bool mc_world_mutation_wire_added(mc_world_runtime *world, const mc_block *block, int32_t x,
                                  int32_t y, int32_t z, mc_world_mutation_error *error) {
    return lifecycle(world, block, x, y, z, true, error);
}

bool mc_world_mutation_wire_removed(mc_world_runtime *world, const mc_block *block, int32_t x,
                                    int32_t y, int32_t z, mc_world_mutation_error *error) {
    return lifecycle(world, block, x, y, z, false, error);
}

bool mc_world_mutation_wire_neighbor(mc_world_runtime *world, const mc_block *block, int32_t x,
                                     int32_t y, int32_t z, int32_t source_id,
                                     mc_world_mutation_error *error) {
    (void)source_id;
    const mc_wire_frame frame = mc_wire_frame_make(world, block, error, MC_WIRE_NEIGHBOR);
    bool remote;
    if (!mc_wire_remote(&frame, 1, &remote))
        return false;
    if (remote)
        return mc_wire_done(&frame);
    int32_t metadata;
    if (!mc_wire_value(&frame, 13, x, y, z, true, &metadata))
        return false;
    mc_wire_frame support = frame;
    support.method = MC_WIRE_SUPPORT;
    bool supported;
    mc_wire_at(&frame, 24);
    if (!mc_wire_solid(&support, 7, x, mc_java_i32_sub(y, 1), z, &supported))
        return mc_wire_nested(&frame, 24, error);
    if (!supported) {
        mc_wire_at(&frame, 42);
        mc_world_mutation_error *lower = mc_wire_workspace(&frame, 42);
        if (lower == NULL)
            return false;
        const bool dropped = mc_world_mutation_drop(world, block, x, y, z, metadata, lower);
        if (!mc_wire_child(&frame, 42, lower, dropped))
            return false;
        mc_wire_at(&frame, 51);
        lower = mc_wire_workspace(&frame, 51);
        if (lower == NULL)
            return false;
        bool ignored;
        const bool complete =
            mc_world_runtime_block_set(world, x, y, z, 0, &ignored, lower) == MC_WORLD_STORAGE_OK;
        if (!mc_wire_child(&frame, 51, lower, complete))
            return false;
    } else if (!mc_wire_drain(&frame, 64, x, y, z))
        return false;
    /* invokespecial75 reaches the empty inherited neighbor body. */
    mc_wire_at(&frame, 75);
    return mc_wire_done(&frame);
}
