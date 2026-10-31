/* @(#)s_atan.c 1.3 95/01/18 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */
#include "atan_internal.h"
#include <float.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Action atan requires strict floating transformations"
#endif
_Static_assert(FLT_RADIX == 2 && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024 &&
                   FLT_EVAL_METHOD == 0 && sizeof(double) == sizeof(uint64_t),
               "Action atan requires binary64 evaluation");

/* Exact original coefficient words, written as binary64 hexadecimal values. */
static const double high[] = {0x1.dac670561bb4fp-2, 0x1.921fb54442d18p-1, 0x1.f730bd281f69bp-1,
                              0x1.921fb54442d18p+0};
static const double low[] = {0x1.a2b7f222f65e2p-56, 0x1.1a62633145c07p-55, 0x1.007887af0cbbdp-56,
                             0x1.1a62633145c07p-54};
static const double coefficient[] = {
    0x1.555555555550dp-2, -0x1.999999998ebc4p-3, 0x1.24924920083ffp-3, -0x1.c71c6fe231671p-4,
    0x1.745cdc54c206ep-4, -0x1.3b0f2af749a6dp-4, 0x1.10d66a0d03d51p-4, -0x1.dde2d52defd9ap-5,
    0x1.97b4b24760debp-5, -0x1.2b4442c6a6c2fp-5, 0x1.0ad3ae322da11p-6};

double mc_action_atan(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    const uint32_t absolute_high = (uint32_t)(bits >> 32) & UINT32_C(0x7fffffff);
    const bool negative = (bits >> 63) != 0;
    if (absolute_high >= UINT32_C(0x44100000)) {
        if (absolute_high > UINT32_C(0x7ff00000) ||
            (absolute_high == UINT32_C(0x7ff00000) && (uint32_t)bits != 0))
            return value + value;
        return negative ? -high[3] - low[3] : high[3] + low[3];
    }
    int index;
    if (absolute_high < UINT32_C(0x3fdc0000)) {
        if (absolute_high < UINT32_C(0x3e200000)) {
            if (1.0e300 + value > 1.0)
                return value;
        }
        index = -1;
    } else {
        bits &= UINT64_C(0x7fffffffffffffff);
        memcpy(&value, &bits, sizeof(value));
        if (absolute_high < UINT32_C(0x3ff30000)) {
            if (absolute_high < UINT32_C(0x3fe60000)) {
                index = 0;
                value = (2.0 * value - 1.0) / (2.0 + value);
            } else {
                index = 1;
                value = (value - 1.0) / (value + 1.0);
            }
        } else {
            if (absolute_high < UINT32_C(0x40038000)) {
                index = 2;
                value = (value - 1.5) / (1.0 + 1.5 * value);
            } else {
                index = 3;
                value = -1.0 / value;
            }
        }
    }
    const double square = value * value, fourth = square * square;
    const double odd =
        square *
        (coefficient[0] +
         fourth * (coefficient[2] +
                   fourth * (coefficient[4] +
                             fourth * (coefficient[6] +
                                       fourth * (coefficient[8] + fourth * coefficient[10])))));
    const double even =
        fourth * (coefficient[1] +
                  fourth * (coefficient[3] +
                            fourth * (coefficient[5] +
                                      fourth * (coefficient[7] + fourth * coefficient[9]))));
    if (index < 0)
        return value - value * (odd + even);
    const double result = high[index] - ((value * (odd + even) - low[index]) - value);
    return negative ? -result : result;
}
