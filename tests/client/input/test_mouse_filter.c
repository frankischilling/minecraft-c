#include "mc/entity/player/mouse_filter.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <xmmintrin.h>

static unsigned int checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "Mouse filter check%u at%d: %s\n", checks, __LINE__, #value);          \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static float from_bits(uint32_t raw) {
    float value;
    memcpy(&value, &raw, sizeof(value));
    return value;
}

static uint32_t bits(const float *value) {
    uint32_t raw;
    memcpy(&raw, value, sizeof(raw));
    return raw;
}

int main(void) {
    static const uint32_t initial[][3] = {
        {0, 0, 0},
        {UINT32_C(0xc0000000), UINT32_C(0x40400000), UINT32_C(0xbf000000)},
        {UINT32_C(1), UINT32_C(0x80000001), UINT32_C(0x80000000)},
        {UINT32_C(0x7f7fffff), UINT32_C(0xff7fffff), UINT32_C(0x3f800000)},
        {UINT32_C(0x7fa12345), 0, 0},
        {0, 0, UINT32_C(0xffc54321)},
        {UINT32_C(0x7f800000), UINT32_C(0x7f800000), UINT32_C(0x80000000)},
        {0, UINT32_C(0xff800000), UINT32_C(0x7fc23456)}};
    static const uint32_t steps[][2] = {{UINT32_C(0x3f800000), UINT32_C(0x3d4ccccd)},
                                        {UINT32_C(0x3f800000), UINT32_C(0x3d4ccccd)},
                                        {UINT32_C(0xc0000000), UINT32_C(0x3d4ccccd)},
                                        {0, UINT32_C(0x3d4ccccd)},
                                        {UINT32_C(0x40a00000), UINT32_C(0x3f000000)},
                                        {UINT32_C(0xc1200000), UINT32_C(0x3f800000)},
                                        {UINT32_C(0x7f7fffff), 0},
                                        {UINT32_C(1), UINT32_C(0x3d4ccccd)},
                                        {UINT32_C(0x80000000), UINT32_C(0x40000000)},
                                        {UINT32_C(0xbf800000), UINT32_C(0xbf800000)},
                                        {UINT32_C(0x7fc12345), UINT32_C(0x3f000000)},
                                        {UINT32_C(0x7f800000), 0}};
    for (size_t mode = 0; mode < sizeof(initial) / sizeof(initial[0]); ++mode) {
        mc_player_mouse_filter_fields fields = {
            from_bits(initial[mode][0]), from_bits(initial[mode][1]), from_bits(initial[mode][2])};
        for (size_t index = 0; index < sizeof(steps) / sizeof(steps[0]); ++index) {
            float out;
            CHECK(mc_player_mouse_filter_apply(&fields, from_bits(steps[index][0]),
                                               from_bits(steps[index][1]), &out) == MC_ENTITY_OK);
            printf("FILTER %zu %zu %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %08" PRIx32 "\n", mode,
                   index, bits(&out), bits(&fields.a), bits(&fields.b), bits(&fields.c));
        }
    }
    mc_player_mouse_filter_fields fields = {1.0f, -2.0f, 3.0f};
    const mc_player_mouse_filter_fields before = fields;
    float out = -73.0f;
    CHECK(mc_player_mouse_filter_apply(NULL, 1.0f, 1.0f, &out) == MC_ENTITY_INVALID_ARGUMENT &&
          out == -73.0f);
    CHECK(mc_player_mouse_filter_apply(&fields, 1.0f, 1.0f, NULL) == MC_ENTITY_INVALID_ARGUMENT &&
          memcmp(&fields, &before, sizeof(fields)) == 0);
    const unsigned int csr = _mm_getcsr();
    _mm_setcsr(csr | 0x8000u);
    const mc_entity_status status = mc_player_mouse_filter_apply(&fields, 1.0f, 1.0f, &out);
    _mm_setcsr(csr);
    CHECK(status == MC_ENTITY_UNSUPPORTED_FP && out == -73.0f &&
          memcmp(&fields, &before, sizeof(fields)) == 0);
    printf("%u mouse filter checks passed\n", checks);
    return 0;
}
