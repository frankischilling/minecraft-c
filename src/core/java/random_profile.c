#include "random_profile_internal.h"

#include <float.h>
#include <limits.h>
#include <stdlib.h>
#include <threads.h>

#if defined(__STDC_NO_ATOMICS__) && __STDC_NO_ATOMICS__
#error "the Random profile requires C17 atomics"
#endif

#if defined(__STDC_NO_THREADS__) && __STDC_NO_THREADS__
#error "the Random profile requires C17 thrd_yield"
#endif

#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "the Random profile requires strict floating transformations"
#endif

_Static_assert(CHAR_BIT == 8, "8-bit bytes required");
_Static_assert(sizeof(double) == sizeof(uint64_t), "64-bit double representation required");
_Static_assert(FLT_RADIX == 2 && DBL_MANT_DIG == 53 && DBL_MIN_EXP == -1021 && DBL_MAX_EXP == 1024,
               "IEEE 754 binary64 required");
_Static_assert(DBL_HAS_SUBNORM == 1, "binary64 subnormal support required");
_Static_assert(FLT_EVAL_METHOD == 0, "binary64 expression evaluation required");
_Static_assert(_Alignof(_Atomic(uint64_t)) <= _Alignof(long double),
               "atomic uint64 storage must have fundamental alignment");
_Static_assert(_Alignof(atomic_uint) <= _Alignof(long double),
               "atomic phase storage must have fundamental alignment");
_Static_assert(_Alignof(mc_java_random_profile) <= _Alignof(long double),
               "profile must have malloc-supported fundamental alignment");
_Static_assert(_Alignof(mc_java_math_random_owner) <= _Alignof(long double),
               "Math owner must have malloc-supported fundamental alignment");

#define INITIAL_UNIQUIFIER UINT64_C(8682522807148012)
#define UNIQUIFIER_MULTIPLIER UINT64_C(1181783497276652981)
#define LCG_MULTIPLIER UINT64_C(0x5deece66d)
#define LCG_MASK UINT64_C(0xffffffffffff)

static mc_java_random_profile_status nanotime_status(mc_platform_nanotime_status status)
{
    switch (status) {
        case MC_PLATFORM_NANOTIME_OK:
            return MC_JAVA_RANDOM_PROFILE_OK;
        case MC_PLATFORM_NANOTIME_INVALID_ARGUMENT:
            return MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT;
        case MC_PLATFORM_NANOTIME_PLATFORM_FAILURE:
            return MC_JAVA_RANDOM_PROFILE_PLATFORM_FAILURE;
        case MC_PLATFORM_NANOTIME_RANGE:
            return MC_JAVA_RANDOM_PROFILE_RANGE;
        case MC_PLATFORM_NANOTIME_UNSUPPORTED_FP:
            return MC_JAVA_RANDOM_PROFILE_UNSUPPORTED_FP;
    }
    return MC_JAVA_RANDOM_PROFILE_PLATFORM_FAILURE;
}

void mc_java_random_finish_default(uint64_t committed, int64_t sample, mc_java_random *out)
{
    const uint64_t bits = committed ^ (uint64_t)sample;
    /* Keep every signed conversion representable, including the sign-bit-only
     * value. An out-of-range unsigned-to-signed cast is implementation-defined. */
    const int64_t seed = bits <= INT64_MAX ? (int64_t)bits : -1 - (int64_t)(UINT64_MAX - bits);
    mc_java_random_seed(out, seed);
}

static uint64_t advance_uniquifier(mc_java_random_profile *profile)
{
    for (;;) {
        const uint64_t current = atomic_load_explicit(&profile->uniquifier, memory_order_seq_cst);
        const uint64_t next = current * UNIQUIFIER_MULTIPLIER;
        uint64_t expected = current;
        if (atomic_compare_exchange_strong_explicit(&profile->uniquifier, &expected, next,
                                                    memory_order_seq_cst, memory_order_seq_cst)) {
            return next;
        }
        /* Java retries from get(), not from a failed CAS's returned operand. */
    }
}

mc_java_random_profile_status mc_java_random_profile_create(size_t max_allocation_bytes,
                                                            mc_java_random_profile **out)
{
    if (out == NULL) {
        return MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT;
    }
    if (max_allocation_bytes < sizeof(mc_java_random_profile)) {
        return MC_JAVA_RANDOM_PROFILE_LIMIT;
    }
    mc_java_random_profile *profile = malloc(sizeof(*profile));
    if (profile == NULL) {
        return MC_JAVA_RANDOM_PROFILE_ALLOCATION_FAILED;
    }
    const mc_java_random_profile_status status =
        nanotime_status(mc_platform_nanotime_init(&profile->clock));
    if (status != MC_JAVA_RANDOM_PROFILE_OK) {
        free(profile);
        return status;
    }
    atomic_init(&profile->uniquifier, INITIAL_UNIQUIFIER);
    atomic_init(&profile->holder_phase, MC_JAVA_RANDOM_HOLDER_COLD);
    profile->math = NULL;
    profile->allocated_bytes = sizeof(*profile);
    profile->max_allocation_bytes = max_allocation_bytes;
    *out = profile;
    return MC_JAVA_RANDOM_PROFILE_OK;
}

size_t mc_java_random_profile_allocation_bytes(const mc_java_random_profile *profile)
{
    return profile == NULL ? 0 : profile->allocated_bytes;
}

