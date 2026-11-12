#include "../../../entity/entity_internal.h"
#include "../../../entity/player/tick_math_internal.h"
#include "../../application_state.h"
#include "internal.h"
#include "mc/client/controller/singleplayer.h"
#include "mc/entity/hit_result.h"
#include "mc/entity/living/view.h"
#include "mc/world/runtime/collision/entities.h"
#include "mc/world/runtime/runtime.h"
#include "mc/world/storage/source_chunk_loader.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct pick_candidate_frame {
    struct pick_candidate_frame *next;
    mc_entity_bounds *box;
    mc_entity_bounds_intercept_call *call;
    mc_entity_bounds_intercept_view state;
} pick_candidate_frame;

struct mc_client_entity_renderer_pick_call {
    mc_client_source_px *owner;
    mc_math_classes *math_argument, *math;
    bool owner_held;
    mc_client_entity_renderer_pick_state state;
    mc_client_entity_renderer_pick_error failure, cleanup;
    mc_client_entity_renderer_error owner_error, owner_cleanup;
    mc_singleplayer_controller_error controller;
    mc_math_class_error math_error;
    mc_entity_view_error view;
    mc_entity_living_ray_call *ray;
    mc_entity_living_ray_state ray_state;
    mc_entity_geometry_error geometry;
    mc_entity_vector_error vector;
    mc_entity_bounds_error bounds;
    mc_entity_hit_error hit, hit_cleanup, replaced_y_error;
    mc_entity_error entity;
    mc_world_nearby_error nearby;
    mc_world_source_chunk_error source_chunk;
    mc_world_entity_list_view list;
    mc_world_runtime *query_world;
    mc_entity_classes *classes;
    mc_entity *excluded;
    mc_entity *bounds_receiver, *candidate_occurrence;
    mc_entity_bounds *sweep, *query_box;
    mc_entity_hit_result *entity_result;
    mc_entity_hit_result *replaced_y;
    bool classes_held;
    pick_candidate_frame *candidates;
};

static mc_client_graphics_site pick_site(unsigned int pc) {
    return (mc_client_graphics_site){"px", "a", "(F)V", (int32_t)pc};
}

static bool pick_fail(mc_client_entity_renderer_pick_call *call, mc_client_pick_status status,
                      const char *operation, unsigned int pc, bool native) {
    call->failure.status = status;
    call->failure.operation = operation;
    call->failure.native_policy = native;
    call->failure.site = pick_site(pc);
    call->state.source_pc = pc;
    if (status == MC_CLIENT_PICK_SOURCE_NULL_REFERENCE)
        call->failure.exception_class = "java/lang/NullPointerException";
    return false;
}

static client_application *pick_app(mc_client_entity_renderer_pick_call *call, unsigned int pc) {
    client_application *app = call->owner->fields.j;
    if (app == NULL) {
        pick_fail(call, MC_CLIENT_PICK_SOURCE_NULL_REFERENCE, "actual px.j null App receiver", pc,
                  false);
        return NULL;
    }
    if (call->state.app != NULL && call->state.app != app) {
        pick_fail(call, MC_CLIENT_PICK_STATE, "picking native exclusive App graph", pc, true);
        return NULL;
    }
    call->state.app = app;
    return app;
}

static bool pick_math(mc_client_entity_renderer_pick_call *call, unsigned int pc) {
    if (call->math != NULL)
        return true;
    if (mc_math_classes_retain(call->math_argument, &call->math_error) != MC_MATH_CLASS_OK) {
        call->failure.math = &call->math_error;
        return pick_fail(call, MC_CLIENT_PICK_STATE, "picking SAME MathHelper occurrence", pc,
                         true);
    }
    call->math = call->math_argument;
    return true;
}

