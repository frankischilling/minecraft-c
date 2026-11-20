#include "redstone_internal.h"

#include "../../power/power_internal.h"

/* Each nested source call owns an independent diagnostic workspace. The
 * receiver, coordinates and local strengths remain in their actual caller. */
static bool set_metadata(const mc_redstone_frame *frame, int32_t x, int32_t y, int32_t z,
                         int32_t metadata) {
    mc_redstone_at(frame, 285);
    mc_world_mutation_error *lower = mc_redstone_workspace(frame, 285);
    if (lower == NULL)
        return false;
    const bool complete = mc_world_runtime_metadata_set(frame->world, x, y, z, metadata, lower) ==
                          MC_WORLD_STORAGE_OK;
    return mc_redstone_child(frame, 285, lower, complete);
}

static bool observe(const mc_redstone_frame *frame, int32_t x, int32_t y, int32_t z) {
    mc_redstone_at(frame, 297);
    mc_world_mutation_error *lower = mc_redstone_workspace(frame, 297);
    if (lower == NULL)
        return false;
    const bool complete = mc_world_mutation_observe_range(frame->world, x, y, z, x, y, z, lower);
    return mc_redstone_child(frame, 297, lower, complete);
}

static bool add_position(const mc_redstone_frame *frame, unsigned int pc, int32_t x, int32_t y,
                         int32_t z) {
    mc_redstone_at(frame, pc);
    bool ignored;
    /* The bounded collection operation constructs this attempt's fresh key,
     * even for duplicates. Its resource policy is separate from Java OOME. */
    return mc_redstone_collection(
        frame, pc,
        mc_registry_redstone_set_add_fresh(frame->block->subtype.sm.b, x, y, z, &ignored));
}

static bool update(mc_redstone_frame *frame, int32_t x, int32_t y, int32_t z, int32_t incoming_x,
                   int32_t incoming_y, int32_t incoming_z) {
    int32_t previous, strength = 0;
    if (!mc_redstone_value(frame, 5, x, y, z, true, &previous))
        return false;
    frame->block->subtype.sm.a = false; /* putfield15 has no source finally. */
    bool powered;
    mc_redstone_at(frame, 23);
    if (mc_world_power_neighbors_held(frame->world, x, y, z, &powered, &frame->error->power) !=
        MC_WORLD_STORAGE_OK) {
        frame->error->registry = frame->error->power.registry;
        frame->error->query = frame->error->power.access.query;
        frame->error->population = frame->error->power.access.query.population;
        const mc_world_storage_error cause = frame->error->power.cause;
        frame->error->status = cause.status;
        frame->error->operation = cause.operation;
        frame->error->cause = cause;
        if (frame->world != NULL)
            frame->world->last_error = cause;
        return false;
    }
    frame->block->subtype.sm.a = true; /* Unconditional source store30. */
    if (powered)
        strength = 15;
    else {
        for (unsigned int side = 0; side < 4; ++side) {
            int32_t nx, nz;
            mc_redstone_horizontal(x, z, side, &nx, &nz);
            if ((nx != incoming_x || y != incoming_y || nz != incoming_z) &&
                !mc_redstone_strength(frame, 125, nx, y, nz, strength, &strength))
                return false;
            bool solid, overhead = true;
            if (!mc_redstone_solid(frame, 136, nx, y, nz, &solid))
                return false;
            const int32_t above = mc_java_i32_add(y, 1);
            if (solid && !mc_redstone_solid(frame, 149, x, above, z, &overhead))
                return false;
            if (solid && !overhead) {
                if ((nx != incoming_x || above != incoming_y || nz != incoming_z) &&
                    !mc_redstone_strength(frame, 188, nx, above, nz, strength, &strength))
                    return false;
            } else {
                /* PC202 repeats the support query, including when the earlier
                 * query was true but the center's overhead was also solid. */
                if (!mc_redstone_solid(frame, 202, nx, y, nz, &solid))
                    return false;
                const int32_t below = mc_java_i32_sub(y, 1);
                if (!solid && (nx != incoming_x || below != incoming_y || nz != incoming_z) &&
                    !mc_redstone_strength(frame, 241, nx, below, nz, strength, &strength))
                    return false;
            }
        }
        strength = strength > 0 ? mc_java_i32_sub(strength, 1) : 0;
    }
    if (previous == strength)
        return mc_redstone_done(frame);

    frame->world->fields.o = true; /* putfield275; failure keeps this value. */
    if (!set_metadata(frame, x, y, z, strength) || !observe(frame, x, y, z))
        return false;
    frame->world->fields.o = false;
    for (unsigned int side = 0; side < 4; ++side) {
        int32_t nx, nz;
        mc_redstone_horizontal(x, z, side, &nx, &nz);
        int32_t alternate_y = mc_java_i32_sub(y, 1), neighbor;
        bool solid;
        if (!mc_redstone_solid(frame, 367, nx, y, nz, &solid))
            return false;
        if (solid)
            alternate_y = mc_java_i32_add(alternate_y, 2);
        if (!mc_redstone_strength(frame, 387, nx, y, nz, -1, &neighbor) ||
            !mc_redstone_value(frame, 397, x, y, z, true, &strength))
            return false;
        if (strength > 0)
            strength = mc_java_i32_sub(strength, 1);
        if (neighbor >= 0 && neighbor != strength &&
            !mc_redstone_update(frame, 433, nx, y, nz, x, y, z))
            return false;
        if (!mc_redstone_strength(frame, 445, nx, alternate_y, nz, -1, &neighbor) ||
            !mc_redstone_value(frame, 455, x, y, z, true, &strength))
            return false;
        if (strength > 0)
            strength = mc_java_i32_sub(strength, 1);
        if (neighbor >= 0 && neighbor != strength &&
            !mc_redstone_update(frame, 492, nx, alternate_y, nz, x, y, z))
            return false;
    }
    /* The final local is the last center reread minus one, not the value
     * originally written at285. Recursion may also have changed the center. */
    if (previous == 0 || strength == 0) {
        if (!add_position(frame, 526, x, y, z) ||
            !add_position(frame, 549, mc_java_i32_sub(x, 1), y, z) ||
            !add_position(frame, 572, mc_java_i32_add(x, 1), y, z) ||
            !add_position(frame, 595, x, mc_java_i32_sub(y, 1), z) ||
            !add_position(frame, 618, x, mc_java_i32_add(y, 1), z) ||
            !add_position(frame, 641, x, y, mc_java_i32_sub(z, 1)) ||
            !add_position(frame, 664, x, y, mc_java_i32_add(z, 1)))
            return false;
    }
    return mc_redstone_done(frame);
}

