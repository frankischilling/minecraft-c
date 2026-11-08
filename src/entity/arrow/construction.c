#include "../construction_internal.h"
#include "../living/fields_internal.h"
#include "../player/tick_math_internal.h"
#include "../position_internal.h"
#include "internal.h"
#include "references_internal.h"

#include <string.h>

static const char *const world_descriptor = "(Lfd;)V";
static const char *const position_descriptor = "(Lfd;DDD)V";
static const char *const living_descriptor = "(Lfd;Lls;)V";

static mc_arrow_site site(const char *name, const char *method, const char *descriptor, int pc) {
    return (mc_arrow_site){name, method, descriptor, pc};
}

static mc_arrow_error fresh(void) {
    mc_arrow_error e = {0};
    e.site.pc = e.child.pc = e.heading_site.pc = -1;
    e.math.source_pc = -1;
    return e;
}

static bool fail(mc_arrow_error *e, mc_entity_status status, const char *operation) {
    e->status = status;
    e->operation = operation;
    e->native_policy = !e->source_exception;
    return false;
}

static bool source_null(mc_arrow_error *e, const char *operation) {
    e->source_exception = true;
    e->exception_class = "java/lang/NullPointerException";
    return fail(e, MC_ENTITY_STATE_ERROR, operation);
}

static bool fp(mc_arrow_error *e) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(e, MC_ENTITY_UNSUPPORTED_FP, "Arrow reached strict floating environment");
}

static void constructor_at(mc_arrow_error *e, const char *descriptor, unsigned int pc) {
    e->site = site("sl", "<init>", descriptor, (int)pc);
}

static void heading_at(mc_arrow_error *e, unsigned int pc) {
    e->site = e->heading_site = site("sl", "a", "(DDDFF)V", (int)pc);
}

static bool math_ready(mc_math_classes *math, mc_arrow_error *e, const char *method,
                       const char *descriptor) {
    e->child = site("in", method, descriptor, -1);
    if (math == NULL)
        return fail(e, MC_ENTITY_STATE_ERROR, "Arrow actual Math namespace");
    const mc_math_class_status status = mc_math_classes_ensure(math, &e->math);
    if (status != MC_MATH_CLASS_OK) {
        if (e->math.source_pc >= 0)
            e->child = site("in", "<clinit>", "()V", e->math.source_pc);
        return fail(e,
                    status == MC_MATH_CLASS_LIMIT               ? MC_ENTITY_LIMIT
                    : status == MC_MATH_CLASS_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
                    : status == MC_MATH_CLASS_UNSUPPORTED_FP    ? MC_ENTITY_UNSUPPORTED_FP
                    : status == MC_MATH_CLASS_BUSY              ? MC_ENTITY_BUSY
                                                                : MC_ENTITY_STATE_ERROR,
                    e->math.operation);
    }
    e->child = site(NULL, NULL, NULL, -1);
    return true;
}

static bool trig(mc_math_classes *math, bool cosine, float argument, float *out,
                 mc_arrow_error *e) {
    if (!math_ready(math, e, cosine ? "b" : "a", "(F)F"))
        return false;
    const mc_math_trig *table = mc_math_classes_table(math);
    if (table == NULL)
        return fail(e, MC_ENTITY_STATE_ERROR, "Arrow actual Math table representation");
    *out = cosine ? mc_math_cos(table, argument) : mc_math_sin(table, argument);
    return true;
}

static bool gaussian(mc_entity *p, unsigned int pc, double *out, mc_arrow_error *e) {
    heading_at(e, pc);
    mc_java_random *random = p->random; /* Three distinct source bs reads. */
    if (random == NULL)
        return source_null(e, "Arrow heading null Random receiver");
    if (!e->random_captured) {
        e->random_before = *random;
        e->random_captured = true;
    }
    *out = mc_java_random_next_gaussian(random);
    e->gaussian_values[e->gaussian_calls] = *out;
    e->random_after = *random;
    ++e->gaussian_calls;
    p->arrow->heading_pc = pc;
    return true;
}

