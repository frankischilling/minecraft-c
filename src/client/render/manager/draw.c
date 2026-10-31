#include "../../../entity/entity_internal.h"
#include "../interpolation_internal.h"
#include "draw_internal.h"

#include <stdint.h>
#include <string.h>

static const char draw_descriptor[] = "(Lsn;F)V";
static const char coordinates_descriptor[] = "(Lsn;DDDFF)V";

static void at(mc_client_render_manager_draw_call *c, const char *descriptor, int32_t pc) {
    c->state.source_pc = (unsigned int)pc;
    c->error.site = (mc_client_graphics_site){"th", "a", descriptor, pc};
}

static bool fail(mc_client_render_manager_draw_call *c, mc_client_manager_status status,
                 const char *operation, bool native) {
    c->error.status = status;
    c->error.operation = operation;
    c->error.native_policy = native;
    return false;
}

static bool source_null(mc_client_render_manager_draw_call *c, const char *operation) {
    c->error.source_exception = true;
    c->error.exception_class = "java/lang/NullPointerException";
    return fail(c, MC_CLIENT_MANAGER_NULL_REFERENCE, operation, false);
}

static bool native_owner(mc_client_render_manager_draw_call *c,
                         const mc_client_manager_error *error) {
    c->error.owner = *error;
    c->error.source_exception = false;
    c->error.exception_class = NULL;
    c->error.unclassified_throwable = false;
    return fail(c, error->status, error->operation, true);
}

static bool arithmetic(mc_client_render_manager_draw_call *c,
                       const mc_client_interpolation_error *error) {
    at(c, draw_descriptor, error->pc);
    return fail(c,
                error->status == MC_CLIENT_INTERPOLATION_UNSUPPORTED_FP
                    ? MC_CLIENT_MANAGER_UNSUPPORTED_FP
                    : MC_CLIENT_MANAGER_UNSUPPORTED_NUMERIC,
                "TH strict interpolation operands", true);
}

static bool subtract(double value, double origin, int32_t pc, double *out,
                     mc_client_interpolation_error *error) {
    *error = (mc_client_interpolation_error){MC_CLIENT_INTERPOLATION_OK, -1};
    if (!mc_client_interpolation_fp(error, pc) ||
        !mc_client_interpolation_double_nan_admit(value, origin, pc, error))
        return false;
    *out = mc_client_interpolation_dsub(value, origin);
    return true;
}

static bool brightness(mc_client_render_manager_draw_call *c) {
    at(c, draw_descriptor, 76);
    if (!manager_draw_capture_math(c, &c->owner_work))
        return native_owner(c, &c->owner_work);
    const bool returned = mc_entity_render_brightness(
        c->actor_hold, c->scope.model.math, c->state.partial, &c->state.brightness, &c->brightness);
    c->error.entity = &c->brightness;
    if (!manager_draw_append_entity_queries(c, &c->brightness, &c->owner_work))
        return native_owner(c, &c->owner_work);
    for (size_t i = 0; i < 2; ++i)
        if (!mc_client_draw_adopt_world(&c->scope, &c->brightness.world_reads[i], &c->owner_work))
            return native_owner(c, &c->owner_work);
    if (!returned) {
        const mc_entity_render_error *error = &c->brightness;
        c->error.parent = c->error.site;
        c->error.site = (mc_client_graphics_site){error->site.class_name, error->site.method,
                                                  error->site.descriptor, error->site.pc};
        c->error.child = (mc_client_graphics_site){error->child.class_name, error->child.method,
                                                   error->child.descriptor, error->child.pc};
        c->error.source_exception = error->source_exception;
        c->error.exception_class = error->exception_class;
        c->error.unclassified_throwable =
            error->throwable == MC_ENTITY_RENDER_UNCLASSIFIED_THROWABLE;
        return fail(c, MC_CLIENT_MANAGER_ENTITY_FAILED, error->operation, error->native_policy);
    }
    c->state.brightness_returned = true;
    return true;
}