static bool pick_m(mc_client_entity_renderer_pick_call *call, mc_entity *value, unsigned int pc) {
    mc_client_source_px *owner = call->owner;
    if (owner->pick_m_published && owner->fields.m != owner->pick_m_occurrence)
        return pick_fail(call, MC_CLIENT_PICK_STATE, "picking owned px.m field changed", pc, true);
    if (value != NULL) {
        call->entity.status = mc_entity_retain(value);
        if (call->entity.status != MC_ENTITY_OK) {
            call->failure.entity = &call->entity;
            return pick_fail(call, MC_CLIENT_PICK_LIMIT, "picking px.m native occurrence", pc,
                             true);
        }
    }
    mc_entity *old = owner->pick_m_occurrence;
    owner->fields.m = value;
    owner->pick_m_occurrence = value;
    owner->pick_m_published = true;
    call->state.selected = value;
    mc_entity_release(old);
    return true;
}

static bool pick_y(mc_client_entity_renderer_pick_call *call, client_application *app,
                   mc_entity_hit_result *value, unsigned int pc) {
    if (app == NULL)
        return pick_fail(call, MC_CLIENT_PICK_SOURCE_NULL_REFERENCE, "actual App.y null receiver",
                         pc, false);
    mc_client_source_px *owner = call->owner;
    if (owner->pick_y_app != NULL &&
        (owner->pick_y_app != app || app->y != owner->pick_y_occurrence))
        return pick_fail(call, MC_CLIENT_PICK_STATE, "picking owned App.y field changed", pc, true);
    if (value != NULL && mc_entity_hit_result_retain(value, &call->hit) != MC_ENTITY_HIT_OK) {
        call->failure.hit = &call->hit;
        return pick_fail(call, MC_CLIENT_PICK_CHILD_FAILED, "picking App.y native occurrence", pc,
                         true);
    }
    mc_entity_hit_result *old = owner->pick_y_occurrence;
    app->y = value;
    owner->pick_y_occurrence = value;
    owner->pick_y_app = app;
    call->state.result = value;
    if (mc_entity_hit_result_release(&old, &call->replaced_y_error) != MC_ENTITY_HIT_OK) {
        /* Exclusive source-field replacement normally cannot fail this drop.
         * Keep a failed occurrence in the call rather than losing its right. */
        call->replaced_y = old;
        call->failure.hit = &call->replaced_y_error;
        return pick_fail(call, MC_CLIENT_PICK_CLEANUP_FAILED,
                         "picking replaced App.y occurrence release", pc, true);
    }
    return true;
}

static bool view_failed(mc_client_entity_renderer_pick_call *call, unsigned int pc) {
    call->failure.view = &call->view;
    call->failure.child =
        (mc_client_graphics_site){call->view.site.class_name, call->view.site.method,
                                  call->view.site.descriptor, call->view.site.pc};
    call->failure.exception_class = call->view.exception_class;
    return pick_fail(call,
                     call->view.status == MC_ENTITY_VIEW_SOURCE_NULL_REFERENCE
                         ? MC_CLIENT_PICK_SOURCE_NULL_REFERENCE
                         : MC_CLIENT_PICK_CHILD_FAILED,
                     call->view.operation, pc, call->view.native_policy);
}

static bool geometry_failed(mc_client_entity_renderer_pick_call *call, unsigned int pc) {
    call->failure.geometry = &call->geometry;
    call->failure.child = (mc_client_graphics_site){
        call->geometry.site.declaring_class, call->geometry.site.method,
        call->geometry.site.descriptor, (int32_t)call->geometry.site.bytecode};
    return pick_fail(call,
                     call->geometry.status == MC_ENTITY_GEOMETRY_SOURCE_NULL_REFERENCE
                         ? MC_CLIENT_PICK_SOURCE_NULL_REFERENCE
                         : MC_CLIENT_PICK_CHILD_FAILED,
                     call->geometry.operation, pc, call->geometry.native_policy);
}

static bool hit_failed(mc_client_entity_renderer_pick_call *call, unsigned int pc) {
    call->failure.hit = &call->hit;
    call->failure.child =
        (mc_client_graphics_site){call->hit.site.declaring_class, call->hit.site.method,
                                  call->hit.site.descriptor, (int32_t)call->hit.site.bytecode};
    return pick_fail(call,
                     call->hit.status == MC_ENTITY_HIT_SOURCE_NULL_REFERENCE
                         ? MC_CLIENT_PICK_SOURCE_NULL_REFERENCE
                         : MC_CLIENT_PICK_CHILD_FAILED,
                     call->hit.operation, pc, call->hit.native_policy);
}

