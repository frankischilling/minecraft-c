#include "internal.h"
#include "mc/block/registry/runtime.h"
#include <string.h>

static bool wolf_flag(mc_mob_tick_scope *s, int mask, bool *out) {
    int8_t b;
    if (!mt_watch_byte(s, 16, &b))
        return false;
    *out = (b & mask) != 0;
    return true;
}

bool mt_despawn(mc_mob_tick_scope *s) {
    mc_entity *p = s->entity, *nearest = NULL;
    mt_at(s, "ls", "X", "()V", 8);
    if (!mt_nearest(s, -1.0, &nearest))
        return false;
    bool allowed = true;
    mt_at(s, "ls", "X", "()V", 13);
    if (mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_WOLF)) {
        bool tame;
        if (!wolf_flag(s, 4, &tame)) {
            mc_entity_release(nearest);
            return false;
        }
        allowed = !tame;
    }
    bool ok = true;
    if (allowed && nearest != NULL) {
        if (!mt_fp(s)) {
            ok = false;
            goto done;
        }
        const double dx = at_sub(nearest->base.x, p->base.x);
        const double dy = at_sub(nearest->base.y, p->base.y);
        const double dz = at_sub(nearest->base.z, p->base.z);
        const double d = at_add(at_add(at_mul(dx, dx), at_mul(dy, dy)), at_mul(dz, dz));
        if (d > 16384.0) {
            mt_at(s, "ls", "X", "()V", 82);
            if (!mt_kill(s)) {
                ok = false;
                goto done;
            }
        }
        if (mt_fields(s)->av > 600) {
            int32_t draw;
            mt_at(s, "ls", "X", "()V", 102);
            if (!mt_int(s, 800, &draw)) {
                ok = false;
                goto done;
            }
            if (draw == 0) {
                if (d < 1024.0)
                    mt_fields(s)->av = 0;
                else {
                    mt_at(s, "ls", "X", "()V", 126);
                    if (!mt_kill(s))
                        ok = false;
                }
            }
        }
    }
done:
    mc_entity_release(nearest);
    if (ok)
        mt_at(s, "ls", "X", "()V", 129);
    return ok;
}

