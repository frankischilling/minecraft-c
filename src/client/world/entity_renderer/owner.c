#include "internal.h"
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(mc_client_entity_renderer_error) <= 16384u,
               "passive EntityRenderer error must fit the bounded native stack policy");

void mc_client_entity_renderer_error_init(mc_client_entity_renderer_error *e) {
    if (e != NULL) {
        memset(e, 0, sizeof(*e));
        e->site.pc = e->px_pc = e->ra_pc = e->iy_pc = -1;
    }
}

mc_client_graphics_site entity_renderer_site(const char *cl, int32_t pc) {
    const char *descriptor = strcmp(cl, "iy") == 0   ? "(Lsj;Lkv;Lji;)V"
                             : strcmp(cl, "cu") == 0 ? "()V"
                                                     : "(Lnet/minecraft/client/Minecraft;)V";
    return (mc_client_graphics_site){cl, "<init>", descriptor, pc};
}

mc_client_entity_renderer_status entity_renderer_fail(mc_client_entity_renderer_error *e,
                                                      mc_client_entity_renderer_status status,
                                                      const char *op, const char *cl, int32_t pc,
                                                      bool native) {
    e->status = status;
    e->operation = op;
    e->native_policy = native;
    e->site =
        cl == NULL ? (mc_client_graphics_site){NULL, NULL, NULL, -1} : entity_renderer_site(cl, pc);
    return status;
}

mc_client_entity_renderer_options mc_client_entity_renderer_default_options(void) {
    return (mc_client_entity_renderer_options){.arena = mc_client_render_engine_default_options(),
                                               .model_bytes = 1048576u,
                                               .image_bytes = 64u * 1048576u,
                                               .map_array = mc_client_color_array_default_limits()};
}

mc_client_entity_renderer_status
mc_client_entity_renderer_class_active_use(mc_client_render_classes *classes,
                                           mc_client_entity_renderer_error *out) {
    mc_client_entity_renderer_error local;
    mc_client_entity_renderer_error *e = out == NULL ? &local : out;
    mc_client_entity_renderer_error_init(e);
    if (classes == NULL)
        return entity_renderer_fail(e, MC_CLIENT_ENTITY_RENDERER_INVALID_ARGUMENT,
                                    "EntityRenderer actual model namespace", NULL, -1, true);
    if (!classes->px_ready) {
        classes->px_a = false; /* px.<clinit> putstatic1; b remains its real zero default. */
        classes->px_ready = true;
    }
    return MC_CLIENT_ENTITY_RENDERER_OK;
}

mc_client_entity_renderer_status mc_client_entity_renderer_allocate(
    const mc_client_entity_renderer_services *s, const mc_client_entity_renderer_options *requested,
    mc_client_source_px **out, mc_client_entity_renderer_error *error) {
    mc_client_entity_renderer_error local;
    mc_client_entity_renderer_error *e = error == NULL ? &local : error;
    mc_client_entity_renderer_error_init(e);
    if (s == NULL || out == NULL || s->models == NULL || s->graphics == NULL || s->profile == NULL)
        return entity_renderer_fail(e, MC_CLIENT_ENTITY_RENDERER_INVALID_ARGUMENT,
                                    "EntityRenderer native services/output", NULL, -1, true);
    const mc_client_entity_renderer_options options =
        requested == NULL ? mc_client_entity_renderer_default_options() : *requested;
    if (options.arena.max_constructor_bytes < sizeof(mc_client_source_px))
        return entity_renderer_fail(e, MC_CLIENT_ENTITY_RENDERER_LIMIT,
                                    "EntityRenderer native owner bound", NULL, -1, true);
    mc_client_source_px *o = calloc(1, sizeof(*o));
    if (o == NULL)
        return entity_renderer_fail(e, MC_CLIENT_ENTITY_RENDERER_ALLOCATION_FAILED,
                                    "EntityRenderer native RAW owner", NULL, -1, true);
    e->model.status = mc_client_render_classes_retain(s->models, s->profile);
    if (e->model.status != MC_CLIENT_MODEL_OK) {
        free(o);
        return entity_renderer_fail(e, MC_CLIENT_ENTITY_RENDERER_MODEL_FAILED,
                                    "EntityRenderer same profile/model namespace hold", NULL, -1,
                                    true);
    }
    e->owner = mc_client_render_engine_classes_retain(s->graphics);
    if (e->owner != MC_CLIENT_RENDER_ENGINE_OK) {
        (void)mc_client_render_classes_release(s->models);
        free(o);
        return entity_renderer_fail(e, MC_CLIENT_ENTITY_RENDERER_GRAPHICS_FAILED,
                                    "EntityRenderer graphics namespace hold", NULL, -1, true);
    }
    o->services = *s;
    o->options = options;
    o->holds = 1;
    o->models_held = o->graphics_held = true;
    o->last_store = -1;
    o->arena.options = options.arena;
    o->arena.bytes = sizeof(*o);
    atomic_init(&o->d, 0);
    atomic_init(&o->e, 0); /* Native RAW defaults, not source182/187. */
    *out = o;
    return MC_CLIENT_ENTITY_RENDERER_OK;
}

