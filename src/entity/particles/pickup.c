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

void mc_particle_error_init(mc_particle_error *e) {
    static const mc_particle_error zero = {0};
    if (e == NULL)
        return;
    *e = zero;
    e->native_policy = true;
    e->math.source_pc = -1;
    e->registry.initializer_pc = -1;
    e->registry.block_id = -1;
    e->registry.constructor_pc = -1;
    e->registry.active_initializer_pc = -1;
    e->registry.item_argument = -1;
}

void mc_particle_constructor_error_publish(mc_particle_error *out,
                                           const mc_particle_constructor_error *e) {
    if (out == NULL)
        return;
    mc_particle_error_init(out);
    out->status = e->status;
    out->cause = e->cause;
    out->operation = e->operation;
    out->source_class = e->source_class;
    out->source_method = e->source_method;
    out->source_pc = e->source_pc;
    out->particle_pc = e->particle_pc;
    out->pickup_pc = e->pickup_pc;
    out->entity = e->entity;
    out->random = e->random;
    out->source_descriptor = e->source_descriptor;
    out->exception_class = e->exception_class;
    out->item_pc = e->item_pc;
    out->tick_pc = e->tick_pc;
    out->native_policy = e->native_policy;
    out->source_exception = e->source_exception;
    out->throwable = e->throwable;
    out->math = e->math;
    out->registry = e->registry;
    out->constructor_random = e->constructor_random;
}

static bool ancestor_math(mc_math_classes *math, mc_math_classes **reached_math,
                          mc_particle_constructor_error *e) {
    e->particle_pc = 164;
    if (reached_math != NULL && *reached_math != NULL) {
        mc_particle_constructor_fail(e, MC_ENTITY_INVALID_ARGUMENT, MC_PARTICLE_OWNER,
                                     "Particle reached Math capture slot is occupied", "xw",
                                     "<init>", 164);
        return false;
    }
    if (math == NULL) {
        mc_particle_constructor_fail(e, MC_ENTITY_STATE_ERROR, MC_PARTICLE_MATH,
                                     "Particle reached Math namespace", "xw", "<init>", 164);
        return false;
    }
    mc_math_class_status status = mc_math_classes_retain(math, &e->math);
    if (status == MC_MATH_CLASS_OK) {
        if (reached_math != NULL)
            *reached_math = math;
        status = mc_math_classes_ensure(math, &e->math);
        if (reached_math == NULL) {
            mc_math_classes *occurrence = math;
            mc_math_class_error cleanup;
            const mc_math_class_status retired = mc_math_classes_release(&occurrence, &cleanup);
            if (retired != MC_MATH_CLASS_OK) {
                e->math = cleanup;
                status = retired;
            }
        }
    }
    if (status == MC_MATH_CLASS_OK)
        return true;
    mc_particle_constructor_fail(
        e,
        status == MC_MATH_CLASS_LIMIT               ? MC_ENTITY_LIMIT
        : status == MC_MATH_CLASS_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
        : status == MC_MATH_CLASS_UNSUPPORTED_FP    ? MC_ENTITY_UNSUPPORTED_FP
        : status == MC_MATH_CLASS_BUSY              ? MC_ENTITY_BUSY
                                                    : MC_ENTITY_STATE_ERROR,
        MC_PARTICLE_MATH, "Particle actual IN sqrt active use", "xw", "<init>", 164);
    return false;
}

static bool particle_argument(const mc_entity *value) {
    if (value == NULL)
        return false;
    bool answer = false;
    const mc_entity_status status = mc_entity_class_literal_is_assignable_from(
        &value->classes->literals[MC_ENTITY_CLASS_KIND_PARTICLE], value->class_literal, &answer);
    return status != MC_ENTITY_OK || answer;
}

