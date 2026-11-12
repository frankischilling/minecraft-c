#ifndef MC_ENTITY_PLAYER_SPAWN_H
#define MC_ENTITY_PLAYER_SPAWN_H

#include "mc/entity/player/player.h"
#include "mc/world/runtime/collision/boxes.h"

typedef struct mc_player_spawn_limits {
    size_t max_search_work;
} mc_player_spawn_limits;

#define MC_PLAYER_SPAWN_DEFAULT_SEARCH_WORK ((size_t)1048576)

typedef enum mc_player_spawn_cause {
    MC_PLAYER_SPAWN_CAUSE_NONE,
    MC_PLAYER_SPAWN_CAUSE_OWNER,
    MC_PLAYER_SPAWN_CAUSE_NULL_REFERENCE,
    MC_PLAYER_SPAWN_CAUSE_FP,
    MC_PLAYER_SPAWN_CAUSE_WORLD,
    MC_PLAYER_SPAWN_CAUSE_LIMIT
} mc_player_spawn_cause;

typedef struct mc_player_spawn_error {
    mc_entity_status status;
    mc_player_spawn_cause cause;
    const char *operation, *source_class, *source_method;
    unsigned int player_bytecode, size_living_bytecode, base_bytecode;
    unsigned int position_bytecode, list_bytecode;
    size_t search_work, max_search_work;
    mc_entity_error entity;
    mc_world_storage_error world;
    mc_world_storage_status list_status;
    /* Complete last failed collision child, including actual acquisition,
     * Block/nearby/Entity/TileEntity descendants. No child is active on success. */
    bool collision_failed;
    mc_world_collision_error collision;
} mc_player_spawn_error;

void mc_player_spawn_error_init(mc_player_spawn_error *error);

/* Actual client gs.t_() and its invoked inherited sn.t_() on a genuine held
 * READY canonical LocalPlayer. The body entry executes only the ancestor;
 * virtual position/size descendants remain those of that actual dc identity.
 * No constructor, Player publication, controller, GUI, save or tick is implied.
 * Null source World really returns from sn before clearing velocities/pitch;
 * the gs wrapper still completes its health/death stores afterward.
 *
 * Calls keep this exact Player alive, own private heap diagnostics, and publish
 * caller error only at return. Existing World/List children retain their real
 * owners through synchronous callbacks. Inputs/outputs/errors and all owners
 * remain live, exclusive and disjoint across reentry; a temporarily replaced
 * source bounds/World reference must retain its original native owner.
 * Reached failure preserves all earlier source effects and full child frames.
 * Strict x86-64 nearest-even arithmetic is admitted at the first reached FP
 * operation; a null World adds no FP guard. Limits fail before a pending search
 * iteration; no shortened search returns success. NULL limits uses the default.
 * No implicit bounds/vector pool reset, fallback collision result or invented
 * source exception is supplied by native argument/phase/resource rejection. */
mc_entity_status mc_player_spawn_reset(mc_entity *player, const mc_player_spawn_limits *limits,
                                       mc_player_spawn_error *error);
mc_entity_status mc_player_spawn_reset_body(mc_entity *player, const mc_player_spawn_limits *limits,
                                            mc_player_spawn_error *error);

#endif
