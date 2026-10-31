#if defined(__linux__)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#elif _POSIX_C_SOURCE < 200809L
#error "millis tests require POSIX.1-2008 declarations"
#endif
#endif

#include "millis.h"

#include <emmintrin.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
_Static_assert(sizeof(DWORD) == sizeof(uint32_t), "32-bit FILETIME halves required");
#elif defined(__linux__)
#include <time.h>
_Static_assert((time_t)-1 < (time_t)0, "signed Linux time_t required");
_Static_assert(sizeof(time_t) <= sizeof(int64_t), "Linux time_t must fit int64_t");
_Static_assert(sizeof(long) <= sizeof(int64_t), "Linux tv_nsec must fit int64_t");
#else
#error "millis tests require the selected Windows/Linux profile"
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

static guarded_output make_output(void) {
    guarded_output out;
    memset(&out, 0xa5, sizeof(out));
    out.before = UINT64_C(0x0123456789abcdef);
    out.value = INT64_C(1234567890123456789);
    out.after = UINT64_C(0xfedcba9876543210);
    return out;
}

static bool guards_unchanged(const guarded_output *out) {
    return out->before == UINT64_C(0x0123456789abcdef) &&
           out->after == UINT64_C(0xfedcba9876543210);
}

static int test_filetime(void) {
    /* Literal arithmetic witnesses, including signed division before 1970.
     * INT64_MAX ticks minus 116444736000000000 is 9106927300854775807;
     * truncating by 10000 gives 910692730085477, with remainder 5807. */
    static const struct {
        uint64_t ticks;
        int64_t expected;
    } exact[] = {{0, -INT64_C(11644473600000)},
                 {1, -INT64_C(11644473599999)},
                 {UINT64_C(9999), -INT64_C(11644473599999)},
                 {UINT64_C(10000), -INT64_C(11644473599999)},
                 {UINT64_C(116444735990000000), -1000},
                 {UINT64_C(116444735999989999), -1},
                 {UINT64_C(116444735999990000), -1},
                 {UINT64_C(116444735999990001), 0},
                 {UINT64_C(116444735999999999), 0},
                 {UINT64_C(116444736000000000), 0},
                 {UINT64_C(116444736000000001), 0},
                 {UINT64_C(116444736000009999), 0},
                 {UINT64_C(116444736000010000), 1},
                 {UINT64_C(116444736000010001), 1},
                 {UINT64_C(116444736010000000), 1000},
                 {UINT64_C(9223372036854775806), INT64_C(910692730085477)},
                 {UINT64_C(9223372036854775807), INT64_C(910692730085477)}};

    static const uint64_t rejected[] = {UINT64_C(0x8000000000000000), UINT64_C(0x8000000000000001),
                                        UINT64_MAX};
    for (size_t i = 0; i < sizeof(exact) / sizeof(exact[0]); ++i) {
        guarded_output out = make_output();
        CHECK(mc_platform_millis_from_filetime(exact[i].ticks, &out.value) ==
              MC_PLATFORM_MILLIS_OK);
        CHECK(out.value == exact[i].expected);
        CHECK(guards_unchanged(&out));
    }
    for (size_t i = 0; i < sizeof(rejected) / sizeof(rejected[0]); ++i) {
        guarded_output out = make_output();
        unsigned char saved[sizeof(out)];
        memcpy(saved, &out, sizeof(saved));
        CHECK(mc_platform_millis_from_filetime(rejected[i], &out.value) ==
              MC_PLATFORM_MILLIS_RANGE);
        CHECK(memcmp(saved, &out, sizeof(saved)) == 0);
    }
    CHECK(mc_platform_millis_from_filetime(0, NULL) == MC_PLATFORM_MILLIS_INVALID_ARGUMENT);
    CHECK(mc_platform_millis_from_filetime(UINT64_MAX, NULL) ==
          MC_PLATFORM_MILLIS_INVALID_ARGUMENT);
    return 0;
}

