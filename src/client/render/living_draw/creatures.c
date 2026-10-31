#include "internal.h"
#include "mc/core/java/java.h"

static const uint16_t eyes_units[] = {'/', 'm', 'o', 'b', '/', 's', 'p', 'i', 'd', 'e',
                                      'r', '_', 'e', 'y', 'e', 's', '.', 'p', 'n', 'g'};
static const mc_player_string eyes = {SIZE_MAX, sizeof(eyes_units) / sizeof(*eyes_units),
                                      eyes_units};

static bool bridge(ld_frame *f, const char *renderer, const char *descriptor,
                   mc_entity_class_kind kind) {
    ld_at(f, renderer, "a", descriptor, 2);
    return ld_cast_actor(f, kind, "Creature renderer actual bridge checkcast");
}

static bool spider_pass(ld_frame *f, int32_t pass, bool *out) {
    mc_client_living_draw_error *e = f->error;
    if (!bridge(f, "yx", "(Lls;IF)Z", MC_ENTITY_CLASS_KIND_SPIDER))
        return false;
    ld_at(f, "yx", "a", "(Lcn;IF)Z", 1);
    if (pass != 0) {
        *out = false;
        ld_at(f, "yx", "a", "(Lcn;IF)Z", 5);
        return true;
    }
    /* The second source pass guard7 also falls through to the lookup. */
    ld_at(f, "yx", "a", "(Lcn;IF)Z", 15);
    if (!ld_texture(f, &eyes, 15))
        return false;
    ld_at(f, "yx", "a", "(Lcn;IF)Z", 21);
    if (f->actor == NULL)
        return ld_null(f, "Spider eyes null brightness invoke");
    float brightness;
    if (!ld_brightness(f, 1.0f, &brightness))
        return false;
    e->state.eyes_brightness = brightness;
    e->state.eyes_brightness_returned = true;
    if (!ld_fp(f))
        return false;
    const float alpha = at_fmul(at_fsub(1.0f, brightness), .5f);
    e->state.eyes_alpha = alpha;
    ld_at(f, "yx", "a", "(Lcn;IF)Z", 33);
    if (!ld_display(f, mc_client_display_source_blend_enable(f->scope->model.display, true,
                                                             &e->detail.display)))
        return false;
    ld_at(f, "yx", "a", "(Lcn;IF)Z", 39);
    if (!ld_display(f, mc_client_display_source_alpha_test(f->scope->model.display, false,
                                                           &e->detail.display)))
        return false;
    ld_at(f, "yx", "a", "(Lcn;IF)Z", 48);
    if (!ld_display(f, mc_client_display_source_blend_function(f->scope->model.display, 770, 771,
                                                               &e->detail.display)))
        return false;
    ld_at(f, "yx", "a", "(Lcn;IF)Z", 56);
    if (!ld_display(f, mc_client_display_source_color(f->scope->model.display, 1.0f, 1.0f, 1.0f,
                                                      alpha, &e->detail.display)))
        return false;
    *out = true;
    ld_at(f, "yx", "a", "(Lcn;IF)Z", 60);
    return true;
}

static bool slime_pass(ld_frame *f, int32_t pass, bool *out) {
    mc_client_living_draw_error *e = f->error;
    if (!bridge(f, "mj", "(Lls;IF)Z", MC_ENTITY_CLASS_KIND_SLIME))
        return false;
    ld_at(f, "mj", "a", "(Luw;IF)Z", 1);
    if (pass == 0) {
        manager_model *outer = f->renderer->own_model; /* MJ.a read6. */
        ld_at(f, "gv", "a", "(Lko;)V", 2);
        f->renderer->secondary = outer;
        e->state.slime_secondary_stored = true;
        e->state.secondary_stored_identity = (uintptr_t)outer;
        ld_at(f, "mj", "a", "(Luw;IF)Z", 15);
        if (!ld_display(f, mc_client_display_source_normalize(f->scope->model.display, true,
                                                              &e->detail.display)))
            return false;
        e->state.slime_normalize_enabled = true;
        ld_at(f, "mj", "a", "(Luw;IF)Z", 21);
        if (!ld_display(f, mc_client_display_source_blend_enable(f->scope->model.display, true,
                                                                 &e->detail.display)))
            return false;
        ld_at(f, "mj", "a", "(Luw;IF)Z", 30);
        if (!ld_display(f, mc_client_display_source_blend_function(f->scope->model.display, 770,
                                                                   771, &e->detail.display)))
            return false;
        *out = true;
        ld_at(f, "mj", "a", "(Luw;IF)Z", 34);
        return true;
    }
    if (pass == 1) {
        ld_at(f, "mj", "a", "(Luw;IF)Z", 43);
        if (!ld_display(f, mc_client_display_source_blend_enable(f->scope->model.display, false,
                                                                 &e->detail.display)))
            return false;
        ld_at(f, "mj", "a", "(Luw;IF)Z", 50);
        if (!ld_display(f, mc_client_display_source_color(f->scope->model.display, 1.0f, 1.0f, 1.0f,
                                                          1.0f, &e->detail.display)))
            return false;
    }
    *out = false;
    ld_at(f, "mj", "a", "(Luw;IF)Z", 54);
    return true;
}

