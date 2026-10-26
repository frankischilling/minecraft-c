#include "../construction_internal.h"
#include "../living/travel_math_internal.h"
#include "particle_internal.h"

/* Reuse the qualified strict Java SSE operations; the particle-method
 * comparison separately qualifies their use in this constructor. */
static double widen(float value) {
    return _mm_cvtsd_f64(_mm_cvtss_sd(_mm_setzero_pd(), _mm_set_ss(value)));
}

static float narrow(double value) {
    return _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), _mm_set_sd(value)));
}

static double divide(double left, double right) {
    return _mm_cvtsd_f64(_mm_div_sd(_mm_set_sd(left), _mm_set_sd(right)));
}

static bool particle_argument(const mc_entity *value) {
    if (value == NULL)
        return false;
    bool answer = false;
    const mc_entity_status status = mc_entity_class_literal_is_assignable_from(
        &value->classes->literals[MC_ENTITY_CLASS_KIND_PARTICLE], value->class_literal, &answer);
    return status != MC_ENTITY_OK || answer;
}

static mc_entity_status fail(mc_particle_error *e, mc_entity_status status, mc_particle_cause cause,
                             const char *operation, const char *source, const char *method,
                             unsigned int pc) {
    e->status = status;
    e->cause = cause;
    e->operation = operation;
    e->source_class = source;
    e->source_method = method;
    e->source_pc = pc;
    if (source != NULL && strcmp(source, "em") == 0)
        e->pickup_pc = pc;
    else if (source != NULL && strcmp(source, "xw") == 0)
        e->particle_pc = pc;
    return status;
}

static bool math_draw(mc_entity *p, double *out, unsigned int pc, mc_particle_error *e) {
    e->particle_pc = pc;
    e->random = mc_java_math_random(p->classes->profile, out);
    if (e->random == MC_JAVA_RANDOM_PROFILE_OK)
        return true;
    fail(e,
         e->random == MC_JAVA_RANDOM_PROFILE_LIMIT               ? MC_ENTITY_LIMIT
         : e->random == MC_JAVA_RANDOM_PROFILE_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
         : e->random == MC_JAVA_RANDOM_PROFILE_UNSUPPORTED_FP    ? MC_ENTITY_UNSUPPORTED_FP
                                                                 : MC_ENTITY_PROFILE_FAILED,
         MC_PARTICLE_RANDOM, "Particle actual Math.random", "xw", "<init>", pc);
    return false;
}

static void completed(mc_entity *p, mc_particle_error *e, unsigned int pc) {
    p->particle->particle_pc = pc;
    e->particle_pc = pc;
}

static double jitter(double draw) {
    const double scaled = mc_travel_dmul(draw, 2.0);
    const double centered = mc_travel_dsub(scaled, 1.0);
    return widen(mc_travel_fmul(narrow(centered), 0.4f));
}

