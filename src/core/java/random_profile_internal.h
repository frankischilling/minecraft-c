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

#define MC_JAVA_RANDOM_PROFILE_MAX_CONSTRUCTOR_INPUTS 65536u

/* One declared constructor boundary, not a seed or a desired Random output.
 * The signed nanoTime return is used as supplied, without conversion or
 * inference from a later seed/state. */
typedef struct mc_java_random_constructor_input {
    uint64_t committed_uniquifier;
    int64_t nanotime;
} mc_java_random_constructor_input;

typedef enum mc_java_random_constructor_input_failure {
    MC_JAVA_RANDOM_INPUT_NONE = 0,
    MC_JAVA_RANDOM_INPUT_EXHAUSTED,
    MC_JAVA_RANDOM_INPUT_ORDER_MISMATCH,
    MC_JAVA_RANDOM_INPUT_EXTRA_INPUTS
} mc_java_random_constructor_input_failure;

typedef struct mc_java_random_constructor_inputs_view {
    bool bound;
    size_t count, consumed;
    mc_java_random_constructor_input_failure failure;
} mc_java_random_constructor_inputs_view;

/* This is the implementation's actual private layout, not a replay shadow.
 * clock/max_allocation_bytes are immutable after creation. The initializer
 * writes math and allocated_bytes before release-publishing READY; other
 * callers acquire the phase before reading math. Queries/free and direct
 * internal inspection require quiescence, including all waiting callers.
 * The optional constructor input binding is immutable after publication;
 * its consumed/failure fields are accessed exclusively in bound mode. */
struct mc_java_random_profile {
    mc_platform_nanotime clock;
    _Atomic(uint64_t) uniquifier;
    atomic_uint holder_phase;
    mc_java_math_random_owner *math;
    size_t allocated_bytes;
    size_t max_allocation_bytes;
    atomic_bool constructor_admitted;
    bool constructor_inputs_bound;
    const mc_java_random_constructor_input *constructor_inputs;
    size_t constructor_input_count, constructor_input_consumed;
    mc_java_random_constructor_input_failure constructor_input_failure;
};

/* Private observation harness only. Bind ONCE to a fresh untouched profile:
 * initial uniquifier, no admitted default constructor and a cold Math holder.
 * Validate every full-width committed U against the complete multiplication
 * progression before publishing any binding. No live U or Random is assigned.
 * An empty bound tape remains distinct from the normal unbound clock route.
 * Invalid shape/progression/freshness returns INVALID_ARGUMENT; excessive count
 * returns LIMIT. Failure preserves every profile field and the borrowed input.
 *
 * Keep the immutable sequence alive through profile retirement. Its storage is
 * borrowed, bounded by MAX_CONSTRUCTOR_INPUTS and not charged to profile-owned
 * allocations. Binding, bound operations, inspection and completion require
 * exclusive/quiescent access, with no concurrent ordinary or Math calls.
 * Sequence and output views must be disjoint from profile storage. The unbound
 * public route retains its normal concurrent CAS/real-clock semantics.
 * This supplies no callbacks, seed injection, JVM clock/OS equivalence or
 * assertion that declared inputs were independently observed. */
mc_java_random_profile_status mc_java_random_profile_bind_constructor_inputs(
    mc_java_random_profile *profile, const mc_java_random_constructor_input *inputs, size_t count);
mc_java_random_profile_status
mc_java_random_profile_constructor_inputs_get(const mc_java_random_profile *profile,
                                              mc_java_random_constructor_inputs_view *out);

/* Terminal tape-completeness assertion, not overall operation success. An
 * unbound profile returns INVALID_ARGUMENT. Unused inputs latch EXTRA_INPUTS;
 * exhaustion/order failures also stay latched. These return RANGE as a native
 * harness policy, not an original clock exception. Later admitted constructors
 * still commit their real U CAS before rejecting a latched failure. No retry,
 * implicit unbind, resampling or normalization repairs a failed observation. */
mc_java_random_profile_status
mc_java_random_profile_constructor_inputs_complete(mc_java_random_profile *profile);

/* Private production constructor tail, also used for recorded-input replay.
 * out must be nonnull fresh writable storage, disjoint from other live owners.
 * XOR the committed full-width uniquifier and actual signed clock sample, then
 * freshly initialize every Random semantic field. No clock, shared-owner
 * advance or allocation is performed by this tail. */
void mc_java_random_finish_default(uint64_t committed, int64_t sample, mc_java_random *out);

#endif
