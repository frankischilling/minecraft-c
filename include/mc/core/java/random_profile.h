#ifndef MC_CORE_JAVA_RANDOM_PROFILE_H
#define MC_CORE_JAVA_RANDOM_PROFILE_H

#include "mc/core/java/java.h"

#include <stddef.h>

/* One noncopyable application/library-class owner, shared above registries and
 * worlds. A fresh profile starts the captured Random class's uniquifier; it
 * cannot attach to an unknown partially executed JVM/class history. */
typedef struct mc_java_random_profile mc_java_random_profile;

typedef enum mc_java_random_profile_status {
    MC_JAVA_RANDOM_PROFILE_OK = 0,
    MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT,
    MC_JAVA_RANDOM_PROFILE_LIMIT,
    MC_JAVA_RANDOM_PROFILE_ALLOCATION_FAILED,
    MC_JAVA_RANDOM_PROFILE_PLATFORM_FAILURE,
    MC_JAVA_RANDOM_PROFILE_RANGE,
    MC_JAVA_RANDOM_PROFILE_UNSUPPORTED_FP,
    MC_JAVA_RANDOM_PROFILE_HOLDER_FAILED
} mc_java_random_profile_status;

/* Allocate one profile and initialize its real platform clock before publishing
 * it. max_allocation_bytes covers this owner and its eventual separate Math
 * Random, using actual native sizes. No constructor advance or Math allocation
 * occurs here. out is unchanged on failure. Native clock/resource failures are
 * distinct policies, not Java exception categories or a full JVM bootstrap. */
mc_java_random_profile_status mc_java_random_profile_create(size_t max_allocation_bytes,
                                                            mc_java_random_profile **out);

/* Quiescent queries/destruction only: no concurrent operation or dependent use
 * may overlap either call. A null query returns zero; free(NULL) does nothing.
 * Freeing a world or ordinary Random must not reset/free its shared profile. */
size_t mc_java_random_profile_allocation_bytes(const mc_java_random_profile *profile);
void mc_java_random_profile_free(mc_java_random_profile *profile);

/* Construct a fresh ordinary Random in caller-owned storage. The normal public
 * route consumes the next full-width shared uniquifier by successful strong
 * CAS BEFORE one actual native nanotime sample. The sample and committed value
 * are XORed, then fresh seed initializes the LCG and +0/false Gaussian cache.
 * No caller stream is drawn.
 * Concurrent calls may use one live profile and disjoint fresh output owners;
 * no whole-constructor lock orders their clock samples. On failure out remains
 * unchanged, while any committed uniquifier/clock prefix remains consumed.
 * Explicit seed/reseed use the existing core functions without this profile. */
mc_java_random_profile_status mc_java_random_construct_default(mc_java_random_profile *profile,
                                                               mc_java_random *out);

/* The first admitted call lazily allocates and constructs a distinct retained
 * Math Random. Other first callers wait for that initialization; allocation or
 * clock failure latches native holder failure, preserving consumed prefixes.
 * Later calls return HOLDER_FAILED without retry. Argument/FP rejection before
 * initialization does not latch failure. Concurrent successful calls use atomic
 * LCG steps: nextDouble's 26-bit and 27-bit draws can interleave with other calls.
 * No entire-nextDouble lock, Gaussian draw or access to an ordinary owner occurs.
 * Output is unchanged on failure. This covers the concrete ordinary holder path
 * without application callbacks; it is not a general Java class-init engine. */
mc_java_random_profile_status mc_java_math_random(mc_java_random_profile *profile, double *out);

/* All live operations require the captured x86-64 strict FP environment from
 * java.h, stable throughout each call. Outputs must be valid, aligned and
 * disjoint from profile storage and concurrently used outputs/ordinary owners.
 * The profile must outlive all operations and users. Creation/destruction and
 * allocation queries require external lifecycle synchronization. These calls
 * are not asynchronous-signal-safe. This public interface exposes no clock/seed
 * injection hook. A separate private observation-harness binding can supply
 * declared constructor-clock inputs under exclusive access; it does not change
 * ordinary unbound operation or claim JVM/OS clock equivalence. */

#endif
