#include "tick_internal.h"

static void base_at(mc_entity_tick_error *e, unsigned int pc) {
    e->source_class = "sn";
    e->source_method = "U()V";
    e->base_bytecode = pc;
    e->bytecode = pc;
}

static void living_at(mc_entity_tick_error *e, unsigned int pc) {
    e->source_class = "ls";
    e->source_method = "U()V";
    e->living_bytecode = pc;
    e->bytecode = pc;
}

static bool next_float(mc_entity *entity, float *out, mc_entity_tick_error *e, unsigned int pc) {
    if (entity->random == NULL)
        return mc_tick_fail(e, MC_ENTITY_STATE_ERROR, "Tick null Entity Random", pc);
    *out = mc_java_random_next_float(entity->random);
    return true;
}

static bool literal(mc_entity *entity, mc_registry_classes *classes, const char *text,
                    const mc_registry_string **out, mc_entity_tick_error *e, unsigned int pc) {
    return mc_tick_code(entity, classes, e, pc) &&
           mc_tick_registry(e, mc_registry_runtime_literal_ascii(classes, text, out, &e->registry),
                            "Tick actual code literal", pc);
}

static bool work(mc_entity_tick_error *e, unsigned int pc) {
    if (e->particle_work == e->max_particle_work)
        return mc_tick_fail(e, MC_ENTITY_LIMIT, "Tick pending particle iteration ceiling", pc);
    ++e->particle_work;
    return true;
}

static bool remote(mc_entity *entity, bool *out, mc_entity_tick_error *e, unsigned int pc) {
    mc_world_runtime *world = entity->base.world;
    if (world == NULL)
        return mc_tick_fail(e, MC_ENTITY_STATE_ERROR, "Tick null World B receiver", pc);
    *out = world->fields.B;
    return true;
}

static bool particle(mc_world_runtime *world, const mc_registry_string *name, double x, double y,
                     double z, double vx, double vy, double vz, mc_entity_tick_error *e,
                     unsigned int pc) {
    return mc_tick_world(
        e, mc_world_runtime_particle(world, name, x, y, z, vx, vy, vz, &e->world.presentation),
        "Tick actual World particle", pc);
}

