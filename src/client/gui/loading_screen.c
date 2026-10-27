#include "../application_state.h"
#include "loading_internal.h"
#include "mc/core/java/java.h"
#include "scaled_resolution_internal.h"

#include <stdlib.h>
#include <xmmintrin.h>

mc_client_loading_status mc_client_loading_fail(mc_client_loading_error *e,
                                                mc_client_loading_status status,
                                                const char *operation, const char *method,
                                                const char *descriptor, int32_t pc) {
    e->status = status;
    e->operation = operation;
    e->site = (mc_client_loading_site){"mk", method, descriptor, pc};
    return status;
}

static mc_client_loading_status finish(mc_client_loading_status status,
                                       const mc_client_loading_error *detail,
                                       mc_client_loading_error *error) {
    if (error != NULL)
        *error = *detail;
    return status;
}

mc_client_loading_status mc_client_loading_classes_create(mc_client_render_engine_classes *graphics,
                                                          mc_client_loading_classes **out) {
    if (graphics == NULL || out == NULL)
        return MC_CLIENT_LOADING_INVALID_ARGUMENT;
    mc_client_loading_classes *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return MC_CLIENT_LOADING_ALLOCATION_FAILED;
    const mc_player_status status = mc_player_string_create(NULL, 0, &c->empty);
    if (status != MC_PLAYER_OK) {
        free(c);
        return status == MC_PLAYER_ALLOCATION_FAILED ? MC_CLIENT_LOADING_ALLOCATION_FAILED
                                                     : MC_CLIENT_LOADING_LIMIT;
    }
    if (mc_client_render_engine_classes_retain(graphics) != MC_CLIENT_RENDER_ENGINE_OK) {
        mc_player_string_release(c->empty);
        free(c);
        return MC_CLIENT_LOADING_LIMIT;
    }
    c->graphics = graphics;
    c->holds = 1;
    *out = c;
    return MC_CLIENT_LOADING_OK;
}

mc_client_loading_status
mc_client_loading_classes_bind_resources(mc_client_loading_classes *c,
                                         const mc_registry_resources *resources,
                                         mc_client_loading_error *error) {
    mc_client_loading_error e = {0};
    if (c == NULL || resources == NULL || (c->resources != NULL && c->resources != resources))
        return finish(mc_client_loading_fail(&e, MC_CLIENT_LOADING_INVALID_ARGUMENT,
                                             "GUI actual class-loader resource binding", NULL, NULL,
                                             -1),
                      &e, error);
    if (c->resources != NULL)
        return finish(MC_CLIENT_LOADING_OK, &e, error);
    mc_client_font_mapping *mapping = NULL;
    const mc_client_font_mapping_status status =
        mc_client_font_mapping_create(resources, c->empty, 8u * 1024u * 1024u, SIZE_MAX, &mapping);
    if (status != MC_CLIENT_FONT_MAPPING_OK) {
        e.fp.status = status;
        return finish(mc_client_loading_fail(&e,
                                             status == MC_CLIENT_FONT_MAPPING_ALLOCATION_FAILED
                                                 ? MC_CLIENT_LOADING_ALLOCATION_FAILED
                                                 : MC_CLIENT_LOADING_LIMIT,
                                             "GUI cold fp class owner", NULL, NULL, -1),
                      &e, error);
    }
    c->resources = resources;
    c->mapping = mapping;
    return finish(MC_CLIENT_LOADING_OK, &e, error);
}

mc_player_string *mc_client_loading_classes_empty(const mc_client_loading_classes *classes) {
    return classes == NULL ? NULL : classes->empty;
}

mc_client_loading_status mc_client_loading_classes_retain(mc_client_loading_classes *c) {
    if (c == NULL)
        return MC_CLIENT_LOADING_INVALID_ARGUMENT;
    if (c->holds == SIZE_MAX)
        return MC_CLIENT_LOADING_LIMIT;
    ++c->holds;
    return MC_CLIENT_LOADING_OK;
}