static bool heading_entered(mc_entity *p, mc_math_classes *math, double x, double y, double z,
                            float speed, float spread, mc_arrow_error *e) {
    e->heading_entered = true;
    p->arrow->heading_active = true;
    p->arrow->heading_pc = 0;
    heading_at(e, 2);
    if (!fp(e))
        goto failed;
    const double squared = at_add(at_add(at_mul(x, x), at_mul(y, y)), at_mul(z, z));
    heading_at(e, 13);
    if (!math_ready(math, e, "a", "(D)F"))
        goto failed;
    const float length = at_root(squared);
    p->arrow->heading_pc = 13;
    x = at_div(x, at_widen(length));
    y = at_div(y, at_widen(length));
    z = at_div(z, at_widen(length));
    double draw;
    if (!gaussian(p, 43, &draw, e))
        goto failed;
    x = at_add(x, at_mul(at_mul(draw, .007499999832361937), at_widen(spread)));
    if (!gaussian(p, 61, &draw, e))
        goto failed;
    y = at_add(y, at_mul(at_mul(draw, .007499999832361937), at_widen(spread)));
    if (!gaussian(p, 80, &draw, e))
        goto failed;
    z = at_add(z, at_mul(at_mul(draw, .007499999832361937), at_widen(spread)));
    x = at_mul(x, at_widen(speed));
    y = at_mul(y, at_widen(speed));
    z = at_mul(z, at_widen(speed));
    p->base.velocity_x = x;
    p->arrow->heading_pc = 116;
    p->base.velocity_y = y;
    p->arrow->heading_pc = 121;
    p->base.velocity_z = z;
    p->arrow->heading_pc = 127;
    heading_at(e, 139);
    if (!math_ready(math, e, "a", "(D)F"))
        goto failed;
    const float horizontal = at_root(at_add(at_mul(x, x), at_mul(z, z)));
    p->arrow->heading_pc = 139;
    const float yaw =
        at_narrow(at_div(at_mul(mc_player_tick_atan2(x, z), 180.0), 3.1415927410125732));
    p->base.yaw = yaw;
    p->arrow->heading_pc = 162;
    p->base.aU = yaw;
    p->arrow->heading_pc = 165;
    const float pitch = at_narrow(
        at_div(at_mul(mc_player_tick_atan2(y, at_widen(horizontal)), 180.0), 3.1415927410125732));
    p->base.aT = pitch;
    p->arrow->heading_pc = 187;
    p->base.aV = pitch;
    p->arrow->heading_pc = 190;
    p->arrow->fields.j = 0;
    p->arrow->heading_pc = 195;
    heading_at(e, 198);
    p->arrow->heading_pc = 198;
    p->arrow->heading_active = false;
    e->heading_returned = true;
    return true;
failed:
    p->arrow->heading_active = false;
    return false;
}

bool mc_entity_has_arrow(const mc_entity *p) {
    return mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_ARROW) &&
           p->classes->source == MC_ENTITY_CLIENT && p->arrow != NULL;
}

mc_entity_status mc_arrow_allocate(mc_entity_classes *classes, mc_entity **out,
                                   mc_entity_error *error) {
    return mc_entity_allocate_canonical(classes, MC_ENTITY_CLASS_KIND_ARROW, "Arrow allocation",
                                        out, error);
}

static bool position(mc_entity *p, double x, double y, double z, mc_arrow_error *e) {
    mc_entity_source_position_error child = {0};
    const mc_entity_status status = mc_entity_source_position_checked(p, x, y, z, &child);
    e->position = (mc_arrow_position_error){
        child.status, child.operation, child.bytecode, 0, child.source_null, child.native_policy};
    if (status == MC_ENTITY_OK)
        return true;
    e->child = site("sn", "e", "(DDD)V", (int)child.bytecode);
    if (child.source_null)
        return source_null(e, child.operation);
    return fail(e, status, child.operation);
}

static bool eye(mc_entity *shooter, float *out, mc_arrow_error *e) {
    if (mc_entity_is_kind(shooter, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER)) {
        *out = .12f; /* Actual inherited GS.w() literal return2. */
        return true;
    }
    const bool wolf = mc_entity_is_kind(shooter, MC_ENTITY_CLASS_KIND_WOLF);
    e->child = site(wolf ? "gi" : "ls", "w", "()F", 6);
    if (!fp(e))
        return false;
    *out = at_fmul(shooter->base.height, wolf ? .8f : .85f);
    e->child = site(NULL, NULL, NULL, -1);
    return true;
}

