#include "render_internal.h"

#include "../../entity/entity_internal.h"
#include "../../entity/particles/particle_internal.h"
#include "../display/source_surface.h"
#include "../render/interpolation_internal.h"
#include "../render/living_draw_internal.h"
#include "mc/core/math/math.h"

#include <stdlib.h>

static const char particle_descriptor[] = "(Lnw;FFFFFF)V";

static void draw_views_expired(mc_client_effect_draw_call *c) {
    mc_client_manager_draw_error *errors[] = {&c->manager_failure, &c->manager_cleanup,
                                              &c->manager_retirement};
    for (size_t i = 0; i < sizeof errors / sizeof *errors; ++i) {
        errors[i]->entity = NULL;
        errors[i]->living = NULL;
        errors[i]->owner.font = NULL;
        errors[i]->owner.acquisition = NULL;
    }
    c->cleanup.manager_draw = NULL;
}

static void draw_retired(mc_client_effect_draw_call *c) {
    ++c->state.pickup_draw_retirements;
    if (c->pickup_last != NULL)
        c->pickup_last->value.retired = true;
    draw_views_expired(c);
}

bool mc_effect_draw_pickup_retire(mc_client_effect_draw_call *c) {
    if (c->manager_draw == NULL)
        return true;
    if (mc_client_render_manager_draw_retire(&c->manager_draw, &c->manager_retirement) !=
        MC_CLIENT_MANAGER_OK) {
        c->cleanup.manager_draw = &c->manager_retirement;
        return false;
    }
    draw_retired(c);
    return true;
}

void mc_effect_draw_pickup_free(mc_client_effect_draw_call *c) {
    while (c->pickup_first != NULL) {
        mc_effect_pickup_draw_record *next = c->pickup_first->next;
        free(c->pickup_first);
        c->pickup_first = next;
    }
    c->pickup_last = NULL;
    c->pickup_count = 0;
}

size_t mc_client_effect_renderer_pickup_draw_count(const mc_client_effect_draw_call *c) {
    return c == NULL || c->state.active ? 0 : c->pickup_count;
}

bool mc_client_effect_renderer_pickup_draw_at(const mc_client_effect_draw_call *c, size_t index,
                                              mc_effect_pickup_draw_receipt *out) {
    if (c == NULL || c->state.active || out == NULL || index >= c->pickup_count)
        return false;
    const mc_effect_pickup_draw_record *record = c->pickup_first;
    while (index-- != 0)
        record = record->next;
    *out = record->value;
    return true;
}

static bool failed(mc_client_effect_draw_call *c, unsigned int invocation,
                   mc_effect_draw_status status, const char *operation, unsigned int pc,
                   bool native) {
    mc_effect_draw_fail(c, status, operation, invocation, native);
    c->state.particle_pc = pc;
    c->failure.child = (mc_client_graphics_site){"em", "a", particle_descriptor, (int32_t)pc};
    return false;
}

static bool interpolate(mc_client_effect_draw_call *c, unsigned int invocation, double previous,
                        double current, float partial, int32_t sub, int32_t widen, int32_t mul,
                        int32_t add, double *out) {
    mc_client_interpolation_error arithmetic;
    if (mc_client_interpolate_double(previous, current, partial, sub, widen, mul, add, out,
                                     &arithmetic))
        return true;
    return failed(c, invocation,
                  arithmetic.status == MC_CLIENT_INTERPOLATION_UNSUPPORTED_FP
                      ? MC_EFFECT_DRAW_UNSUPPORTED_FP
                      : MC_EFFECT_DRAW_UNSUPPORTED_NUMERIC,
                  "EM reached interpolation admission", (unsigned int)arithmetic.pc, true);
}

static bool math(mc_client_effect_draw_call *c, unsigned int invocation, unsigned int pc) {
    if (mc_effect_draw_math(c, pc))
        return true;
    const mc_effect_draw_status status = c->failure.status;
    return failed(c, invocation, status, c->failure.operation, pc, true);
}

