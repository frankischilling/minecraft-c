#include "mc/world/biome/sky_color.h"

#include "../../block/registry/registry_internal.h"
#include "mc/core/java/java.h"
#include "registry_internal.h"
#include "sky_color_internal.h"

#include <emmintrin.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Biome sky color requires the captured x86-64/SSE2 profile"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Biome sky color requires strict floating transformations"
#endif
_Static_assert(CHAR_BIT == 8 && sizeof(float) == 4 && sizeof(double) == 8 && FLT_RADIX == 2 &&
                   FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128 && DBL_MANT_DIG == 53 &&
                   DBL_MAX_EXP == 1024 && FLT_EVAL_METHOD == 0,
               "Biome sky color requires source-width binary32/binary64");

static float add(float a, float b) {
    return _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static float sub(float a, float b) {
    return _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static float mul(float a, float b) {
    return _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static float floor_float(float value) {
    return _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), _mm_set_sd(floor((double)value))));
}

static int32_t channel(float value) {
    return mc_java_float_to_i32(add(mul(value, 255.0f), 0.5f));
}

bool mc_java_color_hsb_to_rgb(float hue, float saturation, float brightness, int32_t *out) {
    if (out == NULL || (_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return false;
    int32_t r = 0, g = 0, b = 0;
    if (saturation == 0.0f)
        r = g = b = channel(brightness);
    else {
        const float h = mul(sub(hue, floor_float(hue)), 6.0f);
        const float f = sub(h, floor_float(h));
        const float p = mul(brightness, sub(1.0f, saturation));
        const float q = mul(brightness, sub(1.0f, mul(saturation, f)));
        const float t = mul(brightness, sub(1.0f, mul(saturation, sub(1.0f, f))));
        switch (mc_java_float_to_i32(h)) {
            case 0:
                r = channel(brightness);
                g = channel(t);
                b = channel(p);
                break;
            case 1:
                r = channel(q);
                g = channel(brightness);
                b = channel(p);
                break;
            case 2:
                r = channel(p);
                g = channel(brightness);
                b = channel(t);
                break;
            case 3:
                r = channel(p);
                g = channel(q);
                b = channel(brightness);
                break;
            case 4:
                r = channel(t);
                g = channel(p);
                b = channel(brightness);
                break;
            case 5:
                r = channel(brightness);
                g = channel(p);
                b = channel(q);
                break;
            default:
                break;
        }
    }
    const uint32_t bits = UINT32_C(0xff000000) | (uint32_t)mc_java_i32_shl(r, 16) |
                          (uint32_t)mc_java_i32_shl(g, 8) | (uint32_t)b;
    memcpy(out, &bits, sizeof(bits));
    return true;
}

static mc_registry_status result(mc_biome_sky_color_error *error, mc_registry_status status,
                                 const char *operation, unsigned int pc, bool native) {
    error->status = status;
    error->operation = operation;
    error->bytecode = pc;
    error->native_policy = native;
    return status;
}

mc_registry_status mc_world_biome_sky_color(const mc_biome *biome, float temperature, int32_t *out,
                                            mc_biome_sky_color_error *error) {
    mc_biome_sky_color_error local = {0};
    mc_biome_sky_color_error *e = error == NULL ? &local : error;
    *e = (mc_biome_sky_color_error){0};
    if (out == NULL)
        return result(e, MC_REGISTRY_INVALID_ARGUMENT, "Biome sky color output", 0, true);
    if (biome == NULL)
        return result(e, MC_REGISTRY_NULL_REFERENCE, "kd sky color null receiver", 0, false);
    if (biome->classes == NULL || biome->classes->options.side != MC_REGISTRY_CLIENT)
        return result(e, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY, "client kd sky color owner", 0,
                      true);
    if (biome->classes->entered != 0)
        return result(e, MC_REGISTRY_BUSY, "Biome sky color Registry flow entered", 0, true);
    if (biome->kind == MC_BIOME_KIND_RY_LE) {
        *out = INT32_C(12632319); /* Actual client ry.a(F) constant, return2. */
        e->source_returned = true;
        return result(e, MC_REGISTRY_OK, "ry fixed sky color return", 2, false);
    }
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return result(e, MC_REGISTRY_INVALID_ARGUMENT,
                      "Biome sky color strict floating environment", 3, true);
    ++biome->classes->entered;
    float value = _mm_cvtss_f32(_mm_div_ss(_mm_set_ss(temperature), _mm_set_ss(3.0f)));
    if (value < -1.0f)
        value = -1.0f;
    if (value > 1.0f)
        value = 1.0f;
    const float hue = sub(0.62222224f, mul(value, 0.05f));
    const float saturation = add(0.5f, mul(value, 0.1f));

    /* Actual Color.getHSBColor NEW0 precedes its HSBtoRGB7 evaluation. */
    struct color_value {
        int32_t value;
    } *color = malloc(sizeof(*color));

    if (color == NULL) {
        --biome->classes->entered;
        return result(e, MC_REGISTRY_ALLOCATION_FAILED, "Color native temporary allocation", 38,
                      true);
    }
    e->color_allocated = true;
    int32_t rgb;
    if (!mc_java_color_hsb_to_rgb(hue, saturation, 1.0f, &rgb)) {
        free(color);
        --biome->classes->entered;
        e->color_bytecode = 7;
        return result(e, MC_REGISTRY_INVALID_ARGUMENT, "Color HSB strict floating environment", 38,
                      true);
    }
    uint32_t bits;
    memcpy(&bits, &rgb, sizeof(bits));
    bits |= UINT32_C(0xff000000); /* Actual Color(int) value store. */
    memcpy(&color->value, &bits, sizeof(bits));
    e->color_constructed = true;
    const int32_t answer = color->value; /* Actual Color.getRGB value read. */
    free(color);
    --biome->classes->entered;
    *out = answer;
    e->source_returned = true;
    return result(e, MC_REGISTRY_OK, "kd sky color return", 44, false);
}
