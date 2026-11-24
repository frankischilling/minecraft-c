#ifndef MC_WORLD_RUNTIME_RESPAWN_H
#define MC_WORLD_RUNTIME_RESPAWN_H

#include "mc/entity/mount/mount.h"
#include "mc/entity/movement/effects.h"
#include "mc/world/runtime/entity_tick.h"
#include "mc/world/runtime/players.h"

/* One actual newly constructed client br identity. The caller owns this value;
 * no World, Info, Entity or cache occurrence is retained by it. */
typedef struct mc_world_respawn_position {
    int32_t x, y, z;
} mc_world_respawn_position;

typedef enum mc_world_respawn_child {
    MC_WORLD_RESPAWN_CHILD_NONE,
    MC_WORLD_RESPAWN_CHILD_MOUNT,
    MC_WORLD_RESPAWN_CHILD_DEAD,
    MC_WORLD_RESPAWN_CHILD_LIST,
    MC_WORLD_RESPAWN_CHILD_SLEEP
} mc_world_respawn_child;

typedef struct mc_world_respawn_error {
    mc_world_storage_status status;
    const char *operation, *method;
    unsigned int bytecode;
    bool source_null;
    mc_world_storage_error cause;
    mc_world_info_status info;
    size_t queries, random_draws;
    int32_t x, z;
    mc_world_respawn_child child_kind;

    /* Only the tagged child may be inspected. Failed owned child work must be
     * retired before this error is reused; value copies do not duplicate it. */
    union {
        mc_entity_mount_error mount;
        mc_entity_effect_error dead;
        mc_world_entity_tick_error list;
        mc_world_player_sleep_error sleep;
    } child;
} mc_world_respawn_error;

#define MC_WORLD_RESPAWN_DEFAULT_SPAWN_QUERIES ((size_t)1048576)

void mc_world_respawn_error_init(mc_world_respawn_error *);
bool mc_world_respawn_error_retire(mc_world_respawn_error *);

/* Actual client fd.u(): allocate before current Info x/y/z reads at8/15/22,
 * then perform br constructor stores and publish the distinct owned identity.
 * Failure preserves out and every reached child effect. */
mc_world_storage_status mc_world_runtime_respawn_spawn_new(mc_world_runtime *,
                                                           mc_world_respawn_position **out,
                                                           mc_world_respawn_error *);
void mc_world_respawn_position_free(mc_world_respawn_position *);

/* Actual fd.d(): default nonpositive SpawnY to64, then query World.a(x,z)
 * until nonair, drawing exactly four current World.Random nextInt(8) values
 * after each air answer. Current Info is reloaded at each getter/setter.
 * A literal zero bound refuses before the first pending top-Block query;
 * limits preserve prior SpawnY and Random effects, never truncate success. */
mc_world_storage_status mc_world_runtime_respawn_repair_spawn(mc_world_runtime *,
                                                              size_t max_queries,
                                                              mc_world_respawn_error *);

/* Actual client fd.v(), sharing the real List/Chunk/WorldAccess children of
 * ordinary Entity updates. Remove deferred occurrences, notify, clear D,
 * unlink stale mount pairs and remove dead Entities. No Entity/weather/Tile
 * tick or terminal container teardown is substituted for this body. */
mc_world_storage_status mc_world_runtime_respawn_cleanup(mc_world_runtime *,
                                                         const mc_world_entity_tick_context *,
                                                         const mc_world_entity_tick_limits *,
                                                         mc_world_entity_tick_error *);

/* Actual fd.e(sn): passenger.i(null), current Entity.i(null), virtual K(),
 * then Player d.remove(first equals match) and World.y(). Source relationship,
 * dead, List and sleeping effects survive later failures. All actual owners,
 * registry/math and explicitly scoped relationship actors stay live/exclusive.
 * Source World and argument need not be the Entity's construction World.
 * Failed K child work remains owned exactly as its effect API specifies. */
mc_world_storage_status mc_world_runtime_respawn_remove(mc_world_runtime *, mc_entity *,
                                                        mc_registry_classes *, mc_math_classes *,
                                                        mc_entity *const *relationship_actors,
                                                        size_t relationship_actor_count,
                                                        mc_world_respawn_error *);

#endif