static bool ancestor(mc_entity *p, mc_world_runtime *world, double x, double y, double z, double vx,
                     double vy, double vz, mc_particle_error *e) {
    e->particle_pc = 2;
    const mc_entity_status base = mc_entity_construct_base(p, world, &e->entity);
    if (base != MC_ENTITY_OK) {
        fail(e, base, MC_PARTICLE_ENTITY, "Particle actual Entity constructor", "xw", "<init>", 2);
        return false;
    }
    mc_particle_fields *f = &p->particle->fields;
    f->e = 0;
    completed(p, e, 7);
    f->f = 0;
    completed(p, e, 12);
    p->base.width = 0.2f;
    p->base.height = 0.2f; /* Actual inherited sn.b(FF), no bounds update. */
    completed(p, e, 20);
    p->base.offset = mc_travel_fdiv(p->base.height, 2.0f);
    completed(p, e, 30);
    mc_entity_source_position(p, x, y, z);
    completed(p, e, 39);
    f->k = 1.0f;
    completed(p, e, 47);
    f->j = 1.0f;
    completed(p, e, 51);
    f->i = 1.0f;
    completed(p, e, 54);
    double a, b;
    if (!math_draw(p, &a, 60, e))
        return false;
    p->base.velocity_x = mc_travel_dadd(vx, jitter(a));
    completed(p, e, 75);
    if (!math_draw(p, &a, 81, e))
        return false;
    p->base.velocity_y = mc_travel_dadd(vy, jitter(a));
    completed(p, e, 96);
    if (!math_draw(p, &a, 102, e))
        return false;
    p->base.velocity_z = mc_travel_dadd(vz, jitter(a));
    completed(p, e, 117);
    if (!math_draw(p, &a, 120, e) || !math_draw(p, &b, 123, e))
        return false;
    const float speed = mc_travel_fmul(narrow(mc_travel_dadd(mc_travel_dadd(a, b), 1.0)), 0.15f);
    const double xx = mc_travel_dmul(p->base.velocity_x, p->base.velocity_x);
    const double yy = mc_travel_dmul(p->base.velocity_y, p->base.velocity_y);
    const double zz = mc_travel_dmul(p->base.velocity_z, p->base.velocity_z);
    const float length = mc_travel_sqrt_float(mc_travel_dadd(mc_travel_dadd(xx, yy), zz));
    p->base.velocity_x =
        mc_travel_dmul(mc_travel_dmul(divide(p->base.velocity_x, widen(length)), widen(speed)),
                       0.4000000059604645);
    completed(p, e, 186);
    p->base.velocity_y = mc_travel_dadd(
        mc_travel_dmul(mc_travel_dmul(divide(p->base.velocity_y, widen(length)), widen(speed)),
                       0.4000000059604645),
        0.10000000149011612);
    completed(p, e, 210);
    p->base.velocity_z =
        mc_travel_dmul(mc_travel_dmul(divide(p->base.velocity_z, widen(length)), widen(speed)),
                       0.4000000059604645);
    completed(p, e, 230);
    f->c = mc_travel_fmul(mc_java_random_next_float(p->random), 3.0f);
    completed(p, e, 244);
    f->d = mc_travel_fmul(mc_java_random_next_float(p->random), 3.0f);
    completed(p, e, 258);
    f->g = mc_travel_fmul(
        mc_travel_fadd(mc_travel_fmul(mc_java_random_next_float(p->random), 0.5f), 0.5f), 2.0f);
    completed(p, e, 277);
    f->f = mc_java_float_to_i32(mc_travel_fdiv(
        4.0f, mc_travel_fadd(mc_travel_fmul(mc_java_random_next_float(p->random), 0.9f), 0.1f)));
    completed(p, e, 298);
    f->e = 0;
    completed(p, e, 303);
    completed(p, e, 306);
    return true;
}

mc_entity_status mc_pickup_particle_allocate(mc_entity_classes *classes, mc_entity **out,
                                             mc_entity_error *error) {
    return mc_entity_allocate_canonical(classes, MC_ENTITY_CLASS_KIND_PICKUP_PARTICLE,
                                        "pickup particle allocation", out, error);
}

