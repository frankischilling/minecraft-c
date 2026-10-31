#ifndef MC_ENTITY_PLAYER_TICK_MATH_INTERNAL_H
#define MC_ENTITY_PLAYER_TICK_MATH_INTERNAL_H
#include <float.h>
#include <stdint.h>
#include <string.h>
#if !defined(__SSE2__) && !defined(_M_X64)
#error "Player active tick requires x86-64/SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Player active tick requires strict floating transformations"
#endif
#include <emmintrin.h>
_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && DBL_MANT_DIG == 53 && FLT_EVAL_METHOD == 0,
               "Player active tick requires binary32/binary64 evaluation");

static inline double at_add(double a, double b) {
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

static inline double at_sub(double a, double b) {
    return _mm_cvtsd_f64(_mm_sub_sd(_mm_set_sd(a), _mm_set_sd(b)));
}

static inline double at_mul(double a, double b) {
    return _mm_cvtsd_f64(_mm_mul_sd(_mm_set_sd(a), _mm_set_sd(b)));
}

static inline double at_div(double a, double b) {
    return _mm_cvtsd_f64(_mm_div_sd(_mm_set_sd(a), _mm_set_sd(b)));
}

static inline double at_widen(float a) {
    return _mm_cvtsd_f64(_mm_cvtss_sd(_mm_setzero_pd(), _mm_set_ss(a)));
}

static inline float at_narrow(double a) {
    return _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), _mm_set_sd(a)));
}

static inline float at_fadd(float a, float b) {
    return _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static inline float at_fsub(float a, float b) {
    return _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static inline float at_fmul(float a, float b) {
    return _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static inline float at_fdiv(float a, float b) {
    return _mm_cvtss_f32(_mm_div_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static inline float at_fneg(float a) {
    uint32_t b;
    memcpy(&b, &a, 4);
    b ^= UINT32_C(0x80000000);
    memcpy(&a, &b, 4);
    return a;
}

static inline float at_root(double a) {
    return at_narrow(_mm_cvtsd_f64(_mm_sqrt_sd(_mm_setzero_pd(), _mm_set_sd(a))));
}

double mc_player_tick_atan2(double y, double x);
#endif
