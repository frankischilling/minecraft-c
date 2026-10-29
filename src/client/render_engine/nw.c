#include "../display/source_surface.h"
#include "context_internal.h"
#include "internal.h"
#include "mc/core/java/java.h"
#include "nw_internal.h"
#include <emmintrin.h>
#include <string.h>
#include <xmmintrin.h>

static mc_client_graphics_site site(const char *method, const char *descriptor, int32_t pc) {
    return (mc_client_graphics_site){"nw", method, descriptor, pc};
}

static bool draw(mc_client_tessellator *, mc_client_display *, mc_client_graphics_error *);

static bool enter(mc_client_tessellator *o, mc_client_graphics_error *e) {
    *e = (mc_client_graphics_error){0};
    if (o == NULL || o->classes->nw.a != o ||
        o->classes->nw.phase != MC_CLIENT_RENDER_ENGINE_READY) {
        mc_client_graphics_fail(
            e, o == NULL ? MC_CLIENT_GRAPHICS_NULL_REFERENCE : MC_CLIENT_GRAPHICS_INVALID_PHASE,
            "native actual nw receiver", (mc_client_graphics_site){0}, true);
        return false;
    }
    if (o->active || o->classes->active_source_calls != 0 ||
        o->classes->resource_retirement_started) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_BUSY,
                                "native nw source operation active/retiring",
                                (mc_client_graphics_site){0}, true);
        return false;
    }
    if (mc_client_render_engine_classes_retain(o->classes) != MC_CLIENT_RENDER_ENGINE_OK) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_LIMIT, "native nw per-call class hold",
                                (mc_client_graphics_site){0}, true);
        return false;
    }
    o->active = true;
    ++o->classes->active_source_calls;
    return true;
}

static bool leave(mc_client_tessellator *o, bool ok, mc_client_graphics_error *e) {
    mc_client_graphics_error cleanup = {0};
    const bool released = mc_client_render_context_unbind_empty(o->classes, &cleanup);
    --o->classes->active_source_calls;
    o->active = false;
    (void)mc_client_render_engine_classes_release(o->classes);
    if (!released) {
        if (ok)
            *e = cleanup;
        else {
            e->cleanup_display = cleanup.display;
            e->cleanup_failed = true;
        }
    }
    return ok && released;
}

static bool reset(mc_client_tessellator *o, mc_client_graphics_error *e) {
    o->fields.h = 0;
    o->last_store_pc = 2;
    mc_client_direct_byte_buffer *bytes = (mc_client_direct_byte_buffer *)(void *)o->fields.d;
    if (bytes == NULL) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_NULL_REFERENCE,
                                "nw actual ByteBuffer.clear receiver", site("d", "()V", 9), false);
        return false;
    }
    bytes->position = 0;
    bytes->limit = bytes->capacity;
    o->fields.o = 0;
    o->last_store_pc = 15;
    o->fields.p = 0;
    o->last_store_pc = 20;
    return true;
}

bool mc_client_nw_begin(mc_client_tessellator *o, int32_t mode, mc_client_graphics_error *e) {
    if (!enter(o, e))
        return false;
    bool ok = false;
    if (o->fields.w) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_STATE, "Already tesselating!",
                                site("a", "(I)V", 16), false);
        goto done;
    }
    o->fields.w = true;
    o->last_store_pc = 19;
    if (!reset(o, e))
        goto done;
    o->fields.r = mode;
    o->last_store_pc = 28;
    o->fields.n = false;
    o->last_store_pc = 33;
    o->fields.l = false;
    o->last_store_pc = 38;
    o->fields.m = false;
    o->last_store_pc = 43;
    o->fields.q = false;
    o->last_store_pc = 48;
    ok = true;
done:
    return leave(o, ok, e);
}

bool mc_client_nw_begin_quads(mc_client_tessellator *o, mc_client_graphics_error *e) {
    return mc_client_nw_begin(o, 7, e);
}