static bool splash(mc_entity *entity, mc_registry_classes *classes, mc_entity_tick_error *e) {
    base_at(e, 138);
    if (!mc_tick_fp(e, 110))
        return false;
    const double vx = entity->base.velocity_x;
    double squared = mc_tick_dmul(mc_tick_dmul(vx, entity->base.velocity_x), mc_tick_widen(.2f));
    squared = mc_tick_dadd(squared, mc_tick_dmul(entity->base.velocity_y, entity->base.velocity_y));
    squared = mc_tick_dadd(
        squared, mc_tick_dmul(mc_tick_dmul(entity->base.velocity_z, entity->base.velocity_z),
                              mc_tick_widen(.2f)));
    const double root = _mm_cvtsd_f64(_mm_sqrt_sd(_mm_setzero_pd(), _mm_set_sd(squared)));
    const float root_float = _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), _mm_set_sd(root)));
    float volume = mc_tick_fmul(root_float, .2f);
    if (volume > 1.0f)
        volume = 1.0f;
    mc_world_runtime *sound_world = entity->base.world;
    const mc_registry_string *name = NULL;
    if (!literal(entity, classes, "random.splash", &name, e, 158))
        return false;
    float first, second;
    if (!next_float(entity, &first, e, 166) || !next_float(entity, &second, e, 173))
        return false;
    const float pitch = mc_tick_fadd(1.0f, mc_tick_fmul(mc_tick_fsub(first, second), .4f));
    base_at(e, 181);
    if (!mc_tick_world(
            e, mc_world_runtime_entity_sound(sound_world, entity, name, volume, pitch, &e->world),
            "Tick actual splash sound", 181))
        return false;
    base_at(e, 188);
    if (entity->bounds == NULL)
        return mc_tick_fail(e, MC_ENTITY_STATE_ERROR, "Tick splash null permanent bounds", 188);
    if (!mc_tick_fp(e, 191))
        return false;
    const float floor_y = mc_tick_i2f(mc_math_floor_double(entity->bounds->min_y));
    for (int kind = 0; kind < 2; ++kind) {
        int32_t index = 0;
        for (;;) {
            base_at(e, kind == 0 ? 209u : 321u);
            if (!mc_tick_fp(e, e->base_bytecode))
                return false;
            const float count = mc_tick_i2f(index);
            const float ceiling = mc_tick_fadd(1.0f, mc_tick_fmul(entity->base.width, 20.0f));
            if (!(count < ceiling))
                break;
            if (!work(e, kind == 0 ? 217u : 329u))
                return false;
            float draw;
            if (!next_float(entity, &draw, e, kind == 0 ? 217u : 329u))
                return false;
            const float ox =
                mc_tick_fmul(mc_tick_fsub(mc_tick_fmul(draw, 2.0f), 1.0f), entity->base.width);
            if (!next_float(entity, &draw, e, kind == 0 ? 235u : 347u))
                return false;
            const float oz =
                mc_tick_fmul(mc_tick_fsub(mc_tick_fmul(draw, 2.0f), 1.0f), entity->base.width);
            mc_world_runtime *world = entity->base.world;
            if (!literal(entity, classes, kind == 0 ? "bubble" : "splash", &name, e,
                         kind == 0 ? 253u : 365u))
                return false;
            const double x = mc_tick_dadd(entity->base.x, mc_tick_widen(ox));
            const double y = mc_tick_widen(mc_tick_fadd(floor_y, 1.0f));
            const double z = mc_tick_dadd(entity->base.z, mc_tick_widen(oz));
            const double velocity_x = entity->base.velocity_x;
            double velocity_y = entity->base.velocity_y;
            if (kind == 0) {
                if (!next_float(entity, &draw, e, 287))
                    return false;
                velocity_y = mc_tick_dsub(velocity_y, mc_tick_widen(mc_tick_fmul(draw, .2f)));
            }
            const double velocity_z = entity->base.velocity_z;
            base_at(e, kind == 0 ? 299u : 399u);
            if (!particle(world, name, x, y, z, velocity_x, velocity_y, velocity_z, e,
                          e->base_bytecode))
                return false;
            index = mc_java_i32_add(index, 1);
        }
    }
    return true;
}

static bool damage(mc_entity *entity, mc_registry_classes *classes, int32_t amount,
                   mc_entity_tick_error *e, unsigned int pc) {
    bool ordinary;
    return mc_tick_effect(
        e, mc_entity_effect_damage_in_context(entity, classes, NULL, amount, &ordinary, &e->effect),
        "Tick actual Player damage virtual", pc);
}

static bool lava_damage(mc_entity *entity, mc_registry_classes *classes, mc_entity_tick_error *e) {
    e->source_class = "sn";
    e->source_method = "ae()V";
    e->lava_bytecode = 1;
    if (entity->base.bC)
        return true;
    e->lava_bytecode = 13;
    if (!damage(entity, classes, 4, e, 13))
        return false;
    entity->base.bv = 600;
    return true;
}

static bool void_damage(mc_entity *entity, mc_registry_classes *classes, mc_entity_tick_error *e) {
    if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM)) {
        e->source_class = "sn";
        e->source_method = "ab()V";
        e->void_bytecode = 1;
        return mc_tick_effect(e, mc_entity_effect_dead(entity, &e->effect),
                              "Item void actual inherited K", 1);
    }
    e->source_class = "ls";
    e->source_method = "ab()V";
    e->void_bytecode = 3;
    return damage(entity, classes, 4, e, 3);
}

