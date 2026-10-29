#include "render_batch_internal.h"

#include "../../entity/player/tick_math_internal.h"
#include "../display/source_surface.h"

#include <xmmintrin.h>

static mc_client_graphics_site site(const char *method, const char *descriptor, unsigned int pc) {
    return (mc_client_graphics_site){"tg", method, descriptor, (int32_t)pc};
}

static bool fail(mc_client_render_global *o, mc_client_render_batch *b,
                 mc_client_render_global_error *e, mc_client_render_global_status status,
                 const char *operation, const char *method, const char *descriptor, unsigned int pc,
                 bool native) {
    if (b != NULL)
        b->state.source_pc = pc;
    (void)mc_render_global_source_fail(e, status, operation, "tg", method, descriptor, pc, native);
    if (o != NULL)
        o->failure = *e;
    return false;
}

static bool admit(mc_client_render_global *o, mc_client_render_batch *b,
                  mc_client_render_global_error *e, const char *method, const char *descriptor) {
    if (e == NULL)
        return false;
    if (o == NULL)
        return fail(o, b, e, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT, "TG native enclosing owner",
                    method, descriptor, 0, true);
    return b != NULL || fail(o, b, e, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual TG receiver",
                             method, descriptor, 0, false);
}

static bool graphics_result(mc_client_render_global *o, mc_client_render_batch *b,
                            mc_client_render_global_error *e, bool ok, const char *method,
                            const char *descriptor, unsigned int pc) {
    if (ok)
        return true;
    const mc_client_graphics_error *g = &e->graphics;
    e->child = g->site;
    return fail(
        o, b, e,
        g->status == MC_CLIENT_GRAPHICS_NULL_REFERENCE ? MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
        : g->status == MC_CLIENT_GRAPHICS_BUSY         ? MC_CLIENT_RENDER_GLOBAL_BUSY
        : g->status == MC_CLIENT_GRAPHICS_LIMIT        ? MC_CLIENT_RENDER_GLOBAL_LIMIT
        : g->status == MC_CLIENT_GRAPHICS_ALLOCATION_FAILED
            ? MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED
        : g->status == MC_CLIENT_GRAPHICS_UNSUPPORTED_FP ? MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP
        : g->status == MC_CLIENT_GRAPHICS_UNIMPLEMENTED_DEPENDENCY
            ? MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY
            : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
        g->operation, method, descriptor, pc, g->native_policy);
}

static bool display_result(mc_client_render_global *o, mc_client_render_batch *b,
                           mc_client_render_global_error *e, bool ok, unsigned int pc) {
    if (ok)
        return true;
    return fail(o, b, e,
                e->display.status == MC_CLIENT_DISPLAY_BUSY ? MC_CLIENT_RENDER_GLOBAL_BUSY
                : e->display.status == MC_CLIENT_DISPLAY_UNSUPPORTED_FP
                    ? MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP
                : e->display.status == MC_CLIENT_DISPLAY_UNSUPPORTED_GL
                    ? MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY
                    : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                e->display.operation, "a", "()V", pc, true);
}

static bool fp(mc_client_render_global *o, mc_client_render_batch *b,
               mc_client_render_global_error *e, const char *descriptor, unsigned int pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(o, b, e, MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP,
                "TG reached strict floating-point prerequisite", "a", descriptor, pc, true);
}

bool mc_render_batch_construct(mc_client_render_global *o, mc_client_render_batch *batch,
                               unsigned int enclosing_pc) {
    batch->state.phase = MC_CLIENT_RENDER_GLOBAL_CONSTRUCTING;
    /* Actual Object constructor1, then ge.d(65536)7 on the SAME graph arena. */
    mc_client_direct_int_buffer *buffer = NULL;
    if (!mc_client_ge_d(o->classes, &o->arena, 65536,
                        (mc_client_graphics_site){"tg", "<init>", "()V", 7}, &buffer,
                        &o->failure.graphics)) {
        batch->state.phase = MC_CLIENT_RENDER_GLOBAL_FAILED;
        return mc_render_global_graphics(o, enclosing_pc);
    }
    batch->state.g = buffer;
    batch->state.last_store_pc = 10;
    batch->state.h = false;
    batch->state.last_store_pc = 15;
    batch->state.i = false;
    batch->state.last_store_pc = 20;
    batch->state.phase = MC_CLIENT_RENDER_GLOBAL_READY; /* Actual return23. */
    batch->state.source_pc = 23;
    return true;
}

