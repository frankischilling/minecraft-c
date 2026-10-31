#include "internal.h"
#include <stdlib.h>
#include <string.h>

struct mc_mob_ray_pending {
    mc_world_raytrace_call *ray;
    mc_entity_vector *arguments[2];
    mc_mob_site caller;
};

static void flatten(mc_mob_tick_ray_error *out, const mc_world_raytrace_error *source) {
    memset(out, 0, sizeof(*out));
    if (source == NULL)
        return;
    out->cause = *source;
    if (source->world != NULL) {
        out->has_world = true;
        out->world = *source->world;
    }
    if (source->collision != NULL) {
        out->has_collision = true;
        out->collision = *source->collision;
        if (source->collision->nearby.query.source_chunk != NULL) {
            out->has_source_chunk = true;
            out->source_chunk = *source->collision->nearby.query.source_chunk;
            out->collision.nearby.query.source_chunk = NULL;
        }
    }
    if (source->source_chunk != NULL) {
        out->has_source_chunk = true;
        out->source_chunk = *source->source_chunk;
    }
    if (source->registry != NULL) {
        out->has_registry = true;
        out->registry = *source->registry;
    }
    if (source->entity != NULL) {
        out->has_entity = true;
        out->entity = *source->entity;
    }
    if (source->vector != NULL) {
        out->has_vector = true;
        out->vector = *source->vector;
    }
    if (source->geometry != NULL) {
        out->has_geometry = true;
        out->geometry = *source->geometry;
    }
    if (source->bounds != NULL) {
        out->has_bounds = true;
        out->bounds = *source->bounds;
    }
    if (source->hit != NULL) {
        out->has_hit = true;
        out->hit = *source->hit;
    }
    out->cause.world = NULL;
    out->cause.collision = NULL;
    out->cause.source_chunk = NULL;
    out->cause.registry = NULL;
    out->cause.entity = NULL;
    out->cause.vector = NULL;
    out->cause.geometry = NULL;
    out->cause.bounds = NULL;
    out->cause.hit = NULL;
}

static mc_entity_status mapped(mc_world_raytrace_status status) {
    return status == MC_WORLD_RAYTRACE_LIMIT        ? MC_ENTITY_LIMIT
           : status == MC_WORLD_RAYTRACE_ALLOCATION ? MC_ENTITY_ALLOCATION_FAILED
           : status == MC_WORLD_RAYTRACE_BUSY       ? MC_ENTITY_BUSY
                                                    : MC_ENTITY_WORLD_FAILED;
}

static bool drain(mc_entity *p, struct mc_mob_ray_pending *pending, mc_mob_tick_error *error) {
    if (pending->ray != NULL) {
        const mc_world_raytrace_status status = mc_world_raytrace_retire(&pending->ray);
        if (status != MC_WORLD_RAYTRACE_OK) {
            mc_world_raytrace_state state = {0};
            if (mc_world_raytrace_inspect(pending->ray, &state))
                flatten(&error->cleanup_ray, state.cleanup);
            if (error->cleanup_ray.cause.status == MC_WORLD_RAYTRACE_OK) {
                error->cleanup_ray.cause.status = status;
                error->cleanup_ray.cause.native_policy = true;
                error->cleanup_ray.cause.operation = "Mob owned World ray native retirement";
            }
            if (error->status == MC_ENTITY_OK) {
                error->status = mapped(status);
                error->native_policy = true;
                error->operation = "Mob owned World ray native retirement";
            }
            error->state.cleanup_pending = true;
            return false;
        }
    }
    for (size_t i = 0; i < 2; ++i) {
        if (pending->arguments[i] == NULL)
            continue;
        const mc_entity_status status = mc_entity_vector_release(p->classes, pending->arguments[i]);
        if (status != MC_ENTITY_OK) {
            error->cleanup_ray.has_entity = true;
            error->cleanup_ray.cause.status = MC_WORLD_RAYTRACE_CLEANUP_FAILED;
            error->cleanup_ray.cause.native_policy = true;
            error->cleanup_ray.entity = (mc_entity_error){
                .status = status,
                .operation = "Mob visibility argument native occurrence retirement"};
            error->cleanup_ray.cause.operation = error->cleanup_ray.entity.operation;
            if (error->status == MC_ENTITY_OK) {
                error->status = status;
                error->native_policy = true;
                error->operation = error->cleanup_ray.entity.operation;
            }
            error->state.cleanup_pending = true;
            return false;
        }
        pending->arguments[i] = NULL;
    }
    return true;
}