mc_entity_status mc_particle_constructor_fail(mc_particle_constructor_error *e,
                                              mc_entity_status status, mc_particle_cause cause,
                                              const char *operation, const char *source,
                                              const char *method, unsigned int pc) {
    e->status = status;
    e->cause = cause;
    e->operation = operation;
    e->source_class = source;
    e->source_method = method;
    e->source_pc = pc;
    e->native_policy = true;
    if (cause == MC_PARTICLE_NULL_REFERENCE) {
        e->throwable = MC_PARTICLE_SOURCE_EXCEPTION;
        e->source_exception = true;
        e->exception_class = "java/lang/NullPointerException";
        e->native_policy = false;
    }
    if (source != NULL && method != NULL && strcmp(method, "<init>") == 0)
        e->source_descriptor = strcmp(source, "em") == 0   ? "(Lfd;Lsn;Lsn;F)V"
                               : strcmp(source, "xw") == 0 ? "(Lfd;DDDDDD)V"
                                                           : NULL;
    if (source != NULL && strcmp(source, "em") == 0)
        e->pickup_pc = pc;
    else if (source != NULL && strcmp(source, "xw") == 0)
        e->particle_pc = pc;
    return status;
}

static bool math_draw(mc_entity *p, double *out, unsigned int pc,
                      mc_particle_constructor_error *e) {
    e->particle_pc = pc;
    e->random = mc_java_math_random(p->classes->profile, out);
    if (e->random == MC_JAVA_RANDOM_PROFILE_OK) {
        e->constructor_random.math_values[e->constructor_random.math_count++] = *out;
        return true;
    }
    mc_particle_constructor_fail(
        e,
        e->random == MC_JAVA_RANDOM_PROFILE_LIMIT               ? MC_ENTITY_LIMIT
        : e->random == MC_JAVA_RANDOM_PROFILE_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
        : e->random == MC_JAVA_RANDOM_PROFILE_UNSUPPORTED_FP    ? MC_ENTITY_UNSUPPORTED_FP
                                                                : MC_ENTITY_PROFILE_FAILED,
        MC_PARTICLE_RANDOM, "Particle actual Math.random", "xw", "<init>", pc);
    return false;
}

static void completed(mc_entity *p, mc_particle_constructor_error *e, unsigned int pc) {
    p->particle->particle_pc = pc;
    e->particle_pc = pc;
}

static double jitter(double draw) {
    const double scaled = mc_travel_dmul(draw, 2.0);
    const double centered = mc_travel_dsub(scaled, 1.0);
    return widen(mc_travel_fmul(narrow(centered), 0.4f));
}

