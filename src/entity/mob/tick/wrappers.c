#include "internal.h"
#include "mc/item/registry/runtime.h"

static bool hostile(mc_mob_tick_scope *s) {
    return mc_entity_is_kind(s->entity, MC_ENTITY_CLASS_KIND_SPIDER) ||
           mc_entity_is_kind(s->entity, MC_ENTITY_CLASS_KIND_ZOMBIE) ||
           mc_entity_is_kind(s->entity, MC_ENTITY_CLASS_KIND_SKELETON) ||
           mc_entity_is_kind(s->entity, MC_ENTITY_CLASS_KIND_CREEPER);
}

static bool monster(mc_mob_tick_scope *s) {
    mt_at(s, "gz", "w_", "()V", 1);
    if (!mt_living_w(s))
        return false;
    mc_world_runtime *w;
    mt_at(s, "gz", "w_", "()V", 8);
    if (!mt_world(s, &w))
        return false;
    if (!w->fields.B) {
        mt_at(s, "gz", "w_", "()V", 18);
        if (!mt_world(s, &w))
            return false;
        if (w->fields.q == 0) {
            mt_at(s, "gz", "w_", "()V", 25);
            if (!mt_kill(s))
                return false;
        }
    }
    mt_at(s, "gz", "w_", "()V", 28);
    return true;
}

static bool creeper(mc_mob_tick_scope *s) {
    mc_mob_entity_state *m = s->entity->mob;
    m->creeper_b = m->creeper_a;
    mc_world_runtime *w;
    mt_at(s, "gb", "w_", "()V", 12);
    if (!mt_world(s, &w))
        return false;
    if (w->fields.B) {
        int8_t direction;
        mt_at(s, "gb", "w_", "()V", 19);
        if (!mt_watch_byte(s, 16, &direction))
            return false;
        if (direction > 0 && m->creeper_a == 0) {
            mc_world_runtime *sound_world = s->entity->base.world;
            const mc_registry_string *sound;
            mt_at(s, "gb", "w_", "()V", 39);
            if (!mt_literal(s, "random.fuse", &sound))
                return false;
            mt_at(s, "gb", "w_", "()V", 44);
            if (!mt_sound_prepared(s, sound_world, sound, 1.0f, .5f))
                return false;
        }
        m->creeper_a = mc_java_i32_add(m->creeper_a, (int32_t)direction);
        if (m->creeper_a < 0)
            m->creeper_a = 0;
        if (m->creeper_a >= 30)
            m->creeper_a = 30;
    }
    mt_at(s, "gb", "w_", "()V", 85);
    if (!monster(s))
        return false;
    mc_entity *target;
    mt_at(s, "gb", "w_", "()V", 89);
    if (!mt_target_get(s, true, &target))
        return false;
    if (target == NULL && m->creeper_a > 0) {
        mt_at(s, "gb", "w_", "()V", 104);
        if (!mt_watch_byte_set(s, 16, -1))
            return false;
        m->creeper_a = mc_java_i32_sub(m->creeper_a, 1);
        if (m->creeper_a < 0)
            m->creeper_a = 0;
    }
    mt_at(s, "gb", "w_", "()V", 129);
    return true;
}

static bool slime(mc_mob_tick_scope *s) {
    mc_entity *p = s->entity;
    p->mob->slime_b = p->mob->slime_a;
    const bool grounded = p->base.aX;
    mt_at(s, "uw", "w_", "()V", 14);
    if (!mt_living_w(s))
        return false;
    if (p->base.aX && !grounded) {
        int8_t size;
        mt_at(s, "uw", "w_", "()V", 29);
        if (!mt_watch_byte(s, 16, &size))
            return false;
        const int32_t count = mc_java_i32_mul((int32_t)size, 8);
        for (int32_t i = 0; i < count; i = mc_java_i32_add(i, 1)) {
            float a, b;
            mt_at(s, "uw", "w_", "()V", 47);
            if (!mt_work(s) || !mt_float(s, &a) || !mt_fp(s))
                return false;
            const float angle = at_fmul(at_fmul(a, 3.1415927f), 2.0f);
            mt_at(s, "uw", "w_", "()V", 61);
            if (!mt_float(s, &b))
                return false;
            const float radius = at_fadd(at_fmul(b, .5f), .5f);
            mt_at(s, "uw", "w_", "()V", 74);
            if (!mt_math(s))
                return false;
            const float x = at_fmul(
                at_fmul(at_fmul(mc_math_sin(mc_math_classes_table(s->math), angle), (float)size),
                        .5f),
                radius);
            mt_at(s, "uw", "w_", "()V", 90);
            if (!mt_math(s))
                return false;
            const float z = at_fmul(
                at_fmul(at_fmul(mc_math_cos(mc_math_classes_table(s->math), angle), (float)size),
                        .5f),
                radius);
            mc_world_runtime *particle_world = p->base.world;
            const mc_registry_string *particle;
            mt_at(s, "uw", "w_", "()V", 108);
            if (!mt_literal(s, "slime", &particle))
                return false;
            mt_at(s, "uw", "w_", "()V", 122);
            if (p->bounds == NULL)
                return mt_null(s, "Slime landing null bounds minY receiver");
            const double y = p->bounds->min_y;
            mt_at(s, "uw", "w_", "()V", 136);
            if (!mt_particle_prepared(s, particle_world, particle, at_add(p->base.x, at_widen(x)),
                                      y, at_add(p->base.z, at_widen(z)), 0.0, 0.0, 0.0))
                return false;
        }
        if (size > 2) {
            float a, b;
            mc_world_runtime *sound_world = p->base.world;
            const mc_registry_string *sound;
            mt_at(s, "uw", "w_", "()V", 155);
            if (!mt_literal(s, "mob.slime", &sound))
                return false;
            mt_at(s, "uw", "w_", "()V", 165);
            if (!mt_float(s, &a))
                return false;
            mt_at(s, "uw", "w_", "()V", 172);
            if (!mt_float(s, &b))
                return false;
            mt_at(s, "uw", "w_", "()V", 184);
            if (!mt_sound_prepared(s, sound_world, sound, .6f,
                                   at_fdiv(at_fadd(at_fmul(at_fsub(a, b), .2f), 1.0f), .8f)))
                return false;
        }
        p->mob->slime_a = -.5f;
    }
    mt_at(s, "uw", "w_", "()V", 200);
    if (!mt_fp(s))
        return false;
    p->mob->slime_a = at_fmul(p->mob->slime_a, .6f);
    mt_at(s, "uw", "w_", "()V", 204);
    return true;
}

