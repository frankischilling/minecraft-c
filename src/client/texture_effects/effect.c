#include "../application_access.h"
#include "effect_internal.h"

const char *mc_texture_effect_class(mc_texture_effect_kind kind) {
    static const char *const names[] = {"vs", "cg", "hs", "av", "ku", "oh", "if", "sd"};
    return (unsigned)kind < sizeof(names) / sizeof(*names) ? names[kind] : NULL;
}

mc_texture_effect_status mc_texture_effect_fail(mc_texture_effect_error *e,
                                                mc_texture_effect_status status,
                                                const char *operation, const char *source,
                                                const char *method, int32_t pc) {
    e->status = status;
    e->operation = operation;
    e->source_class = source;
    e->source_method = method;
    e->source_pc = pc;
    return status;
}

void *mc_texture_effect_array(mc_texture_effect_owner *o, size_t bytes, int32_t pc,
                              mc_texture_effect_error *e) {
    if (o->array_bytes > o->options.max_array_bytes ||
        bytes > o->options.max_array_bytes - o->array_bytes) {
        mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_LIMIT, "effect native array quota",
                               mc_texture_effect_class(o->base.kind), "<init>", pc);
        return NULL;
    }
    void *p = calloc(1, bytes);
    if (p == NULL) {
        mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_ALLOCATION_FAILED,
                               "effect actual array storage", mc_texture_effect_class(o->base.kind),
                               "<init>", pc);
        return NULL;
    }
    o->array_bytes += bytes;
    return p;
}

bool mc_texture_effect_fp(mc_texture_effect_error *e, const char *source, int32_t pc) {
    if ((_mm_getcsr() & 0xffc0u) == 0x1f80u)
        return true;
    mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_FP_UNSUPPORTED,
                           "effect reached strict FP prerequisite", source, e->source_method, pc);
    return false;
}

bool mc_texture_effect_random(mc_texture_effect_owner *o, int32_t pc, double *out,
                              mc_texture_effect_error *e) {
    e->random = mc_java_math_random(o->services.profile, out);
    if (e->random == MC_JAVA_RANDOM_PROFILE_OK)
        return true;
    mc_texture_effect_fail(e, MC_TEXTURE_EFFECT_RANDOM_FAILED, "effect shared actual Math.random",
                           mc_texture_effect_class(o->base.kind), "a", pc);
    return false;
}

bool mc_texture_effect_load(mc_texture_effect_owner *o, const float *p, size_t length, size_t index,
                            int32_t pc, float *out, mc_texture_effect_error *e) {
    if (p == NULL || index >= length) {
        mc_texture_effect_fail(
            e, p == NULL ? MC_TEXTURE_EFFECT_NULL_REFERENCE : MC_TEXTURE_EFFECT_INDEX_OUT_OF_RANGE,
            "effect actual faload", mc_texture_effect_class(o->base.kind), "a", pc);
        return false;
    }
    *out = p[index];
    return true;
}

bool mc_texture_effect_store(mc_texture_effect_owner *o, float *p, size_t length, size_t index,
                             float value, int32_t pc, mc_texture_effect_error *e) {
    if (p == NULL || index >= length) {
        mc_texture_effect_fail(
            e, p == NULL ? MC_TEXTURE_EFFECT_NULL_REFERENCE : MC_TEXTURE_EFFECT_INDEX_OUT_OF_RANGE,
            "effect actual fastore", mc_texture_effect_class(o->base.kind), "a", pc);
        return false;
    }
    p[index] = value;
    o->last_store_pc = pc;
    return true;
}