static bool base_body(mc_entity *entity, mc_registry_classes *classes,
                      const mc_entity_tick_limits *limits, mc_entity_tick_error *e) {
    base_at(e, 1);
    mc_entity *rider = entity->base.aH;
    if (rider != NULL) {
        base_at(e, 11);
        if (rider->classes == NULL || rider->holds == 0 || rider->phase != MC_ENTITY_READY)
            return mc_tick_fail(e, MC_ENTITY_STATE_ERROR, "Tick riding Entity live field owner",
                                11);
        const mc_entity_status held = mc_entity_retain(rider);
        if (!mc_tick_effect(e, held, "Tick riding Entity lifetime hold", 11))
            return false;
        const bool dead = rider->base.be;
        mc_entity_release(rider);
        if (dead)
            entity->base.aH = NULL;
    }
    entity->base.bt = mc_java_i32_add(entity->base.bt, 1);
    entity->base.bi = entity->base.bj;
    entity->base.aJ = entity->base.x;
    entity->base.aK = entity->base.y;
    entity->base.aL = entity->base.z;
    entity->base.aV = entity->base.aT;
    entity->base.aU = entity->base.yaw;
    bool water;
    base_at(e, 81);
    if (!mc_tick_effect(e,
                        mc_entity_environment_water(entity, classes,
                                                    limits == NULL ? NULL : &limits->material,
                                                    &water, &e->environment),
                        "Tick actual water virtual", 81))
        return false;
    if (water) {
        if (!entity->base.bx && !entity->base.private_c && !splash(entity, classes, e))
            return false;
        entity->base.bk = 0.0f;
        entity->base.bx = true;
        entity->base.bv = 0;
    } else
        entity->base.bx = false;
    bool is_remote;
    base_at(e, 435);
    if (!remote(entity, &is_remote, e, 435))
        return false;
    if (is_remote)
        entity->base.bv = 0;
    else if (entity->base.bv > 0) {
        if (entity->base.bC) {
            entity->base.bv = mc_java_i32_sub(entity->base.bv, 4);
            if (entity->base.bv < 0)
                entity->base.bv = 0;
        } else {
            if (entity->base.bv % 20 == 0) {
                base_at(e, 501);
                if (!damage(entity, classes, 1, e, 501))
                    return false;
            }
            entity->base.bv = mc_java_i32_sub(entity->base.bv, 1);
        }
    }
    bool lava;
    base_at(e, 516);
    if (!mc_tick_effect(e,
                        mc_entity_environment_lava(entity, classes,
                                                   limits == NULL ? NULL : &limits->material, &lava,
                                                   &e->environment),
                        "Tick actual lava virtual", 516))
        return false;
    if (lava) {
        base_at(e, 523);
        if (!lava_damage(entity, classes, e))
            return false;
    }
    base_at(e, 533);
    if (!mc_tick_fp(e, 533))
        return false;
    if (entity->base.y < -64.0) {
        base_at(e, 538);
        if (!void_damage(entity, classes, e))
            return false;
    }
    base_at(e, 545);
    if (!remote(entity, &is_remote, e, 545))
        return false;
    if (!is_remote) {
        base_at(e, 565);
        if (!mc_tick_flag(entity, 0, entity->base.bv > 0, e))
            return false;
        base_at(e, 582);
        if (!mc_tick_flag(entity, 2, entity->base.aH != NULL, e))
            return false;
    }
    entity->base.private_c = false;
    base_at(e, 590);
    return true;
}

static bool drowning_particles(mc_entity *entity, mc_registry_classes *classes,
                               mc_entity_tick_error *e) {
    for (int index = 0; index < 8; ++index) {
        living_at(e, 152);
        if (!work(e, 152))
            return false;
        float offsets[3];
        static const unsigned int pcs[] = {152, 159, 168, 175, 184, 191};
        for (size_t i = 0; i < 3; ++i) {
            float first, second;
            if (!next_float(entity, &first, e, pcs[2 * i]) ||
                !next_float(entity, &second, e, pcs[2 * i + 1]))
                return false;
            if (i == 0) {
                living_at(e, 162);
                if (!mc_tick_fp(e, 162))
                    return false;
            }
            offsets[i] = mc_tick_fsub(first, second);
        }
        mc_world_runtime *world = entity->base.world;
        const mc_registry_string *name;
        if (!literal(entity, classes, "bubble", &name, e, 201))
            return false;
        const double x = mc_tick_dadd(entity->base.x, mc_tick_widen(offsets[0]));
        const double y = mc_tick_dadd(entity->base.y, mc_tick_widen(offsets[1]));
        const double z = mc_tick_dadd(entity->base.z, mc_tick_widen(offsets[2]));
        const double vx = entity->base.velocity_x, vy = entity->base.velocity_y,
                     vz = entity->base.velocity_z;
        living_at(e, 237);
        if (!particle(world, name, x, y, z, vx, vy, vz, e, 237))
            return false;
    }
    return true;
}

