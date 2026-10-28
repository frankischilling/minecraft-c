#include "../application_state.h"
#include "../display/sdk_display_internal.h"
#include "../display/source_surface.h"
#include "../render_engine/nw.h"
#include "loading_internal.h"
#include "mc/core/java/java.h"

#include <threads.h>
#include <xmmintrin.h>

static mc_client_loading_status failed(mc_client_loading_error *e, mc_client_loading_status status,
                                       const char *operation, int32_t pc) {
    return mc_client_loading_fail(e, status, operation, "a", "(I)V", pc);
}

static bool fp(mc_client_loading_error *e, int32_t pc) {
    if ((_mm_getcsr() & 0xffc0u) == 0x1f80u)
        return true;
    failed(e, MC_CLIENT_LOADING_UNSUPPORTED_FP, "mk reached caller FP conversion", pc);
    return false;
}

static mc_client_loading_status device(mc_client_loading_error *e, const char *method,
                                       const char *descriptor, int32_t pc) {
    e->child = (mc_client_loading_site){"org/lwjgl/opengl/GL11", method, descriptor, -1};
    return failed(e,
                  e->display.status == MC_CLIENT_DISPLAY_UNSUPPORTED_FP
                      ? MC_CLIENT_LOADING_UNSUPPORTED_FP
                  : e->display.status == MC_CLIENT_DISPLAY_UNSUPPORTED_GL
                      ? MC_CLIENT_LOADING_UNIMPLEMENTED_DEPENDENCY
                      : MC_CLIENT_LOADING_DISPLAY_FAILED,
                  "mk actual Display adapter", pc);
}

static bool graphics(mc_client_loading_error *e, bool ok, int32_t pc) {
    if (ok)
        return true;
    e->child = (mc_client_loading_site){e->graphics.site.class_name, e->graphics.site.method,
                                        e->graphics.site.descriptor, e->graphics.site.pc};
    failed(
        e,
        e->graphics.status == MC_CLIENT_GRAPHICS_ALLOCATION_FAILED
            ? MC_CLIENT_LOADING_ALLOCATION_FAILED
        : e->graphics.status == MC_CLIENT_GRAPHICS_LIMIT          ? MC_CLIENT_LOADING_LIMIT
        : e->graphics.status == MC_CLIENT_GRAPHICS_BUSY           ? MC_CLIENT_LOADING_BUSY
        : e->graphics.status == MC_CLIENT_GRAPHICS_UNSUPPORTED_FP ? MC_CLIENT_LOADING_UNSUPPORTED_FP
        : e->graphics.status == MC_CLIENT_GRAPHICS_NULL_REFERENCE ? MC_CLIENT_LOADING_NULL_REFERENCE
        : e->graphics.status == MC_CLIENT_GRAPHICS_DISPLAY_FAILED ? MC_CLIENT_LOADING_DISPLAY_FAILED
        : e->graphics.status == MC_CLIENT_GRAPHICS_UNIMPLEMENTED_DEPENDENCY
            ? MC_CLIENT_LOADING_UNIMPLEMENTED_DEPENDENCY
            : MC_CLIENT_LOADING_GRAPHICS_FAILED,
        "mk actual nw child", pc);
    return false;
}

static bool font_child(mc_client_loading_error *e, mc_client_bitmap_font_status status,
                       int32_t pc) {
    if (status == MC_CLIENT_BITMAP_FONT_OK)
        return true;
    e->child = (mc_client_loading_site){"sj", e->font.method, e->font.descriptor, e->font.pc};
    failed(e,
           status == MC_CLIENT_BITMAP_FONT_ALLOCATION_FAILED ? MC_CLIENT_LOADING_ALLOCATION_FAILED
           : status == MC_CLIENT_BITMAP_FONT_LIMIT           ? MC_CLIENT_LOADING_LIMIT
           : status == MC_CLIENT_BITMAP_FONT_BUSY            ? MC_CLIENT_LOADING_BUSY
           : status == MC_CLIENT_BITMAP_FONT_UNSUPPORTED_FP  ? MC_CLIENT_LOADING_UNSUPPORTED_FP
           : status == MC_CLIENT_BITMAP_FONT_NULL_REFERENCE  ? MC_CLIENT_LOADING_NULL_REFERENCE
           : status == MC_CLIENT_BITMAP_FONT_UNIMPLEMENTED_DEPENDENCY ||
                   status == MC_CLIENT_BITMAP_FONT_UNSUPPORTED_LOWERCASE
               ? MC_CLIENT_LOADING_UNIMPLEMENTED_DEPENDENCY
               : MC_CLIENT_LOADING_FONT_FAILED,
           "mk actual sj child", pc);
    return false;
}