static bool bounds_failed(mc_client_entity_renderer_pick_call *call, unsigned int pc) {
    call->failure.bounds = &call->bounds;
    return pick_fail(call, MC_CLIENT_PICK_CHILD_FAILED, call->bounds.operation, pc, true);
}

static bool pick_reach(mc_client_entity_renderer_pick_call *call, client_application *app,
                       unsigned int pc, double *out) {
    const mc_client_singleplayer_controller *controller = app->controller;
    if (controller == NULL)
        return pick_fail(call, MC_CLIENT_PICK_SOURCE_NULL_REFERENCE,
                         "actual ob.b null controller receiver", pc, false);
    float reach = 0;
    if (mc_client_singleplayer_controller_reach(controller, &reach, &call->controller) !=
        MC_SINGLEPLAYER_CONTROLLER_OK) {
        call->failure.controller = &call->controller;
        return pick_fail(call, MC_CLIENT_PICK_CHILD_FAILED, call->controller.operation, pc, true);
    }
    *out = at_widen(reach);
    return true;
}

/* Only the canonical ob/os owner exists in the admitted client graph. The
 * actual OS ancestry is not PJ; another controller needs its own real owner. */
static bool pick_creative(mc_client_entity_renderer_pick_call *call, client_application *app,
                          unsigned int pc, bool *out) {
    if (app->controller != NULL) {
        mc_singleplayer_controller_snapshot actual;
        if (!mc_client_singleplayer_controller_read(app->controller, &actual))
            return pick_fail(call, MC_CLIENT_PICK_STATE, "picking actual OS controller owner", pc,
                             true);
    }
    *out = false;
    return true;
}

static bool pick_classes(mc_client_entity_renderer_pick_call *call, mc_entity *receiver,
                         unsigned int pc) {
    if (receiver == NULL)
        return pick_fail(call, MC_CLIENT_PICK_SOURCE_NULL_REFERENCE, "actual LS view null receiver",
                         pc, false);
    if (call->classes != NULL && call->classes != receiver->classes)
        return pick_fail(call, MC_CLIENT_PICK_STATE, "picking SAME Entity class namespace", pc,
                         true);
    if (!call->classes_held) {
        call->entity.status = mc_entity_classes_retain(receiver->classes, MC_ENTITY_CLIENT,
                                                       call->owner->services.profile);
        if (call->entity.status != MC_ENTITY_OK) {
            call->failure.entity = &call->entity;
            return pick_fail(call, MC_CLIENT_PICK_CHILD_FAILED,
                             "picking Entity namespace occurrence", pc, true);
        }
        call->classes = receiver->classes;
        call->classes_held = true;
    }
    return true;
}

static bool pick_vector_failed(mc_client_entity_renderer_pick_call *call, unsigned int pc) {
    call->failure.vector = &call->vector;
    return pick_fail(call, MC_CLIENT_PICK_CHILD_FAILED, call->vector.operation, pc, true);
}

