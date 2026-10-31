#include "client/application_state.h"
#include "client/render_engine/texture_lookup_internal.h"
#include "draw_internal.h"
#include "entity/entity_internal.h"
#include "mc/client/settings/construction.h"
#include "world/runtime/runtime_internal.h"
#include <stdlib.h>
#include <string.h>

static bool owner_fail(mc_client_manager_error *e, mc_client_manager_status status,
                       const char *operation) {
    if (e != NULL)
        (void)manager_fail(e, status, operation, NULL, NULL, NULL, -1, true);
    return false;
}

void manager_draw_error_init(mc_client_manager_draw_error *e) {
    if (e == NULL)
        return;
    memset(e, 0, sizeof(*e));
    e->site.pc = e->parent.pc = e->child.pc = -1;
    manager_error_init(&e->owner);
}

mc_client_manager_draw_limits mc_client_render_manager_draw_default_limits(void) {
    return (mc_client_manager_draw_limits){1048576u, 256u, 65536u, 4096u, 2048u, 65536u};
}

void *manager_draw_allocate_bytes(mc_client_render_manager_draw_call *c, size_t bytes,
                                  mc_client_manager_error *e) {
    if (c == NULL || c->manager_hold == NULL || bytes == 0) {
        owner_fail(e, MC_CLIENT_MANAGER_INVALID_ARGUMENT, "Draw native allocation owner/extent");
        return NULL;
    }
    mc_client_render_manager_classes *classes = c->manager_hold->classes;
    if (c->bytes > c->limits.max_bytes || bytes > c->limits.max_bytes - c->bytes ||
        classes->bytes > classes->options.class_bytes ||
        bytes > classes->options.class_bytes - classes->bytes) {
        owner_fail(e, MC_CLIENT_MANAGER_LIMIT, "Draw native overlapping allocation ceiling");
        return NULL;
    }
    void *p = calloc(1, bytes);
    if (p == NULL) {
        owner_fail(e, MC_CLIENT_MANAGER_ALLOCATION_FAILED, "Draw native allocation");
        return NULL;
    }
    c->bytes += bytes;
    classes->bytes += bytes;
    if (c->bytes > c->peak_bytes)
        c->peak_bytes = c->bytes;
    c->state.allocation_bytes = c->bytes;
    return p;
}

void manager_draw_free_bytes(mc_client_render_manager_draw_call *c, void *p, size_t bytes) {
    if (p == NULL)
        return;
    c->bytes -= bytes;
    c->manager_hold->classes->bytes -= bytes;
    c->state.allocation_bytes = c->bytes;
    free(p);
}

mc_client_manager_status manager_draw_allocate(mc_client_render_manager *m,
                                               const mc_client_manager_draw_services *services,
                                               const mc_client_manager_draw_limits *limits,
                                               mc_client_render_manager_draw_call **out,
                                               mc_client_manager_draw_error *error) {
    manager_draw_error_init(error);
    if (m == NULL || m->classes == NULL || out == NULL || *out != NULL || services == NULL) {
        if (error != NULL) {
            error->status = MC_CLIENT_MANAGER_INVALID_ARGUMENT;
            error->operation = "Draw native allocation arguments";
            error->native_policy = true;
        }
        return MC_CLIENT_MANAGER_INVALID_ARGUMENT;
    }
    mc_client_render_manager_classes *associated = m->classes;
    if (associated->models == NULL || associated->models->th != associated ||
        associated->models->th_retired || associated->phase != MC_CLIENT_MANAGER_READY ||
        associated->a != m || m->holds == 0 || m->active || associated->active) {
        if (error != NULL) {
            error->status = m->active || associated->active ? MC_CLIENT_MANAGER_BUSY
                                                            : MC_CLIENT_MANAGER_INVALID_PHASE;
            error->operation = "Draw canonical inactive singleton provenance";
            error->native_policy = true;
        }
        return m->active || associated->active ? MC_CLIENT_MANAGER_BUSY
                                               : MC_CLIENT_MANAGER_INVALID_PHASE;
    }
    if (m->draw_call != NULL) {
        if (error != NULL) {
            error->status = MC_CLIENT_MANAGER_BUSY;
            error->operation = "Draw earlier retained call still owns the manager graph";
            error->native_policy = true;
        }
        return MC_CLIENT_MANAGER_BUSY;
    }
    const mc_client_manager_draw_limits selected =
        limits == NULL ? mc_client_render_manager_draw_default_limits() : *limits;
    mc_client_render_manager_classes *classes = m->classes;
    if (selected.max_bytes < sizeof(mc_client_render_manager_draw_call) ||
        classes->bytes > classes->options.class_bytes ||
        sizeof(mc_client_render_manager_draw_call) >
            classes->options.class_bytes - classes->bytes) {
        if (error != NULL) {
            error->status = MC_CLIENT_MANAGER_LIMIT;
            error->operation = "Draw native retained call allocation ceiling";
            error->native_policy = true;
        }
        return MC_CLIENT_MANAGER_LIMIT;
    }
    mc_client_render_manager_draw_call *c = calloc(1, sizeof(*c));
    if (c == NULL) {
        if (error != NULL) {
            error->status = MC_CLIENT_MANAGER_ALLOCATION_FAILED;
            error->operation = "Draw native retained call allocation";
            error->native_policy = true;
        }
        return MC_CLIENT_MANAGER_ALLOCATION_FAILED;
    }
    manager_draw_error_init(&c->error);
    manager_draw_error_init(&c->cleanup);
    mc_entity_render_error_init(&c->brightness);
    mc_client_living_draw_error_init(&c->living);
    mc_client_living_draw_error_init(&c->post);
    const mc_client_manager_status held = mc_client_render_manager_retain(m, &c->error.owner);
    if (held != MC_CLIENT_MANAGER_OK) {
        c->error.status = held;
        c->error.operation = c->error.owner.operation;
        c->error.native_policy = true;
        if (error != NULL)
            *error = c->error;
        free(c);
        return held;
    }
    c->manager_hold = m;
    c->services = *services;
    c->limits = selected;
    c->scope = (mc_client_draw_scope){
        .owner = c,
        .model = {services->math, services->graphics, classes->models->entities, services->display},
        .application_classes = services->application_classes,
        .max_work = selected.max_work,
        .max_shadow_cells = selected.max_shadow_cells,
        .max_shadow_quads = selected.max_shadow_quads};
    c->bytes = c->peak_bytes = sizeof(*c);
    classes->bytes += sizeof(*c);
    c->state.receiver = m;
    c->state.allocation_bytes = c->bytes;
    c->state.failure = &c->error;
    c->state.cleanup = &c->cleanup;
    c->owned_list = -1;
    m->draw_call = c;
    *out = c;
    return MC_CLIENT_MANAGER_OK;
}

