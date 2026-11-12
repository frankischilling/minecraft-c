#include "mc/core/math/trig_double.h"
#include <float.h>
#include <math.h>
#if defined(__FAST_MATH__) || (defined(_M_FP_FAST) && _M_FP_FAST)
#error "double Math adapter requires strict floating point"
#endif
_Static_assert(sizeof(double) == 8 && sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24 &&
                   DBL_MANT_DIG == 53 && DBL_MIN_EXP == -1021 && DBL_MAX_EXP == 1024 &&
                   DBL_HAS_SUBNORM == 1 && FLT_EVAL_METHOD == 0,
               "binary64 expression evaluation required");

double mc_math_native_double_sin(double value) {
    return sin(value);
}

double mc_math_native_double_cos(double value) {
    return cos(value);
}
