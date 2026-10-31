#ifndef MC_ENTITY_MOB_TICK_H
#define MC_ENTITY_MOB_TICK_H
#include "mc/core/math/classes.h"
#include "mc/entity/action/action.h"
#include "mc/entity/hit_result.h"
#include "mc/entity/mob/construction.h"
#include "mc/entity/tick/environment.h"
#include "mc/world/runtime/brightness.h"
#include "mc/world/runtime/collision/class_entities.h"
#include "mc/world/runtime/path.h"
#include "mc/world/runtime/raytrace.h"
#include "mc/world/storage/source_chunk_loader.h"

typedef struct mc_mob_tick_limits {
    size_t max_work;
    mc_entity_tick_limits environment;
    mc_player_action_limits action;
    mc_entity_move_limits movement;
    mc_world_path_limits path;
} mc_mob_tick_limits;

#define MC_MOB_TICK_DEFAULT_WORK ((size_t)1048576)

typedef enum mc_mob_tick_path_kind {
    MC_MOB_TICK_PATH_NONE,
    MC_MOB_TICK_PATH_COORDINATES,
    MC_MOB_TICK_PATH_TARGET
} mc_mob_tick_path_kind;

typedef struct mc_mob_tick_path_arguments {
    mc_mob_tick_path_kind kind;
    bool stored;
    /* Native comparison-only identities of the actual evaluated operands.
     * They add no owner hold and are not Java identity hashes. */
    uintptr_t receiver_identity, target_identity;
    int32_t x, y, z;
    float range;
} mc_mob_tick_path_arguments;

typedef struct mc_mob_tick_drop_arguments {
    bool stored;
    int32_t item_id, count;
} mc_mob_tick_drop_arguments;

typedef struct mc_mob_tick_state {
    bool returned, environment_returned, action_returned, behavior_returned;
    bool cleanup_pending, ray_returned, ray_arguments_stored;
    bool path_returned, path_store_completed;
    double ray_start[3], ray_end[3];
    mc_entity_class_kind kind;
    /* Direct concrete-body calls; shared children retain separate diagnostics. */
    size_t work, concrete_random_calls, concrete_world_random_calls, particles, sounds, ray_calls;
    size_t path_calls, path_waypoints, path_advances;
    unsigned int source_pc, living_pc, creature_pc, monster_pc, concrete_pc;
    mc_mob_tick_path_arguments path;
    mc_mob_tick_drop_arguments drop;
} mc_mob_tick_state;

typedef enum mc_mob_tick_child {
    MC_MOB_TICK_CHILD_NONE,
    MC_MOB_TICK_CHILD_ENVIRONMENT,
    MC_MOB_TICK_CHILD_ACTION,
    MC_MOB_TICK_CHILD_BRIGHTNESS,
    MC_MOB_TICK_CHILD_QUERY,
    MC_MOB_TICK_CHILD_PRESENTATION,
    MC_MOB_TICK_CHILD_SOUND,
    MC_MOB_TICK_CHILD_CLASS_LIST,
    MC_MOB_TICK_CHILD_BOUNDS,
    MC_MOB_TICK_CHILD_VECTOR,
    MC_MOB_TICK_CHILD_MATERIAL,
    MC_MOB_TICK_CHILD_EFFECT,
    MC_MOB_TICK_CHILD_RAY,
    MC_MOB_TICK_CHILD_PATH
} mc_mob_tick_child;

typedef struct mc_mob_tick_query_error {
    mc_world_storage_error cause;
    mc_world_runtime_acquisition_error acquisition;
    mc_world_query_error source;
} mc_mob_tick_query_error;

/* Selected retained ray descendants are flattened before child retirement.
 * cause's pointer members are NULL; the presence bits select these value
 * records instead. Source-loader and invocation receipts remain borrowed from
 * their actual producers, never from the retired ray working allocation. */
