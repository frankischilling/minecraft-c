#if defined(__linux__)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#elif _POSIX_C_SOURCE < 200809L
#error "nanotime tests require POSIX.1-2008 declarations"
#endif
#endif

#include "nanotime.h"

#include <emmintrin.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
_Static_assert(sizeof(LONGLONG) == sizeof(int64_t), "64-bit Windows counter required");
_Static_assert((LONGLONG)-1 < (LONGLONG)0, "signed Windows counter required");
#elif defined(__linux__)
#include <time.h>
_Static_assert((time_t)-1 < (time_t)0, "signed Linux time_t required");
_Static_assert(sizeof(time_t) <= sizeof(int64_t), "Linux time_t must fit int64_t");
_Static_assert(sizeof(long) <= sizeof(int64_t), "Linux tv_nsec must fit int64_t");
#else
#error "nanotime tests require the selected Windows/Linux profile"
#endif

static unsigned int checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

typedef struct guarded_output {
    uint64_t before;
    int64_t value;
    uint64_t after;
} guarded_output;

static guarded_output make_output(void)
{
    guarded_output out;
    memset(&out, 0xa5, sizeof(out));
    out.before = UINT64_C(0x0123456789abcdef);
    out.value = INT64_C(1234567890123456789);
    out.after = UINT64_C(0xfedcba9876543210);
    return out;
}

static bool guards_unchanged(const guarded_output *out)
{
    return out->before == UINT64_C(0x0123456789abcdef) &&
           out->after == UINT64_C(0xfedcba9876543210);
}

static int test_counter_contracts(void)
{
    /* These identities have certain exact binary64 results, independently of
     * any native/JDK rounded-vector capture. */
    static const struct {
        int64_t count;
        int64_t frequency;
        int64_t expected;
    } exact[] = {{0, 1, 0},
                 {0, INT64_MAX, 0},
                 {1, 1, INT64_C(1000000000)},
                 {-1, 1, -INT64_C(1000000000)},
                 {3, 3, INT64_C(1000000000)},
                 {INT64_MAX, INT64_MAX, INT64_C(1000000000)}};
    static const struct {
        int64_t count;
        int64_t frequency;
    } rejected[] = {{1, 0}, {1, -1}, {1, INT64_MIN}, {INT64_MAX, 1}, {INT64_MIN, 1}};

    for (size_t i = 0; i < sizeof(exact) / sizeof(exact[0]); ++i) {
        guarded_output out = make_output();
        const mc_platform_nanotime_status status =
            mc_platform_nanotime_from_counter(exact[i].count, exact[i].frequency, &out.value);
        CHECK(status == MC_PLATFORM_NANOTIME_OK);
        CHECK(out.value == exact[i].expected);
        CHECK(guards_unchanged(&out));
    }
    for (size_t i = 0; i < sizeof(rejected) / sizeof(rejected[0]); ++i) {
        guarded_output out = make_output();
        unsigned char saved[sizeof(out)];
        memcpy(saved, &out, sizeof(saved));
        const mc_platform_nanotime_status status =
            mc_platform_nanotime_from_counter(rejected[i].count, rejected[i].frequency, &out.value);
        CHECK(status == MC_PLATFORM_NANOTIME_RANGE);
        CHECK(memcmp(saved, &out, sizeof(saved)) == 0);
    }
    const mc_platform_nanotime_status status = mc_platform_nanotime_from_counter(1, 1, NULL);
    CHECK(status == MC_PLATFORM_NANOTIME_INVALID_ARGUMENT);
    CHECK((_mm_getcsr() & 0xffc0u) == 0x1f80u);
    return 0;
}

