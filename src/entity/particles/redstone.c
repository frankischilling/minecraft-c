#include "particle_internal.h"

#include "../construction_internal.h"
#include "../living/travel_math_internal.h"
#include "mc/core/java/java.h"

static const char scaled_descriptor[] = "(Lfd;DDDFFFF)V";

_Static_assert(sizeof(((mc_particle_constructor_random *)0)->math_values) / sizeof(double) >= 10,
               "Actual XW5 plus IM5 Math draws must all fit");

static void at(mc_entity *p, mc_particle_constructor_error *e, unsigned int pc) {
    p->particle->item_native.item_pc = e->item_pc = pc;
}

static bool fail(mc_particle_constructor_error *e, mc_entity_status status, mc_particle_cause cause,
                 const char *operation, unsigned int pc) {
    (void)mc_particle_constructor_fail(e, status, cause, operation, "im", "<init>", pc);
    e->source_descriptor = scaled_descriptor;
    e->item_pc = pc;
    return false;
}

static bool floating(mc_particle_constructor_error *e, unsigned int pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(e, MC_ENTITY_UNSUPPORTED_FP, MC_PARTICLE_OWNER,
                "Redstone constructor strict floating environment", pc);
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
                    MC_PARTICLE_RANDOM, "Redstone actual Math.random", pc);
    if (e->constructor_random.math_count >=
        sizeof(e->constructor_random.math_values) / sizeof(e->constructor_random.math_values[0]))
        return fail(e, MC_ENTITY_LIMIT, MC_PARTICLE_OWNER,
                    "Redstone native constructor Math receipt extent", pc);
    e->constructor_random.math_values[e->constructor_random.math_count++] = *out;
    return true;
}

mc_entity_status mc_redstone_particle_allocate(mc_entity_classes *classes, mc_entity **out,
                                               mc_entity_error *error) {
    return mc_entity_allocate_canonical(classes, MC_ENTITY_CLASS_KIND_REDSTONE_PARTICLE,
                                        "Redstone particle allocation", out, error);
}

static float narrow(double value) {
    return _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), _mm_set_sd(value)));
}