static bool vertex(mc_client_tessellator *nw, int32_t x, int32_t y, int32_t conversion_pc,
                   int32_t invoke_pc, mc_client_loading_error *e) {
    if (!fp(e, conversion_pc))
        return false;
    const double dx = (double)x;
    const double dy = (double)y;
    return graphics(e, mc_client_nw_vertex(nw, dx, dy, 0.0, &e->graphics), invoke_pc);
}

static mc_client_bitmap_font *capture_font(mc_client_loading_screen *p, size_t slot, int32_t pc,
                                           bool *ok) {
    client_application *app = p->fields.b;
    if (app == NULL) {
        failed(&p->call->error, MC_CLIENT_LOADING_NULL_REFERENCE, "mk actual App.q receiver", pc);
        *ok = false;
        return NULL;
    }
    mc_client_bitmap_font *font = app->font_renderer;
    *ok = mc_client_loading_call_font(p, slot, font, pc);
    return font;
}

static bool text_pair(mc_client_loading_screen *p, mc_client_display *display, int32_t width,
                      int32_t height, bool message) {
    mc_client_loading_error *e = &p->call->error;
    const size_t f = message ? 2u : 0u;
    const size_t s = message ? 3u : 1u;
    const int32_t measure_pc = message ? 524 : 480;
    const int32_t draw_pc = message ? 541 : 497;
    bool ok;
    mc_client_bitmap_font *outer = capture_font(p, f, message ? 504 : 460, &ok);
    if (!ok)
        return false;
    const mc_player_string *outer_text = message ? p->fields.a : p->fields.c;
    if (!mc_client_loading_call_string(p, s, outer_text, message ? 508 : 464))
        return false;
    mc_client_bitmap_font *inner = capture_font(p, f + 1u, message ? 517 : 473, &ok);
    if (!ok)
        return false;
    const mc_player_string *inner_text = message ? p->fields.a : p->fields.c;
    if (!mc_client_loading_call_string(p, s + 1u, inner_text, message ? 521 : 477))
        return false;
    if (inner == NULL) {
        failed(e, MC_CLIENT_LOADING_NULL_REFERENCE, "mk actual sj measure receiver", measure_pc);
        return false;
    }
    if (!mc_client_bitmap_font_uses_context(inner, p->classes, display)) {
        failed(e, MC_CLIENT_LOADING_INVALID_ARGUMENT, "mk measure native Font context", measure_pc);
        return false;
    }
    int32_t measured = 0, x = 0, y = 0;
    if (!font_child(e, mc_client_bitmap_font_measure(inner, inner_text, &measured, &e->font),
                    measure_pc))
        return false;
    (void)mc_java_i32_div(mc_java_i32_sub(width, measured), 2, &x);
    (void)mc_java_i32_div(height, 2, &y);
    y = mc_java_i32_sub(y, 4);
    y = message ? mc_java_i32_add(y, 8) : mc_java_i32_sub(y, 16);
    if (outer == NULL) {
        failed(e, MC_CLIENT_LOADING_NULL_REFERENCE, "mk actual sj draw receiver", draw_pc);
        return false;
    }
    if (!mc_client_bitmap_font_uses_context(outer, p->classes, display)) {
        failed(e, MC_CLIENT_LOADING_INVALID_ARGUMENT, "mk draw native Font context", draw_pc);
        return false;
    }
    return font_child(
        e, mc_client_bitmap_font_draw_shadow(outer, outer_text, x, y, UINT32_C(16777215), &e->font),
        draw_pc);
}