bool mc_client_nw_uv(mc_client_tessellator *o, double u, double v, mc_client_graphics_error *e) {
    if (!enter(o, e))
        return false;
    o->fields.m = true;
    o->last_store_pc = 2;
    o->fields.i = u;
    o->last_store_pc = 7;
    o->fields.j = v;
    o->last_store_pc = 12;
    return leave(o, true, e);
}

bool mc_client_nw_translation(mc_client_tessellator *o, double x, double y, double z,
                              mc_client_graphics_error *e) {
    if (!enter(o, e))
        return false;
    memcpy(&o->fields.s, &x, sizeof(x));
    o->last_store_pc = 2;
    memcpy(&o->fields.t, &y, sizeof(y));
    o->last_store_pc = 7;
    memcpy(&o->fields.u, &z, sizeof(z));
    o->last_store_pc = 13;
    return leave(o, true, e);
}

static void color(mc_client_tessellator *o, int32_t r, int32_t g, int32_t b, int32_t a) {
    if (!o->fields.q) {
        if (r > 255)
            r = 255;
        if (g > 255)
            g = 255;
        if (b > 255)
            b = 255;
        if (a > 255)
            a = 255;
        if (r < 0)
            r = 0;
        if (g < 0)
            g = 0;
        if (b < 0)
            b = 0;
        if (a < 0)
            a = 0;
        o->fields.l = true;
        o->last_store_pc = 82;
        /* Admitted x86-64 is little-endian; packed bytes are source RGBA. */
        const uint32_t packed =
            ((uint32_t)a << 24u) | ((uint32_t)b << 16u) | ((uint32_t)g << 8u) | (uint32_t)r;
        memcpy(&o->fields.k, &packed, sizeof(packed));
        o->last_store_pc = 112;
    }
}

bool mc_client_nw_color(mc_client_tessellator *o, int32_t r, int32_t g, int32_t b, int32_t a,
                        mc_client_graphics_error *e) {
    if (!enter(o, e))
        return false;
    color(o, r, g, b, a);
    return leave(o, true, e);
}

bool mc_client_nw_color_float(mc_client_tessellator *o, float r, float g, float b,
                              mc_client_graphics_error *e) {
    if (!enter(o, e))
        return false;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_UNSUPPORTED_FP,
                                "nw float color floating environment", site("a", "(FFF)V", 2),
                                true);
        return leave(o, false, e);
    }
    /* Actual a(FFF): three ordered fmul/f2i pairs, then a(III)16 supplies
     * alpha255 to the SAME a(IIII)7 body. Even q=true does these conversions. */
    const float red = _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(r), _mm_set_ss(255.0f)));
    const int32_t ir = mc_java_float_to_i32(red);
    const float green = _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(g), _mm_set_ss(255.0f)));
    const int32_t ig = mc_java_float_to_i32(green);
    const float blue = _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(b), _mm_set_ss(255.0f)));
    const int32_t ib = mc_java_float_to_i32(blue);
    color(o, ir, ig, ib, 255);
    return leave(o, true, e);
}

bool mc_client_nw_rgb(mc_client_tessellator *o, int32_t rgb, mc_client_graphics_error *e) {
    const uint32_t bits = (uint32_t)rgb;
    return mc_client_nw_color(o, (int32_t)((bits >> 16u) & 255u), (int32_t)((bits >> 8u) & 255u),
                              (int32_t)(bits & 255u), 255, e);
}

static bool store(mc_client_tessellator *o, int32_t index, int32_t value, int32_t pc,
                  mc_client_graphics_error *e) {
    if (o->fields.g == NULL) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_NULL_REFERENCE, "nw actual vertex array",
                                site("a", "(DDD)V", pc), false);
        return false;
    }
    if (index < 0 || index >= o->fields.B) {
        e->actual_index = index;
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_ARRAY_INDEX, "nw actual vertex array index",
                                site("a", "(DDD)V", pc), false);
        return false;
    }
    ((int32_t *)(void *)o->fields.g)[index] = value;
    return true;
}

