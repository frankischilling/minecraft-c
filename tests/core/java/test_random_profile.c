#include "mc/core/java/random_profile.h"

#include "../../../src/core/java/random_profile_internal.h"
#include "../../support/java_random.h"

#include <emmintrin.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

/* Actual private production definitions are inspected only while quiescent.
 * No clock seed, constructor hook, shadow layout or replay tail is supplied. */
static unsigned int checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

#define INITIAL_UNIQUIFIER UINT64_C(8682522807148012)
#define UNIQUIFIER_MULTIPLIER UINT64_C(1181783497276652981)
#define LCG_MASK UINT64_C(0xffffffffffff)
#define WORKERS 6u
#define CALLS 8u

_Static_assert(sizeof(mc_java_random_profile) <= SIZE_MAX - sizeof(mc_java_math_random_owner),
               "profile and separate Math owner must fit size_t");

static size_t full_budget(void) {
    return sizeof(mc_java_random_profile) + sizeof(mc_java_math_random_owner);
}

static uint64_t double_bits(const double *value) {
    uint64_t bits;
    _Static_assert(sizeof(bits) == sizeof(*value), "binary64 storage required");
    memcpy(&bits, value, sizeof(bits));
    return bits;
}

static uint64_t advance_uniquifier(uint64_t value, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        value *= UNIQUIFIER_MULTIPLIER;
    }
    return value;
}

static uint64_t uniquifier(const mc_java_random_profile *profile) {
    return atomic_load_explicit(&profile->uniquifier, memory_order_relaxed);
}

static bool holder_is(const mc_java_random_profile *profile, mc_java_random_holder_phase phase) {
    return atomic_load_explicit(&profile->holder_phase, memory_order_acquire) ==
           (unsigned int)phase;
}

static bool cold_is(const mc_java_random_profile *profile, uint64_t expected) {
    return holder_is(profile, MC_JAVA_RANDOM_HOLDER_COLD) && profile->math == NULL &&
           uniquifier(profile) == expected &&
           mc_java_random_profile_allocation_bytes(profile) == sizeof(*profile) &&
           profile->allocated_bytes == sizeof(*profile);
}

static bool fresh_is(const mc_java_random *random) {
    return random->state <= LCG_MASK && double_bits(&random->next_next_gaussian) == 0 &&
           !random->have_next_next_gaussian;
}

static bool unit_double(double value) {
    return isfinite(value) && value >= 0.0 && value < 1.0;
}

static void poison_random(mc_java_random *random) {
    memset(random, 0xa5, sizeof(*random));
    /* An omitted success-path bool write must fail without an invalid bool read. */
    random->have_next_next_gaussian = true;
}

static mc_java_random math_snapshot(const mc_java_random_profile *profile) {
    mc_java_random result = {
        .state = atomic_load_explicit(&profile->math->state, memory_order_relaxed),
        .next_next_gaussian = profile->math->next_next_gaussian,
        .have_next_next_gaussian = profile->math->have_next_next_gaussian,
    };
    return result;
}

static bool math_matches(const mc_java_random_profile *profile, const mc_java_random *expected) {
    const mc_java_random actual = math_snapshot(profile);
    return mc_test_java_random_equal(&actual, expected);
}