mc_entity_status mc_pickup_particle_construct(mc_entity *p, mc_world_runtime *world,
                                              mc_entity *picked, mc_entity *target, float offset,
                                              mc_particle_error *out) {
    mc_particle_error e = {0};
    if (mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_PICKUP_PARTICLE) && p->phase == MC_ENTITY_RAW) {
        if (picked == p || target == p ||
            (picked != NULL && (picked->classes != p->classes || picked->holds == 0)) ||
            (target != NULL && (target->classes != p->classes || target->holds == 0)))
            fail(&e, MC_ENTITY_INVALID_ARGUMENT, MC_PARTICLE_OWNER,
                 "Pickup particle native argument identities", NULL, NULL, 0);
        else if ((picked != NULL && picked->phase != MC_ENTITY_READY) ||
                 (target != NULL && target->phase != MC_ENTITY_READY))
            fail(&e, MC_ENTITY_INVALID_PHASE, MC_PARTICLE_OWNER,
                 "Pickup particle READY argument identities", NULL, NULL, 0);
        else if (particle_argument(picked) || particle_argument(target))
            fail(&e, MC_ENTITY_INVALID_ARGUMENT, MC_PARTICLE_OWNER,
                 "Pickup particle ordinary non-particle actor identities", NULL, NULL, 0);
        if (e.status != MC_ENTITY_OK) {
            if (out != NULL)
                *out = e;
            return e.status;
        }
    }
    const mc_entity_status entered = mc_entity_constructor_begin(
        p, MC_ENTITY_CLASS_KIND_PICKUP_PARTICLE, world, "pickup particle constructor",
        "pickup particle World profile", &e.entity);
    if (entered != MC_ENTITY_OK)
        fail(&e, entered, MC_PARTICLE_OWNER, e.entity.operation, NULL, NULL, 0);
    else {
        if (picked == NULL)
            fail(&e, MC_ENTITY_STATE_ERROR, MC_PARTICLE_NULL_REFERENCE,
                 "Pickup particle null picked Entity argument", "em", "<init>", 3);
        else {
            /* Actual six argument reads precede xw/sn construction and entropy. */
            const double x = picked->base.x, y = picked->base.y, z = picked->base.z;
            const double vx = picked->base.velocity_x, vy = picked->base.velocity_y,
                         vz = picked->base.velocity_z;
            e.pickup_pc = 26;
            if (ancestor(p, world, x, y, z, vx, vy, vz, &e)) {
                mc_pickup_particle_fields *f = &p->particle->pickup;
                f->p = 0;
                p->particle->pickup_pc = 31;
                f->q = 0;
                p->particle->pickup_pc = 36;
                e.entity.status = mc_entity_retain(picked);
                if (e.entity.status != MC_ENTITY_OK)
                    fail(&e, e.entity.status, MC_PARTICLE_OWNER,
                         "Pickup particle picked Entity native hold", NULL, NULL, 0);
                else {
                    f->a = picked;
                    p->particle->pickup_pc = 41;
                    e.entity.status = target == NULL ? MC_ENTITY_OK : mc_entity_retain(target);
                    if (e.entity.status != MC_ENTITY_OK)
                        fail(&e, e.entity.status, MC_PARTICLE_OWNER,
                             "Pickup particle target Entity native hold", NULL, NULL, 0);
                    else {
                        f->o = target;
                        p->particle->pickup_pc = 46;
                        f->q = 3;
                        p->particle->pickup_pc = 51;
                        f->r = offset;
                        p->particle->pickup_pc = 57;
                        p->particle->pickup_pc = 60;
                    }
                }
            }
        }
        (void)mc_entity_constructor_finish(p, e.status, &e.entity);
        if (e.status == MC_ENTITY_OK)
            e = (mc_particle_error){0};
    }
    if (out != NULL)
        *out = e;
    return e.status;
}

mc_entity_status mc_pickup_particle_tick(mc_entity *p, mc_particle_error *out) {
    mc_particle_error e = {0};
    if (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_PICKUP_PARTICLE))
        fail(&e, MC_ENTITY_INVALID_ARGUMENT, MC_PARTICLE_OWNER, "Pickup tick concrete receiver",
             NULL, NULL, 0);
    else if (p->phase != MC_ENTITY_READY)
        fail(&e, MC_ENTITY_INVALID_PHASE, MC_PARTICLE_OWNER, "Pickup tick READY receiver", NULL,
             NULL, 0);
    else {
        mc_pickup_particle_fields *f = &p->particle->pickup;
        f->p = mc_java_i32_add(f->p, 1);
        if (f->p == f->q)
            p->base.be = true; /* Actual em.K resolves to inherited sn.K store2. */
    }
    if (out != NULL)
        *out = e;
    return e.status;
}

mc_entity_status mc_pickup_particle_layer(const mc_entity *p, int32_t *out) {
    if (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_PICKUP_PARTICLE) || out == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    *out = 3; /* Actual em.c_ literal return1, no source field/phase read. */
    return MC_ENTITY_OK;
}

bool mc_particle_read(const mc_entity *p, mc_particle_snapshot *out) {
    if (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_PICKUP_PARTICLE) || out == NULL)
        return false;
    *out = (mc_particle_snapshot){p->phase, p->particle->particle_pc, p->particle->pickup_pc,
                                  p->particle->fields, p->particle->pickup};
    return true;
}

void mc_pickup_particle_dispose(mc_entity *p) {
    mc_entity_release(p->particle->pickup.a);
    mc_entity_release(p->particle->pickup.o);
}