static bool living(mc_entity *p, mc_entity *shooter, mc_math_classes *math, mc_arrow_error *e) {
    constructor_at(e, living_descriptor, 52);
    const mc_entity_status held = mc_arrow_shooter_store(p, shooter, &e->entity);
    if (held != MC_ENTITY_OK)
        return fail(e, held, e->entity.operation);
    p->arrow->constructor_pc = 52;
    p->arrow->fields.a = mc_entity_is_kind(shooter, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER);
    p->arrow->constructor_pc = 60;
    mc_entity_source_size(p, .5f, .5f);
    p->arrow->constructor_pc = 68;
    constructor_at(e, living_descriptor, 73);
    if (shooter == NULL)
        return source_null(e, "Arrow constructor null Living position receiver");
    const double x = shooter->base.x, y = shooter->base.y;
    float height;
    constructor_at(e, living_descriptor, 81);
    if (!eye(shooter, &height, e))
        return false;
    constructor_at(e, living_descriptor, 85);
    if (!fp(e))
        return false;
    const double posed_y = at_add(y, at_widen(height)), z = shooter->base.z;
    const float yaw = shooter->base.yaw, pitch = shooter->base.aT;
    constructor_at(e, living_descriptor, 98);
    mc_entity_position_rotation_error pose = {0};
    const mc_entity_status posed =
        mc_entity_source_position_rotation(p, x, posed_y, z, yaw, pitch, &pose);
    e->pose = (mc_arrow_position_error){pose.status,      pose.operation,
                                        pose.bytecode,    pose.position_bytecode,
                                        pose.source_null, pose.native_policy};
    if (posed != MC_ENTITY_OK) {
        e->child = pose.position_bytecode == 0
                       ? site("sn", "c", "(DDDFF)V", (int)pose.bytecode)
                       : site("sn", "e", "(DDD)V", (int)pose.position_bytecode);
        if (pose.source_null)
            return source_null(e, pose.operation);
        return fail(e, posed, pose.operation);
    }
    p->arrow->constructor_pc = 98;
    constructor_at(e, living_descriptor, 112);
    if (!fp(e))
        return false;
    float value;
    constructor_at(e, living_descriptor, 116);
    if (!trig(math, true, at_fmul(at_fdiv(p->base.yaw, 180.0f), 3.1415927f), &value, e))
        return false;
    p->base.x = at_sub(p->base.x, at_widen(at_fmul(value, .16f)));
    p->arrow->constructor_pc = 124;
    p->base.y = at_sub(p->base.y, .10000000149011612);
    p->arrow->constructor_pc = 136;
    constructor_at(e, living_descriptor, 154);
    if (!trig(math, false, at_fmul(at_fdiv(p->base.yaw, 180.0f), 3.1415927f), &value, e))
        return false;
    p->base.z = at_sub(p->base.z, at_widen(at_fmul(value, .16f)));
    p->arrow->constructor_pc = 162;
    constructor_at(e, living_descriptor, 178);
    if (!position(p, p->base.x, p->base.y, p->base.z, e))
        return false;
    p->arrow->constructor_pc = 178;
    p->base.offset = 0.0f;
    p->arrow->constructor_pc = 183;
    float first, second;
    constructor_at(e, living_descriptor, 197);
    if (!trig(math, false, at_fmul(at_fdiv(p->base.yaw, 180.0f), 3.1415927f), &first, e))
        return false;
    first = at_fneg(first);
    constructor_at(e, living_descriptor, 211);
    if (!trig(math, true, at_fmul(at_fdiv(p->base.aT, 180.0f), 3.1415927f), &second, e))
        return false;
    p->base.velocity_x = at_widen(at_fmul(first, second));
    p->arrow->constructor_pc = 216;
    constructor_at(e, living_descriptor, 230);
    if (!trig(math, true, at_fmul(at_fdiv(p->base.yaw, 180.0f), 3.1415927f), &first, e))
        return false;
    constructor_at(e, living_descriptor, 243);
    if (!trig(math, true, at_fmul(at_fdiv(p->base.aT, 180.0f), 3.1415927f), &second, e))
        return false;
    p->base.velocity_z = at_widen(at_fmul(first, second));
    p->arrow->constructor_pc = 248;
    constructor_at(e, living_descriptor, 262);
    if (!trig(math, false, at_fmul(at_fdiv(p->base.aT, 180.0f), 3.1415927f), &value, e))
        return false;
    p->base.velocity_y = at_widen(at_fneg(value));
    p->arrow->constructor_pc = 267;
    const bool headed = heading_entered(p, math, p->base.velocity_x, p->base.velocity_y,
                                        p->base.velocity_z, 1.5f, 1.0f, e);
    if (!headed && e->child.class_name == NULL)
        e->child = e->site;
    constructor_at(e, living_descriptor, 286);
    if (!headed)
        return false;
    p->arrow->constructor_pc = 286;
    return true;
}

typedef enum constructor_kind { WORLD, POSITION, LIVING } constructor_kind;

