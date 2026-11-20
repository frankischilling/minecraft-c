#ifndef MC_PLATFORM_TIME_MILLIS_H
#define MC_PLATFORM_TIME_MILLIS_H

#include <stdint.h>

typedef enum mc_platform_millis_status {
    MC_PLATFORM_MILLIS_OK = 0,
    MC_PLATFORM_MILLIS_INVALID_ARGUMENT,
    MC_PLATFORM_MILLIS_PLATFORM_FAILURE,
    MC_PLATFORM_MILLIS_RANGE
} mc_platform_millis_status;

/* Private Windows/Linux x86-64 wall clock: milliseconds since the Unix epoch.
 * A nonnull valid aligned output receives exactly one selected OS query's
 * converted result. NULL rejects before querying. Failure preserves output.
 * Windows uses GetSystemTimeAsFileTime; Linux uses CLOCK_REALTIME.
 * This clock may move backward. No monotonic fallback, clamping, allocation,
 * retained clock owner, entropy/profile operation or initialization occurs.
 * Integer-only operations neither require nor change FP controls.
 * All writable outputs must be disjoint from other live input/owner storage. */
mc_platform_millis_status mc_platform_current_time_millis(int64_t *out);

/* Pure production conversions; neither samples a clock nor injects a sample
 * into a World constructor. Both preserve output on failure and reject NULL.
 * FILETIME ticks must fit nonnegative int64: subtract 116444736000000000, then
 * divide by10000 with signed truncation toward zero. Reject larger inputs.
 * Timespec requires nsec in[0,999999999], representable sec*1000 intermediate
 * and final addition of nsec/1000000. A potentially rescuing final addition
 * does not admit an overflowing intermediate. RANGE/PLATFORM_FAILURE are
 * explicit native failure policies rather than JVM exception categories. */
mc_platform_millis_status mc_platform_millis_from_filetime(uint64_t ticks, int64_t *out);
mc_platform_millis_status mc_platform_millis_from_timespec(int64_t sec, int64_t nsec, int64_t *out);

#endif