static bool scope_valid(mc_client_draw_scope *scope, mc_client_manager_error *e) {
    if (e != NULL)
        manager_error_init(e);
    return (scope != NULL && scope->owner != NULL && &scope->owner->scope == scope &&
            scope->owner->manager_hold != NULL &&
            scope->owner->manager_hold->draw_call == scope->owner && !scope->owner->retiring) ||
           owner_fail(e, MC_CLIENT_MANAGER_INVALID_PHASE, "Draw actual entered capture scope");
}

static const void *owner_identity(const manager_draw_owner *p) {
    switch (p->kind) {
        case MANAGER_DRAW_WORLD:
            return p->value.world;
        case MANAGER_DRAW_ENGINE:
            return p->value.engine;
        case MANAGER_DRAW_STRING:
            return p->value.string;
        case MANAGER_DRAW_SETTINGS:
            return p->value.settings;
        case MANAGER_DRAW_APPLICATION:
            return p->value.application;
    }
    return NULL;
}

static manager_draw_owner *find_owner(mc_client_render_manager_draw_call *c,
                                      manager_draw_owner_kind kind, const void *identity) {
    for (size_t i = 0; i < c->owner_count; ++i)
        if (c->owners[i].kind == kind && owner_identity(&c->owners[i]) == identity)
            return &c->owners[i];
    return NULL;
}

static manager_draw_owner *owner_slot(mc_client_render_manager_draw_call *c,
                                      mc_client_manager_error *e) {
    if (c->owner_count == c->limits.max_owners) {
        owner_fail(e, MC_CLIENT_MANAGER_LIMIT, "Draw distinct native owner ceiling");
        return NULL;
    }
    if (c->owner_count == c->owner_capacity) {
        size_t capacity = c->owner_capacity == 0 ? 8u : c->owner_capacity;
        if (capacity > SIZE_MAX / 2u)
            capacity = c->limits.max_owners;
        else if (c->owner_capacity != 0)
            capacity *= 2u;
        if (capacity > c->limits.max_owners)
            capacity = c->limits.max_owners;
        if (capacity <= c->owner_capacity || capacity > SIZE_MAX / sizeof(*c->owners)) {
            owner_fail(e, MC_CLIENT_MANAGER_LIMIT, "Draw native owner array extent");
            return NULL;
        }
        manager_draw_owner *grown = manager_draw_allocate_bytes(c, capacity * sizeof(*grown), e);
        if (grown == NULL)
            return NULL;
        if (c->owner_count != 0)
            memcpy(grown, c->owners, c->owner_count * sizeof(*grown));
        manager_draw_free_bytes(c, c->owners, c->owner_capacity * sizeof(*grown));
        c->owners = grown;
        c->owner_capacity = capacity;
    }
    return &c->owners[c->owner_count];
}

static void publish_owner(mc_client_render_manager_draw_call *c) {
    ++c->owner_count;
    c->state.owners = c->owner_count;
}

bool mc_client_draw_capture_world(mc_client_draw_scope *scope, mc_world_runtime *world,
                                  mc_client_manager_error *e) {
    if (!scope_valid(scope, e))
        return false;
    if (world == NULL || find_owner(scope->owner, MANAGER_DRAW_WORLD, world) != NULL)
        return true;
    manager_draw_owner *p = owner_slot(scope->owner, e);
    if (p == NULL)
        return false;
    if (!mc_world_runtime_binding_retain(world))
        return owner_fail(e,
                          world->persistent_bindings == SIZE_MAX ? MC_CLIENT_MANAGER_LIMIT
                                                                 : MC_CLIENT_MANAGER_WORLD_FAILED,
                          "Draw actual World occurrence");
    p->kind = MANAGER_DRAW_WORLD;
    p->value.world = world;
    publish_owner(scope->owner);
    return true;
}

bool mc_client_draw_adopt_world(mc_client_draw_scope *scope, mc_world_runtime **slot,
                                mc_client_manager_error *e) {
    if (!scope_valid(scope, e))
        return false;
    if (slot == NULL)
        return owner_fail(e, MC_CLIENT_MANAGER_INVALID_ARGUMENT, "Draw World adoption slot");
    mc_world_runtime *world = *slot;
    if (world == NULL)
        return true;
    if (world->persistent_bindings == 0)
        return owner_fail(e, MC_CLIENT_MANAGER_INVALID_PHASE, "Draw World adoption occurrence");
    if (find_owner(scope->owner, MANAGER_DRAW_WORLD, world) != NULL) {
        if (world->persistent_bindings < 2u)
            return owner_fail(e, MC_CLIENT_MANAGER_INVALID_PHASE,
                              "Draw duplicate World adoption occurrence invariant");
        /* Consume the extra actual occurrence without admitting a retired World. */
        mc_world_runtime_binding_release(world);
        *slot = NULL;
        return true;
    }
    manager_draw_owner *p = owner_slot(scope->owner, e);
    if (p == NULL)
        return false;
    p->kind = MANAGER_DRAW_WORLD;
    p->value.world = world;
    publish_owner(scope->owner);
    *slot = NULL;
    return true;
}

