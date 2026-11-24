#include "mc/world/biome/biome.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static unsigned int checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static uint64_t bits_of(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static double from_bits(uint64_t bits) {
    double value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static float float_from_bits(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

/* Observed in both untouched originals before this C implementation was tested.
 * Rows are rainfall-major; temperature is the fast table coordinate. */
static const char observed_table[64][65] = {
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkeeeeeeeeeeeeeeeeeeeeeeeeeeeehhhh",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkeeeeeeeeeeeeeeeeeeeeeeeeeeeehhhh",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkeeeeeeeeeeeeeeeeeeeeeeeeeeeehhhh",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkeeeeeeeeeeeeeeeeeeeeeeeeeeeehhhh",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkeeeeeeeeeeeeeeeeeeeeeeeeeeeehhhh",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkeeeeeeeeeeeeeeeeeeeeeeeeeeeehhhh",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkeeeeeeeeeeeeeeeeeeeeeeeeeeeehhhh",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkeeeeeeeeeeeeeeeeeeeeeeeeeeeehhhh",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkeeeeeeeeeeeeeeeeeeeeeeeeeeeehhhh",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkeeeeeeeeeeeeeeeeeeeeeeeeeeeehhhh",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkeeeeeeeeeeeeeeeeeeeeeeeeeeeehhhh",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkeeeeeeeeeeeeeeeeeeeeeeeeeeeehhhh",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkeeeeeeeeeeeeeeeeeeeeeeeeeeeehhhh",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkeeeeeeeeeeeeeeeeeeeeeeeeeeeehhii",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkeeeeeeeeeeeeeeeeeeeeeeeeefffffii",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkeeeeeeeeeeeeeeeeeeeeefffffffffii",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkeeeeeeeeeeeeeeeeeeffffffffffffii",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkeeeeeeeeeeeeeeefffffffffffffffii",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkeeeeeeeeeeeeefffffffffffffffffii",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkeeeeeeeeeeffffffffffffffffffffii",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkeeeeeeeeffffffffffffffffffffffii",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkeeeeeeffffffffffffffffffffffffii",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkeeeeefffffffffffffffffffffffffii",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkeeeffffffffffffffffffffffffffdii",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkeeffffffffffffffffffffffffddddii",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkffffffffffffffffffffffffddddddii",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkgffffffffffffffffffffffddddddddii",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkggffffffffffffffffffffddddddddddii",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkgggffffffffffffffffffddddddddddddii",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkggggffffffffffffffffddddddddddddddcc",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkgggggfffffffffffffffdddddddddddddddcc",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkggggggfffffffffffffdddddddddddddddddcc",
    "kkkkkkkkkkkkkkkkkkkkkkkkkgggggggffffffffffffddddddddddddddddddcc",
    "kkkkkkkkkkkkkkkkkkkkkkkkkgggggggfffffffffffdddddddddddddddddddcc",
    "kkkkkkkkkkkkkkkkkkkkkkkkggggggggfffffffffdddddddddddddddddddddcc",
    "kkkkkkkkkkkkkkkkkkkkkkkgggggggggffffffffddddddddddddddddddddddcc",
    "kkkkkkkkkkkkkkkkkkkkkkkgggggggggfffffffdddddddddddddddddddddddcc",
    "kkkkkkkkkkkkkkkkkkkkkkggggggggggffffffddddddddddddddddddddddddcc",
    "kkkkkkkkkkkkkkkkkkkkkgggggggggggfffffdddddddddddddddddddddddddcc",
    "kkkkkkkkkkkkkkkkkkkkkgggggggggggffffddddddddddddddddddddddddddcc",
    "kkkkkkkkkkkkkkkkkkkkggggggggggggfffdddddddddddddddddddddddddddcc",
    "kkkkkkkkkkkkkkkkkkkkggggggggggggffddddddddddddddddddddddddddddcc",
    "kkkkkkkkkkkkkkkkkkkgggggggggggggffddddddddddddddddddddddddddddcc",
    "kkkkkkkkkkkkkkkkkkkgggggggggggggfdddddddddddddddddddddddddddddcc",
    "kkkkkkkkkkkkkkkkkkkgggggggggggggddddddddddddddddddddddddddddddcc",
    "kkkkkkkkkkkkkkkkkkggggggggggggggddddddddddddddddddddddddddddddcc",
    "kkkkkkkkkkkkkkkkkkggggggggggggggddddddddddddbdddddddddddddddddcc",
    "kkkkkkkkkkkkkkkkkgggggggggggggggdddddddddddbbdddddddddddddddddcc",
    "kkkkkkkkkkkkkkkkkgggggggggggggggddddddddddbbbdddddddddddddddddcc",
    "kkkkkkkkkkkkkkkkkgggggggggggggggdddddddddbbbbdddddddddddddddddcc",
    "kkkkkkkkkkkkkkkkggggggggggggggggddddddddbbbbbdddddddddddddddddcc",
    "kkkkkkkkkkkkkkkkggggggggggggggggdddddddbbbbbbdddddddddddddddddcc",
    "kkkkkkkkkkkkkkkkggggggggggggggggdddddddbbbbbbdddddddddddddddddcc",
    "kkkkkkkkkkkkkkkgggggggggggggggggddddddbbbbbbbdddddddddddddddddcc",
    "kkkkkkkkkkkkkkkgggggggggggggggggdddddbbbbbbbbdddddddddddddddddcc",
    "kkkkkkkkkkkkkkkgggggggggggggggggdddddbbbbbbbbdddddddddddddddddcc",
    "kkkkkkkkkkkkkkkgggggggggggggggggddddbbbbbbbbbdddddddddddddddddcc",
    "kkkkkkkkkkkkkkggggggggggggggggggdddbbbbbbbbbbdddddddddddddddddca",
    "kkkkkkkkkkkkkkggggggggggggggggggdddbbbbbbbbbbdddddddddddddddddaa",
    "kkkkkkkkkkkkkkggggggggggggggggggddbbbbbbbbbbbdddddddddddddddddaa",
    "kkkkkkkkkkkkkkggggggggggggggggggddbbbbbbbbbbbdddddddddddddddddaa",
    "kkkkkkkkkkkkkkggggggggggggggggggdbbbbbbbbbbbbdddddddddddddddddaa",
    "kkkkkkkkkkkkkgggggggggggggggggggdbbbbbbbbbbbbdddddddddddddddddaa",
    "kkkkkkkkkkkkkgggggggggggggggggggbbbbbbbbbbbbbdddddddddddddddddaa",
};

typedef struct selector_fixture {
    uint64_t temperature;
    uint64_t rainfall;
    char field;
} selector_fixture;

static const selector_fixture observed_floats[] = {
    {UINT64_C(0x00000000), UINT64_C(0x00000000), 'k'},
    {UINT64_C(0x3dcccccc), UINT64_C(0x3f800000), 'k'},
    {UINT64_C(0x3dcccccd), UINT64_C(0x3f800000), 'k'},
    {UINT64_C(0x3f000000), UINT64_C(0x3e4ccccd), 'e'},
    {UINT64_C(0x3f333333), UINT64_C(0x3f800000), 'd'},
    {UINT64_C(0x3f733333), UINT64_C(0x00000000), 'h'},
    {UINT64_C(0x3f7851ec), UINT64_C(0x3f000000), 'c'},
    {UINT64_C(0x3f800000), UINT64_C(0x3f666666), 'a'},
    {UINT64_C(0x3f800000), UINT64_C(0x3f800000), 'a'},
    {UINT64_C(0x00000000), UINT64_C(0x3f800000), 'k'},
    {UINT64_C(0x00000000), UINT64_C(0x80000000), 'k'},
    {UINT64_C(0x3dcccccc), UINT64_C(0x00000000), 'k'},
    {UINT64_C(0x3dcccccc), UINT64_C(0x3dcccccd), 'k'},
    {UINT64_C(0x3dcccccc), UINT64_C(0x3e4ccccd), 'k'},
    {UINT64_C(0x3dcccccc), UINT64_C(0x3eb33333), 'k'},
    {UINT64_C(0x3dcccccc), UINT64_C(0x3ee66666), 'k'},
    {UINT64_C(0x3dcccccc), UINT64_C(0x3f000000), 'k'},
    {UINT64_C(0x3dcccccc), UINT64_C(0x3f666666), 'k'},
    {UINT64_C(0x3dcccccd), UINT64_C(0x00000000), 'k'},
    {UINT64_C(0x3dcccccd), UINT64_C(0x3dcccccd), 'k'},
    {UINT64_C(0x3dcccccd), UINT64_C(0x3e4ccccd), 'k'},
    {UINT64_C(0x3dcccccd), UINT64_C(0x3eb33333), 'k'},
    {UINT64_C(0x3dcccccd), UINT64_C(0x3ee66666), 'k'},
    {UINT64_C(0x3dcccccd), UINT64_C(0x3f000000), 'k'},
    {UINT64_C(0x3dcccccd), UINT64_C(0x3f666666), 'k'},
    {UINT64_C(0x3dccccce), UINT64_C(0x00000000), 'k'},
    {UINT64_C(0x3dccccce), UINT64_C(0x3dcccccd), 'k'},
    {UINT64_C(0x3dccccce), UINT64_C(0x3e4ccccd), 'k'},
    {UINT64_C(0x3dccccce), UINT64_C(0x3eb33333), 'k'},
    {UINT64_C(0x3dccccce), UINT64_C(0x3ee66666), 'k'},
    {UINT64_C(0x3dccccce), UINT64_C(0x3f000000), 'k'},
    {UINT64_C(0x3dccccce), UINT64_C(0x3f666666), 'k'},
    {UINT64_C(0x3dccccce), UINT64_C(0x3f800000), 'k'},
    {UINT64_C(0x3ecccccd), UINT64_C(0x3effffff), 'k'},
    {UINT64_C(0x3ecccccd), UINT64_C(0x3f000000), 'g'},
    {UINT64_C(0x3ecccccd), UINT64_C(0x3f000001), 'g'},
    {UINT64_C(0x3ecccccd), UINT64_C(0x3f5fffff), 'g'},
    {UINT64_C(0x3ecccccd), UINT64_C(0x3f600000), 'g'},
    {UINT64_C(0x3ecccccd), UINT64_C(0x3f600001), 'g'},
    {UINT64_C(0x3effffff), UINT64_C(0x00000000), 'k'},
    {UINT64_C(0x3effffff), UINT64_C(0x3dcccccd), 'k'},
    {UINT64_C(0x3effffff), UINT64_C(0x3e4ccccd), 'k'},
    {UINT64_C(0x3effffff), UINT64_C(0x3eb33333), 'k'},
    {UINT64_C(0x3effffff), UINT64_C(0x3ee66666), 'g'},
    {UINT64_C(0x3effffff), UINT64_C(0x3f000000), 'g'},
    {UINT64_C(0x3effffff), UINT64_C(0x3f666666), 'g'},
    {UINT64_C(0x3effffff), UINT64_C(0x3f800000), 'g'},
    {UINT64_C(0x3f000000), UINT64_C(0x00000000), 'e'},
    {UINT64_C(0x3f000000), UINT64_C(0x3dcccccd), 'e'},
    {UINT64_C(0x3f000000), UINT64_C(0x3eb33333), 'e'},
    {UINT64_C(0x3f000000), UINT64_C(0x3ecccccc), 'e'},
    {UINT64_C(0x3f000000), UINT64_C(0x3ecccccd), 'f'},
    {UINT64_C(0x3f000000), UINT64_C(0x3eccccce), 'f'},
    {UINT64_C(0x3f000000), UINT64_C(0x3ee66666), 'f'},
    {UINT64_C(0x3f000000), UINT64_C(0x3f000000), 'f'},
    {UINT64_C(0x3f000000), UINT64_C(0x3f333332), 'f'},
    {UINT64_C(0x3f000000), UINT64_C(0x3f333333), 'd'},
    {UINT64_C(0x3f000000), UINT64_C(0x3f333334), 'd'},
    {UINT64_C(0x3f000000), UINT64_C(0x3f666665), 'd'},
    {UINT64_C(0x3f000000), UINT64_C(0x3f666666), 'd'},
    {UINT64_C(0x3f000000), UINT64_C(0x3f666667), 'd'},
    {UINT64_C(0x3f000000), UINT64_C(0x3f7fffff), 'd'},
    {UINT64_C(0x3f000000), UINT64_C(0x3f800000), 'd'},
    {UINT64_C(0x3f000001), UINT64_C(0x00000000), 'e'},
    {UINT64_C(0x3f000001), UINT64_C(0x3dcccccd), 'e'},
    {UINT64_C(0x3f000001), UINT64_C(0x3e4ccccd), 'e'},
    {UINT64_C(0x3f000001), UINT64_C(0x3eb33333), 'e'},
    {UINT64_C(0x3f000001), UINT64_C(0x3ee66666), 'f'},
    {UINT64_C(0x3f000001), UINT64_C(0x3f000000), 'f'},
    {UINT64_C(0x3f000001), UINT64_C(0x3f666666), 'd'},
    {UINT64_C(0x3f000001), UINT64_C(0x3f800000), 'b'},
    {UINT64_C(0x3f19999a), UINT64_C(0x3eaaaaa9), 'e'},
    {UINT64_C(0x3f19999a), UINT64_C(0x3eaaaaaa), 'f'},
    {UINT64_C(0x3f19999a), UINT64_C(0x3eaaaaab), 'f'},
    {UINT64_C(0x3f19999a), UINT64_C(0x3f155554), 'f'},
    {UINT64_C(0x3f19999a), UINT64_C(0x3f155555), 'd'},
    {UINT64_C(0x3f19999a), UINT64_C(0x3f155556), 'd'},
    {UINT64_C(0x3f19999a), UINT64_C(0x3f3ffffe), 'd'},
    {UINT64_C(0x3f19999a), UINT64_C(0x3f3fffff), 'd'},
    {UINT64_C(0x3f19999a), UINT64_C(0x3f400000), 'd'},
    {UINT64_C(0x3f19999a), UINT64_C(0x3f555554), 'd'},
    {UINT64_C(0x3f19999a), UINT64_C(0x3f555555), 'd'},
    {UINT64_C(0x3f19999a), UINT64_C(0x3f555556), 'b'},
    {UINT64_C(0x3f333332), UINT64_C(0x00000000), 'e'},
    {UINT64_C(0x3f333332), UINT64_C(0x3dcccccd), 'e'},
    {UINT64_C(0x3f333332), UINT64_C(0x3e4ccccd), 'e'},
    {UINT64_C(0x3f333332), UINT64_C(0x3eb33333), 'f'},
    {UINT64_C(0x3f333332), UINT64_C(0x3ee66666), 'f'},
    {UINT64_C(0x3f333332), UINT64_C(0x3f000000), 'f'},
    {UINT64_C(0x3f333332), UINT64_C(0x3f666666), 'b'},
    {UINT64_C(0x3f333332), UINT64_C(0x3f800000), 'b'},
    {UINT64_C(0x3f333333), UINT64_C(0x00000000), 'e'},
    {UINT64_C(0x3f333333), UINT64_C(0x3dcccccd), 'e'},
    {UINT64_C(0x3f333333), UINT64_C(0x3e4ccccd), 'e'},
    {UINT64_C(0x3f333333), UINT64_C(0x3e924924), 'e'},
    {UINT64_C(0x3f333333), UINT64_C(0x3e924925), 'f'},
    {UINT64_C(0x3f333333), UINT64_C(0x3e924926), 'f'},
    {UINT64_C(0x3f333333), UINT64_C(0x3eb33333), 'f'},
    {UINT64_C(0x3f333333), UINT64_C(0x3ee66666), 'f'},
    {UINT64_C(0x3f333333), UINT64_C(0x3effffff), 'f'},
    {UINT64_C(0x3f333333), UINT64_C(0x3f000000), 'd'},
    {UINT64_C(0x3f333333), UINT64_C(0x3f000001), 'd'},
    {UINT64_C(0x3f333333), UINT64_C(0x3f249248), 'd'},
    {UINT64_C(0x3f333333), UINT64_C(0x3f249249), 'd'},
    {UINT64_C(0x3f333333), UINT64_C(0x3f24924a), 'd'},
    {UINT64_C(0x3f333333), UINT64_C(0x3f36db6d), 'd'},
    {UINT64_C(0x3f333333), UINT64_C(0x3f36db6e), 'd'},
    {UINT64_C(0x3f333333), UINT64_C(0x3f36db6f), 'd'},
    {UINT64_C(0x3f333333), UINT64_C(0x3f666666), 'd'},
    {UINT64_C(0x3f333334), UINT64_C(0x00000000), 'e'},
    {UINT64_C(0x3f333334), UINT64_C(0x3dcccccd), 'e'},
    {UINT64_C(0x3f333334), UINT64_C(0x3e4ccccd), 'e'},
    {UINT64_C(0x3f333334), UINT64_C(0x3eb33333), 'f'},
    {UINT64_C(0x3f333334), UINT64_C(0x3ee66666), 'f'},
    {UINT64_C(0x3f333334), UINT64_C(0x3f000000), 'd'},
    {UINT64_C(0x3f333334), UINT64_C(0x3f666666), 'd'},
    {UINT64_C(0x3f333334), UINT64_C(0x3f800000), 'd'},
    {UINT64_C(0x3f733332), UINT64_C(0x00000000), 'e'},
    {UINT64_C(0x3f733332), UINT64_C(0x3dcccccd), 'e'},
    {UINT64_C(0x3f733332), UINT64_C(0x3e4ccccd), 'e'},
    {UINT64_C(0x3f733332), UINT64_C(0x3eb33333), 'f'},
    {UINT64_C(0x3f733332), UINT64_C(0x3ee66666), 'd'},
    {UINT64_C(0x3f733332), UINT64_C(0x3f000000), 'd'},
    {UINT64_C(0x3f733332), UINT64_C(0x3f666666), 'd'},
    {UINT64_C(0x3f733332), UINT64_C(0x3f800000), 'd'},
    {UINT64_C(0x3f733333), UINT64_C(0x3dcccccd), 'h'},
    {UINT64_C(0x3f733333), UINT64_C(0x3e4ccccd), 'h'},
    {UINT64_C(0x3f733333), UINT64_C(0x3e579435), 'h'},
    {UINT64_C(0x3f733333), UINT64_C(0x3e579436), 'f'},
    {UINT64_C(0x3f733333), UINT64_C(0x3e579437), 'f'},
    {UINT64_C(0x3f733333), UINT64_C(0x3eb33333), 'f'},
    {UINT64_C(0x3f733333), UINT64_C(0x3ebca1ae), 'f'},
    {UINT64_C(0x3f733333), UINT64_C(0x3ebca1af), 'd'},
    {UINT64_C(0x3f733333), UINT64_C(0x3ebca1b0), 'd'},
    {UINT64_C(0x3f733333), UINT64_C(0x3ee66666), 'd'},
    {UINT64_C(0x3f733333), UINT64_C(0x3ef286bb), 'd'},
    {UINT64_C(0x3f733333), UINT64_C(0x3ef286bc), 'd'},
    {UINT64_C(0x3f733333), UINT64_C(0x3ef286bd), 'd'},
    {UINT64_C(0x3f733333), UINT64_C(0x3f000000), 'd'},
    {UINT64_C(0x3f733333), UINT64_C(0x3f06bca1), 'd'},
    {UINT64_C(0x3f733333), UINT64_C(0x3f06bca2), 'd'},
    {UINT64_C(0x3f733333), UINT64_C(0x3f06bca3), 'd'},
    {UINT64_C(0x3f733333), UINT64_C(0x3f666666), 'd'},
    {UINT64_C(0x3f733333), UINT64_C(0x3f7286bb), 'd'},
    {UINT64_C(0x3f733333), UINT64_C(0x3f7286bc), 'd'},
    {UINT64_C(0x3f733333), UINT64_C(0x3f7286bd), 'd'},
    {UINT64_C(0x3f733333), UINT64_C(0x3f800000), 'd'},
    {UINT64_C(0x3f733334), UINT64_C(0x00000000), 'h'},
    {UINT64_C(0x3f733334), UINT64_C(0x3dcccccd), 'h'},
    {UINT64_C(0x3f733334), UINT64_C(0x3e4ccccd), 'h'},
    {UINT64_C(0x3f733334), UINT64_C(0x3eb33333), 'f'},
    {UINT64_C(0x3f733334), UINT64_C(0x3ee66666), 'd'},
    {UINT64_C(0x3f733334), UINT64_C(0x3f000000), 'd'},
    {UINT64_C(0x3f733334), UINT64_C(0x3f666666), 'd'},
    {UINT64_C(0x3f733334), UINT64_C(0x3f800000), 'd'},
    {UINT64_C(0x3f7851eb), UINT64_C(0x00000000), 'h'},
    {UINT64_C(0x3f7851eb), UINT64_C(0x3dcccccd), 'h'},
    {UINT64_C(0x3f7851eb), UINT64_C(0x3e4ccccd), 'h'},
    {UINT64_C(0x3f7851eb), UINT64_C(0x3eb33333), 'f'},
    {UINT64_C(0x3f7851eb), UINT64_C(0x3ee66666), 'd'},
    {UINT64_C(0x3f7851eb), UINT64_C(0x3f000000), 'd'},
    {UINT64_C(0x3f7851eb), UINT64_C(0x3f666666), 'd'},
    {UINT64_C(0x3f7851eb), UINT64_C(0x3f800000), 'd'},
    {UINT64_C(0x3f7851ec), UINT64_C(0x00000000), 'h'},
    {UINT64_C(0x3f7851ec), UINT64_C(0x3dcccccd), 'h'},
    {UINT64_C(0x3f7851ec), UINT64_C(0x3e4ccccd), 'h'},
    {UINT64_C(0x3f7851ec), UINT64_C(0x3e53224e), 'h'},
    {UINT64_C(0x3f7851ec), UINT64_C(0x3e53224f), 'i'},
    {UINT64_C(0x3f7851ec), UINT64_C(0x3e532250), 'i'},
    {UINT64_C(0x3f7851ec), UINT64_C(0x3eb33333), 'i'},
    {UINT64_C(0x3f7851ec), UINT64_C(0x3eb8be04), 'i'},
    {UINT64_C(0x3f7851ec), UINT64_C(0x3eb8be05), 'i'},
    {UINT64_C(0x3f7851ec), UINT64_C(0x3eb8be06), 'i'},
    {UINT64_C(0x3f7851ec), UINT64_C(0x3ee66666), 'i'},
    {UINT64_C(0x3f7851ec), UINT64_C(0x3eed8697), 'i'},
    {UINT64_C(0x3f7851ec), UINT64_C(0x3eed8698), 'c'},
    {UINT64_C(0x3f7851ec), UINT64_C(0x3eed8699), 'c'},
    {UINT64_C(0x3f7851ec), UINT64_C(0x3f03f570), 'c'},
    {UINT64_C(0x3f7851ec), UINT64_C(0x3f03f571), 'c'},
    {UINT64_C(0x3f7851ec), UINT64_C(0x3f03f572), 'c'},
    {UINT64_C(0x3f7851ec), UINT64_C(0x3f666666), 'c'},
    {UINT64_C(0x3f7851ec), UINT64_C(0x3f6d8697), 'c'},
    {UINT64_C(0x3f7851ec), UINT64_C(0x3f6d8698), 'a'},
    {UINT64_C(0x3f7851ec), UINT64_C(0x3f6d8699), 'a'},
    {UINT64_C(0x3f7851ec), UINT64_C(0x3f800000), 'a'},
    {UINT64_C(0x3f7851ed), UINT64_C(0x00000000), 'h'},
    {UINT64_C(0x3f7851ed), UINT64_C(0x3dcccccd), 'h'},
    {UINT64_C(0x3f7851ed), UINT64_C(0x3e4ccccd), 'h'},
    {UINT64_C(0x3f7851ed), UINT64_C(0x3eb33333), 'i'},
    {UINT64_C(0x3f7851ed), UINT64_C(0x3ee66666), 'i'},
    {UINT64_C(0x3f7851ed), UINT64_C(0x3f000000), 'c'},
    {UINT64_C(0x3f7851ed), UINT64_C(0x3f666666), 'c'},
    {UINT64_C(0x3f7851ed), UINT64_C(0x3f800000), 'a'},
    {UINT64_C(0x3f800000), UINT64_C(0x00000000), 'h'},
    {UINT64_C(0x3f800000), UINT64_C(0x3e4ccccc), 'h'},
    {UINT64_C(0x3f800000), UINT64_C(0x3e4ccccd), 'i'},
    {UINT64_C(0x3f800000), UINT64_C(0x3e4cccce), 'i'},
    {UINT64_C(0x3f800000), UINT64_C(0x3eb33332), 'i'},
    {UINT64_C(0x3f800000), UINT64_C(0x3eb33333), 'i'},
    {UINT64_C(0x3f800000), UINT64_C(0x3eb33334), 'i'},
    {UINT64_C(0x3f800000), UINT64_C(0x3ee66665), 'i'},
    {UINT64_C(0x3f800000), UINT64_C(0x3ee66666), 'c'},
    {UINT64_C(0x3f800000), UINT64_C(0x3ee66667), 'c'},
    {UINT64_C(0x3f800000), UINT64_C(0x3effffff), 'c'},
    {UINT64_C(0x3f800000), UINT64_C(0x3f000000), 'c'},
    {UINT64_C(0x3f800000), UINT64_C(0x3f000001), 'c'},
    {UINT64_C(0x3f800000), UINT64_C(0x3f666665), 'c'},
    {UINT64_C(0x3f800000), UINT64_C(0x3f666667), 'a'},
    {UINT64_C(0x3f800000), UINT64_C(0x80000000), 'h'},
    {UINT64_C(0x80000000), UINT64_C(0x00000000), 'k'},
    {UINT64_C(0x80000000), UINT64_C(0x3f800000), 'k'},
    {UINT64_C(0x80000000), UINT64_C(0x80000000), 'k'},
};

static const selector_fixture observed_doubles[] = {
    {UINT64_C(0x0000000000000000), UINT64_C(0x0000000000000000), 'k'},
    {UINT64_C(0x3fb999999999999a), UINT64_C(0x3ff0000000000000), 'k'},
    {UINT64_C(0x3fe0000000000000), UINT64_C(0x3fc999999999999a), 'k'},
    {UINT64_C(0x3fe6666666666666), UINT64_C(0x3ff0000000000000), 'b'},
    {UINT64_C(0x3fee666666666666), UINT64_C(0x0000000000000000), 'e'},
    {UINT64_C(0x3fef0a3d70a3d70a), UINT64_C(0x3fe0000000000000), 'd'},
    {UINT64_C(0x3ff0000000000000), UINT64_C(0x3feccccccccccccd), 'c'},
    {UINT64_C(0x3ff0000000000000), UINT64_C(0x3ff0000000000000), 'a'},
    {UINT64_C(0x3f9041041041040f), UINT64_C(0x3fe0000000000000), 'k'},
    {UINT64_C(0x3f90410410410410), UINT64_C(0x3fe0000000000000), 'k'},
    {UINT64_C(0x3fca69a69a69a699), UINT64_C(0x3ff0000000000000), 'k'},
    {UINT64_C(0x3fca69a69a69a69a), UINT64_C(0x3ff0000000000000), 'g'},
    {UINT64_C(0x3fca69a69a69a69b), UINT64_C(0x3ff0000000000000), 'g'},
    {UINT64_C(0x3fce79e79e79e79d), UINT64_C(0x3feccccccccccccd), 'k'},
    {UINT64_C(0x3fce79e79e79e79e), UINT64_C(0x3feccccccccccccd), 'g'},
    {UINT64_C(0x3fce79e79e79e79f), UINT64_C(0x3feccccccccccccd), 'g'},
    {UINT64_C(0x3fda69a69a69a699), UINT64_C(0x3fe0000000000000), 'k'},
    {UINT64_C(0x3fda69a69a69a69a), UINT64_C(0x3fe0000000000000), 'g'},
    {UINT64_C(0x3fda69a69a69a69b), UINT64_C(0x3fe0000000000000), 'g'},
    {UINT64_C(0x3fe0000000000000), UINT64_C(0x3fda69a69a69a699), 'k'},
    {UINT64_C(0x3fe0000000000000), UINT64_C(0x3fda69a69a69a69a), 'g'},
    {UINT64_C(0x3fe0000000000000), UINT64_C(0x3fda69a69a69a69b), 'g'},
    {UINT64_C(0x3fe041041041040f), UINT64_C(0x0000000000000000), 'k'},
    {UINT64_C(0x3fe0410410410410), UINT64_C(0x0000000000000000), 'e'},
    {UINT64_C(0x3fe041041041040f), UINT64_C(0x3fc999999999999a), 'k'},
    {UINT64_C(0x3fe0410410410410), UINT64_C(0x3fc999999999999a), 'e'},
    {UINT64_C(0x3fe041041041040f), UINT64_C(0x3fe0000000000000), 'g'},
    {UINT64_C(0x3fe0410410410410), UINT64_C(0x3fe0000000000000), 'f'},
    {UINT64_C(0x3fe041041041040f), UINT64_C(0x3feccccccccccccd), 'g'},
    {UINT64_C(0x3fe0410410410410), UINT64_C(0x3feccccccccccccd), 'd'},
    {UINT64_C(0x3fe041041041040f), UINT64_C(0x3ff0000000000000), 'g'},
    {UINT64_C(0x3fe0410410410410), UINT64_C(0x3ff0000000000000), 'b'},
    {UINT64_C(0x3fe0410410410411), UINT64_C(0x0000000000000000), 'e'},
    {UINT64_C(0x3fe0410410410411), UINT64_C(0x3fc999999999999a), 'e'},
    {UINT64_C(0x3fe0410410410411), UINT64_C(0x3fe0000000000000), 'f'},
    {UINT64_C(0x3fe0410410410411), UINT64_C(0x3feccccccccccccd), 'd'},
    {UINT64_C(0x3fe0410410410411), UINT64_C(0x3ff0000000000000), 'b'},
    {UINT64_C(0x3fe2492492492491), UINT64_C(0x3feccccccccccccd), 'd'},
    {UINT64_C(0x3fe2492492492492), UINT64_C(0x3feccccccccccccd), 'b'},
    {UINT64_C(0x3fe2492492492493), UINT64_C(0x3feccccccccccccd), 'b'},
    {UINT64_C(0x3fe6db6db6db6db6), UINT64_C(0x3fe0000000000000), 'f'},
    {UINT64_C(0x3fe6db6db6db6db7), UINT64_C(0x3fe0000000000000), 'd'},
    {UINT64_C(0x3fe6db6db6db6db6), UINT64_C(0x3feccccccccccccd), 'b'},
    {UINT64_C(0x3fe6db6db6db6db7), UINT64_C(0x3feccccccccccccd), 'd'},
    {UINT64_C(0x3fe6db6db6db6db6), UINT64_C(0x3ff0000000000000), 'b'},
    {UINT64_C(0x3fe6db6db6db6db7), UINT64_C(0x3ff0000000000000), 'd'},
    {UINT64_C(0x3fe6db6db6db6db8), UINT64_C(0x3fe0000000000000), 'd'},
    {UINT64_C(0x3fe6db6db6db6db8), UINT64_C(0x3feccccccccccccd), 'd'},
    {UINT64_C(0x3fe6db6db6db6db8), UINT64_C(0x3ff0000000000000), 'd'},
    {UINT64_C(0x3feccccccccccccd), UINT64_C(0x3fce79e79e79e79d), 'e'},
    {UINT64_C(0x3feccccccccccccd), UINT64_C(0x3fce79e79e79e79e), 'f'},
    {UINT64_C(0x3feccccccccccccd), UINT64_C(0x3fce79e79e79e79f), 'f'},
    {UINT64_C(0x3feccccccccccccd), UINT64_C(0x3fd9659659659658), 'f'},
    {UINT64_C(0x3feccccccccccccd), UINT64_C(0x3fd9659659659659), 'd'},
    {UINT64_C(0x3feccccccccccccd), UINT64_C(0x3fd965965965965a), 'd'},
    {UINT64_C(0x3fee79e79e79e79d), UINT64_C(0x0000000000000000), 'e'},
    {UINT64_C(0x3fee79e79e79e79e), UINT64_C(0x0000000000000000), 'h'},
    {UINT64_C(0x3fee79e79e79e79d), UINT64_C(0x3fc999999999999a), 'e'},
    {UINT64_C(0x3fee79e79e79e79e), UINT64_C(0x3fc999999999999a), 'h'},
    {UINT64_C(0x3fee79e79e79e79f), UINT64_C(0x0000000000000000), 'h'},
    {UINT64_C(0x3fee79e79e79e79f), UINT64_C(0x3fc999999999999a), 'h'},
    {UINT64_C(0x3fef7df7df7df7de), UINT64_C(0x3fe0000000000000), 'd'},
    {UINT64_C(0x3fef7df7df7df7df), UINT64_C(0x3fe0000000000000), 'c'},
    {UINT64_C(0x3fef7df7df7df7de), UINT64_C(0x3feccccccccccccd), 'd'},
    {UINT64_C(0x3fef7df7df7df7df), UINT64_C(0x3feccccccccccccd), 'c'},
    {UINT64_C(0x3fef7df7df7df7de), UINT64_C(0x3ff0000000000000), 'd'},
    {UINT64_C(0x3fef7df7df7df7df), UINT64_C(0x3ff0000000000000), 'a'},
    {UINT64_C(0x3fef7df7df7df7e0), UINT64_C(0x3fe0000000000000), 'c'},
    {UINT64_C(0x3fef7df7df7df7e0), UINT64_C(0x3feccccccccccccd), 'c'},
    {UINT64_C(0x3fef7df7df7df7e0), UINT64_C(0x3ff0000000000000), 'a'},
    {UINT64_C(0x3ff0000000000000), UINT64_C(0x3fca69a69a69a699), 'h'},
    {UINT64_C(0x3ff0000000000000), UINT64_C(0x3fca69a69a69a69a), 'i'},
    {UINT64_C(0x3ff0000000000000), UINT64_C(0x3fca69a69a69a69b), 'i'},
    {UINT64_C(0x3ff0000000000000), UINT64_C(0x3fdd75d75d75d75c), 'i'},
    {UINT64_C(0x3ff0000000000000), UINT64_C(0x3fdd75d75d75d75d), 'c'},
    {UINT64_C(0x3ff0000000000000), UINT64_C(0x3fdd75d75d75d75e), 'c'},
    {UINT64_C(0x3ff0000000000000), UINT64_C(0x3fecf3cf3cf3cf3c), 'c'},
    {UINT64_C(0x3ff0000000000000), UINT64_C(0x3fecf3cf3cf3cf3d), 'a'},
    {UINT64_C(0x3ff0000000000000), UINT64_C(0x3fecf3cf3cf3cf3e), 'a'},
};

typedef struct climate_fixture {
    int64_t seed;
    int32_t x;
    int32_t z;
    uint64_t initial[3][4];
    const char *biomes;
    uint64_t temperature[9];
    uint64_t auxiliary[6];
    uint64_t point[3];
    char point_biome;
} climate_fixture;

/* Original manager calls: direct 2x2, disjoint temperature 2x3 into length9,
 * cached 4x4, then point(x+1,z-1). Original-side setup records exact lengths. */
static const climate_fixture observed_climates[] = {
    {0,
     0,
     0,
     {{UINT64_C(0x3fed65e1be63da72), UINT64_C(0x3fed7669139c629f), UINT64_C(0x3fed6d9d071b1f24),
       UINT64_C(0x3fed81eaa789b8c0)},
      {UINT64_C(0x3fe085574d0ece45), UINT64_C(0x3fe0ab4677e0ae99), UINT64_C(0x3fe08eb447e9d8bd),
       UINT64_C(0x3fe0bb9c20438673)},
      {UINT64_C(0x3fb44909c6b8b90a), UINT64_C(0x3fd3bfa72f02c26a), UINT64_C(0x3fcc65668b2797db),
       UINT64_C(0x3fe05f200c0af9d7)}},
     "dddd",
     {UINT64_C(0x3fed65e1be63da72), UINT64_C(0x3fed7669139c629f), UINT64_C(0x3fed7cfbe4f1c12c),
      UINT64_C(0x3fed6d9d071b1f24), UINT64_C(0x3fed81eaa789b8c0), UINT64_C(0x3fed86ea0872820c),
      UINT64_C(0x0000000000000000), UINT64_C(0x0000000000000000), UINT64_C(0x0000000000000000)},
     {UINT64_C(0x3fb44909c6b8b90a), UINT64_C(0x3fd3bfa72f02c26a), UINT64_C(0x3fd6bec0853f6bba),
      UINT64_C(0x3fcc65668b2797db), UINT64_C(0x3fe05f200c0af9d7), UINT64_C(0x3fe0760f2ac9d165)},
     {UINT64_C(0x3fed4958ef2222f9), UINT64_C(0x3fe055f5527d2a19), UINT64_C(0xbfd695e55f8d9f70)},
     'd'},
    {-1,
     -17,
     31,
     {{UINT64_C(0x3fea90da40c65840), UINT64_C(0x3fea8c4b0a43a808), UINT64_C(0x3feaa2efbb00834c),
       UINT64_C(0x3feaa713f9e448ab)},
      {UINT64_C(0x3fccfb53cc2262a0), UINT64_C(0x3fcee3baef63dfbd), UINT64_C(0x3fcdc91ed8a976bb),
       UINT64_C(0x3fcfb949d0effa07)},
      {UINT64_C(0x3fe82996a4ed3e40), UINT64_C(0x3fe7168ec5d02298), UINT64_C(0x3feb8cd740a4869f),
       UINT64_C(0x3fee28eca55c1efa)}},
     "eeee",
     {UINT64_C(0x3fea90da40c65840), UINT64_C(0x3fea8c4b0a43a808), UINT64_C(0x3fea733224f5a816),
      UINT64_C(0x3feaa2efbb00834c), UINT64_C(0x3feaa713f9e448ab), UINT64_C(0x3fea9b248b1b2684),
      UINT64_C(0x0000000000000000), UINT64_C(0x0000000000000000), UINT64_C(0x0000000000000000)},
     {UINT64_C(0x3fe82996a4ed3e40), UINT64_C(0x3fe7168ec5d02298), UINT64_C(0x3fd76bff24caf15e),
      UINT64_C(0x3feb8cd740a4869f), UINT64_C(0x3fee28eca55c1efa), UINT64_C(0x3fe8354a6db12de2)},
     {UINT64_C(0x3fea9295b5055d81), UINT64_C(0x3fcbac0998dc31b9), UINT64_C(0x3fe205c7fc8bc8d2)},
     'e'},
    {INT64_MIN,
     INT32_MIN,
     INT32_MAX,
     {{UINT64_C(0x3fed8467243c32be), UINT64_C(0x3fed67d5e26d2f3d), UINT64_C(0x3fed87f74b07df30),
       UINT64_C(0x3fed6dd9225953b2)},
      {UINT64_C(0x3fe60e1c15f68bae), UINT64_C(0x3fe624f44486da4b), UINT64_C(0x3fe60b8cf0455b60),
       UINT64_C(0x3fe6285275c9038c)},
      {UINT64_C(0x3ff11d9e3bc77404), UINT64_C(0x3fec85d2d1f13f9e), UINT64_C(0x3ff2b2e70cb726df),
       UINT64_C(0x3ff07f39d4778ce3)}},
     "dddd",
     {UINT64_C(0x3fed8467243c32be), UINT64_C(0x3fed67d5e26d2f3d), UINT64_C(0x3fed40a330ca6f29),
      UINT64_C(0x3fed87f74b07df30), UINT64_C(0x3fed6dd9225953b2), UINT64_C(0x3fed46aae2ea61c4),
      UINT64_C(0x0000000000000000), UINT64_C(0x0000000000000000), UINT64_C(0x0000000000000000)},
     {UINT64_C(0x3ff11d9e3bc77404), UINT64_C(0x3fec85d2d1f13f9e), UINT64_C(0x3fe0ce015aa26fa3),
      UINT64_C(0x3ff2b2e70cb726df), UINT64_C(0x3ff07f39d4778ce3), UINT64_C(0x3fe501ce49d296a0)},
     {UINT64_C(0x3fed8723613715ca), UINT64_C(0x3fe5efe777609e12), UINT64_C(0x3fe8f09b96b41fb3)},
     'd'},
    {INT64_MAX,
     INT32_MIN,
     INT32_MAX,
     {{UINT64_C(0x3feec43776467e81), UINT64_C(0x3feed24b1e1a7da4), UINT64_C(0x3feec35b295417c7),
       UINT64_C(0x3feecf5a9e8ba8a2)},
      {UINT64_C(0x3f940e918c596a93), UINT64_C(0x3f86929d9dd54f81), UINT64_C(0x3f922d74af1cb8f0),
       UINT64_C(0x3f82a38c62590a21)},
      {UINT64_C(0xbfb3eddba0fd9a78), UINT64_C(0xbfd354799d350815), UINT64_C(0xbfdafd2f4e2f7404),
       UINT64_C(0xbfe274421b0b15c2)}},
     "hhhh",
     {UINT64_C(0x3feec43776467e81), UINT64_C(0x3feed24b1e1a7da4), UINT64_C(0x3feeccebb55d453b),
      UINT64_C(0x3feec35b295417c7), UINT64_C(0x3feecf5a9e8ba8a2), UINT64_C(0x3feec7ae4a5d1ab9),
      UINT64_C(0x0000000000000000), UINT64_C(0x0000000000000000), UINT64_C(0x0000000000000000)},
     {UINT64_C(0xbfb3eddba0fd9a78), UINT64_C(0xbfd354799d350815), UINT64_C(0xbf6bdcb1a95568c0),
      UINT64_C(0xbfdafd2f4e2f7404), UINT64_C(0xbfe274421b0b15c2), UINT64_C(0xbfca197803b031ab)},
     {UINT64_C(0x3feea8a887e0f50b), UINT64_C(0x3f9b03d92e84617c), UINT64_C(0x3fb9695c5f7ce0b4)},
     'h'},
};

static int test_catalogue_and_classifiers(void) {
    static const char *names[] = {
        "Rainforest", "Swampland", "Seasonal Forest", "Forest", "Savanna", "Shrubland", "Taiga",
        "Desert",     "Plains",    "Ice Desert",      "Tundra", "Hell",    "Sky"};
    for (size_t i = 0; i < 13; ++i) {
        mc_biome_id id = (mc_biome_id)(i + 1u);
        CHECK(mc_biome_field(id) == (char)('a' + i));
        CHECK(strcmp(mc_biome_name(id), names[i]) == 0);
    }
    CHECK(mc_biome_name(MC_BIOME_NONE) == NULL);
    CHECK(mc_biome_name((mc_biome_id)99) == NULL);
    CHECK(mc_biome_field(MC_BIOME_NONE) == '\0');
    CHECK(mc_biome_field((mc_biome_id)99) == '\0');
    mc_biome_classifier classifier;
    CHECK(mc_biome_classifier_init(&classifier) == MC_BIOME_OK);
    static const size_t expected_counts[] = {0,   13, 138, 57, 839,  518, 397,
                                             473, 54, 32,  0,  1575, 0,   0};
    size_t counts[14] = {0};
    for (size_t rainfall = 0; rainfall < 64; ++rainfall) {
        for (size_t temperature = 0; temperature < 64; ++temperature) {
            mc_biome_id id = classifier.table[temperature + rainfall * 64u];
            char expected = observed_table[rainfall][temperature];
            CHECK(mc_biome_field(id) == expected);
            CHECK(id >= MC_BIOME_RAINFOREST && id <= MC_BIOME_SKY);
            ++counts[id];
            mc_biome_id selected = MC_BIOME_NONE;
            CHECK(mc_biome_classify_float((float)temperature / 63.0f, (float)rainfall / 63.0f,
                                          &selected) == MC_BIOME_OK);
            CHECK(mc_biome_field(selected) == expected);
            /* Interior points identify every lookup cell without assuming
             * that a rounded binary64 i/63 lands on its lower boundary. */
            double t = temperature == 63 ? 1.0 : ((double)temperature + 0.25) / 63.0;
            double r = rainfall == 63 ? 1.0 : ((double)rainfall + 0.25) / 63.0;
            CHECK(mc_biome_lookup(&classifier, t, r, &selected) == MC_BIOME_OK);
            CHECK(mc_biome_field(selected) == expected);
        }
    }
    CHECK(memcmp(counts, expected_counts, sizeof(counts)) == 0);
    for (size_t i = 0; i < sizeof(observed_floats) / sizeof(observed_floats[0]); ++i) {
        const selector_fixture *fixture = &observed_floats[i];
        mc_biome_id id = MC_BIOME_NONE;
        CHECK(mc_biome_classify_float(float_from_bits((uint32_t)fixture->temperature),
                                      float_from_bits((uint32_t)fixture->rainfall),
                                      &id) == MC_BIOME_OK);
        CHECK(mc_biome_field(id) == fixture->field);
    }
    for (size_t i = 0; i < sizeof(observed_doubles) / sizeof(observed_doubles[0]); ++i) {
        const selector_fixture *fixture = &observed_doubles[i];
        mc_biome_id id = MC_BIOME_NONE;
        CHECK(mc_biome_lookup(&classifier, from_bits(fixture->temperature),
                              from_bits(fixture->rainfall), &id) == MC_BIOME_OK);
        CHECK(mc_biome_field(id) == fixture->field);
    }
    mc_biome_id direct = MC_BIOME_NONE;
    mc_biome_id lookup = MC_BIOME_NONE;
    CHECK(mc_biome_classify_float(0.5f, 0.2f, &direct) == MC_BIOME_OK);
    CHECK(mc_biome_lookup(&classifier, 0.5, 0.2, &lookup) == MC_BIOME_OK);
    CHECK(direct == MC_BIOME_SAVANNA && lookup == MC_BIOME_TUNDRA);
    return 0;
}

static int test_invalid_selectors(void) {
    mc_biome_classifier classifier;
    CHECK(mc_biome_classifier_init(&classifier) == MC_BIOME_OK);
    mc_biome_classifier saved = classifier;
    static const uint64_t bad_doubles[] = {
        UINT64_C(0xbff0000000000000), UINT64_C(0x3ff0000000000001), UINT64_C(0x7ff0000000000000),
        UINT64_C(0xfff0000000000000), UINT64_C(0x7ff8000000000001)};
    static const uint32_t bad_floats[] = {UINT32_C(0xbf800000), UINT32_C(0x3f800001),
                                          UINT32_C(0x7f800000), UINT32_C(0xff800000),
                                          UINT32_C(0x7fc00001)};
    mc_biome_id out = MC_BIOME_SKY;
    for (size_t i = 0; i < sizeof(bad_doubles) / sizeof(bad_doubles[0]); ++i) {
        CHECK(mc_biome_lookup(&classifier, from_bits(bad_doubles[i]), 0.5, &out) ==
              MC_BIOME_INVALID_ARGUMENT);
        CHECK(out == MC_BIOME_SKY);
        CHECK(mc_biome_lookup(&classifier, 0.5, from_bits(bad_doubles[i]), &out) ==
              MC_BIOME_INVALID_ARGUMENT);
        CHECK(out == MC_BIOME_SKY);
    }
    for (size_t i = 0; i < sizeof(bad_floats) / sizeof(bad_floats[0]); ++i) {
        CHECK(mc_biome_classify_float(float_from_bits(bad_floats[i]), 0.5f, &out) ==
              MC_BIOME_INVALID_ARGUMENT);
        CHECK(out == MC_BIOME_SKY);
        CHECK(mc_biome_classify_float(0.5f, float_from_bits(bad_floats[i]), &out) ==
              MC_BIOME_INVALID_ARGUMENT);
        CHECK(out == MC_BIOME_SKY);
    }
    CHECK(mc_biome_classify_float(0, 0, NULL) == MC_BIOME_INVALID_ARGUMENT);
    CHECK(mc_biome_lookup(NULL, 0, 0, &out) == MC_BIOME_INVALID_ARGUMENT);
    CHECK(mc_biome_lookup(&classifier, 0, 0, NULL) == MC_BIOME_INVALID_ARGUMENT);
    CHECK(out == MC_BIOME_SKY);
    CHECK(mc_biome_classifier_init(NULL) == MC_BIOME_INVALID_ARGUMENT);
    CHECK(memcmp(&classifier, &saved, sizeof(saved)) == 0);
    CHECK(mc_biome_classify_float(-0.0f, -0.0f, &out) == MC_BIOME_OK);
    CHECK(out == MC_BIOME_TUNDRA);
    CHECK(mc_biome_lookup(&classifier, -0.0, -0.0, &out) == MC_BIOME_OK);
    CHECK(out == MC_BIOME_TUNDRA);
    return 0;
}

static int test_buffer_lifetimes(void) {
    mc_biome_memory memory = {0};
    mc_biome_memory other = {0};
    CHECK(mc_biome_memory_init(NULL, 0) == MC_BIOME_INVALID_ARGUMENT);
    CHECK(mc_biome_memory_init(&memory, 0) == MC_BIOME_OK);
    mc_biome_double_buffer doubles = {0};
    mc_biome_buffer biomes = {0};
    CHECK(mc_biome_double_buffer_resize(&memory, &doubles, 0) == MC_BIOME_LIMIT);
    CHECK(mc_biome_buffer_resize(&memory, &biomes, 0) == MC_BIOME_LIMIT);
    CHECK(doubles.data == NULL && doubles.length == 0);
    CHECK(biomes.data == NULL && biomes.length == 0);
    CHECK(memory.live_allocation_bytes == 0);
    CHECK(mc_biome_memory_init(&memory, SIZE_MAX) == MC_BIOME_OK);
    CHECK(mc_biome_memory_init(&other, SIZE_MAX) == MC_BIOME_OK);
    CHECK(mc_biome_double_buffer_resize(&memory, &doubles, 0) == MC_BIOME_OK);
    CHECK(doubles.data != NULL && doubles.length == 0);
    CHECK(memory.live_allocation_bytes > 0);
    mc_biome_memory saved_memory = memory;
    CHECK(mc_biome_memory_init(&memory, 0) == MC_BIOME_INVALID_ARGUMENT);
    CHECK(memcmp(&memory, &saved_memory, sizeof(memory)) == 0);
    CHECK(mc_biome_double_buffer_resize(&memory, &doubles, 3) == MC_BIOME_OK);
    CHECK(doubles.length == 3);
    for (size_t i = 0; i < 3; ++i) {
        CHECK(bits_of(doubles.data[i]) == 0);
        doubles.data[i] = (double)i + 2;
    }
    double *same = doubles.data;
    CHECK(mc_biome_double_buffer_resize(&memory, &doubles, 3) == MC_BIOME_OK);
    CHECK(doubles.data == same);
    for (size_t i = 0; i < 3; ++i)
        CHECK(doubles.data[i] == (double)i + 2);
    CHECK(mc_biome_double_buffer_resize(&other, &doubles, 3) == MC_BIOME_INVALID_ARGUMENT);
    CHECK(doubles.data == same && doubles.length == 3);
    CHECK(other.live_allocation_bytes == 0);
    saved_memory = memory;
    CHECK(mc_biome_double_buffer_resize(&memory, &doubles, (size_t)INT32_MAX + 1u) != MC_BIOME_OK);
    CHECK(mc_biome_double_buffer_resize(&memory, &doubles, SIZE_MAX / sizeof(double) + 1u) !=
          MC_BIOME_OK);
    CHECK(mc_biome_double_buffer_resize(NULL, &doubles, 2) == MC_BIOME_INVALID_ARGUMENT);
    CHECK(mc_biome_double_buffer_resize(&memory, NULL, 2) == MC_BIOME_INVALID_ARGUMENT);
    CHECK(doubles.data == same && doubles.length == 3);
    CHECK(memcmp(&memory, &saved_memory, sizeof(memory)) == 0);
    for (size_t i = 0; i < 3; ++i)
        CHECK(doubles.data[i] == (double)i + 2);
    CHECK(mc_biome_double_buffer_resize(&memory, &doubles, 2) == MC_BIOME_OK);
    CHECK(doubles.length == 2 && bits_of(doubles.data[0]) == 0 && bits_of(doubles.data[1]) == 0);
    CHECK(mc_biome_buffer_resize(&memory, &biomes, 3) == MC_BIOME_OK);
    for (size_t i = 0; i < 3; ++i) {
        CHECK(biomes.data[i] == MC_BIOME_NONE);
        biomes.data[i] = MC_BIOME_HELL;
    }
    mc_biome_id *same_biomes = biomes.data;
    CHECK(mc_biome_buffer_resize(&memory, &biomes, 3) == MC_BIOME_OK);
    CHECK(biomes.data == same_biomes);
    CHECK(mc_biome_buffer_resize(&other, &biomes, 2) == MC_BIOME_INVALID_ARGUMENT);
    CHECK(biomes.data == same_biomes && biomes.length == 3);
    CHECK(mc_biome_buffer_resize(&memory, &biomes, SIZE_MAX) != MC_BIOME_OK);
    CHECK(mc_biome_buffer_resize(NULL, &biomes, 2) == MC_BIOME_INVALID_ARGUMENT);
    CHECK(mc_biome_buffer_resize(&memory, NULL, 2) == MC_BIOME_INVALID_ARGUMENT);
    for (size_t i = 0; i < 3; ++i)
        CHECK(biomes.data[i] == MC_BIOME_HELL);
    mc_biome_double_buffer_free(&doubles);
    mc_biome_buffer_free(&biomes);
    CHECK(doubles.data == NULL && doubles.length == 0);
    CHECK(biomes.data == NULL && biomes.length == 0);
    CHECK(memory.live_allocation_bytes == 0);
    mc_biome_double_buffer_free(&doubles);
    mc_biome_buffer_free(&biomes);
    mc_biome_double_buffer_free(NULL);
    mc_biome_buffer_free(NULL);
    CHECK(mc_biome_memory_init(&memory, 0) == MC_BIOME_OK);
    return 0;
}

static int test_original_climate_sequences(void) {
    for (size_t f = 0; f < sizeof(observed_climates) / sizeof(observed_climates[0]); ++f) {
        const climate_fixture *fixture = &observed_climates[f];
        mc_biome_memory memory = {0};
        mc_biome_manager manager = {0};
        mc_biome_buffer output = {0};
        mc_biome_double_buffer temperatures = {0};
        CHECK(mc_biome_memory_init(&memory, MC_BIOME_DEFAULT_ALLOCATION_BYTES) == MC_BIOME_OK);
        CHECK(mc_biome_manager_init(&manager, fixture->seed, &memory) == MC_BIOME_OK);
        mc_biome_manager initialized = manager;
        CHECK(manager.a.data == NULL && manager.b.data == NULL && manager.c.data == NULL &&
              manager.d.data == NULL);
        CHECK(mc_biome_sample_square(&manager, fixture->x, fixture->z, 2, &output) == MC_BIOME_OK);
        CHECK(output.length == 4 && manager.d.data == NULL);
        mc_biome_double_buffer *channels[] = {&manager.a, &manager.b, &manager.c};
        for (size_t channel = 0; channel < 3; ++channel) {
            CHECK(channels[channel]->length == 4);
            for (size_t i = 0; i < 4; ++i)
                CHECK(bits_of(channels[channel]->data[i]) == fixture->initial[channel][i]);
        }
        for (size_t i = 0; i < 4; ++i)
            CHECK(mc_biome_field(output.data[i]) == fixture->biomes[i]);
        CHECK(mc_biome_double_buffer_resize(&memory, &temperatures, 9) == MC_BIOME_OK);
        for (size_t i = 0; i < 9; ++i)
            temperatures.data[i] = (double)((int)(i % 11u) - 5) * 0.125;
        double *external = temperatures.data;
        double *a_before = manager.a.data;
        double *b_before = manager.b.data;
        CHECK(mc_biome_temperatures(&manager, fixture->x, fixture->z, 2, 3, &temperatures) ==
              MC_BIOME_OK);
        CHECK(temperatures.data == external && temperatures.length == 9);
        CHECK(manager.a.data == a_before && manager.b.data == b_before && manager.d.data == NULL);
        CHECK(temperatures.data != manager.a.data);
        for (size_t i = 0; i < 9; ++i)
            CHECK(bits_of(temperatures.data[i]) == fixture->temperature[i]);
        for (size_t i = 0; i < 6; ++i)
            CHECK(bits_of(manager.c.data[i]) == fixture->auxiliary[i]);
        CHECK(manager.c.length == 6);
        for (size_t i = 0; i < 4; ++i) {
            CHECK(bits_of(manager.a.data[i]) == fixture->initial[0][i]);
            CHECK(bits_of(manager.b.data[i]) == fixture->initial[1][i]);
            CHECK(mc_biome_field(output.data[i]) == fixture->biomes[i]);
        }
        CHECK(mc_biome_cached_square(&manager, fixture->x, fixture->z, 4) == MC_BIOME_OK);
        CHECK(manager.a.length == 16 && manager.b.length == 16 && manager.c.length == 16 &&
              manager.d.length == 16);
        double large[3][16];
        mc_biome_id large_biomes[16];
        for (size_t channel = 0; channel < 3; ++channel)
            memcpy(large[channel], channels[channel]->data, sizeof(large[channel]));
        memcpy(large_biomes, manager.d.data, sizeof(large_biomes));
        const void *retained[] = {manager.a.data, manager.b.data, manager.c.data, manager.d.data};
        mc_biome_id point = MC_BIOME_NONE;
        CHECK(mc_biome_point(&manager, fixture->x + 1, fixture->z - 1, &point) == MC_BIOME_OK);
        CHECK(mc_biome_field(point) == fixture->point_biome);
        CHECK(manager.a.data == retained[0] && manager.b.data == retained[1] &&
              manager.c.data == retained[2] && manager.d.data == retained[3]);
        for (size_t channel = 0; channel < 3; ++channel) {
            CHECK(bits_of(channels[channel]->data[0]) == fixture->point[channel]);
            for (size_t i = 1; i < 16; ++i)
                CHECK(bits_of(channels[channel]->data[i]) == 0);
        }
        for (size_t i = 1; i < 16; ++i)
            CHECK(manager.d.data[i] == large_biomes[i]);
        CHECK(mc_biome_cached_square(&manager, fixture->x, fixture->z, 4) == MC_BIOME_OK);
        for (size_t channel = 0; channel < 3; ++channel)
            CHECK(memcmp(large[channel], channels[channel]->data, sizeof(large[channel])) == 0);
        CHECK(memcmp(large_biomes, manager.d.data, sizeof(large_biomes)) == 0);
        CHECK(memcmp(initialized.temperature_layers, manager.temperature_layers,
                     sizeof(manager.temperature_layers)) == 0);
        CHECK(memcmp(initialized.rainfall_layers, manager.rainfall_layers,
                     sizeof(manager.rainfall_layers)) == 0);
        CHECK(memcmp(initialized.auxiliary_layers, manager.auxiliary_layers,
                     sizeof(manager.auxiliary_layers)) == 0);
        CHECK(memcmp(&initialized.classifier, &manager.classifier, sizeof(manager.classifier)) ==
              0);
        mc_biome_manager_free(&manager);
        CHECK(output.data != NULL && temperatures.data == external);
        mc_biome_buffer_free(&output);
        mc_biome_double_buffer_free(&temperatures);
        CHECK(memory.live_allocation_bytes == 0);
    }
    return 0;
}

static int test_channel_construction(void) {
    static const int64_t seeds[] = {0, 1, -1, INT64_MIN, INT64_MAX, INT64_C(1234567890123456789)};
    /* Constructor channel counts and independent wrapped seed products were
     * checked in both originals' manager constructors before the fixture setup. */
    static const int64_t factors[] = {9871, 39811, 543321};
    static const size_t counts[] = {4, 4, 2};
    for (size_t i = 0; i < sizeof(seeds) / sizeof(seeds[0]); ++i) {
        mc_biome_memory memory = {0};
        mc_biome_manager manager = {0};
        CHECK(mc_biome_memory_init(&memory, 0) == MC_BIOME_OK);
        CHECK(mc_biome_manager_init(&manager, seeds[i], &memory) == MC_BIOME_OK);
        const mc_noise_simplex *channels[] = {manager.temperature_layers, manager.rainfall_layers,
                                              manager.auxiliary_layers};
        for (size_t channel = 0; channel < 3; ++channel) {
            mc_java_random random;
            mc_java_random_seed(&random, mc_java_i64_mul(seeds[i], factors[channel]));
            for (size_t layer = 0; layer < counts[channel]; ++layer) {
                mc_noise_simplex expected;
                CHECK(mc_noise_simplex_init(&expected, &random));
                CHECK(memcmp(&expected, &channels[channel][layer], sizeof(expected)) == 0);
            }
        }
        CHECK(memory.live_allocation_bytes == 0);
        mc_biome_manager_free(&manager);
    }
    return 0;
}

static int test_reuse_adapters_and_client_raw(void) {
    mc_biome_memory memory = {0};
    mc_biome_manager manager = {0};
    mc_biome_buffer output = {0};
    mc_biome_double_buffer temperatures = {0};
    CHECK(mc_biome_memory_init(&memory, MC_BIOME_DEFAULT_ALLOCATION_BYTES) == MC_BIOME_OK);
    CHECK(mc_biome_manager_init(&manager, 0, &memory) == MC_BIOME_OK);
    double raw = 123;
    CHECK(mc_biome_client_raw_temperature(&manager, 0, 0, &raw) == MC_BIOME_OK);
    CHECK(bits_of(raw) == UINT64_C(0x3fbb897494b0fae0));
    CHECK(manager.a.length == 1 && bits_of(manager.a.data[0]) == bits_of(raw));
    CHECK(manager.b.data == NULL && manager.c.data == NULL && manager.d.data == NULL);
    CHECK(mc_biome_buffer_resize(&memory, &output, 19) == MC_BIOME_OK);
    for (size_t i = 0; i < output.length; ++i)
        output.data[i] = MC_BIOME_SKY;
    mc_biome_id *retained_output = output.data;
    CHECK(mc_biome_sample_square(&manager, -3, 7, 4, &output) == MC_BIOME_OK);
    CHECK(output.data == retained_output && output.length == 19);
    CHECK(manager.d.data == NULL);
    for (size_t i = 16; i < 19; ++i)
        CHECK(output.data[i] == MC_BIOME_SKY);
    CHECK(mc_biome_cached_square(&manager, -3, 7, 4) == MC_BIOME_OK);
    CHECK(output.data != manager.d.data && manager.d.length == 16);
    CHECK(memcmp(output.data, manager.d.data, 16u * sizeof(*output.data)) == 0);
    double rain[16];
    mc_biome_id cached[16];
    memcpy(rain, manager.b.data, sizeof(rain));
    memcpy(cached, manager.d.data, sizeof(cached));
    CHECK(mc_biome_temperatures(&manager, -3, 7, 4, 4, &temperatures) == MC_BIOME_OK);
    CHECK(temperatures.length == 16);
    CHECK(memcmp(temperatures.data, manager.a.data, 16u * sizeof(double)) == 0);
    CHECK(memcmp(rain, manager.b.data, sizeof(rain)) == 0);
    CHECK(memcmp(cached, manager.d.data, sizeof(cached)) == 0);
    double *a_retained = manager.a.data;
    CHECK(mc_biome_client_raw_temperature(&manager, 0, 0, &raw) == MC_BIOME_OK);
    CHECK(manager.a.data == a_retained && manager.a.length == 16);
    CHECK(bits_of(raw) == UINT64_C(0x3fbb897494b0fae0));
    for (size_t i = 1; i < 16; ++i)
        CHECK(bits_of(manager.a.data[i]) == 0);
    CHECK(memcmp(rain, manager.b.data, sizeof(rain)) == 0);
    CHECK(memcmp(cached, manager.d.data, sizeof(cached)) == 0);
    static const int32_t chunks[][2] = {
        {-1, 2}, {INT32_MIN, INT32_MAX}, {INT32_C(134217728), INT32_C(-134217729)}};
    for (size_t i = 0; i < sizeof(chunks) / sizeof(chunks[0]); ++i) {
        mc_biome_id chunk = MC_BIOME_NONE;
        mc_biome_id point = MC_BIOME_NONE;
        CHECK(mc_biome_chunk(&manager, chunks[i][0], chunks[i][1], &chunk) == MC_BIOME_OK);
        double t = manager.a.data[0];
        double r = manager.b.data[0];
        double g = manager.c.data[0];
        CHECK(mc_biome_point(&manager, mc_java_i32_shl(chunks[i][0], 4),
                             mc_java_i32_shl(chunks[i][1], 4), &point) == MC_BIOME_OK);
        CHECK(chunk == point);
        CHECK(bits_of(t) == bits_of(manager.a.data[0]));
        CHECK(bits_of(r) == bits_of(manager.b.data[0]));
        CHECK(bits_of(g) == bits_of(manager.c.data[0]));
    }
    memcpy(cached, manager.d.data, sizeof(cached));
    CHECK(mc_biome_temperatures(&manager, 0, 0, 0, 3, &temperatures) == MC_BIOME_OK);
    CHECK(temperatures.length == 16 && manager.c.length == 16);
    for (size_t i = 0; i < 16; ++i) {
        CHECK(bits_of(temperatures.data[i]) == 0);
        CHECK(bits_of(manager.c.data[i]) == 0);
    }
    CHECK(memcmp(cached, manager.d.data, sizeof(cached)) == 0);
    CHECK(mc_biome_cached_square(&manager, 0, 0, 0) == MC_BIOME_OK);
    CHECK(manager.a.length == 16 && manager.b.length == 16 && manager.c.length == 16 &&
          manager.d.length == 16);
    for (size_t i = 0; i < 16; ++i) {
        CHECK(bits_of(manager.a.data[i]) == 0);
        CHECK(bits_of(manager.b.data[i]) == 0);
        CHECK(bits_of(manager.c.data[i]) == 0);
    }
    CHECK(memcmp(cached, manager.d.data, sizeof(cached)) == 0);
    mc_biome_id output_saved[19];
    memcpy(output_saved, output.data, sizeof(output_saved));
    CHECK(mc_biome_sample_square(&manager, 0, 0, 0, &output) == MC_BIOME_OK);
    CHECK(output.data == retained_output && output.length == 19);
    CHECK(memcmp(output_saved, output.data, sizeof(output_saved)) == 0);
    mc_biome_manager_free(&manager);
    mc_biome_buffer_free(&output);
    mc_biome_double_buffer_free(&temperatures);
    CHECK(memory.live_allocation_bytes == 0);
    CHECK(mc_biome_manager_init(&manager, 1, &memory) == MC_BIOME_OK);
    CHECK(mc_biome_cached_square(&manager, 0, 0, 0) == MC_BIOME_OK);
    CHECK(manager.a.data != NULL && manager.b.data != NULL && manager.c.data != NULL &&
          manager.d.data != NULL);
    CHECK(manager.a.length == 0 && manager.b.length == 0 && manager.c.length == 0 &&
          manager.d.length == 0);
    CHECK(mc_biome_temperatures(&manager, 0, 0, 0, 3, &temperatures) == MC_BIOME_OK);
    CHECK(temperatures.data != NULL && temperatures.length == 0);
    mc_biome_manager_free(&manager);
    mc_biome_double_buffer_free(&temperatures);
    CHECK(memory.live_allocation_bytes == 0);
    mc_biome_manager_free(NULL);
    return 0;
}

typedef struct manager_snapshot {
    mc_biome_manager manager;
    mc_biome_memory memory;
    double channels[3][16];
    mc_biome_id cached[16];
} manager_snapshot;

static void save_manager(manager_snapshot *snapshot, const mc_biome_manager *manager) {
    memcpy(&snapshot->manager, manager, sizeof(*manager));
    snapshot->memory = *manager->memory;
    const mc_biome_double_buffer *channels[] = {&manager->a, &manager->b, &manager->c};
    for (size_t i = 0; i < 3; ++i)
        memcpy(snapshot->channels[i], channels[i]->data, channels[i]->length * sizeof(double));
    memcpy(snapshot->cached, manager->d.data, manager->d.length * sizeof(mc_biome_id));
}

static int check_saved(const manager_snapshot *snapshot, const mc_biome_manager *manager) {
    CHECK(memcmp(&snapshot->manager, manager, sizeof(*manager)) == 0);
    CHECK(memcmp(&snapshot->memory, manager->memory, sizeof(snapshot->memory)) == 0);
    const mc_biome_double_buffer *channels[] = {&manager->a, &manager->b, &manager->c};
    for (size_t i = 0; i < 3; ++i)
        CHECK(memcmp(snapshot->channels[i], channels[i]->data,
                     channels[i]->length * sizeof(double)) == 0);
    CHECK(memcmp(snapshot->cached, manager->d.data, manager->d.length * sizeof(mc_biome_id)) == 0);
    return 0;
}

static int test_invalid_manager_calls(void) {
    mc_biome_memory memory = {0};
    mc_biome_manager manager = {0};
    mc_biome_double_buffer temperatures = {0};
    mc_biome_buffer biomes = {0};
    CHECK(mc_biome_memory_init(&memory, MC_BIOME_DEFAULT_ALLOCATION_BYTES) == MC_BIOME_OK);
    CHECK(mc_biome_manager_init(NULL, 0, &memory) == MC_BIOME_INVALID_ARGUMENT);
    CHECK(mc_biome_manager_init(&manager, 0, NULL) == MC_BIOME_INVALID_ARGUMENT);
    CHECK(mc_biome_cached_square(&manager, 0, 0, 1) == MC_BIOME_INVALID_ARGUMENT);
    CHECK(mc_biome_manager_init(&manager, 0, &memory) == MC_BIOME_OK);
    CHECK(mc_biome_cached_square(&manager, 0, 0, 2) == MC_BIOME_OK);
    CHECK(mc_biome_temperatures(&manager, 0, 0, 2, 2, &temperatures) == MC_BIOME_OK);
    CHECK(mc_biome_sample_square(&manager, 0, 0, 2, &biomes) == MC_BIOME_OK);
    manager_snapshot saved;
    save_manager(&saved, &manager);
    double t_saved[4];
    mc_biome_id b_saved[4];
    memcpy(t_saved, temperatures.data, sizeof(t_saved));
    memcpy(b_saved, biomes.data, sizeof(b_saved));
    mc_biome_double_buffer t_descriptor = temperatures;
    mc_biome_buffer b_descriptor = biomes;
    CHECK(mc_biome_manager_init(&manager, 1, &memory) == MC_BIOME_INVALID_ARGUMENT);
    CHECK(mc_biome_temperatures(NULL, 0, 0, 1, 1, &temperatures) == MC_BIOME_INVALID_ARGUMENT);
    CHECK(mc_biome_temperatures(&manager, 0, 0, 1, 1, NULL) == MC_BIOME_INVALID_ARGUMENT);
    CHECK(mc_biome_sample_square(NULL, 0, 0, 1, &biomes) == MC_BIOME_INVALID_ARGUMENT);
    CHECK(mc_biome_sample_square(&manager, 0, 0, 1, NULL) == MC_BIOME_INVALID_ARGUMENT);
    CHECK(mc_biome_cached_square(NULL, 0, 0, 1) == MC_BIOME_INVALID_ARGUMENT);
    CHECK(mc_biome_point(&manager, 0, 0, NULL) == MC_BIOME_INVALID_ARGUMENT);
    CHECK(mc_biome_chunk(&manager, 0, 0, NULL) == MC_BIOME_INVALID_ARGUMENT);
    CHECK(mc_biome_client_raw_temperature(&manager, 0, 0, NULL) == MC_BIOME_INVALID_ARGUMENT);
    CHECK(mc_biome_temperatures(&manager, 0, 0, 1, 1, &manager.a) == MC_BIOME_INVALID_ARGUMENT);
    CHECK(mc_biome_temperatures(&manager, 0, 0, 1, 1, &manager.b) == MC_BIOME_INVALID_ARGUMENT);
    CHECK(mc_biome_temperatures(&manager, 0, 0, 1, 1, &manager.c) == MC_BIOME_INVALID_ARGUMENT);
    CHECK(mc_biome_sample_square(&manager, 0, 0, 1, &manager.d) == MC_BIOME_INVALID_ARGUMENT);
    static const size_t dimensions[][2] = {
        {(size_t)INT32_MAX + 1u, 0}, {0, (size_t)INT32_MAX + 1u}, {46341, 46341}, {SIZE_MAX, 2}};
    for (size_t i = 0; i < sizeof(dimensions) / sizeof(dimensions[0]); ++i) {
        CHECK(mc_biome_temperatures(&manager, 0, 0, dimensions[i][0], dimensions[i][1],
                                    &temperatures) != MC_BIOME_OK);
        size_t square = dimensions[i][0] == 0 ? SIZE_MAX : dimensions[i][0];
        CHECK(mc_biome_cached_square(&manager, 0, 0, square) != MC_BIOME_OK);
        CHECK(check_saved(&saved, &manager) == 0);
    }
    CHECK(memcmp(&temperatures, &t_descriptor, sizeof(temperatures)) == 0);
    CHECK(memcmp(&biomes, &b_descriptor, sizeof(biomes)) == 0);
    CHECK(memcmp(t_saved, temperatures.data, sizeof(t_saved)) == 0);
    CHECK(memcmp(b_saved, biomes.data, sizeof(b_saved)) == 0);
    CHECK(check_saved(&saved, &manager) == 0);
    mc_biome_memory foreign_memory = {0};
    mc_biome_double_buffer foreign = {0};
    CHECK(mc_biome_memory_init(&foreign_memory, SIZE_MAX) == MC_BIOME_OK);
    CHECK(mc_biome_double_buffer_resize(&foreign_memory, &foreign, 2) == MC_BIOME_OK);
    foreign.data[0] = 17;
    CHECK(mc_biome_temperatures(&manager, 0, 0, 1, 1, &foreign) == MC_BIOME_INVALID_ARGUMENT);
    CHECK(foreign.length == 2 && foreign.data[0] == 17);
    CHECK(check_saved(&saved, &manager) == 0);
    mc_biome_double_buffer_free(&foreign);
    mc_biome_double_buffer_free(&temperatures);
    mc_biome_buffer_free(&biomes);
    mc_biome_manager_free(&manager);
    CHECK(memory.live_allocation_bytes == 0 && foreign_memory.live_allocation_bytes == 0);
    return 0;
}

static int test_staged_allocation_rollback(void) {
    /* Determine real allocation charges through the public live counter.
     * Sweep every byte of headroom for a four-buffer replacement. This crosses
     * every staged failure point while retaining real, initialized buffers. */
    mc_biome_memory baseline_memory = {0};
    mc_biome_manager baseline = {0};
    CHECK(mc_biome_memory_init(&baseline_memory, SIZE_MAX) == MC_BIOME_OK);
    CHECK(mc_biome_manager_init(&baseline, -1, &baseline_memory) == MC_BIOME_OK);
    CHECK(mc_biome_cached_square(&baseline, -17, 31, 2) == MC_BIOME_OK);
    size_t old_live = baseline_memory.live_allocation_bytes;
    CHECK(mc_biome_cached_square(&baseline, -17, 31, 4) == MC_BIOME_OK);
    size_t new_live = baseline_memory.live_allocation_bytes;
    CHECK(new_live > old_live && old_live <= SIZE_MAX - new_live);
    size_t upper = old_live + new_live;
    mc_biome_manager_free(&baseline);
    CHECK(baseline_memory.live_allocation_bytes == 0);
    size_t failures = 0;
    size_t minimum_success = SIZE_MAX;
    for (size_t limit = old_live; limit <= upper; ++limit) {
        mc_biome_memory memory = {0};
        mc_biome_manager manager = {0};
        CHECK(mc_biome_memory_init(&memory, limit) == MC_BIOME_OK);
        CHECK(mc_biome_manager_init(&manager, -1, &memory) == MC_BIOME_OK);
        CHECK(mc_biome_cached_square(&manager, -17, 31, 2) == MC_BIOME_OK);
        manager_snapshot saved;
        save_manager(&saved, &manager);
        mc_biome_status status = mc_biome_cached_square(&manager, -17, 31, 4);
        if (status == MC_BIOME_OK) {
            if (minimum_success == SIZE_MAX)
                minimum_success = limit;
            CHECK(memory.live_allocation_bytes == new_live);
            CHECK(manager.a.length == 16 && manager.b.length == 16 && manager.c.length == 16 &&
                  manager.d.length == 16);
        } else {
            ++failures;
            CHECK(status == MC_BIOME_LIMIT);
            CHECK(check_saved(&saved, &manager) == 0);
        }
        mc_biome_manager_free(&manager);
        CHECK(memory.live_allocation_bytes == 0);
    }
    CHECK(failures > 1);
    CHECK(minimum_success == upper);
    CHECK(minimum_success > new_live);
    /* Scalar outputs must also survive a rejected lazy allocation. */
    mc_biome_memory empty_memory = {0};
    mc_biome_manager empty = {0};
    CHECK(mc_biome_memory_init(&empty_memory, 0) == MC_BIOME_OK);
    CHECK(mc_biome_manager_init(&empty, 0, &empty_memory) == MC_BIOME_OK);
    mc_biome_manager saved;
    memcpy(&saved, &empty, sizeof(saved));
    mc_biome_id biome = MC_BIOME_SKY;
    double raw = 123;
    CHECK(mc_biome_point(&empty, 0, 0, &biome) == MC_BIOME_LIMIT);
    CHECK(biome == MC_BIOME_SKY);
    CHECK(mc_biome_chunk(&empty, 0, 0, &biome) == MC_BIOME_LIMIT);
    CHECK(biome == MC_BIOME_SKY);
    CHECK(mc_biome_client_raw_temperature(&empty, 0, 0, &raw) == MC_BIOME_LIMIT);
    CHECK(raw == 123);
    CHECK(memcmp(&saved, &empty, sizeof(saved)) == 0);
    CHECK(empty_memory.live_allocation_bytes == 0);
    mc_biome_manager_free(&empty);
    return 0;
}

int main(void) {
    if (test_catalogue_and_classifiers() || test_invalid_selectors() || test_buffer_lifetimes() ||
        test_original_climate_sequences() || test_channel_construction() ||
        test_reuse_adapters_and_client_raw() || test_invalid_manager_calls() ||
        test_staged_allocation_rollback())
        return 1;
    printf("Biome: %u checks passed\n", checks);
    return 0;
}