bool mc_render_batch_configure(mc_client_render_global *o, mc_client_render_batch *b, int32_t x,
                               int32_t y, int32_t z, double camera_x, double camera_y,
                               double camera_z, mc_client_render_global_error *e) {
    static const char *const descriptor = "(IIIDDD)V";
    if (!admit(o, b, e, "a", descriptor))
        return false;
    b->state.h = true;
    b->state.last_store_pc = 2;
    if (!graphics_result(o, b, e,
                         mc_client_ge_int_clear((mc_client_direct_int_buffer *)b->state.g,
                                                site("a", descriptor, 9), &e->graphics),
                         "a", descriptor, 9))
        return false;
    b->state.a = x;
    b->state.last_store_pc = 15;
    b->state.b = y;
    b->state.last_store_pc = 20;
    b->state.c = z;
    b->state.last_store_pc = 25;
    if (!fp(o, b, e, descriptor, 31))
        return false;
    b->state.d = at_narrow(camera_x);
    b->state.last_store_pc = 32;
    b->state.e = at_narrow(camera_y);
    b->state.last_store_pc = 39;
    b->state.f = at_narrow(camera_z);
    b->state.last_store_pc = 46;
    b->state.source_pc = 49;
    return true;
}

bool mc_render_batch_matches(mc_client_render_global *o, mc_client_render_batch *b, int32_t x,
                             int32_t y, int32_t z, bool *out, mc_client_render_global_error *e) {
    if (!admit(o, b, e, "a", "(III)Z"))
        return false;
    if (out == NULL)
        return fail(o, b, e, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT, "TG native match output",
                    "a", "(III)Z", 0, true);
    if (!b->state.h) {
        b->state.source_pc = 8;
        *out = false;
    } else {
        *out = x == b->state.a && y == b->state.b && z == b->state.c;
        b->state.source_pc = 38;
    }
    return true;
}

bool mc_render_batch_reset(mc_client_render_global *o, mc_client_render_batch *b,
                           mc_client_render_global_error *e) {
    if (!admit(o, b, e, "b", "()V"))
        return false;
    b->state.h = false;
    b->state.last_store_pc = 2;
    b->state.i = false;
    b->state.last_store_pc = 7;
    b->state.source_pc = 10;
    return true;
}

bool mc_render_batch_append(mc_client_render_global *o, mc_client_render_batch *b, int32_t list,
                            mc_client_render_global_error *e) {
    if (!admit(o, b, e, "a", "(I)V"))
        return false;
    if (!graphics_result(o, b, e,
                         mc_client_ge_int_put((mc_client_direct_int_buffer *)b->state.g, list,
                                              site("a", "(I)V", 5), &e->graphics),
                         "a", "(I)V", 5))
        return false;
    /* put returned on this exact buffer; no callback or source store changes g. */
    if (mc_client_ge_int_remaining(b->state.g) == 0 && !mc_render_batch_draw(o, b, e)) {
        e->parent = site("a", "(I)V", 20);
        o->failure = *e;
        return false;
    }
    b->state.source_pc = 23;
    return true;
}

bool mc_render_batch_draw(mc_client_render_global *o, mc_client_render_batch *b,
                          mc_client_render_global_error *e) {
    if (!admit(o, b, e, "a", "()V"))
        return false;
    if (!b->state.h) {
        b->state.source_pc = 7;
        return true;
    }
    if (!b->state.i) {
        if (!graphics_result(o, b, e,
                             mc_client_ge_int_flip((mc_client_direct_int_buffer *)b->state.g,
                                                   site("a", "()V", 19), &e->graphics),
                             "a", "()V", 19))
            return false;
        b->state.i = true;
        b->state.last_store_pc = 25;
    }
    if (b->state.g == NULL)
        return fail(o, b, e, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                    "actual IntBuffer.remaining receiver", "a", "()V", 32, false);
    if (mc_client_ge_int_remaining(b->state.g) > 0) {
        if (!display_result(o, b, e, mc_client_display_source_push(o->display, &e->display), 38))
            return false;
        b->state.matrix_pushed = true;
        /* Source i2f/fsub happen AFTER the successful Push38. */
        if (!fp(o, b, e, "()V", 45))
            return false;
        const float x = at_fsub((float)b->state.a, b->state.d);
        const float y = at_fsub((float)b->state.b, b->state.e);
        const float z = at_fsub((float)b->state.c, b->state.f);
        if (!display_result(
                o, b, e, mc_client_display_source_translate(o->display, x, y, z, &e->display), 71))
            return false;
        if (!graphics_result(o, b, e,
                             mc_client_ge_call_lists(o->classes, o->display, b->state.g,
                                                     site("a", "()V", 78), &e->graphics),
                             "a", "()V", 78))
            return false;
        if (!display_result(o, b, e, mc_client_display_source_pop(o->display, &e->display), 81))
            return false;
        b->state.matrix_pushed = false;
    }
    b->state.source_pc = 84;
    return true;
}
