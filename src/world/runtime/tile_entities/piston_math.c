#include "piston_math_internal.h"

#include <float.h>
#include <string.h>

#if !defined(__SSE2__) && !defined(_M_X64) && !(defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#error "Piston arithmetic requires SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Piston arithmetic requires strict floating transformations"
#endif
#include <emmintrin.h>

_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128 && DBL_MANT_DIG == 53 &&
                   DBL_MAX_EXP == 1024 && FLT_EVAL_METHOD == 0 &&
                   sizeof(float) == sizeof(uint32_t) && sizeof(double) == sizeof(uint64_t),
               "Piston arithmetic requires binary32/binary64 evaluation and storage");

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static float value(uint32_t raw) {
    float result;
    memcpy(&result, &raw, sizeof(result));
    return result;
}

static bool is_nan32(uint32_t raw) {
    return (raw & UINT32_C(0x7fffffff)) > UINT32_C(0x7f800000);
}

bool mc_piston_math_admitted(void) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u;
}

float mc_piston_float_subtract(float left, float right) {
    const uint32_t l = bits(left), r = bits(right);
    if (is_nan32(l) && is_nan32(r))
        return value(l | UINT32_C(0x00400000));
    return _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(left), _mm_set_ss(right)));
}

float mc_piston_float_multiply(float left, float right) {
    const uint32_t l = bits(left), r = bits(right);
    if (is_nan32(l) && is_nan32(r))
        return value(r | UINT32_C(0x00400000));
    return _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(left), _mm_set_ss(right)));
}

float mc_piston_float_add(float left, float right) {
    const uint32_t l = bits(left), r = bits(right);
    if (is_nan32(l) && is_nan32(r))
        return value(r | UINT32_C(0x00400000));
    return _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(left), _mm_set_ss(right)));
}

uint32_t mc_piston_interpolate_bits(uint32_t current_bits, uint32_t previous_bits,
                                    uint32_t factor_bits) {
    float factor = value(factor_bits);
    if (factor > 1.0f)
        factor = 1.0f;
    const float previous = value(previous_bits);
    const float delta = mc_piston_float_subtract(value(current_bits), value(previous_bits));
    const float scaled = mc_piston_float_multiply(delta, factor);
    return bits(mc_piston_float_add(previous, scaled));
}

double mc_piston_coordinate_subtract(double coordinate, float offset) {
    /* Preserve the original f2d followed by dsub; neither multiplication in
     * binary64 nor negate-and-add has the same exceptional-value behavior. */
    const __m128d widened = _mm_cvtss_sd(_mm_setzero_pd(), _mm_set_ss(offset));
    return _mm_cvtsd_f64(_mm_sub_sd(_mm_set_sd(coordinate), widened));
}
