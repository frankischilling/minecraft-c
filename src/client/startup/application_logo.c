#include "application_logo.h"
#include "application_owner_internal.h"
#include "application_startup_internal.h"

#include "../display/sdk_display_internal.h"
#include "../display/source_surface.h"
#include "../display/startup_surface.h"
#include "../gui/loading_internal.h"
#include "mc/core/java/java.h"

#include <stdlib.h>
#include <xmmintrin.h>

#define MC_CLASS "net/minecraft/client/Minecraft"

static bool logo_literal(mc_client_application_classes *classes,
                         mc_client_startup_logo_call *call) {
    call->pc = 119;
    if (mtx_lock(&classes->mutex) != thrd_success) {
        call->operation = "startup logo literal namespace lock";
        return false;
    }
    bool ok = !classes->native_retiring && classes->phase == MC_CLIENT_APPLICATION_CLASS_READY;
    if (ok && classes->startup_logo_resource == NULL) {
        static const uint16_t units[] = {'/', 't', 'i', 't', 'l', 'e', '/', 'm', 'o',
                                         'j', 'a', 'n', 'g', '.', 'p', 'n', 'g'};
        call->string = mc_player_string_create(units, sizeof(units) / sizeof(units[0]),
                                               &classes->startup_logo_resource);
        ok = call->string == MC_PLAYER_OK;
    }
    const int unlocked = mtx_unlock(&classes->mutex);
    if (!ok || unlocked != thrd_success) {
        call->operation = "actual startup logo ldc native prerequisite";
        return false;
    }
    call->literal = classes->startup_logo_resource;
    return true;
}

static bool graphics(mc_client_startup_logo_call *call, bool ok, int32_t pc) {
    call->pc = pc;
    if (!ok) {
        call->operation = "startup logo actual nw child";
        call->child = call->graphics.site;
        call->native_policy = call->graphics.native_policy;
    }
    return ok;
}

static bool device(mc_client_startup_logo_call *call, bool ok, int32_t pc, const char *method,
                   const char *descriptor) {
    call->pc = pc;
    if (!ok) {
        call->operation = call->display.operation;
        call->child = (mc_client_graphics_site){"org/lwjgl/opengl/GL11", method, descriptor, -1};
    }
    return ok;
}

static bool fp(mc_client_startup_logo_call *call, int32_t pc) {
    call->pc = pc;
    if ((_mm_getcsr() & 0xffc0u) == 0x1f80u)
        return true;
    call->operation = "startup logo reached native FP conversion";
    call->child = (mc_client_graphics_site){MC_CLASS, "x", "()V", pc};
    return false;
}