void *entity_renderer_context_allocate(entity_renderer_context *o, size_t bytes, const char *cl,
                                       int32_t pc) {
    void *p = mc_client_render_arena_allocate(
        o->arena, bytes,
        cl == NULL ? (mc_client_graphics_site){NULL, NULL, NULL, -1} : entity_renderer_site(cl, pc),
        &o->failure->graphics);
    if (p == NULL)
        entity_renderer_fail(o->failure,
                             o->failure->graphics.status == MC_CLIENT_GRAPHICS_LIMIT
                                 ? MC_CLIENT_ENTITY_RENDERER_LIMIT
                                 : MC_CLIENT_ENTITY_RENDERER_ALLOCATION_FAILED,
                             "EntityRenderer genuine child allocation", cl, pc, true);
    return p;
}

void *entity_renderer_allocate_child(mc_client_source_px *o, size_t bytes, const char *cl,
                                     int32_t pc) {
    entity_renderer_context context = {&o->services, &o->options, &o->arena, &o->failure};
    return entity_renderer_context_allocate(&context, bytes, cl, pc);
}

mc_client_entity_renderer_status
mc_client_entity_renderer_construct(mc_client_source_px *o, client_application *app,
                                    mc_client_entity_renderer_error *out) {
    mc_client_entity_renderer_error local;
    if (o == NULL) {
        mc_client_entity_renderer_error_init(&local);
        entity_renderer_fail(&local, MC_CLIENT_ENTITY_RENDERER_INVALID_ARGUMENT,
                             "EntityRenderer constructor receiver", NULL, -1, true);
        if (out != NULL)
            *out = local;
        return local.status;
    }
    if (o->active || o->phase != MC_CLIENT_ENTITY_RENDERER_RAW || !o->services.models->px_ready) {
        mc_client_entity_renderer_error_init(&local);
        entity_renderer_fail(&local,
                             o->active ? MC_CLIENT_ENTITY_RENDERER_BUSY
                                       : MC_CLIENT_ENTITY_RENDERER_INVALID_PHASE,
                             "EntityRenderer constructor admission", NULL, -1, true);
        if (out != NULL)
            *out = local;
        return local.status;
    }
    mc_client_entity_renderer_error_init(&o->failure);
    o->phase = MC_CLIENT_ENTITY_RENDERER_CONSTRUCTING;
    o->active = true;
    o->fields.k = 0.0f;
    o->last_store = 6;
    o->fields.m = NULL;
    o->last_store = 11;
    static const int32_t news[6] = {15, 26, 37, 48, 59, 70};
    static const int32_t stores[6] = {22, 33, 44, 55, 66, 77};
    for (size_t i = 0; i < 6; ++i) {
        o->failure.px_pc = news[i];
        mc_player_mouse_filter_fields *cu =
            entity_renderer_allocate_child(o, sizeof(*cu), "px", news[i]);
        if (cu == NULL)
            goto failed;
        /* Distinct actual cu: Object init and no own source field stores.
         * Arena calloc supplied JVM positive-zero defaults; no Player exists. */
        o->fields.filters[i] = cu;
        o->last_store = stores[i];
    }
    o->fields.t = 4.0f;
    o->last_store = 83;
    o->fields.u = 4.0f;
    o->last_store = 89;
    static const int32_t floats[8] = {94, 99, 104, 109, 114, 119, 124, 129};
    for (size_t i = 0; i < 8; ++i) {
        o->fields.v_to_C[i] = 0.0f;
        o->last_store = floats[i];
    }
    o->fields.D = false;
    o->last_store = 134;
    o->fields.E = 1.0;
    o->last_store = 139;
    o->fields.F = 0.0;
    o->last_store = 144;
    o->fields.G = 0.0;
    o->last_store = 149;
    o->failure.px_pc = 153;
    o->failure.clock = mc_platform_current_time_millis(&o->fields.H);
    if (o->failure.clock != MC_PLATFORM_MILLIS_OK) {
        entity_renderer_fail(&o->failure, MC_CLIENT_ENTITY_RENDERER_CLOCK_FAILED,
                             "actual px System.currentTimeMillis", "px", 153, true);
        goto failed;
    }
    o->last_store = 156;
    o->fields.I = 0;
    o->last_store = 161;
    o->failure.px_pc = 165;
    o->pending_random = entity_renderer_allocate_child(o, sizeof(*o->pending_random), "px", 165);
    if (o->pending_random == NULL)
        goto failed;
    o->failure.px_pc = 169;
    o->failure.random = mc_java_random_construct_default(o->services.profile, o->pending_random);
    if (o->failure.random != MC_JAVA_RANDOM_PROFILE_OK) {
        entity_renderer_fail(&o->failure, MC_CLIENT_ENTITY_RENDERER_RANDOM_FAILED,
                             "actual px no-argument Random constructor", "px", 169, true);
        goto failed;
    }
    o->fields.J = o->pending_random;
    o->pending_random = NULL;
    o->last_store = 172;
    o->fields.K = 0;
    o->last_store = 177;
    atomic_store_explicit(&o->d, 0, memory_order_seq_cst);
    o->last_store = 182;
    atomic_store_explicit(&o->e, 0, memory_order_seq_cst);
    o->last_store = 187;
    o->failure.px_pc = 193;
    if (!mc_client_ge_e(o->services.graphics, &o->arena, 16, entity_renderer_site("px", 193),
                        &o->fields.f, &o->failure.graphics)) {
        entity_renderer_fail(&o->failure, MC_CLIENT_ENTITY_RENDERER_GRAPHICS_FAILED,
                             "actual px ge.e FloatBuffer", "px", 193,
                             o->failure.graphics.native_policy);
        goto failed;
    }
    o->last_store = 196;
    o->fields.j = app;
    o->last_store = 201;
    o->failure.px_pc = 205;
    entity_renderer_context context = {&o->services, &o->options, &o->arena, &o->failure};
    o->pending_ra = entity_renderer_held_borrowed(&context, "px", 205);
    if (o->pending_ra == NULL)
        goto failed;
    o->pending_ra->source->last_store = -1;
    o->failure.px_pc = 210;
    if (mc_client_held_item_construct(o->pending_ra, app, &o->failure) !=
        MC_CLIENT_ENTITY_RENDERER_OK)
        goto failed;
    o->fields.c = o->pending_ra->source;
    o->last_store = 213;
    o->phase = MC_CLIENT_ENTITY_RENDERER_READY;
    o->active = false;
    if (out != NULL)
        *out = o->failure;
    return MC_CLIENT_ENTITY_RENDERER_OK;
failed:
    o->phase = MC_CLIENT_ENTITY_RENDERER_FAILED;
    o->active = false;
    if (out != NULL)
        *out = o->failure;
    return o->failure.status;
}