static void draw_receipt(mc_client_effect_draw_call *c, mc_entity *actor) {
    mc_effect_pickup_draw_receipt *r = &c->pickup_last->value;
    r->admitted = c->manager_draw != NULL;
    r->actor_identity = (uintptr_t)actor;
    r->actor_kind = -1;
    if (actor != NULL) {
        r->actor_dead = actor->base.be;
        mc_entity_class_literal_view view;
        if (mc_entity_class_literal_inspect(actor->class_literal, &view) == MC_ENTITY_OK)
            r->actor_kind = (int32_t)view.kind;
    }
    mc_client_manager_draw_state s;
    if (c->manager_draw == NULL || !mc_client_render_manager_draw_inspect(c->manager_draw, &s))
        return;
    r->returned = s.returned;
    r->coordinates_only = s.coordinates_only;
    r->primary_returned = s.primary_returned;
    r->post_draw_returned = s.post_draw_returned;
    r->th_brightness_returned = s.brightness_returned;
    r->th_color_returned = s.color_returned;
    r->th_pc = s.source_pc;
    r->x = s.x;
    r->y = s.y;
    r->z = s.z;
    r->yaw = s.yaw;
    r->partial = s.partial;
    r->renderer_identity = (uintptr_t)s.renderer_identity;
    mc_client_managed_renderer_state renderer;
    if (mc_client_managed_renderer_inspect(s.renderer, &renderer))
        r->renderer_class = renderer.source_class;
    if (s.living != NULL) {
        r->primary_pc = s.living->source_pc;
        r->bb_returned = s.living->item.returned;
        r->copies = s.living->item.copies_returned;
        r->faces = s.living->item.faces_returned;
    }
    if (s.post_draw != NULL)
        r->post_pc = s.post_draw->source_pc;
}

static bool draw_actor(mc_client_effect_draw_call *c, unsigned int invocation, mc_entity *actor,
                       float yaw) {
    const size_t ceiling = c->state.owner->max_allocation_bytes;
    const size_t used = c->state.owner->allocation_bytes;
    if (used > ceiling || sizeof(*c) > ceiling - used ||
        c->pickup_count >= (ceiling - used - sizeof(*c)) / sizeof(mc_effect_pickup_draw_record))
        return failed(c, invocation, MC_EFFECT_DRAW_LIMIT,
                      "EM reached per-invocation draw diagnostic extent", 286, true);
    mc_effect_pickup_draw_record *record = calloc(1, sizeof(*record));
    if (record == NULL)
        return failed(c, invocation, MC_EFFECT_DRAW_ALLOCATION,
                      "EM reached per-invocation draw diagnostic allocation", 286, true);
    if (c->pickup_last == NULL)
        c->pickup_first = record;
    else
        c->pickup_last->next = record;
    c->pickup_last = record;
    ++c->pickup_count;
    record->value.attempted = true;
    ++c->state.pickup_draw_attempts;
    const mc_client_manager_draw_services services = {.math = c->math,
                                                      .graphics = c->graphics,
                                                      .display = c->services.display,
                                                      .application_classes =
                                                          c->services.application_classes};
    c->failure.manager_draw = &c->manager_failure;
    const mc_client_manager_status status = mc_client_render_manager_coordinates_draw_begin(
        c->manager, actor, c->state.draw_x, c->state.draw_y, c->state.draw_z, yaw, c->state.partial,
        &services, NULL, &c->manager_draw, &c->manager_failure);
    draw_receipt(c, actor);
    if (status != MC_CLIENT_MANAGER_OK) {
        c->failure.source_exception = c->manager_failure.source_exception;
        c->failure.exception_class = c->manager_failure.exception_class;
        c->failure.throwable = c->manager_failure.native_policy ? MC_PARTICLE_NO_THROWABLE
                               : c->manager_failure.unclassified_throwable
                                   ? MC_PARTICLE_UNCLASSIFIED_THROWABLE
                               : c->manager_failure.source_exception ? MC_PARTICLE_SOURCE_EXCEPTION
                                                                     : MC_PARTICLE_SOURCE_ERROR;
        return failed(c, invocation,
                      status == MC_CLIENT_MANAGER_BUSY    ? MC_EFFECT_DRAW_BUSY
                      : status == MC_CLIENT_MANAGER_LIMIT ? MC_EFFECT_DRAW_LIMIT
                                                          : MC_EFFECT_DRAW_CHILD_FAILED,
                      c->manager_failure.operation, 286, c->manager_failure.native_policy);
    }
    if (!record->value.returned)
        return failed(c, invocation, MC_EFFECT_DRAW_CHILD_FAILED,
                      "EM actual TH coordinate body did not return", 286, true);
    ++c->state.pickup_draw_returns;
    if (mc_client_render_manager_draw_retire(&c->manager_draw, &c->manager_cleanup) !=
        MC_CLIENT_MANAGER_OK) {
        c->failure.manager_draw = &c->manager_cleanup;
        return failed(c, invocation, MC_EFFECT_DRAW_CLEANUP_FAILED, c->manager_cleanup.operation,
                      286, true);
    }
    draw_retired(c);
    c->failure.manager_draw = NULL;
    if (mc_client_render_manager_release(&c->manager, &c->failure.manager) != MC_CLIENT_MANAGER_OK)
        return failed(c, invocation, MC_EFFECT_DRAW_CLEANUP_FAILED,
                      "EM completed actual TH receiver retirement", 286, true);
    c->state.particle_pc = 289;
    return true;
}