static bool class_lookup(mc_client_render_manager_draw_call *c, const mc_entity_class_literal *key,
                         mc_client_managed_renderer **out, unsigned int depth) {
    at(c, "(Ljava/lang/Class;)Lbw;", 5);
    if (c->scope.work >= c->scope.max_work)
        return fail(c, MC_CLIENT_MANAGER_LIMIT, "TH class lookup native work ceiling", true);
    ++c->scope.work;
    ++c->state.map_queries;
    mc_client_render_manager *m = c->manager_hold;
    if (m->fields.o == NULL)
        return source_null(c, "TH actual Map.get receiver");
    if (m->fields.o != m->map || m->map == NULL || depth >= MC_ENTITY_CLASS_KIND_COUNT)
        return fail(c, MC_CLIENT_MANAGER_INVALID_PHASE, "TH actual Map provenance", true);
    manager_map *map = m->map;
    if (map->capacity != 0 && (map->capacity & (map->capacity - 1u)) != 0)
        return fail(c, MC_CLIENT_MANAGER_INVALID_PHASE, "TH native identity Map extent", true);
    uintptr_t hash = (uintptr_t)key;
    hash ^= hash >> 7;
    hash ^= hash >> 17;
    mc_client_managed_renderer *renderer = NULL;
    if (map->capacity != 0)
        for (manager_map_entry *entry = map->buckets[(size_t)hash & (map->capacity - 1u)];
             entry != NULL; entry = entry->next)
            if (entry->key == key) {
                renderer = entry->owner;
                if (entry->value != (renderer == NULL       ? NULL
                                     : renderer->bb != NULL ? (const void *)renderer->bb
                                                            : (const void *)renderer))
                    return fail(c, MC_CLIENT_MANAGER_INVALID_PHASE, "TH actual renderer Map value",
                                true);
                break;
            }
    if (renderer == NULL) {
        mc_entity_class_literal_view view;
        if (mc_entity_class_literal_inspect(key, &view) != MC_ENTITY_OK)
            return fail(c, MC_CLIENT_MANAGER_INVALID_PHASE, "TH canonical Class receiver", true);
        if (view.kind != MC_ENTITY_CLASS_KIND_ENTITY) {
            mc_entity_class_kind parent;
            switch (view.kind) {
                case MC_ENTITY_CLASS_KIND_LOCAL_PLAYER:
                    parent = MC_ENTITY_CLASS_KIND_PLAYER;
                    break;
                case MC_ENTITY_CLASS_KIND_PLAYER:
                    parent = MC_ENTITY_CLASS_KIND_LIVING_ENTITY;
                    break;
                case MC_ENTITY_CLASS_KIND_LIVING_ENTITY:
                    parent = MC_ENTITY_CLASS_KIND_ENTITY;
                    break;
                default:
                    at(c, "(Ljava/lang/Class;)Lbw;", 26);
                    return fail(c, MC_CLIENT_MANAGER_UNIMPLEMENTED_DEPENDENCY,
                                "TH actual intermediate superclass identity", true);
            }
            const mc_entity_class_literal *superclass = NULL;
            if (mc_entity_class_literal_get(view.owner, parent, &superclass) != MC_ENTITY_OK)
                return fail(c, MC_CLIENT_MANAGER_INVALID_PHASE, "TH actual superclass cell", true);
            if (!class_lookup(c, superclass, &renderer, depth + 1u))
                return false;
            at(c, "(Ljava/lang/Class;)Lbw;", 39);
            if (renderer == NULL)
                return fail(c, MC_CLIENT_MANAGER_UNIMPLEMENTED_DEPENDENCY,
                            "TH nullable renderer memoization", true);
            if (!manager_map_put(m, key, renderer, 39))
                return native_owner(c, &m->classes->failure);
            ++c->state.memoized_keys;
        }
    }
    *out = renderer;
    return true;
}

