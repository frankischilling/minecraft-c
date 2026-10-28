#include "internal.h"
#include <stdlib.h>

/* Fixed native occurrence metadata is deliberately separate from the source
 * arena. In the PX path no source owner/layout/arena request changes size. */
mc_client_held_item *entity_renderer_held_borrowed(entity_renderer_context *context, const char *cl,
                                                   int32_t pc) {
    mc_client_held_item *o = calloc(1, sizeof(*o));
    if (o == NULL) {
        entity_renderer_fail(context->failure, MC_CLIENT_ENTITY_RENDERER_ALLOCATION_FAILED,
                             "actual RA native occurrence metadata", cl, pc, true);
        return NULL;
    }
    mc_client_entity_renderer_error_init(&o->failure);
    mc_client_entity_renderer_error_init(&o->retirement);
    o->context = *context;
    o->holds = 1;
    o->source = entity_renderer_context_allocate(context, sizeof(*o->source), cl, pc);
    if (o->source == NULL) {
        free(o);
        return NULL;
    }
    o->source->last_store = -1;
    return o;
}

mc_client_entity_renderer_status
mc_client_held_item_allocate(const mc_client_entity_renderer_services *services,
                             const mc_client_entity_renderer_options *requested,
                             mc_client_held_item **out, mc_client_entity_renderer_error *error) {
    mc_client_entity_renderer_error local;
    mc_client_entity_renderer_error *e = error == NULL ? &local : error;
    mc_client_entity_renderer_error_init(e);
    if (services == NULL || out == NULL || services->models == NULL || services->graphics == NULL ||
        services->profile == NULL)
        return entity_renderer_fail(e, MC_CLIENT_ENTITY_RENDERER_INVALID_ARGUMENT,
                                    "independent actual RA services/output", NULL, -1, true);
    mc_client_held_item *o = calloc(1, sizeof(*o));
    if (o == NULL)
        return entity_renderer_fail(e, MC_CLIENT_ENTITY_RENDERER_ALLOCATION_FAILED,
                                    "independent actual RA native occurrence metadata", NULL, -1,
                                    true);
    mc_client_entity_renderer_error_init(&o->failure);
    mc_client_entity_renderer_error_init(&o->retirement);
    o->independent = true;
    o->holds = 1;
    o->services = *services;
    o->options = requested == NULL ? mc_client_entity_renderer_default_options() : *requested;
    o->arena.options = o->options.arena;
    o->context = (entity_renderer_context){&o->services, &o->options, &o->arena, &o->failure};
    e->model.status = mc_client_render_classes_retain(services->models, services->profile);
    if (e->model.status != MC_CLIENT_MODEL_OK) {
        free(o);
        return entity_renderer_fail(e, MC_CLIENT_ENTITY_RENDERER_MODEL_FAILED,
                                    "independent RA model namespace hold", NULL, -1, true);
    }
    o->models_held = true;
    e->owner = mc_client_render_engine_classes_retain(services->graphics);
    if (e->owner != MC_CLIENT_RENDER_ENGINE_OK) {
        (void)mc_client_render_classes_release(services->models);
        free(o);
        return entity_renderer_fail(e, MC_CLIENT_ENTITY_RENDERER_GRAPHICS_FAILED,
                                    "independent RA graphics namespace hold", NULL, -1, true);
    }
    o->graphics_held = true;
    o->source = entity_renderer_context_allocate(&o->context, sizeof(*o->source), NULL, -1);
    if (o->source == NULL) {
        /* No source object has been allocated; native class holds are balanced. */
        *e = o->failure;
        (void)mc_client_render_engine_classes_release(services->graphics);
        (void)mc_client_render_classes_release(services->models);
        free(o);
        return e->status;
    }
    o->source->last_store = -1;
    *out = o;
    return MC_CLIENT_ENTITY_RENDERER_OK;
}

mc_client_entity_renderer_status
mc_client_held_item_construct(mc_client_held_item *o, client_application *app,
                              mc_client_entity_renderer_error *out) {
    mc_client_entity_renderer_error local;
    mc_client_entity_renderer_error_init(&local);
    if (o == NULL || o->source == NULL)
        entity_renderer_fail(&local, MC_CLIENT_ENTITY_RENDERER_INVALID_ARGUMENT,
                             "actual RA constructor receiver", NULL, -1, true);
    else if (o->active || o->source->phase != MC_CLIENT_ENTITY_RENDERER_RAW)
        entity_renderer_fail(&local,
                             o->active ? MC_CLIENT_ENTITY_RENDERER_BUSY
                                       : MC_CLIENT_ENTITY_RENDERER_INVALID_PHASE,
                             "actual RA constructor admission", NULL, -1, true);
    else {
        o->active = true;
        (void)entity_renderer_ra_construct(&o->context, o->source, app);
        o->active = false;
        local = *o->context.failure;
    }
    if (out != NULL)
        *out = local;
    return local.status;
}

bool mc_client_held_item_is_final_inactive(const mc_client_held_item *o) {
    return o != NULL && !o->active && o->holds == 1 &&
           o->source->phase != MC_CLIENT_ENTITY_RENDERER_CONSTRUCTING;
}

const mc_client_source_ra *mc_client_held_item_identity(const mc_client_held_item *o) {
    return o == NULL ? NULL : o->source;
}

