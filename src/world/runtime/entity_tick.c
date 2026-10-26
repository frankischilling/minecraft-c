#include "entity_tick_internal.h"

void mc_world_entity_tick_error_init(mc_world_entity_tick_error *e) {
    if (e != NULL) {
        memset(e, 0, sizeof(*e));
        e->weather_index = e->deferred_index = e->loaded_index = -1;
        e->tile_index = e->comparison_index = SIZE_MAX;
    }
}

void wt_at(wt_scope *s, const char *method, unsigned int pc) {
    s->error.source_class =
        s->world->classes != NULL && s->world->classes->side == MC_WORLD_INFO_SERVER ? "dj" : "fd";
    s->error.source_method = method;
    s->error.bytecode = pc;
}

bool wt_fail(wt_scope *s, mc_world_storage_status status, const char *operation) {
    s->error.status = status;
    s->error.operation = operation;
    s->error.cause = (mc_world_storage_error){status, operation, 0};
    return false;
}

bool wt_cause(wt_scope *s, mc_world_storage_error cause) {
    s->error.cause = cause;
    s->error.status = cause.status;
    s->error.operation = cause.operation;
    if (cause.status == MC_WORLD_STORAGE_OK)
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World update missing child cause");
    return false;
}

mc_world_storage_status wt_entity_status(mc_entity_status status) {
    return status == MC_ENTITY_OK                  ? MC_WORLD_STORAGE_OK
           : status == MC_ENTITY_LIMIT             ? MC_WORLD_STORAGE_LIMIT
           : status == MC_ENTITY_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
           : status == MC_ENTITY_BUSY              ? MC_WORLD_STORAGE_BUSY
           : status == MC_ENTITY_INVALID_ARGUMENT  ? MC_WORLD_STORAGE_INVALID_ARGUMENT
                                                   : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
}

bool wt_hold(wt_scope *s, mc_entity *entity, unsigned int pc) {
    if (entity == NULL)
        return true;
    const mc_entity_status held = mc_entity_retain(entity);
    if (held == MC_ENTITY_OK)
        return true;
    s->error.bytecode = pc;
    s->error.entity =
        (mc_entity_error){.status = held, .operation = "World update local Entity hold"};
    return wt_fail(s, wt_entity_status(held), s->error.entity.operation);
}

bool wt_relationship(wt_scope *s, mc_entity *link, mc_entity *current, unsigned int pc,
                     mc_entity **out) {
    const unsigned int caller = s->error.bytecode;
    s->error.relationship_bytecode = pc;
    if (link == NULL) {
        *out = NULL;
        return true;
    }
    bool proved = link == current;
    for (size_t i = 0; !proved && i < s->parent_count; ++i) {
        if (!wt_work(s, caller))
            return false;
        proved = s->parents[i] == link;
    }
    if (!proved && s->context != NULL) {
        for (size_t i = 0; i < s->context->relationship_actor_count; ++i) {
            if (!wt_work(s, caller))
                return false;
            if (s->context->relationship_actors[i] == link) {
                proved = true;
                break;
            }
        }
    }
    if (!proved) {
        s->error.bytecode = caller;
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "World unproved mount/passenger association lifetime dependency");
    }
    /* Pointer identity was proved against an independently live owner BEFORE
     * dereferencing the borrowed raw link. */
    if (current != NULL && link->classes != current->classes) {
        s->error.bytecode = caller;
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "World relationship actual Entity namespace");
    }
    if (!wt_hold(s, link, caller))
        return false;
    *out = link;
    return true;
}

bool wt_fp(wt_scope *s, unsigned int pc) {
    s->error.bytecode = pc;
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                   "World update reached strict FP admission");
}

bool wt_work(wt_scope *s, unsigned int pc) {
    s->error.bytecode = pc;
    if (s->error.loop_work == s->error.max_loop_work)
        return wt_fail(s, MC_WORLD_STORAGE_LIMIT, "World pending source work ceiling");
    ++s->error.loop_work;
    return true;
}

void *wt_allocate(wt_scope *s, size_t bytes, unsigned int pc) {
    s->error.bytecode = pc;
    if (bytes == 0)
        return NULL;
    if (s->temporary_bytes > s->error.max_temporary_bytes ||
        bytes > s->error.max_temporary_bytes - s->temporary_bytes) {
        wt_fail(s, MC_WORLD_STORAGE_LIMIT, "World update temporary peak allocation ceiling");
        return NULL;
    }
    void *p = malloc(bytes);
    if (p == NULL) {
        wt_fail(s, MC_WORLD_STORAGE_ALLOCATION_FAILED, "World update temporary allocation");
        return NULL;
    }
    s->temporary_bytes += bytes;
    s->error.temporary_bytes = s->temporary_bytes;
    return p;
}