bool mt_concrete_w(mc_mob_tick_scope *s) {
    if (mc_entity_is_kind(s->entity, MC_ENTITY_CLASS_KIND_CREEPER))
        return creeper(s);
    if (mc_entity_is_kind(s->entity, MC_ENTITY_CLASS_KIND_SLIME))
        return slime(s);
    if (hostile(s))
        return monster(s);
    if (!mt_living_w(s))
        return false;
    if (mc_entity_is_kind(s->entity, MC_ENTITY_CLASS_KIND_WOLF))
        return mt_wolf_tail(s);
    return true;
}

bool mt_chicken_action_tail(mc_mob_tick_scope *s) {
    mc_entity *p = s->entity;
    mc_mob_chicken_fields *v = &p->mob->chicken;
    v->g = v->b;
    v->f = v->c;
    mt_at(s, "ww", "o", "()V", 42);
    if (!mt_fp(s))
        return false;
    v->c = at_narrow(at_add(at_widen(v->c), at_mul(p->base.aX ? -1.0 : 4.0, .3)));
    if (v->c < 0.0f)
        v->c = 0.0f;
    if (v->c > 1.0f)
        v->c = 1.0f;
    if (!p->base.aX && v->h < 1.0f)
        v->h = 1.0f;
    v->h = at_narrow(at_mul(at_widen(v->h), .9));
    if (!p->base.aX && p->base.velocity_y < 0.0)
        p->base.velocity_y = at_mul(p->base.velocity_y, .6);
    v->b = at_fadd(v->b, at_fmul(v->h, 2.0f));
    mc_world_runtime *w;
    mt_at(s, "ww", "o", "()V", 158);
    if (!mt_world(s, &w))
        return false;
    if (!w->fields.B) {
        v->i = mc_java_i32_sub(v->i, 1);
        if (v->i <= 0) {
            float a, b;
            mc_world_runtime *sound_world = p->base.world;
            const mc_registry_string *sound;
            mt_at(s, "ww", "o", "()V", 183);
            if (!mt_literal(s, "mob.chickenplop", &sound))
                return false;
            mt_at(s, "ww", "o", "()V", 190);
            if (!mt_float(s, &a))
                return false;
            mt_at(s, "ww", "o", "()V", 197);
            if (!mt_float(s, &b))
                return false;
            mt_at(s, "ww", "o", "()V", 206);
            if (!mt_sound_prepared(s, sound_world, sound, 1.0f,
                                   at_fadd(at_fmul(at_fsub(a, b), .2f), 1.0f)))
                return false;
            const mc_item *egg = NULL;
            mt_at(s, "ww", "o", "()V", 210);
            if (s->registry == NULL ||
                mc_registry_item_static_get(s->registry, "aN", &egg, &s->error->registry) !=
                    MC_REGISTRY_OK) {
                (void)mt_fail(s, MC_ENTITY_STATE_ERROR, "Chicken actual Egg Item active use");
                mt_registry_classify(s, &s->error->registry);
                return false;
            }
            mt_at(s, "ww", "o", "()V", 213);
            if (egg == NULL)
                return mt_null(s, "Chicken null Egg Item receiver");
            const int32_t item_id = mc_item_fields_get(egg)->bf;
            s->error->state.drop =
                (mc_mob_tick_drop_arguments){.stored = true, .item_id = item_id, .count = 1};
            mt_at(s, "ww", "o", "()V", 217);
            return mt_fail(s, MC_ENTITY_STATE_ERROR, "Chicken actual Item drop child");
        }
    }
    mt_at(s, "ww", "o", "()V", 239);
    return true;
}
