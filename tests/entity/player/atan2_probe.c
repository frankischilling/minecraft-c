#include "../../../src/entity/player/tick_math_internal.h"
#include <inttypes.h>
#include <stdio.h>
static const uint64_t edge[] = {0,
                                UINT64_C(0x8000000000000000),
                                1,
                                UINT64_C(0x8000000000000001),
                                UINT64_C(0x000fffffffffffff),
                                UINT64_C(0x0010000000000000),
                                UINT64_C(0x3c30000000000000),
                                UINT64_C(0xbc30000000000000),
                                UINT64_C(0x3ff0000000000000),
                                UINT64_C(0xbff0000000000000),
                                UINT64_C(0x3fe0000000000000),
                                UINT64_C(0xbfe0000000000000),
                                UINT64_C(0x43b0000000000000),
                                UINT64_C(0xc3b0000000000000),
                                UINT64_C(0x7fefffffffffffff),
                                UINT64_C(0xffefffffffffffff),
                                UINT64_C(0x7ff0000000000000),
                                UINT64_C(0xfff0000000000000),
                                UINT64_C(0x7ff8123456789abc),
                                UINT64_C(0xfff8456789abcdef),
                                UINT64_C(0x7ff0123456789abc),
                                UINT64_C(0xfff0456789abcdef)};

static int row(size_t i, uint64_t yb, uint64_t xb) {
    double y, x;
    memcpy(&y, &yb, 8);
    memcpy(&x, &xb, 8);
    const double value = mc_player_tick_atan2(y, x);
    uint64_t out;
    memcpy(&out, &value, 8);
    return printf("ATAN2 %zu %016" PRIx64 " %016" PRIx64 " %016" PRIx64 "\n", i, yb, xb, out) < 0
               ? 1
               : 0;
}

int main(void) {
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return 1;
    size_t count = 0;
    for (size_t y = 0; y < sizeof(edge) / sizeof(edge[0]); ++y)
        for (size_t x = 0; x < sizeof(edge) / sizeof(edge[0]); ++x)
            if (row(count++, edge[y], edge[x]))
                return 1;
    uint64_t state = UINT64_C(0x781a3e6c904b5d2f);
    for (size_t i = 0; i < 8192; ++i) {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        const uint64_t y = state;
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        if (row(count++, y, state))
            return 1;
    }
    return printf("Player atan2: %zu records\n", count) < 0 ? 1 : 0;
}
