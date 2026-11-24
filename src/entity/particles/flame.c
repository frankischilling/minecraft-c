#include "particle_internal.h"

#include "../construction_internal.h"
#include "../living/travel_math_internal.h"
#include "mc/core/java/java.h"

static const char descriptor[] = "(Lfd;DDDDDD)V";

_Static_assert(sizeof(((mc_particle_constructor_random *)0)->own_values) / sizeof(float) >= 10,
               "Actual XW4 plus QU6 private Random draws must all fit");

static void at(mc_entity *p, mc_particle_constructor_error *e, unsigned int pc) {
    p->particle->item_native.item_pc = e->item_pc = pc;
}

static bool fail(mc_particle_constructor_error *e, mc_entity_status status, mc_particle_cause cause,
                 const char *operation, unsigned int pc) {
    (void)mc_particle_constructor_fail(e, status, cause, operation, "qu", "<init>", pc);
    e->source_descriptor = descriptor;
    e->item_pc = pc;
    return false;
}

static bool floating(mc_particle_constructor_error *e, unsigned int pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(e, MC_ENTITY_UNSUPPORTED_FP, MC_PARTICLE_OWNER,
                "Flame constructor strict floating environment", pc);
}

static bool own(mc_entity *p, mc_particle_constructor_error *e, unsigned int pc, float *out) {
    at(p, e, pc);
    if (p->random == NULL)
        return fail(e, MC_ENTITY_STATE_ERROR, MC_PARTICLE_NULL_REFERENCE,
                    "Flame actual null private Random receiver", pc);
    *out = mc_java_random_next_float(p->random);
    if (e->constructor_random.own_count >=
        sizeof(e->constructor_random.own_values) / sizeof(e->constructor_random.own_values[0]))
        return fail(e, MC_ENTITY_LIMIT, MC_PARTICLE_OWNER,
                    "Flame native constructor private Random receipt extent", pc);
    e->constructor_random.own_values[e->constructor_random.own_count++] = *out;
    return true;
}

mc_entity_status mc_flame_particle_allocate(mc_entity_classes *classes, mc_entity **out,
                                            mc_entity_error *error) {
    return mc_entity_allocate_canonical(classes, MC_ENTITY_CLASS_KIND_FLAME_PARTICLE,
                                        "Flame particle allocation", out, error);
}