bool mc_client_draw_capture_engine(mc_client_draw_scope *scope, mc_client_render_engine *engine,
                                   mc_client_manager_error *e) {
    if (!scope_valid(scope, e))
        return false;
    if (engine == NULL || find_owner(scope->owner, MANAGER_DRAW_ENGINE, engine) != NULL)
        return true;
    manager_draw_owner *p = owner_slot(scope->owner, e);
    if (p == NULL)
        return false;
    e->engine_status = mc_client_render_engine_retain(engine);
    if (e->engine_status != MC_CLIENT_RENDER_ENGINE_OK)
        return owner_fail(e,
                          e->engine_status == MC_CLIENT_RENDER_ENGINE_LIMIT
                              ? MC_CLIENT_MANAGER_LIMIT
                              : MC_CLIENT_MANAGER_ENGINE_FAILED,
                          "Draw actual Engine occurrence");
    p->kind = MANAGER_DRAW_ENGINE;
    p->value.engine = engine;
    publish_owner(scope->owner);
    return true;
}

bool mc_client_draw_capture_string(mc_client_draw_scope *scope, const mc_player_string *string,
                                   mc_client_manager_error *e) {
    if (!scope_valid(scope, e))
        return false;
    if (string == NULL || find_owner(scope->owner, MANAGER_DRAW_STRING, string) != NULL)
        return true;
    manager_draw_owner *p = owner_slot(scope->owner, e);
    if (p == NULL)
        return false;
    e->string = mc_player_string_retain((mc_player_string *)string);
    if (e->string != MC_PLAYER_OK)
        return owner_fail(e, MC_CLIENT_MANAGER_STRING_FAILED, "Draw actual String occurrence");
    p->kind = MANAGER_DRAW_STRING;
    p->value.string = string;
    publish_owner(scope->owner);
    return true;
}

bool mc_client_draw_capture_settings(mc_client_draw_scope *scope, mc_game_settings *settings,
                                     mc_client_manager_error *e) {
    if (!scope_valid(scope, e))
        return false;
    if (settings == NULL || find_owner(scope->owner, MANAGER_DRAW_SETTINGS, settings) != NULL)
        return true;
    manager_draw_owner *p = owner_slot(scope->owner, e);
    if (p == NULL)
        return false;
    if (mc_game_settings_borrow(settings, &e->settings) != MC_GAME_SETTINGS_OK)
        return owner_fail(e,
                          e->settings.status == MC_GAME_SETTINGS_LIMIT
                              ? MC_CLIENT_MANAGER_LIMIT
                              : MC_CLIENT_MANAGER_SETTINGS_FAILED,
                          "Draw actual Settings occurrence");
    p->kind = MANAGER_DRAW_SETTINGS;
    p->value.settings = settings;
    publish_owner(scope->owner);
    return true;
}

bool manager_draw_capture_actor(mc_client_render_manager_draw_call *c, mc_entity *actor,
                                mc_client_manager_error *e) {
    manager_error_init(e);
    if (c == NULL)
        return owner_fail(e, MC_CLIENT_MANAGER_INVALID_ARGUMENT, "Draw actual actor capture call");
    if (actor == NULL)
        return true;
    if (actor->classes != c->manager_hold->classes->models->entities)
        return owner_fail(e, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                          "Draw Entity belongs to a different native class namespace");
    if (c->actor_hold != NULL)
        return owner_fail(e, MC_CLIENT_MANAGER_INVALID_PHASE,
                          "Draw actual actor occurrence changed");
    e->entity_status = mc_entity_retain(actor);
    if (e->entity_status != MC_ENTITY_OK)
        return owner_fail(e, MC_CLIENT_MANAGER_ENTITY_FAILED, "Draw actual Entity occurrence");
    c->actor_hold = actor;
    return true;
}

bool manager_draw_capture_renderer(mc_client_render_manager_draw_call *c,
                                   mc_client_managed_renderer *renderer,
                                   mc_client_manager_error *e) {
    manager_error_init(e);
    if (c == NULL || c->manager_hold == NULL)
        return owner_fail(e, MC_CLIENT_MANAGER_INVALID_ARGUMENT, "Draw renderer capture owner");
    if (renderer == NULL || c->renderer_hold == renderer)
        return true;
    if (c->renderer_hold != NULL || renderer->parent != c->manager_hold)
        return owner_fail(e, MC_CLIENT_MANAGER_INVALID_PHASE, "Draw renderer native association");
    bool found = false;
    if (c->manager_hold->allocated_renderers >
        sizeof(c->manager_hold->renderers) / sizeof(c->manager_hold->renderers[0]))
        return owner_fail(e, MC_CLIENT_MANAGER_INVALID_PHASE,
                          "Draw renderer ledger extent invariant");
    for (size_t i = 0; i < c->manager_hold->allocated_renderers; ++i)
        if (c->manager_hold->renderers[i] == renderer) {
            found = true;
            break;
        }
    if (!found || renderer->phase != MC_CLIENT_MODEL_READY || renderer->active ||
        renderer->holds == 0)
        return owner_fail(e, MC_CLIENT_MANAGER_INVALID_PHASE, "Draw actual renderer provenance");
    if (renderer->holds == SIZE_MAX)
        return owner_fail(e, MC_CLIENT_MANAGER_LIMIT, "Draw actual renderer occurrence ceiling");
    ++renderer->holds;
    c->renderer_hold = renderer;
    return true;
}

bool manager_draw_capture_math(mc_client_render_manager_draw_call *c, mc_client_manager_error *e) {
    manager_error_init(e);
    if (c->math_hold != NULL || c->services.math == NULL)
        return true;
    if (mc_math_classes_retain(c->services.math, &e->math) != MC_MATH_CLASS_OK)
        return owner_fail(e,
                          e->math.status == MC_MATH_CLASS_LIMIT ? MC_CLIENT_MANAGER_LIMIT
                                                                : MC_CLIENT_MANAGER_MATH_FAILED,
                          "Draw actual reached Math namespace occurrence");
    c->math_hold = c->services.math;
    return true;
}