static bool copy(mc_client_tessellator *o, int32_t target, int32_t from, int32_t read_pc,
                 int32_t write_pc, mc_client_graphics_error *e) {
    if (o->fields.g == NULL || from < 0 || from >= o->fields.B) {
        e->actual_index = from;
        mc_client_graphics_fail(e,
                                o->fields.g == NULL ? MC_CLIENT_GRAPHICS_NULL_REFERENCE
                                                    : MC_CLIENT_GRAPHICS_ARRAY_INDEX,
                                "nw actual duplicated vertex read", site("a", "(DDD)V", read_pc),
                                false);
        return false;
    }
    return store(o, target, o->fields.g[from], write_pc, e);
}

static bool fp(mc_client_graphics_error *e, int32_t pc) {
    if ((_mm_getcsr() & 0xffc0u) == 0x1f80u)
        return true;
    mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_UNSUPPORTED_FP, "nw reached strict x86-64 FP",
                            site("a", "(DDD)V", pc), true);
    return false;
}

static int32_t narrowed_bits(double value) {
    const __m128 result = _mm_cvtsd_ss(_mm_setzero_ps(), _mm_set_sd(value));
    const float narrowed = _mm_cvtss_f32(result);
    int32_t bits;
    memcpy(&bits, &narrowed, sizeof(bits));
    return bits;
}

static int32_t added_narrowed_bits(double left, double right) {
    const __m128d sum = _mm_add_sd(_mm_set_sd(left), _mm_set_sd(right));
    const __m128 result = _mm_cvtsd_ss(_mm_setzero_ps(), sum);
    const float narrowed = _mm_cvtss_f32(result);
    int32_t bits;
    memcpy(&bits, &narrowed, sizeof(bits));
    return bits;
}

static bool vertex(mc_client_tessellator *o, double x, double y, double z,
                   mc_client_graphics_error *e) {
    mc_client_nw_fields *f = &o->fields;
    f->p = mc_java_i32_add(f->p, 1);
    o->last_store_pc = 7;
    if (f->r == 7 && o->classes->nw.b && f->p % 4 == 0) {
        for (int32_t i = 0; i < 2; ++i) {
            const int32_t distance = mc_java_i32_mul(8, mc_java_i32_sub(3, i));
            const int32_t from = mc_java_i32_sub(f->o, distance);
            if (f->m && (!copy(o, mc_java_i32_add(f->o, 3), mc_java_i32_add(from, 3), 82, 83, e) ||
                         !copy(o, mc_java_i32_add(f->o, 4), mc_java_i32_add(from, 4), 107, 108, e)))
                return false;
            if (f->l && !copy(o, mc_java_i32_add(f->o, 5), mc_java_i32_add(from, 5), 139, 140, e))
                return false;
            if (!copy(o, f->o, from, 164, 165, e) ||
                !copy(o, mc_java_i32_add(f->o, 1), mc_java_i32_add(from, 1), 189, 190, e) ||
                !copy(o, mc_java_i32_add(f->o, 2), mc_java_i32_add(from, 2), 214, 215, e))
                return false;
            f->h = mc_java_i32_add(f->h, 1);
            o->last_store_pc = 223;
            f->o = mc_java_i32_add(f->o, 8);
            o->last_store_pc = 234;
        }
    }
    if (f->m) {
        if (!fp(e, 264) || !store(o, mc_java_i32_add(f->o, 3), narrowed_bits(f->i), 268, e) ||
            !store(o, mc_java_i32_add(f->o, 4), narrowed_bits(f->j), 287, e))
            return false;
    }
    if (f->l && !store(o, mc_java_i32_add(f->o, 5), f->k, 309, e))
        return false;
    if (f->n && !store(o, mc_java_i32_add(f->o, 6), f->v, 332, e))
        return false;
    if (!fp(e, 348) || !store(o, f->o, added_narrowed_bits(x, f->s), 353, e) ||
        !store(o, mc_java_i32_add(f->o, 1), added_narrowed_bits(y, f->t), 374, e) ||
        !store(o, mc_java_i32_add(f->o, 2), added_narrowed_bits(z, f->u), 396, e))
        return false;
    f->o = mc_java_i32_add(f->o, 8);
    o->last_store_pc = 405;
    f->h = mc_java_i32_add(f->h, 1);
    o->last_store_pc = 415;
    if (f->h % 4 == 0 && f->o >= mc_java_i32_sub(f->B, 32)) {
        if (!draw(o, o->classes->bound_display, e)) {
            e->parent = site("a", "(DDD)V", 442);
            return false;
        }
        f->w = true;
        o->last_store_pc = 447;
    }
    return true;
}