static bool pick_query(mc_client_entity_renderer_pick_call *call) {
    client_application *app = pick_app(call, 189);
    if (app == NULL)
        return false;
    call->query_world = app->active_world;
    if (call->query_world != NULL && !mc_world_runtime_binding_retain(call->query_world)) {
        call->query_world = NULL;
        return pick_fail(call, MC_CLIENT_PICK_BUSY, "picking query World occurrence", 189, true);
    }
    app = pick_app(call, 196);
    if (app == NULL)
        return false;
    call->excluded = app->i;
    if (call->excluded != NULL && mc_entity_retain(call->excluded) != MC_ENTITY_OK) {
        call->excluded = NULL;
        return pick_fail(call, MC_CLIENT_PICK_LIMIT, "picking excluded Entity occurrence", 196,
                         true);
    }
    app = pick_app(call, 203);
    if (app == NULL)
        return false;
    if (!pick_classes(call, app->i, 206))
        return false;
    if (mc_entity_retain(app->i) != MC_ENTITY_OK)
        return pick_fail(call, MC_CLIENT_PICK_LIMIT, "picking bounds receiver occurrence", 206,
                         true);
    call->bounds_receiver = app->i;
    const mc_entity_bounds *box = app->i->bounds;
    const double dx = at_mul(call->state.look->x, call->state.entity_reach);
    const double dy = at_mul(call->state.look->y, call->state.entity_reach);
    const double dz = at_mul(call->state.look->z, call->state.entity_reach);
    if (box == NULL)
        return pick_fail(call, MC_CLIENT_PICK_SOURCE_NULL_REFERENCE,
                         "actual eq.a sweep null receiver", 230, false);
    if (mc_entity_bounds_sweep(call->classes, box, dx, dy, dz, &call->sweep, &call->bounds) !=
        MC_ENTITY_OK)
        return bounds_failed(call, 230);
    if (mc_entity_bounds_expand(call->classes, call->sweep, 1.0, 1.0, 1.0, &call->query_box,
                                &call->bounds) != MC_ENTITY_OK)
        return bounds_failed(call, 242);
    if (call->query_world == NULL)
        return pick_fail(call, MC_CLIENT_PICK_SOURCE_NULL_REFERENCE,
                         "actual fd.b nearby null receiver", 245, false);
    if (mc_world_runtime_nearby_entities(call->query_world, call->excluded, call->query_box,
                                         &call->list, &call->nearby) != MC_WORLD_STORAGE_OK) {
        if (call->nearby.query.source_chunk != NULL) {
            call->source_chunk = *call->nearby.query.source_chunk;
            call->nearby.query.source_chunk = &call->source_chunk;
            call->failure.source_chunk = &call->source_chunk;
        }
        call->failure.nearby = &call->nearby;
        return pick_fail(call, MC_CLIENT_PICK_CHILD_FAILED, "actual fd.b nearby Entity query", 245,
                         true);
    }
    call->state.query_returned = true;
    return true;
}

