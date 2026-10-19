#ifndef MC_CORE_JAVA_RANDOM_PROFILE_INTERNAL_H
#define MC_CORE_JAVA_RANDOM_PROFILE_INTERNAL_H

#include "mc/core/java/random_profile.h"

#include "../../platform/time/nanotime.h"

#include <stdatomic.h>

typedef enum mc_java_random_holder_phase {
    MC_JAVA_RANDOM_HOLDER_COLD = 0,
    MC_JAVA_RANDOM_HOLDER_INITIALIZING,
    MC_JAVA_RANDOM_HOLDER_READY,
    MC_JAVA_RANDOM_HOLDER_FAILED
} mc_java_random_holder_phase;

/* A separate ordinary Math owner; never overlay a plain mc_java_random with
 * atomic storage. Only its LCG changes after publication. */
typedef struct mc_java_math_random_owner {
    _Atomic(uint64_t) state;
    double next_next_gaussian;
    bool have_next_next_gaussian;
} mc_java_math_random_owner;

/* This is the implementation's actual private layout, not a replay shadow.
 * clock/max_allocation_bytes are immutable after creation. The initializer
 * writes math and allocated_bytes before release-publishing READY; other
 * callers acquire the phase before reading math. Queries/free and direct
 * internal inspection require quiescence, including all waiting callers. */
struct mc_java_random_profile {
    mc_platform_nanotime clock;
    _Atomic(uint64_t) uniquifier;
    atomic_uint holder_phase;
    mc_java_math_random_owner *math;
    size_t allocated_bytes;
    size_t max_allocation_bytes;
};

/* Private production constructor tail, also used for recorded-input replay.
 * out must be nonnull fresh writable storage, disjoint from other live owners.
 * XOR the committed full-width uniquifier and actual signed clock sample, then
 * freshly initialize every Random semantic field. No clock, shared-owner
 * advance, allocation or production entropy-injection interface is provided. */
void mc_java_random_finish_default(uint64_t committed, int64_t sample, mc_java_random *out);

#endif