static bool death_particles(mc_entity *entity, mc_registry_classes *classes,
                            mc_entity_tick_error *e) {
    for (int index = 0; index < 20; ++index) {
        living_at(e, 374);
        if (!work(e, 374) || !mc_tick_fp(e, 374))
            return false;
        double velocity[3];
        static const unsigned int pcs[] = {374, 386, 399};
        for (size_t i = 0; i < 3; ++i) {
            if (entity->random == NULL)
                return mc_tick_fail(e, MC_ENTITY_STATE_ERROR, "Death null Entity Random", pcs[i]);
            velocity[i] = mc_tick_dmul(mc_java_random_next_gaussian(entity->random), .02);
        }
        mc_world_runtime *world = entity->base.world;
        const mc_registry_string *name;
        if (!literal(entity, classes, "explode", &name, e, 412))
            return false;
        const double old_x = entity->base.x;
        float draw;
        if (!next_float(entity, &draw, e, 422))
            return false;
        const double x = mc_tick_dsub(
            mc_tick_dadd(old_x,
                         mc_tick_widen(mc_tick_fmul(mc_tick_fmul(draw, entity->base.width), 2.0f))),
            mc_tick_widen(entity->base.width));
        const double old_y = entity->base.y;
        if (!next_float(entity, &draw, e, 448))
            return false;
        const double y =
            mc_tick_dadd(old_y, mc_tick_widen(mc_tick_fmul(draw, entity->base.height)));
        const double old_z = entity->base.z;
        if (!next_float(entity, &draw, e, 466))
            return false;
        const double z = mc_tick_dsub(
            mc_tick_dadd(old_z,
                         mc_tick_widen(mc_tick_fmul(mc_tick_fmul(draw, entity->base.width), 2.0f))),
            mc_tick_widen(entity->base.width));
        living_at(e, 489);
        if (!particle(world, name, x, y, z, velocity[0], velocity[1], velocity[2], e, 489))
            return false;
    }
    return true;
}