bool manager_draw_capture_graphics(mc_client_render_manager_draw_call *c,
                                   mc_client_manager_error *e) {
    manager_error_init(e);
    if (c->graphics_hold != NULL || c->services.graphics == NULL)
        return true;
    e->engine_status = mc_client_render_engine_classes_retain(c->services.graphics);
    if (e->engine_status != MC_CLIENT_RENDER_ENGINE_OK)
        return owner_fail(e,
                          e->engine_status == MC_CLIENT_RENDER_ENGINE_LIMIT
                              ? MC_CLIENT_MANAGER_LIMIT
                              : MC_CLIENT_MANAGER_ENGINE_FAILED,
                          "Draw actual reached graphics namespace occurrence");
    c->graphics_hold = c->services.graphics;
    return true;
}

bool mc_client_draw_capture_math(mc_client_draw_scope *scope, mc_client_manager_error *e) {
    return scope_valid(scope, e) && manager_draw_capture_math(scope->owner, e);
}

bool mc_client_draw_capture_graphics(mc_client_draw_scope *scope, mc_client_manager_error *e) {
    return scope_valid(scope, e) && manager_draw_capture_graphics(scope->owner, e);
}

bool mc_client_draw_pending_lookup(mc_client_draw_scope *scope, mc_client_render_engine *engine,
                                   bool pending_on_entry, mc_client_manager_error *e) {
    if (!scope_valid(scope, e))
        return false;
    if (pending_on_entry)
        return owner_fail(e, MC_CLIENT_MANAGER_BUSY, "Draw foreign pending lookup is not owned");
    manager_draw_owner *p = find_owner(scope->owner, MANAGER_DRAW_ENGINE, engine);
    if (engine == NULL || p == NULL || engine->lookup == NULL || engine->lookup->pending == NULL)
        return owner_fail(e, MC_CLIENT_MANAGER_INVALID_PHASE, "Draw actual new lookup occurrence");
    const void *identity = engine->lookup->pending;
    if (p->pending_lookup != NULL && p->pending_lookup != identity)
        return owner_fail(e, MC_CLIENT_MANAGER_BUSY, "Draw pending lookup identity changed");
    p->pending_lookup = identity;
    return true;
}

bool mc_client_draw_pending_list(mc_client_draw_scope *scope,
                                 const mc_client_model_draw_error *part,
                                 mc_client_manager_error *e) {
    if (!scope_valid(scope, e))
        return false;
    if (part == NULL || !part->pending_list)
        return owner_fail(e, MC_CLIENT_MANAGER_INVALID_ARGUMENT, "Draw actual PS pending right");
    const void *identity = NULL;
    if (!mc_client_display_pending_list_identity(scope->model.display, part->owned_list, &identity))
        return owner_fail(e, MC_CLIENT_MANAGER_INVALID_PHASE, "Draw actual pending list identity");
    mc_client_render_manager_draw_call *c = scope->owner;
    if (c->pending_list &&
        (c->owned_list != part->owned_list || c->list_display != scope->model.display ||
         c->list_identity != identity))
        return owner_fail(e, MC_CLIENT_MANAGER_BUSY, "Draw foreign pending list occurrence");
    c->pending_list = true;
    c->owned_list = part->owned_list;
    c->list_display = scope->model.display;
    c->list_identity = identity;
    return true;
}

bool mc_client_draw_query_begin(mc_client_draw_scope *scope,
                                const mc_client_manager_draw_query *query, size_t *index,
                                mc_client_manager_error *e) {
    if (!scope_valid(scope, e))
        return false;
    if (query == NULL || index == NULL || query->nargs > 6u)
        return owner_fail(e, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                          "Draw direct query receipt arguments");
    mc_client_render_manager_draw_call *c = scope->owner;
    if (c->query_count == c->limits.max_queries)
        return owner_fail(e, MC_CLIENT_MANAGER_LIMIT, "Draw direct query receipt ceiling");
    if (c->query_count == c->query_capacity) {
        size_t capacity = c->query_capacity == 0 ? 16u : c->query_capacity;
        if (capacity > SIZE_MAX / 2u)
            capacity = c->limits.max_queries;
        else if (c->query_capacity != 0)
            capacity *= 2u;
        if (capacity > c->limits.max_queries)
            capacity = c->limits.max_queries;
        if (capacity <= c->query_capacity || capacity > SIZE_MAX / sizeof(*c->queries))
            return owner_fail(e, MC_CLIENT_MANAGER_LIMIT, "Draw direct query receipt array extent");
        mc_client_manager_draw_query *grown =
            manager_draw_allocate_bytes(c, capacity * sizeof(*grown), e);
        if (grown == NULL)
            return false;
        if (c->query_count != 0)
            memcpy(grown, c->queries, c->query_count * sizeof(*grown));
        manager_draw_free_bytes(c, c->queries, c->query_capacity * sizeof(*grown));
        c->queries = grown;
        c->query_capacity = capacity;
    }
    *index = c->query_count;
    c->queries[c->query_count++] = *query;
    return true;
}

void mc_client_draw_query_enter(mc_client_draw_scope *scope, size_t index) {
    if (scope != NULL && scope->owner != NULL && &scope->owner->scope == scope &&
        index < scope->owner->query_count)
        scope->owner->queries[index].invoked = true;
}

void mc_client_draw_query_end(mc_client_draw_scope *scope, size_t index, bool invoked,
                              bool returned, uint32_t bits, bool native_policy,
                              bool source_exception, const char *exception_class) {
    if (scope == NULL || scope->owner == NULL || &scope->owner->scope != scope ||
        index >= scope->owner->query_count)
        return;
    mc_client_manager_draw_query *q = &scope->owner->queries[index];
    q->invoked = invoked;
    q->returned = returned;
    q->failed = !returned;
    q->native_policy = native_policy;
    q->source_exception = source_exception;
    q->exception_class = exception_class;
    if (!returned)
        return;
    switch (q->kind) {
        case MC_CLIENT_MANAGER_DRAW_QUERY_INTEGER:
            memcpy(&q->value.integer, &bits, sizeof(bits));
            break;
        case MC_CLIENT_MANAGER_DRAW_QUERY_BOOLEAN:
            q->value.boolean = bits != 0;
            break;
        case MC_CLIENT_MANAGER_DRAW_QUERY_FLOAT_BITS:
            q->value.float_bits = bits;
            break;
    }
}

