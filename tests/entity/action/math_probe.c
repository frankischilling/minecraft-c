#include "../../../src/entity/action/angle_internal.h"
#include "../../../src/entity/action/atan_internal.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <xmmintrin.h>
static const uint64_t edges[] = {0,
                                 UINT64_C(0x8000000000000000),
                                 1,
                                 UINT64_C(0x8000000000000001),
                                 UINT64_C(0x000fffffffffffff),
                                 UINT64_C(0x0010000000000000),
                                 UINT64_C(0x3e1fffffffffffff),
                                 UINT64_C(0x3e20000000000000),
                                 UINT64_C(0x3fdbffffffffffff),
                                 UINT64_C(0x3fdc000000000000),
                                 UINT64_C(0x3fe5ffffffffffff),
                                 UINT64_C(0x3fe6000000000000),
                                 UINT64_C(0x3ff2ffffffffffff),
                                 UINT64_C(0x3ff3000000000000),
                                 UINT64_C(0x40037fffffffffff),
                                 UINT64_C(0x4003800000000000),
                                 UINT64_C(0x440fffffffffffff),
                                 UINT64_C(0x4410000000000000),
                                 UINT64_C(0x7fefffffffffffff),
                                 UINT64_C(0xffefffffffffffff),
                                 UINT64_C(0x7ff0000000000000),
                                 UINT64_C(0xfff0000000000000),
                                 UINT64_C(0x7ff8123456789abc),
                                 UINT64_C(0xfff8456789abcdef),
                                 UINT64_C(0x7ff0123456789abc),
                                 UINT64_C(0xfff0456789abcdef)};
static const uint32_t angle_edges[] = {0,
                                       UINT32_C(0x80000000),
                                       1,
                                       UINT32_C(0x80000001),
                                       UINT32_C(0x007fffff),
                                       UINT32_C(0x00800000),
                                       UINT32_C(0x43b3ffff),
                                       UINT32_C(0x43b40000),
                                       UINT32_C(0x43b40001),
                                       UINT32_C(0xc3b40000),
                                       UINT32_C(0x7f7fffff),
                                       UINT32_C(0xff7fffff),
                                       UINT32_C(0x7f800000),
                                       UINT32_C(0xff800000),
                                       UINT32_C(0x7fc12345),
                                       UINT32_C(0xffc23456),
                                       UINT32_C(0x7f812345),
                                       UINT32_C(0xff823456)};

int main(void) {
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return 1;
    uint64_t state = UINT64_C(0x910d334a5b67c8ef);
    for (size_t i = 0; i < 8192; ++i) {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        uint64_t bits = i < sizeof(edges) / sizeof(edges[0]) ? edges[i] : state;
        double value;
        memcpy(&value, &bits, 8);
        const double result = mc_action_atan(value);
        uint64_t out;
        memcpy(&out, &result, 8);
        printf("ATAN %zu %016" PRIx64 " %016" PRIx64 "\n", i, bits, out);
        uint32_t fb = i < sizeof(angle_edges) / sizeof(angle_edges[0]) ? angle_edges[i]
                                                                       : (uint32_t)(state >> 32);
        float f;
        memcpy(&f, &fb, 4);
        const float r = mc_action_angle_remainder(f);
        uint32_t rb;
        memcpy(&rb, &r, 4);
        printf("ANGLE %zu %08" PRIx32 " %08" PRIx32 "\n", i, fb, rb);
    }
    puts("Action math: 16384 records");
    return ferror(stdout) ? 1 : 0;
}
