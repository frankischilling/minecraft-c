#include "../application_access.h"
#include "../application_state.h"
#include "source_screen_internal.h"

#include <stdlib.h>
#include <string.h>

mc_client_screen_status mc_client_screen_particles_allocate(mc_client_screen_classes *c,
                                                            const mc_client_screen_options *options,
                                                            mc_client_source_du **out,
                                                            mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (out == NULL)
        mc_screen_fail(&e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "du allocation output", NULL, NULL,
                       NULL, 0);
    else if (mc_screen_lock(c, &e)) {
        if (c->retiring || c->holds == SIZE_MAX)
            mc_screen_fail(&e, c->retiring ? MC_CLIENT_SCREEN_BUSY : MC_CLIENT_SCREEN_LIMIT,
                           "du class occurrence", NULL, NULL, NULL, 0);
        else if (options != NULL && options->maximum_bytes < sizeof(mc_client_source_du))
            mc_screen_fail(&e, MC_CLIENT_SCREEN_LIMIT, "du native owner budget", NULL, NULL, NULL,
                           0);
        else {
            mc_client_source_du *o = calloc(1, sizeof(*o));
            if (o == NULL)
                mc_screen_fail(&e, MC_CLIENT_SCREEN_ALLOCATION_FAILED, "du RAW owner", NULL, NULL,
                               NULL, 0);
            else {
                o->classes = c;
                o->holds = 1;
                mc_screen_memory_init(&o->memory, options, sizeof(*o));
                ++c->holds;
                *out = o;
            }
        }
        (void)mtx_unlock(&c->mutex);
    }
    return mc_screen_publish(&e, error);
}

bool mc_screen_particles_construct_locked(mc_client_source_du *o, client_application *app,
                                          mc_client_screen_error *e) {
    if (o->phase != MC_CLIENT_SCREEN_RAW || o->classes->retiring) {
        mc_screen_fail(e, MC_CLIENT_SCREEN_INVALID_PHASE, "one-shot du constructor", "du", "<init>",
                       "(Lnet/minecraft/client/Minecraft;)V", 0);
        return false;
    }
    o->phase = MC_CLIENT_SCREEN_CONSTRUCTING;
    o->last_pc = 1;
    o->k = 0.0f;
    o->last_pc = 5;
    mc_client_screen_list *list = mc_screen_allocate(&o->memory, sizeof(*list));
    if (list == NULL)
        mc_screen_fail(e, o->memory.allocation_status, "du actual new ArrayList", "du", "<init>",
                       "(Lnet/minecraft/client/Minecraft;)V", 5);
    else {
        list->kind = MC_SCREEN_LIST_PARTICLES;
        list->native_live = true;
        o->a = list;
        o->last_pc = 12;
        o->last_pc = 17;
        if (!mc_client_application_screen_borrow(app, &o->app_counted))
            mc_screen_fail(e, MC_CLIENT_SCREEN_INVALID_PHASE, "du actual App native screen borrow",
                           "du", "<init>", "(Lnet/minecraft/client/Minecraft;)V", 17);
        else {
            o->app_held = true;
            o->b = app;
            o->last_pc = 20;
            e->source_returned = true;
        }
    }
    o->phase = e->status == MC_CLIENT_SCREEN_OK ? MC_CLIENT_SCREEN_READY : MC_CLIENT_SCREEN_FAILED;
    if (e->status != MC_CLIENT_SCREEN_OK)
        o->failure = *e;
    return e->status == MC_CLIENT_SCREEN_OK;
}

