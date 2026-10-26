/* @(#)e_atan2.c 1.3 95/01/18 */
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
#include "../action/atan_internal.h"
#include "mc/core/java/java.h"
#include "tick_math_internal.h"

/* Netlib origin; OpenJDK is an independent target-source audit input only.
 * Alias-safe words and exact coefficient words replace fdlibm's pointer macros.
 * Parent callers admit strict nearest-even/gradual-underflow arithmetic. */
double mc_player_tick_atan2(double y, double x) {
    static const double tiny = 1.0e-300, pi4 = 0x1.921fb54442d18p-1, pi2 = 0x1.921fb54442d18p+0,
                        pi = 0x1.921fb54442d18p+1, lo = 0x1.1a62633145c07p-53;
    uint64_t xb, yb;
    memcpy(&xb, &x, 8);
    memcpy(&yb, &y, 8);
    const uint32_t ix = (uint32_t)(xb >> 32) & UINT32_C(0x7fffffff),
                   iy = (uint32_t)(yb >> 32) & UINT32_C(0x7fffffff);
    const uint32_t lx = (uint32_t)xb, ly = (uint32_t)yb;
    const bool nx = (xb >> 63) != 0, ny = (yb >> 63) != 0;
    const bool x_nan = (xb & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x7ff0000000000000);
    const bool y_nan = (yb & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x7ff0000000000000);
    if (x_nan || y_nan) {
        /* Qualified native fdlibm x+y selects x when both operands are NaN.
         * Make that payload choice independent of C operand commutation.
         * Floating exception flags are outside this value-result profile. */
        uint64_t selected = (x_nan ? xb : yb) | UINT64_C(0x0008000000000000);
        double result;
        memcpy(&result, &selected, 8);
        return result;
    }
    if (xb == UINT64_C(0x3ff0000000000000))
        return mc_action_atan(y);
    const unsigned int quadrant = (nx ? 2u : 0u) | (ny ? 1u : 0u);
    if ((iy | ly) == 0) {
        if (quadrant < 2)
            return y;
        return quadrant == 2 ? at_add(pi, tiny) : at_sub(-pi, tiny);
    }
    if ((ix | lx) == 0)
        return ny ? at_sub(-pi2, tiny) : at_add(pi2, tiny);
    if (ix == UINT32_C(0x7ff00000)) {
        if (iy == UINT32_C(0x7ff00000)) {
            switch (quadrant) {
                case 0:
                    return at_add(pi4, tiny);
                case 1:
                    return at_sub(-pi4, tiny);
                case 2:
                    return at_add(at_mul(3.0, pi4), tiny);
                default:
                    return at_sub(at_mul(-3.0, pi4), tiny);
            }
        }
        switch (quadrant) {
            case 0:
                return 0.0;
            case 1:
                return -0.0;
            case 2:
                return at_add(pi, tiny);
            default:
                return at_sub(-pi, tiny);
        }
    }
    if (iy == UINT32_C(0x7ff00000))
        return ny ? at_sub(-pi2, tiny) : at_add(pi2, tiny);
    const int32_t exponent = mc_java_i32_shr((int32_t)iy - (int32_t)ix, 20);
    double z;
    if (exponent > 60)
        z = at_add(pi2, at_mul(.5, lo));
    else if (nx && exponent < -60)
        z = 0.0;
    else {
        double ratio = at_div(y, x);
        uint64_t bits;
        memcpy(&bits, &ratio, 8);
        bits &= UINT64_C(0x7fffffffffffffff);
        memcpy(&ratio, &bits, 8);
        z = mc_action_atan(ratio);
    }
    switch (quadrant) {
        case 0:
            return z;
        case 1: {
            uint64_t bits;
            memcpy(&bits, &z, 8);
            bits ^= UINT64_C(0x8000000000000000);
            memcpy(&z, &bits, 8);
            return z;
        }
        case 2:
            return at_sub(pi, at_sub(z, lo));
        default:
            return at_sub(at_sub(z, lo), pi);
    }
}