static bool living_body(mc_entity *entity, mc_registry_classes *classes,
                        const mc_entity_tick_limits *limits, mc_entity_tick_error *e) {
    mc_living_entity_fields *fields = &entity->local_player->fields.living;
    fields->W = fields->X;
    living_at(e, 9);
    if (!base_body(entity, classes, limits, e))
        return false;
    living_at(e, 19);
    if (entity->random == NULL)
        return mc_tick_fail(e, MC_ENTITY_STATE_ERROR, "Living tick null ambient Random", 19);
    int32_t draw;
    if (!mc_java_random_next_bounded(entity->random, 1000, &draw))
        return mc_tick_fail(e, MC_ENTITY_STATE_ERROR, "Living tick actual Random.nextInt1000", 19);
    const int32_t old = fields->private_a;
    fields->private_a = mc_java_i32_add(old, 1);
    if (draw < old) {
        fields->private_a = -80; /* Actual ls.e() literal80 and Java ineg. */
        e->ambient_bytecode = 1; /* ls.T -> actual inherited g() null. */
        /* ls.g() is a real null-return body on dc; T returns42 before World,
         * sound arguments or any additional Random.nextFloat call. */
    }
    if (!entity->base.be && fields->Y > 0) {
        bool suffocating;
        living_at(e, 57);
        if (!mc_tick_suffocating(entity, &suffocating, e))
            return false;
        if (suffocating) {
            living_at(e, 66);
            if (!damage(entity, classes, 1, e, 66))
                return false;
        }
    }
    living_at(e, 71);
    if (entity->base.bC)
        entity->base.bv = 0;
    else {
        bool is_remote;
        living_at(e, 81);
        if (!remote(entity, &is_remote, e, 81))
            return false;
        if (is_remote)
            entity->base.bv = 0;
    }
    bool submerged = false;
    if (!entity->base.be && fields->Y > 0) {
        mc_material *water = NULL;
        living_at(e, 100);
        if (!mc_tick_code(entity, classes, e, 100) ||
            !mc_tick_registry(
                e, mc_registry_material_get(classes, MC_MATERIAL_g, &water, &e->registry),
                "Living tick actual Material.g", 100))
            return false;
        living_at(e, 103);
        if (!mc_tick_effect(e, mc_entity_inside_material(entity, water, &submerged, &e->material),
                            "Living tick actual eye immersion", 103))
            return false;
    }
    /* The actual dc inherits ls.d_(), literal false. */
    if (submerged) {
        entity->base.bz = mc_java_i32_sub(entity->base.bz, 1);
        if (entity->base.bz == -20) {
            entity->base.bz = 0;
            if (!drowning_particles(entity, classes, e))
                return false;
            living_at(e, 249);
            if (!damage(entity, classes, 2, e, 249))
                return false;
        }
        entity->base.bv = 0;
    } else
        entity->base.bz = entity->base.bw;
    fields->af = fields->ag;
    if (fields->ae > 0)
        --fields->ae;
    if (fields->aa > 0)
        --fields->aa;
    if (entity->base.by > 0)
        --entity->base.by;
    if (fields->Y <= 0) {
        fields->ad = mc_java_i32_add(fields->ad, 1);
        if (fields->ad > 20) {
            living_at(e, 355); /* Actual inherited ls.aa() literal return0. */
            living_at(e, 359);
            if (!mc_tick_effect(e, mc_entity_effect_dead(entity, &e->effect),
                                "Living tick actual Player K close", 359))
                return false;
            if (!death_particles(entity, classes, e))
                return false;
        }
    }
    fields->M = fields->L;
    fields->I = fields->H;
    entity->base.aU = entity->base.yaw;
    entity->base.aV = entity->base.aT;
    living_at(e, 530);
    return true;
}

static mc_entity_status run(mc_entity *entity, mc_registry_classes *classes,
                            const mc_entity_tick_limits *limits, bool living,
                            mc_entity_tick_error *out) {
    mc_entity_tick_error frame;
    mc_entity_tick_error_init(&frame);
    frame.max_particle_work =
        limits == NULL ? MC_ENTITY_TICK_DEFAULT_PARTICLE_WORK : limits->max_particle_work;
    if (mc_tick_enter(entity, &frame)) {
        if (living)
            (void)living_body(entity, classes, limits, &frame);
        else
            (void)base_body(entity, classes, limits, &frame);
        mc_entity_release(entity);
    }
    return mc_tick_finish(&frame, out);
}

mc_entity_status mc_entity_tick_update_in_context(mc_entity *entity, mc_registry_classes *classes,
                                                  const mc_entity_tick_limits *limits,
                                                  mc_entity_tick_error *out) {
    return run(entity, classes, limits, true, out);
}

mc_entity_status mc_entity_tick_base_body_in_context(mc_entity *entity,
                                                     mc_registry_classes *classes,
                                                     const mc_entity_tick_limits *limits,
                                                     mc_entity_tick_error *out) {
    return run(entity, classes, limits, false, out);
}

/* Private selected hl caller already owns the real Item and its native hold.
 * hl inherits U from sn; it must not run the separate Living body. */
bool mc_tick_item_base_body(mc_entity *entity, mc_registry_classes *classes,
                            const mc_entity_tick_limits *limits, mc_entity_tick_error *error) {
    error->max_particle_work =
        limits == NULL ? MC_ENTITY_TICK_DEFAULT_PARTICLE_WORK : limits->max_particle_work;
    return base_body(entity, classes, limits, error);
}
