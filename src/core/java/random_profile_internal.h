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

/* These are native observation labels, not inferred Java caller classes. */
typedef enum mc_java_random_constructor_kind {
    MC_JAVA_RANDOM_CONSTRUCTOR_ORDINARY = 0,
    MC_JAVA_RANDOM_CONSTRUCTOR_MATH
} mc_java_random_constructor_kind;

typedef enum mc_java_random_constructor_record_failure {
    MC_JAVA_RANDOM_RECORD_NONE = 0,
    MC_JAVA_RANDOM_RECORD_OVERFLOW = 1,
    MC_JAVA_RANDOM_RECORD_CONCURRENT = 2
} mc_java_random_constructor_record_failure;

typedef struct mc_java_random_constructor_record {
    uint64_t committed_uniquifier;
    int64_t nanotime; /* Valid only when sample_returned is true. */
    mc_java_random_profile_status sample_status;
    mc_java_random_constructor_kind kind;
    /* Value identities at the actual call, never owned/dereferenceable borrows.
     * Math uses an actual temporary Random tail destination, then copies into
     * a separately allocated atomic owner. Both identities are retained here. */
    uintptr_t destination, math_destination;
    bool sample_returned, tail_returned;
} mc_java_random_constructor_record;

typedef struct mc_java_random_constructor_records_view {
    bool bound, active;
    size_t capacity, recorded;
    unsigned int failures;      /* Bitwise MC_JAVA_RANDOM_RECORD_* observer failures. */
    unsigned int first_failure; /* First committed observer failure; never replaced. */
} mc_java_random_constructor_records_view;

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
    bool constructor_records_bound;
    mc_java_random_constructor_record *constructor_records;
    size_t constructor_record_capacity;
    _Atomic(size_t) constructor_record_count;
    atomic_uint constructor_record_failures;
    atomic_uint constructor_record_first_failure;
    atomic_bool constructor_record_active;
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

/* Callback-free authentic observation, mutually exclusive with strict replay.
 * Bind ONCE to the same fresh state required by bind_constructor_inputs. The
 * caller precharges a valid writable array of capacity actual typed records,
 * keeps it live/disjoint from profile, Random outputs and all other users until
 * profile retirement, and inspects records only under external quiescence.
 * Zero capacity/NULL is a valid explicit recorder that overflows on its first
 * committed constructor; unused output capacity is not unused replay input.
 * The same MAX_CONSTRUCTOR_INPUTS bounds capacity. No array element is touched
 * by binding, and every rejection leaves profile/array unchanged.
 *
 * Every admitted default constructor still commits its real U, samples its
 * actual native clock exactly once, then uses the unchanged real tail. A row
 * copies that returned sample before the tail, or marks the actual failed
 * sample with nanotime invalid. tail_returned is stored only after that actual
 * tail returns. A NULL/FP pre-admission failure commits no U and creates no row.
 * Ordinary and actual Math-holder construction share that same entered body.
 * No new source getter/callback, desired seed, inferred clock or clock override
 * is supplied. Existing already-ready Math draws are not constructor records.
 *
 * A nonblocking observer-only gate detects overlapping default constructors.
 * The losing call latches CONCURRENT and continues its unchanged CAS/clock/tail
 * without a record; no call waits, retries admission or serializes Source work.
 * Capacity exhaustion latches OVERFLOW without changing constructor status or
 * output. Either failure invalidates the complete observation, even if every
 * Source call succeeds. Recorded count never exceeds capacity; after failure
 * it is not a count of all committed constructors. Normal unbound operation
 * touches no recorder atomics and keeps its concurrent CAS/clock semantics.
 *
 * Bind/get/complete/free require external lifecycle quiescence. The atomic
 * metadata prevents recorder writers from racing each other; it grants no
 * right to read a live row, overwrite borrowed storage or retire active users.
 * Complete checks observer integrity, not replay completeness or Source success;
 * it returns RANGE for observer failure and INVALID_ARGUMENT if not bound.
 * No unbind, prefix escape, implicit reset, failure repair or allocation exists.
 */
mc_java_random_profile_status mc_java_random_profile_bind_constructor_records(
    mc_java_random_profile *, mc_java_random_constructor_record *, size_t capacity);
mc_java_random_profile_status
mc_java_random_profile_constructor_records_get(const mc_java_random_profile *,
                                               mc_java_random_constructor_records_view *out);
mc_java_random_profile_status
mc_java_random_profile_constructor_records_complete(mc_java_random_profile *);

/* Private production constructor tail, also used for recorded-input replay.
 * out must be nonnull fresh writable storage, disjoint from other live owners.
 * XOR the committed full-width uniquifier and actual signed clock sample, then
 * freshly initialize every Random semantic field. No clock, shared-owner
 * advance or allocation is performed by this tail. */
void mc_java_random_finish_default(uint64_t committed, int64_t sample, mc_java_random *out);

#endif