static bool creeper_pass(ld_frame *f, int32_t pass, float partial, bool *out) {
    mc_client_living_draw_error *e = f->error;
    if (!bridge(f, "m", "(Lls;IF)Z", MC_ENTITY_CLASS_KIND_CREEPER))
        return false;
    ld_at(f, "m", "a", "(Lgb;IF)Z", 1);
    if (f->actor == NULL)
        return ld_null(f, "Creeper pass null powered invoke");
    bool powered;
    if (!ld_entity(f, mc_entity_render_creeper_powered(f->actor, &powered, &e->detail.entity)))
        return false;
    if (pass >= 0 && pass < 4) {
        e->state.powered_returned[(size_t)pass] = true;
        e->state.powered[(size_t)pass] = powered;
    }
    if (powered && pass == 1) {
        /* Source reads/converts bt13/16 and adds partial18 before its first
         * unimplemented powered texture child. No field is changed. */
        const int32_t ticks = f->actor->base.bt;
        ld_at(f, "m", "a", "(Lgb;IF)Z", 16);
        if (!ld_fp(f))
            return false;
        e->state.creeper_powered_age = at_fadd((float)ticks, partial);
        e->state.creeper_powered_age_read = true;
        ld_at(f, "m", "a", "(Lgb;IF)Z", 24);
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_UNIMPLEMENTED,
                       "Creeper actual powered texture pass child");
    }
    if (powered && pass == 2) {
        ld_at(f, "m", "a", "(Lgb;IF)Z", 113);
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_UNIMPLEMENTED,
                       "Creeper actual powered texture-matrix child");
    }
    *out = false;
    ld_at(f, "m", "a", "(Lgb;IF)Z", 138);
    return true;
}

bool ld_creature_pass(ld_frame *f, int32_t pass, float partial, bool *out) {
    mc_client_living_draw_error *e = f->error;
    const mc_client_graphics_site parent = e->parent;
    e->parent = e->site; /* Actual GV.a315 or inherited overlay GV.b4 invocation. */
    bool ok;
    if (strcmp(f->renderer->source_class, "yx") == 0)
        ok = spider_pass(f, pass, out);
    else if (strcmp(f->renderer->source_class, "mj") == 0)
        ok = slime_pass(f, pass, out);
    else
        ok = creeper_pass(f, pass, partial, out);
    if (ok)
        e->parent = parent;
    return ok;
}

bool ld_squid_age(ld_frame *f, float partial, float *out) {
    mc_client_living_draw_error *e = f->error;
    const mc_client_graphics_site parent = e->parent;
    e->parent = ld_site("gv", "a", "(Lls;DDDFF)V", 147);
    ld_at(f, "es", "c", "(Lls;F)F", 2);
    if (!ld_cast_actor(f, MC_ENTITY_CLASS_KIND_SQUID, "Squid age actual XT checkcast"))
        return false;
    ld_at(f, "es", "b", "(Lxt;F)F", 1);
    if (f->actor == NULL)
        return ld_null(f, "Squid age null first field receiver");
    if (!mc_entity_has_mob(f->actor))
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_STATE, "Squid actual concrete fields");
    const mc_mob_squid_fields *v = &f->actor->mob->squid;
    const float previous = v->j;
    const float current = v->i;
    const float previous_again = v->j;
    ld_at(f, "es", "b", "(Lxt;F)F", 12);
    if (!ld_fp(f))
        return false;
    *out = at_fadd(previous, at_fmul(at_fsub(current, previous_again), partial));
    ld_at(f, "es", "b", "(Lxt;F)F", 18);
    e->parent = parent;
    return true;
}