static bool pick_entities(mc_client_entity_renderer_pick_call *call) {
    for (int32_t index = 0;; ++index) {
        call->state.index = index;
        call->state.source_pc = 260;
        size_t size = 0;
        if (call->list.identity == NULL)
            return pick_fail(call, MC_CLIENT_PICK_SOURCE_NULL_REFERENCE,
                             "actual nearby List.size null receiver", 260, false);
        if (mc_world_entity_list_view_count(&call->list, &size) != MC_WORLD_STORAGE_OK)
            return pick_fail(call, MC_CLIENT_PICK_CHILD_FAILED, "actual nearby List.size", 260,
                             true);
        if (size > (size_t)INT32_MAX)
            return pick_fail(call, MC_CLIENT_PICK_LIMIT, "picking native List size domain", 260,
                             true);
        if ((size_t)index >= size)
            return true;
        mc_entity *candidate = NULL;
        if (mc_world_entity_list_view_get(&call->list, (size_t)index, &candidate) !=
            MC_WORLD_STORAGE_OK)
            return pick_fail(call, MC_CLIENT_PICK_CHILD_FAILED, "actual nearby List.get", 272,
                             true);
        if (candidate != NULL && mc_entity_retain(candidate) != MC_ENTITY_OK)
            return pick_fail(call, MC_CLIENT_PICK_LIMIT, "picking candidate occurrence", 280, true);
        mc_entity_release(call->candidate_occurrence);
        call->candidate_occurrence = candidate;
        call->state.candidate = candidate;
        ++call->state.entities_visited;
        bool pickable = false;
        if (mc_entity_pickable(candidate, &pickable, &call->hit) != MC_ENTITY_HIT_OK)
            return hit_failed(call, 284);
        if (!pickable)
            continue;
        float border = 0;
        if (mc_entity_pick_border_size(candidate, &border, &call->hit) != MC_ENTITY_HIT_OK)
            return hit_failed(call, 295);
        const mc_entity_bounds *bounds = candidate->bounds;
        const double x = at_widen(border), y = at_widen(border), z = at_widen(border);
        if (bounds == NULL)
            return pick_fail(call, MC_CLIENT_PICK_SOURCE_NULL_REFERENCE,
                             "actual eq.b candidate bounds null receiver", 314, false);
        pick_candidate_frame *frame = calloc(1, sizeof(*frame));
        if (frame == NULL)
            return pick_fail(call, MC_CLIENT_PICK_ALLOCATION,
                             "picking retained candidate workspace", 314, true);
        frame->next = call->candidates;
        call->candidates = frame;
        if (mc_entity_bounds_expand(call->classes, bounds, x, y, z, &frame->box, &call->bounds) !=
            MC_ENTITY_OK)
            return bounds_failed(call, 314);
        if (mc_entity_bounds_intercept_begin(call->classes, frame->box, call->state.origin,
                                             call->state.endpoint, &frame->call,
                                             &call->entity) != MC_ENTITY_HIT_OK) {
            if (frame->call != NULL) {
                (void)mc_entity_bounds_intercept_inspect(frame->call, &frame->state);
                call->failure.intercept = &frame->state;
            } else
                call->failure.entity = &call->entity;
            return pick_fail(call, MC_CLIENT_PICK_CHILD_FAILED, "actual eq.a segment intercept",
                             325, frame->state.error == NULL || frame->state.error->native_policy);
        }
        (void)mc_entity_bounds_intercept_inspect(frame->call, &frame->state);
        bool inside = false;
        if (mc_entity_bounds_contains_point(call->classes, frame->box, call->state.origin, &inside,
                                            &call->geometry) != MC_ENTITY_GEOMETRY_OK)
            return geometry_failed(call, 334);
        if (inside) {
            if (0.0 < call->state.nearest_distance || call->state.nearest_distance == 0.0) {
                if (!pick_m(call, candidate, 357))
                    return false;
                call->state.nearest_distance = 0.0;
            }
        } else if (frame->state.result != NULL) {
            mc_entity_hit_result_view result;
            if (mc_entity_hit_result_inspect(frame->state.result, &result) != MC_ENTITY_HIT_OK)
                return pick_fail(call, MC_CLIENT_PICK_STATE, "picking actual intercept VF", 375,
                                 true);
            double distance = 0;
            if (mc_entity_vector_distance(call->classes, call->state.origin, result.fields.f,
                                          &distance, &call->geometry) != MC_ENTITY_GEOMETRY_OK)
                return geometry_failed(call, 378);
            if (distance < call->state.nearest_distance || call->state.nearest_distance == 0.0) {
                if (!pick_m(call, candidate, 401))
                    return false;
                call->state.nearest_distance = distance;
            }
        }
        if (index == INT32_MAX)
            return pick_fail(call, MC_CLIENT_PICK_LIMIT, "picking native List index width", 408,
                             true);
    }
}

