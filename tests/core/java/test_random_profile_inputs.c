#include "../../../src/core/java/random_profile_internal.h"

#include "../../support/java_random.h"

#include <emmintrin.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define INITIAL_UNIQUIFIER UINT64_C(8682522807148012)
#define UNIQUIFIER_MULTIPLIER UINT64_C(1181783497276652981)
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

/* Constructed scalar inputs, not captured JVM clock measurements. Clock values
 * are declared independently of outputs. Literal U/state/Math vectors follow
 * full-width multiply, XOR/scramble and the two Java LCG steps; no production
 * finish_default call supplies expected state. Expected seeds/states are never
 * installed in a profile or Random owner. Fresh output bytes are poisoned only
 * to check complete initialization and failure preservation. Existing real-
 * clock/concurrency tests remain a separate suite. */
static const mc_java_random_constructor_input inputs[] = {
    {UINT64_C(0x2fd89ddcd351e3dc), 0},
    {UINT64_C(0xd72bcc3aedda868c), -1},
    {UINT64_C(0x7e5bfab66f0d7cfc), INT64_MIN},
    {UINT64_C(0xb750d96441b26a2c), INT64_MAX},
    {UINT64_C(0x92cc8c52d6068d1c), INT64_C(0x0123456789abcdef)},
};

static const uint64_t states[] = {
    UINT64_C(0x9dd90dbd05b1), UINT64_C(0x33c0ccc99f1e), UINT64_C(0xfab3b1e19a91),
    UINT64_C(0x269e60a173be), UINT64_C(0xc9308141a69e),
};

static unsigned int checks;

static size_t full_budget(void) {
    return sizeof(mc_java_random_profile) + sizeof(mc_java_math_random_owner);
}

static uint64_t uniquifier(const mc_java_random_profile *profile) {
    return atomic_load_explicit(&profile->uniquifier, memory_order_seq_cst);
}

static bool admitted(const mc_java_random_profile *profile) {
    return atomic_load_explicit(&profile->constructor_admitted, memory_order_relaxed);
}

static bool holder_is(const mc_java_random_profile *profile, mc_java_random_holder_phase phase) {
    return atomic_load_explicit(&profile->holder_phase, memory_order_acquire) ==
           (unsigned int)phase;
}

static uint64_t double_bits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void poison(mc_java_random *random) {
    memset(random, 0xa5, sizeof(*random));
    random->have_next_next_gaussian = true;
}

static bool fresh(const mc_java_random *random, uint64_t expected) {
    return random->state == expected && double_bits(random->next_next_gaussian) == 0 &&
           !random->have_next_next_gaussian;
}

static bool tape_is(const mc_java_random_profile *profile, bool bound, size_t count,
                    size_t consumed, mc_java_random_constructor_input_failure failure) {
    mc_java_random_constructor_inputs_view view;
    return mc_java_random_profile_constructor_inputs_get(profile, &view) ==
               MC_JAVA_RANDOM_PROFILE_OK &&
           view.bound == bound && view.count == count && view.consumed == consumed &&
           view.failure == failure;
}

static bool untouched(const mc_java_random_profile *profile, const mc_platform_nanotime *clock) {
    return uniquifier(profile) == INITIAL_UNIQUIFIER && !admitted(profile) &&
           holder_is(profile, MC_JAVA_RANDOM_HOLDER_COLD) && profile->math == NULL &&
           profile->allocated_bytes == sizeof(*profile) &&
           profile->max_allocation_bytes == full_budget() &&
           profile->clock.frequency == clock->frequency &&
           profile->clock.initial_sample == clock->initial_sample &&
           profile->clock.initialized == clock->initialized &&
           profile->constructor_inputs == NULL &&
           tape_is(profile, false, 0, 0, MC_JAVA_RANDOM_INPUT_NONE);
}