static mc_entity_status construct(mc_entity *p, mc_world_runtime *world, constructor_kind kind,
                                  double x, double y, double z, mc_entity *shooter,
                                  mc_math_classes *math, mc_arrow_error *out) {
    mc_arrow_error e = fresh();
    const char *descriptor = kind == WORLD      ? world_descriptor
                             : kind == POSITION ? position_descriptor
                                                : living_descriptor;
    if (!mc_entity_has_arrow(p)) {
        fail(&e, MC_ENTITY_INVALID_ARGUMENT, "Arrow constructor actual canonical storage");
        goto done;
    }
    e.status = mc_entity_constructor_begin(p, MC_ENTITY_CLASS_KIND_ARROW, world,
                                           "Arrow constructor", "Arrow World profile", &e.entity);
    if (e.status != MC_ENTITY_OK) {
        fail(&e, e.status, e.entity.operation);
        goto done;
    }
    p->arrow->constructor_active = true;
    constructor_at(&e, descriptor, 2);
    e.status = mc_entity_construct_base(p, world, &e.entity);
    if (e.status != MC_ENTITY_OK) {
        e.child = site("sn", "<init>", "(Lfd;)V", (int)e.entity.bytecode);
        fail(&e, e.status, e.entity.operation);
    } else {
        mc_arrow_entity_state *s = p->arrow;
        s->constructor_pc = 2;
#define STORE(field, value, pc)                                                                    \
    do {                                                                                           \
        s->fields.field = value;                                                                   \
        s->constructor_pc = pc;                                                                    \
    } while (0)
        STORE(d, -1, 7);
        STORE(e, -1, 12);
        STORE(f, -1, 17);
        STORE(g, 0, 22);
        STORE(h, 0, 27);
        STORE(i, false, 32);
        STORE(a, false, 37);
        STORE(b, 0, 42);
        STORE(k, 0, 47);
#undef STORE
        bool completed = true;
        if (kind == LIVING)
            completed = living(p, shooter, math, &e);
        else {
            mc_entity_source_size(p, .5f, .5f);
            s->constructor_pc = 55;
            if (kind == POSITION) {
                constructor_at(&e, descriptor, 64);
                completed = position(p, x, y, z, &e);
                if (completed) {
                    s->constructor_pc = 64;
                    p->base.offset = 0.0f;
                    s->constructor_pc = 69;
                }
            }
        }
        if (completed) {
            s->constructor_pc = kind == WORLD ? 58u : kind == POSITION ? 72u : 289u;
            constructor_at(&e, descriptor, s->constructor_pc);
        }
    }
    (void)mc_entity_constructor_finish(p, e.status, &e.entity);
    p->arrow->constructor_active = false;
    if (e.status != MC_ENTITY_OK)
        p->arrow->failure = e;
done:
    if (out != NULL)
        *out = e;
    return e.status;
}

mc_entity_status mc_arrow_construct_world(mc_entity *p, mc_world_runtime *world,
                                          mc_arrow_error *e) {
    return construct(p, world, WORLD, 0, 0, 0, NULL, NULL, e);
}

mc_entity_status mc_arrow_construct_position(mc_entity *p, mc_world_runtime *world, double x,
                                             double y, double z, mc_arrow_error *e) {
    return construct(p, world, POSITION, x, y, z, NULL, NULL, e);
}

mc_entity_status mc_arrow_construct_living(mc_entity *p, mc_world_runtime *world,
                                           mc_entity *shooter, mc_math_classes *math,
                                           mc_arrow_error *e) {
    return construct(p, world, LIVING, 0, 0, 0, shooter, math, e);
}

mc_entity_status mc_arrow_heading(mc_entity *p, mc_math_classes *math, double x, double y, double z,
                                  float speed, float spread, mc_arrow_error *out) {
    mc_arrow_error e = fresh();
    if (!mc_entity_has_arrow(p))
        fail(&e, MC_ENTITY_INVALID_ARGUMENT, "Arrow heading actual canonical storage");
    else if (p->phase != MC_ENTITY_READY)
        fail(&e, MC_ENTITY_INVALID_PHASE, "Arrow heading READY owner");
    else if (p->arrow->heading_active || p->arrow->constructor_active)
        fail(&e, MC_ENTITY_BUSY, "Arrow heading already entered");
    else {
        e.status = mc_entity_retain(p);
        if (e.status != MC_ENTITY_OK)
            fail(&e, e.status, "Arrow heading independent owner hold");
        else {
            (void)heading_entered(p, math, x, y, z, speed, spread, &e);
            mc_entity_release(p);
        }
    }
    if (out != NULL)
        *out = e;
    return e.status;
}

bool mc_arrow_inspect(const mc_entity *p, mc_arrow_state *out) {
    if (!mc_entity_has_arrow(p) || out == NULL)
        return false;
    *out = (mc_arrow_state){.phase = p->phase,
                            .fields = p->arrow->fields,
                            .constructor_pc = p->arrow->constructor_pc,
                            .heading_pc = p->arrow->heading_pc,
                            .constructor_active = p->arrow->constructor_active,
                            .heading_active = p->arrow->heading_active,
                            .failure = &p->arrow->failure};
    return true;
}
