#include "mc/core/math/strict.h"

#include <float.h>
#include <limits.h>
#include <stdint.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "strict sqrt requires the captured x86-64/SSE2 target"
#endif

#if !defined(_MSC_VER) && !defined(__SSE2__)
#error "strict sqrt requires SSE2 instructions"
#endif

#if defined(__FAST_MATH__) || (defined(_M_FP_FAST) && _M_FP_FAST)
#error "strict math requires floating transformations to be disabled"
#endif

#include <emmintrin.h>

_Static_assert(CHAR_BIT == 8, "8-bit bytes required");
_Static_assert(sizeof(double) == sizeof(uint64_t), "64-bit double representation required");
_Static_assert(FLT_RADIX == 2 && DBL_MANT_DIG == 53 && DBL_MIN_EXP == -1021 && DBL_MAX_EXP == 1024,
               "IEEE 754 binary64 required");
_Static_assert(DBL_HAS_SUBNORM == 1, "binary64 subnormal support required");
_Static_assert(FLT_EVAL_METHOD == 0, "binary64 expression evaluation required");

double mc_math_strict_sqrt(double value)
{
    const __m128d input = _mm_set_sd(value);
    const __m128d result = _mm_sqrt_sd(input, input);
    return _mm_cvtsd_f64(result);
}
