#include "particle_internal.h"

#include "../construction_internal.h"
#include "../living/travel_math_internal.h"
#include "mc/core/java/java.h"

static const char *const constructor = "(Lfd;DDDDDD)V";

static void at(mc_entity *p, mc_particle_constructor_error *e, unsigned int pc) {
    p->particle->item_native.item_pc = e->item_pc = pc;
}

static bool failure(mc_particle_constructor_error *e, mc_entity_status status,
                    mc_particle_cause cause, const char *operation, unsigned int pc) {
    (void)mc_particle_constructor_fail(e, status, cause, operation, "gy", "<init>", pc);
    e->source_descriptor = constructor;
    e->item_pc = pc;
    return false;
}

static double widen(float value) {
    return _mm_cvtsd_f64(_mm_cvtss_sd(_mm_setzero_pd(), _mm_set_ss(value)));
}

static float narrow(double value) {
    return _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), _mm_set_sd(value)));
}

static bool floating(mc_particle_constructor_error *e, unsigned int pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           failure(e, MC_ENTITY_UNSUPPORTED_FP, MC_PARTICLE_OWNER,
                   "Explosion particle strict floating environment", pc);
}

static bool math(mc_entity *p, mc_particle_constructor_error *e, unsigned int pc, double *out) {
    at(p, e, pc);
    e->random = mc_java_math_random(p->classes->profile, out);
    if (e->random != MC_JAVA_RANDOM_PROFILE_OK)
        return failure(
            e,
            e->random == MC_JAVA_RANDOM_PROFILE_LIMIT               ? MC_ENTITY_LIMIT
            : e->random == MC_JAVA_RANDOM_PROFILE_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
            : e->random == MC_JAVA_RANDOM_PROFILE_UNSUPPORTED_FP    ? MC_ENTITY_UNSUPPORTED_FP
                                                                    : MC_ENTITY_PROFILE_FAILED,
            MC_PARTICLE_RANDOM, "Explosion particle actual Math.random", pc);
    e->constructor_random.math_values[e->constructor_random.math_count++] = *out;
    return true;
}

static bool own(mc_entity *p, mc_particle_constructor_error *e, unsigned int pc, float *out) {
    at(p, e, pc);
    if (p->random == NULL)
        return failure(e, MC_ENTITY_STATE_ERROR, MC_PARTICLE_NULL_REFERENCE,
                       "Explosion particle null private Random receiver", pc);
    *out = mc_java_random_next_float(p->random);
    e->constructor_random.own_values[e->constructor_random.own_count++] = *out;
    return true;
}

mc_entity_status mc_explosion_particle_allocate(mc_entity_classes *classes, mc_entity **out,
                                                mc_entity_error *error) {
    return mc_entity_allocate_canonical(classes, MC_ENTITY_CLASS_KIND_EXPLOSION_PARTICLE,
                                        "explosion particle allocation", out, error);
}