static int test_timespec(void) {
    /* Independent decimal literals: seconds*1000 must fit before adding the
     * truncated fraction. At the upper boundary only 807 further ms fit. */
    static const struct {
        int64_t sec, nsec, expected;
    } exact[] = {{0, 0, 0},
                 {0, 1, 0},
                 {0, INT64_C(999999), 0},
                 {0, INT64_C(1000000), 1},
                 {0, INT64_C(1000001), 1},
                 {0, INT64_C(999999999), 999},
                 {1, 0, 1000},
                 {1, INT64_C(999999999), 1999},
                 {-1, 0, -1000},
                 {-1, 1, -1000},
                 {-1, INT64_C(999999), -1000},
                 {-1, INT64_C(1000000), -999},
                 {-1, INT64_C(999999900), -1},
                 {-2, INT64_C(999999999), -1001},
                 {INT64_C(9223372036854775), 0, INT64_C(9223372036854775000)},
                 {INT64_C(9223372036854775), INT64_C(807000000), INT64_MAX},
                 {INT64_C(9223372036854775), INT64_C(807999999), INT64_MAX},
                 {-INT64_C(9223372036854775), 0, -INT64_C(9223372036854775000)},
                 {-INT64_C(9223372036854775), INT64_C(999999999), -INT64_C(9223372036854774001)}};

    static const struct {
        int64_t sec, nsec;
    } rejected[] = {{INT64_C(9223372036854775), INT64_C(808000000)},
                    {INT64_C(9223372036854775), INT64_C(999999999)},
                    {INT64_C(9223372036854776), 0},
                    {-INT64_C(9223372036854776), 0},
                    /* The mathematical final sum would equal INT64_MIN, but its seconds
                     * multiplication is already below INT64_MIN by 192. */
                    {-INT64_C(9223372036854776), INT64_C(192000000)},
                    {-INT64_C(9223372036854776), INT64_C(999999999)},
                    {INT64_MAX, 0},
                    {INT64_MIN, 0},
                    {0, -1},
                    {0, INT64_C(1000000000)},
                    {0, INT64_MAX},
                    {0, INT64_MIN}};

    for (size_t i = 0; i < sizeof(exact) / sizeof(exact[0]); ++i) {
        guarded_output out = make_output();
        CHECK(mc_platform_millis_from_timespec(exact[i].sec, exact[i].nsec, &out.value) ==
              MC_PLATFORM_MILLIS_OK);
        CHECK(out.value == exact[i].expected);
        CHECK(guards_unchanged(&out));
    }
    for (size_t i = 0; i < sizeof(rejected) / sizeof(rejected[0]); ++i) {
        guarded_output out = make_output();
        unsigned char saved[sizeof(out)];
        memcpy(saved, &out, sizeof(saved));
        CHECK(mc_platform_millis_from_timespec(rejected[i].sec, rejected[i].nsec, &out.value) ==
              MC_PLATFORM_MILLIS_RANGE);
        CHECK(memcmp(saved, &out, sizeof(saved)) == 0);
    }
    CHECK(mc_platform_millis_from_timespec(0, 0, NULL) == MC_PLATFORM_MILLIS_INVALID_ARGUMENT);
    CHECK(mc_platform_millis_from_timespec(INT64_MAX, INT64_MIN, NULL) ==
          MC_PLATFORM_MILLIS_INVALID_ARGUMENT);
    return 0;
}

static mc_platform_millis_status read_os_millis(int64_t *out) {
#if defined(_WIN32)
    FILETIME value;
    GetSystemTimeAsFileTime(&value);
    const uint64_t ticks = ((uint64_t)value.dwHighDateTime << 32u) | (uint64_t)value.dwLowDateTime;
    return mc_platform_millis_from_filetime(ticks, out);
#else
    struct timespec value;
    if (clock_gettime(CLOCK_REALTIME, &value) != 0) {
        return MC_PLATFORM_MILLIS_PLATFORM_FAILURE;
    }
    return mc_platform_millis_from_timespec((int64_t)value.tv_sec, (int64_t)value.tv_nsec, out);
#endif
}

static int test_live_clock(void) {
    CHECK(mc_platform_current_time_millis(NULL) == MC_PLATFORM_MILLIS_INVALID_ARGUMENT);
    unsigned int ordered_brackets = 0;
    for (size_t i = 0; i < 4u; ++i) {
        guarded_output out = make_output();
        int64_t before = 0, after = 0;
        const mc_platform_millis_status before_status = read_os_millis(&before);
        const mc_platform_millis_status status = mc_platform_current_time_millis(&out.value);
        const mc_platform_millis_status after_status = read_os_millis(&after);
        CHECK(before_status == MC_PLATFORM_MILLIS_OK && after_status == MC_PLATFORM_MILLIS_OK);
        CHECK(status == MC_PLATFORM_MILLIS_OK);
        CHECK(guards_unchanged(&out));
        if (before <= after) {
            CHECK(before <= out.value && out.value <= after);
            ++ordered_brackets;
        } else {
            /* A detected wall-clock reversal removes this interval's ordering
             * premise. No relation to the preceding interval is asserted. */
            fprintf(stderr, "millis: wall-clock reversal in OS bracket; ordering check skipped\n");
        }
    }
    printf("millis: %u/4 OS clock brackets ordered\n", ordered_brackets);
    CHECK(ordered_brackets > 0);
    return 0;
}

