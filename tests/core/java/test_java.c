#include "mc/core/java/java.h"

#include <inttypes.h>
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

static uint32_t float_bits(float value) {
    uint32_t bits;
    _Static_assert(sizeof(bits) == sizeof(value), "binary32 storage required");
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint64_t double_bits(double value) {
    uint64_t bits;
    _Static_assert(sizeof(bits) == sizeof(value), "binary64 storage required");
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static float float_from_bits(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static double double_from_bits(uint64_t bits) {
    double value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static int test_integers(void) {
    int32_t i32 = 123;
    int64_t i64 = 456;
    CHECK(mc_java_i32_add(INT32_MAX, 1) == INT32_MIN);
    CHECK(mc_java_i32_sub(INT32_MIN, 1) == INT32_MAX);
    CHECK(mc_java_i32_mul(INT32_MAX, INT32_MAX) == 1);
    CHECK(mc_java_i32_mul(INT32_MIN, -1) == INT32_MIN);
    CHECK(mc_java_i32_neg(INT32_MIN) == INT32_MIN);
    CHECK(mc_java_i64_add(INT64_MAX, 1) == INT64_MIN);
    CHECK(mc_java_i64_sub(INT64_MIN, 1) == INT64_MAX);
    CHECK(mc_java_i64_mul(INT64_MAX, INT64_MAX) == 1);
    CHECK(mc_java_i64_mul(INT64_MIN, -1) == INT64_MIN);
    CHECK(mc_java_i64_neg(INT64_MIN) == INT64_MIN);
    CHECK(mc_java_i32_div(INT32_MIN, -1, &i32) && i32 == INT32_MIN);
    CHECK(mc_java_i32_rem(INT32_MIN, -1, &i32) && i32 == 0);
    CHECK(mc_java_i32_div(-7, 3, &i32) && i32 == -2);
    CHECK(mc_java_i32_rem(-7, 3, &i32) && i32 == -1);
    CHECK(mc_java_i32_div(7, -3, &i32) && i32 == -2);
    CHECK(mc_java_i32_rem(7, -3, &i32) && i32 == 1);
    CHECK(!mc_java_i32_div(1, 0, &i32) && i32 == 1);
    CHECK(!mc_java_i32_rem(1, 0, &i32) && i32 == 1);
    CHECK(!mc_java_i32_div(1, 1, NULL));
    CHECK(!mc_java_i32_rem(1, 1, NULL));
    CHECK(mc_java_i64_div(INT64_MIN, -1, &i64) && i64 == INT64_MIN);
    CHECK(mc_java_i64_rem(INT64_MIN, -1, &i64) && i64 == 0);
    CHECK(mc_java_i64_div(-7, 3, &i64) && i64 == -2);
    CHECK(mc_java_i64_rem(-7, 3, &i64) && i64 == -1);
    CHECK(!mc_java_i64_div(1, 0, &i64) && i64 == -1);
    CHECK(!mc_java_i64_rem(1, 0, &i64) && i64 == -1);
    CHECK(!mc_java_i64_div(1, 1, NULL));
    CHECK(!mc_java_i64_rem(1, 1, NULL));
    CHECK(mc_java_i32_shl(1, -1) == INT32_MIN);
    CHECK(mc_java_i32_shl(1, 32) == 1);
    CHECK(mc_java_i32_shl(1, 33) == 2);
    CHECK(mc_java_i32_shr(-7, 1) == -4);
    CHECK(mc_java_i32_shr(INT32_MIN, -1) == -1);
    CHECK(mc_java_i32_shr(INT32_MIN, 32) == INT32_MIN);
    CHECK(mc_java_i32_ushr(-1, 1) == INT32_MAX);
    CHECK(mc_java_i32_ushr(-1, 32) == -1);
    CHECK(mc_java_i32_ushr(INT32_MIN, -1) == 1);
    CHECK(mc_java_i64_shl(1, -1) == INT64_MIN);
    CHECK(mc_java_i64_shl(1, 64) == 1);
    CHECK(mc_java_i64_shl(1, 65) == 2);
    CHECK(mc_java_i64_shr(-7, 1) == -4);
    CHECK(mc_java_i64_shr(INT64_MIN, -1) == -1);
    CHECK(mc_java_i64_shr(INT64_MIN, 64) == INT64_MIN);
    CHECK(mc_java_i64_ushr(-1, 1) == INT64_MAX);
    CHECK(mc_java_i64_ushr(-1, 64) == -1);
    CHECK(mc_java_i64_ushr(INT64_MIN, -1) == 1);
    CHECK(mc_java_i8_narrow(255) == -1);
    CHECK(mc_java_i8_narrow(128) == -128);
    CHECK(mc_java_i8_narrow(-129) == 127);
    CHECK(mc_java_i16_narrow(65535) == -1);
    CHECK(mc_java_i16_narrow(32768) == -32768);
    CHECK(mc_java_i16_narrow(-32769) == 32767);
    CHECK(mc_java_u16_narrow(-1) == 65535);
    CHECK(mc_java_u16_narrow(65536) == 0);
    CHECK(mc_java_i32_narrow(INT64_MAX) == -1);
    CHECK(mc_java_i32_narrow(INT64_C(0x80000000)) == INT32_MIN);
    return 0;
}

static int test_casts(void) {
    CHECK(mc_java_double_to_i32(NAN) == 0);
    CHECK(mc_java_double_to_i64(NAN) == 0);
    CHECK(mc_java_double_to_i32(INFINITY) == INT32_MAX);
    CHECK(mc_java_double_to_i32(-INFINITY) == INT32_MIN);
    CHECK(mc_java_double_to_i64(INFINITY) == INT64_MAX);
    CHECK(mc_java_double_to_i64(-INFINITY) == INT64_MIN);
    CHECK(mc_java_double_to_i32(-1.999) == -1);
    CHECK(mc_java_double_to_i64(1.999) == 1);
    CHECK(mc_java_double_to_i32(0x1.fffffffffffffp30) == INT32_MAX);
    CHECK(mc_java_double_to_i32(0x1p31) == INT32_MAX);
    CHECK(mc_java_double_to_i32(-0x1p31) == INT32_MIN);
    CHECK(mc_java_double_to_i64(0x1.fffffffffffffp62) == INT64_MAX - 1023);
    CHECK(mc_java_double_to_i64(0x1p63) == INT64_MAX);
    CHECK(mc_java_double_to_i64(-0x1p63) == INT64_MIN);
    CHECK(mc_java_float_to_i32(NAN) == 0);
    CHECK(mc_java_float_to_i64(NAN) == 0);
    CHECK(mc_java_float_to_i32(0x1p31f) == INT32_MAX);
    CHECK(mc_java_float_to_i32(-0x1p31f) == INT32_MIN);
    CHECK(mc_java_float_to_i64(0x1p63f) == INT64_MAX);
    CHECK(mc_java_float_to_i64(-0x1p63f) == INT64_MIN);
    CHECK(mc_java_float_to_i32(-1.999f) == -1);
    CHECK(mc_java_float_to_i64(-0.0f) == 0);
    return 0;
}

static int test_random(void) {
    /* Captured by JavaSemanticsProbe on the actual Temurin 17.0.17 runtime.
     * These are fixed oracle values, not a second C RNG implementation. */
    static const int32_t ints[] = {-1155484576, -723955400, 1033096058,  -1690734402,
                                   -1557280266, 1327362106, -1930858313, 502539523};
    static const int64_t longs[] = {-INT64_C(4962768465676381896), INT64_C(4437113781045784766),
                                    -INT64_C(6688467811848818630), -INT64_C(8292973307042192125),
                                    -INT64_C(7423979211207825555), INT64_C(6146794652083548235),
                                    INT64_C(7105486291024734541),  -INT64_C(279624296851435688)};
    static const uint32_t floats[] = {0x3f3b20b4, 0x3f54d951, 0x3e764f2c, 0x3f1b3970,
                                      0x3f232dc9, 0x3e9e3be0, 0x3f0ce970, 0x3defa128};
    static const uint64_t doubles[] = {UINT64_C(0x3fe764168ea6ca89), UINT64_C(0x3fcec9e5b3672e14),
                                       UINT64_C(0x3fe465b93a78ef81), UINT64_C(0x3fe19d2e10efa128),
                                       UINT64_C(0x3fe31f174640953b), UINT64_C(0x3fd55373440b5f04),
                                       UINT64_C(0x3fd8a6f089cefe94), UINT64_C(0x3fef83d267dcd07a)};
    static const int32_t rejected[] = {516548029, 663681053, 251269761, 715581077,
                                       542832677, 827187473, 49567875,  377907320};
    mc_java_random a, b, c, d;
    int32_t out = 123;
    uint64_t saved;
    mc_java_random_seed(&a, 0);
    mc_java_random_seed(&b, 0);
    mc_java_random_seed(&c, 0);
    mc_java_random_seed(&d, 0);
    for (size_t i = 0; i < sizeof(ints) / sizeof(ints[0]); ++i) {
        CHECK(mc_java_random_next_i32(&a) == ints[i]);
        CHECK(mc_java_random_next_i64(&b) == longs[i]);
        CHECK(float_bits(mc_java_random_next_float(&c)) == floats[i]);
        CHECK(double_bits(mc_java_random_next_double(&d)) == doubles[i]);
    }
    mc_java_random_seed(&a, 0);
    for (size_t i = 0; i < sizeof(rejected) / sizeof(rejected[0]); ++i) {
        CHECK(mc_java_random_next_bounded(&a, 1073741825, &out));
        CHECK(out == rejected[i]);
    }
    saved = a.state;
    CHECK(!mc_java_random_next_bounded(&a, 0, &out) && a.state == saved);
    CHECK(!mc_java_random_next_bounded(&a, -1, &out) && a.state == saved);
    CHECK(!mc_java_random_next_bounded(&a, INT32_MIN, &out) && a.state == saved);
    CHECK(!mc_java_random_next_bounded(&a, 1, NULL) && a.state == saved);
    CHECK(out == 377907320);
    mc_java_random_seed(&a, -1);
    CHECK(mc_java_random_next_i32(&a) == 1155099827);
    mc_java_random_seed(&a, -1);
    CHECK(mc_java_random_next_i64(&a) == INT64_C(4961115982468162243));
    mc_java_random_seed(&a, -1);
    CHECK(mc_java_random_next_bounded(&a, INT32_MAX, &out) && out == 577549913);
    mc_java_random_seed(&a, -123456789);
    CHECK(mc_java_random_next_i64(&a) == INT64_C(6194098178591492762));
    mc_java_random_seed(&a, INT64_MIN);
    CHECK(mc_java_random_next_i32(&a) == ints[0]);
    mc_java_random_seed(&a, INT64_MAX);
    CHECK(mc_java_random_next_i32(&a) == 1155099827);
    mc_java_random_seed(&a, 0);
    CHECK(mc_java_random_next_bounded(&a, 1, &out) && out == 0);
    CHECK(mc_java_random_next_i32(&a) == ints[1]);
    mc_java_random_seed(&a, 0);
    CHECK(mc_java_random_next_bounded(&a, 16, &out) && out == 11);
    mc_java_random_seed(&a, 0);
    CHECK(mc_java_random_next_bounded(&a, 3, &out) && out == 0);
    mc_java_random_seed(&a, 0);
    CHECK(mc_java_random_next_i32(&a) == -1155484576);
    CHECK(mc_java_random_next_bounded(&a, 1073741825, &out) && out == 516548029);
    CHECK(mc_java_random_next_i64(&a) == -INT64_C(7261648964369397258));
    CHECK(float_bits(mc_java_random_next_float(&a)) == UINT32_C(0x3e9e3be0));
    CHECK(double_bits(mc_java_random_next_double(&a)) == UINT64_C(0x3fe19d2e10efa128));
    return 0;
}

static void vectors(void) {
    static const int32_t ints[] = {0, 1,     -1,    INT32_MIN,  INT32_MAX,  -7,
                                   7, 65535, 65536, 1431655765, -1431655766};
    static const int64_t longs[] = {0,
                                    1,
                                    -1,
                                    INT64_MIN,
                                    INT64_MAX,
                                    -7,
                                    7,
                                    INT64_C(0x123456789abcdef),
                                    -INT64_C(0x123456789abcdef),
                                    INT64_C(0x100000000),
                                    INT64_C(0xffffffff)};
    static const int32_t shifts[] = {-65, -64, -33, -32, -31, -1, 0,         1,        7,
                                     31,  32,  33,  63,  64,  65, INT32_MIN, INT32_MAX};
    static const int64_t seeds[] = {
        0, 1, -1, INT64_MIN, INT64_MAX, INT64_C(0x123456789abcdef), -123456789};
    static const int32_t bounds[] = {1, 2, 3, 16, 17, 1073741825, INT32_MAX};
    static const uint64_t doubles[] = {0,
                                       UINT64_C(0x8000000000000000),
                                       UINT64_C(0x7ff0000000000000),
                                       UINT64_C(0xfff0000000000000),
                                       UINT64_C(0x7ff8000000000000),
                                       UINT64_C(0xfff8000000000001),
                                       UINT64_C(0x3fffffffffffffff),
                                       UINT64_C(0xbfffffffffffffff),
                                       UINT64_C(0x41dfffffffffffff),
                                       UINT64_C(0x41e0000000000000),
                                       UINT64_C(0xc1e0000000000000),
                                       UINT64_C(0xc1e0000000000001),
                                       UINT64_C(0x43dfffffffffffff),
                                       UINT64_C(0x43e0000000000000),
                                       UINT64_C(0xc3e0000000000000),
                                       UINT64_C(0xc3e0000000000001),
                                       1,
                                       UINT64_C(0x8000000000000001)};
    static const uint32_t floats[] = {0,          0x80000000, 0x7f800000, 0xff800000, 0x7fc00000,
                                      0xffc00001, 0x3fffffff, 0xbfffffff, 0x4effffff, 0x4f000000,
                                      0xcf000000, 0xcf000001, 0x5effffff, 0x5f000000, 0xdf000000,
                                      0xdf000001, 1,          0x80000001};
    for (size_t i = 0; i < sizeof(ints) / sizeof(ints[0]); ++i) {
        int32_t a = ints[i];
        for (size_t j = 0; j < sizeof(ints) / sizeof(ints[0]); ++j) {
            int32_t b = ints[j], quotient = 0, remainder = 0;
            printf("i32 %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32, a, b,
                   mc_java_i32_add(a, b), mc_java_i32_sub(a, b), mc_java_i32_mul(a, b));
            if (b == 0) {
                printf(" zero\n");
            } else {
                (void)mc_java_i32_div(a, b, &quotient);
                (void)mc_java_i32_rem(a, b, &remainder);
                printf(" %" PRId32 " %" PRId32 "\n", quotient, remainder);
            }
        }
        for (size_t j = 0; j < sizeof(shifts) / sizeof(shifts[0]); ++j) {
            int32_t shift = shifts[j];
            printf("s32 %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 "\n",
                   a, shift, mc_java_i32_shl(a, shift), mc_java_i32_shr(a, shift),
                   mc_java_i32_ushr(a, shift), mc_java_i32_neg(a));
        }
    }
    for (size_t i = 0; i < sizeof(longs) / sizeof(longs[0]); ++i) {
        int64_t a = longs[i];
        for (size_t j = 0; j < sizeof(longs) / sizeof(longs[0]); ++j) {
            int64_t b = longs[j], quotient = 0, remainder = 0;
            printf("i64 %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64, a, b,
                   mc_java_i64_add(a, b), mc_java_i64_sub(a, b), mc_java_i64_mul(a, b));
            if (b == 0) {
                printf(" zero\n");
            } else {
                (void)mc_java_i64_div(a, b, &quotient);
                (void)mc_java_i64_rem(a, b, &remainder);
                printf(" %" PRId64 " %" PRId64 "\n", quotient, remainder);
            }
        }
        for (size_t j = 0; j < sizeof(shifts) / sizeof(shifts[0]); ++j) {
            int32_t shift = shifts[j];
            printf("s64 %" PRId64 " %" PRId32 " %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 "\n",
                   a, shift, mc_java_i64_shl(a, shift), mc_java_i64_shr(a, shift),
                   mc_java_i64_ushr(a, shift), mc_java_i64_neg(a));
        }
        printf("narrow %" PRId64 " %d %d %u %" PRId32 "\n", a, (int)mc_java_i8_narrow(a),
               (int)mc_java_i16_narrow(a), (unsigned int)mc_java_u16_narrow(a),
               mc_java_i32_narrow(a));
    }
    for (int32_t a = -65536; a <= 65536; ++a) {
        printf("n16 %" PRId32 " %d %d %u\n", a, (int)mc_java_i8_narrow(a),
               (int)mc_java_i16_narrow(a), (unsigned int)mc_java_u16_narrow(a));
    }
    for (size_t i = 0; i < sizeof(doubles) / sizeof(doubles[0]); ++i) {
        double value = double_from_bits(doubles[i]);
        printf("dcast %016" PRIx64 " %" PRId32 " %" PRId64 "\n", doubles[i],
               mc_java_double_to_i32(value), mc_java_double_to_i64(value));
    }
    for (size_t i = 0; i < sizeof(floats) / sizeof(floats[0]); ++i) {
        float value = float_from_bits(floats[i]);
        printf("fcast %08" PRIx32 " %" PRId32 " %" PRId64 "\n", floats[i],
               mc_java_float_to_i32(value), mc_java_float_to_i64(value));
    }
    for (size_t s = 0; s < sizeof(seeds) / sizeof(seeds[0]); ++s) {
        mc_java_random a, b, c, d, mixed;
        int64_t seed = seeds[s];
        mc_java_random_seed(&a, seed);
        mc_java_random_seed(&b, seed);
        mc_java_random_seed(&c, seed);
        mc_java_random_seed(&d, seed);
        for (int i = 0; i < 64; ++i) {
            int32_t integer = mc_java_random_next_i32(&a);
            int64_t lng = mc_java_random_next_i64(&b);
            uint32_t flt = float_bits(mc_java_random_next_float(&c));
            uint64_t dbl = double_bits(mc_java_random_next_double(&d));
            printf("random %" PRId64 " %d %" PRId32 " %" PRId64 " %08" PRIx32 " %016" PRIx64 "\n",
                   seed, i, integer, lng, flt, dbl);
        }
        for (size_t n = 0; n < sizeof(bounds) / sizeof(bounds[0]); ++n) {
            mc_java_random bounded;
            mc_java_random_seed(&bounded, seed);
            for (int i = 0; i < 64; ++i) {
                int32_t value = 0;
                (void)mc_java_random_next_bounded(&bounded, bounds[n], &value);
                printf("bound %" PRId64 " %" PRId32 " %d %" PRId32 "\n", seed, bounds[n], i, value);
            }
        }
        mc_java_random_seed(&mixed, seed);
        for (int i = 0; i < 64; ++i) {
            int32_t integer = mc_java_random_next_i32(&mixed);
            int32_t bounded = 0;
            int64_t lng;
            uint32_t flt;
            uint64_t dbl;
            (void)mc_java_random_next_bounded(&mixed, 1073741825, &bounded);
            lng = mc_java_random_next_i64(&mixed);
            flt = float_bits(mc_java_random_next_float(&mixed));
            dbl = double_bits(mc_java_random_next_double(&mixed));
            printf("mixed %" PRId64 " %d %" PRId32 " %" PRId32 " %" PRId64 " %08" PRIx32
                   " %016" PRIx64 "\n",
                   seed, i, integer, bounded, lng, flt, dbl);
        }
    }
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--vectors") == 0) {
        vectors();
        return ferror(stdout) ? 1 : 0;
    }
    if (argc != 1) {
        fprintf(stderr, "usage: %s [--vectors]\n", argv[0]);
        return 2;
    }
    if (test_integers() != 0 || test_casts() != 0 || test_random() != 0) {
        return 1;
    }
    printf("PASS: %u Java compatibility checks\n", checks);
    return 0;
}
