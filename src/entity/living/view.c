#include "mc/entity/living/view.h"

#include "../entity_internal.h"
#include "../player/tick_math_internal.h"

#include <stdlib.h>
#include <string.h>

struct mc_entity_living_ray_call {
    mc_entity_living_ray_state state;
    mc_entity *receiver;
    mc_entity_classes *classes;
    mc_math_classes *math;
    mc_world_runtime *world;
    mc_world_raytrace_call *world_call;
    bool entity_held, world_held;
    mc_entity_view_error failure, cleanup;
};

static mc_entity_view_status fail(mc_entity_view_error *e, mc_entity_view_status status,
                                  const char *operation, const char *method, const char *descriptor,
                                  int pc, bool native, bool entered) {
    e->status = status;
    e->operation = operation;
    e->native_policy = native;
    e->site = (mc_entity_view_site){"ls", method, descriptor, pc, entered};
    if (status == MC_ENTITY_VIEW_SOURCE_NULL_REFERENCE) {
        e->source_exception = true;
        e->exception_class = "java/lang/NullPointerException";
    }
    return status;
}

static mc_entity_view_status admit(mc_entity *entity, mc_entity_view_error *e, const char *method,
                                   const char *descriptor) {
    if (entity == NULL)
        return fail(e, MC_ENTITY_VIEW_SOURCE_NULL_REFERENCE, "actual LivingEntity null receiver",
                    method, descriptor, 0, false, false);
    const mc_entity_class_literal *living = NULL;
    bool matches = false;
    e->native_status =
        mc_entity_class_literal_get(entity->classes, MC_ENTITY_CLASS_KIND_LIVING_ENTITY, &living);
    if (e->native_status == MC_ENTITY_OK)
        e->native_status =
            mc_entity_class_literal_is_assignable_from(living, entity->class_literal, &matches);
    if (e->native_status != MC_ENTITY_OK || entity->classes->source != MC_ENTITY_CLIENT || !matches)
        return fail(e, MC_ENTITY_VIEW_UNSUPPORTED_SOURCE, "canonical client LivingEntity receiver",
                    method, descriptor, 0, true, false);
    return MC_ENTITY_VIEW_OK;
}

static bool fp(mc_entity_view_error *e, const char *method, int pc) {
    if ((_mm_getcsr() & 0xffc0u) == 0x1f80u)
        return true;
    (void)fail(e, MC_ENTITY_VIEW_UNSUPPORTED_FP, "LivingEntity view strict floating environment",
               method, "(F)Lbt;", pc, true, true);
    return false;
}

static mc_entity_view_status vector_result(mc_entity_view_error *e, mc_entity_status status,
                                           const char *method, const char *descriptor, int pc) {
    if (status == MC_ENTITY_OK)
        return MC_ENTITY_VIEW_OK;
    return fail(e,
                status == MC_ENTITY_LIMIT               ? MC_ENTITY_VIEW_LIMIT
                : status == MC_ENTITY_ALLOCATION_FAILED ? MC_ENTITY_VIEW_ALLOCATION
                : status == MC_ENTITY_UNSUPPORTED_FP    ? MC_ENTITY_VIEW_UNSUPPORTED_FP
                                                        : MC_ENTITY_VIEW_VECTOR_FAILED,
                e->vector.operation, method, descriptor, pc, true, true);
}

static bool output(mc_entity_vector **out, mc_entity_view_error *e) {
    if (e == NULL)
        return false;
    memset(e, 0, sizeof(*e));
    if (out == NULL || *out != NULL) {
        e->status = MC_ENTITY_VIEW_ARGUMENT;
        e->operation = "LivingEntity view disjoint empty output";
        e->native_policy = true;
        return false;
    }
    return true;
}

static double multiply(double left, double right) {
    uint64_t a, b;
    memcpy(&a, &left, sizeof(a));
    memcpy(&b, &right, sizeof(b));
    /* Original LS.e row155: DMUL selects the top/right NaN when both
     * operands are NaNs. Ordered SSE alone selects the other payload. */
    if ((a & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x7ff0000000000000) &&
        (b & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x7ff0000000000000)) {
        b |= UINT64_C(0x0008000000000000);
        memcpy(&right, &b, sizeof(right));
        return right;
    }
    return at_mul(left, right);
}