static mc_entity_status construct(mc_entity *p, mc_world_runtime *world, mc_math_classes *math,
                                  double x, double y, double z, float scale, float red, float green,
                                  float blue, bool wrapper, mc_math_classes **capture,
                                  mc_particle_error *out) {
    if (out == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    mc_particle_constructor_error e = {.native_policy = true};
    const mc_entity_status entered = mc_entity_constructor_begin(
        p, MC_ENTITY_CLASS_KIND_REDSTONE_PARTICLE, world, "Redstone particle constructor",
        "Redstone particle World profile", &e.entity);
    if (entered != MC_ENTITY_OK) {
        (void)fail(&e, entered, MC_PARTICLE_OWNER, e.entity.operation, 0);
        if (wrapper)
            e.source_descriptor = "(Lfd;DDDFFF)V";
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
        /* fcmpl52 accepts both signed zeros and leaves NaN unchanged. */
        if (red == 0.0f)
            red = 1.0f;
        double value;
        if (!draw(p, &e, 59, &value) || !floating(&e, 62))
            goto finish;
        const float factor = mc_travel_fadd(mc_travel_fmul(narrow(value), .4f), .6f);
        const float channels[3] = {red, green, blue};
        float *const stores[3] = {&p->particle->fields.i, &p->particle->fields.j,
                                  &p->particle->fields.k};
        static const unsigned int draws[3] = {72, 93, 114};
        static const unsigned int arithmetic[3] = {78, 99, 120};
        static const unsigned int writes[3] = {89, 110, 131};
        for (size_t channel = 0; channel < 3; ++channel) {
            if (!draw(p, &e, draws[channel], &value) || !floating(&e, arithmetic[channel]))
                goto finish;
            const float mixed =
                mc_travel_fadd(narrow(mc_travel_dmul(value, .20000000298023224)), .8f);
            *stores[channel] = mc_travel_fmul(mc_travel_fmul(mixed, channels[channel]), factor);
            at(p, &e, writes[channel]);
        }
        p->particle->fields.g = mc_travel_fmul(p->particle->fields.g, .75f);
        at(p, &e, 142);
        p->particle->fields.g = mc_travel_fmul(p->particle->fields.g, scale);
        at(p, &e, 153);
        p->particle->redstone.a = p->particle->fields.g;
        at(p, &e, 161);
        if (!draw(p, &e, 168, &value) || !floating(&e, 174))
            goto finish;
        const double denominator = mc_travel_dadd(mc_travel_dmul(value, .8), .2);
        const double lifetime = mc_travel_ddiv(8, denominator);
        p->particle->fields.f = mc_java_double_to_i32(lifetime);
        at(p, &e, 181);
        p->particle->fields.f =
            mc_java_float_to_i32(mc_travel_fmul((float)p->particle->fields.f, scale));
        at(p, &e, 194);
        p->base.bq = false;
        at(p, &e, 199);
        at(p, &e, wrapper ? 17u : 202u);
    finish:
        (void)mc_entity_constructor_finish(p, e.status, &e.entity);
        if (e.status == MC_ENTITY_OK) {
            e.source_class = "im";
            e.source_method = "<init>";
            e.source_descriptor = wrapper ? "(Lfd;DDDFFF)V" : scaled_descriptor;
            e.source_pc = wrapper ? 17u : 202u;
        }
    }
    mc_particle_constructor_error_publish(out, &e);
    return e.status;
}

mc_entity_status mc_redstone_particle_construct_entered(mc_entity *p, mc_world_runtime *world,
                                                        mc_math_classes *math, double x, double y,
                                                        double z, float red, float green,
                                                        float blue, mc_math_classes **capture,
                                                        mc_particle_error *error) {
    return construct(p, world, math, x, y, z, 1, red, green, blue, true, capture, error);
}

mc_entity_status
mc_redstone_particle_construct_scaled_entered(mc_entity *p, mc_world_runtime *world,
                                              mc_math_classes *math, double x, double y, double z,
                                              float scale, float red, float green, float blue,
                                              mc_math_classes **capture, mc_particle_error *error) {
    return construct(p, world, math, x, y, z, scale, red, green, blue, false, capture, error);
}

mc_entity_status mc_redstone_particle_construct(mc_entity *p, mc_world_runtime *world,
                                                mc_math_classes *math, double x, double y, double z,
                                                float red, float green, float blue,
                                                mc_particle_error *error) {
    return mc_redstone_particle_construct_entered(p, world, math, x, y, z, red, green, blue, NULL,
                                                  error);
}

mc_entity_status mc_redstone_particle_construct_scaled(mc_entity *p, mc_world_runtime *world,
                                                       mc_math_classes *math, double x, double y,
                                                       double z, float scale, float red,
                                                       float green, float blue,
                                                       mc_particle_error *error) {
    return mc_redstone_particle_construct_scaled_entered(p, world, math, x, y, z, scale, red, green,
                                                         blue, NULL, error);
}

static mc_entity_status tick_fail(mc_particle_error *e, mc_entity_status status,
                                  mc_particle_cause cause, const char *operation, unsigned int pc) {
    e->status = status;
    e->cause = cause;
    e->operation = operation;
    e->source_class = "im";
    e->source_method = "w_";
    e->source_descriptor = "()V";
    e->source_pc = e->tick_pc = pc;
    return status;
}

mc_entity_status mc_redstone_particle_tick(mc_entity *p, mc_registry_classes *registry,
                                           const mc_entity_move_limits *limits,
                                           mc_item_particle_tick_error *error) {
    if (error == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    mc_item_particle_tick_error_init(error);
    mc_particle_error *e = &error->particle;
    if (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_REDSTONE_PARTICLE) || p->particle == NULL)
        return tick_fail(e, MC_ENTITY_INVALID_ARGUMENT, MC_PARTICLE_OWNER,
                         "Redstone tick canonical IM storage", 0);
    if (p->phase != MC_ENTITY_READY || p->particle->item_native.tick_active)
        return tick_fail(
            e, p->particle->item_native.tick_active ? MC_ENTITY_BUSY : MC_ENTITY_INVALID_PHASE,
            MC_PARTICLE_OWNER, "Redstone tick inactive READY owner", 0);
    const mc_entity_status held = mc_entity_retain(p);
    if (held != MC_ENTITY_OK)
        return tick_fail(e, held, MC_PARTICLE_OWNER, "Redstone tick native occurrence", 0);
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
                        "Redstone texture actual integer division by zero", 60);
        e->native_policy = false;
        e->source_exception = true;
        e->throwable = MC_PARTICLE_SOURCE_EXCEPTION;
        e->exception_class = "java/lang/ArithmeticException";
        goto done;
    }
    f->b = mc_java_i32_sub(7, quotient);
    p->particle->item_native.tick_pc = 62;
    e->status = mc_entity_move_in_context(p, registry, p->base.velocity_x, p->base.velocity_y,
                                          p->base.velocity_z, limits, &error->movement);
    if (e->status != MC_ENTITY_OK) {
        (void)tick_fail(e, e->status, MC_PARTICLE_MOVEMENT, error->movement.operation, 78);
        mc_particle_movement_exception(e, &error->movement, registry);
        goto done;
    }
    if (p->base.y == p->base.aK) {
        p->base.velocity_x = mc_travel_dmul(p->base.velocity_x, 1.1);
        p->particle->item_native.tick_pc = 102;
        p->base.velocity_z = mc_travel_dmul(p->base.velocity_z, 1.1);
        p->particle->item_native.tick_pc = 114;
    }
    p->base.velocity_x = mc_travel_dmul(p->base.velocity_x, .9599999785423279);
    p->particle->item_native.tick_pc = 126;
    p->base.velocity_y = mc_travel_dmul(p->base.velocity_y, .9599999785423279);
    p->particle->item_native.tick_pc = 138;
    p->base.velocity_z = mc_travel_dmul(p->base.velocity_z, .9599999785423279);
    p->particle->item_native.tick_pc = 150;
    if (p->base.aX) {
        p->base.velocity_x = mc_travel_dmul(p->base.velocity_x, .699999988079071);
        p->particle->item_native.tick_pc = 169;
        p->base.velocity_z = mc_travel_dmul(p->base.velocity_z, .699999988079071);
        p->particle->item_native.tick_pc = 181;
    }
    (void)tick_fail(e, MC_ENTITY_OK, MC_PARTICLE_NONE, NULL, 184);
    p->particle->item_native.tick_pc = 184;
done:
    p->particle->item_native.tick_active = false;
    mc_entity_release(p);
    return e->status;
}

mc_entity_status mc_redstone_particle_layer(const mc_entity *p, int32_t *out) {
    if (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_REDSTONE_PARTICLE) || out == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    *out = 0;
    return MC_ENTITY_OK;
}