bool mc_particle_construct_ancestor(mc_entity *p, mc_world_runtime *world, double x, double y,
                                    double z, double vx, double vy, double vz,
                                    mc_math_classes *math, bool active_math,
                                    mc_math_classes **reached_math,
                                    mc_particle_constructor_error *e) {
    e->particle_pc = 2;
    const mc_entity_status base = mc_entity_construct_base(p, world, &e->entity);
    if (base != MC_ENTITY_OK) {
        mc_particle_constructor_fail(e, base, MC_PARTICLE_ENTITY,
                                     "Particle actual Entity constructor", "xw", "<init>", 2);
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
    if (active_math && !ancestor_math(math, reached_math, e))
        return false;
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
    e->constructor_random.own_initial_stored = true;
    e->constructor_random.own_identity = (uintptr_t)p->random;
    e->constructor_random.own_initial = *p->random;
    float draw = mc_java_random_next_float(p->random);
    e->constructor_random.own_values[e->constructor_random.own_count++] = draw;
    f->c = mc_travel_fmul(draw, 3.0f);
    completed(p, e, 244);
    draw = mc_java_random_next_float(p->random);
    e->constructor_random.own_values[e->constructor_random.own_count++] = draw;
    f->d = mc_travel_fmul(draw, 3.0f);
    completed(p, e, 258);
    draw = mc_java_random_next_float(p->random);
    e->constructor_random.own_values[e->constructor_random.own_count++] = draw;
    f->g = mc_travel_fmul(mc_travel_fadd(mc_travel_fmul(draw, 0.5f), 0.5f), 2.0f);
    completed(p, e, 277);
    draw = mc_java_random_next_float(p->random);
    e->constructor_random.own_values[e->constructor_random.own_count++] = draw;
    f->f = mc_java_float_to_i32(
        mc_travel_fdiv(4.0f, mc_travel_fadd(mc_travel_fmul(draw, 0.9f), 0.1f)));
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
    mc_particle_constructor_error e = {0};
    if (mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_PICKUP_PARTICLE) && p->phase == MC_ENTITY_RAW) {
        if (picked == p || target == p ||
            (picked != NULL && (picked->classes != p->classes || picked->holds == 0)) ||
            (target != NULL && (target->classes != p->classes || target->holds == 0)))
            mc_particle_constructor_fail(&e, MC_ENTITY_INVALID_ARGUMENT, MC_PARTICLE_OWNER,
                                         "Pickup particle native argument identities", NULL, NULL,
                                         0);
        else if ((picked != NULL && picked->phase != MC_ENTITY_READY) ||
                 (target != NULL && target->phase != MC_ENTITY_READY))
            mc_particle_constructor_fail(&e, MC_ENTITY_INVALID_PHASE, MC_PARTICLE_OWNER,
                                         "Pickup particle READY argument identities", NULL, NULL,
                                         0);
        else if (particle_argument(picked) || particle_argument(target))
            mc_particle_constructor_fail(&e, MC_ENTITY_INVALID_ARGUMENT, MC_PARTICLE_OWNER,
                                         "Pickup particle ordinary non-particle actor identities",
                                         NULL, NULL, 0);
        if (e.status != MC_ENTITY_OK) {
            if (out != NULL)
                mc_particle_constructor_error_publish(out, &e);
            return e.status;
        }
    }
    const mc_entity_status entered = mc_entity_constructor_begin(
        p, MC_ENTITY_CLASS_KIND_PICKUP_PARTICLE, world, "pickup particle constructor",
        "pickup particle World profile", &e.entity);
    if (entered != MC_ENTITY_OK)
        mc_particle_constructor_fail(&e, entered, MC_PARTICLE_OWNER, e.entity.operation, NULL, NULL,
                                     0);
    else {
        if (picked == NULL)
            mc_particle_constructor_fail(&e, MC_ENTITY_STATE_ERROR, MC_PARTICLE_NULL_REFERENCE,
                                         "Pickup particle null picked Entity argument", "em",
                                         "<init>", 3);
        else {
            /* Actual six argument reads precede xw/sn construction and entropy. */
            const double x = picked->base.x, y = picked->base.y, z = picked->base.z;
            const double vx = picked->base.velocity_x, vy = picked->base.velocity_y,
                         vz = picked->base.velocity_z;
            e.pickup_pc = 26;
            if (mc_particle_construct_ancestor(p, world, x, y, z, vx, vy, vz, NULL, false, NULL,
                                               &e)) {
                mc_pickup_particle_fields *f = &p->particle->pickup;
                f->p = 0;
                p->particle->pickup_pc = 31;
                f->q = 0;
                p->particle->pickup_pc = 36;
                e.entity.status = mc_entity_retain(picked);
                if (e.entity.status != MC_ENTITY_OK)
                    mc_particle_constructor_fail(&e, e.entity.status, MC_PARTICLE_OWNER,
                                                 "Pickup particle picked Entity native hold", NULL,
                                                 NULL, 0);
                else {
                    f->a = picked;
                    p->particle->pickup_pc = 41;
                    e.entity.status = target == NULL ? MC_ENTITY_OK : mc_entity_retain(target);
                    if (e.entity.status != MC_ENTITY_OK)
                        mc_particle_constructor_fail(&e, e.entity.status, MC_PARTICLE_OWNER,
                                                     "Pickup particle target Entity native hold",
                                                     NULL, NULL, 0);
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
            e = (mc_particle_constructor_error){0};
    }
    if (out != NULL)
        mc_particle_constructor_error_publish(out, &e);
    return e.status;
}

mc_entity_status mc_pickup_particle_tick(mc_entity *p, mc_particle_error *out) {
    mc_particle_constructor_error e = {0};
    if (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_PICKUP_PARTICLE))
        mc_particle_constructor_fail(&e, MC_ENTITY_INVALID_ARGUMENT, MC_PARTICLE_OWNER,
                                     "Pickup tick concrete receiver", NULL, NULL, 0);
    else if (p->phase != MC_ENTITY_READY)
        mc_particle_constructor_fail(&e, MC_ENTITY_INVALID_PHASE, MC_PARTICLE_OWNER,
                                     "Pickup tick READY receiver", NULL, NULL, 0);
    else {
        mc_pickup_particle_fields *f = &p->particle->pickup;
        f->p = mc_java_i32_add(f->p, 1);
        if (f->p == f->q)
            p->base.be = true; /* Actual em.K resolves to inherited sn.K store2. */
    }
    if (out != NULL)
        mc_particle_constructor_error_publish(out, &e);
    return e.status;
}

mc_entity_status mc_pickup_particle_layer(const mc_entity *p, int32_t *out) {
    if (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_PICKUP_PARTICLE) || out == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    *out = 3; /* Actual em.c_ literal return1, no source field/phase read. */
    return MC_ENTITY_OK;
}

bool mc_particle_read(const mc_entity *p, mc_particle_snapshot *out) {
    if (!mc_entity_has_particle(p) || out == NULL)
        return false;
    const bool item = !mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_PICKUP_PARTICLE);
    *out = (mc_particle_snapshot){.phase = p->phase,
                                  .particle_pc = p->particle->particle_pc,
                                  .pickup_pc = p->particle->pickup_pc,
                                  .particle = p->particle->fields,
                                  .pickup =
                                      item ? (mc_pickup_particle_fields){0} : p->particle->pickup,
                                  .item_pc = item ? p->particle->item_native.item_pc : 0,
                                  .tick_pc = item ? p->particle->item_native.tick_pc : 0,
                                  .tick_active = item && p->particle->item_native.tick_active,
                                  .digging_block = p->particle->digging_block,
                                  .digging_registry = p->particle->digging_registry,
                                  .digging_face = p->particle->digging_face,
                                  .smoke = mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_SMOKE_PARTICLE)
                                               ? p->particle->smoke
                                               : (mc_smoke_particle_fields){0},
                                  .flame = mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_FLAME_PARTICLE)
                                               ? p->particle->flame
                                               : (mc_flame_particle_fields){0}};
    return true;
}