static bool squid_rotation(ld_frame *f, float body, float partial) {
    mc_client_living_draw_error *e = f->error;
    if (!bridge(f, "es", "(Lls;FFF)V", MC_ENTITY_CLASS_KIND_SQUID))
        return false;
    ld_at(f, "es", "a", "(Lxt;FFF)V", 1);
    if (f->actor == NULL)
        return ld_null(f, "Squid rotation null first field receiver");
    if (!mc_entity_has_mob(f->actor))
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_STATE, "Squid actual rotation fields");
    const mc_mob_squid_fields *v = &f->actor->mob->squid;
    const float previous_pitch = v->b;
    const float current_pitch = v->a;
    const float previous_pitch_again = v->b;
    ld_at(f, "es", "a", "(Lxt;FFF)V", 12);
    if (!ld_fp(f))
        return false;
    const float pitch =
        at_fadd(previous_pitch, at_fmul(at_fsub(current_pitch, previous_pitch_again), partial));
    const float previous_yaw = v->f;
    const float current_yaw = v->c;
    const float previous_yaw_again = v->f;
    const float yaw =
        at_fadd(previous_yaw, at_fmul(at_fsub(current_yaw, previous_yaw_again), partial));
    e->state.squid_pitch = pitch;
    e->state.squid_yaw = yaw;
    ld_at(f, "es", "a", "(Lxt;FFF)V", 42);
    if (!ld_display(f, mc_client_display_source_translate(f->scope->model.display, 0.0f, .5f, 0.0f,
                                                          &e->detail.display)))
        return false;
    ld_at(f, "es", "a", "(Lxt;FFF)V", 52);
    if (!ld_display(f,
                    mc_client_display_source_rotate(f->scope->model.display, at_fsub(180.0f, body),
                                                    0.0f, 1.0f, 0.0f, &e->detail.display)))
        return false;
    ld_at(f, "es", "a", "(Lxt;FFF)V", 60);
    if (!ld_display(f, mc_client_display_source_rotate(f->scope->model.display, pitch, 1.0f, 0.0f,
                                                       0.0f, &e->detail.display)))
        return false;
    ld_at(f, "es", "a", "(Lxt;FFF)V", 68);
    if (!ld_display(f, mc_client_display_source_rotate(f->scope->model.display, yaw, 0.0f, 1.0f,
                                                       0.0f, &e->detail.display)))
        return false;
    ld_at(f, "es", "a", "(Lxt;FFF)V", 75);
    if (!ld_display(f, mc_client_display_source_translate(f->scope->model.display, 0.0f, -1.2f,
                                                          0.0f, &e->detail.display)))
        return false;
    ld_at(f, "es", "a", "(Lxt;FFF)V", 78);
    return true;
}