mc_client_loading_status mc_client_loading_classes_release(mc_client_loading_classes *c) {
    if (c == NULL)
        return MC_CLIENT_LOADING_OK;
    if (c->holds == 1) {
        mc_client_font_mapping_state mapping;
        if (c->mapping != NULL && (!mc_client_font_mapping_inspect(c->mapping, &mapping) ||
                                   mapping.phase == MC_REGISTRY_INITIALIZING))
            return MC_CLIENT_LOADING_BUSY;
        if (mc_client_render_engine_classes_release(c->graphics) != MC_CLIENT_RENDER_ENGINE_OK)
            return MC_CLIENT_LOADING_BUSY;
        (void)mc_client_font_mapping_release(c->mapping);
        mc_player_string_release(c->empty);
        free(c);
    } else
        --c->holds;
    return MC_CLIENT_LOADING_OK;
}

mc_client_render_engine_classes *
mc_client_loading_screen_graphics(const mc_client_loading_screen *p) {
    return p == NULL ? NULL : p->classes->graphics;
}

mc_client_loading_status mc_client_loading_screen_allocate(mc_client_loading_classes *classes,
                                                           mc_client_loading_screen **out) {
    if (classes == NULL || out == NULL)
        return MC_CLIENT_LOADING_INVALID_ARGUMENT;
    mc_client_loading_screen *p = calloc(1, sizeof(*p));
    if (p == NULL)
        return MC_CLIENT_LOADING_ALLOCATION_FAILED;
    const mc_client_loading_status status = mc_client_loading_classes_retain(classes);
    if (status != MC_CLIENT_LOADING_OK) {
        free(p);
        return status;
    }
    p->holds = 1;
    p->classes = classes;
    p->last_store_pc = -1;
    *out = p;
    return MC_CLIENT_LOADING_OK;
}

mc_client_loading_status mc_client_loading_screen_retain(mc_client_loading_screen *p) {
    if (p == NULL)
        return MC_CLIENT_LOADING_INVALID_ARGUMENT;
    if (p->holds == SIZE_MAX)
        return MC_CLIENT_LOADING_LIMIT;
    ++p->holds;
    return MC_CLIENT_LOADING_OK;
}

mc_client_loading_status mc_client_loading_screen_release(mc_client_loading_screen *p) {
    if (p == NULL)
        return MC_CLIENT_LOADING_OK;
    if (p->active || p->phase == MC_CLIENT_LOADING_CONSTRUCTING)
        return MC_CLIENT_LOADING_BUSY;
    if (p->holds > 1) {
        --p->holds;
        return MC_CLIENT_LOADING_OK;
    }
    const mc_client_loading_status status = mc_client_loading_classes_release(p->classes);
    if (status != MC_CLIENT_LOADING_OK)
        return status;
    mc_player_string_release((mc_player_string *)p->fields.a);
    mc_player_string_release((mc_player_string *)p->fields.c);
    free(p);
    return MC_CLIENT_LOADING_OK;
}

bool mc_client_loading_screen_inspect(const mc_client_loading_screen *p,
                                      mc_client_loading_state *out) {
    if (p == NULL || out == NULL)
        return false;
    *out = (mc_client_loading_state){p->phase, p->fields, p->last_store_pc, p->holds, p->active};
    return true;
}

static bool string_store(mc_client_loading_screen *p, const mc_player_string **slot,
                         mc_player_string *value, int32_t pc, mc_client_loading_error *e) {
    if (value != NULL && mc_player_string_retain(value) != MC_PLAYER_OK) {
        mc_client_loading_fail(e, MC_CLIENT_LOADING_LIMIT, "LoadingScreen String native hold", NULL,
                               NULL, pc);
        return false;
    }
    const mc_player_string *old = *slot;
    *slot = value;
    p->last_store_pc = pc;
    mc_player_string_release((mc_player_string *)old);
    return true;
}