bool manager_draw_append_entity_queries(mc_client_render_manager_draw_call *c,
                                        mc_entity_render_error *v, mc_client_manager_error *e) {
    manager_error_init(e);
    if (c == NULL || v == NULL || v->query_count > 2u || v->published_query_count > v->query_count)
        return owner_fail(e, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                          "Draw actual Entity query receipt extent");
    while (v->published_query_count < v->query_count) {
        const mc_entity_render_query *source = &v->queries[v->published_query_count];
        mc_client_manager_draw_query q = {
            .receiver = source->receiver,
            .method = {source->method.class_name, source->method.method, source->method.descriptor,
                       source->method.pc},
            .caller = {source->caller.class_name, source->caller.method, source->caller.descriptor,
                       source->caller.pc},
            .nargs = source->nargs,
            .kind = source->kind == MC_ENTITY_RENDER_QUERY_BOOLEAN
                        ? MC_CLIENT_MANAGER_DRAW_QUERY_BOOLEAN
                        : MC_CLIENT_MANAGER_DRAW_QUERY_FLOAT_BITS,
            .invoked = source->invoked,
            .returned = source->returned,
            .failed = source->failed,
            .native_policy = source->native_policy,
            .source_exception = source->source_exception,
            .exception_class = source->exception_class};
        memcpy(q.args, source->args, sizeof(q.args));
        if (source->kind == MC_ENTITY_RENDER_QUERY_BOOLEAN)
            q.value.boolean = source->value.boolean;
        else
            q.value.float_bits = source->value.float_bits;
        size_t index;
        if (!mc_client_draw_query_begin(&c->scope, &q, &index, e))
            return false;
        ++v->published_query_count;
    }
    return true;
}

bool mc_client_draw_entity_queries(mc_client_draw_scope *scope, mc_entity_render_error *v,
                                   mc_client_manager_error *e) {
    if (!scope_valid(scope, e))
        return false;
    return manager_draw_append_entity_queries(scope->owner, v, e);
}

size_t mc_client_render_manager_draw_query_count(const mc_client_render_manager_draw_call *c) {
    return c == NULL ? 0u : c->query_count;
}

bool mc_client_render_manager_draw_query_at(const mc_client_render_manager_draw_call *c,
                                            size_t index, mc_client_manager_draw_query *out) {
    if (c == NULL || out == NULL || index >= c->query_count)
        return false;
    *out = c->queries[index];
    return true;
}

static bool capture_application(mc_client_render_manager_draw_call *c, client_application *app,
                                mc_client_manager_error *e) {
    if (find_owner(c, MANAGER_DRAW_APPLICATION, app) != NULL)
        return true;
    if (app->source_namespace != c->application_classes_hold)
        return owner_fail(e, MC_CLIENT_MANAGER_INVALID_PHASE, "Draw canonical Minecraft namespace");
    manager_draw_owner *p = owner_slot(c, e);
    if (p == NULL)
        return false;
    const mc_client_application_owner_status status =
        mc_client_application_render_borrow(app, &c->application_error);
    if (status != MC_CLIENT_APPLICATION_OWNER_OK)
        return owner_fail(e,
                          status == MC_CLIENT_APPLICATION_OWNER_LIMIT
                              ? MC_CLIENT_MANAGER_LIMIT
                              : MC_CLIENT_MANAGER_INVALID_PHASE,
                          "Draw actual registered Minecraft render borrow");
    p->kind = MANAGER_DRAW_APPLICATION;
    p->value.application = app;
    publish_owner(c);
    return true;
}

static bool debug_fail(mc_client_manager_error *e, mc_client_manager_status status,
                       const char *operation, int32_t pc, bool native) {
    (void)manager_fail(e, status, operation, "net/minecraft/client/Minecraft", "w", "()Z", pc,
                       native);
    return false;
}

bool mc_client_draw_debug_flag(mc_client_draw_scope *scope, bool *out, mc_client_manager_error *e) {
    if (!scope_valid(scope, e))
        return false;
    if (out == NULL)
        return owner_fail(e, MC_CLIENT_MANAGER_INVALID_ARGUMENT, "Draw debug scalar output");
    mc_client_render_manager_draw_call *c = scope->owner;
    mc_client_application_classes *classes = scope->application_classes;
    if (classes == NULL)
        return debug_fail(e, MC_CLIENT_MANAGER_UNIMPLEMENTED_DEPENDENCY,
                          "Draw reached canonical Minecraft namespace", 0, true);
    if (c->application_classes_hold == NULL) {
        const mc_client_application_owner_status status =
            mc_client_application_classes_retain(classes);
        if (status != MC_CLIENT_APPLICATION_OWNER_OK)
            return debug_fail(e,
                              status == MC_CLIENT_APPLICATION_OWNER_LIMIT
                                  ? MC_CLIENT_MANAGER_LIMIT
                                  : MC_CLIENT_MANAGER_INVALID_PHASE,
                              "Draw actual Minecraft class occurrence", 0, true);
        c->application_classes_hold = classes;
    }
    const mc_client_application_owner_status activated =
        mc_client_application_classes_active_use(classes, &c->application_error);
    if (activated != MC_CLIENT_APPLICATION_OWNER_OK)
        return debug_fail(e,
                          activated == MC_CLIENT_APPLICATION_OWNER_LIMIT ? MC_CLIENT_MANAGER_LIMIT
                          : activated == MC_CLIENT_APPLICATION_OWNER_ALLOCATION_FAILED
                              ? MC_CLIENT_MANAGER_ALLOCATION_FAILED
                              : MC_CLIENT_MANAGER_CLASS_ERRONEOUS,
                          "Draw actual Minecraft class active use", 0, true);
    mc_client_application_class_state state;
    if (!mc_client_application_classes_inspect(classes, &state))
        return debug_fail(e, MC_CLIENT_MANAGER_BUSY, "Draw canonical Minecraft first static read",
                          0, true);
    if (state.a == NULL) {
        *out = false;
        return true;
    }
    /* Actual second getstatic6, not the nonnull test's captured App. */
    if (!mc_client_application_classes_inspect(classes, &state))
        return debug_fail(e, MC_CLIENT_MANAGER_BUSY, "Draw canonical Minecraft fresh static read",
                          6, true);
    client_application *app = state.a;
    if (app == NULL)
        return debug_fail(e, MC_CLIENT_MANAGER_NULL_REFERENCE, "Draw debug null fresh Minecraft", 9,
                          false);
    if (!capture_application(c, app, e))
        return false;
    mc_game_settings *settings = app->settings; /* Source z9. */
    if (!mc_client_draw_capture_settings(scope, settings, e))
        return false;
    if (settings == NULL)
        return debug_fail(e, MC_CLIENT_MANAGER_NULL_REFERENCE, "Draw debug null Settings", 12,
                          false);
    mc_game_settings_state values;
    if (!mc_game_settings_read_state(settings, &values))
        return debug_fail(e, MC_CLIENT_MANAGER_SETTINGS_FAILED, "Draw actual debug Settings fields",
                          12, true);
    *out = values.values.show_debug_info;
    return true;
}