static int test_fp_independence(void) {
    const unsigned int saved_csr = _mm_getcsr();
    /* Mask traps and clear status/rounding/FTZ/DAZ in each installed baseline.
     * Restore the complete original register before CHECK or diagnostics. */
    const unsigned int baseline = (saved_csr & ~0xe07fu) | 0x1f80u;
    const unsigned int variants[] = {
        baseline | 0x8000u, /* FTZ */
        baseline | 0x0040u, /* DAZ */
        baseline | 0x2000u, /* round downward */
        baseline | 0x4000u, /* round upward */
        baseline | 0x6000u, /* round toward zero */
        baseline | 0xe040u  /* FTZ, DAZ and round toward zero */
    };
    for (size_t i = 0; i < sizeof(variants) / sizeof(variants[0]); ++i) {
        guarded_output windows = make_output(), posix = make_output(), sample = make_output();
        guarded_output rejected_windows = make_output(), rejected_posix = make_output();
        unsigned char saved_windows[sizeof(rejected_windows)], saved_posix[sizeof(rejected_posix)];
        unsigned int after[6];
        memcpy(saved_windows, &rejected_windows, sizeof(saved_windows));
        memcpy(saved_posix, &rejected_posix, sizeof(saved_posix));

        _mm_setcsr(variants[i]);
        const mc_platform_millis_status windows_status =
            mc_platform_millis_from_filetime(UINT64_C(116444736000010000), &windows.value);
        after[0] = _mm_getcsr();
        const mc_platform_millis_status posix_status =
            mc_platform_millis_from_timespec(-1, INT64_C(999999900), &posix.value);
        after[1] = _mm_getcsr();
        const mc_platform_millis_status sample_status =
            mc_platform_current_time_millis(&sample.value);
        after[2] = _mm_getcsr();
        const mc_platform_millis_status windows_range =
            mc_platform_millis_from_filetime(UINT64_MAX, &rejected_windows.value);
        after[3] = _mm_getcsr();
        const mc_platform_millis_status posix_range =
            mc_platform_millis_from_timespec(INT64_MAX, 0, &rejected_posix.value);
        after[4] = _mm_getcsr();
        const mc_platform_millis_status null_status = mc_platform_current_time_millis(NULL);
        after[5] = _mm_getcsr();
        _mm_setcsr(saved_csr);

        CHECK(_mm_getcsr() == saved_csr);
        CHECK(windows_status == MC_PLATFORM_MILLIS_OK && windows.value == 1);
        CHECK(posix_status == MC_PLATFORM_MILLIS_OK && posix.value == -1);
        CHECK(sample_status == MC_PLATFORM_MILLIS_OK);
        CHECK(guards_unchanged(&windows) && guards_unchanged(&posix) && guards_unchanged(&sample));
        CHECK(windows_range == MC_PLATFORM_MILLIS_RANGE && posix_range == MC_PLATFORM_MILLIS_RANGE);
        CHECK(memcmp(saved_windows, &rejected_windows, sizeof(saved_windows)) == 0);
        CHECK(memcmp(saved_posix, &rejected_posix, sizeof(saved_posix)) == 0);
        CHECK(null_status == MC_PLATFORM_MILLIS_INVALID_ARGUMENT);
        for (size_t j = 0; j < sizeof(after) / sizeof(after[0]); ++j) {
            CHECK((after[j] & 0xffc0u) == (variants[i] & 0xffc0u));
        }
    }
    return 0;
}

int main(void) {
    const unsigned int initial_csr = _mm_getcsr();
    if (test_filetime() != 0 || test_timespec() != 0 || test_live_clock() != 0 ||
        test_fp_independence() != 0) {
        return 1;
    }
    CHECK((_mm_getcsr() & 0xffc0u) == (initial_csr & 0xffc0u));
    printf("PASS: %u platform millis checks\n", checks);
    return 0;
}