bool mt_base_behavior(mc_mob_tick_scope *s) {
    mc_entity *p = s->entity;
    mc_living_entity_fields *v = mt_fields(s);
    v->av = mc_java_i32_add(v->av, 1);
    mc_entity *near = NULL;
    mt_at(s, "ls", "f_", "()V", 18);
    if (!mt_nearest(s, -1.0, &near))
        return false;
    if (!mt_despawn(s)) {
        mc_entity_release(near);
        return false;
    }
    v->aw = 0.0f;
    v->ax = 0.0f;
    float draw;
    mt_at(s, "ls", "f_", "()V", 43);
    if (!mt_float(s, &draw) || !mt_fp(s))
        goto fail;
    if (draw < .02f) {
        mc_entity_release(near);
        near = NULL;
        mt_at(s, "ls", "f_", "()V", 59);
        if (!mt_nearest(s, 8.0, &near))
            goto fail;
        if (near != NULL) {
            mt_at(s, "ls", "f_", "()V", 69);
            if (!mt_target_store(s, false, near))
                goto fail;
            int32_t n;
            mt_at(s, "ls", "f_", "()V", 81);
            if (!mt_int(s, 20, &n))
                goto fail;
            v->aC = mc_java_i32_add(10, n);
        } else {
            mt_at(s, "ls", "f_", "()V", 96);
            if (!mt_float(s, &draw))
                goto fail;
            v->ay = at_fmul(at_fsub(draw, .5f), 20.0f);
        }
    }
    mc_entity *look;
    mt_at(s, "ls", "f_", "()V", 109);
    if (!mt_target_get(s, false, &look))
        goto fail;
    if (look != NULL) {
        float limit = 40.0f;
        if (mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_WOLF)) {
            bool sit;
            if (!wolf_flag(s, 1, &sit))
                goto fail;
            if (sit)
                limit = 20.0f;
        }
        mt_at(s, "ls", "f_", "()V", 127);
        if (!mt_look(s, look, 10.0f, limit))
            goto fail;
        const int32_t old = v->aC;
        v->aC = mc_java_i32_sub(old, 1);
        bool clear = old <= 0 || look->base.be;
        if (!clear) {
            const double dx = at_sub(p->base.x, look->base.x);
            const double dy = at_sub(p->base.y, look->base.y);
            const double dz = at_sub(p->base.z, look->base.z);
            clear = at_add(at_add(at_mul(dx, dx), at_mul(dy, dy)), at_mul(dz, dz)) > 64.0;
        }
        if (clear) {
            mt_at(s, "ls", "f_", "()V", 172);
            if (!mt_target_store(s, false, NULL))
                goto fail;
        }
    } else {
        mt_at(s, "ls", "f_", "()V", 182);
        if (!mt_float(s, &draw))
            goto fail;
        if (draw < .05f) {
            mt_at(s, "ls", "f_", "()V", 196);
            if (!mt_float(s, &draw))
                goto fail;
            v->ay = at_fmul(at_fsub(draw, .5f), 20.0f);
        }
        p->base.yaw = at_fadd(p->base.yaw, v->ay);
        p->base.aT = v->aA;
    }
    bool water, lava;
    mt_at(s, "ls", "f_", "()V", 230);
    if (!mt_water(s, &water))
        goto fail;
    mt_at(s, "ls", "f_", "()V", 235);
    if (!mt_lava(s, &lava))
        goto fail;
    if (water || lava) {
        mt_at(s, "ls", "f_", "()V", 254);
        if (!mt_float(s, &draw))
            goto fail;
        v->az = draw < .8f;
    }
    mc_entity_release(near);
    mt_at(s, "ls", "f_", "()V", 271);
    return true;
fail:
    mc_entity_release(near);
    return false;
}

static bool weight(mc_mob_tick_scope *s, int32_t x, int32_t y, int32_t z, float *out) {
    mc_world_runtime *w;
    if (!mt_world(s, &w))
        return false;
    const bool animal = !mc_entity_is_kind(s->entity, MC_ENTITY_CLASS_KIND_SPIDER) &&
                        !mc_entity_is_kind(s->entity, MC_ENTITY_CLASS_KIND_ZOMBIE) &&
                        !mc_entity_is_kind(s->entity, MC_ENTITY_CLASS_KIND_SKELETON) &&
                        !mc_entity_is_kind(s->entity, MC_ENTITY_CLASS_KIND_CREEPER);
    if (animal) {
        int32_t below;
        s->error->child = MC_MOB_TICK_CHILD_QUERY;
        if (mc_world_runtime_block_get_detailed(
                w, x, mc_java_i32_sub(y, 1), z, &below, &s->error->detail.query.cause,
                &s->error->detail.query.acquisition) != MC_WORLD_STORAGE_OK) {
            (void)mt_fail(s, MC_ENTITY_WORLD_FAILED, "Creature path-weight actual block query");
            mt_registry_classify(s, &s->error->detail.query.acquisition.registry);
            return false;
        }
        const mc_block *grass = NULL;
        if (s->registry == NULL ||
            mc_registry_block_static_get(s->registry, "v", &grass, &s->error->registry) !=
                MC_REGISTRY_OK) {
            (void)mt_fail(s, MC_ENTITY_STATE_ERROR, "Creature path-weight actual Grass static");
            mt_registry_classify(s, &s->error->registry);
            return false;
        }
        if (grass == NULL)
            return mt_null(s, "Creature path-weight null Grass receiver");
        if (below == mc_block_fields_get(grass)->bn) {
            *out = 10.0f;
            s->error->child = MC_MOB_TICK_CHILD_NONE;
            return true;
        }
    }
    s->error->child = MC_MOB_TICK_CHILD_BRIGHTNESS;
    float light;
    if (mc_world_runtime_brightness(w, x, y, z, &light, &s->error->detail.brightness) !=
        MC_WORLD_STORAGE_OK)
        return mt_brightness_fail(s, "Creature path-weight brightness child");
    if (!mt_fp(s))
        return false;
    *out = animal ? at_fsub(light, .5f) : at_fsub(.5f, light);
    s->error->child = MC_MOB_TICK_CHILD_NONE;
    return true;
}

