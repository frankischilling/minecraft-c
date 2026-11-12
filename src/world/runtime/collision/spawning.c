#include "mc/world/runtime/collision/spawning.h"

#include "../runtime_internal.h"

static mc_world_storage_status fail(mc_world_spawn_collision_error *e,
                                    mc_world_storage_status status, unsigned int pc,
                                    const char *operation, bool native) {
    e->status = status;
    e->bytecode = pc;
    e->operation = operation;
    e->native_policy = native;
    e->source_exception = !native;
    e->exception_class = native ? NULL : "java/lang/NullPointerException";
    return status;
}

mc_world_storage_status
mc_world_runtime_no_entity_collision(mc_world_runtime *world, const mc_entity_bounds *bounds,
                                     bool *out, mc_world_spawn_collision_error *error) {
    mc_world_spawn_collision_error local = {0};
    mc_world_spawn_collision_error *e = error == NULL ? &local : error;
    *e = (mc_world_spawn_collision_error){0};
    if (world == NULL || out == NULL)
        return fail(e, MC_WORLD_STORAGE_INVALID_ARGUMENT, 0, "Spawn collision arguments", true);
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        return fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, 0, "Spawn collision World phase", true);
    mc_world_storage_error hold = {0};
    mc_world_storage_status status = mc_world_runtime_call_enter(world, &hold);
    if (status != MC_WORLD_STORAGE_OK)
        return fail(e, status, 0, hold.operation, true);
    mc_world_entity_list_view view = {0};
    e->bytecode = 3;
    status = mc_world_runtime_nearby_entities(world, NULL, bounds, &view, &e->nearby);
    if (status != MC_WORLD_STORAGE_OK) {
        const bool source_null = bounds == NULL && e->nearby.query.bytecode == 10;
        (void)fail(e, status, 3, e->nearby.query.operation, !source_null);
        goto done;
    }
    for (size_t i = 0;; ++i) {
        e->index = i;
        size_t count;
        status = mc_world_entity_list_view_count(&view, &count);
        if (status != MC_WORLD_STORAGE_OK) {
            (void)fail(e, status, 11, "Spawn collision List.size", view.identity != NULL);
            break;
        }
        if (i >= count) {
            *out = true;
            e->bytecode = 56;
            break;
        }
        mc_entity *candidate = NULL;
        status = mc_world_entity_list_view_get(&view, i, &candidate);
        if (status != MC_WORLD_STORAGE_OK) {
            (void)fail(e, status, 21, "Spawn collision List.get", true);
            break;
        }
        if (candidate == NULL) {
            status = fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, 33,
                          "Spawn collision null Entity receiver", false);
            break;
        }
        mc_entity_snapshot fields;
        const mc_entity_status read = mc_entity_get_snapshot(candidate, &fields);
        if (read != MC_ENTITY_OK) {
            status = fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, 33,
                          "Spawn collision Entity fields", true);
            break;
        }
        if (!fields.base.be && fields.base.aF) {
            *out = false;
            e->bytecode = 48;
            break;
        }
    }
    mc_world_entity_list_view_release(&view);
done:
    mc_world_runtime_call_leave(world);
    return status;
}
