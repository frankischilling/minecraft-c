#include "angle_internal.h"
#include <float.h>
#include <stdint.h>
#include <string.h>
_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128 &&
                   sizeof(float) == sizeof(uint32_t),
               "binary32 required");

/* Exact finite binary32 remainder by the source constant360. Integer reduction
 * avoids host-libm rounding; every resulting grid point is representable. */
float mc_action_angle_remainder(float value) {
    uint32_t raw;
    memcpy(&raw, &value, 4);
    const uint32_t sign = raw & UINT32_C(0x80000000), absolute = raw & UINT32_C(0x7fffffff);
    if (absolute >= UINT32_C(0x7f800000)) {
        raw = absolute > UINT32_C(0x7f800000) ? raw | UINT32_C(0x00400000) : UINT32_C(0xffc00000);
        memcpy(&value, &raw, 4);
        return value;
    }
    if (absolute < UINT32_C(0x43b40000))
        return value;
    const uint32_t significand = (absolute & UINT32_C(0x007fffff)) | UINT32_C(0x00800000);
    const int shift = (int)(absolute >> 23) - 150;
    uint32_t remainder;
    if (shift >= 0) {
        remainder = significand % 360u;
        for (int i = 0; i < shift; ++i)
            remainder = (remainder * 2u) % 360u;
        value = (float)remainder;
    } else {
        remainder = significand % (360u << (unsigned int)(-shift));
        uint32_t scale = (uint32_t)(127 + shift) << 23;
        float multiplier;
        memcpy(&multiplier, &scale, 4);
        volatile float product = (float)remainder * multiplier;
        value = product;
    }
    memcpy(&raw, &value, 4);
    raw |= sign;
    memcpy(&value, &raw, 4);
    return value;
}
