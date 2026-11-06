#include "internal.h"

bool mt_brightness(mc_mob_tick_scope *s, float *out) {
    mc_entity *p = s->entity;
    s->error->child_site = (mc_mob_site){"sn", "a", "(F)F", 4};
    if (!mt_math(s) || !mt_fp(s))
        return false;
    const int32_t x = mc_math_floor_double(p->base.x);
    s->error->child_site.pc = 12;
    if (p->bounds == NULL)
        return mt_null(s, "Mob brightness null permanent bounds");
    const double h = at_mul(at_sub(p->bounds->max_y, p->bounds->min_y), .66);
    const int32_t y = mc_math_floor_double(at_add(at_sub(p->base.y, at_widen(p->base.offset)), h));
    const int32_t z = mc_math_floor_double(p->base.z);
    mc_world_runtime *w = p->base.world;
    int32_t bounds[6];
    for (size_t i = 0; i < 6; ++i) {
        const double value = i == 0   ? p->bounds->min_x
                             : i == 1 ? p->bounds->min_y
                             : i == 2 ? p->bounds->min_z
                             : i == 3 ? p->bounds->max_x
                             : i == 4 ? p->bounds->max_y
                                      : p->bounds->max_z;
        bounds[i] = mc_math_floor_double(value);
    }
    s->error->child_site.pc = 118;
    if (w == NULL)
        return mt_null(s, "Mob brightness null captured area World receiver");
    s->error->child = MC_MOB_TICK_CHILD_SOUND;
    bool loaded;
    if (mc_world_runtime_area_loaded(w, bounds[0], bounds[1], bounds[2], bounds[3], bounds[4],
                                     bounds[5], &loaded,
                                     &s->error->detail.sound) != MC_WORLD_STORAGE_OK)
        return mt_fail(s, MC_ENTITY_WORLD_FAILED, "Mob brightness actual area presence");
    float value = p->base.bE;
    if (loaded) {
        s->error->child_site.pc = 133;
        if (!mt_world(s, &w))
            return false;
        s->error->child = MC_MOB_TICK_CHILD_BRIGHTNESS;
        if (mc_world_runtime_brightness(w, x, y, z, &value, &s->error->detail.brightness) !=
            MC_WORLD_STORAGE_OK)
            return mt_brightness_fail(s, "Mob actual source brightness");
        if (value < p->base.bE)
            value = p->base.bE;
    }
    s->error->child = MC_MOB_TICK_CHILD_NONE;
    s->error->child_site = (mc_mob_site){0};
    *out = value;
    return true;
}

bool mt_eye(mc_mob_tick_scope *s, mc_entity *p, float *out) {
    if (mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER)) {
        *out = .12f;
        return true;
    }
    if (!mc_entity_has_mob(p))
        return mt_fail(s, MC_ENTITY_STATE_ERROR, "Mob facing actual Living eye-height child");
    *out = at_fmul(p->base.height, mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_WOLF) ? .8f : .85f);
    return true;
}

static bool approach(mc_mob_tick_scope *s, float current, float target, float maximum, float *out) {
    float delta = at_fsub(target, current);
    while (delta < -180.0f) {
        s->error->child_site = (mc_mob_site){"ls", "b", "(FFF)F", 16};
        if (!mt_work(s))
            return false;
        delta = at_fadd(delta, 360.0f);
    }
    while (delta >= 180.0f) {
        s->error->child_site = (mc_mob_site){"ls", "b", "(FFF)F", 34};
        if (!mt_work(s))
            return false;
        delta = at_fsub(delta, 360.0f);
    }
    if (delta > maximum)
        delta = maximum;
    if (delta < -maximum)
        delta = -maximum;
    *out = at_fadd(current, delta);
    return true;
}

bool mt_look(mc_mob_tick_scope *s, mc_entity *target, float yaw_limit, float pitch_limit) {
    s->error->child_site = (mc_mob_site){"ls", "a", "(Lsn;FF)V", 1};
    if (target == NULL)
        return mt_null(s, "Mob facing null target");
    if (!mt_fp(s))
        return false;
    mc_entity *p = s->entity;
    const double dx = at_sub(target->base.x, p->base.x), dz = at_sub(target->base.z, p->base.z);
    double dy;
    float own_eye, other_eye;
    s->error->child_site.pc = 40;
    if (!mt_eye(s, p, &own_eye))
        return false;
    if (mc_entity_living_fields(target) != NULL) {
        s->error->child_site.pc = 52;
        if (!mt_eye(s, target, &other_eye))
            return false;
        dy = at_sub(at_add(p->base.y, at_widen(own_eye)),
                    at_add(target->base.y, at_widen(other_eye)));
    } else {
        s->error->child_site.pc = 67;
        if (target->bounds == NULL)
            return mt_null(s, "Mob facing target null bounds");
        dy = at_sub(at_div(at_add(target->bounds->min_y, target->bounds->max_y), 2.0),
                    at_add(p->base.y, at_widen(own_eye)));
    }
    s->error->child_site.pc = 106;
    if (!mt_math(s))
        return false;
    const double horizontal = at_widen(at_root(at_add(at_mul(dx, dx), at_mul(dz, dz))));
    const float yaw = at_fsub(
        at_narrow(at_div(at_mul(mc_player_tick_atan2(dz, dx), 180.0), 3.1415927410125732)), 90.0f);
    const float pitch =
        at_narrow(-at_div(at_mul(mc_player_tick_atan2(dy, horizontal), 180.0), 3.1415927410125732));
    float value;
    if (!approach(s, p->base.aT, pitch, pitch_limit, &value))
        return false;
    p->base.aT = at_fneg(value);
    if (!approach(s, p->base.yaw, yaw, yaw_limit, &value))
        return false;
    p->base.yaw = value;
    s->error->child_site = (mc_mob_site){0};
    return true;
}
