#ifndef MC_WORLD_WEATHER_MATH_INTERNAL_H
#define MC_WORLD_WEATHER_MATH_INTERNAL_H

#include <float.h>
#include <stdbool.h>
#include <stdint.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "World weather arithmetic requires x86-64/SSE2"
#endif
#if !defined(_MSC_VER) && !defined(__SSE2__)
#error "World weather arithmetic requires SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "World weather arithmetic requires strict floating transformations"
#endif
#include <emmintrin.h>

_Static_assert(sizeof(float) == sizeof(uint32_t) && sizeof(double) == sizeof(uint64_t) &&
                   FLT_RADIX == 2 && FLT_MANT_DIG == 24 && DBL_MANT_DIG == 53 &&
                   FLT_EVAL_METHOD == 0,
               "World weather requires binary32/binary64 evaluation");

static inline float mc_weather_step(float value, bool increase) {
    const __m128d widened = _mm_cvtss_sd(_mm_setzero_pd(), _mm_set_ss(value));
    /* Actual CONSTANT_Double bits3f847ae147ae147b, never a binary32 step. */
    const __m128d step = _mm_set_sd(0x1.47ae147ae147bp-7);
    const __m128d changed = increase ? _mm_add_sd(widened, step) : _mm_sub_sd(widened, step);
    return _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), changed));
}

#endif
