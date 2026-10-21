#if defined(__linux__)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#elif _POSIX_C_SOURCE < 200809L
#error "wall time requires POSIX.1-2008 declarations"
#endif
#endif

#include "millis.h"

#include <limits.h>
#include <stddef.h>

#if !defined(_WIN32) && !defined(__linux__)
#error "wall time supports only Windows/Linux"
#endif
#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "wall time supports only the selected x86-64 profile"
#endif

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
_Static_assert(sizeof(DWORD) == sizeof(uint32_t), "32-bit FILETIME halves required");
#else
#include <time.h>
_Static_assert((time_t)-1 < (time_t)0, "signed Linux time_t required");
_Static_assert(sizeof(time_t) <= sizeof(int64_t), "Linux time_t must fit int64_t");
_Static_assert(sizeof(long) <= sizeof(int64_t), "Linux nanoseconds must fit int64_t");
#endif
_Static_assert(CHAR_BIT == 8, "8-bit bytes required");

#define FILETIME_UNIX_EPOCH INT64_C(116444736000000000)
#define MILLIS_PER_SECOND INT64_C(1000)
#define NANOS_PER_MILLI INT64_C(1000000)

mc_platform_millis_status mc_platform_millis_from_filetime(uint64_t ticks, int64_t *out) {
    if (out == NULL)
        return MC_PLATFORM_MILLIS_INVALID_ARGUMENT;
    if (ticks > (uint64_t)INT64_MAX)
        return MC_PLATFORM_MILLIS_RANGE;
    /* The admitted nonnegative input makes this subtraction representable. */
    const int64_t since_epoch = (int64_t)ticks - FILETIME_UNIX_EPOCH;
    *out = since_epoch / INT64_C(10000);
    return MC_PLATFORM_MILLIS_OK;
}

mc_platform_millis_status mc_platform_millis_from_timespec(int64_t sec, int64_t nsec,
                                                           int64_t *out) {
    if (out == NULL)
        return MC_PLATFORM_MILLIS_INVALID_ARGUMENT;
    if (nsec < 0 || nsec >= INT64_C(1000000000) || sec < INT64_MIN / MILLIS_PER_SECOND ||
        sec > INT64_MAX / MILLIS_PER_SECOND)
        return MC_PLATFORM_MILLIS_RANGE;
    const int64_t whole = sec * MILLIS_PER_SECOND;
    const int64_t fraction = nsec / NANOS_PER_MILLI;
    if (whole > INT64_MAX - fraction)
        return MC_PLATFORM_MILLIS_RANGE;
    *out = whole + fraction;
    return MC_PLATFORM_MILLIS_OK;
}

mc_platform_millis_status mc_platform_current_time_millis(int64_t *out) {
    if (out == NULL)
        return MC_PLATFORM_MILLIS_INVALID_ARGUMENT;
#if defined(_WIN32)
    FILETIME value;
    GetSystemTimeAsFileTime(&value);
    const uint64_t ticks = ((uint64_t)value.dwHighDateTime << 32u) | (uint64_t)value.dwLowDateTime;
    return mc_platform_millis_from_filetime(ticks, out);
#else
    struct timespec value;
    if (clock_gettime(CLOCK_REALTIME, &value) != 0)
        return MC_PLATFORM_MILLIS_PLATFORM_FAILURE;
    return mc_platform_millis_from_timespec((int64_t)value.tv_sec, (int64_t)value.tv_nsec, out);
#endif
}