mc_client_screen_status mc_client_screen_particles_construct(mc_client_source_du *o,
                                                             client_application *app,
                                                             mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (o == NULL)
        mc_screen_fail(&e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "du constructor receiver", NULL, NULL,
                       NULL, 0);
    else if (mc_screen_lock(o->classes, &e)) {
        (void)mc_screen_particles_construct_locked(o, app, &e);
        (void)mtx_unlock(&o->classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

mc_client_screen_status mc_client_screen_particles_retain(mc_client_source_du *o,
                                                          mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (o == NULL)
        mc_screen_fail(&e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "du retain receiver", NULL, NULL,
                       NULL, 0);
    else if (mc_screen_lock(o->classes, &e)) {
        if (o->classes->retiring || o->phase == MC_CLIENT_SCREEN_CONSTRUCTING ||
            o->phase == MC_CLIENT_SCREEN_RETIRING || o->holds == SIZE_MAX)
            mc_screen_fail(&e,
                           o->holds == SIZE_MAX ? MC_CLIENT_SCREEN_LIMIT : MC_CLIENT_SCREEN_BUSY,
                           "du native retain", NULL, NULL, NULL, 0);
        else
            ++o->holds;
        (void)mtx_unlock(&o->classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

static mc_client_screen_status release_particles(mc_client_source_du **slot, bool strict,
                                                 mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (slot == NULL)
        mc_screen_fail(&e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "du release slot", NULL, NULL, NULL,
                       0);
    else if (*slot != NULL && mc_screen_lock((*slot)->classes, &e)) {
        mc_client_source_du *o = *slot;
        mc_client_screen_classes *c = o->classes;
        if (o->phase == MC_CLIENT_SCREEN_CONSTRUCTING || (strict && o->holds != 1))
            mc_screen_fail(&e, MC_CLIENT_SCREEN_BUSY, "du strict retirement/active constructor",
                           NULL, NULL, NULL, 0);
        else if (o->holds > 1) {
            --o->holds;
            *slot = NULL;
        } else {
            o->phase = MC_CLIENT_SCREEN_RETIRING;
            if (c->holds != 1 || mc_screen_class_final_locked(c, &e)) {
                if (o->app_held && !mc_client_application_screen_release(o->b, o->app_counted)) {
                    mc_screen_fail(&e, MC_CLIENT_SCREEN_APP_FAILED, "du native App screen release",
                                   NULL, NULL, NULL, 0);
                    (void)mtx_unlock(&c->mutex);
                    return mc_screen_publish(&e, error);
                }
                o->app_held = false;
                o->app_counted = false;
                if (o->a != NULL)
                    o->a->native_live = false;
                mc_screen_memory_free(&o->memory);
                free(o);
                *slot = NULL;
                mc_screen_class_drop_locked(c);
                return mc_screen_publish(&e, error);
            }
        }
        (void)mtx_unlock(&c->mutex);
    }
    return mc_screen_publish(&e, error);
}

mc_client_screen_status mc_client_screen_particles_release(mc_client_source_du **o,
                                                           mc_client_screen_error *e) {
    return release_particles(o, false, e);
}

mc_client_screen_status mc_client_screen_particles_retire(mc_client_source_du **o,
                                                          mc_client_screen_error *e) {
    return release_particles(o, true, e);
}

mc_client_screen_status mc_client_screen_particles_inspect(mc_client_source_du *o,
                                                           mc_client_screen_du_state *out,
                                                           mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (o == NULL || out == NULL)
        mc_screen_fail(&e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "du inspection receiver/output", NULL,
                       NULL, NULL, 0);
    else if (mc_screen_lock(o->classes, &e)) {
        *out = (mc_client_screen_du_state){o->phase, o->holds,   o->k,      o->a,
                                           o->b,     o->last_pc, o->failure};
        (void)mtx_unlock(&o->classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

mc_client_screen_status mc_client_screen_button_allocate(mc_client_screen_classes *c,
                                                         mc_client_source_ke **out,
                                                         mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (out == NULL)
        mc_screen_fail(&e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "ke allocation output", NULL, NULL,
                       NULL, 0);
    else if (mc_screen_lock(c, &e)) {
        if (c->retiring || c->holds == SIZE_MAX)
            mc_screen_fail(&e, c->retiring ? MC_CLIENT_SCREEN_BUSY : MC_CLIENT_SCREEN_LIMIT,
                           "ke class occurrence", NULL, NULL, NULL, 0);
        else {
            mc_client_source_ke *o = calloc(1, sizeof(*o));
            if (o == NULL)
                mc_screen_fail(&e, MC_CLIENT_SCREEN_ALLOCATION_FAILED, "ke RAW owner", NULL, NULL,
                               NULL, 0);
            else {
                o->classes = c;
                o->holds = 1;
                ++c->holds;
                *out = o;
            }
        }
        (void)mtx_unlock(&c->mutex);
    }
    return mc_screen_publish(&e, error);
}

static void button_store(mc_client_source_ke *o, unsigned int pc) {
    o->last_pc = pc;
    o->stores[o->store_count++] = pc;
}

bool mc_screen_button_construct_locked(mc_client_source_ke *o, int32_t id, int32_t x, int32_t y,
                                       int32_t width, int32_t height, mc_client_screen_text text,
                                       mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (o->phase != MC_CLIENT_SCREEN_RAW || o->classes->retiring)
        mc_screen_fail(&e, MC_CLIENT_SCREEN_INVALID_PHASE, "one-shot ke constructor", "ke",
                       "<init>", "(IIIIILjava/lang/String;)V", 0);
    else {
        o->phase = MC_CLIENT_SCREEN_CONSTRUCTING;
        o->fields.k = 0.0f;
        button_store(o, 6); /* ub.<init>, not a ke store. */
        o->fields.a = 200;
        button_store(o, 8);
        o->fields.b = 20;
        button_store(o, 14);
        o->fields.g = true;
        button_store(o, 19);
        o->fields.h = true;
        button_store(o, 24);
        o->fields.f = id;
        button_store(o, 29);
        o->fields.c = x;
        button_store(o, 34);
        o->fields.d = y;
        button_store(o, 39);
        o->fields.a = width;
        button_store(o, 45);
        o->fields.b = height;
        button_store(o, 51);
        bool valid = false;
        if (text.kind == MC_CLIENT_SCREEN_TEXT_NULL)
            valid = text.player == NULL && text.registry == NULL;
        else if (text.kind == MC_CLIENT_SCREEN_TEXT_PLAYER && text.player != NULL &&
                 text.registry == NULL) {
            e.string = mc_player_string_retain((mc_player_string *)text.player);
            valid = e.string == MC_PLAYER_OK;
        } else if (text.kind == MC_CLIENT_SCREEN_TEXT_REGISTRY)
            valid = o->classes->registry_held && text.registry != NULL && text.player == NULL;
        if (!valid) {
            mc_screen_fail(&e,
                           e.string != MC_PLAYER_OK ? MC_CLIENT_SCREEN_STRING_FAILED
                                                    : MC_CLIENT_SCREEN_INVALID_ARGUMENT,
                           "ke same retained String owner admission", "ke", "<init>",
                           "(IIIIILjava/lang/String;)V", 57);
            o->last_pc = 57;
            o->phase = MC_CLIENT_SCREEN_FAILED;
            o->failure = e;
        } else {
            o->fields.e = text;
            button_store(o, 57);
            o->last_pc = 60;
            o->phase = MC_CLIENT_SCREEN_READY;
            e.source_returned = true;
        }
    }
    (void)mc_screen_publish(&e, error);
    return e.status == MC_CLIENT_SCREEN_OK;
}

mc_client_screen_status mc_client_screen_button_construct(mc_client_source_ke *o, int32_t id,
                                                          int32_t x, int32_t y, int32_t width,
                                                          int32_t height,
                                                          mc_client_screen_text text,
                                                          mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (o == NULL)
        mc_screen_fail(&e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "ke constructor receiver", NULL, NULL,
                       NULL, 0);
    else if (mc_screen_lock(o->classes, &e)) {
        (void)mc_screen_button_construct_locked(o, id, x, y, width, height, text, &e);
        (void)mtx_unlock(&o->classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

mc_client_screen_status mc_client_screen_button_construct_default(mc_client_source_ke *o,
                                                                  int32_t id, int32_t x, int32_t y,
                                                                  mc_client_screen_text text,
                                                                  mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    const mc_client_screen_status status =
        mc_client_screen_button_construct(o, id, x, y, 200, 20, text, &e);
    if (status != MC_CLIENT_SCREEN_OK)
        e.parent_site = (mc_client_screen_site){"ke", "<init>", "(IIILjava/lang/String;)V", 11};
    return mc_screen_publish(&e, error);
}

mc_client_screen_status mc_client_screen_button_retain(mc_client_source_ke *o,
                                                       mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (o == NULL)
        mc_screen_fail(&e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "ke retain receiver", NULL, NULL,
                       NULL, 0);
    else if (mc_screen_lock(o->classes, &e)) {
        if (o->classes->retiring || o->phase == MC_CLIENT_SCREEN_CONSTRUCTING ||
            o->phase == MC_CLIENT_SCREEN_RETIRING || o->holds == SIZE_MAX)
            mc_screen_fail(&e,
                           o->holds == SIZE_MAX ? MC_CLIENT_SCREEN_LIMIT : MC_CLIENT_SCREEN_BUSY,
                           "ke native retain", NULL, NULL, NULL, 0);
        else
            ++o->holds;
        (void)mtx_unlock(&o->classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

static mc_client_screen_status release_button(mc_client_source_ke **slot, bool strict,
                                              mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (slot == NULL)
        mc_screen_fail(&e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "ke release slot", NULL, NULL, NULL,
                       0);
    else if (*slot != NULL && mc_screen_lock((*slot)->classes, &e)) {
        mc_client_source_ke *o = *slot;
        mc_client_screen_classes *c = o->classes;
        if (o->phase == MC_CLIENT_SCREEN_CONSTRUCTING || (strict && o->holds != 1))
            mc_screen_fail(&e, MC_CLIENT_SCREEN_BUSY, "ke strict retirement/active constructor",
                           NULL, NULL, NULL, 0);
        else if (o->holds > 1) {
            --o->holds;
            *slot = NULL;
        } else {
            o->phase = MC_CLIENT_SCREEN_RETIRING;
            if (c->holds != 1 || mc_screen_class_final_locked(c, &e)) {
                if (o->fields.e.kind == MC_CLIENT_SCREEN_TEXT_PLAYER)
                    mc_player_string_release((mc_player_string *)o->fields.e.player);
                free(o);
                *slot = NULL;
                mc_screen_class_drop_locked(c);
                return mc_screen_publish(&e, error);
            }
        }
        (void)mtx_unlock(&c->mutex);
    }
    return mc_screen_publish(&e, error);
}

mc_client_screen_status mc_client_screen_button_release(mc_client_source_ke **o,
                                                        mc_client_screen_error *e) {
    return release_button(o, false, e);
}

mc_client_screen_status mc_client_screen_button_retire(mc_client_source_ke **o,
                                                       mc_client_screen_error *e) {
    return release_button(o, true, e);
}

mc_client_screen_status mc_client_screen_button_inspect(mc_client_source_ke *o,
                                                        mc_client_screen_ke_state *out,
                                                        mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (o == NULL || out == NULL)
        mc_screen_fail(&e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "ke inspection receiver/output", NULL,
                       NULL, NULL, 0);
    else if (mc_screen_lock(o->classes, &e)) {
        *out = (mc_client_screen_ke_state){0};
        out->phase = o->phase;
        out->holds = o->holds;
        out->fields = o->fields;
        out->last_pc = o->last_pc;
        out->store_count = o->store_count;
        memcpy(out->stores, o->stores, sizeof(out->stores));
        out->failure = o->failure;
        (void)mtx_unlock(&o->classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

/* Admitted nested source children execute under the SAME held class monitor.
 * These do not make public reentry recursive or admit a fabricated child. */
mc_client_source_du *mc_screen_particles_allocate_locked(mc_client_screen_classes *c,
                                                         const mc_client_screen_options *options,
                                                         mc_client_screen_error *e) {
    if (c->retiring || c->holds == SIZE_MAX) {
        mc_screen_fail(e, c->retiring ? MC_CLIENT_SCREEN_BUSY : MC_CLIENT_SCREEN_LIMIT,
                       "du class occurrence", NULL, NULL, NULL, 0);
        return NULL;
    }
    if (options != NULL && options->maximum_bytes < sizeof(mc_client_source_du)) {
        mc_screen_fail(e, MC_CLIENT_SCREEN_LIMIT, "du native owner budget", NULL, NULL, NULL, 0);
        return NULL;
    }
    mc_client_source_du *o = calloc(1, sizeof(*o));
    if (o == NULL) {
        mc_screen_fail(e, MC_CLIENT_SCREEN_ALLOCATION_FAILED, "du RAW owner", NULL, NULL, NULL, 0);
        return NULL;
    }
    o->classes = c;
    o->holds = 1;
    mc_screen_memory_init(&o->memory, options, sizeof(*o));
    ++c->holds;
    return o;
}

mc_client_source_ke *mc_screen_button_allocate_locked(mc_client_screen_classes *c,
                                                      mc_client_screen_error *e) {
    if (c->retiring || c->holds == SIZE_MAX) {
        mc_screen_fail(e, c->retiring ? MC_CLIENT_SCREEN_BUSY : MC_CLIENT_SCREEN_LIMIT,
                       "ke class occurrence", NULL, NULL, NULL, 0);
        return NULL;
    }
    mc_client_source_ke *o = calloc(1, sizeof(*o));
    if (o == NULL) {
        mc_screen_fail(e, MC_CLIENT_SCREEN_ALLOCATION_FAILED, "ke RAW owner", NULL, NULL, NULL, 0);
        return NULL;
    }
    o->classes = c;
    o->holds = 1;
    ++c->holds;
    return o;
}

bool mc_screen_button_hold_locked(mc_client_source_ke *o, mc_client_screen_error *e) {
    if (o == NULL || o->classes->retiring || o->phase == MC_CLIENT_SCREEN_CONSTRUCTING ||
        o->phase == MC_CLIENT_SCREEN_RETIRING || o->holds == SIZE_MAX) {
        mc_screen_fail(
            e, o != NULL && o->holds == SIZE_MAX ? MC_CLIENT_SCREEN_LIMIT : MC_CLIENT_SCREEN_BUSY,
            "ke native held occurrence", NULL, NULL, NULL, 0);
        return false;
    }
    ++o->holds;
    return true;
}

bool mc_screen_particles_release_locked(mc_client_source_du **slot, mc_client_screen_error *e) {
    if (*slot == NULL)
        return true;
    mc_client_source_du *o = *slot;
    if (o->phase == MC_CLIENT_SCREEN_CONSTRUCTING || o->classes->holds <= 1) {
        mc_screen_fail(e, MC_CLIENT_SCREEN_BUSY, "du nested retirement admission", NULL, NULL, NULL,
                       0);
        return false;
    }
    if (o->holds > 1) {
        --o->holds;
        *slot = NULL;
        return true;
    }
    o->phase = MC_CLIENT_SCREEN_RETIRING;
    if (o->app_held && !mc_client_application_screen_release(o->b, o->app_counted)) {
        mc_screen_fail(e, MC_CLIENT_SCREEN_APP_FAILED, "du native App screen release", NULL, NULL,
                       NULL, 0);
        return false;
    }
    o->app_held = false;
    o->app_counted = false;
    if (o->a != NULL)
        o->a->native_live = false;
    --o->classes->holds;
    mc_screen_memory_free(&o->memory);
    free(o);
    *slot = NULL;
    return true;
}

bool mc_screen_button_release_locked(mc_client_source_ke **slot, mc_client_screen_error *e) {
    if (*slot == NULL)
        return true;
    mc_client_source_ke *o = *slot;
    if (o->phase == MC_CLIENT_SCREEN_CONSTRUCTING || o->classes->holds <= 1) {
        mc_screen_fail(e, MC_CLIENT_SCREEN_BUSY, "ke nested retirement admission", NULL, NULL, NULL,
                       0);
        return false;
    }
    if (o->holds > 1) {
        --o->holds;
        *slot = NULL;
        return true;
    }
    o->phase = MC_CLIENT_SCREEN_RETIRING;
    if (o->fields.e.kind == MC_CLIENT_SCREEN_TEXT_PLAYER)
        mc_player_string_release((mc_player_string *)o->fields.e.player);
    --o->classes->holds;
    free(o);
    *slot = NULL;
    return true;
}
