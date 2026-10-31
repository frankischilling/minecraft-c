#include "internal.h"

bool mt_slime_behavior(mc_mob_tick_scope *s) {
    if (!mt_despawn(s))
        return false;
    mc_entity *p = s->entity, *near = NULL;
    mc_living_entity_fields *v = mt_fields(s);
    mt_at(s, "uw", "f_", "()V", 12);
    if (!mt_nearest(s, 16.0, &near))
        return false;
    if (near != NULL) {
        mt_at(s, "uw", "f_", "()V", 26);
        if (!mt_look(s, near, 10.0f, 20.0f)) {
            mc_entity_release(near);
            return false;
        }
    }
    if (p->base.aX) {
        const int32_t old = p->mob->slime_jump_delay;
        p->mob->slime_jump_delay = mc_java_i32_sub(old, 1);
        if (old <= 0) {
            int32_t n;
            mt_at(s, "uw", "f_", "()V", 57);
            if (!mt_int(s, 20, &n))
                goto fail;
            p->mob->slime_jump_delay = mc_java_i32_add(n, 10);
            if (near != NULL)
                p->mob->slime_jump_delay /= 3;
            v->az = true;
            int8_t size;
            mt_at(s, "uw", "f_", "()V", 86);
            if (!mt_watch_byte(s, 16, &size))
                goto fail;
            if (size > 1) {
                float a, b;
                mc_world_runtime *sound_world = p->base.world;
                const mc_registry_string *sound;
                mt_at(s, "uw", "f_", "()V", 98);
                if (!mt_literal(s, "mob.slime", &sound))
                    goto fail;
                mt_at(s, "uw", "f_", "()V", 108);
                if (!mt_float(s, &a))
                    goto fail;
                mt_at(s, "uw", "f_", "()V", 115);
                if (!mt_float(s, &b) || !mt_fp(s))
                    goto fail;
                mt_at(s, "uw", "f_", "()V", 127);
                if (!mt_sound_prepared(s, sound_world, sound, .6f,
                                       at_fmul(at_fadd(at_fmul(at_fsub(a, b), .2f), 1.0f), .8f)))
                    goto fail;
            }
            p->mob->slime_a = 1.0f;
            float f;
            mt_at(s, "uw", "f_", "()V", 141);
            if (!mt_float(s, &f) || !mt_fp(s))
                goto fail;
            v->aw = at_fsub(1.0f, at_fmul(f, 2.0f));
            mt_at(s, "uw", "f_", "()V", 153);
            if (!mt_watch_byte(s, 16, &size))
                goto fail;
            v->ax = (float)size;
            mc_entity_release(near);
            mt_at(s, "uw", "f_", "()V", 186);
            return true;
        }
    }
    v->az = false;
    if (p->base.aX) {
        v->ax = 0.0f;
        v->aw = 0.0f;
    }
    mc_entity_release(near);
    mt_at(s, "uw", "f_", "()V", 186);
    return true;
fail:
    mc_entity_release(near);
    return false;
}

bool mt_squid_behavior(mc_mob_tick_scope *s) {
    int32_t draw;
    mc_mob_squid_fields *v = &s->entity->mob->squid;
    mt_at(s, "xt", "f_", "()V", 6);
    if (!mt_int(s, 50, &draw))
        return false;
    if (draw == 0 || !s->entity->base.bx || (v->n == 0.0f && v->o == 0.0f && v->p == 0.0f)) {
        float f;
        mt_at(s, "xt", "f_", "()V", 50);
        if (!mt_float(s, &f) || !mt_fp(s))
            return false;
        const float angle = at_fmul(at_fmul(f, 3.1415927f), 2.0f);
        mt_at(s, "xt", "f_", "()V", 61);
        if (!mt_math(s))
            return false;
        v->n = at_fmul(mc_math_cos(mc_math_classes_table(s->math), angle), .2f);
        mt_at(s, "xt", "f_", "()V", 77);
        if (!mt_float(s, &f))
            return false;
        v->o = at_fadd(-.1f, at_fmul(f, .2f));
        mt_at(s, "xt", "f_", "()V", 89);
        if (!mt_math(s))
            return false;
        v->p = at_fmul(mc_math_sin(mc_math_classes_table(s->math), angle), .2f);
    }
    if (!mt_despawn(s))
        return false;
    mt_at(s, "xt", "f_", "()V", 102);
    return true;
}

