#if defined(__linux__)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#elif _POSIX_C_SOURCE < 200809L
#error "monotonic nanotime requires POSIX.1-2008 declarations"
#endif
#endif

#include "nanotime.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>

#if !defined(_WIN32) && !defined(__linux__)
#error "nanotime supports only the selected Windows/Linux profiles"
#endif

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "nanotime requires the selected x86-64/SSE2 profile"
#endif

#if !defined(_MSC_VER) && !defined(__SSE2__)
#error "nanotime requires SSE2 instructions"
#endif

#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "nanotime requires strict floating transformations and finite checks"
#endif

#include <emmintrin.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <time.h>
#endif

_Static_assert(CHAR_BIT == 8, "8-bit bytes required");
_Static_assert(sizeof(double) == sizeof(uint64_t), "64-bit double representation required");
_Static_assert(FLT_RADIX == 2 && DBL_MANT_DIG == 53 && DBL_MIN_EXP == -1021 && DBL_MAX_EXP == 1024,
               "IEEE 754 binary64 required");
_Static_assert(DBL_HAS_SUBNORM == 1, "binary64 subnormal support required");
_Static_assert(FLT_EVAL_METHOD == 0, "binary64 expression evaluation required");

#if defined(_WIN32)
_Static_assert(sizeof(LONGLONG) == sizeof(int64_t), "64-bit Windows counter required");
_Static_assert((LONGLONG)-1 < (LONGLONG)0, "signed Windows counter required");
#else
_Static_assert((time_t)-1 < (time_t)0, "signed Linux time_t required");
_Static_assert(sizeof(time_t) <= sizeof(int64_t), "Linux time_t must fit int64_t");
_Static_assert(sizeof(long) <= sizeof(int64_t), "Linux tv_nsec must fit int64_t");
#endif

#define NANOSECONDS_PER_SECOND INT64_C(1000000000)

static mc_platform_nanotime_status check_fp(void) {
    /* Ignore sticky exception status bits; do not change any controls. */
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        return MC_PLATFORM_NANOTIME_UNSUPPORTED_FP;
    }
    return MC_PLATFORM_NANOTIME_OK;
}

mc_platform_nanotime_status mc_platform_nanotime_from_counter(int64_t count, int64_t frequency,
                                                              int64_t *out) {
    if (out == NULL) {
        return MC_PLATFORM_NANOTIME_INVALID_ARGUMENT;
    }
    if (frequency <= 0) {
        return MC_PLATFORM_NANOTIME_RANGE;
    }
    const mc_platform_nanotime_status status = check_fp();
    if (status != MC_PLATFORM_NANOTIME_OK) {
        return status;
    }
    const double counter_value = (double)count;
    const double frequency_value = (double)frequency;
    const double quotient = counter_value / frequency_value;
    const double nanoseconds = quotient * 1000000000.0;
    if (!isfinite(nanoseconds) || nanoseconds < -0x1p63 || nanoseconds >= 0x1p63) {
        return MC_PLATFORM_NANOTIME_RANGE;
    }
    const int64_t result = (int64_t)nanoseconds;
    *out = result;
    return MC_PLATFORM_NANOTIME_OK;
}

mc_platform_nanotime_status mc_platform_nanotime_from_timespec(int64_t sec, int64_t nsec,
                                                               int64_t *out) {
    if (out == NULL) {
        return MC_PLATFORM_NANOTIME_INVALID_ARGUMENT;
    }
    if (nsec < 0 || nsec >= NANOSECONDS_PER_SECOND) {
        return MC_PLATFORM_NANOTIME_RANGE;
    }
    if (sec < INT64_MIN / NANOSECONDS_PER_SECOND || sec > INT64_MAX / NANOSECONDS_PER_SECOND) {
        return MC_PLATFORM_NANOTIME_RANGE;
    }
    const int64_t whole_seconds = sec * NANOSECONDS_PER_SECOND;
    if (whole_seconds > INT64_MAX - nsec) {
        return MC_PLATFORM_NANOTIME_RANGE;
    }
    const int64_t result = whole_seconds + nsec;
    *out = result;
    return MC_PLATFORM_NANOTIME_OK;
}

#if !defined(_WIN32)
static mc_platform_nanotime_status sample_monotonic(int64_t *out) {
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) {
        return MC_PLATFORM_NANOTIME_PLATFORM_FAILURE;
    }
    /* Signed time_t and long fit int64_t by the profile assertions above. */
    const int64_t seconds = (int64_t)value.tv_sec;
    const int64_t nanoseconds = (int64_t)value.tv_nsec;
    return mc_platform_nanotime_from_timespec(seconds, nanoseconds, out);
}
#endif

mc_platform_nanotime_status mc_platform_nanotime_init(mc_platform_nanotime *out) {
    if (out == NULL) {
        return MC_PLATFORM_NANOTIME_INVALID_ARGUMENT;
    }
    const mc_platform_nanotime_status status = check_fp();
    if (status != MC_PLATFORM_NANOTIME_OK) {
        return status;
    }
    mc_platform_nanotime candidate;
#if defined(_WIN32)
    LARGE_INTEGER frequency;
    if (QueryPerformanceFrequency(&frequency) == 0) {
        return MC_PLATFORM_NANOTIME_PLATFORM_FAILURE;
    }
    const int64_t frequency_value = (int64_t)frequency.QuadPart;
    if (frequency_value <= 0) {
        return MC_PLATFORM_NANOTIME_RANGE;
    }
    LARGE_INTEGER initial;
    if (QueryPerformanceCounter(&initial) == 0) {
        return MC_PLATFORM_NANOTIME_PLATFORM_FAILURE;
    }
    candidate.frequency = frequency_value;
    candidate.initial_sample = (int64_t)initial.QuadPart;
#else
    int64_t initial;
    const mc_platform_nanotime_status sample_status = sample_monotonic(&initial);
    if (sample_status != MC_PLATFORM_NANOTIME_OK) {
        return sample_status;
    }
    candidate.frequency = NANOSECONDS_PER_SECOND;
    candidate.initial_sample = initial;
#endif
    candidate.initialized = true;
    *out = candidate;
    return MC_PLATFORM_NANOTIME_OK;
}

mc_platform_nanotime_status mc_platform_nanotime_check(const mc_platform_nanotime *clock) {
    if (clock == NULL || !clock->initialized) {
        return MC_PLATFORM_NANOTIME_INVALID_ARGUMENT;
    }
#if defined(_WIN32)
    if (clock->frequency <= 0) {
        return MC_PLATFORM_NANOTIME_RANGE;
    }
#else
    if (clock->frequency != NANOSECONDS_PER_SECOND) {
        return MC_PLATFORM_NANOTIME_RANGE;
    }
#endif
    return check_fp();
}

mc_platform_nanotime_status mc_platform_nanotime_sample(const mc_platform_nanotime *clock,
                                                        int64_t *out) {
    if (out == NULL) {
        return MC_PLATFORM_NANOTIME_INVALID_ARGUMENT;
    }
    const mc_platform_nanotime_status status = mc_platform_nanotime_check(clock);
    if (status != MC_PLATFORM_NANOTIME_OK) {
        return status;
    }
#if defined(_WIN32)
    LARGE_INTEGER counter;
    if (QueryPerformanceCounter(&counter) == 0) {
        return MC_PLATFORM_NANOTIME_PLATFORM_FAILURE;
    }
    return mc_platform_nanotime_from_counter((int64_t)counter.QuadPart, clock->frequency, out);
#else
    return sample_monotonic(out);
#endif
}
