#include "internal.h"
#include <stdlib.h>
#include <string.h>

static void environment_site(mc_mob_tick_scope *s, const mc_entity_tick_error *child) {
    const char *method = "U", *descriptor = "()V";
    const char *raw = child->source_method;
    if (raw != NULL) {
        if (strcmp(raw, "T()V") == 0)
            method = "T";
        else if (strcmp(raw, "L()Z") == 0) {
            method = "L";
            descriptor = "()Z";
        } else if (strcmp(raw, "b(IZ)V") == 0) {
            method = "b";
            descriptor = "(IZ)V";
        } else if (strcmp(raw, "ae()V") == 0)
            method = "ae";
        else if (strcmp(raw, "ab()V") == 0)
            method = "ab";
        else if (strcmp(raw, "g()Ljava/lang/String;") == 0) {
            method = "g";
            descriptor = "()Ljava/lang/String;";
        }
    }
    mt_at(s, child->source_class == NULL ? "sn" : child->source_class, method, descriptor,
          child->bytecode);
}

static bool delta(mc_mob_tick_scope *s, float *v, float bound, bool negative, unsigned int pc) {
    while (negative ? *v < bound : *v >= bound) {
        mt_at(s, "ls", "w_", "()V", pc);
        if (!mt_work(s))
            return false;
        *v = negative ? at_fadd(*v, 360.0f) : at_fsub(*v, 360.0f);
    }
    return true;
}

static bool previous(mc_mob_tick_scope *s, float current, float *old, bool negative,
                     unsigned int pc) {
    while (negative ? at_fsub(current, *old) < -180.0f : at_fsub(current, *old) >= 180.0f) {
        mt_at(s, "ls", "w_", "()V", pc);
        if (!mt_work(s))
            return false;
        *old = negative ? at_fsub(*old, 360.0f) : at_fadd(*old, 360.0f);
    }
    return true;
}

bool mt_living_w(mc_mob_tick_scope *s) {
    mt_at(s, "ls", "w_", "()V", 1);
    mc_entity_tick_error *child = malloc(sizeof(*child));
    if (child == NULL)
        return mt_fail(s, MC_ENTITY_ALLOCATION_FAILED, "Mob environment diagnostic workspace");
    mc_entity_tick_error_init(child);
    const bool environment = mc_entity_mob_environment_entered(s, child);
    if (!environment && s->error->status == MC_ENTITY_OK) {
        s->error->child = MC_MOB_TICK_CHILD_ENVIRONMENT;
        s->error->detail.environment = *child;
        environment_site(s, child);
        mt_fail(s, child->status, child->operation);
        s->error->native_policy = child->native_policy;
        s->error->source_exception = child->source_exception;
        s->error->exception_class = child->exception_class;
    }
    free(child);
    if (!environment)
        return false;
    s->error->state.environment_returned = true;
    mt_at(s, "ls", "w_", "()V", 5);
    if (!mt_concrete_o(s))
        return false;
    mc_entity *p = s->entity;
    mc_living_entity_fields *v = mt_fields(s);
    mt_at(s, "ls", "w_", "()V", 16);
    if (!mt_fp(s))
        return false;
    const double x = at_sub(p->base.x, p->base.aJ), z = at_sub(p->base.z, p->base.aL);
    mt_at(s, "ls", "w_", "()V", 35);
    if (!mt_math(s))
        return false;
    const float speed = at_root(at_add(at_mul(x, x), at_mul(z, z)));
    float target = v->H, phase = 0.0f, weight = 0.0f;
    v->J = v->K;
    if (!(speed <= .05f)) {
        weight = 1.0f;
        phase = at_fmul(speed, 3.0f);
        target = at_fsub(
            at_fdiv(at_fmul(at_narrow(mc_player_tick_atan2(z, x)), 180.0f), 3.1415927f), 90.0f);
    }
    if (v->X > 0.0f)
        target = p->base.yaw;
    if (!p->base.aX)
        weight = 0.0f;
    v->K = at_fadd(v->K, at_fmul(at_fsub(weight, v->K), .3f));
    float difference = at_fsub(target, v->H);
    if (!delta(s, &difference, -180.0f, true, 163) || !delta(s, &difference, 180.0f, false, 181))
        return false;
    v->H = at_fadd(v->H, at_fmul(difference, .3f));
    difference = at_fsub(p->base.yaw, v->H);
    if (!delta(s, &difference, -180.0f, true, 224) || !delta(s, &difference, 180.0f, false, 242))
        return false;
    const bool backwards = difference < -90.0f || difference >= 90.0f;
    if (difference < -75.0f)
        difference = -75.0f;
    if (difference >= 75.0f)
        difference = 75.0f;
    v->H = at_fsub(p->base.yaw, difference);
    if (at_fmul(difference, difference) > 2500.0f)
        v->H = at_fadd(v->H, at_fmul(difference, .2f));
    if (backwards)
        phase = at_fmul(phase, -1.0f);
    if (!previous(s, p->base.yaw, &p->base.aU, true, 365) ||
        !previous(s, p->base.yaw, &p->base.aU, false, 394) ||
        !previous(s, v->H, &v->I, true, 423) || !previous(s, v->H, &v->I, false, 452) ||
        !previous(s, p->base.aT, &p->base.aV, true, 481) ||
        !previous(s, p->base.aT, &p->base.aV, false, 510))
        return false;
    v->L = at_fadd(v->L, phase);
    mt_at(s, "ls", "w_", "()V", 528);
    return true;
}