bool mt_visible(mc_mob_tick_scope *s, mc_entity *target, bool *out) {
    mc_entity *p = s->entity;
    mc_world_runtime *captured = p->base.world;
    float height;
    s->error->child_site = (mc_mob_site){"ls", "e", "(Lsn;)Z", 13};
    if (!mt_fp(s) || !mt_eye(s, p, &height))
        return false;
    const double x = p->base.x, y = at_add(p->base.y, at_widen(height)), z = p->base.z;
    s->error->child_site.pc = 22;
    struct mc_mob_ray_pending *pending = calloc(1, sizeof(*pending));
    if (pending == NULL)
        return mt_fail(s, MC_ENTITY_ALLOCATION_FAILED, "Mob visibility native child workspace");
    pending->caller = s->error->site;
    bool target_held = false;
    if (target != NULL) {
        const mc_entity_status held = mc_entity_retain(target);
        if (held != MC_ENTITY_OK) {
            free(pending);
            return mt_fail(s, held, "Creature visibility argument local occurrence");
        }
        target_held = true;
    }
    bool ok = false, visible = false;
    s->error->child = MC_MOB_TICK_CHILD_VECTOR;
    mc_entity_status status = mc_entity_vector_pool_get(p->classes, x, y, z, &pending->arguments[0],
                                                        &s->error->detail.vector);
    if (status != MC_ENTITY_OK) {
        mt_fail(s, status, "Creature actual first visibility vector");
        goto done;
    }
    s->error->child_site.pc = 26;
    if (target == NULL) {
        mt_null(s, "Creature visibility null target coordinate receiver");
        goto done;
    }
    s->error->child_site.pc = 34;
    if (!mt_eye(s, target, &height))
        goto done;
    s->error->child_site.pc = 43;
    status = mc_entity_vector_pool_get(p->classes, target->base.x,
                                       at_add(target->base.y, at_widen(height)), target->base.z,
                                       &pending->arguments[1], &s->error->detail.vector);
    if (status != MC_ENTITY_OK) {
        mt_fail(s, status, "Creature actual second visibility vector");
        goto done;
    }
    s->error->child_site.pc = 46;
    if (captured == NULL) {
        mt_null(s, "Creature visibility null captured World receiver");
        goto done;
    }
    ++s->error->state.ray_calls;
    s->error->state.ray_start[0] = pending->arguments[0]->x;
    s->error->state.ray_start[1] = pending->arguments[0]->y;
    s->error->state.ray_start[2] = pending->arguments[0]->z;
    s->error->state.ray_end[0] = pending->arguments[1]->x;
    s->error->state.ray_end[1] = pending->arguments[1]->y;
    s->error->state.ray_end[2] = pending->arguments[1]->z;
    s->error->state.ray_arguments_stored = true;
    s->error->child = MC_MOB_TICK_CHILD_RAY;
    const mc_world_raytrace_status ray_status = mc_world_raytrace_begin(
        captured, pending->arguments[0], pending->arguments[1], &pending->ray);
    mc_world_raytrace_state state = {0};
    const bool inspected = pending->ray != NULL && mc_world_raytrace_inspect(pending->ray, &state);
    if (ray_status != MC_WORLD_RAYTRACE_OK) {
        flatten(&s->error->detail.ray, inspected ? state.failure : NULL);
        if (!inspected) {
            s->error->detail.ray.cause.status = ray_status;
            s->error->detail.ray.cause.native_policy = true;
            s->error->detail.ray.cause.operation = "Mob World ray admission";
        }
        mt_fail(s, mapped(ray_status),
                inspected && state.failure != NULL ? state.failure->operation
                                                   : "Mob World ray admission");
        if (inspected && state.failure != NULL) {
            s->error->native_policy = state.failure->native_policy;
            s->error->source_exception = state.failure->source_exception;
            s->error->exception_class = state.failure->exception_class;
        }
        goto done;
    }
    if (!inspected || !state.source_returned) {
        mt_fail(s, MC_ENTITY_STATE_ERROR, "Mob World ray did not return its source body");
        goto done;
    }
    visible = state.result == NULL;
    s->error->state.ray_returned = true;
    ok = true;
done:
    if (target_held)
        mc_entity_release(target);
    if (!drain(p, pending, s->error)) {
        p->mob->pending_ray = pending;
        s->hold_transferred = true;
        return false;
    }
    free(pending);
    if (!ok)
        return false;
    s->error->child = MC_MOB_TICK_CHILD_NONE;
    s->error->child_site = (mc_mob_site){0};
    *out = visible;
    return true;
}

mc_entity_status mc_mob_tick_cleanup(mc_entity *p, mc_mob_tick_error *out) {
    mc_mob_tick_error *error = malloc(sizeof(*error));
    if (error == NULL) {
        mc_mob_tick_error_init(out);
        if (out != NULL) {
            out->status = MC_ENTITY_ALLOCATION_FAILED;
            out->native_policy = true;
            out->operation = "Mob cleanup diagnostic workspace";
        }
        return MC_ENTITY_ALLOCATION_FAILED;
    }
    mc_mob_tick_error_init(error);
    if (!mc_entity_has_mob(p) || !mc_entity_is_kind(p, p->class_literal->kind)) {
        error->status = MC_ENTITY_STATE_ERROR;
        error->native_policy = true;
        error->operation = "Mob cleanup canonical owner";
    } else if (p->mob->tick_active) {
        error->status = MC_ENTITY_BUSY;
        error->native_policy = true;
        error->operation = "Mob cleanup while source tick entered";
    } else if (p->mob->pending_ray != NULL) {
        error->state.cleanup_pending = true;
        error->status = mc_entity_retain(p);
        if (error->status != MC_ENTITY_OK) {
            error->native_policy = true;
            error->operation = "Mob cleanup local lifetime hold";
        } else {
            struct mc_mob_ray_pending *pending = p->mob->pending_ray;
            error->site = pending->caller;
            if (drain(p, pending, error)) {
                p->mob->pending_ray = NULL;
                free(pending);
                error->state.cleanup_pending = false;
                mc_entity_release(p); /* The transferred failed-tick occurrence. */
            }
            mc_entity_release(p); /* This cleanup's independent local occurrence. */
        }
    }
    const mc_entity_status result = error->status;
    if (out != NULL)
        *out = *error;
    free(error);
    return result;
}