bool ld_rotation(ld_frame *f, float age, float body, float partial) {
    (void)age;
    mc_client_living_draw_error *e = f->error;
    const mc_client_graphics_site parent = e->parent;
    e->parent = ld_site("gv", "a", "(Lls;DDDFF)V", 160);
    if (strcmp(f->renderer->source_class, "es") == 0) {
        if (!squid_rotation(f, body, partial))
            return false;
    } else {
        ld_at(f, "gv", "a", "(Lls;FFF)V", 7);
        if (!ld_fp(f) || !ld_display(f, mc_client_display_source_rotate(
                                            f->scope->model.display, at_fsub(180.0f, body), 0.0f,
                                            1.0f, 0.0f, &e->detail.display)))
            return false;
        ld_at(f, "gv", "a", "(Lls;FFF)V", 14);
        if (f->actor == NULL)
            return ld_null(f, "Living rotation null death field receiver");
        mc_living_entity_fields *ls = mc_entity_living_fields(f->actor);
        if (ls == NULL)
            return ld_fail(f, MC_CLIENT_LIVING_DRAW_STATE, "Living actual LS rotation storage");
        if (ls->ad > 0) {
            float value =
                at_fmul(at_fdiv(at_fsub(at_fadd((float)ls->ad, partial), 1.0f), 20.0f), 1.6f);
            ld_at(f, "gv", "a", "(Lls;FFF)V", 37);
            if (!ld_float_math(f, "c", value, &value))
                return false;
            if (value > 1.0f)
                value = 1.0f;
            float limit = 90.0f;
            if (strcmp(f->renderer->source_class, "yx") == 0) {
                /* Actual YX.a(LS) bridge2 invokes the constant CN leaf5. */
                e->parent = ld_site("gv", "a", "(Lls;FFF)V", 56);
                ld_at(f, "yx", "a", "(Lls;)F", 2);
                if (!ld_cast_actor(f, MC_ENTITY_CLASS_KIND_SPIDER,
                                   "Spider death-roll actual CN checkcast"))
                    return false;
                limit = 180.0f;
                e->parent = ld_site("gv", "a", "(Lls;DDDFF)V", 160);
            }
            ld_at(f, "gv", "a", "(Lls;FFF)V", 63);
            if (!ld_display(f, mc_client_display_source_rotate(f->scope->model.display,
                                                               at_fmul(value, limit), 0.0f, 0.0f,
                                                               1.0f, &e->detail.display)))
                return false;
        }
    }
    e->state.rotation_returned = true;
    e->parent = parent;
    return true;
}

static bool slime_prescale(ld_frame *f, float partial) {
    mc_client_living_draw_error *e = f->error;
    if (!bridge(f, "mj", "(Lls;F)V", MC_ENTITY_CLASS_KIND_SLIME))
        return false;
    ld_at(f, "mj", "a", "(Luw;F)V", 1);
    if (f->actor == NULL)
        return ld_null(f, "Slime pre-scale null size invoke");
    int32_t size;
    if (!ld_entity(f, mc_entity_render_slime_size(f->actor, &size, &e->detail.entity)))
        return false;
    e->state.slime_size = size;
    e->state.slime_size_returned = true;
    ld_at(f, "mj", "a", "(Luw;F)V", 6);
    if (!mc_entity_has_mob(f->actor))
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_STATE, "Slime actual squash fields");
    const float previous = f->actor->mob->slime_b;
    const float current = f->actor->mob->slime_a;
    const float previous_again = f->actor->mob->slime_b;
    ld_at(f, "mj", "a", "(Luw;F)V", 17);
    if (!ld_fp(f))
        return false;
    const float squash =
        at_fdiv(at_fadd(previous, at_fmul(at_fsub(current, previous_again), partial)),
                at_fadd(at_fmul((float)size, .5f), 1.0f));
    const float reciprocal = at_fdiv(1.0f, at_fadd(squash, 1.0f));
    const float scale = (float)size;
    e->state.slime_squash = squash;
    e->state.slime_reciprocal = reciprocal;
    e->state.prescale[0] = at_fmul(reciprocal, scale);
    e->state.prescale[1] = at_fmul(at_fdiv(1.0f, reciprocal), scale);
    e->state.prescale[2] = at_fmul(reciprocal, scale);
    ld_at(f, "mj", "a", "(Luw;F)V", 60);
    if (!ld_display(f, mc_client_display_source_scale(
                           f->scope->model.display, at_widen(e->state.prescale[0]),
                           at_widen(e->state.prescale[1]), at_widen(e->state.prescale[2]),
                           &e->detail.display)))
        return false;
    ld_at(f, "mj", "a", "(Luw;F)V", 63);
    return true;
}

