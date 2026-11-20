/* @(#)e_log.c 1.3 95/01/18 */
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

/* __ieee754_log(x)
 * Return the logrithm of x
 *
 * Method :
 *   1. Argument Reduction: find k and f such that
 *                      x = 2^k * (1+f),
 *         where  sqrt(2)/2 < 1+f < sqrt(2) .
 *
 *   2. Approximation of log(1+f).
 *      Let s = f/(2+f) ; based on log(1+f) = log(1+s) - log(1-s)
 *               = 2s + 2/3 s**3 + 2/5 s**5 + .....,
 *               = 2s + s*R
 *      We use a special Reme algorithm on [0,0.1716] to generate
 *      a polynomial of degree 14 to approximate R The maximum error
 *      of this polynomial approximation is bounded by 2**-58.45. In
 *      other words,
 *                      2      4      6      8      10      12      14
 *          R(z) ~ Lg1*s +Lg2*s +Lg3*s +Lg4*s +Lg5*s  +Lg6*s  +Lg7*s
 *      (the values of Lg1 to Lg7 are listed in the program)
 *      and
 *          |      2          14          |     -58.45
 *          | Lg1*s +...+Lg7*s    -  R(z) | <= 2
 *          |                             |
 *      Note that 2s = f - s*f = f - hfsq + s*hfsq, where hfsq = f*f/2.
 *      In order to guarantee error in log below 1ulp, we compute log
 *      by
 *              log(1+f) = f - s*(f - R)        (if f is not too large)
 *              log(1+f) = f - (hfsq - s*(hfsq+R)).     (better accuracy)
 *
 *      3. Finally,  log(x) = k*ln2 + log(1+f).
 *                          = k*ln2_hi+(f-(hfsq-(s*(hfsq+R)+k*ln2_lo)))
 *         Here ln2 is split into two floating point number:
 *                      ln2_hi + ln2_lo,
 *         where n*ln2_hi is always exact for |n| < 2000.
 *
 * Special cases:
 *      log(x) is NaN with signal if x < 0 (including -INF) ;
 *      log(+INF) is +INF; log(0) is -INF with signal;
 *      log(NaN) is that NaN with no signal.
 *
 * Accuracy:
 *      according to an error analysis, the error is always less than
 *      1 ulp (unit in the last place).
 *
 * Constants:
 * The hexadecimal values are the intended ones for the following
 * constants. The decimal values may be used, provided that the
 * compiler will convert from decimal to binary accurately enough
 * to produce the hexadecimal values shown.
 */

/* Adapted from the independently acquired Netlib fdlibm 5.3 source only.
 * Source URL, byte hash, license and modifications: licenses/fdlibm.txt.
 * The public contract admits positive finite inputs; the original algorithm
 * comments and exceptional branches above/below are retained as provenance,
 * without claiming general exceptional-input or floating-status parity. */

#include "mc/core/math/strict.h"

#include <float.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

#if defined(__FAST_MATH__) || (defined(_M_FP_FAST) && _M_FP_FAST)
#error "strict math requires floating transformations to be disabled"
#endif

_Static_assert(CHAR_BIT == 8, "8-bit bytes required");
_Static_assert(sizeof(double) == sizeof(uint64_t), "64-bit double representation required");
_Static_assert(FLT_RADIX == 2 && DBL_MANT_DIG == 53 && DBL_MIN_EXP == -1021 && DBL_MAX_EXP == 1024,
               "IEEE 754 binary64 required");
_Static_assert(DBL_HAS_SUBNORM == 1, "binary64 subnormal support required");
_Static_assert(FLT_EVAL_METHOD == 0, "binary64 expression evaluation required");

static uint64_t strict_log_bits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

/* Decode a two's-complement word without an out-of-range signed cast. */
static int32_t strict_log_signed_word(uint32_t word) {
    if (word <= UINT32_C(0x7fffffff)) {
        return (int32_t)word;
    }
    return -INT32_C(1) - (int32_t)(UINT32_MAX - word);
}

static int32_t strict_log_high(double value) {
    return strict_log_signed_word((uint32_t)(strict_log_bits(value) >> 32));
}