static bool pick_body(mc_client_entity_renderer_pick_call *call) {
    client_application *app = pick_app(call, 4);
    if (app == NULL)
        return false;
    if (app->i == NULL) {
        call->state.source_pc = 10;
        call->state.source_returned = true;
        return true;
    }
    app = pick_app(call, 15);
    if (app == NULL)
        return false;
    if (app->active_world == NULL) {
        call->state.source_pc = 21;
        call->state.source_returned = true;
        return true;
    }
    app = pick_app(call, 26);
    if (app == NULL || !pick_reach(call, app, 29, &call->state.block_reach))
        return false;
    client_application *left = call->owner->fields.j; /* Actual LHS j35, before RHS. */
    app = pick_app(call, 42);
    if (app == NULL || !pick_classes(call, app->i, 47) || !pick_math(call, 47))
        return false;
    if (mc_entity_living_ray_begin(app->i, call->math, call->state.block_reach, call->state.partial,
                                   &call->ray) != MC_ENTITY_VIEW_OK) {
        if (call->ray != NULL) {
            (void)mc_entity_living_ray_inspect(call->ray, &call->ray_state);
            call->failure.ray = &call->ray_state;
            if (call->ray_state.failure != NULL) {
                call->view = *call->ray_state.failure;
                return view_failed(call, 47);
            }
        }
        return pick_fail(call, MC_CLIENT_PICK_ALLOCATION, "actual LS ray native admission", 47,
                         true);
    }
    (void)mc_entity_living_ray_inspect(call->ray, &call->ray_state);
    call->state.block_result = call->ray_state.result;
    if (!pick_y(call, left, call->ray_state.result, 50))
        return false;
    call->state.block_published = true;
    call->state.entity_reach = call->state.block_reach;
    app = pick_app(call, 60);
    if (app == NULL || !pick_classes(call, app->i, 64))
        return false;
    if (mc_entity_living_position(app->i, call->state.partial, &call->state.origin, &call->view) !=
        MC_ENTITY_VIEW_OK)
        return view_failed(call, 64);
    app = pick_app(call, 73);
    if (app == NULL)
        return false;
    if (app->y != NULL) {
        app = pick_app(call, 83);
        if (app == NULL)
            return false;
        mc_entity_hit_result_view hit;
        if (mc_entity_hit_result_inspect(app->y, &hit) != MC_ENTITY_HIT_OK)
            return pick_fail(call, MC_CLIENT_PICK_STATE, "picking actual App.y VF", 86, true);
        if (mc_entity_vector_distance(call->classes, hit.fields.f, call->state.origin,
                                      &call->state.entity_reach,
                                      &call->geometry) != MC_ENTITY_GEOMETRY_OK)
            return geometry_failed(call, 91);
    }
    app = pick_app(call, 100);
    bool creative = false;
    if (app == NULL || !pick_creative(call, app, 103, &creative))
        return false;
    if (creative)
        call->state.entity_reach = 32.0;
    else if (call->state.entity_reach > 3.0)
        call->state.entity_reach = 3.0;
    app = pick_app(call, 140);
    if (app == NULL || !pick_classes(call, app->i, 144))
        return false;
    if (mc_entity_living_look(app->i, call->math, call->state.partial, &call->state.look,
                              &call->view) != MC_ENTITY_VIEW_OK)
        return view_failed(call, 144);
    const double dx = at_mul(call->state.look->x, call->state.entity_reach);
    const double dy = at_mul(call->state.look->y, call->state.entity_reach);
    const double dz = at_mul(call->state.look->z, call->state.entity_reach);
    if (mc_entity_vector_add(call->classes, call->state.origin, dx, dy, dz, &call->state.endpoint,
                             &call->vector) != MC_ENTITY_OK)
        return pick_vector_failed(call, 172);
    if (!pick_m(call, NULL, 179))
        return false;
    call->state.target_cleared = true;
    if (!pick_query(call) || !pick_entities(call))
        return false;
    if (call->owner->fields.m != NULL) {
        app = pick_app(call, 425);
        if (app == NULL || !pick_creative(call, app, 428, &creative))
            return false;
        if (!creative) {
            left = call->owner->fields.j; /* Actual LHS435; NULL faults only after RHS. */
            if (mc_entity_hit_result_allocate(call->classes, &call->entity_result, &call->hit) !=
                MC_ENTITY_HIT_OK)
                return hit_failed(call, 438);
            if (mc_entity_hit_result_construct_entity(call->entity_result, call->owner->fields.m,
                                                      &call->hit) != MC_ENTITY_HIT_OK)
                return hit_failed(call, 446);
            if (!pick_y(call, left, call->entity_result, 449))
                return false;
            call->state.entity_published = true;
        }
    }
    call->state.source_pc = 452;
    call->state.source_returned = true;
    return true;
}

mc_client_pick_status
mc_client_entity_renderer_pick_begin(mc_client_source_px *owner, mc_math_classes *math,
                                     float partial, mc_client_entity_renderer_pick_call **out) {
    if (out == NULL || *out != NULL)
        return MC_CLIENT_PICK_ARGUMENT;
    if (owner != NULL && (owner->active || owner->pick_call != NULL ||
                          owner->phase == MC_CLIENT_ENTITY_RENDERER_CONSTRUCTING ||
                          owner->phase == MC_CLIENT_ENTITY_RENDERER_RETIRING))
        return MC_CLIENT_PICK_BUSY;
    mc_client_entity_renderer_pick_call *call = calloc(1, sizeof(*call));
    if (call == NULL)
        return MC_CLIENT_PICK_ALLOCATION;
    call->owner = owner;
    call->math_argument = math;
    call->state.owner = owner;
    call->state.partial = partial;
    call->state.active = true;
    *out = call;
    if (owner == NULL) {
        pick_fail(call, MC_CLIENT_PICK_SOURCE_NULL_REFERENCE, "actual PX null receiver", 0, false);
        goto done;
    }
    if (mc_client_entity_renderer_retain(owner, &call->owner_error) !=
        MC_CLIENT_ENTITY_RENDERER_OK) {
        call->failure.owner = &call->owner_error;
        pick_fail(call, MC_CLIENT_PICK_CHILD_FAILED, "picking PX occurrence", 0, true);
        goto done;
    }
    call->owner_held = true;
    owner->pick_call = call;
    owner->active = true;
    (void)pick_body(call);
    owner->active = false;
done:
    call->state.active = false;
    return call->failure.status;
}