bool mc_effect_draw_particle(mc_client_effect_draw_call *c, unsigned int invocation) {
    if (c->current == NULL)
        return mc_effect_draw_fail(c, MC_EFFECT_DRAW_SOURCE_NULL,
                                   "actual DN particle draw receiver", invocation, false);
    if ((mc_entity_is_kind(c->current, MC_ENTITY_CLASS_KIND_SMOKE_PARTICLE) ||
         mc_entity_is_kind(c->current, MC_ENTITY_CLASS_KIND_FLAME_PARTICLE)) &&
        c->current->particle != NULL)
        return mc_effect_draw_torch_particle(c, invocation);
    if (mc_entity_is_kind(c->current, MC_ENTITY_CLASS_KIND_EXPLOSION_PARTICLE) &&
        c->current->particle != NULL)
        return mc_effect_draw_sprite(c, invocation);
    if ((mc_entity_is_kind(c->current, MC_ENTITY_CLASS_KIND_ITEM_PARTICLE) ||
         mc_entity_is_kind(c->current, MC_ENTITY_CLASS_KIND_DIGGING_PARTICLE)) &&
        c->current->particle != NULL)
        return mc_effect_draw_item(c, invocation);
    if (!mc_entity_is_kind(c->current, MC_ENTITY_CLASS_KIND_PICKUP_PARTICLE) ||
        c->current->particle == NULL)
        return mc_effect_draw_fail(c, MC_EFFECT_DRAW_DEPENDENCY,
                                   "actual particle concrete draw body", invocation, true);
    mc_pickup_particle_fields *f = &c->current->particle->pickup;
    const int32_t p = f->p; /* EM field1 before i2f4. */
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return failed(c, invocation, MC_EFFECT_DRAW_UNSUPPORTED_FP,
                      "EM i2f reached strict floating controls", 4, true);
    volatile float numerator = mc_client_interpolation_fadd((float)p, c->state.partial);
    const float denominator = (float)f->q;
    const float fraction = at_fdiv(numerator, denominator);
    c->state.fraction = mc_client_interpolation_fmul(fraction, fraction);
    c->state.particle_pc = 20;
    double picked[3], target[3];
    static const unsigned int picked_pc[] = {26, 35, 44};
    static const unsigned int target_pc[] = {53, 81, 115};
    static const int32_t sub[] = {70, 98, 132}, widen[] = {72, 100, 134};
    static const int32_t mul[] = {73, 101, 135}, add[] = {74, 102, 136};
    for (unsigned int axis = 0; axis < 3; ++axis) {
        mc_entity *actor = f->a; /* Each actual picked field is independently reloaded. */
        if (actor == NULL)
            return failed(c, invocation, MC_EFFECT_DRAW_SOURCE_NULL,
                          "actual EM picked coordinate receiver", picked_pc[axis], false);
        picked[axis] = axis == 0 ? actor->base.x : axis == 1 ? actor->base.y : actor->base.z;
    }
    for (unsigned int axis = 0; axis < 3; ++axis) {
        mc_entity *actor = f->o;
        if (actor == NULL)
            return failed(c, invocation, MC_EFFECT_DRAW_SOURCE_NULL,
                          "actual EM target coordinate receiver", target_pc[axis], false);
        const double previous = axis == 0   ? actor->base.bl
                                : axis == 1 ? actor->base.bm
                                            : actor->base.bn;
        actor = f->o;
        const double current = axis == 0   ? actor->base.x
                               : axis == 1 ? actor->base.y
                                           : actor->base.z;
        if (!interpolate(c, invocation, previous, current, c->state.partial, sub[axis], widen[axis],
                         mul[axis], add[axis], &target[axis]))
            return false;
        if (axis == 1) {
            const double offset = mc_client_interpolation_widen(f->r);
            mc_client_interpolation_error arithmetic;
            if (!mc_client_interpolation_double_nan_admit(target[axis], offset, 108, &arithmetic))
                return failed(c, invocation, MC_EFFECT_DRAW_UNSUPPORTED_NUMERIC,
                              "EM offset unmeasured signaling operands", 108, true);
            target[axis] = mc_client_interpolation_dadd(target[axis], offset);
        }
    }
    if (!interpolate(c, invocation, picked[0], target[0], c->state.fraction, 145, 148, 149, 150,
                     &c->state.draw_x) ||
        !interpolate(c, invocation, picked[1], target[1], c->state.fraction, 159, 162, 163, 164,
                     &c->state.draw_y) ||
        !interpolate(c, invocation, picked[2], target[2], c->state.fraction, 173, 176, 177, 178,
                     &c->state.draw_z))
        return false;
    if (!math(c, invocation, 183))
        return false;
    const int32_t x = mc_math_floor_double(c->state.draw_x);
    const double half_offset =
        mc_client_interpolation_widen(at_fdiv(c->current->base.offset, 2.0f));
    mc_client_interpolation_error arithmetic;
    if (!mc_client_interpolation_double_nan_admit(c->state.draw_y, half_offset, 197, &arithmetic))
        return failed(c, invocation, MC_EFFECT_DRAW_UNSUPPORTED_NUMERIC,
                      "EM brightness height unmeasured signaling operands", 197, true);
    const double light_y = mc_client_interpolation_dadd(c->state.draw_y, half_offset);
    if (!math(c, invocation, 198))
        return false;
    const int32_t y = mc_math_floor_double(light_y);
    if (!math(c, invocation, 205))
        return false;
    const int32_t z = mc_math_floor_double(c->state.draw_z);
    mc_world_runtime *world = c->current->base.world; /* Actual aI211. */
    if (world == NULL)
        return failed(c, invocation, MC_EFFECT_DRAW_SOURCE_NULL,
                      "actual EM World brightness receiver", 220, false);
    if (c->world_binding == NULL) {
        if (!mc_world_runtime_binding_retain(world))
            return failed(c, invocation, MC_EFFECT_DRAW_BUSY, "EM reached World native occurrence",
                          220, true);
        c->world_binding = world;
    } else if (c->world_binding != world) {
        return failed(c, invocation, MC_EFFECT_DRAW_DEPENDENCY,
                      "EM additional returned World draw closure", 220, true);
    }
    if (mc_world_runtime_brightness(world, x, y, z, &c->state.brightness, &c->failure.brightness) !=
        MC_WORLD_STORAGE_OK)
        return failed(c, invocation, MC_EFFECT_DRAW_CHILD_FAILED, c->failure.brightness.operation,
                      220, c->failure.brightness.native_policy);
    c->state.brightness_returned = true;
    mc_particle_origin origin;
    if (mc_entity_particle_origin_read(c->state.owner->classes, &origin, &c->failure.entity) !=
        MC_ENTITY_OK)
        return failed(c, invocation, MC_EFFECT_DRAW_CHILD_FAILED, c->failure.entity.operation, 227,
                      true);
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return failed(c, invocation, MC_EFFECT_DRAW_UNSUPPORTED_FP,
                      "EM subtraction reached strict floating controls", 230, true);
    double *coordinates[] = {&c->state.draw_x, &c->state.draw_y, &c->state.draw_z};
    const double origins[] = {origin.l, origin.m, origin.n};
    static const unsigned int subtraction_pc[] = {230, 238, 246};
    for (unsigned int axis = 0; axis < 3; ++axis) {
        if (!mc_client_interpolation_double_nan_admit(*coordinates[axis], origins[axis],
                                                      (int32_t)subtraction_pc[axis], &arithmetic))
            return failed(c, invocation, MC_EFFECT_DRAW_UNSUPPORTED_NUMERIC,
                          "EM origin subtraction unmeasured signaling operands",
                          subtraction_pc[axis], true);
        *coordinates[axis] = mc_client_interpolation_dsub(*coordinates[axis], origins[axis]);
    }
    if (!mc_client_display_source_color(c->services.display, c->state.brightness,
                                        c->state.brightness, c->state.brightness, 1.0f,
                                        &c->failure.display))
        return failed(c, invocation, MC_EFFECT_DRAW_CHILD_FAILED, "EM actual glColor4f child", 256,
                      true);
    c->state.color_set = true;
    if (c->services.manager == NULL)
        return failed(c, invocation, MC_EFFECT_DRAW_DEPENDENCY, "EM reached actual TH namespace",
                      259, true);
    mc_client_render_manager *manager = NULL;
    if (mc_client_render_manager_static_get(c->services.manager, &manager, &c->failure.manager) !=
        MC_CLIENT_MANAGER_OK)
        return failed(c, invocation, MC_EFFECT_DRAW_CHILD_FAILED, c->failure.manager.operation, 259,
                      c->failure.manager.native_policy);
    if (manager != NULL &&
        mc_client_render_manager_retain(manager, &c->failure.manager) != MC_CLIENT_MANAGER_OK)
        return failed(c, invocation, MC_EFFECT_DRAW_BUSY, c->failure.manager.operation, 259, true);
    c->manager = manager;
    mc_entity *drawn_actor = f->a; /* Actual argument captured at263 before narrowing. */
    /* These actual narrowing/widening operations precede the draw invocation. */
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return failed(c, invocation, MC_EFFECT_DRAW_UNSUPPORTED_FP,
                      "EM narrowing reached strict floating controls", 268, true);
    c->state.draw_x = mc_client_interpolation_widen(at_narrow(c->state.draw_x));
    c->state.draw_y = mc_client_interpolation_widen(at_narrow(c->state.draw_y));
    c->state.draw_z = mc_client_interpolation_widen(at_narrow(c->state.draw_z));
    mc_entity *picked_actor = f->a;
    if (picked_actor == NULL)
        return failed(c, invocation, MC_EFFECT_DRAW_SOURCE_NULL, "actual EM picked yaw receiver",
                      282, false);
    const float yaw = picked_actor->base.yaw;
    if (manager == NULL)
        return failed(c, invocation, MC_EFFECT_DRAW_SOURCE_NULL, "actual EM TH draw receiver", 286,
                      false);
    return draw_actor(c, invocation, drawn_actor, yaw);
}