bool mc_client_nw_vertex(mc_client_tessellator *o, double x, double y, double z,
                         mc_client_graphics_error *e) {
    if (!enter(o, e))
        return false;
    const bool ok = vertex(o, x, y, z, e);
    return leave(o, ok, e);
}

bool mc_client_nw_vertex_uv(mc_client_tessellator *o, double x, double y, double z, double u,
                            double v, mc_client_graphics_error *e) {
    if (!enter(o, e))
        return false;
    o->fields.m = true;
    o->last_store_pc = 2;
    o->fields.i = u;
    o->last_store_pc = 7;
    o->fields.j = v;
    o->last_store_pc = 12;
    const bool ok = vertex(o, x, y, z, e);
    if (!ok)
        e->parent = site("a", "(DDDDD)V", 13);
    return leave(o, ok, e);
}

static bool device(mc_client_graphics_error *e, int32_t pc) {
    mc_client_graphics_fail(e,
                            e->display.status == MC_CLIENT_DISPLAY_BUSY ? MC_CLIENT_GRAPHICS_BUSY
                            : e->display.status == MC_CLIENT_DISPLAY_UNSUPPORTED_FP
                                ? MC_CLIENT_GRAPHICS_UNSUPPORTED_FP
                            : e->display.status == MC_CLIENT_DISPLAY_UNSUPPORTED_GL
                                ? MC_CLIENT_GRAPHICS_UNIMPLEMENTED_DEPENDENCY
                                : MC_CLIENT_GRAPHICS_DISPLAY_FAILED,
                            e->display.operation, site("a", "()V", pc), true);
    return false;
}

static bool bind(mc_client_tessellator *o, mc_client_display *display, int32_t pc,
                 mc_client_graphics_error *e) {
    return mc_client_render_context_bind(o->classes, display, site("a", "()V", pc), e);
}