static bool living_failure(mc_client_render_manager_draw_call *c,
                           const mc_client_living_draw_error *error) {
    c->error.living = error;
    c->error.parent = error->caller;
    c->error.site = error->site;
    c->error.child = error->child;
    c->error.source_exception = error->source_exception;
    c->error.exception_class = error->exception_class;
    c->error.unclassified_throwable =
        error->throwable == MC_CLIENT_LIVING_DRAW_UNCLASSIFIED_THROWABLE;
    const mc_client_manager_status status =
        error->status == MC_CLIENT_LIVING_DRAW_LIMIT  ? MC_CLIENT_MANAGER_LIMIT
        : error->status == MC_CLIENT_LIVING_DRAW_BUSY ? MC_CLIENT_MANAGER_BUSY
        : error->status == MC_CLIENT_LIVING_DRAW_UNIMPLEMENTED
            ? MC_CLIENT_MANAGER_UNIMPLEMENTED_DEPENDENCY
            : MC_CLIENT_MANAGER_MODEL_FAILED;
    return fail(c, status, error->operation, error->native_policy);
}

static bool coordinates(mc_client_render_manager_draw_call *c) {
    at(c, "(Lsn;)Lbw;", 2);
    if (c->actor_hold == NULL)
        return source_null(c, "TH actual Entity.getClass receiver");
    c->state.actor_class = c->actor_hold->class_literal;
    mc_client_managed_renderer *renderer = NULL;
    if (!class_lookup(c, c->state.actor_class, &renderer, 0))
        return false;
    c->state.lookup_returned = true;
    c->state.renderer = renderer;
    if (renderer == NULL) {
        at(c, coordinates_descriptor, 42);
        return true;
    }
    if (!manager_draw_capture_renderer(c, renderer, &c->owner_work))
        return native_owner(c, &c->owner_work);
    c->state.renderer_identity =
        renderer->bb == NULL ? (const void *)renderer : (const void *)renderer->bb;
    renderer->active = true;
    at(c, coordinates_descriptor, 24);
    if (!mc_client_living_draw_entered(&c->scope, renderer, c->actor_hold, c->state.x, c->state.y,
                                       c->state.z, c->state.yaw, c->state.partial, c->error.site,
                                       &c->living))
        return living_failure(c, &c->living);
    c->state.primary_returned = true;
    at(c, coordinates_descriptor, 39);
    if (!mc_client_entity_post_draw_entered(&c->scope, renderer, c->actor_hold, c->state.x,
                                            c->state.y, c->state.z, c->state.yaw, c->state.partial,
                                            c->error.site, &c->post))
        return living_failure(c, &c->post);
    c->state.post_draw_returned = true;
    at(c, coordinates_descriptor, 42);
    return true;
}

static bool execute(mc_client_render_manager_draw_call *c) {
    at(c, draw_descriptor, 1);
    if (c->actor_hold == NULL)
        return source_null(c, "TH actual Entity interpolation receiver");
    const mc_entity_base_fields *f = &c->actor_hold->base;
    mc_client_interpolation_error error;
    if (!mc_client_interpolate_double(f->bl, f->x, c->state.partial, 12, 14, 15, 16, &c->state.x,
                                      &error) ||
        !mc_client_interpolate_double(f->bm, f->y, c->state.partial, 30, 32, 33, 34, &c->state.y,
                                      &error) ||
        !mc_client_interpolate_double(f->bn, f->z, c->state.partial, 49, 51, 52, 53, &c->state.z,
                                      &error) ||
        !mc_client_interpolate_float(f->aU, f->yaw, c->state.partial, 68, 70, 71, &c->state.yaw,
                                     &error))
        return arithmetic(c, &error);
    if (!brightness(c))
        return false;
    at(c, draw_descriptor, 87);
    if (!manager_draw_capture_graphics(c, &c->owner_work))
        return native_owner(c, &c->owner_work);
    if (!mc_client_display_source_color(c->services.display, c->state.brightness,
                                        c->state.brightness, c->state.brightness, 1.0f,
                                        &c->error.graphics.display))
        return fail(c, MC_CLIENT_MANAGER_MODEL_FAILED, c->error.graphics.display.operation, true);
    c->state.color_returned = true;
    if (!subtract(c->state.x, c->manager_hold->classes->b, 96, &c->state.x, &error) ||
        !subtract(c->state.y, c->manager_hold->classes->c, 102, &c->state.y, &error) ||
        !subtract(c->state.z, c->manager_hold->classes->d, 108, &c->state.z, &error))
        return arithmetic(c, &error);
    at(c, draw_descriptor, 112);
    if (!coordinates(c))
        return false;
    at(c, draw_descriptor, 115);
    return true;
}

