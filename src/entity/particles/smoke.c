#include "particle_internal.h"

#include "../construction_internal.h"
#include "../living/travel_math_internal.h"
#include "mc/core/java/java.h"

static const char scaled_descriptor[] = "(Lfd;DDDDDDF)V";

_Static_assert(sizeof(((mc_particle_constructor_random *)0)->math_values) / sizeof(double) >= 7,
               "Actual XW5 plus XN2 Math draws must all fit");

static void at(mc_entity *p, mc_particle_constructor_error *e, unsigned int pc) {
    p->particle->item_native.item_pc = e->item_pc = pc;
}

static bool fail(mc_particle_constructor_error *e, mc_entity_status status, mc_particle_cause cause,
                 const char *operation, unsigned int pc) {
    (void)mc_particle_constructor_fail(e, status, cause, operation, "xn", "<init>", pc);
    e->source_descriptor = scaled_descriptor;
    e->item_pc = pc;
    return false;
}

static bool floating(mc_particle_constructor_error *e, unsigned int pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(e, MC_ENTITY_UNSUPPORTED_FP, MC_PARTICLE_OWNER,
                "Smoke constructor strict floating environment", pc);
}

static bool draw(mc_entity *p, mc_particle_constructor_error *e, unsigned int pc, double *out) {
    at(p, e, pc);
    e->random = mc_java_math_random(p->classes->profile, out);
    if (e->random != MC_JAVA_RANDOM_PROFILE_OK)
        return fail(e,
                    e->random == MC_JAVA_RANDOM_PROFILE_LIMIT ? MC_ENTITY_LIMIT
                    : e->random == MC_JAVA_RANDOM_PROFILE_ALLOCATION_FAILED
                        ? MC_ENTITY_ALLOCATION_FAILED
                    : e->random == MC_JAVA_RANDOM_PROFILE_UNSUPPORTED_FP ? MC_ENTITY_UNSUPPORTED_FP
                                                                         : MC_ENTITY_PROFILE_FAILED,
                    MC_PARTICLE_RANDOM, "Smoke actual Math.random", pc);
    if (e->constructor_random.math_count >=
        sizeof(e->constructor_random.math_values) / sizeof(e->constructor_random.math_values[0]))
        return fail(e, MC_ENTITY_LIMIT, MC_PARTICLE_OWNER,
                    "Smoke native constructor Math receipt extent", pc);
    e->constructor_random.math_values[e->constructor_random.math_count++] = *out;
    return true;
}

mc_entity_status mc_smoke_particle_allocate(mc_entity_classes *classes, mc_entity **out,
                                            mc_entity_error *error) {
    return mc_entity_allocate_canonical(classes, MC_ENTITY_CLASS_KIND_SMOKE_PARTICLE,
                                        "Smoke particle allocation", out, error);
}

