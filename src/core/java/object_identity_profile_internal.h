#ifndef MC_CORE_JAVA_OBJECT_IDENTITY_PROFILE_INTERNAL_H
#define MC_CORE_JAVA_OBJECT_IDENTITY_PROFILE_INTERNAL_H

#include "mc/core/java/random_profile.h"
#include "object_identity_internal.h"

/* Borrow the same admitted main logical Thread for exclusive native Source
 * simulation/render execution. NULL returns NULL; this passive query never
 * initializes, draws or resets anything. The application profile outlives
 * every borrower. It is not an implicit mapping of arbitrary OS workers and
 * does not authorize concurrent use of this logical Thread or object cells. */
mc_java_identity_thread *mc_java_random_profile_main_identity(mc_java_random_profile *profile);

#endif
