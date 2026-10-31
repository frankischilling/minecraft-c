#ifndef MC_WORLD_RUNTIME_POPULATION_LAKE_SPRING_H
#define MC_WORLD_RUNTIME_POPULATION_LAKE_SPRING_H

#include "mc/core/java/java.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/mutations.h"
#include "mc/world/runtime/queries.h"

/* Actual dj/cd and xo/pc retained private field a:I. No ID palette. */
typedef struct mc_population_lake_feature {
    int32_t a;
} mc_population_lake_feature;

typedef struct mc_population_spring_feature {
    int32_t a;
} mc_population_spring_feature;

typedef struct mc_population_lake_spring_error {
    mc_world_storage_status status;
    const char *operation;
    const char *feature_class, *feature_method;
    unsigned int bytecode;
    /* Concrete local fd.d/dj.f(III)Z body invokes actual World.a at4. */
    unsigned int air_bytecode;
    mc_world_storage_error cause;
    mc_registry_error registry;
    mc_world_material_error material;
    mc_world_query_error query;
    mc_world_mutation_error mutation;
} mc_population_lake_spring_error;

/* Actual field constructors: empty pg/jm/Object base call1, captured full
 * signed target store6, return9. Supply fresh exclusive native field storage;
 * these functions do not emulate Java allocation identity/failure. No Block
 * initialization, target validation, Random, table or World operation occurs. */
mc_world_storage_status mc_population_lake_feature_init(mc_population_lake_feature *feature,
                                                        int32_t target);
mc_world_storage_status mc_population_spring_feature_init(mc_population_spring_feature *feature,
                                                          int32_t target);

/* Inherited pg/jm a(DDD)V is literal return0. No retained scale, arithmetic,
 * class use or floating-environment test; live receiver required. */
mc_world_storage_status mc_population_lake_feature_scale(mc_population_lake_feature *feature,
                                                         double x, double y, double z);
mc_world_storage_status mc_population_spring_feature_scale(mc_population_spring_feature *feature,
                                                           double x, double y, double z);

/* Complete source placement bodies through real current World/Registry/
 * supplied JavaRandom owners. Require live exclusive GAME owners/dependencies
 * and disjoint feature/result/error storage. Constructing/failed prefixes are
 * admitted; retirement and COMPOSITION are refused. Hold the actual World
 * across all acquisition, active use, writes, ticks and synchronous reentry.
 * Feature fields stay late at each reached getfield; all world-coordinate int
 * arithmetic wraps as Java. No source work clamp, retry or rollback is added.
 *
 * Lake descends using actual air queries before allocating its zeroed2048-cell
 * local mask at35; native allocation failure preserves the earlier prefix.
 * It consumes the supplied actual Random in source order, validates every
 * boundary before its first write, performs quiet fill/grass/lava-rim writes,
 * and uses actual shared Sky saved light and late Block/material identities.
 * Ordinary validation rejection publishes false with OK. Full completion
 * publishes true even when setters returned ordinary false or no bytes changed.
 * Mask storage is private temporary ownership, released on every return.
 *
 * Spring performs all source stone and air queries, then conditionally invokes
 * the actual NOTIFYING setter264 and ignores its ordinary bool. Only after that
 * call returns normally does it set actual World.a=true270, capture actual m273,
 * reload target277, index280 and dispatch selected Block.tick288 with the
 * supplied Random. Successful tick return alone reaches World.a=false293.
 * Failure does not restore the previous flag or execute that later false store.
 * Nested schedules still use current World.r and current World.a as their own
 * concrete source body requires; no Random source is silently exchanged.
 * Admission tests return ordinary false. Every other completed spring path
 * returns true, including a condition miss and an ignored setter false.
 *
 * Native failure preserves out and all reached world/class/Chunk/hook/queue/
 * Random/flag effects. Existing lower dispatch may explicitly reject genuine
 * unsupported concrete WorldAccess receivers, ticks, drops or hooks; they remain unfinished
 * descendants rather than ordinary false or accepted feature parity. See the
 * primary map for exact prefixes and remaining qualification.
 *
 * Lake double geometry requires captured x86-64 binary64 nearest/even,
 * gradual underflow, masked exceptions, FLT_EVAL_METHOD0, no fast math or
 * contraction/reassociation. Keep that profile through nested calls. The first
 * lake FP admission is at nextDouble59, AFTER nextInt41 and mask allocation;
 * each later nextDouble is guarded at its reached call. Spring has no earlier
 * top-level floating admission or Random-null gate: actual tick/descendant
 * bodies decide whether/when Random and floating operations are reached.
 * Original/native runtime qualification remains separate from implementation.
 */
mc_world_storage_status mc_population_lake_place(const mc_population_lake_feature *feature,
                                                 mc_world_runtime *world, mc_java_random *random,
                                                 int32_t x, int32_t y, int32_t z, bool *out,
                                                 mc_population_lake_spring_error *error);
mc_world_storage_status mc_population_spring_place(const mc_population_spring_feature *feature,
                                                   mc_world_runtime *world, mc_java_random *random,
                                                   int32_t x, int32_t y, int32_t z, bool *out,
                                                   mc_population_lake_spring_error *error);

#endif
