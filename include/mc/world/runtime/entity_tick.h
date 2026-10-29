#ifndef MC_WORLD_RUNTIME_ENTITY_TICK_H
#define MC_WORLD_RUNTIME_ENTITY_TICK_H

#include "mc/entity/item/tick.h"
#include "mc/entity/player/tick.h"
#include "mc/world/chunk/entity_errors.h"
#include "mc/world/runtime/acquisition_error.h"
#include "mc/world/runtime/tile_entities/piston.h"
#include "mc/world/runtime/world_access_error.h"

typedef struct mc_world_entity_tick_context {
    mc_player_action_context player;
    const mc_player_tick_limits *player_limits;
    const mc_item_entity_tick_limits *item_limits;
    const mc_entity_move_limits *piston_move_limits;
    /* Temporary native lifetime admission only. Every actual actor in this
     * immutable array remains independently held through the whole call and
     * reentry. It is not an association owner or a gameplay snapshot. */
    mc_entity *const *relationship_actors;
    size_t relationship_actor_count;
} mc_world_entity_tick_context;

typedef struct mc_world_entity_tick_limits {
    size_t max_loop_work;
    size_t max_passenger_calls;
    size_t max_temporary_bytes;
} mc_world_entity_tick_limits;

#define MC_WORLD_ENTITY_TICK_DEFAULT_LOOP_WORK ((size_t)1048576)
#define MC_WORLD_ENTITY_TICK_DEFAULT_PASSENGER_CALLS ((size_t)4096)
#define MC_WORLD_ENTITY_TICK_DEFAULT_TEMPORARY_BYTES ((size_t)16 * 1024 * 1024)

typedef enum mc_world_entity_tick_child {
    MC_WORLD_ENTITY_TICK_CHILD_NONE,
    MC_WORLD_ENTITY_TICK_CHILD_PLAYER,
    MC_WORLD_ENTITY_TICK_CHILD_ITEM,
    MC_WORLD_ENTITY_TICK_CHILD_PISTON,
    MC_WORLD_ENTITY_TICK_CHILD_ACQUISITION,
    MC_WORLD_ENTITY_TICK_CHILD_TILE_ENTITY,
    MC_WORLD_ENTITY_TICK_CHILD_PRESENTATION,
    MC_WORLD_ENTITY_TICK_CHILD_REGISTRY
} mc_world_entity_tick_child;

typedef struct mc_world_entity_tick_error {
    mc_world_storage_status status;
    const char *operation, *source_class, *source_method;
    const char *virtual_class, *virtual_method, *riding_class, *mount_class, *mount_method;
    unsigned int bytecode, loop_bytecode, wrapper_bytecode, update_bytecode;
    unsigned int chunk_bytecode, presence_bytecode, provider_bytecode;
    unsigned int world_access_bytecode, dedicated_bytecode, index_bytecode;
    unsigned int tile_bytecode, spawner_bytecode, nearest_bytecode;
    unsigned int relationship_bytecode, riding_bytecode, mount_bytecode;
    size_t loop_work, max_loop_work, passenger_calls, max_passenger_calls;
    size_t temporary_bytes, max_temporary_bytes;
    int32_t weather_index, deferred_index, loaded_index;
    size_t tile_index, comparison_index;
    mc_entity_error entity;
    mc_world_chunk_entity_error chunk;
    mc_world_storage_error cause;
    mc_world_entity_tick_child child;

    /* Only the tagged failed child is active. Completed child calls clear the
     * tag; no inactive union member may be inspected. */
    union {
        mc_player_tick_error player;
        mc_item_entity_tick_error item;
        mc_piston_tick_error piston;
        mc_world_runtime_acquisition_error acquisition;
        mc_live_tile_entity_error tile_entity;
        mc_world_mutation_error presentation;
        mc_registry_error registry;
    } child_error;

    mc_world_access_failure world_access_failure;
} mc_world_entity_tick_error;

void mc_world_entity_tick_error_init(mc_world_entity_tick_error *);
/* Genuine GAME World bodies: fd.f(Entity), fd.a(Entity,flag), fd.g(). These
 * are not the World time/block/weather scheduler or Minecraft/frontend loop.
 * Context, limits, exact owners and disjoint caller outputs stay live/exclusive
 * through synchronous descendants. No implicit pool reset, rollback, synthetic
 * Chunk/List or successful unsupported virtual is provided. Each native call
 * owns private heap diagnostics; caller output is published only at return.
 * Passenger tail continuation retains all source caller lifetimes in a bounded
 * per-call hold ledger. Limits fail before pending work, never truncate success.
 * Server dedicated a(Entity,flag), unimplemented concrete tick/riding/mob bodies
 * remain explicit reached dependencies. Read only the tagged failed child. */
/* These three client fd.f/a/g entries require an actual CLIENT GAME World.
 * Other source owners fail native admission before source effects; this is
 * not a synthetic Java exception. Base/dedicated SERVER updates remain a
 * separate unfinished dependency. Admission preserves World/Entity state and
 * earlier World diagnostics; optional caller error is fresh for this attempt. */
mc_world_storage_status mc_world_runtime_tick_entity(mc_world_runtime *, mc_entity *,
                                                     const mc_world_entity_tick_context *,
                                                     const mc_world_entity_tick_limits *,
                                                     mc_world_entity_tick_error *);
mc_world_storage_status mc_world_runtime_update_entity(mc_world_runtime *, mc_entity *, bool,
                                                       const mc_world_entity_tick_context *,
                                                       const mc_world_entity_tick_limits *,
                                                       mc_world_entity_tick_error *);
mc_world_storage_status mc_world_runtime_tick_entities(mc_world_runtime *,
                                                       const mc_world_entity_tick_context *,
                                                       const mc_world_entity_tick_limits *,
                                                       mc_world_entity_tick_error *);

/* Actual fd.a(Entity) weather publication. Appends the same nullable occurrence
 * before returning true; each nonnull occurrence owns a separate native hold.
 * No fields of the Entity, Chunk, callback or tick are reached by this body. */
mc_world_storage_status mc_world_runtime_add_weather_entity(mc_world_runtime *, mc_entity *, bool *,
                                                            mc_world_entity_error *);
#endif
