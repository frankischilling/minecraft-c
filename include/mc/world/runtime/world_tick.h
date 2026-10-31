#ifndef MC_WORLD_RUNTIME_WORLD_TICK_H
#define MC_WORLD_RUNTIME_WORLD_TICK_H

#include "mc/core/math/classes.h"
#include "mc/world/dimension/world_chunk_manager.h"
#include "mc/world/runtime/entity_errors.h"
#include "mc/world/runtime/saving/errors.h"
#include "mc/world/runtime/scheduling.h"
#include "mc/world/runtime/spawning.h"
#include "mc/world/runtime/weather.h"
#include "mc/world/runtime/world_access_error.h"

typedef struct mc_world_scheduled_drain_error {
    mc_world_schedule_error call;
    const char *exception_class;
    bool native_policy;
    int32_t initial_count, iterations, removed;
} mc_world_scheduled_drain_error;

/* Actual client fd.a(Z)Z: compare E/F sizes, process at most1000 structural
 * first records, test current Info time unless forced, remove E then F, test
 * nonloading radius8 presence, then dispatch the matching positive Block tick
 * with current World.r. The returned bool means E remains nonempty. Each
 * selected record has its own hold across both removals and synchronous tick
 * reentry. No repair, rollback, queue snapshot or replacement RNG is supplied.
 * Native admission requires a live client GAME owner; outputs are disjoint,
 * exclusive and published only at return. On failure pending is unchanged. */
mc_world_storage_status mc_world_runtime_drain_scheduled(mc_world_runtime *, bool force,
                                                         bool *pending,
                                                         mc_world_scheduled_drain_error *);

typedef struct mc_world_tick_call mc_world_tick_call;
typedef struct mc_mob_error mc_mob_error;
typedef struct mc_mob_spawn_state mc_mob_spawn_state;
typedef struct mc_mob_spawn_error mc_mob_spawn_error;
typedef struct mc_entity mc_entity;

typedef enum mc_world_tick_child {
    MC_WORLD_TICK_CHILD_NONE,
    MC_WORLD_TICK_CHILD_WEATHER,
    MC_WORLD_TICK_CHILD_SLEEP,
    MC_WORLD_TICK_CHILD_SPAWNING,
    MC_WORLD_TICK_CHILD_PROVIDER,
    MC_WORLD_TICK_CHILD_SKY,
    MC_WORLD_TICK_CHILD_ACCESS,
    MC_WORLD_TICK_CHILD_SAVE,
    MC_WORLD_TICK_CHILD_SCHEDULED,
    MC_WORLD_TICK_CHILD_RANDOM
} mc_world_tick_child;

typedef struct mc_world_tick_error {
    mc_world_storage_status status;
    const char *operation, *source_class, *source_method, *exception_class;
    unsigned int bytecode, child_bytecode, descendant_bytecode;
    bool native_policy;
    mc_world_tick_child child;
    mc_world_storage_error cause;
    const mc_world_weather_error *weather;
    const mc_world_runtime_lifecycle_error *lifecycle;
    const mc_world_scheduled_drain_error *scheduled;
    const mc_world_chunk_manager_error *biome;
    const mc_world_spawn_pick_error *spawning;
    const mc_mob_error *mob;
    const mc_mob_spawn_error *eligibility;
    const mc_world_entity_error *publication;
    mc_math_class_error math;
    mc_world_access_failure world_access;
} mc_world_tick_error;

/* Passive observation at the first reached CQ773, before genuine allocation,
 * constructor effects or the World Random yaw draw. References borrow the
 * call-held class graph; Random is the actual stream's value at that point. */
typedef struct mc_world_spawn_constructor_trace {
    bool captured, random_present;
    const mc_entity_class_literal *class_literal;
    int32_t category, chunk_x, chunk_z, initial_x, initial_y, initial_z;
    int32_t trial_x, trial_y, trial_z, group, attempt, total_weight, ticket;
    size_t chunk_count, candidates_visited, random_calls;
    mc_java_random random;
} mc_world_spawn_constructor_trace;

typedef struct mc_world_tick_state {
    bool active, returned, weather_returned, sleeping, spawning_returned;
    bool provider_returned, sky_stored, time_stored, scheduled_returned;
    unsigned int source_pc;
    size_t spawn_chunk_count, random_chunk_count, sky_callbacks;
    int32_t spawn_category, spawn_chunk_x, spawn_chunk_z;
    size_t spawn_candidates_visited, spawn_random_calls;
    mc_world_spawn_pick spawn_pick;
    mc_world_spawn_constructor_trace first_constructor;
    const mc_entity_class_literal *spawn_class;
    mc_entity *spawn_entity; /* Borrowed candidate occurrence owned by this call. */
    mc_entity *spawn_jockey; /* Separate reached Skeleton occurrence. */
    bool spawn_constructed, spawn_positioned;
    bool spawn_eligibility_returned, spawn_eligible;
    const mc_mob_spawn_state *eligibility;
    size_t spawn_constructors, spawn_predicates, spawn_rejections, spawn_publications;
    int32_t spawn_group_successes, spawn_total;
    unsigned int spawn_postspawn_pc;
    bool spawn_publication_returned, spawn_published, spawn_postspawn_returned;
    int32_t spawn_initial_x, spawn_initial_y, spawn_initial_z;
    int32_t spawn_trial_x, spawn_trial_y, spawn_trial_z, spawn_group, spawn_attempt;
    int64_t next_time;
    mc_world_runtime *world;
    mc_math_classes *math; /* Borrowed same MathHelper namespace held by the call. */
    const mc_world_tick_error *error;
} mc_world_tick_state;

/* Ordered actual fd.l prefix and completed reachable branches. Missing source
 * descendants stop explicitly; returned is true only after actual return198.
 * Weather, sleeping predicate, real spawn candidate/count/weighted selection,
 * one concrete provider maintenance pass, sky callback loop, save/time stores,
 * whole scheduled drain and random-work candidate gathering run in source order.
 * Spawn support/material/nearest-player/distance gates and all3x4 trial loops
 * run before selected Mob initialization/construction. All eleven default Mob
 * constructors run on the same Entity classes/profile, followed by the fresh
 * World Random yaw, actual position and virtual spawn predicate. False predicates
 * continue the attempt loop; true predicates increment the per-Chunk group count
 * before actual publication, postspawn and the virtual group limit. Sheep color
 * and Spider jockey draws preserve World Random order. A reached Skeleton mount
 * body remains a dependency after the real Skeleton construction/publication.
 * Sleeping wake transitions and nonempty random Chunk work remain
 * separate dependencies, never successful no-ops.
 * Both false spawn flags return before touching its shared candidate Set.
 *
 * A published call independently binds the World and retains complete typed
 * child diagnostics until retire. Source effects survive failure and are never
 * replayed. out starts NULL; native admission preserves it and World diagnostics.
 * Native candidate ceiling65536 and same-hash/different-coordinate rejection
 * bound the modeled JDK17 HashSet domain; neither truncates source success. */
mc_world_storage_status mc_world_runtime_tick_begin(mc_world_runtime *, mc_math_classes *,
                                                    mc_world_tick_call **,
                                                    mc_world_tick_error *admission_error);
bool mc_world_runtime_tick_inspect(const mc_world_tick_call *, mc_world_tick_state *);
mc_world_storage_status mc_world_runtime_tick_retire(mc_world_tick_call **,
                                                     mc_world_tick_error *cleanup_error);

#endif