bool mc_client_entity_renderer_pick_inspect(const mc_client_entity_renderer_pick_call *call,
                                            mc_client_entity_renderer_pick_state *out) {
    if (call == NULL || out == NULL || call->state.active)
        return false;
    *out = call->state;
    out->failure = &call->failure;
    out->cleanup = &call->cleanup;
    return true;
}

static mc_client_pick_status pick_cleanup_fail(mc_client_entity_renderer_pick_call *call,
                                               const char *operation) {
    call->cleanup.status = MC_CLIENT_PICK_CLEANUP_FAILED;
    call->cleanup.operation = operation;
    call->cleanup.native_policy = true;
    call->state.active = false;
    return call->cleanup.status;
}

static bool pick_drop_vector(mc_client_entity_renderer_pick_call *call, mc_entity_vector **slot) {
    if (*slot == NULL)
        return true;
    if (mc_entity_vector_release(call->classes, *slot) != MC_ENTITY_OK)
        return false;
    *slot = NULL;
    return true;
}

static bool pick_drop_bounds(mc_client_entity_renderer_pick_call *call, mc_entity_bounds **slot) {
    if (*slot == NULL)
        return true;
    if (mc_entity_bounds_release(call->classes, *slot) != MC_ENTITY_OK)
        return false;
    *slot = NULL;
    return true;
}