static bool action(mc_mob_tick_scope *s) {
    mc_player_action_error *child = malloc(sizeof(*child));
    if (child == NULL)
        return mt_fail(s, MC_ENTITY_ALLOCATION_FAILED, "Mob action diagnostic workspace");
    mc_player_action_error_init(child);
    const bool ok = mc_entity_mob_action_entered(s, child);
    if (!ok && s->error->status == MC_ENTITY_OK) {
        s->error->child = MC_MOB_TICK_CHILD_ACTION;
        s->error->detail.action = *child;
        mt_at(s, child->source_class == NULL ? "ls" : child->source_class, "o", "()V",
              child->living_bytecode);
        mt_fail(s, child->status, child->operation);
    }
    free(child);
    if (ok)
        s->error->state.action_returned = true;
    return ok;
}

static bool monster_action(mc_mob_tick_scope *s) {
    float light;
    mt_at(s, "gz", "o", "()V", 2);
    if (!mt_brightness(s, &light))
        return false;
    if (light > .5f)
        mt_fields(s)->av = mc_java_i32_add(mt_fields(s)->av, 2);
    mt_at(s, "gz", "o", "()V", 24);
    if (!action(s))
        return false;
    mt_at(s, "gz", "o", "()V", 27);
    return true;
}

static bool sunlight_action(mc_mob_tick_scope *s, const char *name) {
    mc_world_runtime *w;
    mt_at(s, name, "o", "()V", 4);
    if (!mt_world(s, &w))
        return false;
    if (w->fields.f < 4) { /* Actual fd.f(): current sky subtraction <4. */
        float light;
        mt_at(s, name, "o", "()V", 12);
        if (!mt_brightness(s, &light))
            return false;
        if (light > .5f) {
            mc_entity *p = s->entity;
            mt_at(s, name, "o", "()V", 31);
            if (!mt_math(s) || !mt_fp(s))
                return false;
            const int32_t x = mc_math_floor_double(p->base.x);
            const int32_t y = mc_math_floor_double(p->base.y);
            const int32_t z = mc_math_floor_double(p->base.z);
            mt_at(s, name, "o", "()V", 48);
            if (!mt_world(s, &w))
                return false;
            s->error->child = MC_MOB_TICK_CHILD_QUERY;
            bool sky;
            if (mc_world_runtime_sky_visible(w, x, y, z, &sky, &s->error->detail.query.source) !=
                MC_WORLD_STORAGE_OK)
                return mt_fail(s, MC_ENTITY_WORLD_FAILED, "Mob actual sunlight sky query");
            s->error->child = MC_MOB_TICK_CHILD_NONE;
            if (sky) {
                float draw;
                mt_at(s, name, "o", "()V", 58);
                if (!mt_float(s, &draw))
                    return false;
                if (at_fmul(draw, 30.0f) < at_fmul(at_fsub(light, .4f), 2.0f))
                    p->base.bv = 300;
            }
        }
    }
    mt_at(s, name, "o", "()V", 82);
    if (!monster_action(s))
        return false;
    mt_at(s, name, "o", "()V", 85);
    return true;
}

bool mt_concrete_o(mc_mob_tick_scope *s) {
    switch (s->entity->class_literal->kind) {
        case MC_ENTITY_CLASS_KIND_ZOMBIE:
            return sunlight_action(s, "uz");
        case MC_ENTITY_CLASS_KIND_SKELETON:
            return sunlight_action(s, "fr");
        case MC_ENTITY_CLASS_KIND_SPIDER:
        case MC_ENTITY_CLASS_KIND_CREEPER:
            return monster_action(s);
        default:
            if (!action(s))
                return false;
            if (mc_entity_is_kind(s->entity, MC_ENTITY_CLASS_KIND_CHICKEN))
                return mt_chicken_action_tail(s);
            if (mc_entity_is_kind(s->entity, MC_ENTITY_CLASS_KIND_WOLF))
                return mt_wolf_action_tail(s);
            if (mc_entity_is_kind(s->entity, MC_ENTITY_CLASS_KIND_SQUID))
                return mt_squid_action_tail(s);
            return true;
    }
}