mc_client_loading_status mc_client_loading_screen_construct(mc_client_loading_screen *p,
                                                            client_application *app,
                                                            mc_client_loading_error *error) {
    mc_client_loading_error e = {0};
    if (p == NULL || p->phase != MC_CLIENT_LOADING_RAW)
        return finish(mc_client_loading_fail(&e,
                                             p == NULL ? MC_CLIENT_LOADING_INVALID_ARGUMENT
                                                       : MC_CLIENT_LOADING_INVALID_PHASE,
                                             "LoadingScreen constructor admission", "<init>",
                                             "(Lnet/minecraft/client/Minecraft;)V", -1),
                      &e, error);
    p->phase = MC_CLIENT_LOADING_CONSTRUCTING;
    if (!string_store(p, &p->fields.a, p->classes->empty, 7, &e) ||
        !string_store(p, &p->fields.c, p->classes->empty, 13, &e))
        goto failed;
    int64_t now;
    e.clock = mc_platform_current_time_millis(&now);
    if (e.clock != MC_PLATFORM_MILLIS_OK) {
        mc_client_loading_fail(&e, MC_CLIENT_LOADING_CLOCK_FAILED,
                               "LoadingScreen constructor clock", "<init>",
                               "(Lnet/minecraft/client/Minecraft;)V", 17);
        goto failed;
    }
    p->fields.d = now;
    p->last_store_pc = 20;
    p->fields.e = false;
    p->last_store_pc = 25;
    p->fields.b = app;
    p->last_store_pc = 30;
    p->phase = MC_CLIENT_LOADING_READY;
    return finish(MC_CLIENT_LOADING_OK, &e, error);
failed:
    if (e.site.method == NULL) {
        e.site.method = "<init>";
        e.site.descriptor = "(Lnet/minecraft/client/Minecraft;)V";
    }
    p->phase = MC_CLIENT_LOADING_FAILED;
    return finish(e.status, &e, error);
}

static mc_client_loading_status gate(mc_client_loading_screen *p, const char *method,
                                     const char *descriptor, mc_client_loading_error *e,
                                     bool *execute) {
    *execute = false;
    client_application *app = p->fields.b;
    if (app == NULL)
        return mc_client_loading_fail(e, MC_CLIENT_LOADING_NULL_REFERENCE, "actual null App.J",
                                      method, descriptor, 4);
    /* Native storage admission precedes the pending source getfield4. It
     * neither supplies a source flag default nor reads other volatile fields. */
    if (app->native_lifetime_phase != MC_CLIENT_APPLICATION_LIFETIME_REGISTERED ||
        app->constructor_phase == MC_CLIENT_APP_RETIRING)
        return mc_client_loading_fail(e, MC_CLIENT_LOADING_INVALID_PHASE,
                                      "mk App.J native registered-lifetime admission", method,
                                      descriptor, 4);
    if (atomic_load_explicit(&app->J, memory_order_seq_cst)) {
        *execute = true;
        return MC_CLIENT_LOADING_OK;
    }
    if (p->fields.e) {
        e->stopped_allowed = true;
        return MC_CLIENT_LOADING_OK;
    }
    e->child = (mc_client_loading_site){"xx", "<init>", "()V", 2};
    return mc_client_loading_fail(e, MC_CLIENT_LOADING_STOPPED_ERROR, "actual stopped xx Error",
                                  method, descriptor, 25);
}