static bool wander(mc_mob_tick_scope *s) {
    bool found = false;
    float best = -99999.0f;
    int32_t selected[3] = {-1, -1, -1};
    for (int i = 0; i < 10; ++i) {
        mt_at(s, "ii", "E", "()V", 33);
        if (!mt_work(s))
            return false;
        int32_t n, xyz[3];
        static const unsigned int pcs[] = {33, 57, 81};
        static const unsigned int floor_pcs[] = {42, 66, 90};
        for (size_t a = 0; a < 3; ++a) {
            mt_at(s, "ii", "E", "()V", pcs[a]);
            if (!mt_int(s, a == 1 ? 7 : 13, &n))
                return false;
            mt_at(s, "ii", "E", "()V", floor_pcs[a]);
            if (!mt_math(s) || !mt_fp(s))
                return false;
            const double value = a == 0   ? s->entity->base.x
                                 : a == 1 ? s->entity->base.y
                                          : s->entity->base.z;
            xyz[a] = mc_math_floor_double(at_sub(at_add(value, (double)n), a == 1 ? 3.0 : 6.0));
        }
        float current;
        mt_at(s, "ii", "E", "()V", 102);
        if (!weight(s, xyz[0], xyz[1], xyz[2], &current))
            return false;
        if (current > best) {
            best = current;
            selected[0] = xyz[0];
            selected[1] = xyz[1];
            selected[2] = xyz[2];
            found = true;
        }
    }
    if (found) {
        mc_world_runtime *captured = s->entity->base.world;
        s->error->state.path = (mc_mob_tick_path_arguments){
            .kind = MC_MOB_TICK_PATH_COORDINATES,
            .stored = true,
            .receiver_identity = captured == NULL ? 0 : (uintptr_t)captured,
            .x = selected[0],
            .y = selected[1],
            .z = selected[2],
            .range = 10.0f};
        mt_at(s, "ii", "E", "()V", 153);
        if (captured == NULL)
            return mt_null(s, "Creature coordinate path null captured World receiver");
        s->error->child_site = (mc_mob_site){"fd", "a", "(Lsn;IIIF)Ldh;", 0};
        return mt_fail(s, MC_ENTITY_STATE_ERROR, "Creature actual World coordinate path child");
    }
    mt_at(s, "ii", "E", "()V", 159);
    return true;
}

static bool target_path(mc_mob_tick_scope *s, unsigned int target_pc, unsigned int invoke_pc) {
    mc_world_runtime *captured = s->entity->base.world;
    mc_entity *target;
    mt_at(s, "ii", "f_", "()V", target_pc);
    if (!mt_target_get(s, true, &target))
        return false;
    s->error->state.path = (mc_mob_tick_path_arguments){
        .kind = MC_MOB_TICK_PATH_TARGET,
        .stored = true,
        .receiver_identity = captured == NULL ? 0 : (uintptr_t)captured,
        .target_identity = target == NULL ? 0 : (uintptr_t)target,
        .range = 16.0f};
    mt_at(s, "ii", "f_", "()V", invoke_pc);
    if (captured == NULL)
        return mt_null(s, "Creature target path null captured World receiver");
    s->error->child_site = (mc_mob_site){"fd", "a", "(Lsn;Lsn;F)Ldh;", 0};
    return mt_fail(s, MC_ENTITY_STATE_ERROR, "Creature actual World target path child");
}