mc_client_manager_status mc_client_render_manager_draw_begin(
    mc_client_render_manager *manager, mc_entity *actor, float partial,
    const mc_client_manager_draw_services *services, const mc_client_manager_draw_limits *limits,
    mc_client_render_manager_draw_call **out, mc_client_manager_draw_error *error) {
    const mc_client_manager_status admitted =
        manager_draw_allocate(manager, services, limits, out, error);
    if (admitted != MC_CLIENT_MANAGER_OK)
        return admitted;
    mc_client_render_manager_draw_call *c = *out;
    c->state.actor = actor;
    c->state.partial = partial;
    c->state.living = &c->living.state;
    c->state.post_draw = &c->post.state;
    if (!manager_draw_capture_actor(c, actor, &c->owner_work))
        native_owner(c, &c->owner_work);
    else {
        c->executing = c->state.active = c->manager_hold->active = true;
        c->state.returned = execute(c);
        c->executing = c->state.active = c->manager_hold->active = false;
        if (c->renderer_hold != NULL)
            c->renderer_hold->active = false;
    }
    if (!manager_draw_capture_children(c, &c->owner_work)) {
        c->error.source_exception = false;
        native_owner(c, &c->owner_work);
    }
    if (error != NULL)
        *error = c->error;
    return c->error.status;
}

mc_client_manager_status mc_client_render_manager_post_draw_begin(
    mc_client_render_manager *manager, mc_client_managed_renderer *renderer, mc_entity *actor,
    double x, double y, double z, float yaw, float partial,
    const mc_client_manager_draw_services *services, const mc_client_manager_draw_limits *limits,
    mc_client_render_manager_draw_call **out, mc_client_manager_draw_error *error) {
    if (renderer == NULL) {
        manager_draw_error_init(error);
        if (error != NULL) {
            error->status = MC_CLIENT_MANAGER_INVALID_ARGUMENT;
            error->operation = "BW entered post-draw native renderer receiver";
            error->native_policy = true;
        }
        return MC_CLIENT_MANAGER_INVALID_ARGUMENT;
    }
    const mc_client_manager_status admitted =
        manager_draw_allocate(manager, services, limits, out, error);
    if (admitted != MC_CLIENT_MANAGER_OK)
        return admitted;
    mc_client_render_manager_draw_call *c = *out;
    c->state.post_only = true;
    c->state.actor = actor;
    c->state.x = x;
    c->state.y = y;
    c->state.z = z;
    c->state.yaw = yaw;
    c->state.partial = partial;
    c->state.renderer = renderer;
    c->state.post_draw = &c->post.state;
    if (!manager_draw_capture_actor(c, actor, &c->owner_work) ||
        !manager_draw_capture_renderer(c, renderer, &c->owner_work))
        native_owner(c, &c->owner_work);
    else {
        c->state.renderer_identity =
            renderer->bb == NULL ? (const void *)renderer : (const void *)renderer->bb;
        c->executing = c->state.active = c->manager_hold->active = renderer->active = true;
        const mc_client_graphics_site caller = {"bw", "b", coordinates_descriptor, 0};
        c->state.returned = mc_client_entity_post_draw_entered(&c->scope, renderer, actor, x, y, z,
                                                               yaw, partial, caller, &c->post);
        if (!c->state.returned)
            living_failure(c, &c->post);
        c->state.post_draw_returned = c->state.returned;
        c->state.source_pc = c->post.state.source_pc;
        c->executing = c->state.active = c->manager_hold->active = renderer->active = false;
    }
    if (!manager_draw_capture_children(c, &c->owner_work)) {
        c->error.source_exception = false;
        native_owner(c, &c->owner_work);
    }
    if (error != NULL)
        *error = c->error;
    return c->error.status;
}

bool mc_client_render_manager_draw_inspect(const mc_client_render_manager_draw_call *c,
                                           mc_client_manager_draw_state *out) {
    if (c == NULL || out == NULL || c->executing)
        return false;
    *out = c->state;
    out->owners = c->owner_count;
    out->allocation_bytes = c->bytes;
    return true;
}