static mc_entity_status construct(mc_entity *p, mc_world_runtime *world, mc_math_classes *math,
                                  double x, double y, double z, double vx, double vy, double vz,
                                  float scale, bool wrapper, mc_math_classes **capture,
                                  mc_particle_error *out) {
    if (out == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    mc_particle_constructor_error e = {.native_policy = true};
    const mc_entity_status entered = mc_entity_constructor_begin(
        p, MC_ENTITY_CLASS_KIND_SMOKE_PARTICLE, world, "Smoke particle constructor",
        "Smoke particle World profile", &e.entity);
    if (entered != MC_ENTITY_OK) {
        (void)fail(&e, entered, MC_PARTICLE_OWNER, e.entity.operation, 0);
        if (wrapper)
            e.source_descriptor = "(Lfd;DDDDDD)V";
    } else {
        at(p, &e, 10);
        if (!mc_particle_construct_ancestor(p, world, x, y, z, 0, 0, 0, math, true, capture, &e) ||
            !floating(&e, 21))
            goto finish;
        p->base.velocity_x = mc_travel_dmul(p->base.velocity_x, .10000000149011612);
        at(p, &e, 22);
        p->base.velocity_y = mc_travel_dmul(p->base.velocity_y, .10000000149011612);
        at(p, &e, 34);
        p->base.velocity_z = mc_travel_dmul(p->base.velocity_z, .10000000149011612);
        at(p, &e, 46);
        p->base.velocity_x = mc_travel_dadd(p->base.velocity_x, vx);
        at(p, &e, 57);
        p->base.velocity_y = mc_travel_dadd(p->base.velocity_y, vy);
        at(p, &e, 68);
        p->base.velocity_z = mc_travel_dadd(p->base.velocity_z, vz);
        at(p, &e, 79);
        double value;
        if (!draw(p, &e, 85, &value) || !floating(&e, 91))
            goto finish;
        const float color = _mm_cvtss_f32(
            _mm_cvtsd_ss(_mm_setzero_ps(), _mm_set_sd(mc_travel_dmul(value, .30000001192092896))));
        p->particle->fields.k = color;
        at(p, &e, 94);
        p->particle->fields.j = color;
        at(p, &e, 98);
        p->particle->fields.i = color;
        at(p, &e, 101);
        p->particle->fields.g = mc_travel_fmul(p->particle->fields.g, .75f);
        at(p, &e, 112);
        p->particle->fields.g = mc_travel_fmul(p->particle->fields.g, scale);
        at(p, &e, 123);
        p->particle->smoke.a = p->particle->fields.g;
        at(p, &e, 131);
        if (!draw(p, &e, 138, &value) || !floating(&e, 144))
            goto finish;
        const double denominator = mc_travel_dadd(mc_travel_dmul(value, .8), .2);
        const double lifetime = _mm_cvtsd_f64(_mm_div_sd(_mm_set_sd(8), _mm_set_sd(denominator)));
        p->particle->fields.f = mc_java_double_to_i32(lifetime);
        at(p, &e, 151);
        p->particle->fields.f =
            mc_java_float_to_i32(mc_travel_fmul((float)p->particle->fields.f, scale));
        at(p, &e, 164);
        p->base.bq = false;
        at(p, &e, 169);
        at(p, &e, wrapper ? 17u : 172u);
    finish:
        (void)mc_entity_constructor_finish(p, e.status, &e.entity);
        if (e.status == MC_ENTITY_OK) {
            e.source_class = "xn";
            e.source_method = "<init>";
            e.source_descriptor = wrapper ? "(Lfd;DDDDDD)V" : scaled_descriptor;
            e.source_pc = wrapper ? 17u : 172u;
        }
    }
    mc_particle_constructor_error_publish(out, &e);
    return e.status;
}

mc_entity_status mc_smoke_particle_construct_entered(mc_entity *p, mc_world_runtime *world,
                                                     mc_math_classes *math, double x, double y,
                                                     double z, double vx, double vy, double vz,
                                                     mc_math_classes **capture,
                                                     mc_particle_error *error) {
    return construct(p, world, math, x, y, z, vx, vy, vz, 1, true, capture, error);
}

mc_entity_status mc_smoke_particle_construct_scaled_entered(mc_entity *p, mc_world_runtime *world,
                                                            mc_math_classes *math, double x,
                                                            double y, double z, double vx,
                                                            double vy, double vz, float scale,
                                                            mc_math_classes **capture,
                                                            mc_particle_error *error) {
    return construct(p, world, math, x, y, z, vx, vy, vz, scale, false, capture, error);
}

mc_entity_status mc_smoke_particle_construct(mc_entity *p, mc_world_runtime *world,
                                             mc_math_classes *math, double x, double y, double z,
                                             double vx, double vy, double vz,
                                             mc_particle_error *error) {
    return mc_smoke_particle_construct_entered(p, world, math, x, y, z, vx, vy, vz, NULL, error);
}

mc_entity_status mc_smoke_particle_construct_scaled(mc_entity *p, mc_world_runtime *world,
                                                    mc_math_classes *math, double x, double y,
                                                    double z, double vx, double vy, double vz,
                                                    float scale, mc_particle_error *error) {
    return mc_smoke_particle_construct_scaled_entered(p, world, math, x, y, z, vx, vy, vz, scale,
                                                      NULL, error);
}

static mc_entity_status tick_fail(mc_particle_error *e, mc_entity_status status,
                                  mc_particle_cause cause, const char *operation, unsigned int pc) {
    e->status = status;
    e->cause = cause;
    e->operation = operation;
    e->source_class = "xn";
    e->source_method = "w_";
    e->source_descriptor = "()V";
    e->source_pc = e->tick_pc = pc;
    return status;
}

mc_entity_status mc_smoke_particle_tick(mc_entity *p, mc_registry_classes *registry,
                                        const mc_entity_move_limits *limits,
                                        mc_item_particle_tick_error *error) {
    if (error == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    mc_item_particle_tick_error_init(error);
    mc_particle_error *e = &error->particle;
    if (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_SMOKE_PARTICLE) || p->particle == NULL)
        return tick_fail(e, MC_ENTITY_INVALID_ARGUMENT, MC_PARTICLE_OWNER,
                         "Smoke tick canonical XN storage", 0);
    if (p->phase != MC_ENTITY_READY || p->particle->item_native.tick_active)
        return tick_fail(
            e, p->particle->item_native.tick_active ? MC_ENTITY_BUSY : MC_ENTITY_INVALID_PHASE,
            MC_PARTICLE_OWNER, "Smoke tick inactive READY owner", 0);
    const mc_entity_status held = mc_entity_retain(p);
    if (held != MC_ENTITY_OK)
        return tick_fail(e, held, MC_PARTICLE_OWNER, "Smoke tick native occurrence", 0);
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
        (void)tick_fail(e, MC_ENTITY_STATE_ERROR, MC_PARTICLE_OWNER,
                        "Smoke texture actual integer division by zero", 60);
        e->native_policy = false;
        e->source_exception = true;
        e->throwable = MC_PARTICLE_SOURCE_EXCEPTION;
        e->exception_class = "java/lang/ArithmeticException";
        goto done;
    }
    f->b = mc_java_i32_sub(7, quotient);
    p->particle->item_native.tick_pc = 62;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        (void)tick_fail(e, MC_ENTITY_UNSUPPORTED_FP, MC_PARTICLE_OWNER,
                        "Smoke tick strict floating environment", 73);
        goto done;
    }
    p->base.velocity_y = mc_travel_dadd(p->base.velocity_y, .004);
    p->particle->item_native.tick_pc = 74;
    e->status = mc_entity_move_in_context(p, registry, p->base.velocity_x, p->base.velocity_y,
                                          p->base.velocity_z, limits, &error->movement);
    if (e->status != MC_ENTITY_OK) {
        (void)tick_fail(e, e->status, MC_PARTICLE_MOVEMENT, error->movement.operation, 90);
        mc_particle_movement_exception(e, &error->movement, registry);
        goto done;
    }
    if (p->base.y == p->base.aK) {
        p->base.velocity_x = mc_travel_dmul(p->base.velocity_x, 1.1);
        p->particle->item_native.tick_pc = 114;
        p->base.velocity_z = mc_travel_dmul(p->base.velocity_z, 1.1);
        p->particle->item_native.tick_pc = 126;
    }
    p->base.velocity_x = mc_travel_dmul(p->base.velocity_x, .9599999785423279);
    p->particle->item_native.tick_pc = 138;
    p->base.velocity_y = mc_travel_dmul(p->base.velocity_y, .9599999785423279);
    p->particle->item_native.tick_pc = 150;
    p->base.velocity_z = mc_travel_dmul(p->base.velocity_z, .9599999785423279);
    p->particle->item_native.tick_pc = 162;
    if (p->base.aX) {
        p->base.velocity_x = mc_travel_dmul(p->base.velocity_x, .699999988079071);
        p->particle->item_native.tick_pc = 181;
        p->base.velocity_z = mc_travel_dmul(p->base.velocity_z, .699999988079071);
        p->particle->item_native.tick_pc = 193;
    }
    (void)tick_fail(e, MC_ENTITY_OK, MC_PARTICLE_NONE, NULL, 196);
    p->particle->item_native.tick_pc = 196;
done:
    p->particle->item_native.tick_active = false;
    mc_entity_release(p);
    return e->status;
}

mc_entity_status mc_smoke_particle_layer(const mc_entity *p, int32_t *out) {
    if (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_SMOKE_PARTICLE) || out == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    *out = 0;
    return MC_ENTITY_OK;
}