static bool draw(mc_client_tessellator *o, mc_client_display *display,
                 mc_client_graphics_error *e) {
    mc_client_nw_fields *f = &o->fields;
    if (!f->w) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_STATE, "Not tesselating!",
                                site("a", "()V", 16), false);
        return false;
    }
    f->w = false;
    o->last_store_pc = 19;
    if (f->h > 0) {
        mc_client_direct_int_buffer *ints = (mc_client_direct_int_buffer *)(void *)f->e;
        if (!mc_client_ge_int_clear(ints, site("a", "()V", 33), e))
            return false;
        if (f->g == NULL) {
            mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_NULL_REFERENCE,
                                    "nw IntBuffer.put actual array", site("a", "()V", 50), false);
            return false;
        }
        if (f->o < 0 || f->o > f->B) {
            mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_ARRAY_INDEX,
                                    "nw IntBuffer.put actual array span", site("a", "()V", 50),
                                    false);
            return false;
        }
        if ((size_t)f->o > ints->limit - ints->position) {
            mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_BUFFER_OVERFLOW,
                                    "nw IntBuffer.put actual span", site("a", "()V", 50), false);
            return false;
        }
        memcpy(ints->bytes->data + ints->position * sizeof(int32_t), f->g,
               (size_t)f->o * sizeof(int32_t));
        ints->position += (size_t)f->o;
        mc_client_direct_byte_buffer *bytes = (mc_client_direct_byte_buffer *)(void *)f->d;
        if (bytes == NULL) {
            mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_NULL_REFERENCE,
                                    "nw ByteBuffer.position actual receiver", site("a", "()V", 59),
                                    false);
            return false;
        }
        bytes->position = 0;
        const int32_t limit = mc_java_i32_mul(f->o, 4);
        if (limit < 0 || (size_t)limit > bytes->capacity) {
            mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_ARGUMENT,
                                    "nw ByteBuffer.limit actual argument", site("a", "()V", 73),
                                    false);
            return false;
        }
        bytes->limit = (size_t)limit;
        if (f->x) {
            mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_UNIMPLEMENTED_DEPENDENCY,
                                    "nw nondefault source VBO upload", site("a", "()V", 112),
                                    false);
            return false;
        }
        mc_client_direct_float_buffer *floats = (mc_client_direct_float_buffer *)(void *)f->f;
        if (f->m) {
            if (floats == NULL) {
                mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_NULL_REFERENCE,
                                        "nw FloatBuffer.position actual receiver",
                                        site("a", "()V", 160), false);
                return false;
            }
            floats->position = 3;
            if (!bind(o, display, 171, e))
                return false;
            if (!mc_client_display_source_pointer(o->classes->context_lease,
                                                  MC_CLIENT_SOURCE_UV_ARRAY, floats->bytes,
                                                  floats->position * sizeof(float), &e->display))
                return device(e, 171);
            if (!mc_client_display_source_array(display, MC_CLIENT_SOURCE_UV_ARRAY, true,
                                                &e->display))
                return device(e, 176);
        }
        if (f->l) {
            bytes->position = 20;
            if (!bind(o, display, 226, e))
                return false;
            if (!mc_client_display_source_pointer(o->classes->context_lease,
                                                  MC_CLIENT_SOURCE_COLOR_ARRAY, bytes,
                                                  bytes->position, &e->display))
                return device(e, 226);
            if (!mc_client_display_source_array(display, MC_CLIENT_SOURCE_COLOR_ARRAY, true,
                                                &e->display))
                return device(e, 231);
        }
        if (f->n) {
            bytes->position = 24;
            if (!bind(o, display, 278, e))
                return false;
            if (!mc_client_display_source_pointer(o->classes->context_lease,
                                                  MC_CLIENT_SOURCE_NORMAL_ARRAY, bytes,
                                                  bytes->position, &e->display))
                return device(e, 278);
            if (!mc_client_display_source_array(display, MC_CLIENT_SOURCE_NORMAL_ARRAY, true,
                                                &e->display))
                return device(e, 283);
        }
        if (floats == NULL) {
            mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_NULL_REFERENCE,
                                    "nw FloatBuffer.position actual receiver",
                                    site("a", "()V", 311), false);
            return false;
        }
        floats->position = 0;
        if (!bind(o, display, 322, e))
            return false;
        if (!mc_client_display_source_pointer(o->classes->context_lease,
                                              MC_CLIENT_SOURCE_VERTEX_ARRAY, floats->bytes,
                                              floats->position * sizeof(float), &e->display))
            return device(e, 322);
        if (!mc_client_display_source_array(display, MC_CLIENT_SOURCE_VERTEX_ARRAY, true,
                                            &e->display))
            return device(e, 327);
        const int32_t mode = f->r == 7 && o->classes->nw.b ? 4 : f->r;
        if (!mc_client_display_source_batch(o->classes->context_lease, bytes, bytes->data,
                                            bytes->limit, f->h, mode, &e->display))
            return device(e, f->r == 7 && o->classes->nw.b ? 351 : 366);
        if (!mc_client_display_source_array(display, MC_CLIENT_SOURCE_VERTEX_ARRAY, false,
                                            &e->display))
            return device(e, 371);
        if (f->m &&
            !mc_client_display_source_array(display, MC_CLIENT_SOURCE_UV_ARRAY, false, &e->display))
            return device(e, 383);
        if (f->l && !mc_client_display_source_array(display, MC_CLIENT_SOURCE_COLOR_ARRAY, false,
                                                    &e->display))
            return device(e, 395);
        if (f->n && !mc_client_display_source_array(display, MC_CLIENT_SOURCE_NORMAL_ARRAY, false,
                                                    &e->display))
            return device(e, 407);
    }
    return reset(o, e);
}

bool mc_client_nw_draw(mc_client_tessellator *o, mc_client_display *display,
                       mc_client_graphics_error *e) {
    if (!enter(o, e))
        return false;
    const bool ok = draw(o, display, e);
    return leave(o, ok, e);
}