bool mc_redstone_update(const mc_redstone_frame *caller, unsigned int pc, int32_t x, int32_t y,
                        int32_t z, int32_t incoming_x, int32_t incoming_y, int32_t incoming_z) {
    mc_redstone_at(caller, pc);
    mc_redstone_frame frame = *caller;
    frame.method = MC_REDSTONE_UPDATE;
    if (!update(&frame, x, y, z, incoming_x, incoming_y, incoming_z))
        return mc_redstone_nested(caller, pc, frame.error);
    return true;
}

bool mc_redstone_drain(const mc_redstone_frame *caller, unsigned int pc, int32_t x, int32_t y,
                       int32_t z) {
    mc_redstone_at(caller, pc);
    mc_redstone_frame frame = *caller;
    frame.method = MC_REDSTONE_DRAIN;
    if (!mc_redstone_update(&frame, 10, x, y, z, x, y, z))
        return mc_redstone_nested(caller, pc, frame.error);
    mc_registry_redstone_snapshot *snapshot = NULL;
    mc_redstone_at(&frame, 21);
    if (!mc_redstone_collection(
            &frame, 21, mc_registry_redstone_snapshot_copy(frame.block->subtype.sm.b, &snapshot)))
        return mc_redstone_nested(caller, pc, frame.error);
    bool complete = mc_redstone_collection(
        &frame, 30, mc_registry_redstone_set_clear(frame.block->subtype.sm.b));
    for (size_t index = 0; complete && index < mc_registry_redstone_snapshot_size(snapshot);
         ++index) {
        mc_registry_redstone_position key;
        complete = mc_redstone_collection(&frame, 54,
                                          mc_registry_redstone_snapshot_at(snapshot, index, &key));
        if (complete)
            complete = mc_redstone_neighbors(&frame, 84, key.x, key.y, key.z);
    }
    mc_registry_redstone_snapshot_release(snapshot);
    if (!complete)
        return mc_redstone_nested(caller, pc, frame.error);
    return mc_redstone_done(&frame);
}