static bool seek(mc_mob_tick_scope *s, mc_entity **out) {
    *out = NULL;
    const mc_entity_class_kind k = s->entity->class_literal->kind;
    if (k == MC_ENTITY_CLASS_KIND_SPIDER) {
        float light;
        if (!mt_brightness(s, &light))
            return false;
        if (!(light < .5f))
            return true;
        return mt_nearest(s, 16.0, out);
    }
    if (k == MC_ENTITY_CLASS_KIND_WOLF) {
        bool angry;
        if (!wolf_flag(s, 2, &angry))
            return false;
        return !angry || mt_nearest(s, 16.0, out);
    }
    if (k == MC_ENTITY_CLASS_KIND_ZOMBIE || k == MC_ENTITY_CLASS_KIND_SKELETON ||
        k == MC_ENTITY_CLASS_KIND_CREEPER) {
        if (!mt_nearest(s, 16.0, out))
            return false;
        if (*out != NULL) {
            bool visible;
            if (!mt_visible(s, *out, &visible)) {
                mc_entity_release(*out);
                *out = NULL;
                return false;
            }
            if (!visible) {
                mc_entity_release(*out);
                *out = NULL;
            }
        }
    }
    return true;
}

bool mt_creature(mc_mob_tick_scope *s) {
    mc_mob_entity_state *m = s->entity->mob;
    mt_at(s, "ii", "f_", "()V", 2);
    bool attacking = false;
    if (mc_entity_is_kind(s->entity, MC_ENTITY_CLASS_KIND_WOLF)) {
        if (!wolf_flag(s, 1, &attacking))
            return false;
        attacking = attacking || m->wolf.g;
    }
    m->creature_attacking = attacking;
    mc_entity *target;
    mt_at(s, "ii", "f_", "()V", 12);
    if (!mt_target_get(s, true, &target))
        return false;
    if (target == NULL) {
        mc_entity *selected = NULL;
        mt_at(s, "ii", "f_", "()V", 20);
        if (!seek(s, &selected))
            return false;
        mt_at(s, "ii", "f_", "()V", 23);
        const bool stored = mt_target_store(s, true, selected);
        mc_entity_release(selected);
        if (!stored || !mt_target_get(s, true, &target))
            return false;
        if (target != NULL) {
            return target_path(s, 40, 44);
        }
    } else {
        mt_at(s, "ii", "f_", "()V", 57);
        mc_living_entity_fields *v = mc_entity_living_fields(target);
        const bool alive = !target->base.be && (v == NULL || v->Y > 0);
        if (!alive) {
            if (!mt_target_store(s, true, NULL))
                return false;
            target = NULL;
        } else {
            mt_at(s, "ii", "f_", "()V", 76);
            if (!mt_math(s) || !mt_fp(s))
                return false;
            const double dx = at_sub(s->entity->base.x, target->base.x);
            const double dy = at_sub(s->entity->base.y, target->base.y);
            const double dz = at_sub(s->entity->base.z, target->base.z);
            const float distance =
                at_root(at_add(at_add(at_mul(dx, dx), at_mul(dy, dy)), at_mul(dz, dz)));
            (void)distance;
            mt_at(s, "ii", "f_", "()V", 85);
            bool visible;
            if (!mt_visible(s, target, &visible))
                return false;
            const mc_entity_class_kind kind = s->entity->class_literal->kind;
            if (visible) {
                mt_at(s, "ii", "f_", "()V", 93);
                if (!mt_target_get(s, true, &target))
                    return false;
                const char *declaring = kind == MC_ENTITY_CLASS_KIND_SPIDER     ? "cn"
                                        : kind == MC_ENTITY_CLASS_KIND_ZOMBIE   ? "gz"
                                        : kind == MC_ENTITY_CLASS_KIND_SKELETON ? "fr"
                                        : kind == MC_ENTITY_CLASS_KIND_CREEPER  ? "gb"
                                        : kind == MC_ENTITY_CLASS_KIND_WOLF     ? "gi"
                                                                                : NULL;
                if (declaring != NULL) {
                    mt_at(s, "ii", "f_", "()V", 97);
                    s->error->child_site = (mc_mob_site){declaring, "a", "(Lsn;F)V", 0};
                    return mt_fail(s, MC_ENTITY_STATE_ERROR,
                                   "Creature actual concrete attack child");
                }
                /* The other selected concrete kinds inherit II.a(sn,float),
                 * whose actual source body is return0. */
            } else {
                mt_at(s, "ii", "f_", "()V", 105);
                if (!mt_target_get(s, true, &target))
                    return false;
                if (kind == MC_ENTITY_CLASS_KIND_CREEPER) {
                    mt_at(s, "ii", "f_", "()V", 109);
                    s->error->child_site = (mc_mob_site){"gb", "b", "(Lsn;F)V", 0};
                    return mt_fail(s, MC_ENTITY_STATE_ERROR, "Creeper actual unseen-target child");
                } /* The other selected kinds inherit actual II.b return0. */
            }
        }
    }
    if (!m->creature_attacking) {
        mt_at(s, "ii", "f_", "()V", 120);
        if (!mt_target_get(s, true, &target))
            return false;
    }
    if (!m->creature_attacking && target != NULL) {
        if (m->creature_path != NULL) {
            int32_t n;
            mt_at(s, "ii", "f_", "()V", 139);
            if (!mt_int(s, 20, &n))
                return false;
            if (n != 0)
                goto after_wander;
        }
        return target_path(s, 152, 156);
    }
    if (!m->creature_attacking) {
        int32_t n = 1;
        bool request = false;
        if (m->creature_path == NULL) {
            mt_at(s, "ii", "f_", "()V", 185);
            if (!mt_int(s, 80, &n))
                return false;
            request = n == 0;
        }
        if (!request) {
            mt_at(s, "ii", "f_", "()V", 197);
            if (!mt_int(s, 80, &n))
                return false;
            request = n == 0;
        }
        if (request && !wander(s))
            return false;
    }
after_wander:
    mt_at(s, "ii", "f_", "()V", 211);
    if (s->entity->bounds == NULL)
        return mt_null(s, "Creature null bounds minY receiver");
    mt_at(s, "ii", "f_", "()V", 218);
    if (!mt_math(s) || !mt_fp(s))
        return false;
    (void)mc_math_floor_double(at_add(s->entity->bounds->min_y, .5));
    bool water, lava;
    mt_at(s, "ii", "f_", "()V", 223);
    if (!mt_water(s, &water))
        return false;
    mt_at(s, "ii", "f_", "()V", 228);
    if (!mt_lava(s, &lava))
        return false;
    s->entity->base.aT = 0.0f;
    if (m->creature_path == NULL) {
        mt_at(s, "ii", "f_", "()V", 258);
        if (!mt_base_behavior(s))
            return false;
        m->creature_path = NULL;
        mt_at(s, "ii", "f_", "()V", 266);
        return true;
    }
    int32_t n;
    mt_at(s, "ii", "f_", "()V", 251);
    if (!mt_int(s, 100, &n))
        return false;
    if (n == 0) {
        if (!mt_base_behavior(s))
            return false;
        m->creature_path = NULL;
        mt_at(s, "ii", "f_", "()V", 266);
        return true;
    }
    mt_at(s, "ii", "f_", "()V", 272);
    return mt_fail(s, MC_ENTITY_STATE_ERROR, "Creature actual retained path traversal child");
}

bool mc_mob_tick_behavior(mc_mob_tick_scope *s) {
    const mc_entity_class_kind k = s->entity->class_literal->kind;
    const bool ok = k == MC_ENTITY_CLASS_KIND_SLIME   ? mt_slime_behavior(s)
                    : k == MC_ENTITY_CLASS_KIND_SQUID ? mt_squid_behavior(s)
                    : k == MC_ENTITY_CLASS_KIND_WOLF  ? mt_wolf_behavior(s)
                                                      : mt_creature(s);
    if (ok)
        s->error->state.behavior_returned = true;
    return ok;
}
