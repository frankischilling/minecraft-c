#include "position_internal.h"

#include <float.h>
#include <stdint.h>
#include <string.h>

#if !defined(__SSE2__) && !defined(_M_X64)
#error "Entity position requires x86-64/SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Entity position requires strict floating transformations"
#endif
#include <emmintrin.h>
_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && DBL_MANT_DIG == 53 && FLT_EVAL_METHOD == 0,
               "Entity position requires binary32/binary64 evaluation");

static double widen(float value) {
    return _mm_cvtsd_f64(_mm_cvtss_sd(_mm_setzero_pd(), _mm_set_ss(value)));
}

static double sub(double left, double right) {
    return _mm_cvtsd_f64(_mm_sub_sd(_mm_set_sd(left), _mm_set_sd(right)));
}

static double add(double left, double right) {
    uint64_t a, b;
    memcpy(&a, &left, sizeof(a));
    memcpy(&b, &right, sizeof(b));
    if ((a & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x7ff0000000000000) &&
        (b & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x7ff0000000000000)) {
        b |= UINT64_C(0x0008000000000000);
        memcpy(&right, &b, sizeof(right));
        return right;
    }
    return _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd(left), _mm_set_sd(right)));
}

mc_entity_bounds mc_entity_source_position_values(const mc_entity *p, double x, double y, double z,
                                                  float half_width, float height) {
    mc_entity_bounds value;
    value.min_x = sub(x, widen(half_width));
    value.min_y = add(sub(y, widen(p->base.offset)), widen(p->base.y_size));
    value.min_z = sub(z, widen(half_width));
    value.max_x = add(x, widen(half_width));
    value.max_y = add(add(sub(y, widen(p->base.offset)), widen(p->base.y_size)), widen(height));
    value.max_z = add(z, widen(half_width));
    return value;
}