static bool capture_receipt(mc_client_render_manager_draw_call *c, const mc_world_access_failure *v,
                            mc_client_manager_error *e) {
    if (v->receipt == NULL)
        return true;
    for (size_t i = 0; i < c->receipt_count; ++i)
        if (c->receipts[i].receipt == v->receipt)
            return true;
    if (c->receipt_count == sizeof(c->receipts) / sizeof(c->receipts[0]))
        return owner_fail(e, MC_CLIENT_MANAGER_LIMIT, "Draw fixed callback receipt extent");
    const mc_world_storage_status status =
        mc_world_access_failure_retain(v, &c->receipts[c->receipt_count]);
    if (status != MC_WORLD_STORAGE_OK)
        return owner_fail(e,
                          status == MC_WORLD_STORAGE_LIMIT ? MC_CLIENT_MANAGER_LIMIT
                                                           : MC_CLIENT_MANAGER_WORLD_FAILED,
                          "Draw actual callback receipt occurrence");
    ++c->receipt_count;
    return true;
}

static bool copy_source_chunk(mc_client_render_manager_draw_call *c,
                              const mc_world_source_chunk_error **slot,
                              mc_client_manager_error *e) {
    const mc_world_source_chunk_error *source = *slot;
    if (source == NULL)
        return true;
    for (size_t i = 0; i < c->source_chunk_count; ++i)
        if (source == &c->source_chunks[i] || source == c->source_chunk_identities[i]) {
            *slot = &c->source_chunks[i];
            return true;
        }
    if (c->source_chunk_count == sizeof(c->source_chunks) / sizeof(c->source_chunks[0]))
        return owner_fail(e, MC_CLIENT_MANAGER_LIMIT, "Draw fixed source loader cause extent");
    const size_t i = c->source_chunk_count++;
    c->source_chunks[i] = *source;
    c->source_chunk_identities[i] = source;
    *slot = &c->source_chunks[i];
    return true;
}

static bool capture_acquisition(mc_client_render_manager_draw_call *c,
                                mc_world_runtime_acquisition_error *v, mc_client_manager_error *e) {
    return copy_source_chunk(c, &v->source_chunk, e) &&
           capture_receipt(c, &v->batch.publication.world_access_failure, e) &&
           capture_receipt(c, &v->tile_entity.world_access_failure, e) &&
           capture_receipt(c, &v->tile_entity.publication.world_access_failure, e) &&
           capture_receipt(c, &v->tile_entity.batch.publication.world_access_failure, e);
}

static bool capture_entity_children(mc_client_render_manager_draw_call *c,
                                    mc_entity_render_error *v, mc_client_manager_error *e) {
    if (v->selected_child == MC_ENTITY_RENDER_CHILD_BRIGHTNESS)
        return copy_source_chunk(c, &v->brightness.query.source_chunk, e);
    /* Area presence itself performs no loading or WorldAccess callbacks. */
    if (v->selected_child == MC_ENTITY_RENDER_CHILD_AREA)
        return copy_source_chunk(c, &v->area.query.source_chunk, e) &&
               capture_receipt(c, &v->area.world_access_failure, e);
    return true;
}

static bool capture_detail(mc_client_render_manager_draw_call *c, mc_client_living_draw_child kind,
                           mc_client_living_draw_detail *v, mc_client_manager_error *e) {
    switch (kind) {
        case MC_CLIENT_LIVING_DRAW_CHILD_ENTITY:
            return capture_entity_children(c, &v->entity, e);
        case MC_CLIENT_LIVING_DRAW_CHILD_QUERY:
            return copy_source_chunk(c, &v->query.source_chunk, e);
        case MC_CLIENT_LIVING_DRAW_CHILD_BLOCK:
            return capture_acquisition(c, &v->block.acquisition, e);
        case MC_CLIENT_LIVING_DRAW_CHILD_BRIGHTNESS:
            return copy_source_chunk(c, &v->brightness.query.source_chunk, e);
        default:
            return true;
    }
}

bool manager_draw_capture_children(mc_client_render_manager_draw_call *c,
                                   mc_client_manager_error *e) {
    manager_error_init(e);
    if (c == NULL)
        return owner_fail(e, MC_CLIENT_MANAGER_INVALID_ARGUMENT,
                          "Draw retained child capture owner");
    return capture_entity_children(c, &c->brightness, e) &&
           capture_detail(c, c->living.selected_child, &c->living.detail, e) &&
           (!c->living.caught.active ||
            capture_detail(c, c->living.caught.selected_child, &c->living.caught.detail, e)) &&
           capture_detail(c, c->post.selected_child, &c->post.detail, e) &&
           (!c->post.caught.active ||
            capture_detail(c, c->post.caught.selected_child, &c->post.caught.detail, e));
}