mc_client_entity_renderer_status mc_client_held_item_retain(mc_client_held_item *o,
                                                            mc_client_entity_renderer_error *out) {
    mc_client_entity_renderer_error e;
    mc_client_entity_renderer_error_init(&e);
    if (o == NULL)
        entity_renderer_fail(&e, MC_CLIENT_ENTITY_RENDERER_INVALID_ARGUMENT,
                             "actual RA occurrence receiver", NULL, -1, true);
    else if (o->active || o->source->phase == MC_CLIENT_ENTITY_RENDERER_CONSTRUCTING)
        entity_renderer_fail(&e, MC_CLIENT_ENTITY_RENDERER_BUSY, "actual RA active occurrence",
                             NULL, -1, true);
    else if (o->source->phase == MC_CLIENT_ENTITY_RENDERER_RETIRING)
        entity_renderer_fail(&e, MC_CLIENT_ENTITY_RENDERER_INVALID_PHASE,
                             "actual RA terminal occurrence", NULL, -1, true);
    else if (o->holds == SIZE_MAX)
        entity_renderer_fail(&e, MC_CLIENT_ENTITY_RENDERER_LIMIT, "actual RA occurrence limit",
                             NULL, -1, true);
    else
        ++o->holds;
    if (out != NULL)
        *out = e;
    return e.status;
}

static mc_client_entity_renderer_status
held_release(mc_client_held_item **slot, mc_client_entity_renderer_error *out, bool strict) {
    mc_client_entity_renderer_error e;
    mc_client_entity_renderer_error_init(&e);
    if (slot == NULL) {
        entity_renderer_fail(&e, MC_CLIENT_ENTITY_RENDERER_INVALID_ARGUMENT,
                             "actual RA occurrence slot", NULL, -1, true);
    } else if (*slot != NULL) {
        mc_client_held_item *o = *slot;
        if (o->active || o->source->phase == MC_CLIENT_ENTITY_RENDERER_CONSTRUCTING ||
            (strict && o->holds != 1)) {
            entity_renderer_fail(&e, MC_CLIENT_ENTITY_RENDERER_BUSY,
                                 "actual RA active/external occurrence", NULL, -1, true);
        } else if (o->holds > 1) {
            --o->holds;
            *slot = NULL;
        } else {
            o->active = true;
            mc_client_entity_renderer_error_init(&o->retirement);
            if (!entity_renderer_ra_retire(&o->context, o->source, &o->retirement))
                goto retry;
            if (o->graphics_held) {
                o->retirement.owner = mc_client_render_engine_classes_release(o->services.graphics);
                if (o->retirement.owner != MC_CLIENT_RENDER_ENGINE_OK) {
                    entity_renderer_fail(&o->retirement, MC_CLIENT_ENTITY_RENDERER_BUSY,
                                         "actual RA graphics namespace cleanup", NULL, -1, true);
                    goto retry;
                }
                o->graphics_held = false;
            }
            if (o->models_held) {
                o->retirement.model.status = mc_client_render_classes_release(o->services.models);
                if (o->retirement.model.status != MC_CLIENT_MODEL_OK) {
                    entity_renderer_fail(&o->retirement, MC_CLIENT_ENTITY_RENDERER_BUSY,
                                         "actual RA model namespace cleanup", NULL, -1, true);
                    goto retry;
                }
                o->models_held = false;
            }
            if (o->independent)
                mc_client_render_arena_free(&o->arena);
            *slot = NULL;
            free(o);
            goto done;
        retry:
            o->active = false;
            e = o->retirement;
        }
    }
done:
    if (out != NULL)
        *out = e;
    return e.status;
}

mc_client_entity_renderer_status mc_client_held_item_release(mc_client_held_item **o,
                                                             mc_client_entity_renderer_error *e) {
    return held_release(o, e, false);
}

mc_client_entity_renderer_status mc_client_held_item_retire(mc_client_held_item **o,
                                                            mc_client_entity_renderer_error *e) {
    return held_release(o, e, true);
}

bool mc_client_held_item_inspect(const mc_client_held_item *o, mc_client_held_item_state *s) {
    if (o == NULL || s == NULL || o->active)
        return false;
    const mc_client_source_ra *ra = o->source;
    const mc_client_source_iy *iy = ra->pending_iy;
    *s = (mc_client_held_item_state){.holds = o->holds,
                                     .arena_bytes = o->context.arena->bytes,
                                     .arena_allocations = o->context.arena->allocations,
                                     .pending_ra = ra,
                                     .pending_iy = iy,
                                     .ra_phase = ra->phase,
                                     .ra_last_store_pc = ra->last_store,
                                     .ra_app = ra->a,
                                     .ra_b = ra->b,
                                     .ra_c = ra->c,
                                     .ra_d = ra->d,
                                     .ra_g = ra->g,
                                     .ra_blocks = ra->e,
                                     .failure = o->context.failure,
                                     .retirement = &o->retirement};
    if (iy != NULL) {
        s->iy_phase = iy->phase;
        s->iy_last_store_pc = iy->last_store;
        s->map_array = iy->a;
        s->pending_array = iy->array_candidate;
        s->map_settings = iy->c;
        s->map_font = iy->d;
        s->map_engine = iy->retained_engine;
        s->map_image = iy->image;
        s->map_texture = iy->b;
        s->map_lease = iy->lease != NULL;
        s->map_zero_stores = iy->zero_stores;
    }
    return true;
}