static int test_timespec_contracts(void)
{
    /* Literal independently exact integer products/sums, with both the source
     * multiplication intermediate and final addition required to fit. */
    static const struct {
        int64_t sec;
        int64_t nsec;
        int64_t expected;
    } exact[] = {{0, 0, 0},
                 {0, INT64_C(999999999), INT64_C(999999999)},
                 {1, 0, INT64_C(1000000000)},
                 {-1, 0, -INT64_C(1000000000)},
                 {-1, INT64_C(999999999), -1},
                 {INT64_C(9223372036), INT64_C(854775807), INT64_MAX},
                 {-INT64_C(9223372036), 0, -INT64_C(9223372036000000000)}};
    static const struct {
        int64_t sec;
        int64_t nsec;
    } rejected[] = {{INT64_C(9223372036), INT64_C(854775808)},
                    {INT64_C(9223372037), 0},
                    {-INT64_C(9223372037), INT64_C(999999999)},
                    {INT64_MAX, 0},
                    {INT64_MIN, 0},
                    {0, -1},
                    {0, INT64_C(1000000000)},
                    {0, INT64_MAX},
                    {0, INT64_MIN}};

    for (size_t i = 0; i < sizeof(exact) / sizeof(exact[0]); ++i) {
        guarded_output out = make_output();
        const mc_platform_nanotime_status status =
            mc_platform_nanotime_from_timespec(exact[i].sec, exact[i].nsec, &out.value);
        CHECK(status == MC_PLATFORM_NANOTIME_OK);
        CHECK(out.value == exact[i].expected);
        CHECK(guards_unchanged(&out));
    }
    for (size_t i = 0; i < sizeof(rejected) / sizeof(rejected[0]); ++i) {
        guarded_output out = make_output();
        unsigned char saved[sizeof(out)];
        memcpy(saved, &out, sizeof(saved));
        const mc_platform_nanotime_status status =
            mc_platform_nanotime_from_timespec(rejected[i].sec, rejected[i].nsec, &out.value);
        CHECK(status == MC_PLATFORM_NANOTIME_RANGE);
        CHECK(memcmp(saved, &out, sizeof(saved)) == 0);
    }
    const mc_platform_nanotime_status status = mc_platform_nanotime_from_timespec(1, 0, NULL);
    CHECK(status == MC_PLATFORM_NANOTIME_INVALID_ARGUMENT);
    return 0;
}

static int test_rejected_arguments(void)
{
    mc_platform_nanotime invalid = {0, 0, false};
    unsigned char saved_clock[sizeof(invalid)];
    guarded_output out = make_output();
    unsigned char saved_out[sizeof(out)];
    memcpy(saved_clock, &invalid, sizeof(saved_clock));
    memcpy(saved_out, &out, sizeof(saved_out));

    mc_platform_nanotime_status status = mc_platform_nanotime_init(NULL);
    CHECK(status == MC_PLATFORM_NANOTIME_INVALID_ARGUMENT);
    status = mc_platform_nanotime_check(NULL);
    CHECK(status == MC_PLATFORM_NANOTIME_INVALID_ARGUMENT);
    status = mc_platform_nanotime_check(&invalid);
    CHECK(status == MC_PLATFORM_NANOTIME_INVALID_ARGUMENT);
    status = mc_platform_nanotime_sample(NULL, &out.value);
    CHECK(status == MC_PLATFORM_NANOTIME_INVALID_ARGUMENT);
    CHECK(memcmp(saved_out, &out, sizeof(saved_out)) == 0);
    status = mc_platform_nanotime_sample(&invalid, &out.value);
    CHECK(status == MC_PLATFORM_NANOTIME_INVALID_ARGUMENT);
    CHECK(memcmp(saved_out, &out, sizeof(saved_out)) == 0);
    status = mc_platform_nanotime_sample(&invalid, NULL);
    CHECK(status == MC_PLATFORM_NANOTIME_INVALID_ARGUMENT);
    CHECK(memcmp(saved_clock, &invalid, sizeof(saved_clock)) == 0);

    invalid.initialized = true;
    memcpy(saved_clock, &invalid, sizeof(saved_clock));
    status = mc_platform_nanotime_check(&invalid);
    CHECK(status == MC_PLATFORM_NANOTIME_RANGE);
    status = mc_platform_nanotime_sample(&invalid, &out.value);
    CHECK(status == MC_PLATFORM_NANOTIME_RANGE);
    CHECK(memcmp(saved_out, &out, sizeof(saved_out)) == 0);
    CHECK(memcmp(saved_clock, &invalid, sizeof(saved_clock)) == 0);
    return 0;
}