void mc_java_random_profile_free(mc_java_random_profile *profile)
{
    if (profile != NULL) {
        free(profile->math);
        free(profile);
    }
}

mc_java_random_profile_status mc_java_random_construct_default(mc_java_random_profile *profile,
                                                               mc_java_random *out)
{
    if (profile == NULL || out == NULL) {
        return MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT;
    }
    const mc_java_random_profile_status check_status =
        nanotime_status(mc_platform_nanotime_check(&profile->clock));
    if (check_status != MC_JAVA_RANDOM_PROFILE_OK) {
        return check_status;
    }
    const uint64_t committed = advance_uniquifier(profile);
    int64_t sample;
    const mc_java_random_profile_status sample_status =
        nanotime_status(mc_platform_nanotime_sample(&profile->clock, &sample));
    if (sample_status != MC_JAVA_RANDOM_PROFILE_OK) {
        return sample_status;
    }
    mc_java_random_finish_default(committed, sample, out);
    return MC_JAVA_RANDOM_PROFILE_OK;
}

static mc_java_random_profile_status initialize_math(mc_java_random_profile *profile)
{
    /* Only the elected initializer enters. No quiescent allocation query or
     * destruction may overlap this allocation, construction or publication. */
    if (profile->allocated_bytes > profile->max_allocation_bytes ||
        sizeof(mc_java_math_random_owner) >
            profile->max_allocation_bytes - profile->allocated_bytes) {
        atomic_store_explicit(&profile->holder_phase, MC_JAVA_RANDOM_HOLDER_FAILED,
                              memory_order_release);
        return MC_JAVA_RANDOM_PROFILE_LIMIT;
    }
    mc_java_math_random_owner *math = malloc(sizeof(*math));
    if (math == NULL) {
        atomic_store_explicit(&profile->holder_phase, MC_JAVA_RANDOM_HOLDER_FAILED,
                              memory_order_release);
        return MC_JAVA_RANDOM_PROFILE_ALLOCATION_FAILED;
    }
    mc_java_random initial;
    const mc_java_random_profile_status status =
        mc_java_random_construct_default(profile, &initial);
    if (status != MC_JAVA_RANDOM_PROFILE_OK) {
        free(math);
        atomic_store_explicit(&profile->holder_phase, MC_JAVA_RANDOM_HOLDER_FAILED,
                              memory_order_release);
        return status;
    }
    atomic_init(&math->state, initial.state);
    math->next_next_gaussian = initial.next_next_gaussian;
    math->have_next_next_gaussian = initial.have_next_next_gaussian;
    profile->math = math;
    /* The subtraction check above proves this addition fits the budget and
     * size_t. Both actual owned allocations are now retained. */
    profile->allocated_bytes += sizeof(*math);
    atomic_store_explicit(&profile->holder_phase, MC_JAVA_RANDOM_HOLDER_READY,
                          memory_order_release);
    return MC_JAVA_RANDOM_PROFILE_OK;
}

static uint32_t math_next_bits(mc_java_math_random_owner *math, unsigned int bits)
{
    for (;;) {
        const uint64_t current = atomic_load_explicit(&math->state, memory_order_seq_cst);
        const uint64_t next = (current * LCG_MULTIPLIER + UINT64_C(11)) & LCG_MASK;
        uint64_t expected = current;
        if (atomic_compare_exchange_strong_explicit(&math->state, &expected, next,
                                                    memory_order_seq_cst, memory_order_seq_cst)) {
            return (uint32_t)(next >> (48u - bits));
        }
    }
}

mc_java_random_profile_status mc_java_math_random(mc_java_random_profile *profile, double *out)
{
    if (profile == NULL || out == NULL) {
        return MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT;
    }
    const mc_java_random_profile_status check_status =
        nanotime_status(mc_platform_nanotime_check(&profile->clock));
    if (check_status != MC_JAVA_RANDOM_PROFILE_OK) {
        return check_status;
    }
    for (;;) {
        const unsigned int phase =
            atomic_load_explicit(&profile->holder_phase, memory_order_acquire);
        if (phase == MC_JAVA_RANDOM_HOLDER_READY) {
            break;
        }
        if (phase == MC_JAVA_RANDOM_HOLDER_FAILED) {
            return MC_JAVA_RANDOM_PROFILE_HOLDER_FAILED;
        }
        if (phase == MC_JAVA_RANDOM_HOLDER_COLD) {
            unsigned int expected = MC_JAVA_RANDOM_HOLDER_COLD;
            if (atomic_compare_exchange_strong_explicit(
                    &profile->holder_phase, &expected, MC_JAVA_RANDOM_HOLDER_INITIALIZING,
                    memory_order_acq_rel, memory_order_acquire)) {
                const mc_java_random_profile_status status = initialize_math(profile);
                if (status != MC_JAVA_RANDOM_PROFILE_OK) {
                    return status;
                }
            }
        } else {
            /* The concrete initializer has no application callbacks or
             * same-thread recursion. Preserve waiting without an arbitrary cap. */
            thrd_yield();
        }
    }
    mc_java_math_random_owner *math = profile->math;
    const uint64_t high = math_next_bits(math, 26);
    const uint64_t low = math_next_bits(math, 27);
    const double result = (double)((high << 27) + low) * 0x1p-53;
    *out = result;
    return MC_JAVA_RANDOM_PROFILE_OK;
}
