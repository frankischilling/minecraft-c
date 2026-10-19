#ifndef MC_WORLD_GENERATION_NORMAL_INTERNAL_H
#define MC_WORLD_GENERATION_NORMAL_INTERNAL_H

#include "mc/core/java/random_profile.h"
#include "mc/world/generation/normal.h"

/* Concrete constructor prerequisite for a real enclosing provider owner.
 * Require fresh zero-initialized generator, initialized same-world manager,
 * immutable trig and a live shared profile; all owners/results are disjoint
 * and exclusive. profile_cause is required. Admission failure preserves both
 * generator and cause and performs no profile operation.
 *
 * Prepare storage/range8, then construct the actual owned cave Random through
 * the profile once BEFORE explicit seeded terrain/noise. The shared numerical
 * tail never erases/reseeds that cave payload. A reached profile result is
 * copied exactly to cause. Failure latches phase FAILED, leaves initialized
 * false and retains completed generator/profile prefixes; neither this entry
 * nor public numerical init can retry until explicit retirement with free.
 * The enclosing constructor must likewise retain its failed World prefix and
 * reject retry. An enclosing owner that has not escaped may explicitly retire
 * its failed unpublished child during cleanup, without undoing profile effects.
 * Profile failure leaves the fresh cave output raw;
 * CAS/clock effects already consumed by the profile are never rolled back.
 *
 * Success selects PROVIDER origin and later caves use existing-owner setSeed
 * semantics. The generator borrows profile only for this call and never frees
 * it. No World reference, original array allocation identity/failure sequence,
 * full provider/World readiness or feature population is implemented here. */
mc_normal_generator_status
mc_normal_generator_construct_provider(mc_normal_generator *generator, int64_t world_seed,
                                       mc_biome_manager *manager, const mc_math_trig *trig,
                                       mc_java_random_profile *profile,
                                       mc_java_random_profile_status *profile_cause);

#endif
