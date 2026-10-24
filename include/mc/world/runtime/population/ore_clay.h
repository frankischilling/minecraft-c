#ifndef MC_WORLD_RUNTIME_POPULATION_ORE_CLAY_H
#define MC_WORLD_RUNTIME_POPULATION_ORE_CLAY_H

#include "mc/core/java/java.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/mutations.h"

/* Actual fl/dr and ms/ib private int fields. These are retained source fields,
 * not a block palette. a is a captured target ID; b is the full signed size. */
typedef struct mc_population_ore_feature {
    int32_t a, b;
} mc_population_ore_feature;

typedef struct mc_population_clay_feature {
    int32_t a, b;
} mc_population_clay_feature;

typedef struct mc_population_ore_clay_error {
    mc_world_storage_status status;
    const char *operation;
    const char *feature_class, *feature_method;
    unsigned int bytecode;
    mc_world_storage_error cause;
    mc_registry_error registry;
    mc_world_material_error material;
    mc_world_mutation_error mutation;
} mc_population_ore_clay_error;

/* Field constructors, not allocation/Java allocation-failure emulation.
 * Supply fresh zero-initialized, exclusive, disjoint feature storage. The base
 * pg/jm/Object constructors have no retained fields or effects. Ore stores
 * target at6 and size at11. Clay performs actual aX active use5, bn read8,
 * target store11, then size store16. Failure preserves every reached prefix;
 * it does not reset the receiver or undo class/profile/resource effects.
 * Clay holds the actual GAME World during construction; a missing Registry
 * binding fails only when its source active use is reached. No target ID validation, detached
 * palette, Random or trig use.
 */
mc_world_storage_status mc_population_ore_feature_init(mc_population_ore_feature *feature,
                                                       int32_t target, int32_t size);
mc_world_storage_status mc_population_clay_feature_init(mc_population_clay_feature *feature,
                                                        mc_world_runtime *world, int32_t size,
                                                        mc_population_ore_clay_error *error);

/* Inherited pg/jm a(DDD)V consists solely of return0. No scale arithmetic,
 * stored scale, World/Random/class access or floating-environment admission. */
mc_world_storage_status mc_population_ore_feature_scale(mc_population_ore_feature *feature,
                                                        double x, double y, double z);
mc_world_storage_status mc_population_clay_feature_scale(mc_population_clay_feature *feature,
                                                         double x, double y, double z);

/* Complete paired placement bodies, with the supplied actual Random
 * continuation and actual World/Registry/Chunk/quiet-mutation services.
 * Clay material query6 and late shared Material.g9 precede all RNG/numeric
 * work. Nonwater publishes ordinary false with OK; every other completed path
 * publishes true, including zero writes, negative size and size0 geometry.
 * Quiet setter booleans are discarded, while native failures stop, preserve
 * out and retain every earlier RNG/world/class/hook/lighting effect.
 *
 * Actual source fields are loaded at each reached getfield. Candidate getter
 * precedes each late u.bn (ore) or F.bn (clay) access. No future static capture,
 * input coordinate/target/size clamp, provider callback, rollback or retry.
 * All signed integer additions/increments wrap as Java ints. Extreme inclusive
 * loops can wrap and fail to terminate exactly as the source can; no budget or
 * successful escape is invented. Arbitrary signed inputs are represented, but
 * exhaustive numeric/runtime qualification is outstanding, including ordinary
 * normal-provider inputs. See the companion primary map for exact cases.
 *
 * Require live exclusive GAME World/dependencies, an initialized immutable
 * actual class trig table, a live supplied Random when its first draw is
 * reached, and disjoint feature/result/error/dependency storage. Constructing
 * and retained failed World prefixes are admitted; retirement is rejected.
 * Hold the World across the entire operation. Feature/Random retirement or
 * direct mutation during nested synchronous World operations is forbidden
 * except source-permitted field mutation by concrete owners.
 *
 * Captured x86-64 binary32/binary64 profile: FLT_EVAL_METHOD0, nearest/even,
 * gradual underflow, all FP exceptions masked, no contraction/reassociation or
 * fast math. This must remain true throughout the call and nested operations.
 * A runtime MXCSR check occurs at each reached nextFloat/nextDouble, before
 * that draw; null Random is diagnosed at that same source invocation. Volatile
 * single-operation temporaries preserve each source rounding boundary. Source
 * trig and Java floor/casts are used, including zero-divisor NaN geometry.
 */
mc_world_storage_status mc_population_ore_place(const mc_population_ore_feature *feature,
                                                mc_world_runtime *world, mc_java_random *random,
                                                int32_t x, int32_t y, int32_t z, bool *out,
                                                mc_population_ore_clay_error *error);
mc_world_storage_status mc_population_clay_place(const mc_population_clay_feature *feature,
                                                 mc_world_runtime *world, mc_java_random *random,
                                                 int32_t x, int32_t y, int32_t z, bool *out,
                                                 mc_population_ore_clay_error *error);

#endif