static mc_client_entity_renderer_status
release_owner(mc_client_source_px **slot, mc_client_entity_renderer_error *out, bool strict) {
    mc_client_entity_renderer_error local;
    mc_client_entity_renderer_error_init(&local);
    if (slot == NULL)
        entity_renderer_fail(&local, MC_CLIENT_ENTITY_RENDERER_INVALID_ARGUMENT,
                             "EntityRenderer retirement slot", NULL, -1, true);
    else if (*slot == NULL) {
        if (out != NULL)
            *out = local;
        return MC_CLIENT_ENTITY_RENDERER_OK;
    }
    if (slot == NULL) {
        if (out != NULL)
            *out = local;
        return local.status;
    }
    mc_client_source_px *o = *slot;
    if (o->active || o->phase == MC_CLIENT_ENTITY_RENDERER_CONSTRUCTING ||
        (strict && o->holds != 1)) {
        entity_renderer_fail(&local, MC_CLIENT_ENTITY_RENDERER_BUSY,
                             "EntityRenderer active/external occurrence", NULL, -1, true);
        if (out != NULL)
            *out = local;
        return local.status;
    }
    if (o->holds > 1) {
        --o->holds;
        *slot = NULL;
        if (out != NULL)
            *out = local;
        return MC_CLIENT_ENTITY_RENDERER_OK;
    }
    if (o->pending_ra != NULL && !mc_client_held_item_is_final_inactive(o->pending_ra)) {
        entity_renderer_fail(&local, MC_CLIENT_ENTITY_RENDERER_BUSY,
                             "EntityRenderer external actual RA occurrence", NULL, -1, true);
        if (out != NULL)
            *out = local;
        return local.status;
    }
    o->active = true;
    o->phase = MC_CLIENT_ENTITY_RENDERER_RETIRING;
    mc_client_entity_renderer_error_init(&o->retirement);
    if (mc_client_held_item_retire(&o->pending_ra, &o->retirement) != MC_CLIENT_ENTITY_RENDERER_OK)
        goto retry;
    o->pending_ra = NULL;
    o->fields.c = NULL;
    if (o->graphics_held) {
        o->retirement.owner = mc_client_render_engine_classes_release(o->services.graphics);
        if (o->retirement.owner != MC_CLIENT_RENDER_ENGINE_OK) {
            entity_renderer_fail(&o->retirement, MC_CLIENT_ENTITY_RENDERER_BUSY,
                                 "EntityRenderer graphics class hold cleanup", NULL, -1, true);
            goto retry;
        }
        o->graphics_held = false;
    }
    if (o->models_held) {
        o->retirement.model.status = mc_client_render_classes_release(o->services.models);
        if (o->retirement.model.status != MC_CLIENT_MODEL_OK) {
            entity_renderer_fail(&o->retirement, MC_CLIENT_ENTITY_RENDERER_BUSY,
                                 "EntityRenderer model class hold cleanup", NULL, -1, true);
            goto retry;
        }
        o->models_held = false;
    }
    mc_client_render_arena_free(&o->arena);
    *slot = NULL;
    free(o);
    if (out != NULL)
        *out = local;
    return MC_CLIENT_ENTITY_RENDERER_OK;
retry:
    o->active = false;
    if (out != NULL)
        *out = o->retirement;
    return o->retirement.status;
}