mc_client_pick_status
mc_client_entity_renderer_pick_retire(mc_client_entity_renderer_pick_call **slot) {
    if (slot == NULL)
        return MC_CLIENT_PICK_ARGUMENT;
    mc_client_entity_renderer_pick_call *call = *slot;
    if (call == NULL)
        return MC_CLIENT_PICK_OK;
    if (call->state.active)
        return MC_CLIENT_PICK_BUSY;
    call->state.active = call->state.retiring = true;
    call->cleanup = (mc_client_entity_renderer_pick_error){0};
    while (call->candidates != NULL) {
        pick_candidate_frame *frame = call->candidates;
        if (mc_entity_bounds_intercept_release(&frame->call, &call->hit_cleanup) !=
            MC_ENTITY_HIT_OK) {
            call->cleanup.hit = &call->hit_cleanup;
            return pick_cleanup_fail(call, "picking actual intercept native retirement");
        }
        if (call->failure.intercept == &frame->state)
            call->failure.intercept = NULL;
        frame->state = (mc_entity_bounds_intercept_view){0};
        if (!pick_drop_bounds(call, &frame->box))
            return pick_cleanup_fail(call, "picking candidate bounds occurrence");
        call->candidates = frame->next;
        free(frame);
    }
    if (mc_entity_hit_result_release(&call->entity_result, &call->hit_cleanup) !=
            MC_ENTITY_HIT_OK ||
        mc_entity_hit_result_release(&call->replaced_y, &call->hit_cleanup) != MC_ENTITY_HIT_OK) {
        call->cleanup.hit = &call->hit_cleanup;
        return pick_cleanup_fail(call, "picking candidate VF occurrence");
    }
    if (call->ray != NULL) {
        if (mc_entity_living_ray_retire(&call->ray) != MC_ENTITY_VIEW_OK) {
            (void)mc_entity_living_ray_inspect(call->ray, &call->ray_state);
            call->cleanup.ray = &call->ray_state;
            return pick_cleanup_fail(call, "picking exact LS ray retirement");
        }
        call->failure.ray = NULL;
        call->view.world = NULL;
        call->ray_state = (mc_entity_living_ray_state){0};
        call->state.block_result = NULL;
    }
    mc_world_entity_list_view_release(&call->list);
    if (!pick_drop_bounds(call, &call->query_box) || !pick_drop_bounds(call, &call->sweep))
        return pick_cleanup_fail(call, "picking query bounds occurrences");
    if (!pick_drop_vector(call, &call->state.endpoint) ||
        !pick_drop_vector(call, &call->state.look) || !pick_drop_vector(call, &call->state.origin))
        return pick_cleanup_fail(call, "picking source vector occurrences");
    mc_entity_release(call->candidate_occurrence);
    call->candidate_occurrence = NULL;
    call->state.candidate = NULL;
    mc_entity_release(call->bounds_receiver);
    call->bounds_receiver = NULL;
    mc_entity_release(call->excluded);
    call->excluded = NULL;
    if (call->query_world != NULL) {
        call->nearby.query.source_chunk = NULL;
        mc_world_runtime_binding_release(call->query_world);
        call->query_world = NULL;
    }
    if (call->classes_held) {
        if (mc_entity_classes_release(call->classes) != MC_ENTITY_OK)
            return pick_cleanup_fail(call, "picking Entity namespace occurrence");
        call->classes_held = false;
    }
    if (call->math != NULL &&
        mc_math_classes_release(&call->math, &call->math_error) != MC_MATH_CLASS_OK) {
        call->cleanup.math = &call->math_error;
        return pick_cleanup_fail(call, "picking MathHelper occurrence");
    }
    if (call->owner_held) {
        mc_client_source_px *owner = call->owner;
        if (owner->pick_call != call)
            return pick_cleanup_fail(call, "picking native call ownership changed");
        owner->pick_call = NULL;
        if (mc_client_entity_renderer_release(&owner, &call->owner_cleanup) !=
            MC_CLIENT_ENTITY_RENDERER_OK) {
            call->owner->pick_call = call;
            call->cleanup.owner = &call->owner_cleanup;
            return pick_cleanup_fail(call, "picking PX occurrence release");
        }
        call->owner_held = false;
        call->owner = NULL;
    }
    free(call);
    *slot = NULL;
    return MC_CLIENT_PICK_OK;
}

mc_client_pick_status
mc_client_entity_renderer_pick_detach(mc_client_source_px *owner,
                                      mc_client_entity_renderer_pick_error *error) {
    if (error == NULL)
        return MC_CLIENT_PICK_ARGUMENT;
    *error = (mc_client_entity_renderer_pick_error){0};
    if (owner == NULL) {
        error->status = MC_CLIENT_PICK_ARGUMENT;
        error->operation = "picking native detach owner";
    } else if (owner->active || owner->pick_call != NULL || owner->holds != 1) {
        error->status = MC_CLIENT_PICK_BUSY;
        error->operation = "picking active/external PX occurrences";
    } else if ((owner->pick_m_published && owner->fields.m != owner->pick_m_occurrence) ||
               (owner->pick_y_app != NULL && owner->pick_y_app->y != owner->pick_y_occurrence)) {
        error->status = MC_CLIENT_PICK_BUSY;
        error->operation = "picking owned source field changed";
    } else {
        if (owner->pick_y_app != NULL) {
            if (mc_entity_hit_result_release(&owner->pick_y_occurrence, &owner->pick_hit_cleanup) !=
                MC_ENTITY_HIT_OK) {
                error->status = MC_CLIENT_PICK_BUSY;
                error->operation = "picking owned App.y occurrence release";
                error->native_policy = true;
                error->hit = &owner->pick_hit_cleanup;
                return error->status;
            }
            owner->pick_y_app->y = NULL;
            owner->pick_y_app = NULL;
        }
        if (owner->pick_m_published) {
            mc_entity_release(owner->pick_m_occurrence);
            owner->pick_m_occurrence = NULL;
            owner->fields.m = NULL;
            owner->pick_m_published = false;
        }
    }
    error->native_policy = error->status != MC_CLIENT_PICK_OK;
    return error->status;
}
