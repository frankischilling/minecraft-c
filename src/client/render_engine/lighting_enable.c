#include "lighting.h"

#include "../../entity/classes/classes_internal.h"
#include "../display/fog_surface.h"
#include "../display/source_surface.h"
#include "internal.h"

#include <emmintrin.h>

static mc_client_graphics_site u_site(int32_t pc) {
    return (mc_client_graphics_site){"u", "b", "()V", pc};
}

static bool fail(mc_client_lighting_error *e, mc_client_lighting_status status,
                 const char *operation, int32_t pc, bool native_policy) {
    e->status = status;
    e->operation = operation;
    e->site = u_site(pc);
    e->native_policy = native_policy;
    return false;
}

static bool graphics_failed(mc_client_lighting_error *e, int32_t pc) {
    e->graphics.entry = e->caller;
    return fail(e, MC_CLIENT_LIGHTING_GRAPHICS_FAILED, e->graphics.operation, pc,
                e->graphics.native_policy);
}

static bool display_result(mc_client_lighting_error *e, bool ok, int32_t pc) {
    if (ok) {
        ++e->completed_display_calls;
        e->graphics.retained_count = e->completed_display_calls;
        return true;
    }
    mc_client_graphics_fail(&e->graphics,
                            e->graphics.display.status == MC_CLIENT_DISPLAY_BUSY
                                ? MC_CLIENT_GRAPHICS_BUSY
                            : e->graphics.display.status == MC_CLIENT_DISPLAY_UNSUPPORTED_FP
                                ? MC_CLIENT_GRAPHICS_UNSUPPORTED_FP
                                : MC_CLIENT_GRAPHICS_DISPLAY_FAILED,
                            e->graphics.display.operation, u_site(pc), true);
    return graphics_failed(e, pc);
}

static bool fill(mc_client_render_engine_classes *c, float x, float y, float z, float w, int32_t pc,
                 bool doubles, mc_client_direct_float_buffer **out, mc_client_lighting_error *e) {
    const char *descriptor = "(FFFF)Ljava/nio/FloatBuffer;";
    e->site = u_site(pc);
    e->helper = doubles ? (mc_client_graphics_site){"u", "a", "(DDDD)Ljava/nio/FloatBuffer;", 10}
                        : (mc_client_graphics_site){0};
    const float values[4] = {x, y, z, w};
    const int32_t puts[4] = {11, 15, 19, 23};
    e->child = (mc_client_graphics_site){"u", "a", descriptor, 3};
    if (!mc_client_ge_float_clear(c->u_a, e->child, &e->graphics))
        return graphics_failed(e, pc);
    mc_client_direct_float_buffer *buffer = c->u_a; /* Actual getstatic7. */
    for (size_t i = 0; i < 4; ++i) {
        e->child.pc = puts[i];
        if (!mc_client_ge_float_put(buffer, values[i], e->child, &e->graphics))
            return graphics_failed(e, pc);
    }
    e->child.pc = 30;
    if (!mc_client_ge_float_flip(c->u_a, e->child, &e->graphics))
        return graphics_failed(e, pc);
    *out = c->u_a; /* Actual getstatic34, areturn37. */
    ++e->completed_buffer_fills;
    e->child = (mc_client_graphics_site){0};
    e->helper = (mc_client_graphics_site){0};
    return true;
}

static float narrow(double value) {
    return _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), _mm_set_sd(value)));
}

static bool upload(mc_client_display *display, unsigned int light, int32_t pname,
                   mc_client_direct_float_buffer *buffer, int32_t pc, bool model,
                   mc_client_lighting_error *e) {
    void *storage = NULL;
    size_t remaining = 0;
    if (!mc_client_ge_float_writable_remaining(buffer, &storage, &remaining, u_site(pc),
                                               &e->graphics))
        return graphics_failed(e, pc);
    return display_result(
        e,
        model ? mc_client_display_source_light_model(display, pname, storage, remaining,
                                                     &e->graphics.display)
              : mc_client_display_source_light_parameter(display, light, pname, storage, remaining,
                                                         &e->graphics.display),
        pc);
}

static bool vector_failed(mc_client_lighting_error *e, int32_t pc, const char *method,
                          const char *descriptor) {
    e->child = (mc_client_graphics_site){"bt", method, descriptor, e->vector.bytecode};
    return fail(e, MC_CLIENT_LIGHTING_VECTOR_FAILED, e->vector.operation, pc, true);
}