mc_client_startup_status application_startup_logo(mc_client_application_startup *startup) {
    if (startup == NULL || startup->app == NULL)
        return MC_CLIENT_STARTUP_INVALID_PHASE;
    if (startup->logo != NULL)
        return startup->logo->active ? MC_CLIENT_STARTUP_BUSY : MC_CLIENT_STARTUP_INVALID_PHASE;
    client_application *app = startup->app;
    if (app->constructor_phase != MC_CLIENT_APP_READY || app->source_namespace == NULL ||
        startup->phase == MC_CLIENT_STARTUP_RETIRING ||
        !mc_client_display_sdk_live(startup->display) || startup->engine_classes == NULL)
        return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_INVALID_PHASE,
                                        "startup logo live actual owners", NULL, NULL, NULL, 0);
    startup->failure.caller = (mc_client_application_owner_site){MC_CLASS, "a", "()V", 359};
    mc_client_startup_logo_call *call = calloc(1, sizeof(*call));
    if (call == NULL)
        return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_ALLOCATION_FAILED,
                                        "startup logo native call storage", NULL, NULL, NULL, 0);
    startup->logo = call;
    startup->failure.logo = call;
    const bool outer_active = startup->active;
    startup->active = true; /* Protect actual borrowed owners through provider callbacks. */
    call->active = call->native_policy = true;
    call->pc = 16;
    if (mc_client_loading_resolution(app, &call->resolution, &call->resolution_error, 16) !=
        MC_CLIENT_LOADING_OK) {
        call->operation = "startup logo actual qq constructor";
        call->child = (mc_client_graphics_site){
            call->resolution_error.child.class_name, call->resolution_error.child.method,
            call->resolution_error.child.descriptor, call->resolution_error.child.pc};
        call->native_policy = call->resolution_error.status != MC_CLIENT_LOADING_NULL_REFERENCE;
        goto done;
    }
    mc_client_startup_projection_stage stage = MC_CLIENT_STARTUP_PROJECTION_CLEAR;
    if (!mc_client_display_startup_projection(startup->display, call->resolution.width_exact,
                                              call->resolution.height_exact, &stage,
                                              &call->display)) {
        call->pc = (int32_t)stage;
        call->operation = call->display.operation;
        goto done;
    }
    /* Actual d73/e77 are reloaded after the matrix calls, not borrowed from qq. */
    const int32_t viewport_width = app->d, viewport_height = app->e;
    if (!mc_client_display_startup_viewport(startup->display, viewport_width, viewport_height,
                                            &stage, &call->display)) {
        call->pc = (int32_t)stage;
        call->operation = call->display.operation;
        goto done;
    }
    mc_client_tessellator *nw = NULL;
    if (!graphics(call,
                  mc_client_nw_active_use(startup->engine_classes,
                                          (mc_client_graphics_site){MC_CLASS, "x", "()V", 90}, &nw,
                                          &call->graphics),
                  90) ||
        !device(call,
                mc_client_display_source_disable_unlit_feature(
                    startup->display, MC_CLIENT_SOURCE_LIGHTING, &call->display),
                97, "glDisable", "(I)V") ||
        !device(call,
                mc_client_display_source_texture_enable(startup->display, true, &call->display),
                103, "glEnable", "(I)V") ||
        !device(call,
                mc_client_display_source_disable_unlit_feature(
                    startup->display, MC_CLIENT_SOURCE_FOG, &call->display),
                109, "glDisable", "(I)V"))
        goto done;
    mc_client_render_engine *engine = app->render_engine; /* Actual p116. */
    if (!logo_literal(app->source_namespace, call))
        goto done;
    call->pc = 121;
    if (engine == NULL) {
        call->native_policy = false;
        call->operation = "startup logo actual null ji receiver";
        call->child = (mc_client_graphics_site){MC_CLASS, "x", "()V", 121};
        goto done;
    }
    mc_player_string_view text;
    if (!mc_player_string_read(call->literal, &text)) {
        call->operation = "startup logo live scoped literal";
        goto done;
    }
    if (mc_client_render_engine_texture(engine, startup->display, NULL,
                                        (mc_texture_pack_text){text.units, text.length, false},
                                        &call->texture_id, &call->texture) != MC_CLIENT_LOOKUP_OK) {
        call->operation = "startup logo actual ji.b(String)";
        call->child = call->texture.site;
        call->native_policy = call->texture.native_policy;
        goto done;
    }
    if (!device(
            call,
            mc_client_display_source_bind_id(startup->display, call->texture_id, &call->display),
            124, "glBindTexture", "(II)V") ||
        !graphics(call, mc_client_nw_begin_quads(nw, &call->graphics), 128) ||
        !graphics(call, mc_client_nw_rgb(nw, 16777215, &call->graphics), 134) || !fp(call, 143))
        goto done;
    if (!graphics(call, mc_client_nw_vertex_uv(nw, 0, (double)app->e, 0, 0, 0, &call->graphics),
                  147) ||
        !fp(call, 155))
        goto done;
    const double width = (double)app->d, height = (double)app->e;
    if (!graphics(call, mc_client_nw_vertex_uv(nw, width, height, 0, 0, 0, &call->graphics), 164) ||
        !fp(call, 172))
        goto done;
    if (!graphics(call, mc_client_nw_vertex_uv(nw, (double)app->d, 0, 0, 0, 0, &call->graphics),
                  177) ||
        !graphics(call, mc_client_nw_vertex_uv(nw, 0, 0, 0, 0, 0, &call->graphics), 186) ||
        !graphics(call, mc_client_nw_draw(nw, startup->display, &call->graphics), 190))
        goto done;
    call->background_drawn = true;
    if (!device(call, mc_client_display_source_color(startup->display, 1, 1, 1, 1, &call->display),
                206, "glColor4f", "(FFFF)V") ||
        !graphics(call, mc_client_nw_rgb(nw, 16777215, &call->graphics), 212))
        goto done;
    int32_t x = 0, y = 0;
    (void)mc_java_i32_div(mc_java_i32_sub(call->resolution.width, 256), 2, &x);
    (void)mc_java_i32_div(mc_java_i32_sub(call->resolution.height, 256), 2, &y);
    call->pc = 238;
    if (!mc_client_application_textured_rectangle(startup->engine_classes, startup->display, x, y,
                                                  0, 0, 256, 256, &call->quad)) {
        call->operation = "startup logo actual Minecraft textured rectangle";
        call->child = (mc_client_graphics_site){MC_CLASS, "a", "(IIIIII)V", call->quad.pc};
        call->native_policy = call->quad.graphics.native_policy;
        goto done;
    }
    call->logo_drawn = true;
    if (!device(call, mc_client_display_startup_alpha(startup->display, &call->display), 244,
                "glDisable", "(I)V"))
        goto done;
    call->pc = 267;
    if (!mc_client_display_sdk_swap(startup->display, &call->swapped, &call->display)) {
        call->operation = "startup logo actual Display.swapBuffers";
        call->child =
            (mc_client_graphics_site){"org/lwjgl/opengl/Display", "swapBuffers", "()V", -1};
        goto done;
    }
    call->pc = 270;
    call->source_returned = true;
done:
    call->active = false;
    startup->active = outer_active;
    if (call->source_returned) {
        call->native_policy = false;
        startup->failure.status = MC_CLIENT_STARTUP_OK;
        startup->failure.operation = NULL;
        startup->failure.child = (mc_client_application_owner_site){0};
        return MC_CLIENT_STARTUP_OK;
    }
    return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_LOGO_FAILED,
                                    call->operation, call->child.class_name, call->child.method,
                                    call->child.descriptor,
                                    call->child.pc < 0 ? 0u : (unsigned int)call->child.pc);
}
