#ifndef MC_PLATFORM_TIME_NANOTIME_H
#define MC_PLATFORM_TIME_NANOTIME_H

#include <stdbool.h>
#include <stdint.h>

/* Private platform value; successful initialization is followed by immutable
 * use. Windows retains the positive QPC frequency and RAW initial QPC count.
 * Linux retains an initial monotonic nanosecond result and frequency=1e9 as
 * a unit marker, unused by its integer sampling conversion. Neither backend
 * subtracts initial_sample from later samples. No allocation is owned. */
typedef struct mc_platform_nanotime {
    int64_t frequency;
    int64_t initial_sample;
    bool initialized;
} mc_platform_nanotime;

typedef enum mc_platform_nanotime_status {
    MC_PLATFORM_NANOTIME_OK = 0,
    MC_PLATFORM_NANOTIME_INVALID_ARGUMENT,
    MC_PLATFORM_NANOTIME_PLATFORM_FAILURE,
    MC_PLATFORM_NANOTIME_RANGE,
    MC_PLATFORM_NANOTIME_UNSUPPORTED_FP
} mc_platform_nanotime_status;

/* All outputs are published only on success and retain their raw bytes on
 * failure. Writable outputs must be correctly aligned, valid and disjoint from
 * clock storage. Successful clocks remain live and immutable through calls;
 * concurrent sampling may share such a clock with disjoint outputs.
 *
 * INVALID_ARGUMENT means a null argument or false initialized flag.
 * PLATFORM_FAILURE means a failed OS clock query.
 * RANGE means a nonpositive counter frequency, invalid retained frequency,
 * invalid nanosecond field or unrepresentable conversion/intermediate.
 * UNSUPPORTED_FP means the required MXCSR controls are not in effect.
 * These are native policies, not Java exception categories.
 *
 * The compiled profile is Windows/Linux x86-64 with SSE2 and binary64 double.
 * init/check/sample/from_counter require masked FP exceptions, nearest-even
 * rounding, gradual underflow, no excess precision, reassociation or FMA
 * contraction. Caller controls must remain stable throughout a call. Functions
 * inspect but never change FP controls. No wall-clock fallback is provided. */

/* Accepts arbitrary prior bytes in fresh writable storage without reading
 * them. Success initializes all semantic fields. Windows queries frequency,
 * validates it, then queries its initial counter; Linux samples monotonic once.
 * Failure may retain completed OS effects but never writes the output. */
mc_platform_nanotime_status mc_platform_nanotime_init(mc_platform_nanotime *out);

/* No OS query. clock must point to a defined C value (including a valid bool
 * representation), not arbitrary poisoned bytes. A false flag is rejected.
 * This is contract/FP validation, not proof of the value's creation history. */
mc_platform_nanotime_status mc_platform_nanotime_check(const mc_platform_nanotime *clock);

/* Validate before making exactly one fresh selected OS clock query. A failed
 * validation makes no query; a failed query/conversion leaves out unchanged. */
mc_platform_nanotime_status mc_platform_nanotime_sample(const mc_platform_nanotime *clock,
                                                        int64_t *out);

/* Pure conversions used by the production backends and deterministic tests.
 * from_counter requires frequency>0 and the FP environment above: convert
 * count/frequency separately to binary64, divide, multiply by 1e9, then truncate
 * only a finite result in [-2^63,2^63). No origin subtraction or saturation.
 * from_timespec is integer-only and independent of FP controls: require nsec
 * in [0,999999999] and representable sec*1e9 intermediate AND final addition.
 * Neither function samples a clock or reads/modifies any global owner. */
mc_platform_nanotime_status mc_platform_nanotime_from_counter(int64_t count, int64_t frequency,
                                                              int64_t *out);
mc_platform_nanotime_status mc_platform_nanotime_from_timespec(int64_t sec, int64_t nsec,
                                                               int64_t *out);

#endif