mc_entity_view_status mc_entity_living_position(mc_entity *entity, float partial,
                                                mc_entity_vector **out, mc_entity_view_error *e) {
    if (!output(out, e))
        return MC_ENTITY_VIEW_ARGUMENT;
    if (admit(entity, e, "e", "(F)Lbt;") != MC_ENTITY_VIEW_OK)
        return e->status;
    e->native_status = mc_entity_retain(entity);
    if (e->native_status != MC_ENTITY_OK)
        return fail(e, MC_ENTITY_VIEW_NATIVE_FAILED, "LivingEntity position native occurrence", "e",
                    "(F)Lbt;", 0, true, false);
    const mc_entity_base_fields *f = &entity->base;
    mc_entity_status status = MC_ENTITY_OK;
    if (partial == 1.0f) {
        status = mc_entity_vector_pool_get(entity->classes, f->x, f->y, f->z, out, &e->vector);
        (void)vector_result(e, status, "e", "(F)Lbt;", 18);
    } else if (fp(e, "e", 34)) {
        const double x = at_add(f->aJ, multiply(at_sub(f->x, f->aJ), at_widen(partial)));
        const double y = at_add(f->aK, multiply(at_sub(f->y, f->aK), at_widen(partial)));
        const double z = at_add(f->aL, multiply(at_sub(f->z, f->aL), at_widen(partial)));
        status = mc_entity_vector_pool_get(entity->classes, x, y, z, out, &e->vector);
        (void)vector_result(e, status, "e", "(F)Lbt;", 83);
    }
    mc_entity_release(entity);
    return e->status;
}

static const mc_math_trig *math_lookup(mc_math_classes *math, mc_entity_view_error *e, int pc) {
    if (mc_math_classes_ensure(math, &e->math) != MC_MATH_CLASS_OK) {
        (void)fail(e, MC_ENTITY_VIEW_MATH_FAILED, e->math.operation, "f", "(F)Lbt;", pc, true,
                   true);
        return NULL;
    }
    return mc_math_classes_table(math);
}

static float yaw_argument(float value) {
    return at_fsub(at_fmul(at_fneg(value), 0.017453292f), 3.1415927f);
}

mc_entity_view_status mc_entity_living_look(mc_entity *entity, mc_math_classes *math, float partial,
                                            mc_entity_vector **out, mc_entity_view_error *e) {
    if (!output(out, e))
        return MC_ENTITY_VIEW_ARGUMENT;
    if (admit(entity, e, "f", "(F)Lbt;") != MC_ENTITY_VIEW_OK)
        return e->status;
    e->native_status = mc_entity_retain(entity);
    if (e->native_status != MC_ENTITY_OK)
        return fail(e, MC_ENTITY_VIEW_NATIVE_FAILED, "LivingEntity look native occurrence", "f",
                    "(F)Lbt;", 0, true, false);
    const mc_entity_base_fields *f = &entity->base;
    if (!fp(e, "f", partial == 1.0f ? 13 : 92))
        goto done;
    float cy, sy, cp, sp;
    const mc_math_trig *table;
    if (partial == 1.0f) {
        const float argument = yaw_argument(f->yaw);
        table = math_lookup(math, e, 17);
        if (table == NULL)
            goto done;
        cy = mc_math_cos(table, argument);
        sy = mc_math_sin(table, yaw_argument(f->yaw));
        cp = at_fneg(mc_math_cos(table, at_fmul(at_fneg(f->aT), 0.017453292f)));
        sp = mc_math_sin(table, at_fmul(at_fneg(f->aT), 0.017453292f));
    } else {
        const float pitch = at_fadd(f->aV, at_fmul(at_fsub(f->aT, f->aV), partial));
        const float yaw = at_fadd(f->aU, at_fmul(at_fsub(f->yaw, f->aU), partial));
        const float argument = yaw_argument(yaw);
        table = math_lookup(math, e, 122);
        if (table == NULL)
            goto done;
        cy = mc_math_cos(table, argument);
        sy = mc_math_sin(table, yaw_argument(yaw));
        cp = at_fneg(mc_math_cos(table, at_fmul(at_fneg(pitch), 0.017453292f)));
        sp = mc_math_sin(table, at_fmul(at_fneg(pitch), 0.017453292f));
    }
    (void)vector_result(e,
                        mc_entity_vector_pool_get(entity->classes, at_widen(at_fmul(sy, cp)),
                                                  at_widen(sp), at_widen(at_fmul(cy, cp)), out,
                                                  &e->vector),
                        "f", "(F)Lbt;", partial == 1.0f ? 76 : 176);
done:
    mc_entity_release(entity);
    return e->status;
}