static int test_arguments_and_budget(void) {
    int result = 1;
    mc_java_random_profile *profile = NULL, *candidate = NULL, *tight = NULL;
    mc_java_random output;
    unsigned char before[sizeof(output)];
    double value = -7.25;
    const uint64_t value_before = double_bits(&value);

    CHECK(mc_java_random_profile_allocation_bytes(NULL) == 0);
    mc_java_random_profile_free(NULL);
    CHECK(mc_java_random_profile_create(SIZE_MAX, NULL) == MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT);
    CHECK(mc_java_random_profile_create(full_budget(), &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(profile != NULL && cold_is(profile, INITIAL_UNIQUIFIER));
    CHECK(profile->max_allocation_bytes == full_budget());

    candidate = profile;
    CHECK(mc_java_random_profile_create(0, &candidate) == MC_JAVA_RANDOM_PROFILE_LIMIT);
    CHECK(candidate == profile);
    CHECK(mc_java_random_profile_create(sizeof(*profile) - 1u, &candidate) ==
          MC_JAVA_RANDOM_PROFILE_LIMIT);
    CHECK(candidate == profile && cold_is(profile, INITIAL_UNIQUIFIER));

    poison_random(&output);
    memcpy(before, &output, sizeof(before));
    CHECK(mc_java_random_construct_default(NULL, &output) ==
          MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT);
    CHECK(memcmp(before, &output, sizeof(before)) == 0);
    CHECK(mc_java_random_construct_default(profile, NULL) ==
          MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT);
    CHECK(mc_java_math_random(NULL, &value) == MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT);
    CHECK(double_bits(&value) == value_before);
    CHECK(mc_java_math_random(profile, NULL) == MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT);
    CHECK(cold_is(profile, INITIAL_UNIQUIFIER));

    CHECK(mc_java_random_profile_create(sizeof(*tight), &tight) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(tight != NULL && cold_is(tight, INITIAL_UNIQUIFIER));
    CHECK(mc_java_math_random(tight, &value) == MC_JAVA_RANDOM_PROFILE_LIMIT);
    CHECK(double_bits(&value) == value_before);
    CHECK(holder_is(tight, MC_JAVA_RANDOM_HOLDER_FAILED));
    CHECK(tight->math == NULL && uniquifier(tight) == INITIAL_UNIQUIFIER);
    CHECK(mc_java_random_profile_allocation_bytes(tight) == sizeof(*tight));
    CHECK(mc_java_math_random(tight, &value) == MC_JAVA_RANDOM_PROFILE_HOLDER_FAILED);
    CHECK(double_bits(&value) == value_before && tight->math == NULL);
    CHECK(uniquifier(tight) == INITIAL_UNIQUIFIER);
    CHECK(mc_java_random_construct_default(tight, &output) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(fresh_is(&output));
    CHECK(uniquifier(tight) == advance_uniquifier(INITIAL_UNIQUIFIER, 1));
    CHECK(holder_is(tight, MC_JAVA_RANDOM_HOLDER_FAILED));
    CHECK(mc_java_math_random(tight, &value) == MC_JAVA_RANDOM_PROFILE_HOLDER_FAILED);
    CHECK(double_bits(&value) == value_before && tight->math == NULL);
    CHECK(uniquifier(tight) == advance_uniquifier(INITIAL_UNIQUIFIER, 1));
    CHECK(mc_java_random_profile_allocation_bytes(tight) == sizeof(*tight));
    result = 0;
cleanup:
    if (candidate != profile) {
        mc_java_random_profile_free(candidate);
    }
    mc_java_random_profile_free(tight);
    mc_java_random_profile_free(profile);
    return result;
}

static int test_sequential_owners_and_math(void) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_java_math_random_owner *retained = NULL;
    mc_java_random first, second, third, first_before, second_before, expected;
    double value;

    CHECK(mc_java_random_profile_create(full_budget(), &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(profile != NULL && cold_is(profile, INITIAL_UNIQUIFIER));
    poison_random(&first);
    CHECK(mc_java_random_construct_default(profile, &first) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(fresh_is(&first));
    CHECK(cold_is(profile, advance_uniquifier(INITIAL_UNIQUIFIER, 1)));
    (void)mc_java_random_next_gaussian(&first);
    first_before = first;
    poison_random(&second);
    CHECK(mc_java_random_construct_default(profile, &second) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(fresh_is(&second));
    CHECK(mc_test_java_random_equal(&first, &first_before));
    CHECK(cold_is(profile, advance_uniquifier(INITIAL_UNIQUIFIER, 2)));
    (void)mc_java_random_next_double(&second);
    second_before = second;
    CHECK(mc_test_java_random_equal(&first, &first_before));
    (void)mc_java_random_next_gaussian(&first);
    CHECK(mc_test_java_random_equal(&second, &second_before));
    first_before = first;

    CHECK(mc_java_math_random(profile, &value) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(unit_double(value));
    CHECK(holder_is(profile, MC_JAVA_RANDOM_HOLDER_READY) && profile->math != NULL);
    retained = profile->math;
    CHECK(mc_java_random_profile_allocation_bytes(profile) == full_budget());
    CHECK(profile->allocated_bytes == full_budget());
    CHECK(uniquifier(profile) == advance_uniquifier(INITIAL_UNIQUIFIER, 3));
    expected = math_snapshot(profile);
    CHECK(fresh_is(&expected));
    for (size_t i = 0; i < 4; ++i) {
        const double wanted = mc_java_random_next_double(&expected);
        CHECK(mc_java_math_random(profile, &value) == MC_JAVA_RANDOM_PROFILE_OK);
        CHECK(double_bits(&value) == double_bits(&wanted));
        CHECK(math_matches(profile, &expected));
        CHECK(profile->math == retained &&
              mc_java_random_profile_allocation_bytes(profile) == full_budget());
        CHECK(uniquifier(profile) == advance_uniquifier(INITIAL_UNIQUIFIER, 3));
    }
    CHECK(mc_test_java_random_equal(&first, &first_before));
    CHECK(mc_test_java_random_equal(&second, &second_before));
    poison_random(&third);
    CHECK(mc_java_random_construct_default(profile, &third) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(fresh_is(&third));
    CHECK(uniquifier(profile) == advance_uniquifier(INITIAL_UNIQUIFIER, 4));
    CHECK(profile->math == retained && math_matches(profile, &expected));
    CHECK(mc_test_java_random_equal(&first, &first_before));
    CHECK(mc_test_java_random_equal(&second, &second_before));
    result = 0;
cleanup:
    mc_java_random_profile_free(profile);
    return result;
}

/* Each wrapper restores the exact incoming control/status word before any
 * assertion, diagnostic, destruction or return, including rejected calls. */
static mc_java_random_profile_status create_with_control(unsigned int control,
                                                         mc_java_random_profile **out) {
    const unsigned int saved = _mm_getcsr();
    _mm_setcsr(control);
    const mc_java_random_profile_status status = mc_java_random_profile_create(full_budget(), out);
    _mm_setcsr(saved);
    return status;
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

static int test_fp_rejection(void) {
    static const struct {
        unsigned int clear;
        unsigned int set;
    } cases[] = {{0, 0x0040u},       {0, 0x8000u}, {0x6000u, 0x2000u}, {0x6000u, 0x4000u},
                 {0x6000u, 0x6000u}, {0x0080u, 0}, {0x0100u, 0},       {0x0200u, 0},
                 {0x0400u, 0},       {0x0800u, 0}, {0x1000u, 0}};

    int result = 1;
    mc_java_random_profile *profile = NULL, *candidate = NULL;
    mc_java_random output;
    unsigned char before[sizeof(output)];
    double value = -7.25;
    const uint64_t value_before = double_bits(&value);

    CHECK(mc_java_random_profile_create(full_budget(), &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(profile != NULL && cold_is(profile, INITIAL_UNIQUIFIER));
    poison_random(&output);
    memcpy(before, &output, sizeof(before));
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        const unsigned int saved = _mm_getcsr();
        /* Clear pending status before deliberately unmasking an exception. */
        const unsigned int unsupported = (saved & ~(cases[i].clear | 0x003fu)) | cases[i].set;
        candidate = profile;
        CHECK(create_with_control(unsupported, &candidate) ==
              MC_JAVA_RANDOM_PROFILE_UNSUPPORTED_FP);
        CHECK(_mm_getcsr() == saved && candidate == profile);
        CHECK(default_with_control(unsupported, profile, &output) ==
              MC_JAVA_RANDOM_PROFILE_UNSUPPORTED_FP);
        CHECK(_mm_getcsr() == saved && memcmp(before, &output, sizeof(before)) == 0);
        CHECK(math_with_control(unsupported, profile, &value) ==
              MC_JAVA_RANDOM_PROFILE_UNSUPPORTED_FP);
        CHECK(_mm_getcsr() == saved && double_bits(&value) == value_before);
        CHECK(cold_is(profile, INITIAL_UNIQUIFIER));
    }
    CHECK(mc_java_math_random(profile, &value) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(unit_double(value) && holder_is(profile, MC_JAVA_RANDOM_HOLDER_READY));
    CHECK(uniquifier(profile) == advance_uniquifier(INITIAL_UNIQUIFIER, 1));
    CHECK(mc_java_random_profile_allocation_bytes(profile) == full_budget());
    result = 0;
cleanup:
    if (candidate != profile) {
        mc_java_random_profile_free(candidate);
    }
    mc_java_random_profile_free(profile);
    return result;
}

typedef enum worker_mode { WORKER_MATH, WORKER_DEFAULT, WORKER_MIXED } worker_mode;

typedef struct start_gate {
    mtx_t mutex;
    cnd_t condition;
    bool open;
} start_gate;

typedef struct worker {
    start_gate *gate;
    mc_java_random_profile *profile;
    worker_mode mode;
    mc_java_random_profile_status status[CALLS];
    mc_java_random ordinary[CALLS];
    double values[CALLS];
} worker;

static bool default_call(worker_mode mode, size_t call) {
    return mode == WORKER_DEFAULT || (mode == WORKER_MIXED && call % 2u == 0);
}

static void require_thread_success(int status);

static int worker_main(void *argument) {
    worker *work = argument;
    require_thread_success(mtx_lock(&work->gate->mutex));
    while (!work->gate->open) {
        require_thread_success(cnd_wait(&work->gate->condition, &work->gate->mutex));
    }
    require_thread_success(mtx_unlock(&work->gate->mutex));
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        return 1;
    }
    int result = 0;
    for (size_t i = 0; i < CALLS; ++i) {
        if (default_call(work->mode, i)) {
            work->status[i] = mc_java_random_construct_default(work->profile, &work->ordinary[i]);
        } else {
            work->status[i] = mc_java_math_random(work->profile, &work->values[i]);
        }
        if (work->status[i] != MC_JAVA_RANDOM_PROFILE_OK) {
            result = 1;
        }
    }
    return result;
}

static void require_thread_success(int status) {
    /* A broken synchronization/join primitive cannot permit teardown beneath
     * a potentially live borrower. Fail the process rather than return unsafely. */
    if (status != thrd_success) {
        fprintf(stderr, "C17 thread synchronization/join failed: %d\n", status);
        abort();
    }
}

static bool run_workers(mc_java_random_profile *profile, worker_mode mode, worker work[WORKERS]) {
    start_gate gate;
    thrd_t threads[WORKERS];
    size_t created = 0;
    bool complete = true;

    if (mtx_init(&gate.mutex, mtx_plain) != thrd_success) {
        return false;
    }
    if (cnd_init(&gate.condition) != thrd_success) {
        mtx_destroy(&gate.mutex);
        return false;
    }
    gate.open = false;
    for (size_t i = 0; i < WORKERS; ++i) {
        work[i].gate = &gate;
        work[i].profile = profile;
        work[i].mode = mode;
        for (size_t call = 0; call < CALLS; ++call) {
            work[i].status[call] = MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT;
            poison_random(&work[i].ordinary[call]);
            work[i].values[call] = -7.25;
        }
        if (thrd_create(&threads[i], worker_main, &work[i]) != thrd_success) {
            complete = false;
            break;
        }
        ++created;
    }

    /* Release and join ALL created workers, even after partial thrd_create failure. */
    require_thread_success(mtx_lock(&gate.mutex));
    gate.open = true;
    require_thread_success(cnd_broadcast(&gate.condition));
    require_thread_success(mtx_unlock(&gate.mutex));
    for (size_t i = 0; i < created; ++i) {
        int status = 1;
        require_thread_success(thrd_join(threads[i], &status));
        if (status != 0) {
            complete = false;
        }
    }
    cnd_destroy(&gate.condition);
    mtx_destroy(&gate.mutex);
    return complete;
}

static bool worker_outputs_are_valid(const worker work[WORKERS], worker_mode mode) {
    for (size_t i = 0; i < WORKERS; ++i) {
        for (size_t call = 0; call < CALLS; ++call) {
            if (work[i].status[call] != MC_JAVA_RANDOM_PROFILE_OK) {
                return false;
            }
            if (default_call(mode, call)) {
                if (!fresh_is(&work[i].ordinary[call])) {
                    return false;
                }
            } else if (!unit_double(work[i].values[call])) {
                return false;
            }
        }
    }
    return true;
}

static void consume_doubles(mc_java_random *random, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        (void)mc_java_random_next_double(random);
    }
}

static int test_concurrent_groups(void) {
    int result = 1;
    mc_java_random_profile *profile = NULL, *defaults = NULL;
    mc_java_math_random_owner *retained = NULL;
    worker work[WORKERS];
    mc_java_random expected;
    uint64_t before;
    const size_t total_calls = (size_t)WORKERS * CALLS;

    CHECK(mc_java_random_profile_create(full_budget(), &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(profile != NULL && cold_is(profile, INITIAL_UNIQUIFIER));
    CHECK(run_workers(profile, WORKER_MATH, work));
    CHECK(worker_outputs_are_valid(work, WORKER_MATH));
    CHECK(holder_is(profile, MC_JAVA_RANDOM_HOLDER_READY) && profile->math != NULL);
    retained = profile->math;
    CHECK(uniquifier(profile) == advance_uniquifier(INITIAL_UNIQUIFIER, 1));
    CHECK(mc_java_random_profile_allocation_bytes(profile) == full_budget());
    CHECK(profile->allocated_bytes == full_budget());
    expected = math_snapshot(profile);
    CHECK(fresh_is(&expected));

    /* The unknown cold clock seed is NOT inferred. Its actual quiescent final
     * state becomes the baseline for exact ready-state aggregate advancement. */
    before = uniquifier(profile);
    consume_doubles(&expected, total_calls);
    CHECK(run_workers(profile, WORKER_MATH, work));
    CHECK(worker_outputs_are_valid(work, WORKER_MATH));
    CHECK(profile->math == retained && math_matches(profile, &expected));
    CHECK(uniquifier(profile) == before);
    CHECK(mc_java_random_profile_allocation_bytes(profile) == full_budget());

    CHECK(mc_java_random_profile_create(full_budget(), &defaults) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(defaults != NULL && cold_is(defaults, INITIAL_UNIQUIFIER));
    CHECK(run_workers(defaults, WORKER_DEFAULT, work));
    CHECK(worker_outputs_are_valid(work, WORKER_DEFAULT));
    CHECK(cold_is(defaults, advance_uniquifier(INITIAL_UNIQUIFIER, total_calls)));
    CHECK(profile->math == retained && math_matches(profile, &expected));
    CHECK(uniquifier(profile) == before);

    /* Every worker alternates four disjoint ordinary outputs with four Math
     * calls. Only aggregate state is asserted; result pairs may interleave. */
    consume_doubles(&expected, total_calls / 2u);
    CHECK(run_workers(profile, WORKER_MIXED, work));
    CHECK(worker_outputs_are_valid(work, WORKER_MIXED));
    CHECK(uniquifier(profile) == advance_uniquifier(before, total_calls / 2u));
    CHECK(profile->math == retained && math_matches(profile, &expected));
    CHECK(holder_is(profile, MC_JAVA_RANDOM_HOLDER_READY));
    CHECK(mc_java_random_profile_allocation_bytes(profile) == full_budget());
    CHECK(cold_is(defaults, advance_uniquifier(INITIAL_UNIQUIFIER, total_calls)));
    result = 0;
cleanup:
    /* run_workers returns only after all created borrowers have been joined. */
    mc_java_random_profile_free(defaults);
    mc_java_random_profile_free(profile);
    return result;
}

int main(void) {
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        fprintf(stderr, "Random profile test requires the strict SSE2 control environment\n");
        return 1;
    }
    if (test_arguments_and_budget() != 0 || test_sequential_owners_and_math() != 0 ||
        test_fp_rejection() != 0 || test_concurrent_groups() != 0) {
        return 1;
    }
    printf("PASS: %u Random profile checks\n", checks);
    return 0;
}
