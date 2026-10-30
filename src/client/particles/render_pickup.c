#include "render_internal.h"

#include "../../entity/entity_internal.h"
#include "../../entity/particles/particle_internal.h"
#include "../display/source_surface.h"
#include "../render/interpolation_internal.h"
#include "mc/core/math/math.h"

static const char particle_descriptor[] = "(Lnw;FFFFFF)V";

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

bool mc_effect_draw_particle(mc_client_effect_draw_call *c, unsigned int invocation) {
    if (c->current == NULL)
        return mc_effect_draw_fail(c, MC_EFFECT_DRAW_SOURCE_NULL,
                                   "actual DN particle draw receiver", invocation, false);
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
    (void)yaw;
    if (manager == NULL)
        return failed(c, invocation, MC_EFFECT_DRAW_SOURCE_NULL, "actual EM TH draw receiver", 286,
                      false);
    return failed(c, invocation, MC_EFFECT_DRAW_DEPENDENCY,
                  "actual TH Entity renderer draw child is not connected", 286, true);
}