mc_client_entity_renderer_status
mc_client_entity_renderer_release(mc_client_source_px **p, mc_client_entity_renderer_error *e) {
    return release_owner(p, e, false);
}

mc_client_entity_renderer_status
mc_client_entity_renderer_retire(mc_client_source_px **p, mc_client_entity_renderer_error *e) {
    return release_owner(p, e, true);
}

mc_client_entity_renderer_status
mc_client_entity_renderer_retain(mc_client_source_px *o, mc_client_entity_renderer_error *out) {
    mc_client_entity_renderer_error local;
    mc_client_entity_renderer_error_init(&local);
    if (o == NULL)
        entity_renderer_fail(&local, MC_CLIENT_ENTITY_RENDERER_INVALID_ARGUMENT,
                             "EntityRenderer retain receiver", NULL, -1, true);
    else if (o->active || o->phase == MC_CLIENT_ENTITY_RENDERER_CONSTRUCTING)
        entity_renderer_fail(&local, MC_CLIENT_ENTITY_RENDERER_BUSY,
                             "EntityRenderer retain while active", NULL, -1, true);
    else if (o->phase == MC_CLIENT_ENTITY_RENDERER_RETIRING)
        entity_renderer_fail(&local, MC_CLIENT_ENTITY_RENDERER_INVALID_PHASE,
                             "EntityRenderer terminal retain", NULL, -1, true);
    else if (o->holds == SIZE_MAX)
        entity_renderer_fail(&local, MC_CLIENT_ENTITY_RENDERER_LIMIT, "EntityRenderer hold limit",
                             NULL, -1, true);
    else
        ++o->holds;
    if (out != NULL)
        *out = local;
    return local.status;
}