static mc_client_manager_status cleanup_failed(mc_client_render_manager_draw_call *c,
                                               mc_client_manager_status status,
                                               const char *operation,
                                               const mc_client_manager_error *owner,
                                               mc_client_manager_draw_error *out) {
    if (c->cleanup.status == MC_CLIENT_MANAGER_OK) {
        c->cleanup.status = status;
        c->cleanup.operation = operation;
        c->cleanup.native_policy = true;
        c->first_lookup_cleanup = c->lookup_cleanup;
        c->first_list_cleanup = c->list_cleanup;
        c->first_application_cleanup = c->application_cleanup;
        if (c->list_cleanup.status != MC_CLIENT_DISPLAY_OK)
            c->cleanup.graphics.display = c->list_cleanup;
        if (owner != NULL && owner != &c->cleanup.owner)
            c->cleanup.owner = *owner;
    }
    if (out != NULL)
        *out = c->cleanup;
    return c->cleanup.status;
}

static bool release_neutral_worlds(mc_client_render_manager_draw_call *c) {
    if (mc_entity_render_worlds_release(&c->brightness) != MC_ENTITY_OK)
        return false;
    mc_client_living_draw_error *leaves[] = {&c->living, &c->post};
    for (size_t i = 0; i < 2; ++i) {
        mc_client_living_draw_error *v = leaves[i];
        if (v->selected_child == MC_CLIENT_LIVING_DRAW_CHILD_ENTITY &&
            mc_entity_render_worlds_release(&v->detail.entity) != MC_ENTITY_OK)
            return false;
        if (v->caught.active && v->caught.selected_child == MC_CLIENT_LIVING_DRAW_CHILD_ENTITY &&
            mc_entity_render_worlds_release(&v->caught.detail.entity) != MC_ENTITY_OK)
            return false;
    }
    return true;
}

static bool release_owner(mc_client_render_manager_draw_call *c, manager_draw_owner *p,
                          mc_client_manager_error *e) {
    manager_error_init(e);
    switch (p->kind) {
        case MANAGER_DRAW_WORLD:
            if (p->value.world != NULL) {
                if (p->value.world->persistent_bindings == 0)
                    return owner_fail(e, MC_CLIENT_MANAGER_INVALID_PHASE,
                                      "Draw World retirement occurrence invariant");
                mc_world_runtime_binding_release(p->value.world);
                p->value.world = NULL;
            }
            break;
        case MANAGER_DRAW_ENGINE:
            if (p->value.engine != NULL) {
                e->engine_status = mc_client_render_engine_release(p->value.engine);
                if (e->engine_status != MC_CLIENT_RENDER_ENGINE_OK)
                    return owner_fail(e,
                                      e->engine_status == MC_CLIENT_RENDER_ENGINE_BUSY
                                          ? MC_CLIENT_MANAGER_BUSY
                                          : MC_CLIENT_MANAGER_ENGINE_FAILED,
                                      "Draw retained Engine release");
                p->value.engine = NULL;
            }
            break;
        case MANAGER_DRAW_STRING:
            mc_player_string_release((mc_player_string *)p->value.string);
            p->value.string = NULL;
            break;
        case MANAGER_DRAW_SETTINGS:
            if (mc_game_settings_release_borrow(&p->value.settings, &e->settings) !=
                MC_GAME_SETTINGS_OK)
                return owner_fail(e,
                                  e->settings.status == MC_GAME_SETTINGS_BUSY
                                      ? MC_CLIENT_MANAGER_BUSY
                                      : MC_CLIENT_MANAGER_SETTINGS_FAILED,
                                  "Draw retained Settings release");
            break;
        case MANAGER_DRAW_APPLICATION: {
            const mc_client_application_owner_status status = mc_client_application_render_release(
                &p->value.application, &c->application_cleanup);
            if (status != MC_CLIENT_APPLICATION_OWNER_OK)
                return owner_fail(e,
                                  status == MC_CLIENT_APPLICATION_OWNER_BUSY
                                      ? MC_CLIENT_MANAGER_BUSY
                                      : MC_CLIENT_MANAGER_INVALID_PHASE,
                                  "Draw retained Minecraft render release");
            break;
        }
        default:
            return owner_fail(e, MC_CLIENT_MANAGER_INVALID_PHASE,
                              "Draw owner ledger tag invariant");
    }
    return true;
}