mc_entity_view_status mc_entity_living_look_current(mc_entity *entity, mc_math_classes *math,
                                                    mc_entity_vector **out,
                                                    mc_entity_view_error *e) {
    if (!output(out, e))
        return MC_ENTITY_VIEW_ARGUMENT;
    if (admit(entity, e, "ac", "()Lbt;") != MC_ENTITY_VIEW_OK)
        return e->status;
    const mc_entity_view_status status = mc_entity_living_look(entity, math, 1.0f, out, e);
    if (e != NULL && status != MC_ENTITY_VIEW_ARGUMENT && status != MC_ENTITY_VIEW_OK)
        e->caller = (mc_entity_view_site){"ls", "ac", "()Lbt;", 2, entity != NULL};
    return status;
}

static bool ray_fail(mc_entity_living_ray_call *c, mc_entity_view_status status, const char *op,
                     int pc, bool native) {
    c->state.source_pc = (unsigned int)pc;
    (void)fail(&c->failure, status, op, "a", "(DF)Lvf;", pc, native, true);
    return false;
}

static void run(mc_entity_living_ray_call *c, double distance, float partial) {
    mc_entity_view_error *e = &c->failure;
    c->state.source_pc = 2;
    if (mc_entity_living_position(c->receiver, partial, &c->state.eye, e) != MC_ENTITY_VIEW_OK) {
        e->caller = (mc_entity_view_site){"ls", "a", "(DF)Lvf;", 2, true};
        return;
    }
    c->state.source_pc = 9;
    if (mc_entity_living_look(c->receiver, c->math, partial, &c->state.look, e) !=
        MC_ENTITY_VIEW_OK) {
        e->caller = (mc_entity_view_site){"ls", "a", "(DF)Lvf;", 9, true};
        return;
    }
    c->state.source_pc = 37;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        (void)ray_fail(c, MC_ENTITY_VIEW_UNSUPPORTED_FP, "ray endpoint strict floating environment",
                       22, true);
        return;
    }
    const mc_entity_vector *look = c->state.look;
    if (vector_result(e,
                      mc_entity_vector_add(c->classes, c->state.eye, multiply(look->x, distance),
                                           multiply(look->y, distance), multiply(look->z, distance),
                                           &c->state.end, &e->vector),
                      "a", "(DF)Lvf;", 37) != MC_ENTITY_VIEW_OK)
        return;
    c->state.source_pc = 43;
    c->world = c->receiver->base.world;
    if (c->world != NULL) {
        if (!mc_world_runtime_binding_retain(c->world)) {
            (void)ray_fail(c, MC_ENTITY_VIEW_NATIVE_FAILED, "captured Entity World occurrence", 43,
                           true);
            return;
        }
        c->world_held = true;
    }
    c->state.source_pc = 50;
    if (c->world == NULL) {
        (void)ray_fail(c, MC_ENTITY_VIEW_SOURCE_NULL_REFERENCE, "actual Entity World ray receiver",
                       50, false);
        return;
    }
    e->world_status = mc_world_raytrace_begin(c->world, c->state.eye, c->state.end, &c->world_call);
    mc_world_raytrace_state world = {0};
    const bool inspected =
        c->world_call != NULL && mc_world_raytrace_inspect(c->world_call, &world);
    if (e->world_status != MC_WORLD_RAYTRACE_OK) {
        const mc_world_raytrace_error *child = inspected ? world.failure : NULL;
        (void)ray_fail(c,
                       child != NULL && child->source_exception && child->exception_class != NULL &&
                               strcmp(child->exception_class, "java/lang/NullPointerException") == 0
                           ? MC_ENTITY_VIEW_SOURCE_NULL_REFERENCE
                           : MC_ENTITY_VIEW_WORLD_FAILED,
                       child == NULL ? "World ray invocation native admission" : child->operation,
                       50, child == NULL || child->native_policy);
        e->world = child;
        if (child != NULL) {
            e->source_exception = child->source_exception;
            e->exception_class = child->exception_class;
        }
        return;
    }
    if (!inspected || !world.source_returned) {
        (void)ray_fail(c, MC_ENTITY_VIEW_WORLD_FAILED, "World ray has no actual normal return", 50,
                       true);
        return;
    }
    if (world.result != NULL) {
        if (mc_entity_hit_result_retain(world.result, &e->hit) != MC_ENTITY_HIT_OK) {
            (void)ray_fail(c, MC_ENTITY_VIEW_HIT_FAILED, "returned World hit native occurrence", 53,
                           true);
            return;
        }
        c->state.result = world.result;
    }
    c->state.source_pc = 53;
    c->state.source_returned = true;
}