static bool creeper_prescale(ld_frame *f, float partial) {
    mc_client_living_draw_error *e = f->error;
    if (!bridge(f, "m", "(Lls;F)V", MC_ENTITY_CLASS_KIND_CREEPER))
        return false;
    ld_at(f, "m", "a", "(Lgb;F)V", 4);
    if (f->actor == NULL)
        return ld_null(f, "Creeper pre-scale null swell invoke");
    float value;
    if (!ld_entity(f, mc_entity_render_creeper_swell(f->actor, partial, &value, &e->detail.entity)))
        return false;
    e->state.creeper_prescale_swell = value;
    e->state.creeper_prescale_swell_returned = true;
    if (!ld_fp(f))
        return false;
    const float argument = at_fmul(value, 100.0f);
    ld_at(f, "m", "a", "(Lgb;F)V", 15);
    float sine;
    if (!ld_float_math(f, "a", argument, &sine))
        return false;
    e->state.creeper_sine = sine;
    e->state.creeper_sine_returned = true;
    const float wave = at_fadd(1.0f, at_fmul(at_fmul(sine, value), .01f));
    if (value < 0.0f)
        value = 0.0f;
    if (value > 1.0f)
        value = 1.0f;
    value = at_fmul(value, value);
    value = at_fmul(value, value);
    const float xz = at_fmul(at_fadd(1.0f, at_fmul(value, .4f)), wave);
    const float y = at_fdiv(at_fadd(1.0f, at_fmul(value, .1f)), wave);
    e->state.prescale[0] = xz;
    e->state.prescale[1] = y;
    e->state.prescale[2] = xz;
    ld_at(f, "m", "a", "(Lgb;F)V", 91);
    if (!ld_display(f,
                    mc_client_display_source_scale(f->scope->model.display, at_widen(xz),
                                                   at_widen(y), at_widen(xz), &e->detail.display)))
        return false;
    ld_at(f, "m", "a", "(Lgb;F)V", 94);
    return true;
}

bool ld_prescale(ld_frame *f, float partial) {
    mc_client_living_draw_error *e = f->error;
    const mc_client_graphics_site parent = e->parent;
    e->parent = ld_site("gv", "a", "(Lls;DDDFF)V", 184);
    const char *name = f->renderer->source_class;
    if (strcmp(name, "mj") == 0) {
        if (!slime_prescale(f, partial))
            return false;
    } else if (strcmp(name, "m") == 0) {
        if (!creeper_prescale(f, partial))
            return false;
    } else if (strcmp(name, "we") == 0) {
        if (!ld_wolf_prescale(f, partial))
            return false;
    } else if (strcmp(name, "es") == 0) {
        if (!bridge(f, "es", "(Lls;F)V", MC_ENTITY_CLASS_KIND_SQUID))
            return false;
        ld_at(f, "es", "a", "(Lxt;F)V", 0); /* Actual empty leaf. */
    } else {
        ld_at(f, "gv", "a", "(Lls;F)V", 0); /* Actual empty inherited leaf. */
    }
    e->state.prescale_returned = true;
    e->parent = parent;
    return true;
}

bool ld_color(ld_frame *f, float brightness, float partial, int32_t *out) {
    (void)brightness;
    mc_client_living_draw_error *e = f->error;
    const mc_client_graphics_site parent = e->parent;
    e->parent = ld_site("gv", "a", "(Lls;DDDFF)V", 382);
    if (strcmp(f->renderer->source_class, "m") != 0) {
        ld_at(f, "gv", "a", "(Lls;FF)I", 1);
        *out = 0;
    } else {
        if (!bridge(f, "m", "(Lls;FF)I", MC_ENTITY_CLASS_KIND_CREEPER))
            return false;
        ld_at(f, "m", "a", "(Lgb;FF)I", 6);
        if (f->actor == NULL)
            return ld_null(f, "Creeper color null swell invoke");
        float value;
        if (!ld_entity(
                f, mc_entity_render_creeper_swell(f->actor, partial, &value, &e->detail.entity)))
            return false;
        e->state.creeper_color_swell = value;
        e->state.creeper_color_swell_returned = true;
        if (!ld_fp(f))
            return false;
        const int32_t interval = mc_java_float_to_i32(at_fmul(value, 10.0f));
        if (interval % 2 == 0) {
            *out = 0;
            ld_at(f, "m", "a", "(Lgb;FF)I", 23);
        } else {
            int32_t alpha = mc_java_float_to_i32(at_fmul(at_fmul(value, .2f), 255.0f));
            if (alpha < 0)
                alpha = 0;
            if (alpha > 255)
                alpha = 255;
            *out = mc_java_i32_shl(alpha, 24) | mc_java_i32_shl(255, 16) | mc_java_i32_shl(255, 8) |
                   255;
            ld_at(f, "m", "a", "(Lgb;FF)I", 91);
        }
    }
    e->state.color = *out;
    e->state.color_returned = true;
    e->parent = parent;
    return true;
}