mc_entity_status mc_particle_layer(const mc_entity *p, int32_t *out) {
    if (!mc_entity_has_particle(p) || out == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    switch (p->class_literal->kind) {
        case MC_ENTITY_CLASS_KIND_PICKUP_PARTICLE:
            *out = 3;
            break;
        case MC_ENTITY_CLASS_KIND_ITEM_PARTICLE:
            *out = 2;
            break;
        case MC_ENTITY_CLASS_KIND_DIGGING_PARTICLE:
            *out = 1;
            break;
        case MC_ENTITY_CLASS_KIND_EXPLOSION_PARTICLE:
        case MC_ENTITY_CLASS_KIND_SMOKE_PARTICLE:
        case MC_ENTITY_CLASS_KIND_FLAME_PARTICLE:
            *out = 0;
            break;
        default:
            return MC_ENTITY_INVALID_ARGUMENT;
    }
    return MC_ENTITY_OK;
}

bool mc_particle_constructor_returned(const mc_entity *p) {
    if (!mc_entity_has_particle(p))
        return false;
    if (mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_PICKUP_PARTICLE))
        return p->particle->pickup_pc == 60;
    const unsigned int pc = p->particle->item_native.item_pc;
    switch (p->class_literal->kind) {
        case MC_ENTITY_CLASS_KIND_ITEM_PARTICLE:
            return pc == 58;
        case MC_ENTITY_CLASS_KIND_DIGGING_PARTICLE:
            return pc == 80;
        case MC_ENTITY_CLASS_KIND_EXPLOSION_PARTICLE:
            return pc == 157;
        case MC_ENTITY_CLASS_KIND_SMOKE_PARTICLE:
            return pc == 17 || pc == 172;
        case MC_ENTITY_CLASS_KIND_FLAME_PARTICLE:
            return pc == 187;
        default:
            return false;
    }
}

void mc_pickup_particle_dispose(mc_entity *p) {
    mc_entity_release(p->particle->pickup.a);
    mc_entity_release(p->particle->pickup.o);
}