static int test_live_clock(mc_platform_nanotime *timer)
{
    guarded_output out = make_output();
    int64_t lower;
    int64_t upper;
    mc_platform_nanotime_status status;

    memset(timer, 0xa5, sizeof(*timer));
    /* Keep the old bool representation valid so an omitted successful flag
     * write causes a defined assertion failure. Other bytes remain poisoned. */
    timer->initialized = false;

#if defined(_WIN32)
    LARGE_INTEGER frequency_before;
    LARGE_INTEGER initial_before;
    LARGE_INTEGER initial_after;
    LARGE_INTEGER frequency_after;
    const BOOL frequency_before_ok = QueryPerformanceFrequency(&frequency_before);
    const BOOL initial_before_ok = QueryPerformanceCounter(&initial_before);
    const mc_platform_nanotime_status init_status = mc_platform_nanotime_init(timer);
    const BOOL initial_after_ok = QueryPerformanceCounter(&initial_after);
    const BOOL frequency_after_ok = QueryPerformanceFrequency(&frequency_after);
    CHECK(frequency_before_ok != 0 && initial_before_ok != 0 && initial_after_ok != 0 &&
          frequency_after_ok != 0);
    CHECK(init_status == MC_PLATFORM_NANOTIME_OK);
    CHECK(timer->initialized);
    CHECK(timer->frequency > 0);
    CHECK(timer->frequency == (int64_t)frequency_before.QuadPart &&
          timer->frequency == (int64_t)frequency_after.QuadPart);
    CHECK((int64_t)initial_before.QuadPart <= timer->initial_sample &&
          timer->initial_sample <= (int64_t)initial_after.QuadPart);
#else
    struct timespec initial_before;
    struct timespec initial_after;
    const int initial_before_status = clock_gettime(CLOCK_MONOTONIC, &initial_before);
    const mc_platform_nanotime_status init_status = mc_platform_nanotime_init(timer);
    const int initial_after_status = clock_gettime(CLOCK_MONOTONIC, &initial_after);
    CHECK(initial_before_status == 0 && initial_after_status == 0);
    CHECK(init_status == MC_PLATFORM_NANOTIME_OK);
    CHECK(timer->initialized);
    CHECK(timer->frequency == INT64_C(1000000000));
    status = mc_platform_nanotime_from_timespec((int64_t)initial_before.tv_sec,
                                                (int64_t)initial_before.tv_nsec, &lower);
    CHECK(status == MC_PLATFORM_NANOTIME_OK);
    status = mc_platform_nanotime_from_timespec((int64_t)initial_after.tv_sec,
                                                (int64_t)initial_after.tv_nsec, &upper);
    CHECK(status == MC_PLATFORM_NANOTIME_OK);
    CHECK(lower <= timer->initial_sample && timer->initial_sample <= upper);
#endif

    unsigned char saved_clock[sizeof(*timer)];
    memcpy(saved_clock, timer, sizeof(saved_clock));
    status = mc_platform_nanotime_check(timer);
    CHECK(status == MC_PLATFORM_NANOTIME_OK);
    CHECK(memcmp(saved_clock, timer, sizeof(saved_clock)) == 0);
    status = mc_platform_nanotime_sample(timer, NULL);
    CHECK(status == MC_PLATFORM_NANOTIME_INVALID_ARGUMENT);
    CHECK(memcmp(saved_clock, timer, sizeof(saved_clock)) == 0);

#if defined(_WIN32)
    LARGE_INTEGER sample_before;
    LARGE_INTEGER sample_after;
    const BOOL sample_before_ok = QueryPerformanceCounter(&sample_before);
    const mc_platform_nanotime_status sample_status =
        mc_platform_nanotime_sample(timer, &out.value);
    const BOOL sample_after_ok = QueryPerformanceCounter(&sample_after);
    CHECK(sample_before_ok != 0 && sample_after_ok != 0);
    CHECK(sample_status == MC_PLATFORM_NANOTIME_OK);
    status = mc_platform_nanotime_from_counter((int64_t)sample_before.QuadPart, timer->frequency,
                                               &lower);
    CHECK(status == MC_PLATFORM_NANOTIME_OK);
    status =
        mc_platform_nanotime_from_counter((int64_t)sample_after.QuadPart, timer->frequency, &upper);
    CHECK(status == MC_PLATFORM_NANOTIME_OK);
#else
    struct timespec sample_before;
    struct timespec sample_after;
    const int sample_before_status = clock_gettime(CLOCK_MONOTONIC, &sample_before);
    const mc_platform_nanotime_status sample_status =
        mc_platform_nanotime_sample(timer, &out.value);
    const int sample_after_status = clock_gettime(CLOCK_MONOTONIC, &sample_after);
    CHECK(sample_before_status == 0 && sample_after_status == 0);
    CHECK(sample_status == MC_PLATFORM_NANOTIME_OK);
    status = mc_platform_nanotime_from_timespec((int64_t)sample_before.tv_sec,
                                                (int64_t)sample_before.tv_nsec, &lower);
    CHECK(status == MC_PLATFORM_NANOTIME_OK);
    status = mc_platform_nanotime_from_timespec((int64_t)sample_after.tv_sec,
                                                (int64_t)sample_after.tv_nsec, &upper);
    CHECK(status == MC_PLATFORM_NANOTIME_OK);
#endif
    CHECK(lower <= out.value && out.value <= upper);
    CHECK(guards_unchanged(&out));
    CHECK(memcmp(saved_clock, timer, sizeof(saved_clock)) == 0);
    CHECK((_mm_getcsr() & 0xffc0u) == 0x1f80u);
    printf("nanotime frequency=%" PRId64 " initial=%" PRId64 " sample=%" PRId64 "\n",
           timer->frequency, timer->initial_sample, out.value);
    return 0;
}