mc_client_loading_status mc_client_loading_resolution(client_application *app,
                                                      mc_scaled_resolution *resolution,
                                                      mc_client_loading_error *e, int32_t pc) {
    /* Capture the real arguments in the invokevirtual caller's order. The
     * Settings receiver is dereferenced by qq only after d/e/c stores. */
    mc_game_settings *settings = app->settings;
    const int32_t width = app->d;
    const int32_t height = app->e;
    e->qq_width = width;
    e->qq_height = height;
    e->qq_factor = 1;
    if (settings == NULL) {
        e->child = (mc_client_loading_site){"qq", "<init>", "(Lkv;II)V", 20};
        return mc_client_loading_fail(e, MC_CLIENT_LOADING_NULL_REFERENCE,
                                      "qq actual null Settings", NULL, NULL, pc);
    }
    mc_game_settings_state state;
    if (!mc_game_settings_read_state(settings, &state))
        return mc_client_loading_fail(e, MC_CLIENT_LOADING_SETTINGS_FAILED, "qq live Settings read",
                                      NULL, NULL, pc);
    if (width <= 0 || height <= 0)
        return mc_client_loading_fail(e, MC_CLIENT_LOADING_LIMIT,
                                      "qq positive-dimension native bound", NULL, NULL, pc);
    e->qq_factor = mc_scaled_resolution_source_factor(width, height, state.values.gui_scale);
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        e->child = (mc_client_loading_site){"qq", "<init>", "(Lkv;II)V", 102};
        return mc_client_loading_fail(e, MC_CLIENT_LOADING_UNSUPPORTED_FP,
                                      "qq reached native FP environment", NULL, NULL, pc);
    }
    mc_scaled_resolution_source_finish(width, height, e->qq_factor, resolution);
    return MC_CLIENT_LOADING_OK;
}

static mc_client_loading_status title(mc_client_loading_screen *p, mc_player_string *text,
                                      mc_client_loading_error *e) {
    bool execute;
    mc_client_loading_status status = gate(p, "c", "(Ljava/lang/String;)V", e, &execute);
    if (status != MC_CLIENT_LOADING_OK || !execute)
        return status;
    if (!string_store(p, &p->fields.c, text, 28, e)) {
        e->site.method = "c";
        e->site.descriptor = "(Ljava/lang/String;)V";
        return e->status;
    }
    mc_scaled_resolution resolution;
    status = mc_client_loading_resolution(p->fields.b, &resolution, e, 56);
    if (status == MC_CLIENT_LOADING_OK)
        status = mc_client_loading_projection(p->fields.b, &resolution, e);
    if (status != MC_CLIENT_LOADING_OK && e->site.method == NULL) {
        e->site.class_name = "mk";
        e->site.method = "c";
        e->site.descriptor = "(Ljava/lang/String;)V";
    }
    return status;
}

static mc_client_loading_status progress(mc_client_loading_screen *p, int32_t value,
                                         mc_client_loading_error *e) {
    bool execute;
    mc_client_loading_status status = gate(p, "a", "(I)V", e, &execute);
    if (status != MC_CLIENT_LOADING_OK || !execute)
        return status;
    int64_t now;
    e->clock = mc_platform_current_time_millis(&now);
    if (e->clock != MC_PLATFORM_MILLIS_OK)
        return mc_client_loading_fail(e, MC_CLIENT_LOADING_CLOCK_FAILED,
                                      "LoadingScreen progress clock", "a", "(I)V", 26);
    if (mc_java_i64_sub(now, p->fields.d) < 20)
        return MC_CLIENT_LOADING_OK;
    p->fields.d = now;
    p->last_store_pc = 46;
    e->sampled_clock_committed = true;
    return mc_client_loading_draw(p, value, e);
}

typedef enum loading_call {
    CALL_START,
    CALL_ALLOW,
    CALL_TITLE,
    CALL_MESSAGE,
    CALL_PROGRESS
} loading_call;