bool mt_squid_action_tail(mc_mob_tick_scope *s) {
    mc_entity *p = s->entity;
    mc_mob_squid_fields *v = &p->mob->squid;
    v->b = v->a;
    v->f = v->c;
    v->h = v->g;
    v->j = v->i;
    mt_at(s, "xt", "o", "()V", 45);
    if (!mt_fp(s))
        return false;
    v->g = at_fadd(v->g, v->l);
    if (v->g > 6.2831855f) {
        v->g = at_fsub(v->g, 6.2831855f);
        int32_t n;
        mt_at(s, "xt", "o", "()V", 76);
        if (!mt_int(s, 10, &n))
            return false;
        if (n == 0) {
            float f;
            mt_at(s, "xt", "o", "()V", 88);
            if (!mt_float(s, &f))
                return false;
            v->l = at_fmul(at_fdiv(1.0f, at_fadd(f, 1.0f)), .2f);
        }
    }
    bool water;
    mt_at(s, "xt", "o", "()V", 101);
    if (!mt_water(s, &water))
        return false;
    if (water) {
        if (v->g < 3.1415927f) {
            const float phase = at_fdiv(v->g, 3.1415927f);
            mt_at(s, "xt", "o", "()V", 132);
            if (!mt_math(s))
                return false;
            v->i = at_fmul(at_fmul(mc_math_sin(mc_math_classes_table(s->math),
                                               at_fmul(at_fmul(phase, phase), 3.1415927f)),
                                   3.1415927f),
                           .25f);
            if (at_widen(phase) > .75) {
                v->k = 1.0f;
                v->m = 1.0f;
            } else
                v->m = at_fmul(v->m, .8f);
        } else {
            v->i = 0.0f;
            v->k = at_fmul(v->k, .9f);
            v->m = at_fmul(v->m, .99f);
        }
        if (!mt_fields(s)->V) {
            p->base.velocity_x = at_widen(at_fmul(v->n, v->k));
            p->base.velocity_y = at_widen(at_fmul(v->o, v->k));
            p->base.velocity_z = at_widen(at_fmul(v->p, v->k));
        }
        mt_at(s, "xt", "o", "()V", 275);
        if (!mt_math(s))
            return false;
        const float speed = at_root(at_add(at_mul(p->base.velocity_x, p->base.velocity_x),
                                           at_mul(p->base.velocity_z, p->base.velocity_z)));
        mc_living_entity_fields *living = mt_fields(s);
        const float heading = at_fdiv(at_fmul(at_fneg(at_narrow(mc_player_tick_atan2(
                                                  p->base.velocity_x, p->base.velocity_z))),
                                              180.0f),
                                      3.1415927f);
        living->H = at_fadd(living->H, at_fmul(at_fsub(heading, living->H), .1f));
        p->base.yaw = living->H;
        v->c = at_fadd(v->c, at_fmul(at_fmul(3.1415927f, v->m), 1.5f));
        const float pitch = at_fdiv(
            at_fmul(at_fneg(at_narrow(mc_player_tick_atan2(at_widen(speed), p->base.velocity_y))),
                    180.0f),
            3.1415927f);
        v->a = at_fadd(v->a, at_fmul(at_fsub(pitch, v->a), .1f));
    } else {
        mt_at(s, "xt", "o", "()V", 384);
        if (!mt_math(s))
            return false;
        float f = mc_math_sin(mc_math_classes_table(s->math), v->g);
        if (f < 0.0f)
            f = at_fneg(f);
        v->i = at_fmul(at_fmul(f, 3.1415927f), .25f);
        if (!mt_fields(s)->V) {
            p->base.velocity_x = 0.0;
            p->base.velocity_y = at_mul(at_sub(p->base.velocity_y, .08), .9800000190734863);
            p->base.velocity_z = 0.0;
        }
        v->a = at_narrow(at_add(at_widen(v->a), at_mul(at_widen(at_fsub(-90.0f, v->a)), .02)));
    }
    mt_at(s, "xt", "o", "()V", 463);
    return true;
}