static uint32_t strict_log_low(double value) {
    return (uint32_t)strict_log_bits(value);
}

static double strict_log_replace_high(double value, uint32_t high) {
    uint64_t bits = strict_log_bits(value);
    bits = (bits & UINT64_C(0x00000000ffffffff)) | ((uint64_t)high << 32);
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static const double ln2_hi = 0x1.62e42fee00000p-1;  /* 3fe62e42 fee00000 */
static const double ln2_lo = 0x1.a39ef35793c76p-33; /* 3dea39ef 35793c76 */
static const double two54 = 0x1p+54;                /* 43500000 00000000 */
static const double Lg1 = 0x1.5555555555593p-1;     /* 3FE55555 55555593 */
static const double Lg2 = 0x1.999999997fa04p-2;     /* 3FD99999 9997FA04 */
static const double Lg3 = 0x1.2492494229359p-2;     /* 3FD24924 94229359 */
static const double Lg4 = 0x1.c71c51d8e78afp-3;     /* 3FCC71C5 1D8E78AF */
static const double Lg5 = 0x1.7466496cb03dep-3;     /* 3FC74664 96CB03DE */
static const double Lg6 = 0x1.39a09d078c69fp-3;     /* 3FC39A09 D078C69F */
static const double Lg7 = 0x1.2f112df3e5244p-3;     /* 3FC2F112 DF3E5244 */
static double zero = 0.0;

double mc_math_strict_log(double x) {
    double hfsq, f, s, z, R, w, t1, t2, dk;
    int32_t k, hx, i, j;
    uint32_t lx;

    hx = strict_log_high(x); /* high word of x */
    lx = strict_log_low(x);  /* low word of x */

    k = 0;
    if (hx < INT32_C(0x00100000)) { /* x < 2**-1022 */
        if ((((uint32_t)hx & UINT32_C(0x7fffffff)) | lx) == 0) {
            return -two54 / zero; /* log(+-0)=-inf */
        }
        if (hx < 0) {
            return (x - x) / zero; /* log(-#) = NaN */
        }
        k -= 54;
        x *= two54;              /* subnormal number, scale up x */
        hx = strict_log_high(x); /* high word of x */
    }
    if (hx >= INT32_C(0x7ff00000)) {
        return x + x;
    }
    /* Negative words have returned above; both signed shifts below have
     * nonnegative operands. The bounded exponent arithmetic cannot overflow. */
    k += (hx >> 20) - 1023;
    hx &= INT32_C(0x000fffff);
    i = (hx + INT32_C(0x95f64)) & INT32_C(0x100000);
    x = strict_log_replace_high(x, (uint32_t)(hx | (i ^ INT32_C(0x3ff00000))));
    /* normalize x or x/2 */
    k += (i >> 20);
    f = x - 1.0;
    if ((INT32_C(0x000fffff) & (2 + hx)) < 3) { /* |f| < 2**-20 */
        if (f == zero) {
            if (k == 0) {
                return zero;
            }
            dk = (double)k;
            return dk * ln2_hi + dk * ln2_lo;
        }
        R = f * f * (0.5 - 0x1.5555555555555p-2 * f);
        if (k == 0) {
            return f - R;
        }
        dk = (double)k;
        return dk * ln2_hi - ((R - dk * ln2_lo) - f);
    }
    s = f / (2.0 + f);
    dk = (double)k;
    z = s * s;
    i = hx - INT32_C(0x6147a);
    w = z * z;
    j = INT32_C(0x6b851) - hx;
    t1 = w * (Lg2 + w * (Lg4 + w * Lg6));
    t2 = z * (Lg1 + w * (Lg3 + w * (Lg5 + w * Lg7)));
    i = strict_log_signed_word((uint32_t)i | (uint32_t)j);
    R = t2 + t1;
    if (i > 0) {
        hfsq = 0.5 * f * f;
        if (k == 0) {
            return f - (hfsq - s * (hfsq + R));
        }
        return dk * ln2_hi - ((hfsq - (s * (hfsq + R) + dk * ln2_lo)) - f);
    }
    if (k == 0) {
        return f - s * (f - R);
    }
    return dk * ln2_hi - ((s * (f - R) - dk * ln2_lo) - f);
}