bool mc_texture_effect_pixel(mc_texture_effect_owner *o, size_t index, int32_t red, int32_t green,
                             int32_t blue, int32_t alpha, const int32_t pcs[4],
                             mc_texture_effect_error *e) {
    if (o->fields.c) {
        const int32_t r = (red * 30 + green * 59 + blue * 11) / 100;
        const int32_t g = (red * 30 + green * 70) / 100;
        const int32_t b = (red * 30 + blue * 70) / 100;
        red = r;
        green = g;
        blue = b;
    }
    const int32_t components[] = {red, green, blue, alpha};
    for (size_t n = 0; n < 4; ++n) {
        const size_t at = index * 4 + n;
        if (o->fields.a == NULL || at >= o->fields.a_length) {
            mc_texture_effect_fail(e,
                                   o->fields.a == NULL ? MC_TEXTURE_EFFECT_NULL_REFERENCE
                                                       : MC_TEXTURE_EFFECT_INDEX_OUT_OF_RANGE,
                                   "effect actual bastore", mc_texture_effect_class(o->base.kind),
                                   "a", pcs[n]);
            return false;
        }
        ((uint8_t *)o->fields.a)[at] = (uint8_t)(uint32_t)components[n];
        o->last_store_pc = pcs[n];
    }
    return true;
}

