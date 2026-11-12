#include "../../../world/path/finder_internal.h"
#include "../references_internal.h"
#include "internal.h"

#include <string.h>

static mc_entity_status path_status(mc_path_status status) {
    return status == MC_PATH_LIMIT        ? MC_ENTITY_LIMIT
           : status == MC_PATH_ALLOCATION ? MC_ENTITY_ALLOCATION_FAILED
           : status == MC_PATH_BUSY       ? MC_ENTITY_BUSY
                                          : MC_ENTITY_STATE_ERROR;
}

static mc_entity_status world_status(mc_world_path_status status) {
    return status == MC_WORLD_PATH_LIMIT        ? MC_ENTITY_LIMIT
           : status == MC_WORLD_PATH_ALLOCATION ? MC_ENTITY_ALLOCATION_FAILED
           : status == MC_WORLD_PATH_BUSY       ? MC_ENTITY_BUSY
                                                : MC_ENTITY_WORLD_FAILED;
}

static void flatten_cache(mc_mob_tick_path_error *out, mc_client_chunk_cache_error *cache) {
    if (cache->acquisition.source_chunk != NULL) {
        out->has_source_chunk = true;
        out->source_chunk = *cache->acquisition.source_chunk;
        cache->acquisition.source_chunk = NULL;
    }
}

static void flatten(mc_mob_tick_path_error *out, const mc_world_path_error *source) {
    memset(out, 0, sizeof(*out));
    out->kind = MC_MOB_TICK_PATH_CHILD_INVOCATION;
    if (source == NULL)
        return;
    out->cause = *source;
    if (source->world != NULL) {
        out->has_world = true;
        out->world = *source->world;
    }
    if (source->entity != NULL) {
        out->has_entity = true;
        out->entity = *source->entity;
    }
    if (source->math != NULL) {
        out->has_math = true;
        out->math = *source->math;
    }
    if (source->registry != NULL) {
        out->has_registry = true;
        out->registry = *source->registry;
    }
    if (source->cache != NULL) {
        out->has_cache = true;
        out->cache = *source->cache;
        flatten_cache(out, &out->cache);
    }
    if (source->finder != NULL) {
        out->has_finder = true;
        out->finder_path = source->finder->path;
        out->finder_registry = source->finder->registry;
        out->finder_cache = source->finder->cache;
        flatten_cache(out, &out->finder_cache);
    }
    if (source->source_chunk != NULL) {
        out->has_source_chunk = true;
        out->source_chunk = *source->source_chunk;
    }
    out->cause.world = NULL;
    out->cause.entity = NULL;
    out->cause.math = NULL;
    out->cause.registry = NULL;
    out->cause.cache = NULL;
    out->cause.finder = NULL;
    out->cause.source_chunk = NULL;
}

bool mt_path_failed(mc_mob_tick_scope *s, const mc_path_error *error, const char *operation) {
    s->error->child = MC_MOB_TICK_CHILD_PATH;
    memset(&s->error->detail.path, 0, sizeof(s->error->detail.path));
    s->error->detail.path.kind = MC_MOB_TICK_PATH_CHILD_PATH;
    s->error->detail.path.path = *error;
    mc_entity_status status = path_status(error->status);
    if (error->status == MC_PATH_CHILD_FAILED) {
        const mc_entity_status child =
            error->vector.status != MC_ENTITY_OK ? error->vector.status : error->entity.status;
        if (child == MC_ENTITY_LIMIT || child == MC_ENTITY_ALLOCATION_FAILED ||
            child == MC_ENTITY_BUSY)
            status = child;
    }
    (void)mt_fail(s, status, operation);
    s->error->native_policy = error->native_policy;
    s->error->source_exception = error->source_exception;
    s->error->exception_class = error->exception_class;
    s->error->child_site = (mc_mob_site){error->site.class_name, error->site.method,
                                         error->site.descriptor, error->site.pc};
    return false;
}

bool mt_path_get(mc_mob_tick_scope *s, mc_path **out) {
    mc_path_error error;
    if (mc_mob_path_get(s->entity, out, &error) == MC_ENTITY_OK)
        return true;
    return mt_path_failed(s, &error, "Creature actual Path field occurrence");
}