mc_entity_status mc_flame_particle_construct_entered(mc_entity *p, mc_world_runtime *world,
                                                     mc_math_classes *math, double x, double y,
                                                     double z, double vx, double vy, double vz,
                                                     mc_math_classes **capture,
                                                     mc_particle_error *out) {
    if (out == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    mc_particle_constructor_error e = {.native_policy = true};
    const mc_entity_status entered = mc_entity_constructor_begin(
        p, MC_ENTITY_CLASS_KIND_FLAME_PARTICLE, world, "Flame particle constructor",
        "Flame particle World profile", &e.entity);
    if (entered != MC_ENTITY_OK)
        (void)fail(&e, entered, MC_PARTICLE_OWNER, e.entity.operation, 0);
    else {
        at(p, &e, 13);
        if (!mc_particle_construct_ancestor(p, world, x, y, z, vx, vy, vz, math, true, capture,
                                            &e) ||
            !floating(&e, 24))
            goto finish;
        p->base.velocity_x =
            mc_travel_dadd(mc_travel_dmul(p->base.velocity_x, .009999999776482582), vx);
        at(p, &e, 28);
        p->base.velocity_y =
            mc_travel_dadd(mc_travel_dmul(p->base.velocity_y, .009999999776482582), vy);
        at(p, &e, 43);
        p->base.velocity_z =
            mc_travel_dadd(mc_travel_dmul(p->base.velocity_z, .009999999776482582), vz);
        at(p, &e, 58);
        /* These are genuine constructor LOCAL stores. There is no subsequent
         * setPosition, so publishing them into Entity would alter Source state.
         * Volatile locals retain the actual reached arithmetic as well as draws. */
        volatile double local_position[3] = {x, y, z};
        static const unsigned int first_pc[3] = {66, 89, 113};
        static const unsigned int second_pc[3] = {73, 96, 120};
        static const unsigned int arithmetic_pc[3] = {76, 99, 123};
        static const unsigned int local_pc[3] = {82, 105, 129};
        for (size_t axis = 0; axis < 3; ++axis) {
            float first, second;
            if (!own(p, &e, first_pc[axis], &first) || !own(p, &e, second_pc[axis], &second) ||
                !floating(&e, arithmetic_pc[axis]))
                goto finish;
            const float jitter = mc_travel_fmul(mc_travel_fsub(first, second), .05f);
            const double widened =
                _mm_cvtsd_f64(_mm_cvtss_sd(_mm_setzero_pd(), _mm_set_ss(jitter)));
            local_position[axis] = mc_travel_dadd(local_position[axis], widened);
            at(p, &e, local_pc[axis]);
        }
        p->particle->flame.a = p->particle->fields.g;
        at(p, &e, 136);
        p->particle->fields.k = 1;
        at(p, &e, 144);
        p->particle->fields.j = 1;
        at(p, &e, 148);
        p->particle->fields.i = 1;
        at(p, &e, 151);
        at(p, &e, 158);
        double draw;
        e.random = mc_java_math_random(p->classes->profile, &draw);
        if (e.random != MC_JAVA_RANDOM_PROFILE_OK) {
            (void)fail(
                &e,
                e.random == MC_JAVA_RANDOM_PROFILE_LIMIT               ? MC_ENTITY_LIMIT
                : e.random == MC_JAVA_RANDOM_PROFILE_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
                : e.random == MC_JAVA_RANDOM_PROFILE_UNSUPPORTED_FP    ? MC_ENTITY_UNSUPPORTED_FP
                                                                       : MC_ENTITY_PROFILE_FAILED,
                MC_PARTICLE_RANDOM, "Flame actual Math.random", 158);
            goto finish;
        }
        if (e.constructor_random.math_count >= sizeof(e.constructor_random.math_values) /
                                                   sizeof(e.constructor_random.math_values[0])) {
            (void)fail(&e, MC_ENTITY_LIMIT, MC_PARTICLE_OWNER,
                       "Flame native constructor Math receipt extent", 158);
            goto finish;
        }
        e.constructor_random.math_values[e.constructor_random.math_count++] = draw;
        if (!floating(&e, 164))
            goto finish;
        const double denominator = mc_travel_dadd(mc_travel_dmul(draw, .8), .2);
        const double lifetime = _mm_cvtsd_f64(_mm_div_sd(_mm_set_sd(8), _mm_set_sd(denominator)));
        p->particle->fields.f = mc_java_i32_add(mc_java_double_to_i32(lifetime), 4);
        at(p, &e, 173);
        p->base.bq = true;
        at(p, &e, 178);
        p->particle->fields.b = 48;
        at(p, &e, 184);
        at(p, &e, 187);
    finish:
        (void)mc_entity_constructor_finish(p, e.status, &e.entity);
        if (e.status == MC_ENTITY_OK) {
            e.source_class = "qu";
            e.source_method = "<init>";
            e.source_descriptor = descriptor;
            e.source_pc = 187;
        }
    }
    mc_particle_constructor_error_publish(out, &e);
    return e.status;
}

mc_entity_status mc_flame_particle_construct(mc_entity *p, mc_world_runtime *world,
                                             mc_math_classes *math, double x, double y, double z,
                                             double vx, double vy, double vz,
                                             mc_particle_error *error) {
    return mc_flame_particle_construct_entered(p, world, math, x, y, z, vx, vy, vz, NULL, error);
}

static mc_entity_status tick_fail(mc_particle_error *e, mc_entity_status status,
                                  mc_particle_cause cause, const char *operation, unsigned int pc) {
    e->status = status;
    e->cause = cause;
    e->operation = operation;
    e->source_class = "qu";
    e->source_method = "w_";
    e->source_descriptor = "()V";
    e->source_pc = e->tick_pc = pc;
    return status;
}

mc_entity_status mc_flame_particle_tick(mc_entity *p, mc_registry_classes *registry,
                                        const mc_entity_move_limits *limits,
                                        mc_item_particle_tick_error *error) {
    if (error == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    mc_item_particle_tick_error_init(error);
    mc_particle_error *e = &error->particle;
    if (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_FLAME_PARTICLE) || p->particle == NULL)
        return tick_fail(e, MC_ENTITY_INVALID_ARGUMENT, MC_PARTICLE_OWNER,
                         "Flame tick canonical QU storage", 0);
    if (p->phase != MC_ENTITY_READY || p->particle->item_native.tick_active)
        return tick_fail(
            e, p->particle->item_native.tick_active ? MC_ENTITY_BUSY : MC_ENTITY_INVALID_PHASE,
            MC_PARTICLE_OWNER, "Flame tick inactive READY owner", 0);
    const mc_entity_status held = mc_entity_retain(p);
    if (held != MC_ENTITY_OK)
        return tick_fail(e, held, MC_PARTICLE_OWNER, "Flame tick native occurrence", 0);
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
    e->status = mc_entity_move_in_context(p, registry, p->base.velocity_x, p->base.velocity_y,
                                          p->base.velocity_z, limits, &error->movement);
    if (e->status != MC_ENTITY_OK) {
        (void)tick_fail(e, e->status, MC_PARTICLE_MOVEMENT, error->movement.operation, 59);
        mc_particle_movement_exception(e, &error->movement, registry);
        goto done;
    }
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        (void)tick_fail(e, MC_ENTITY_UNSUPPORTED_FP, MC_PARTICLE_OWNER,
                        "Flame tick strict floating environment", 70);
        goto done;
    }
    p->base.velocity_x = mc_travel_dmul(p->base.velocity_x, .9599999785423279);
    p->particle->item_native.tick_pc = 71;
    p->base.velocity_y = mc_travel_dmul(p->base.velocity_y, .9599999785423279);
    p->particle->item_native.tick_pc = 83;
    p->base.velocity_z = mc_travel_dmul(p->base.velocity_z, .9599999785423279);
    p->particle->item_native.tick_pc = 95;
    if (p->base.aX) {
        p->base.velocity_x = mc_travel_dmul(p->base.velocity_x, .699999988079071);
        p->particle->item_native.tick_pc = 114;
        p->base.velocity_z = mc_travel_dmul(p->base.velocity_z, .699999988079071);
        p->particle->item_native.tick_pc = 126;
    }
    (void)tick_fail(e, MC_ENTITY_OK, MC_PARTICLE_NONE, NULL, 129);
    p->particle->item_native.tick_pc = 129;
done:
    p->particle->item_native.tick_active = false;
    mc_entity_release(p);
    return e->status;
}

mc_entity_status mc_flame_particle_layer(const mc_entity *p, int32_t *out) {
    if (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_FLAME_PARTICLE) || out == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    *out = 0;
    return MC_ENTITY_OK;
}