mc_client_manager_status
mc_client_render_manager_draw_retire(mc_client_render_manager_draw_call **slot,
                                     mc_client_manager_draw_error *out) {
    manager_draw_error_init(out);
    if (slot == NULL) {
        if (out != NULL) {
            out->status = MC_CLIENT_MANAGER_INVALID_ARGUMENT;
            out->operation = "Draw native retirement slot";
            out->native_policy = true;
        }
        return MC_CLIENT_MANAGER_INVALID_ARGUMENT;
    }
    mc_client_render_manager_draw_call *c = *slot;
    if (c == NULL)
        return MC_CLIENT_MANAGER_OK;
    mc_client_render_manager *m = c->manager_hold;
    if (m == NULL || m->draw_call != c || m->holds <= 1 || c->owner_count > c->owner_capacity ||
        c->owner_capacity > c->limits.max_owners ||
        (c->renderer_hold != NULL && c->renderer_hold->holds <= 1))
        return cleanup_failed(c, MC_CLIENT_MANAGER_INVALID_PHASE,
                              "Draw retained native occurrence invariant", NULL, out);
    if (c->executing || c->state.active || m->active || m->classes->active ||
        (c->renderer_hold != NULL && c->renderer_hold->active))
        return cleanup_failed(c, MC_CLIENT_MANAGER_BUSY, "Draw source owner still active", NULL,
                              out);
    c->retiring = c->state.retiring = true;
    /* No allocation or source replay. Earlier captures survive any refusal. */
    if (!manager_draw_capture_children(c, &c->owner_work))
        return cleanup_failed(c, c->owner_work.status, c->owner_work.operation, &c->owner_work,
                              out);
    if (!release_neutral_worlds(c))
        return cleanup_failed(c, MC_CLIENT_MANAGER_INVALID_PHASE,
                              "Draw unadopted World occurrence retirement", NULL, out);
    if (c->pending_list) {
        const void *identity = NULL;
        if (!mc_client_display_pending_list_identity(c->list_display, c->owned_list, &identity) ||
            identity != c->list_identity)
            return cleanup_failed(c, MC_CLIENT_MANAGER_BUSY,
                                  "Draw pending list changed; foreign definition preserved", NULL,
                                  out);
        if (!mc_client_display_lists_abort(c->list_display, c->owned_list, &c->list_cleanup))
            return cleanup_failed(c,
                                  c->list_cleanup.status == MC_CLIENT_DISPLAY_BUSY
                                      ? MC_CLIENT_MANAGER_BUSY
                                      : MC_CLIENT_MANAGER_MODEL_FAILED,
                                  "Draw native owned list abort", NULL, out);
        c->pending_list = false;
        c->list_identity = NULL;
        c->list_display = NULL;
    }
    for (size_t i = 0; i < c->owner_count; ++i) {
        manager_draw_owner *p = &c->owners[i];
        if (p->kind != MANAGER_DRAW_ENGINE || p->pending_lookup == NULL)
            continue;
        mc_client_render_engine *engine = p->value.engine;
        if (engine == NULL || engine->lookup == NULL ||
            engine->lookup->pending != p->pending_lookup)
            return cleanup_failed(c, MC_CLIENT_MANAGER_BUSY,
                                  "Draw pending lookup changed; foreign attempt preserved", NULL,
                                  out);
        const mc_client_texture_lookup_status status =
            mc_client_render_engine_retire_lookup(engine, &c->lookup_cleanup);
        if (engine->lookup->pending == NULL)
            p->pending_lookup = NULL; /* Consumed even if final native close reported failure. */
        if (status != MC_CLIENT_LOOKUP_OK)
            return cleanup_failed(c,
                                  status == MC_CLIENT_LOOKUP_BUSY ? MC_CLIENT_MANAGER_BUSY
                                                                  : MC_CLIENT_MANAGER_ENGINE_FAILED,
                                  "Draw owned lookup prefix retirement", NULL, out);
        if (p->pending_lookup != NULL)
            return cleanup_failed(c, MC_CLIENT_MANAGER_ENGINE_FAILED,
                                  "Draw lookup cleanup returned with owned prefix", NULL, out);
    }
    while (c->owner_count != 0) {
        manager_draw_owner *p = &c->owners[c->owner_count - 1u];
        if (!release_owner(c, p, &c->owner_work))
            return cleanup_failed(c, c->owner_work.status, c->owner_work.operation, &c->owner_work,
                                  out);
        --c->owner_count;
        c->state.owners = c->owner_count;
    }
    if (c->application_classes_hold != NULL) {
        const mc_client_application_owner_status status = mc_client_application_classes_release(
            &c->application_classes_hold, &c->application_cleanup);
        if (status != MC_CLIENT_APPLICATION_OWNER_OK)
            return cleanup_failed(c,
                                  status == MC_CLIENT_APPLICATION_OWNER_BUSY
                                      ? MC_CLIENT_MANAGER_BUSY
                                      : MC_CLIENT_MANAGER_INVALID_PHASE,
                                  "Draw retained Minecraft class release", NULL, out);
    }
    if (c->math_hold != NULL &&
        mc_math_classes_release(&c->math_hold, &c->owner_work.math) != MC_MATH_CLASS_OK)
        return cleanup_failed(c,
                              c->owner_work.math.status == MC_MATH_CLASS_BUSY
                                  ? MC_CLIENT_MANAGER_BUSY
                                  : MC_CLIENT_MANAGER_MATH_FAILED,
                              "Draw retained Math namespace release", &c->owner_work, out);
    if (c->graphics_hold != NULL) {
        c->owner_work.engine_status = mc_client_render_engine_classes_release(c->graphics_hold);
        if (c->owner_work.engine_status != MC_CLIENT_RENDER_ENGINE_OK)
            return cleanup_failed(c,
                                  c->owner_work.engine_status == MC_CLIENT_RENDER_ENGINE_BUSY
                                      ? MC_CLIENT_MANAGER_BUSY
                                      : MC_CLIENT_MANAGER_ENGINE_FAILED,
                                  "Draw retained graphics namespace release", &c->owner_work, out);
        c->graphics_hold = NULL;
    }
    /* All fallible external cleanup is complete before source identity holds
     * are consumed. Their private-count proof above admits these final drops. */
    if (c->renderer_hold != NULL && mc_client_managed_renderer_release(
                                        &c->renderer_hold, &c->owner_work) != MC_CLIENT_MANAGER_OK)
        return cleanup_failed(c, c->owner_work.status, c->owner_work.operation, &c->owner_work,
                              out);
    for (size_t i = 0; i < c->receipt_count; ++i)
        mc_world_access_failure_release(&c->receipts[i]);
    c->receipt_count = 0;
    mc_entity_release(c->actor_hold);
    c->actor_hold = NULL;
    manager_draw_free_bytes(c, c->queries, c->query_capacity * sizeof(*c->queries));
    c->queries = NULL;
    c->query_count = c->query_capacity = 0;
    manager_draw_free_bytes(c, c->owners, c->owner_capacity * sizeof(*c->owners));
    c->owners = NULL;
    c->owner_capacity = 0;
    mc_client_render_manager_classes *classes = m->classes;
    const mc_client_manager_status status =
        mc_client_render_manager_release(&c->manager_hold, &c->owner_work);
    if (status != MC_CLIENT_MANAGER_OK)
        return cleanup_failed(c, status, c->owner_work.operation, &c->owner_work, out);
    m->draw_call = NULL;
    classes->bytes -= sizeof(*c);
    free(c);
    *slot = NULL;
    manager_draw_error_init(out);
    return MC_CLIENT_MANAGER_OK;
}
