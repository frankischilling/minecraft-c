#ifndef MC_WORLD_ENTITY_TICK_MATH_INTERNAL_H
#define MC_WORLD_ENTITY_TICK_MATH_INTERNAL_H
#include <float.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#if !defined(__SSE2__) && !defined(_M_X64)
#error "World Entity update requires x86-64/SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "World Entity update requires strict floating transformations"
#endif
#include <emmintrin.h>
_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && DBL_MANT_DIG == 53 && FLT_EVAL_METHOD == 0,
               "World Entity update requires binary32/binary64 evaluation");

static inline double wt_add(double a, double b) {
    uint64_t x, y;
    memcpy(&x, &a, 8);
    memcpy(&y, &b, 8);
    if ((x & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x7ff0000000000000) &&
        (y & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x7ff0000000000000)) {
        y |= UINT64_C(0x0008000000000000);
        memcpy(&b, &y, 8);
        return b;
    }
    return _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd(a), _mm_set_sd(b)));
}

static inline double wt_sub(double a, double b) {
    return _mm_cvtsd_f64(_mm_sub_sd(_mm_set_sd(a), _mm_set_sd(b)));
}

static inline double wt_mul(double a, double b) {
    return _mm_cvtsd_f64(_mm_mul_sd(_mm_set_sd(a), _mm_set_sd(b)));
}

static inline double wt_div(double a, double b) {
    return _mm_cvtsd_f64(_mm_div_sd(_mm_set_sd(a), _mm_set_sd(b)));
}

static inline double wt_widen(float a) {
    return _mm_cvtsd_f64(_mm_cvtss_sd(_mm_setzero_pd(), _mm_set_ss(a)));
}

static inline float wt_i2f(int32_t a) {
    return _mm_cvtss_f32(_mm_cvtsi32_ss(_mm_setzero_ps(), a));
}

static inline float wt_fadd(float a, float b) {
    return _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static inline float wt_fsub(float a, float b) {
    return _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static inline float wt_fdiv(float a, float b) {
    return _mm_cvtss_f32(_mm_div_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static inline bool wt_nonfinite(double value) {
    uint64_t bits;
    memcpy(&bits, &value, 8);
    return (bits & UINT64_C(0x7fffffffffffffff)) >= UINT64_C(0x7ff0000000000000);
}

static inline bool wt_nonfinite_float(float value) {
    uint32_t bits;
    memcpy(&bits, &value, 4);
    return (bits & UINT32_C(0x7fffffff)) >= UINT32_C(0x7f800000);
}
#endif