static mc_client_loading_status invoke(mc_client_loading_screen *p, loading_call call,
                                       mc_player_string *text, int32_t value,
                                       mc_client_loading_error *error) {
    mc_client_loading_error admission = {0};
    if (p == NULL || p->active || p->phase != MC_CLIENT_LOADING_READY)
        return finish(mc_client_loading_fail(&admission,
                                             p == NULL   ? MC_CLIENT_LOADING_NULL_REFERENCE
                                             : p->active ? MC_CLIENT_LOADING_BUSY
                                                         : MC_CLIENT_LOADING_INVALID_PHASE,
                                             "LoadingScreen invocation admission", NULL, NULL, -1),
                      &admission, error);
    mc_client_loading_error *e = calloc(1, sizeof(*e));
    if (e == NULL)
        return finish(mc_client_loading_fail(&admission, MC_CLIENT_LOADING_ALLOCATION_FAILED,
                                             "LoadingScreen native call diagnostics", NULL, NULL,
                                             -1),
                      &admission, error);
    mc_client_loading_status status = mc_client_loading_screen_retain(p);
    if (status != MC_CLIENT_LOADING_OK) {
        free(e);
        return finish(mc_client_loading_fail(&admission, status, "LoadingScreen native call hold",
                                             NULL, NULL, -1),
                      &admission, error);
    }
    p->active = true;
    switch (call) {
        case CALL_START:
            p->fields.e = false;
            p->last_store_pc = 2;
            status = title(p, text, e);
            if (status != MC_CLIENT_LOADING_OK)
                e->parent = (mc_client_loading_site){"mk", "a", "(Ljava/lang/String;)V", 7};
            break;
        case CALL_ALLOW:
            p->fields.e = true;
            p->last_store_pc = 2;
            status = title(p, (mc_player_string *)p->fields.c, e);
            if (status != MC_CLIENT_LOADING_OK)
                e->parent = (mc_client_loading_site){"mk", "b", "(Ljava/lang/String;)V", 10};
            break;
        case CALL_TITLE:
            status = title(p, text, e);
            break;
        case CALL_MESSAGE: {
            bool execute;
            status = gate(p, "d", "(Ljava/lang/String;)V", e, &execute);
            if (status != MC_CLIENT_LOADING_OK || !execute)
                break;
            p->fields.d = 0;
            p->last_store_pc = 28;
            if (!string_store(p, &p->fields.a, text, 33, e)) {
                e->site.method = "d";
                e->site.descriptor = "(Ljava/lang/String;)V";
                status = e->status;
                break;
            }
            status = progress(p, -1, e);
            if (status != MC_CLIENT_LOADING_OK)
                e->parent = (mc_client_loading_site){"mk", "d", "(Ljava/lang/String;)V", 38};
            if (status == MC_CLIENT_LOADING_OK) {
                p->fields.d = 0;
                p->last_store_pc = 43;
            }
            break;
        }
        case CALL_PROGRESS:
            status = progress(p, value, e);
            break;
    }
    p->active = false;
    (void)mc_client_loading_screen_release(p);
    finish(status, e, error);
    free(e);
    return status;
}

mc_client_loading_status mc_client_loading_screen_start(mc_client_loading_screen *p,
                                                        mc_player_string *text,
                                                        mc_client_loading_error *e) {
    return invoke(p, CALL_START, text, 0, e);
}

mc_client_loading_status mc_client_loading_screen_allow_stopped(mc_client_loading_screen *p,
                                                                mc_player_string *ignored,
                                                                mc_client_loading_error *e) {
    (void)ignored;
    return invoke(p, CALL_ALLOW, NULL, 0, e);
}

mc_client_loading_status mc_client_loading_screen_title(mc_client_loading_screen *p,
                                                        mc_player_string *text,
                                                        mc_client_loading_error *e) {
    return invoke(p, CALL_TITLE, text, 0, e);
}

mc_client_loading_status mc_client_loading_screen_message(mc_client_loading_screen *p,
                                                          mc_player_string *text,
                                                          mc_client_loading_error *e) {
    return invoke(p, CALL_MESSAGE, text, 0, e);
}

mc_client_loading_status mc_client_loading_screen_progress(mc_client_loading_screen *p,
                                                           int32_t value,
                                                           mc_client_loading_error *e) {
    return invoke(p, CALL_PROGRESS, NULL, value, e);
}