static int binding_contracts(void) {
    int result = 1;
    mc_java_random_profile *profile = NULL, *large = NULL;
    mc_java_random_constructor_input *many = NULL;
    mc_java_random_constructor_input bad[COUNT(inputs)];
    CHECK(mc_java_random_profile_create(full_budget(), &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    const mc_platform_nanotime clock = profile->clock;
    CHECK(untouched(profile, &clock));
    CHECK(mc_java_random_profile_constructor_inputs_complete(profile) ==
          MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT);
    CHECK(mc_java_random_profile_bind_constructor_inputs(NULL, inputs, COUNT(inputs)) ==
          MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT);
    CHECK(mc_java_random_profile_bind_constructor_inputs(profile, NULL, 1) ==
          MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT);
    CHECK(mc_java_random_profile_bind_constructor_inputs(
              profile, inputs, (size_t)MC_JAVA_RANDOM_PROFILE_MAX_CONSTRUCTOR_INPUTS + 1u) ==
          MC_JAVA_RANDOM_PROFILE_LIMIT);
    CHECK(untouched(profile, &clock));
    memcpy(bad, inputs, sizeof(bad));
    bad[1].committed_uniquifier ^= UINT64_C(0x8000000000000000);
    CHECK(mc_java_random_profile_bind_constructor_inputs(profile, bad, COUNT(bad)) ==
          MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT);
    CHECK(untouched(profile, &clock));
    memcpy(bad, inputs, sizeof(bad));
    bad[1] = inputs[2];
    bad[2] = inputs[1];
    CHECK(mc_java_random_profile_bind_constructor_inputs(profile, bad, COUNT(bad)) ==
          MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT);
    CHECK(untouched(profile, &clock));

    /* Validate the last entry at the admitted bound, without allocating or
     * modifying the profile. Every declared sample is the scalar zero. */
    many = calloc(MC_JAVA_RANDOM_PROFILE_MAX_CONSTRUCTOR_INPUTS, sizeof(*many));
    CHECK(many != NULL);
    uint64_t next = INITIAL_UNIQUIFIER;
    for (size_t index = 0; index < MC_JAVA_RANDOM_PROFILE_MAX_CONSTRUCTOR_INPUTS; ++index) {
        next *= UNIQUIFIER_MULTIPLIER;
        many[index].committed_uniquifier = next;
    }
    many[MC_JAVA_RANDOM_PROFILE_MAX_CONSTRUCTOR_INPUTS - 1u].committed_uniquifier ^= UINT64_C(1);
    CHECK(mc_java_random_profile_bind_constructor_inputs(
              profile, many, MC_JAVA_RANDOM_PROFILE_MAX_CONSTRUCTOR_INPUTS) ==
          MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT);
    CHECK(untouched(profile, &clock));
    many[MC_JAVA_RANDOM_PROFILE_MAX_CONSTRUCTOR_INPUTS - 1u].committed_uniquifier ^= UINT64_C(1);
    CHECK(mc_java_random_profile_create(sizeof(*large), &large) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_java_random_profile_bind_constructor_inputs(
              large, many, MC_JAVA_RANDOM_PROFILE_MAX_CONSTRUCTOR_INPUTS) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(tape_is(large, true, MC_JAVA_RANDOM_PROFILE_MAX_CONSTRUCTOR_INPUTS, 0,
                  MC_JAVA_RANDOM_INPUT_NONE) &&
          large->constructor_inputs == many && large->allocated_bytes == sizeof(*large));
    mc_java_random_profile_free(large);
    large = NULL;
    free(many);
    many = NULL;

    CHECK(mc_java_random_profile_bind_constructor_inputs(profile, inputs, COUNT(inputs)) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(tape_is(profile, true, COUNT(inputs), 0, MC_JAVA_RANDOM_INPUT_NONE) &&
          profile->constructor_inputs == inputs && uniquifier(profile) == INITIAL_UNIQUIFIER &&
          !admitted(profile));
    CHECK(mc_java_random_profile_bind_constructor_inputs(profile, NULL, 0) ==
          MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT);
    CHECK(tape_is(profile, true, COUNT(inputs), 0, MC_JAVA_RANDOM_INPUT_NONE));
    mc_java_random_constructor_inputs_view view = {true, 77u, 11u,
                                                   MC_JAVA_RANDOM_INPUT_ORDER_MISMATCH};
    CHECK(mc_java_random_profile_constructor_inputs_get(NULL, &view) ==
              MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT &&
          view.bound && view.count == 77u && view.consumed == 11u &&
          view.failure == MC_JAVA_RANDOM_INPUT_ORDER_MISMATCH);
    CHECK(mc_java_random_profile_constructor_inputs_get(profile, NULL) ==
          MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT);
    result = 0;
cleanup:
    mc_java_random_profile_free(large);
    mc_java_random_profile_free(profile);
    free(many);
    return result;
}

static int ordinary_and_math(void) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_java_random first, later, retained;
    unsigned char before[sizeof(later)];
    static const uint64_t math_bits[] = {
        UINT64_C(0x3fefd0a148fc89b2),
        UINT64_C(0x3fdabb367012d2a4),
        UINT64_C(0x3feb137c4512ffce),
        UINT64_C(0x3faea9279896c2e0),
    };
    static const uint64_t math_states[] = {
        UINT64_C(0x1f9136575608),
        UINT64_C(0x012d2a5c8002),
        UINT64_C(0xa25ff9de7b8c),
        UINT64_C(0x312d85c20b26),
    };
    CHECK(mc_java_random_profile_create(full_budget(), &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_java_random_profile_bind_constructor_inputs(profile, inputs, COUNT(inputs)) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    poison(&first);
    CHECK(mc_java_random_construct_default(profile, &first) == MC_JAVA_RANDOM_PROFILE_OK &&
          fresh(&first, states[0]) && uniquifier(profile) == inputs[0].committed_uniquifier &&
          admitted(profile) && holder_is(profile, MC_JAVA_RANDOM_HOLDER_COLD));
    CHECK(tape_is(profile, true, COUNT(inputs), 1, MC_JAVA_RANDOM_INPUT_NONE));
    (void)mc_java_random_next_i32(&first);
    retained = first;
    mc_java_math_random_owner *math = NULL;
    for (size_t index = 0; index < COUNT(math_bits); ++index) {
        double value = -7.25;
        CHECK(mc_java_math_random(profile, &value) == MC_JAVA_RANDOM_PROFILE_OK &&
              double_bits(value) == math_bits[index] && profile->math != NULL &&
              holder_is(profile, MC_JAVA_RANDOM_HOLDER_READY));
        if (index == 0)
            math = profile->math;
        CHECK(profile->math == math &&
              atomic_load_explicit(&math->state, memory_order_seq_cst) == math_states[index] &&
              double_bits(math->next_next_gaussian) == 0 && !math->have_next_next_gaussian &&
              uniquifier(profile) == inputs[1].committed_uniquifier &&
              mc_java_random_profile_allocation_bytes(profile) == full_budget());
        CHECK(tape_is(profile, true, COUNT(inputs), 2, MC_JAVA_RANDOM_INPUT_NONE));
        CHECK(mc_test_java_random_equal(&first, &retained));
    }
    for (size_t index = 2; index < COUNT(inputs); ++index) {
        poison(&later);
        CHECK(mc_java_random_construct_default(profile, &later) == MC_JAVA_RANDOM_PROFILE_OK &&
              fresh(&later, states[index]) &&
              uniquifier(profile) == inputs[index].committed_uniquifier);
        CHECK(tape_is(profile, true, COUNT(inputs), index + 1u, MC_JAVA_RANDOM_INPUT_NONE));
        CHECK(profile->math == math && mc_test_java_random_equal(&first, &retained) &&
              atomic_load_explicit(&math->state, memory_order_seq_cst) == math_states[3]);
    }
    CHECK(mc_java_random_profile_constructor_inputs_complete(profile) == MC_JAVA_RANDOM_PROFILE_OK);
    poison(&later);
    memcpy(before, &later, sizeof(before));
    CHECK(mc_java_random_construct_default(profile, &later) == MC_JAVA_RANDOM_PROFILE_RANGE &&
          memcmp(before, &later, sizeof(before)) == 0 &&
          uniquifier(profile) == UINT64_C(0x9c7c4f70379a70cc));
    CHECK(tape_is(profile, true, COUNT(inputs), COUNT(inputs), MC_JAVA_RANDOM_INPUT_EXHAUSTED));
    CHECK(mc_java_random_construct_default(profile, &later) == MC_JAVA_RANDOM_PROFILE_RANGE &&
          memcmp(before, &later, sizeof(before)) == 0 &&
          uniquifier(profile) == UINT64_C(0x8f601f5669ff5c3c));
    CHECK(mc_java_random_profile_constructor_inputs_complete(profile) ==
          MC_JAVA_RANDOM_PROFILE_RANGE);
    result = 0;
cleanup:
    mc_java_random_profile_free(profile);
    return result;
}

static int failed_input_prefixes(void) {
    int result = 1;
    mc_java_random_profile *empty = NULL, *extra = NULL, *order = NULL;
    mc_java_random random;
    unsigned char before[sizeof(random)];
    mc_java_random_constructor_input corrupt[2] = {inputs[0], inputs[1]};
    poison(&random);
    memcpy(before, &random, sizeof(before));
    CHECK(mc_java_random_profile_create(full_budget(), &empty) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_java_random_profile_bind_constructor_inputs(empty, NULL, 0) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(tape_is(empty, true, 0, 0, MC_JAVA_RANDOM_INPUT_NONE) && !admitted(empty));
    CHECK(mc_java_random_profile_bind_constructor_inputs(empty, NULL, 0) ==
          MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT);
    CHECK(mc_java_random_profile_constructor_inputs_complete(empty) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_java_random_construct_default(empty, &random) == MC_JAVA_RANDOM_PROFILE_RANGE &&
          memcmp(before, &random, sizeof(before)) == 0 &&
          uniquifier(empty) == inputs[0].committed_uniquifier && admitted(empty));
    CHECK(tape_is(empty, true, 0, 0, MC_JAVA_RANDOM_INPUT_EXHAUSTED));
    double value = -7.25;
    CHECK(mc_java_math_random(empty, &value) == MC_JAVA_RANDOM_PROFILE_RANGE &&
          double_bits(value) == UINT64_C(0xc01d000000000000) &&
          uniquifier(empty) == inputs[1].committed_uniquifier && empty->math == NULL &&
          holder_is(empty, MC_JAVA_RANDOM_HOLDER_FAILED) &&
          empty->allocated_bytes == sizeof(*empty));
    CHECK(mc_java_math_random(empty, &value) == MC_JAVA_RANDOM_PROFILE_HOLDER_FAILED &&
          uniquifier(empty) == inputs[1].committed_uniquifier);

    CHECK(mc_java_random_profile_create(full_budget(), &extra) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_java_random_profile_bind_constructor_inputs(extra, inputs, 2) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_java_random_construct_default(extra, &random) == MC_JAVA_RANDOM_PROFILE_OK &&
          fresh(&random, states[0]));
    CHECK(mc_java_random_profile_constructor_inputs_complete(extra) ==
          MC_JAVA_RANDOM_PROFILE_RANGE);
    CHECK(tape_is(extra, true, 2, 1, MC_JAVA_RANDOM_INPUT_EXTRA_INPUTS));
    poison(&random);
    memcpy(before, &random, sizeof(before));
    CHECK(mc_java_random_construct_default(extra, &random) == MC_JAVA_RANDOM_PROFILE_RANGE &&
          memcmp(before, &random, sizeof(before)) == 0 &&
          uniquifier(extra) == inputs[1].committed_uniquifier);
    CHECK(tape_is(extra, true, 2, 1, MC_JAVA_RANDOM_INPUT_EXTRA_INPUTS));

    CHECK(mc_java_random_profile_create(full_budget(), &order) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_java_random_profile_bind_constructor_inputs(order, corrupt, COUNT(corrupt)) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_java_random_construct_default(order, &random) == MC_JAVA_RANDOM_PROFILE_OK);
    poison(&random);
    memcpy(before, &random, sizeof(before));
    /* Deliberate post-bind corruption violates the immutable tape contract.
     * This checks its defensive late guard; no actual U/Random is assigned. */
    corrupt[1].committed_uniquifier ^= UINT64_C(0x8000000000000000);
    const mc_java_random_profile_status status = mc_java_random_construct_default(order, &random);
    corrupt[1] = inputs[1];
    CHECK(status == MC_JAVA_RANDOM_PROFILE_RANGE && memcmp(before, &random, sizeof(before)) == 0 &&
          uniquifier(order) == inputs[1].committed_uniquifier);
    CHECK(tape_is(order, true, 2, 1, MC_JAVA_RANDOM_INPUT_ORDER_MISMATCH));
    CHECK(mc_java_random_profile_constructor_inputs_complete(order) ==
          MC_JAVA_RANDOM_PROFILE_RANGE);
    CHECK(mc_java_random_construct_default(order, &random) == MC_JAVA_RANDOM_PROFILE_RANGE &&
          memcmp(before, &random, sizeof(before)) == 0 &&
          uniquifier(order) == inputs[2].committed_uniquifier);
    result = 0;
cleanup:
    mc_java_random_profile_free(order);
    mc_java_random_profile_free(extra);
    mc_java_random_profile_free(empty);
    return result;
}

static mc_java_random_profile_status
default_with_control(unsigned int control, mc_java_random_profile *profile, mc_java_random *out) {
    const unsigned int saved = _mm_getcsr();
    _mm_setcsr(control);
    const mc_java_random_profile_status status = mc_java_random_construct_default(profile, out);
    _mm_setcsr(saved);
    return status;
}

static mc_java_random_profile_status
math_with_control(unsigned int control, mc_java_random_profile *profile, double *out) {
    const unsigned int saved = _mm_getcsr();
    _mm_setcsr(control);
    const mc_java_random_profile_status status = mc_java_math_random(profile, out);
    _mm_setcsr(saved);
    return status;
}

static int admission_and_freshness(void) {
    int result = 1;
    mc_java_random_profile *profile = NULL, *real = NULL, *tight = NULL, *bound_tight = NULL;
    mc_java_random random;
    unsigned char before[sizeof(random)];
    double value = -7.25;
    CHECK(mc_java_random_profile_create(full_budget(), &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    poison(&random);
    memcpy(before, &random, sizeof(before));
    CHECK(mc_java_random_construct_default(profile, NULL) ==
          MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT);
    CHECK(mc_java_math_random(profile, NULL) == MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT);
    const unsigned int saved = _mm_getcsr();
    CHECK(default_with_control(saved | 0x8000u, profile, &random) ==
              MC_JAVA_RANDOM_PROFILE_UNSUPPORTED_FP &&
          _mm_getcsr() == saved && memcmp(before, &random, sizeof(before)) == 0 &&
          !admitted(profile));
    CHECK(uniquifier(profile) == INITIAL_UNIQUIFIER &&
          holder_is(profile, MC_JAVA_RANDOM_HOLDER_COLD));
    CHECK(mc_java_random_profile_bind_constructor_inputs(profile, inputs, 1) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    static const unsigned int controls[] = {0x0040u, 0x8000u, 0x2000u};
    for (size_t index = 0; index < COUNT(controls); ++index) {
        CHECK(default_with_control(saved | controls[index], profile, &random) ==
                  MC_JAVA_RANDOM_PROFILE_UNSUPPORTED_FP &&
              _mm_getcsr() == saved && memcmp(before, &random, sizeof(before)) == 0);
        CHECK(math_with_control(saved | controls[index], profile, &value) ==
                  MC_JAVA_RANDOM_PROFILE_UNSUPPORTED_FP &&
              _mm_getcsr() == saved && double_bits(value) == UINT64_C(0xc01d000000000000));
        CHECK(tape_is(profile, true, 1, 0, MC_JAVA_RANDOM_INPUT_NONE) && !admitted(profile) &&
              uniquifier(profile) == INITIAL_UNIQUIFIER &&
              holder_is(profile, MC_JAVA_RANDOM_HOLDER_COLD));
    }
    CHECK(mc_java_random_construct_default(profile, &random) == MC_JAVA_RANDOM_PROFILE_OK &&
          fresh(&random, states[0]));
    CHECK(mc_java_random_profile_constructor_inputs_complete(profile) == MC_JAVA_RANDOM_PROFILE_OK);

    /* A genuine real-clock constructor makes later binding unavailable. */
    CHECK(mc_java_random_profile_create(full_budget(), &real) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_java_random_construct_default(real, &random) == MC_JAVA_RANDOM_PROFILE_OK);
    const mc_java_random retained = random;
    CHECK(mc_java_random_profile_bind_constructor_inputs(real, inputs, COUNT(inputs)) ==
          MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT);
    CHECK(tape_is(real, false, 0, 0, MC_JAVA_RANDOM_INPUT_NONE) && admitted(real) &&
          uniquifier(real) == inputs[0].committed_uniquifier &&
          mc_test_java_random_equal(&random, &retained));

    /* Math allocation rejection happens before its default constructor/CAS. */
    CHECK(mc_java_random_profile_create(sizeof(*tight), &tight) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_java_math_random(tight, &value) == MC_JAVA_RANDOM_PROFILE_LIMIT && !admitted(tight) &&
          uniquifier(tight) == INITIAL_UNIQUIFIER &&
          holder_is(tight, MC_JAVA_RANDOM_HOLDER_FAILED));
    CHECK(mc_java_random_profile_bind_constructor_inputs(tight, inputs, 1) ==
          MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT);
    CHECK(tape_is(tight, false, 0, 0, MC_JAVA_RANDOM_INPUT_NONE));
    CHECK(mc_java_random_profile_create(sizeof(*bound_tight), &bound_tight) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_java_random_profile_bind_constructor_inputs(bound_tight, inputs, 1) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_java_math_random(bound_tight, &value) == MC_JAVA_RANDOM_PROFILE_LIMIT &&
          double_bits(value) == UINT64_C(0xc01d000000000000) && !admitted(bound_tight) &&
          uniquifier(bound_tight) == INITIAL_UNIQUIFIER && bound_tight->math == NULL);
    CHECK(tape_is(bound_tight, true, 1, 0, MC_JAVA_RANDOM_INPUT_NONE));
    CHECK(mc_java_random_construct_default(bound_tight, &random) == MC_JAVA_RANDOM_PROFILE_OK &&
          fresh(&random, states[0]));
    CHECK(mc_java_math_random(bound_tight, &value) == MC_JAVA_RANDOM_PROFILE_HOLDER_FAILED &&
          uniquifier(bound_tight) == inputs[0].committed_uniquifier);
    CHECK(mc_java_random_profile_constructor_inputs_complete(bound_tight) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    result = 0;
cleanup:
    mc_java_random_profile_free(bound_tight);
    mc_java_random_profile_free(tight);
    mc_java_random_profile_free(real);
    mc_java_random_profile_free(profile);
    return result;
}

int main(void) {
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        fprintf(stderr, "constructor input tests require the qualified strict SSE environment\n");
        return 1;
    }
    if (binding_contracts() || ordinary_and_math() || failed_input_prefixes() ||
        admission_and_freshness())
        return 1;
    printf("Random profile constructor inputs: %u checks passed\n", checks);
    return 0;
}
