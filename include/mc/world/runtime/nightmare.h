#ifndef MC_WORLD_RUNTIME_NIGHTMARE_H
#define MC_WORLD_RUNTIME_NIGHTMARE_H

#include "mc/entity/player/sleep.h"
#include "mc/world/runtime/bed_spawn.h"
#include "mc/world/runtime/path.h"

typedef struct mc_world_nightmare_call mc_world_nightmare_call;
typedef struct mc_world_nightmare_class_error mc_world_nightmare_class_error;
typedef struct mc_world_spawn_postspawn_work mc_world_spawn_postspawn_work;
typedef struct mc_mob_error mc_mob_error;
typedef struct mc_mob_spawn_error mc_mob_spawn_error;
typedef struct mc_mob_spawn_state mc_mob_spawn_state;
typedef struct mc_world_solid_support_detail mc_world_solid_support_detail;
typedef struct mc_entity_position_rotation_error mc_entity_position_rotation_error;
typedef struct mc_entity_tick_error mc_entity_tick_error;
typedef struct mc_path_finder_error mc_path_finder_error;

typedef struct mc_world_nightmare_context {
    /* Actual CQ/VE/Entity defining namespaces, independent of nullable World.
     * list_owner keeps the captured canonical Player List and typed ledger
     * alive; it may differ from World. A shaped empty borrowed List needs no
     * owner, and its storage stays live through the retained call. */
    mc_world_runtime_classes *code_classes;
    mc_world_runtime *list_owner;
    mc_math_classes *math;
} mc_world_nightmare_context;

typedef struct mc_world_nightmare_limits {
    size_t max_work;
    mc_world_path_limits path;
    mc_world_bed_spawn_limits bed;
} mc_world_nightmare_limits;

typedef enum mc_world_nightmare_child {
    MC_WORLD_NIGHTMARE_NONE,
    MC_WORLD_NIGHTMARE_CLASSES,
    MC_WORLD_NIGHTMARE_MATH,
    MC_WORLD_NIGHTMARE_LIST,
    MC_WORLD_NIGHTMARE_SUPPORT,
    MC_WORLD_NIGHTMARE_MATERIAL,
    MC_WORLD_NIGHTMARE_MOB,
    MC_WORLD_NIGHTMARE_POSITION,
    MC_WORLD_NIGHTMARE_ELIGIBILITY,
    MC_WORLD_NIGHTMARE_FINDER,
    MC_WORLD_NIGHTMARE_PATH,
    MC_WORLD_NIGHTMARE_BED,
    MC_WORLD_NIGHTMARE_PUBLICATION,
    MC_WORLD_NIGHTMARE_POSTSPAWN,
    MC_WORLD_NIGHTMARE_WAKE,
    MC_WORLD_NIGHTMARE_AMBIENT,
    MC_WORLD_NIGHTMARE_REFLECTION_PRINT
} mc_world_nightmare_child;

typedef struct mc_world_nightmare_error {
    mc_world_storage_status status;
    const char *operation, *exception_class;
    bool native_policy, source_exception, source_error;
    unsigned int pc, child_pc;
    mc_world_nightmare_child child;
    mc_world_storage_error cause;
    const mc_world_nightmare_class_error *classes;
    const mc_math_class_error *math;
    const mc_registry_error *registry;
    const mc_world_solid_support_detail *support;
    const mc_world_material_error *material;
    const mc_mob_error *mob;
    const mc_entity_position_rotation_error *position;
    const mc_mob_spawn_error *eligibility;
    const mc_path_finder_error *finder;
    const mc_path_error *path;
    const mc_world_bed_spawn_error *bed;
    const mc_world_entity_error *publication;
    const mc_world_spawn_postspawn_work *postspawn;
    const mc_player_sleep_error *wake;
    const mc_player_sleep_cleanup *wake_cleanup;
    const mc_entity_tick_error *ambient;
    /* A caught source Exception reaches the real unfinished printer at362.
     * This is neither a returned false nor an unhandled constructor Error. */
    bool reflection_caught, print_stack_trace_pending;
    const char *reflection_exception, *reflection_cause;
} mc_world_nightmare_error;

typedef struct mc_world_nightmare_cleanup {
    mc_world_storage_status status;
    const char *operation;
    mc_world_nightmare_child child;
    mc_world_storage_error world;
    mc_registry_error registry;
    mc_math_class_error math;
    mc_path_error finder;
    mc_player_sleep_cleanup wake;
    const mc_world_bed_spawn_error *bed;
    const mc_world_spawn_postspawn_work *postspawn;
} mc_world_nightmare_cleanup;

typedef struct mc_world_nightmare_state {
    bool active, returned, result, player_succeeded;
    unsigned int source_pc;
    mc_world_runtime *world;
    const mc_world_reference_list *players;
    size_t cursor, players_started, attempts_started, attempts_returned;
    int32_t expected_mod_count, attempt, x, y, z, initial_y, class_index;
    uintptr_t class_array_identity;
    size_t class_array_length;
    const mc_entity_class_literal *selected_class;
    const mc_world_runtime_classes *selected_world_class;
    mc_entity *player, *mob, *candidate;
    const mc_path_finder *finder;
    mc_path *path;
    const mc_path_point *last_point;
    const mc_player_coordinates *exit;
    const mc_mob_spawn_state *eligibility;
    size_t random_calls, constructors, predicates, paths, publications;
    size_t postspawn_calls, wakes, ambient_calls, work;
    size_t native_bytes, native_peak_bytes, native_allocations;
    const mc_world_nightmare_error *failure;
    const mc_world_nightmare_cleanup *cleanup;
} mc_world_nightmare_state;

mc_world_nightmare_limits mc_world_nightmare_default_limits(void);
/* Actual client CQ.a(World,List)Z. One FW is constructed against the nullable
 * World before List.iterator. Its same heap/map serve every reached attempt.
 * The captured List iterator and late Class-array reads retain source order;
 * every Player receives up to20 real attempts, stopping only after success.
 * No sleeping/alive precheck, EW, World-path wrapper or fresh finder is added.
 * A true result is published only after pose, publication, postspawn, wake and
 * ambient sound all return. A false result requires the actual return653.
 * out starts NULL. Context/limits are copied; dependent storage is exclusive,
 * live and disjoint. Reached failures publish a retained owner even with NULL
 * admission output. Inspect borrows children until checked retirement. */
mc_world_storage_status mc_world_nightmare_begin(mc_world_runtime *nullable_world,
                                                 mc_world_reference_list *nullable_players,
                                                 const mc_world_nightmare_context *,
                                                 const mc_world_nightmare_limits *,
                                                 mc_world_nightmare_call **out,
                                                 mc_world_nightmare_error *admission);
bool mc_world_nightmare_inspect(const mc_world_nightmare_call *, mc_world_nightmare_state *);
/* Cleanup only: preserve first failure and refuse while active or while a
 * reached child cannot retire. Partial cleanup consumes completed occurrences
 * and ends their views. No source work, retry, wake or rollback runs here. */
mc_world_storage_status mc_world_nightmare_retire(mc_world_nightmare_call **,
                                                  mc_world_nightmare_cleanup *);

#endif