mc_client_loading_status
mc_client_loading_draw_body(mc_client_loading_screen *p, mc_client_tessellator *nw,
                            mc_client_display *display, const mc_scaled_resolution *resolution,
                            int32_t progress, mc_client_texture id, mc_client_loading_error *e) {
    const int32_t width = resolution->width, height = resolution->height;
    if (!mc_client_display_source_bind_id(display, id, &e->display))
        return device(e, "glBindTexture", "(II)V", 175);
    if (nw == NULL)
        return failed(e, MC_CLIENT_LOADING_NULL_REFERENCE, "mk actual nw begin receiver", 184);
    if (!graphics(e, mc_client_nw_begin_quads(nw, &e->graphics), 184) ||
        !graphics(e, mc_client_nw_rgb(nw, 4210752, &e->graphics), 191))
        return e->status;
    if (!fp(e, 199))
        return e->status;
    const double h1 = (double)height;
    volatile float hf1 = (float)height;
    volatile float hv1 = hf1 / 32.0f;
    if (!graphics(e, mc_client_nw_vertex_uv(nw, 0.0, h1, 0.0, 0.0, (double)hv1, &e->graphics), 209))
        return e->status;
    if (!fp(e, 216))
        return e->status;
    const double w2 = (double)width, h2 = (double)height;
    volatile float wf2 = (float)width;
    volatile float wv2 = wf2 / 32.0f;
    const double u2 = (double)wv2;
    volatile float hf2 = (float)height;
    volatile float hv2 = hf2 / 32.0f;
    if (!graphics(e, mc_client_nw_vertex_uv(nw, w2, h2, 0.0, u2, (double)hv2, &e->graphics), 235))
        return e->status;
    if (!fp(e, 242))
        return e->status;
    const double w3 = (double)width;
    volatile float wf3 = (float)width;
    volatile float wv3 = wf3 / 32.0f;
    if (!graphics(e, mc_client_nw_vertex_uv(nw, w3, 0.0, 0.0, (double)wv3, 0.0, &e->graphics),
                  253) ||
        !graphics(e, mc_client_nw_vertex_uv(nw, 0.0, 0.0, 0.0, 0.0, 0.0, &e->graphics), 263) ||
        !graphics(e, mc_client_nw_draw(nw, display, &e->graphics), 268))
        return e->status;
    if (progress >= 0) {
        int32_t x = 0, y = 0;
        (void)mc_java_i32_div(width, 2, &x);
        x = mc_java_i32_sub(x, 50);
        (void)mc_java_i32_div(height, 2, &y);
        y = mc_java_i32_add(y, 16);
        if (!mc_client_display_source_texture_enable(display, false, &e->display))
            return device(e, "glDisable", "(I)V", 305);
        if (!graphics(e, mc_client_nw_begin_quads(nw, &e->graphics), 310) ||
            !graphics(e, mc_client_nw_rgb(nw, 8421504, &e->graphics), 317) ||
            !vertex(nw, x, y, 324, 329, e) || !vertex(nw, x, mc_java_i32_add(y, 2), 336, 344, e) ||
            !vertex(nw, mc_java_i32_add(x, 100), mc_java_i32_add(y, 2), 354, 362, e) ||
            !vertex(nw, mc_java_i32_add(x, 100), y, 372, 377, e) ||
            !graphics(e, mc_client_nw_rgb(nw, 8454016, &e->graphics), 384) ||
            !vertex(nw, x, y, 391, 396, e) || !vertex(nw, x, mc_java_i32_add(y, 2), 403, 411, e) ||
            !vertex(nw, mc_java_i32_add(x, progress), mc_java_i32_add(y, 2), 420, 428, e) ||
            !vertex(nw, mc_java_i32_add(x, progress), y, 437, 442, e) ||
            !graphics(e, mc_client_nw_draw(nw, display, &e->graphics), 447))
            return e->status;
        if (!mc_client_display_source_texture_enable(display, true, &e->display))
            return device(e, "glEnable", "(I)V", 453);
    }
    if (!text_pair(p, display, width, height, false) || !text_pair(p, display, width, height, true))
        return e->status;
    e->draw_prefix_completed = true;
    /* Source Display.update544 is OUTSIDE the Exception handler547..550.
     * The actual canonical SDK child owns the one event port and device poll.
     * No component loop dispatch, unconditional swap or second pump occurs. */
    const mc_client_sdk_update_status status =
        mc_client_sdk_display_update(mc_client_display_update_owner(display), &p->call->update);
    e->swapped = p->call->update.swapped;
    e->messages_pumped = p->call->update.messages_pumped;
    if (status != MC_CLIENT_SDK_UPDATE_OK) {
        p->call->update_failed = true;
        e->update = &p->call->update;
        e->child =
            (mc_client_loading_site){p->call->update.site.class_name, p->call->update.site.method,
                                     p->call->update.site.descriptor, p->call->update.site.pc};
        return failed(e,
                      p->call->update.input.status == MC_CLIENT_SDK_INPUT_UNSUPPORTED_FP
                          ? MC_CLIENT_LOADING_UNSUPPORTED_FP
                      : status == MC_CLIENT_SDK_UPDATE_BUSY ? MC_CLIENT_LOADING_BUSY
                      : status == MC_CLIENT_SDK_UPDATE_INVALID_PHASE
                          ? MC_CLIENT_LOADING_INVALID_PHASE
                      : status == MC_CLIENT_SDK_UPDATE_UNSUPPORTED_PROFILE
                          ? MC_CLIENT_LOADING_UNIMPLEMENTED_DEPENDENCY
                          : MC_CLIENT_LOADING_DISPLAY_FAILED,
                      "mk actual SDK Display.update child", 544);
    }
    e->update_completed = true;
    /* Actual Thread.yield547 alone is inside catch(Exception)547..550.
     * C17 thrd_yield is a real void operation, with no error/Java exception
     * channel. Do not swallow native update/policy failures or invent a caught
     * Exception. No FP work follows this external scheduling boundary. */
    thrd_yield();
    e->yield_completed = true;
    return MC_CLIENT_LOADING_OK;
}