static int test_unsupported_fp(const mc_platform_nanotime *timer)
{
    const unsigned int saved_csr = _mm_getcsr();
    CHECK((saved_csr & 0xffc0u) == 0x1f80u);
    const unsigned int clean_csr = saved_csr & ~0x003fu;
    const unsigned int variants[] = {
        clean_csr | 0x2000u, /* round downward */
        clean_csr | 0x8000u, /* flush to zero */
        clean_csr | 0x0040u, /* denormals are zero */
        clean_csr & ~0x0080u /* unmask invalid-operation exception */
    };
    unsigned char saved_clock[sizeof(*timer)];
    memcpy(saved_clock, timer, sizeof(saved_clock));

    for (size_t i = 0; i < sizeof(variants) / sizeof(variants[0]); ++i) {
        mc_platform_nanotime fresh;
        unsigned char saved_fresh[sizeof(fresh)];
        guarded_output sample_out = make_output();
        guarded_output counter_out = make_output();
        guarded_output integer_out = make_output();
        unsigned char saved_sample[sizeof(sample_out)];
        unsigned char saved_counter[sizeof(counter_out)];
        unsigned int after[5];
        memset(&fresh, 0x5a, sizeof(fresh));
        memcpy(saved_fresh, &fresh, sizeof(saved_fresh));
        memcpy(saved_sample, &sample_out, sizeof(saved_sample));
        memcpy(saved_counter, &counter_out, sizeof(saved_counter));

        /* No CHECK, diagnostic or return is allowed before restoration. Clear
         * sticky flags in the value being installed before unmasking anything. */
        _mm_setcsr(variants[i]);
        const mc_platform_nanotime_status check_status = mc_platform_nanotime_check(timer);
        after[0] = _mm_getcsr();
        const mc_platform_nanotime_status init_status = mc_platform_nanotime_init(&fresh);
        after[1] = _mm_getcsr();
        const mc_platform_nanotime_status sample_status =
            mc_platform_nanotime_sample(timer, &sample_out.value);
        after[2] = _mm_getcsr();
        const mc_platform_nanotime_status counter_status =
            mc_platform_nanotime_from_counter(1, 1, &counter_out.value);
        after[3] = _mm_getcsr();
        const mc_platform_nanotime_status integer_status =
            mc_platform_nanotime_from_timespec(1, 2, &integer_out.value);
        after[4] = _mm_getcsr();
        _mm_setcsr(saved_csr);

        CHECK(_mm_getcsr() == saved_csr);
        CHECK(check_status == MC_PLATFORM_NANOTIME_UNSUPPORTED_FP);
        CHECK(init_status == MC_PLATFORM_NANOTIME_UNSUPPORTED_FP);
        CHECK(sample_status == MC_PLATFORM_NANOTIME_UNSUPPORTED_FP);
        CHECK(counter_status == MC_PLATFORM_NANOTIME_UNSUPPORTED_FP);
        CHECK(integer_status == MC_PLATFORM_NANOTIME_OK);
        CHECK(integer_out.value == INT64_C(1000000002));
        CHECK(guards_unchanged(&integer_out));
        CHECK(memcmp(saved_fresh, &fresh, sizeof(saved_fresh)) == 0);
        CHECK(memcmp(saved_sample, &sample_out, sizeof(saved_sample)) == 0);
        CHECK(memcmp(saved_counter, &counter_out, sizeof(saved_counter)) == 0);
        CHECK(memcmp(saved_clock, timer, sizeof(saved_clock)) == 0);
        for (size_t j = 0; j < sizeof(after) / sizeof(after[0]); ++j) {
            CHECK((after[j] & 0xffc0u) == (variants[i] & 0xffc0u));
        }
    }
    return 0;
}

int main(void)
{
    mc_platform_nanotime timer;
    const unsigned int initial_csr = _mm_getcsr();
    CHECK((initial_csr & 0xffc0u) == 0x1f80u);
    if (test_counter_contracts() != 0 || test_timespec_contracts() != 0 ||
        test_rejected_arguments() != 0 || test_live_clock(&timer) != 0 ||
        test_unsupported_fp(&timer) != 0) {
        return 1;
    }
    CHECK((_mm_getcsr() & 0xffc0u) == (initial_csr & 0xffc0u));
    printf("PASS: %u platform nanotime checks\n", checks);
    return 0;
}
