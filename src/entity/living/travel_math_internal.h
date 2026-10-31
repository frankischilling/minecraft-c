#ifndef MC_ENTITY_LIVING_TRAVEL_MATH_INTERNAL_H
#define MC_ENTITY_LIVING_TRAVEL_MATH_INTERNAL_H

#include "mc/core/java/java.h"
#include <stdint.h>
#include <string.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Living travel requires the captured x86-64/SSE2 target"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Living travel requires strict floating transformations"
#endif
#include <emmintrin.h>

/* Captured interpreter commutative operations keep the top/right source
 * operand in the destination register, including NaN payload priority. */
static inline float mc_travel_fadd(float a, float b) {
    return _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(b), _mm_set_ss(a)));
}

static inline float mc_travel_fsub(float a, float b) {
    return _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static inline float mc_travel_fmul(float a, float b) {
    return _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(b), _mm_set_ss(a)));
}

static inline float mc_travel_fdiv(float a, float b) {
    return _mm_cvtss_f32(_mm_div_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static inline double mc_travel_dadd(double a, double b) {
    return _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd(b), _mm_set_sd(a)));
}

static inline double mc_travel_dsub(double a, double b) {
    return _mm_cvtsd_f64(_mm_sub_sd(_mm_set_sd(a), _mm_set_sd(b)));
}

static inline double mc_travel_dmul(double a, double b) {
    return _mm_cvtsd_f64(_mm_mul_sd(_mm_set_sd(b), _mm_set_sd(a)));
}

static inline float mc_travel_sqrt_float(double a) {
    const __m128d root = _mm_sqrt_sd(_mm_setzero_pd(), _mm_set_sd(a));
    return _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), root));
}

/* Actual OpenJDK17 Math.round(F)I and round(D)J significand paths. This is
 * deliberately not floor(value+0.5), which double-rounds near the boundary.
 * Fallback uses the existing exact Java narrowing/saturation helpers. */
static inline int32_t mc_travel_round_float(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    const int32_t shift = 149 - (int32_t)((bits >> 23) & 0xffu);
    if (shift >= 0 && shift < 32) {
        int32_t significand = (int32_t)((bits & UINT32_C(0x007fffff)) | UINT32_C(0x00800000));
        if ((bits >> 31) != 0)
            significand = -significand;
        return mc_java_i32_shr(mc_java_i32_add(mc_java_i32_shr(significand, shift), 1), 1);
    }
    return mc_java_float_to_i32(value);
}

static inline int64_t mc_travel_round_double(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    const int32_t shift = 1074 - (int32_t)((bits >> 52) & 0x7ffu);
    if (shift >= 0 && shift < 64) {
        int64_t significand =
            (int64_t)((bits & UINT64_C(0x000fffffffffffff)) | UINT64_C(0x0010000000000000));
        if ((bits >> 63) != 0)
            significand = -significand;
        return mc_java_i64_shr(mc_java_i64_add(mc_java_i64_shr(significand, shift), 1), 1);
    }
    return mc_java_double_to_i64(value);
}
#endif