void wt_free(wt_scope *s, void *p, size_t bytes) {
    if (p != NULL) {
        free(p);
        s->temporary_bytes -= bytes;
    }
    if (s->error.status == MC_WORLD_STORAGE_OK)
        s->error.temporary_bytes = s->temporary_bytes;
}

static mc_world_storage_status rejected(mc_world_entity_tick_error *e,
                                        mc_world_storage_status status, const char *op) {
    if (e != NULL) {
        mc_world_entity_tick_error_init(e);
        e->status = status;
        e->operation = op;
        e->cause = (mc_world_storage_error){status, op, 0};
    }
    return status;
}

static mc_world_storage_status run(mc_world_runtime *world, mc_entity *entity, bool flag,
                                   bool wrapper, bool loops,
                                   const mc_world_entity_tick_context *context,
                                   const mc_world_entity_tick_limits *limits,
                                   mc_world_entity_tick_error *out) {
    if (world == NULL)
        return rejected(out, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World update receiver");
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED || world->entity_containers_retired)
        return rejected(out, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World update GAME owner");
    if (world->classes == NULL || world->classes->side != MC_WORLD_INFO_CLIENT)
        return rejected(out, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        "World Entity updates require actual CLIENT source owner");
    if (entity != NULL && (world->classes == NULL || world->classes->entities != entity->classes))
        return rejected(out, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                        "World update actual Entity namespace");
    if (context != NULL &&
        (context->relationship_actor_count > (size_t)INT32_MAX ||
         (context->relationship_actor_count != 0 && context->relationship_actors == NULL)))
        return rejected(out, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World relationship scope extent");
    mc_world_storage_error cause = {0};
    const mc_world_storage_status entered = mc_world_runtime_call_enter(world, &cause);
    if (entered != MC_WORLD_STORAGE_OK)
        return rejected(out, entered, cause.operation);
    /* Fixed per-call native scratch; no caller output or shared work state. */
    wt_scope *s = malloc(sizeof(*s));
    if (s == NULL) {
        mc_world_runtime_call_leave(world);
        return rejected(out, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                        "World update diagnostic scratch allocation");
    }
    memset(s, 0, sizeof(*s));
    mc_world_entity_tick_error_init(&s->error);
    s->world = world;
    s->context = context;
    s->error.max_loop_work =
        limits == NULL ? MC_WORLD_ENTITY_TICK_DEFAULT_LOOP_WORK : limits->max_loop_work;
    s->error.max_passenger_calls =
        limits == NULL ? MC_WORLD_ENTITY_TICK_DEFAULT_PASSENGER_CALLS : limits->max_passenger_calls;
    s->error.max_temporary_bytes =
        limits == NULL ? MC_WORLD_ENTITY_TICK_DEFAULT_TEMPORARY_BYTES : limits->max_temporary_bytes;
    if (loops)
        (void)wt_loops(s);
    else
        (void)wt_update(s, entity, flag, wrapper);
    /* Every completed/failed update has already retired its occurrence holds. */
    while (s->parent_count != 0)
        mc_entity_release(s->parents[--s->parent_count]);
    wt_free(s, s->parents, s->parent_capacity * sizeof(*s->parents));
    const mc_world_storage_status status = s->error.status;
    world->last_error =
        status == MC_WORLD_STORAGE_OK ? (mc_world_storage_error){0} : s->error.cause;
    if (status == MC_WORLD_STORAGE_OK)
        mc_world_entity_tick_error_init(&s->error);
    if (out != NULL)
        *out = s->error;
    free(s);
    mc_world_runtime_call_leave(world);
    return status;
}

mc_world_storage_status mc_world_runtime_tick_entity(mc_world_runtime *w, mc_entity *p,
                                                     const mc_world_entity_tick_context *c,
                                                     const mc_world_entity_tick_limits *l,
                                                     mc_world_entity_tick_error *e) {
    return run(w, p, true, true, false, c, l, e);
}

mc_world_storage_status mc_world_runtime_update_entity(mc_world_runtime *w, mc_entity *p, bool flag,
                                                       const mc_world_entity_tick_context *c,
                                                       const mc_world_entity_tick_limits *l,
                                                       mc_world_entity_tick_error *e) {
    return run(w, p, flag, false, false, c, l, e);
}

mc_world_storage_status mc_world_runtime_tick_entities(mc_world_runtime *w,
                                                       const mc_world_entity_tick_context *c,
                                                       const mc_world_entity_tick_limits *l,
                                                       mc_world_entity_tick_error *e) {
    return run(w, NULL, false, false, true, c, l, e);
}