mc_entity_view_status mc_entity_living_ray_begin(mc_entity *entity, mc_math_classes *math,
                                                 double distance, float partial,
                                                 mc_entity_living_ray_call **out) {
    if (out == NULL || *out != NULL)
        return MC_ENTITY_VIEW_ARGUMENT;
    mc_entity_living_ray_call *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return MC_ENTITY_VIEW_ALLOCATION;
    *out = c;
    c->state.active = true;
    if (entity != NULL) {
        c->failure.native_status = mc_entity_retain(entity);
        if (c->failure.native_status != MC_ENTITY_OK) {
            (void)ray_fail(c, MC_ENTITY_VIEW_NATIVE_FAILED, "LivingEntity ray native occurrence", 0,
                           true);
            c->failure.site.entered = false;
            goto done;
        }
        c->entity_held = true;
        c->receiver = entity;
        c->classes = entity->classes;
    }
    if (admit(entity, &c->failure, "a", "(DF)Lvf;") != MC_ENTITY_VIEW_OK)
        goto done;
    if (math != NULL) {
        if (mc_math_classes_retain(math, &c->failure.math) != MC_MATH_CLASS_OK) {
            (void)ray_fail(c, MC_ENTITY_VIEW_NATIVE_FAILED, "ray MathHelper native occurrence", 0,
                           true);
            c->failure.site.entered = false;
            goto done;
        }
        c->math = math;
    }
    run(c, distance, partial);
done:
    c->state.active = false;
    c->state.status = c->failure.status;
    return c->failure.status;
}

bool mc_entity_living_ray_inspect(const mc_entity_living_ray_call *c,
                                  mc_entity_living_ray_state *out) {
    if (c == NULL || out == NULL || c->state.active)
        return false;
    *out = c->state;
    out->receiver = c->receiver;
    out->math = c->math;
    out->world = c->world;
    out->world_call = c->world_call;
    out->failure = &c->failure;
    out->cleanup = &c->cleanup;
    return true;
}

mc_entity_view_status mc_entity_living_ray_retire(mc_entity_living_ray_call **slot) {
    if (slot == NULL)
        return MC_ENTITY_VIEW_ARGUMENT;
    mc_entity_living_ray_call *c = *slot;
    if (c == NULL)
        return MC_ENTITY_VIEW_OK;
    if (c->state.active)
        return MC_ENTITY_VIEW_BUSY;
    c->state.retiring = true;
    mc_entity_view_error *e = &c->cleanup;
    memset(e, 0, sizeof(*e));
    if (mc_entity_hit_result_release(&c->state.result, &e->hit) != MC_ENTITY_HIT_OK)
        goto failed;
    e->world_status = mc_world_raytrace_retire(&c->world_call);
    if (e->world_status != MC_WORLD_RAYTRACE_OK) {
        mc_world_raytrace_state state;
        if (mc_world_raytrace_inspect(c->world_call, &state))
            e->world = state.cleanup;
        goto failed;
    }
    c->failure.world = NULL; /* Consumed native child view; source failure itself stays. */
    mc_entity_vector **vectors[] = {&c->state.end, &c->state.look, &c->state.eye};
    for (size_t i = 0; i < 3; ++i) {
        if (*vectors[i] != NULL) {
            e->native_status = mc_entity_vector_release(c->classes, *vectors[i]);
            if (e->native_status != MC_ENTITY_OK)
                goto failed;
            *vectors[i] = NULL;
        }
    }
    if (c->world_held) {
        mc_world_runtime_binding_release(c->world);
        c->world_held = false;
    }
    c->world = NULL;
    if (c->entity_held) {
        mc_entity_release(c->receiver);
        c->entity_held = false;
    }
    c->receiver = NULL;
    c->classes = NULL;
    if (mc_math_classes_release(&c->math, &e->math) != MC_MATH_CLASS_OK)
        goto failed;
    free(c);
    *slot = NULL;
    return MC_ENTITY_VIEW_OK;
failed:
    e->status = MC_ENTITY_VIEW_CLEANUP_FAILED;
    e->native_policy = true;
    e->operation = "LivingEntity ray checked native occurrence cleanup";
    return e->status;
}