mc_entity_status mc_explosion_particle_construct_entered(mc_entity *p, mc_world_runtime *world,
                                                         mc_math_classes *helper, double x,
                                                         double y, double z, double vx, double vy,
                                                         double vz, mc_math_classes **capture,
                                                         mc_particle_error *out) {
    if (out == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    mc_particle_constructor_error e = {.native_policy = true};
    const mc_entity_status entered = mc_entity_constructor_begin(
        p, MC_ENTITY_CLASS_KIND_EXPLOSION_PARTICLE, world, "explosion particle constructor",
        "explosion particle World profile", &e.entity);
    if (entered != MC_ENTITY_OK)
        (void)failure(&e, entered, MC_PARTICLE_OWNER, e.entity.operation, 0);
    else {
        at(p, &e, 13);
        if (mc_particle_construct_ancestor(p, world, x, y, z, vx, vy, vz, helper, true, capture,
                                           &e)) {
            const double input[3] = {vx, vy, vz};
            static const unsigned int random_pc[3] = {19, 40, 61};
            static const unsigned int store_pc[3] = {34, 55, 76};
            bool returned = true;
            for (size_t i = 0; i < 3; ++i) {
                double draw;
                if (!math(p, &e, random_pc[i], &draw) || !floating(&e, random_pc[i] + 6u)) {
                    returned = false;
                    break;
                }
                const float centered = narrow(mc_travel_dsub(mc_travel_dmul(draw, 2.0), 1.0));
                const double value =
                    mc_travel_dadd(input[i], widen(mc_travel_fmul(centered, .05f)));
                if (i == 0)
                    p->base.velocity_x = value;
                else if (i == 1)
                    p->base.velocity_y = value;
                else
                    p->base.velocity_z = value;
                at(p, &e, store_pc[i]);
            }
            float draw, first, second;
            if (returned && own(p, &e, 86, &draw) && floating(&e, 91)) {
                const float color = mc_travel_fadd(mc_travel_fmul(draw, .3f), .7f);
                p->particle->fields.k = color;
                at(p, &e, 96);
                p->particle->fields.j = color;
                at(p, &e, 100);
                p->particle->fields.i = color;
                at(p, &e, 103);
                if (own(p, &e, 111, &first) && own(p, &e, 118, &second) && floating(&e, 121)) {
                    p->particle->fields.g =
                        mc_travel_fadd(mc_travel_fmul(mc_travel_fmul(first, second), 6.0f), 1.0f);
                    at(p, &e, 127);
                    if (own(p, &e, 138, &draw) && floating(&e, 145)) {
                        const double denominator =
                            mc_travel_dadd(mc_travel_dmul(widen(draw), .8), .2);
                        const double life =
                            _mm_cvtsd_f64(_mm_div_sd(_mm_set_sd(16.0), _mm_set_sd(denominator)));
                        p->particle->fields.f = mc_java_i32_add(mc_java_double_to_i32(life), 2);
                        at(p, &e, 154);
                        at(p, &e, 157);
                    }
                }
            }
        }
        (void)mc_entity_constructor_finish(p, e.status, &e.entity);
        if (e.status == MC_ENTITY_OK) {
            e.source_class = "gy";
            e.source_method = "<init>";
            e.source_descriptor = constructor;
            e.source_pc = 157;
        }
    }
    mc_particle_constructor_error_publish(out, &e);
    return e.status;
}

mc_entity_status mc_explosion_particle_construct(mc_entity *p, mc_world_runtime *world,
                                                 mc_math_classes *math_helper, double x, double y,
                                                 double z, double vx, double vy, double vz,
                                                 mc_particle_error *error) {
    return mc_explosion_particle_construct_entered(p, world, math_helper, x, y, z, vx, vy, vz, NULL,
                                                   error);
}

static mc_entity_status tick_failure(mc_particle_error *e, mc_entity_status status,
                                     mc_particle_cause cause, const char *operation,
                                     unsigned int pc) {
    e->status = status;
    e->cause = cause;
    e->operation = operation;
    e->source_class = "gy";
    e->source_method = "w_";
    e->source_descriptor = "()V";
    e->source_pc = e->tick_pc = pc;
    return status;
}

mc_entity_status mc_explosion_particle_tick(mc_entity *p, mc_registry_classes *registry,
                                            const mc_entity_move_limits *limits,
                                            mc_item_particle_tick_error *error) {
    if (error == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    mc_item_particle_tick_error_init(error);
    mc_particle_error *e = &error->particle;
    if (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_EXPLOSION_PARTICLE) || p->particle == NULL)
        return tick_failure(e, MC_ENTITY_INVALID_ARGUMENT, MC_PARTICLE_OWNER,
                            "Explosion tick actual GY owner", 0);
    if (p->phase != MC_ENTITY_READY || p->particle->item_native.tick_active)
        return tick_failure(
            e, p->particle->item_native.tick_active ? MC_ENTITY_BUSY : MC_ENTITY_INVALID_PHASE,
            MC_PARTICLE_OWNER, "Explosion tick inactive READY owner", 0);
    const mc_entity_status held = mc_entity_retain(p);
    if (held != MC_ENTITY_OK)
        return tick_failure(e, held, MC_PARTICLE_OWNER, "Explosion tick native occurrence", 0);
    p->particle->item_native.tick_active = true;
    mc_particle_fields *f = &p->particle->fields;
    p->base.aJ = p->base.x;
    p->particle->item_native.tick_pc = 5;
    p->base.aK = p->base.y;
    p->particle->item_native.tick_pc = 13;
    p->base.aL = p->base.z;
    p->particle->item_native.tick_pc = 21;
    const int32_t old_age = f->e;
    f->e = mc_java_i32_add(f->e, 1);
    p->particle->item_native.tick_pc = 32;
    if (old_age >= f->f) {
        p->base.be = true;
        p->particle->item_native.tick_pc = 43;
    }
    int32_t quotient;
    if (!mc_java_i32_div(mc_java_i32_mul(f->e, 8), f->f, &quotient)) {
        (void)tick_failure(e, MC_ENTITY_STATE_ERROR, MC_PARTICLE_OWNER,
                           "Explosion tick actual texture integer division by zero", 60);
        e->native_policy = false;
        e->source_exception = true;
        e->throwable = MC_PARTICLE_SOURCE_EXCEPTION;
        e->exception_class = "java/lang/ArithmeticException";
        goto done;
    }
    f->b = mc_java_i32_sub(7, quotient);
    p->particle->item_native.tick_pc = 62;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        (void)tick_failure(e, MC_ENTITY_UNSUPPORTED_FP, MC_PARTICLE_OWNER,
                           "Explosion tick strict floating environment", 73);
        goto done;
    }
    p->base.velocity_y = mc_travel_dadd(p->base.velocity_y, .004);
    p->particle->item_native.tick_pc = 74;
    e->status = mc_entity_move_in_context(p, registry, p->base.velocity_x, p->base.velocity_y,
                                          p->base.velocity_z, limits, &error->movement);
    if (e->status != MC_ENTITY_OK) {
        (void)tick_failure(e, e->status, MC_PARTICLE_MOVEMENT, error->movement.operation, 90);
        mc_particle_movement_exception(e, &error->movement, registry);
        goto done;
    }
    p->base.velocity_x = mc_travel_dmul(p->base.velocity_x, .8999999761581421);
    p->particle->item_native.tick_pc = 102;
    p->base.velocity_y = mc_travel_dmul(p->base.velocity_y, .8999999761581421);
    p->particle->item_native.tick_pc = 114;
    p->base.velocity_z = mc_travel_dmul(p->base.velocity_z, .8999999761581421);
    p->particle->item_native.tick_pc = 126;
    if (p->base.aX) {
        p->base.velocity_x = mc_travel_dmul(p->base.velocity_x, .699999988079071);
        p->particle->item_native.tick_pc = 145;
        p->base.velocity_z = mc_travel_dmul(p->base.velocity_z, .699999988079071);
        p->particle->item_native.tick_pc = 157;
    }
    e->source_class = "gy";
    e->source_method = "w_";
    e->source_descriptor = "()V";
    e->source_pc = e->tick_pc = p->particle->item_native.tick_pc = 160;
done:
    p->particle->item_native.tick_active = false;
    mc_entity_release(p);
    return e->status;
}

mc_entity_status mc_explosion_particle_layer(const mc_entity *p, int32_t *out) {
    if (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_EXPLOSION_PARTICLE) || out == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    *out = 0; /* Actual inherited XW.c_ literal0/return1. */
    return MC_ENTITY_OK;
}