bool mt_path_store(mc_mob_tick_scope *s, mc_path *value) {
    mc_path_error error;
    if (mc_mob_path_store(s->entity, value, &error) == MC_ENTITY_OK)
        return true;
    /* These callers execute direct II putfield instructions, not II.a(DH). */
    error.site = (mc_path_site){s->error->site.class_name, s->error->site.method,
                                s->error->site.descriptor, s->error->site.pc};
    return mt_path_failed(s, &error, "Creature actual Path field store");
}

bool mt_path_present(mc_mob_tick_scope *s, bool *out) {
    mc_path_error error;
    if (mc_mob_path_present_entered(s->entity, out, &error) == MC_ENTITY_OK)
        return true;
    return mt_path_failed(s, &error, "Creature actual Path presence");
}

static bool retire_child(mc_world_path_call **slot, mc_mob_tick_error *error) {
    const mc_world_path_status status = mc_world_path_call_retire(slot);
    if (status == MC_WORLD_PATH_OK)
        return true;
    mc_world_path_call_state state = {0};
    if (mc_world_path_call_inspect(*slot, &state))
        flatten(&error->cleanup_path, state.cleanup);
    if (error->cleanup_path.cause.status == MC_WORLD_PATH_OK) {
        error->cleanup_path.cause.status = status;
        error->cleanup_path.cause.native_policy = true;
        error->cleanup_path.cause.operation = "Mob owned World path native retirement";
    }
    if (error->status == MC_ENTITY_OK) {
        error->status = world_status(status);
        error->native_policy = true;
        error->operation = "Mob owned World path native retirement";
    }
    error->state.cleanup_pending = true;
    return false;
}

bool mt_path_request(mc_mob_tick_scope *s, mc_world_runtime *world, mc_entity *target,
                     bool to_entity, int32_t x, int32_t y, int32_t z, float range,
                     unsigned int store_pc) {
    s->error->state.path_returned = false;
    s->error->state.path_store_completed = false;
    if (world == NULL)
        return mt_null(s, "Creature path null captured World receiver");
    mc_world_path_call *call = NULL;
    ++s->error->state.path_calls;
    const mc_world_path_status status =
        to_entity ? mc_world_runtime_path_to_entity_begin(world, s->entity, target, range, s->math,
                                                          s->path_limits, &call)
                  : mc_world_runtime_path_to_coordinates_begin(world, s->entity, x, y, z, range,
                                                               s->math, s->path_limits, &call);
    mc_world_path_call_state state = {0};
    const bool inspected = call != NULL && mc_world_path_call_inspect(call, &state);
    bool ok = false;
    if (status != MC_WORLD_PATH_OK) {
        s->error->child = MC_MOB_TICK_CHILD_PATH;
        flatten(&s->error->detail.path, inspected ? state.failure : NULL);
        if (!inspected) {
            s->error->detail.path.cause.status = status;
            s->error->detail.path.cause.native_policy = true;
            s->error->detail.path.cause.operation = "Mob World path native admission";
        }
        (void)mt_fail(s, world_status(status), "Creature actual World path child");
        if (inspected && state.failure != NULL) {
            s->error->native_policy = state.failure->native_policy;
            s->error->source_exception = state.failure->source_exception;
            s->error->exception_class = state.failure->exception_class;
            s->error->child_site =
                (mc_mob_site){state.failure->site.class_name, state.failure->site.method,
                              state.failure->site.descriptor, state.failure->site.pc};
        }
    } else if (!inspected || !state.source_returned) {
        (void)mt_fail(s, MC_ENTITY_STATE_ERROR, "Creature World path did not return source body");
    } else {
        s->error->state.path_returned = true;
        const mc_mob_site caller = s->error->site;
        mt_at(s, caller.class_name, caller.method, caller.descriptor, store_pc);
        ok = mt_path_store(s, state.result);
        if (ok)
            s->error->state.path_store_completed = true;
    }
    if (!retire_child(&call, s->error)) {
        s->entity->mob->pending_path = call;
        s->hold_transferred = true;
        return false;
    }
    if (ok) {
        s->error->child = MC_MOB_TICK_CHILD_NONE;
        s->error->child_site = (mc_mob_site){0};
    }
    return ok;
}

bool mt_path_cleanup_entered(mc_entity *p, mc_mob_tick_error *error) {
    if (p->mob->pending_path == NULL)
        return true;
    if (!retire_child(&p->mob->pending_path, error))
        return false;
    mc_entity_release(p); /* The transferred failed-tick occurrence. */
    return true;
}