mc_texture_effect_status
mc_client_texture_effect_allocate(const mc_texture_effect_services *services,
                                  mc_texture_effect_kind kind,
                                  const mc_texture_effect_options *options,
                                  mc_client_texture_effect **out, mc_texture_effect_error *error) {
    mc_texture_effect_error e = {.source_pc = -1, .enclosing_pc = -1, .cause_pc = -1};
    if (services == NULL || services->registry == NULL || services->profile == NULL ||
        out == NULL || kind < MC_TEXTURE_EFFECT_PORTAL || kind > MC_TEXTURE_EFFECT_FIRE) {
        mc_texture_effect_fail(&e, MC_TEXTURE_EFFECT_INVALID_ARGUMENT,
                               "effect cold allocation inputs", NULL, NULL, -1);
        goto done;
    }
    mc_texture_effect_owner *o = calloc(1, sizeof(*o));
    if (o == NULL) {
        mc_texture_effect_fail(&e, MC_TEXTURE_EFFECT_ALLOCATION_FAILED, "effect native owner", NULL,
                               NULL, -1);
        goto done;
    }
    if (mc_registry_classes_retain(services->registry, MC_REGISTRY_CLIENT, services->profile,
                                   &e.registry) != MC_REGISTRY_OK) {
        free(o);
        mc_texture_effect_fail(&e, MC_TEXTURE_EFFECT_REGISTRY_FAILED, "effect cold Registry hold",
                               NULL, NULL, -1);
        goto done;
    }
    o->base.kind = kind;
    o->base.holds = 1;
    o->services = *services;
    if (options != NULL)
        o->options = *options;
    if (o->options.max_array_bytes == 0)
        o->options.max_array_bytes = 65536;
    if (o->options.max_encoded_bytes == 0)
        o->options.max_encoded_bytes = 16u * 1024u * 1024u;
    if (o->options.max_decoded_bytes == 0)
        o->options.max_decoded_bytes = 16u * 1024u * 1024u;
    if (o->options.max_inflater_bytes == 0)
        o->options.max_inflater_bytes = 256u * 1024u;
    if (o->options.max_angle_iterations == 0)
        o->options.max_angle_iterations = 1048576;
    o->last_store_pc = -1;
    o->failure = e;
    *out = &o->base;
done:
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_texture_effect_status mc_client_texture_effect_construct(mc_client_texture_effect *effect,
                                                            struct client_application *app,
                                                            int32_t argument,
                                                            mc_texture_effect_error *error) {
    mc_texture_effect_error e = {
        .source_pc = -1, .enclosing_pc = -1, .cause_pc = -1, .source_method = "<init>"};
    if (effect == NULL || effect->kind < MC_TEXTURE_EFFECT_PORTAL ||
        effect->kind > MC_TEXTURE_EFFECT_FIRE)
        mc_texture_effect_fail(&e, MC_TEXTURE_EFFECT_INVALID_ARGUMENT, "effect actual new subtype",
                               NULL, NULL, -1);
    else {
        mc_texture_effect_owner *o = (mc_texture_effect_owner *)effect;
        if (effect->active || o->phase == MC_TEXTURE_EFFECT_CONSTRUCTING)
            mc_texture_effect_fail(&e, MC_TEXTURE_EFFECT_BUSY, "effect construction entered", NULL,
                                   NULL, -1);
        else if (o->phase != MC_TEXTURE_EFFECT_RAW)
            mc_texture_effect_fail(&e, MC_TEXTURE_EFFECT_INVALID_PHASE,
                                   "effect one-shot constructor", NULL, NULL, -1);
        else {
            effect->active = true;
            o->phase = MC_TEXTURE_EFFECT_CONSTRUCTING;
            e.source_descriptor =
                effect->kind == MC_TEXTURE_EFFECT_COMPASS || effect->kind == MC_TEXTURE_EFFECT_WATCH
                    ? "(Lnet/minecraft/client/Minecraft;)V"
                : effect->kind == MC_TEXTURE_EFFECT_FIRE ? "(I)V"
                                                         : "()V";
            const bool ok = mc_texture_effect_constructor(o, app, argument, &e);
            o->phase = ok ? MC_TEXTURE_EFFECT_READY : MC_TEXTURE_EFFECT_FAILED;
            effect->active = false;
            o->failure = e;
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_texture_effect_status mc_client_texture_effect_tick(mc_client_texture_effect *effect,
                                                       mc_texture_effect_error *error) {
    mc_texture_effect_error e = {.source_pc = -1,
                                 .enclosing_pc = -1,
                                 .cause_pc = -1,
                                 .source_method = "a",
                                 .source_descriptor = "()V"};
    if (effect == NULL)
        mc_texture_effect_fail(&e, MC_TEXTURE_EFFECT_NULL_REFERENCE,
                               "actual aw.a null receiver before callee entry", NULL, NULL, -1);
    else if (effect->active)
        mc_texture_effect_fail(&e, MC_TEXTURE_EFFECT_BUSY, "effect native call entered", NULL, NULL,
                               -1);
    else {
        effect->active = true;
        if (effect->kind == MC_TEXTURE_EFFECT_WATER) {
            const mc_water_texture_status status =
                mc_client_water_texture_animate((mc_client_water_texture_effect *)effect, &e.water);
            if (status != MC_WATER_TEXTURE_OK)
                mc_texture_effect_fail(&e, MC_TEXTURE_EFFECT_CHILD_FAILED,
                                       "actual vs animation child", e.water.source_class, "a",
                                       e.water.source_pc);
        } else if (effect->kind == MC_TEXTURE_EFFECT_LAVA) {
            const mc_lava_texture_status status =
                mc_client_lava_texture_animate((mc_client_lava_texture_effect *)effect, &e.lava);
            if (status != MC_LAVA_TEXTURE_OK)
                mc_texture_effect_fail(&e, MC_TEXTURE_EFFECT_CHILD_FAILED,
                                       "actual cg animation child", e.lava.source_class, "a",
                                       e.lava.source_pc);
        } else {
            mc_texture_effect_owner *o = (mc_texture_effect_owner *)effect;
            if (o->phase != MC_TEXTURE_EFFECT_READY)
                mc_texture_effect_fail(&e, MC_TEXTURE_EFFECT_INVALID_PHASE,
                                       "effect subtype native READY admission", NULL, NULL, -1);
            else {
                switch (effect->kind) {
                    case MC_TEXTURE_EFFECT_PORTAL:
                        (void)mc_texture_portal_tick(o, &e);
                        break;
                    case MC_TEXTURE_EFFECT_COMPASS:
                        (void)mc_texture_compass_tick(o, &e);
                        break;
                    case MC_TEXTURE_EFFECT_WATCH:
                        (void)mc_texture_watch_tick(o, &e);
                        break;
                    case MC_TEXTURE_EFFECT_FLOWING_WATER:
                        (void)mc_texture_flowing_water_tick(o, &e);
                        break;
                    case MC_TEXTURE_EFFECT_FLOWING_LAVA:
                        (void)mc_texture_flowing_lava_tick(o, &e);
                        break;
                    case MC_TEXTURE_EFFECT_FIRE:
                        (void)mc_texture_fire_tick(o, &e);
                        break;
                    default:
                        mc_texture_effect_fail(&e, MC_TEXTURE_EFFECT_INVALID_ARGUMENT,
                                               "unknown actual subtype", NULL, NULL, -1);
                        break;
                }
            }
            o->failure = e;
        }
        effect->active = false;
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_texture_effect_status mc_client_texture_effect_retain(mc_client_texture_effect *effect,
                                                         mc_texture_effect_error *error) {
    mc_texture_effect_error e = {.source_pc = -1, .enclosing_pc = -1, .cause_pc = -1};
    if (effect == NULL)
        mc_texture_effect_fail(&e, MC_TEXTURE_EFFECT_INVALID_ARGUMENT, "effect hold receiver", NULL,
                               NULL, -1);
    else if (effect->active)
        mc_texture_effect_fail(&e, MC_TEXTURE_EFFECT_BUSY, "effect entered hold", NULL, NULL, -1);
    else if (effect->kind == MC_TEXTURE_EFFECT_WATER) {
        mc_water_texture_state state;
        if (!mc_client_water_texture_read((mc_client_water_texture_effect *)effect, &state) ||
            state.phase == MC_WATER_TEXTURE_CONSTRUCTING)
            mc_texture_effect_fail(&e, MC_TEXTURE_EFFECT_BUSY, "water constructing occurrence hold",
                                   NULL, NULL, -1);
        else if (effect->holds == SIZE_MAX)
            mc_texture_effect_fail(&e, MC_TEXTURE_EFFECT_LIMIT, "effect native hold overflow", NULL,
                                   NULL, -1);
        else
            ++effect->holds;
    } else if (effect->kind == MC_TEXTURE_EFFECT_LAVA) {
        mc_lava_texture_state state;
        if (!mc_client_lava_texture_read((mc_client_lava_texture_effect *)effect, &state) ||
            state.phase == MC_LAVA_TEXTURE_CONSTRUCTING)
            mc_texture_effect_fail(&e, MC_TEXTURE_EFFECT_BUSY, "lava constructing occurrence hold",
                                   NULL, NULL, -1);
        else if (effect->holds == SIZE_MAX)
            mc_texture_effect_fail(&e, MC_TEXTURE_EFFECT_LIMIT, "effect native hold overflow", NULL,
                                   NULL, -1);
        else
            ++effect->holds;
    } else if (((mc_texture_effect_owner *)effect)->phase == MC_TEXTURE_EFFECT_RETIRING)
        mc_texture_effect_fail(&e, MC_TEXTURE_EFFECT_INVALID_PHASE,
                               "effect retiring occurrence hold", NULL, NULL, -1);
    else if (effect->holds == SIZE_MAX)
        mc_texture_effect_fail(&e, MC_TEXTURE_EFFECT_LIMIT, "effect native hold overflow", NULL,
                               NULL, -1);
    else
        ++effect->holds;
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_texture_effect_status mc_client_texture_effect_release(mc_client_texture_effect **slot,
                                                          mc_texture_effect_error *error) {
    mc_texture_effect_error e = {.source_pc = -1, .enclosing_pc = -1, .cause_pc = -1};
    if (slot == NULL)
        mc_texture_effect_fail(&e, MC_TEXTURE_EFFECT_INVALID_ARGUMENT, "effect release slot", NULL,
                               NULL, -1);
    else if (*slot != NULL) {
        mc_client_texture_effect *effect = *slot;
        if (effect->active) {
            mc_texture_effect_fail(&e, MC_TEXTURE_EFFECT_BUSY, "effect active release", NULL, NULL,
                                   -1);
            goto done;
        }
        if (effect->kind == MC_TEXTURE_EFFECT_WATER) {
            mc_client_water_texture_effect *water = (mc_client_water_texture_effect *)effect;
            if (mc_client_water_texture_retire(&water, &e.water) == MC_WATER_TEXTURE_OK)
                *slot = NULL;
            else
                mc_texture_effect_fail(&e, MC_TEXTURE_EFFECT_BUSY, "water occurrence retirement",
                                       NULL, NULL, -1);
        } else if (effect->kind == MC_TEXTURE_EFFECT_LAVA) {
            mc_client_lava_texture_effect *lava = (mc_client_lava_texture_effect *)effect;
            if (mc_client_lava_texture_retire(&lava, &e.lava) == MC_LAVA_TEXTURE_OK)
                *slot = NULL;
            else
                mc_texture_effect_fail(&e, MC_TEXTURE_EFFECT_BUSY, "lava occurrence retirement",
                                       NULL, NULL, -1);
        } else if (effect->holds > 1) {
            --effect->holds;
            *slot = NULL;
        } else {
            mc_texture_effect_owner *o = (mc_texture_effect_owner *)effect;
            effect->active = true;
            o->phase = MC_TEXTURE_EFFECT_RETIRING;
            for (size_t n = 0; n < 2; ++n)
                if (mc_client_class_url_release(&o->urls[n], &e.image) != MC_CLIENT_CLASS_URL_OK) {
                    effect->active = false;
                    mc_texture_effect_fail(&e, MC_TEXTURE_EFFECT_NATIVE_CLOSE_FAILED,
                                           "effect pending URL retirement", NULL, NULL, -1);
                    o->retirement = e;
                    goto done;
                }
            if (mc_registry_classes_release(o->services.registry, &e.registry) != MC_REGISTRY_OK) {
                effect->active = false;
                mc_texture_effect_fail(&e, MC_TEXTURE_EFFECT_REGISTRY_FAILED,
                                       "effect Registry retirement", NULL, NULL, -1);
                o->retirement = e;
                goto done;
            }
            for (size_t n = 0; n < 2; ++n)
                mc_client_argb_image_release(o->images[n]);
            free((void *)o->fields.a);
            free(o->g);
            free(o->h);
            free(o->i);
            free(o->j);
            if (o->frames != NULL)
                for (size_t n = 0; n < o->frame_count; ++n)
                    free(o->frames[n]);
            if (o->pending_frames != NULL)
                for (size_t n = 0; n < o->pending_frame_count; ++n)
                    free(o->pending_frames[n]);
            free(o->pending_frames);
            free(o->random);
            free(o->frames);
            free(o->icon);
            free(o->dial);
            free(o->coordinate);
            /* Every fallible cleanup above has completed. A failed/busy
             * attempt retains both this bool and the actual App borrow. */
            mc_client_application_effect_release(o->app, o->app_borrow_counted);
            o->app_borrow_counted = false;
            free(o);
            *slot = NULL;
        }
    }
done:
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_texture_effect_status mc_client_texture_effect_set_anaglyph(mc_client_texture_effect *effect,
                                                               bool value,
                                                               mc_texture_effect_error *error) {
    mc_texture_effect_error e = {.source_pc = -1, .enclosing_pc = -1, .cause_pc = -1};
    if (effect == NULL)
        mc_texture_effect_fail(&e, MC_TEXTURE_EFFECT_NULL_REFERENCE, "aw.c field receiver", "aw",
                               NULL, -1);
    else if (effect->active)
        mc_texture_effect_fail(&e, MC_TEXTURE_EFFECT_BUSY, "effect entered c store", NULL, NULL,
                               -1);
    else if (effect->kind == MC_TEXTURE_EFFECT_WATER) {
        if (mc_client_water_texture_set_anaglyph((mc_client_water_texture_effect *)effect, value,
                                                 &e.water) != MC_WATER_TEXTURE_OK)
            mc_texture_effect_fail(&e, MC_TEXTURE_EFFECT_INVALID_PHASE, "actual water c store",
                                   NULL, NULL, -1);
    } else if (effect->kind == MC_TEXTURE_EFFECT_LAVA) {
        if (mc_client_lava_texture_set_anaglyph((mc_client_lava_texture_effect *)effect, value,
                                                &e.lava) != MC_LAVA_TEXTURE_OK)
            mc_texture_effect_fail(&e, MC_TEXTURE_EFFECT_INVALID_PHASE, "actual lava c store", NULL,
                                   NULL, -1);
    } else if (((mc_texture_effect_owner *)effect)->phase != MC_TEXTURE_EFFECT_READY)
        mc_texture_effect_fail(&e, MC_TEXTURE_EFFECT_INVALID_PHASE, "effect READY c store", NULL,
                               NULL, -1);
    else
        ((mc_texture_effect_owner *)effect)->fields.c = value;
    if (error != NULL)
        *error = e;
    return e.status;
}

bool mc_client_texture_effect_inspect(const mc_client_texture_effect *effect,
                                      mc_texture_effect_state *out) {
    if (effect == NULL || out == NULL)
        return false;
    *out = (mc_texture_effect_state){.kind = effect->kind, .native_holds = effect->holds};
    if (effect->kind == MC_TEXTURE_EFFECT_WATER) {
        mc_water_texture_state s;
        if (!mc_client_water_texture_read((const mc_client_water_texture_effect *)effect, &s))
            return false;
        out->phase = (mc_texture_effect_phase)s.phase;
        out->array_bytes = s.array_bytes;
        out->fields =
            (mc_texture_effect_fields){s.fields.a, s.fields.a_length, s.fields.b, s.fields.c,
                                       s.fields.d, s.fields.e,        s.fields.f};
        out->g = s.fields.g;
        out->h = s.fields.h;
        out->i = s.fields.i;
        out->j = s.fields.j;
        out->g_length = s.fields.g_length;
        out->h_length = s.fields.h_length;
        out->i_length = s.fields.i_length;
        out->j_length = s.fields.j_length;
        out->counter = s.fields.k;
    } else if (effect->kind == MC_TEXTURE_EFFECT_LAVA) {
        mc_lava_texture_state s;
        if (!mc_client_lava_texture_read((const mc_client_lava_texture_effect *)effect, &s))
            return false;
        out->phase = (mc_texture_effect_phase)s.phase;
        out->array_bytes = s.array_bytes;
        out->fields =
            (mc_texture_effect_fields){s.fields.a, s.fields.a_length, s.fields.b, s.fields.c,
                                       s.fields.d, s.fields.e,        s.fields.f};
        out->g = s.fields.g;
        out->h = s.fields.h;
        out->i = s.fields.i;
        out->j = s.fields.j;
        out->g_length = s.fields.g_length;
        out->h_length = s.fields.h_length;
        out->i_length = s.fields.i_length;
        out->j_length = s.fields.j_length;
    } else {
        const mc_texture_effect_owner *o = (const mc_texture_effect_owner *)effect;
        out->phase = o->phase;
        out->array_bytes = o->array_bytes;
        out->fields = o->fields;
        out->g = o->g;
        out->h = o->h;
        out->i = o->i;
        out->j = o->j;
        out->g_length = o->g_length;
        out->h_length = o->h_length;
        out->i_length = o->i_length;
        out->j_length = o->j_length;
        out->frames = (const uint8_t *const *)o->frames;
        out->frame_count = o->frame_count;
        out->icon = o->icon;
        out->dial = o->dial;
        out->app = o->app;
        out->counter = o->counter;
        out->last_store_pc = o->last_store_pc;
        out->angle = o->angle;
        out->velocity = o->velocity;
        out->failure = &o->failure;
        out->retirement = &o->retirement;
    }
    return true;
}
