#include "mc/core/java/java.h"

#include "../../../src/core/java/random_profile_internal.h"

#include <emmintrin.h>
#include <stdio.h>
#include <string.h>

/* Literal actual Windows JDK17 -Xint JDI observations, not C-generated expectations.
 * tests/reference/java/random_constructor_observation.tsv
 * SHA256 7c0034058912efc2791edfef70ebdcbf878b1a584457edf456dc716ee7f6cf88
 * tests/reference/java/random_constructor_provenance.json
 * SHA256 93146a3ecd01f8e1a86d9bbac89582a20e0e0a1ac364d200e36fded486e1acd9
 * Replay covers the production recorded-input constructor tail and plain Random
 * continuation. It does not create a profile, sample a clock, advance shared U,
 * or reproduce the production atomic Math holder. */
static unsigned int checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static uint64_t double_bits(const double *value) {
    uint64_t bits;
    _Static_assert(sizeof(bits) == sizeof(*value), "binary64 storage required");
    memcpy(&bits, value, sizeof(bits));
    return bits;
}

static bool state_is(const mc_java_random *random, uint64_t state, uint64_t cache, bool flag) {
    return random->state == state && double_bits(&random->next_next_gaussian) == cache &&
           random->have_next_next_gaussian == flag;
}

static void poison_fresh(mc_java_random *random) {
    memset(random, 0xa5, sizeof(*random));
    /* Keep every semantic representation valid if a fresh initializer omits a
     * write. Signed zero exposes a missing +0 cache write without NaN handling. */
    random->next_next_gaussian = -0.0;
    random->have_next_next_gaussian = true;
}

int main(void) {
    mc_java_random a, e, m, b;
    double value;
    poison_fresh(&a);
    poison_fresh(&e);
    poison_fresh(&m);
    poison_fresh(&b);

    /* Admit the qualified Gaussian backend environment; do not alter it. */
    CHECK((_mm_getcsr() & 0xffc0u) == 0x1f80u);

    /* default_a: actual uniquifier_return, nano_time_return, default_return A. */
    mc_java_random_finish_default(UINT64_C(0x2fd89ddcd351e3dc), INT64_C(0x000010be0e23c774), &a);
    CHECK(state_is(&a, UINT64_C(0x00008d67039ec2c5), UINT64_C(0x0000000000000000), false));

    /* explicit_e / gaussian_e / reseed_e: actual return and after-marker states. */
    mc_java_random_seed(&e, INT64_C(0));
    CHECK(state_is(&e, UINT64_C(0x00000005deece66d), UINT64_C(0x0000000000000000), false));
    value = mc_java_random_next_gaussian(&e);
    CHECK(double_bits(&value) == UINT64_C(0x3fe9ae59d1d6f861));
    CHECK(state_is(&e, UINT64_C(0x00009b3970be8d41), UINT64_C(0xbfecd9772eb2e0c8), true));
    mc_java_random_reseed(&e, -INT64_C(1));
    CHECK(state_is(&e, UINT64_C(0x0000fffa21131992), UINT64_C(0xbfecd9772eb2e0c8), false));

    /* math_first: actual holder constructor, followed by its first real draw. */
    mc_java_random_finish_default(UINT64_C(0xd72bcc3aedda868c), INT64_C(0x000010be1033daf4), &m);
    CHECK(state_is(&m, UINT64_C(0x0000dc812305ba15), UINT64_C(0x0000000000000000), false));
    value = mc_java_random_next_double(&m);
    CHECK(double_bits(&value) == UINT64_C(0x3fe97039f9f995b5));
    CHECK(state_is(&m, UINT64_C(0x00003f32b6ac0b57), UINT64_C(0x0000000000000000), false));

    /* math_later: continue the same local owner, without reseeding or copying it. */
    value = mc_java_random_next_double(&m);
    CHECK(double_bits(&value) == UINT64_C(0x3fd2764fe578da5c));
    CHECK(state_is(&m, UINT64_C(0x0000578da5cff369), UINT64_C(0x0000000000000000), false));

    /* default_b happens between M's second and third calls in the actual trace. */
    mc_java_random_finish_default(UINT64_C(0x7e5bfab66f0d7cfc), INT64_C(0x000010be118ddb20), &b);
    CHECK(state_is(&b, UINT64_C(0x0000ea0da06c41b1), UINT64_C(0x0000000000000000), false));
    CHECK(state_is(&m, UINT64_C(0x0000578da5cff369), UINT64_C(0x0000000000000000), false));

    value = mc_java_random_next_double(&m);
    CHECK(double_bits(&value) == UINT64_C(0x3fdf07b84f714d14));
    CHECK(state_is(&m, UINT64_C(0x0000f714d15ad6cb), UINT64_C(0x0000000000000000), false));

    /* done after: unrelated owners retain their actual observed complete states. */
    CHECK(state_is(&a, UINT64_C(0x00008d67039ec2c5), UINT64_C(0x0000000000000000), false));
    CHECK(state_is(&e, UINT64_C(0x0000fffa21131992), UINT64_C(0xbfecd9772eb2e0c8), false));
    CHECK(state_is(&b, UINT64_C(0x0000ea0da06c41b1), UINT64_C(0x0000000000000000), false));

    printf("Random constructor observation: %u checks passed\n", checks);
    return 0;
}