typedef struct mc_mob_tick_ray_error {
    mc_world_raytrace_error cause;
    bool has_world, has_collision, has_source_chunk, has_registry, has_entity;
    bool has_vector, has_geometry, has_bounds, has_hit;
    mc_world_storage_error world;
    mc_world_collision_error collision;
    mc_world_source_chunk_error source_chunk;
    mc_registry_error registry;
    mc_entity_error entity;
    mc_entity_vector_error vector;
    mc_entity_geometry_error geometry;
    mc_entity_bounds_error bounds;
    mc_entity_hit_error hit;
} mc_mob_tick_ray_error;

/* Complete value projection before the FD owner is retired. All pointer
 * members in cause and nested acquisition.source_chunk are NULL; the presence
 * bits select these copies. Receipt views still borrow their actual producers. */
typedef enum mc_mob_tick_path_child {
    MC_MOB_TICK_PATH_CHILD_NONE,
    MC_MOB_TICK_PATH_CHILD_INVOCATION,
    MC_MOB_TICK_PATH_CHILD_PATH,
    MC_MOB_TICK_PATH_CHILD_GEOMETRY
} mc_mob_tick_path_child;

typedef struct mc_mob_tick_path_error {
    mc_mob_tick_path_child kind;
    mc_world_path_error cause;
    bool has_world, has_entity, has_math, has_registry, has_cache, has_finder, has_source_chunk;
    mc_world_storage_error world;
    mc_entity_error entity;
    mc_math_class_error math;
    mc_registry_error registry;
    mc_client_chunk_cache_error cache;
    mc_path_error finder_path;
    mc_registry_error finder_registry;
    mc_client_chunk_cache_error finder_cache;
    mc_world_source_chunk_error source_chunk;
    mc_path_error path;
    mc_entity_geometry_error geometry;
} mc_mob_tick_path_error;

typedef struct mc_mob_tick_error {
    mc_entity_status status;
    const char *operation, *exception_class;
    bool native_policy, source_exception;
    mc_mob_site site, child_site;
    mc_mob_tick_state state;
    mc_registry_error registry;
    mc_math_class_error math;
    mc_entity_error reference;
    mc_world_storage_error world;
    mc_mob_tick_ray_error cleanup_ray;
    mc_mob_tick_path_error cleanup_path;
    mc_mob_tick_child child;

    union {
        mc_entity_tick_error environment;
        mc_player_action_error action;
        mc_world_brightness_error brightness;
        mc_mob_tick_query_error query;
        mc_world_mutation_error presentation;
        mc_world_movement_error sound;
        mc_world_nearby_error class_list;
        mc_entity_bounds_error bounds;
        mc_entity_vector_error vector;
        mc_entity_environment_error material;
        mc_entity_effect_error effect;
        mc_mob_tick_ray_error ray;
        mc_mob_tick_path_error path;
    } detail;
} mc_mob_tick_error;

void mc_mob_tick_error_init(mc_mob_tick_error *);
/* Actual eleven canonical client Mob w_ declarations and their reached children.
 * The same Entity, Registry, Math and whole World graph stay live/exclusive
 * through synchronous callbacks and diagnosis. Actual source reference fields
 * use their owning native cells. No profile, Random, World or Player substitute
 * is installed. Native ceilings stop before pending work and preserve prefixes.
 * The complete value error contains only producer-borrowed receipt views;
 * callers keep those producers live. No diagnostic points into working scratch.
 * Constructor diagnostics and source state are never repaired on failure. */
mc_entity_status mc_mob_tick(mc_entity *, mc_registry_classes *, mc_math_classes *,
                             const mc_mob_tick_limits *, mc_mob_tick_error *);
/* Retry only an exceptionally retained native child/argument cleanup. A
 * pending child owns one native Mob occurrence; success releases it. Source
 * fields, Random/pool state and the original tick diagnostic are not repaired
 * or replayed. The caller retains its independent occurrence through cleanup. */
mc_entity_status mc_mob_tick_cleanup(mc_entity *, mc_mob_tick_error *);
#endif