/* The canonical normalization kernel has no MathHelper class input. This
 * source-aware adapter puts its actual active use between BT.c arithmetic28
 * and in.a29, then uses the SAME pooled factory for the returned BT. */
static bool normalize(mc_entity_classes *entities, const mc_entity_vector *raw,
                      mc_math_classes *math, bool *math_held, mc_entity_vector **out, int32_t pc,
                      mc_client_lighting_error *e) {
    e->site = u_site(pc);
    e->child = (mc_client_graphics_site){"bt", "c", "()Lbt;", 8};
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        e->vector = (mc_entity_vector_error){.status = MC_ENTITY_UNSUPPORTED_FP,
                                             .operation = "vector normalize FP admission",
                                             .bytecode = 8};
        return vector_failed(e, pc, "c", "()Lbt;");
    }
    volatile double xx = _mm_cvtsd_f64(_mm_mul_sd(_mm_set_sd(raw->x), _mm_set_sd(raw->x)));
    volatile double yy = _mm_cvtsd_f64(_mm_mul_sd(_mm_set_sd(raw->y), _mm_set_sd(raw->y)));
    volatile double xy = _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd(xx), _mm_set_sd(yy)));
    volatile double zz = _mm_cvtsd_f64(_mm_mul_sd(_mm_set_sd(raw->z), _mm_set_sd(raw->z)));
    volatile double square = _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd(xy), _mm_set_sd(zz)));
    e->child.pc = 29;
    if (!*math_held) {
        if (mc_math_classes_retain(math, &e->math) != MC_MATH_CLASS_OK)
            return fail(e, MC_CLIENT_LIGHTING_MATH_FAILED, e->math.operation, pc, true);
        *math_held = true;
    }
    if (mc_math_classes_ensure(math, &e->math) != MC_MATH_CLASS_OK)
        return fail(e, MC_CLIENT_LIGHTING_MATH_FAILED, e->math.operation, pc,
                    e->math.source_pc < 0);
    const __m128d root = _mm_sqrt_sd(_mm_setzero_pd(), _mm_set_sd(square));
    volatile float root32 = _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), root));
    const double length = _mm_cvtsd_f64(_mm_cvtss_sd(_mm_setzero_pd(), _mm_set_ss(root32)));
    const bool zero = length < 1.0e-4;
    const double x = zero ? 0 : _mm_cvtsd_f64(_mm_div_sd(_mm_set_sd(raw->x), _mm_set_sd(length)));
    const double y = zero ? 0 : _mm_cvtsd_f64(_mm_div_sd(_mm_set_sd(raw->y), _mm_set_sd(length)));
    const double z = zero ? 0 : _mm_cvtsd_f64(_mm_div_sd(_mm_set_sd(raw->z), _mm_set_sd(length)));
    if (mc_entity_vector_pool_get(entities, x, y, z, out, &e->vector) != MC_ENTITY_OK) {
        e->vector.factory = (mc_entity_error){.status = e->vector.status,
                                              .operation = e->vector.operation,
                                              .bytecode = e->vector.bytecode};
        e->vector.operation = "vector normalize";
        e->vector.bytecode = zero ? 45 : 67;
        return vector_failed(e, pc, "c", "()Lbt;");
    }
    ++e->pooled_vectors;
    e->child = (mc_client_graphics_site){0};
    return true;
}

static bool direction(mc_client_render_engine_classes *c, mc_entity_classes *entities,
                      mc_math_classes *math, mc_client_display *display, unsigned int light,
                      mc_entity_vector **raw, mc_entity_vector **normalized, bool *entities_held,
                      bool *math_held, mc_client_lighting_error *e) {
    const int32_t factory_pc = light == 0 ? 48 : 135;
    if (!*entities_held) {
        e->native_entity_status = mc_entity_classes_retain(
            entities, MC_ENTITY_CLIENT, entities == NULL ? NULL : entities->profile);
        if (e->native_entity_status != MC_ENTITY_OK)
            return fail(e, MC_CLIENT_LIGHTING_NATIVE_FAILED, "u Entity namespace occurrence",
                        factory_pc, true);
        *entities_held = true;
    }
    const double x = light == 0 ? 0.20000000298023224 : -0.20000000298023224;
    const double z = light == 0 ? -0.699999988079071 : 0.699999988079071;
    if (mc_entity_vector_pool_get(entities, x, 1, z, raw, &e->vector) != MC_ENTITY_OK)
        return vector_failed(e, factory_pc, "b", "(DDD)Lbt;");
    ++e->pooled_vectors;
    if (!normalize(entities, *raw, math, math_held, normalized, light == 0 ? 51 : 138, e))
        return false;
    const int32_t fill_pc = light == 0 ? 74 : 161;
    mc_client_direct_float_buffer *buffer = NULL;
    const float x32 = narrow((*normalized)->x);
    const float y32 = narrow((*normalized)->y);
    const float z32 = narrow((*normalized)->z);
    const float w32 = narrow(0);
    return fill(c, x32, y32, z32, w32, fill_pc, true, &buffer, e) &&
           upload(display, light, 4611, buffer, light == 0 ? 77 : 164, false, e);
}