bool mc_client_entity_renderer_inspect(const mc_client_source_px *o,
                                       mc_client_entity_renderer_state *out) {
    if (o == NULL || out == NULL || o->active)
        return false;
    const mc_client_source_ra *ra = o->pending_ra == NULL ? NULL : o->pending_ra->source;
    const mc_client_source_iy *iy = ra == NULL ? NULL : ra->pending_iy;
    const int32_t d = atomic_load_explicit(&o->d, memory_order_seq_cst);
    const int32_t e = atomic_load_explicit(&o->e, memory_order_seq_cst);
    *out = (mc_client_entity_renderer_state){
        .phase = o->phase,
        .holds = o->holds,
        .arena_bytes = o->arena.bytes,
        .arena_allocations = o->arena.allocations,
        .active = o->active,
        .px_class_ready = o->services.models->px_ready,
        .px_a = o->services.models->px_a,
        .px_b = o->services.models->px_b,
        .d = d,
        .e = e,
        .last_store_pc = o->last_store,
        .fields = o->fields,
        .pending_random = o->pending_random,
        .pending_ra = ra,
        .ra_occurrence = o->pending_ra,
        .pending_iy = iy,
        .failure = &o->failure,
        .retirement = &o->retirement,
        .ra_phase = ra == NULL ? MC_CLIENT_ENTITY_RENDERER_RAW : ra->phase,
        .iy_phase = iy == NULL ? MC_CLIENT_ENTITY_RENDERER_RAW : iy->phase,
        .ra_last_store_pc = ra == NULL ? -1 : ra->last_store,
        .iy_last_store_pc = iy == NULL ? -1 : iy->last_store,
        .ra_blocks = ra == NULL ? NULL : ra->e,
        .ra_app = ra == NULL ? NULL : ra->a,
        .ra_b = ra == NULL ? NULL : ra->b,
        .ra_c = ra == NULL ? 0.0f : ra->c,
        .ra_d = ra == NULL ? 0.0f : ra->d,
        .ra_g = ra == NULL ? 0 : ra->g,
        .map_array = iy == NULL ? NULL : iy->a,
        .pending_array = iy == NULL ? NULL : iy->array_candidate,
        .map_settings = iy == NULL ? NULL : iy->c,
        .map_font = iy == NULL ? NULL : iy->d,
        .map_engine = iy == NULL ? NULL : iy->retained_engine,
        .map_image = iy == NULL ? NULL : iy->image,
        .map_texture = iy == NULL ? 0 : iy->b,
        .map_lease = iy != NULL && iy->lease != NULL,
        .map_zero_stores = iy == NULL ? 0 : iy->zero_stores};
    return true;
}