bool mc_client_lighting_enable(mc_client_render_engine_classes *c, mc_entity_classes *entities,
                               mc_math_classes *math, mc_client_display *display,
                               mc_client_graphics_site caller, mc_client_lighting_error *e) {
    if (e == NULL)
        return false;
    *e = (mc_client_lighting_error){.caller = caller};
    if (!mc_client_lighting_active_use(c, caller, &e->graphics)) {
        e->child = e->graphics.site;
        return graphics_failed(e, 0);
    }
    e->native_graphics_status = mc_client_render_engine_classes_retain(c);
    if (e->native_graphics_status != MC_CLIENT_RENDER_ENGINE_OK)
        return fail(e, MC_CLIENT_LIGHTING_NATIVE_FAILED, "u call native class occurrence", 0, true);
    ++c->active_source_calls;
    bool entities_held = false, math_held = false;
    mc_entity_vector *vectors[4] = {0};
    mc_client_direct_float_buffer *buffer = NULL;
    bool ok =
        display_result(
            e, mc_client_display_source_lighting_enable(display, true, &e->graphics.display), 3) &&
        display_result(
            e, mc_client_display_source_light_enable(display, 0, true, &e->graphics.display), 9) &&
        display_result(
            e, mc_client_display_source_light_enable(display, 1, true, &e->graphics.display), 15) &&
        display_result(
            e, mc_client_display_source_color_material_enable(display, true, &e->graphics.display),
            21) &&
        display_result(
            e, mc_client_display_source_color_material(display, 1032, 5634, &e->graphics.display),
            30);
    for (unsigned int light = 0; ok && light < 2; ++light) {
        ok = direction(c, entities, math, display, light, &vectors[2 * light],
                       &vectors[2 * light + 1], &entities_held, &math_held, e);
        const int32_t fill_pcs[2][3] = {{90, 106, 122}, {177, 193, 209}};
        const int32_t display_pcs[2][3] = {{93, 109, 125}, {180, 196, 212}};
        const int32_t parameters[3] = {4609, 4608, 4610};
        for (size_t i = 0; ok && i < 3; ++i) {
            const float value = i == 0 ? 0.6f : 0.0f;
            ok = fill(c, value, value, value, 1, fill_pcs[light][i], false, &buffer, e) &&
                 upload(display, light, parameters[i], buffer, display_pcs[light][i], false, e);
        }
    }
    if (ok)
        ok = display_result(
                 e, mc_client_display_source_shade_model(display, 7424, &e->graphics.display),
                 218) &&
             fill(c, 0.4f, 0.4f, 0.4f, 1, 228, false, &buffer, e) &&
             upload(display, 0, 2899, buffer, 231, true, e);
    if (ok) {
        e->site = u_site(234);
        e->source_returned = true;
    }
    for (size_t i = 4; i != 0; --i) {
        if (vectors[i - 1] != NULL) {
            const mc_entity_status status = mc_entity_vector_release(entities, vectors[i - 1]);
            if (status != MC_ENTITY_OK) {
                e->cleanup_entity_status = status;
                e->cleanup_failed = true;
            }
        }
    }
    if (math_held && mc_math_classes_release(&math, &e->cleanup_math) != MC_MATH_CLASS_OK)
        e->cleanup_failed = true;
    if (entities_held) {
        const mc_entity_status status = mc_entity_classes_release(entities);
        if (status != MC_ENTITY_OK) {
            e->cleanup_entity_status = status;
            e->cleanup_failed = true;
        }
    }
    --c->active_source_calls;
    e->cleanup_graphics_status = mc_client_render_engine_classes_release(c);
    if (e->cleanup_graphics_status != MC_CLIENT_RENDER_ENGINE_OK)
        e->cleanup_failed = true;
    if (e->cleanup_failed && ok)
        return fail(e, MC_CLIENT_LIGHTING_NATIVE_FAILED, "u terminal native occurrence release",
                    234, true);
    return ok;
}
