#ifndef MC_ENTITY_PLAYER_ALLY_ALERT_H
#define MC_ENTITY_PLAYER_ALLY_ALERT_H

#include "mc/entity/player/player.h"
#include "mc/world/runtime/collision/class_entities.h"

typedef struct mc_player_ally_alert_call mc_player_ally_alert_call;

typedef struct mc_player_ally_alert_error {
    mc_entity_status status;
    const char *operation, *exception_class;
    mc_player_site site, child;
    bool native_policy, source_exception, source_error;
    mc_entity_error entity;
    mc_entity_bounds_error bounds;
    mc_world_nearby_error nearby;
} mc_player_ally_alert_error;

typedef struct mc_player_ally_alert_cleanup {
    mc_entity_status status;
    const char *operation;
    mc_world_storage_status world;
} mc_player_ally_alert_cleanup;

typedef struct mc_player_ally_alert_state {
    mc_entity *player, *target;
    mc_world_runtime *world;
    const mc_entity_bounds *bounds, *expanded;
    const mc_world_class_entity_list *wolves;
    bool active, returned, attack;
    unsigned int source_pc;
    size_t wolves_examined, wolves_alerted;
    const mc_player_ally_alert_error *error;
    const mc_player_ally_alert_cleanup *cleanup;
} mc_player_ally_alert_state;

/* Actual client GS.a(Living,Z). Capture World before the real pooled box and
 * expansion, query the actual Class List, then visit its source occurrences.
 * attack=true tests sitting and skips sitting Wolves; false never calls that
 * getter and unsits eligible Wolves before the owning target store.
 * target may be NULL. Canonical RAW Player fields remain observable; no READY
 * gate, replacement World, pool reset or source-field repair is introduced.
 * Keep the graph exclusive and all source fields live through retirement.
 * A native admission refusal leaves the fresh output NULL. A reached source
 * failure, including a NULL receiver, returns a retained owner even without
 * any separate diagnostic output. Inspect borrows its stable error/cleanup.
 * The caller keeps its enclosing invocation site independently of this GS
 * helper's own source site and deepest reached child. */
mc_entity_status mc_player_ally_alert_begin(mc_entity *player, mc_entity *target, bool attack,
                                            mc_player_ally_alert_call **out);
bool mc_player_ally_alert_inspect(const mc_player_ally_alert_call *, mc_player_ally_alert_state *);
/* Native cleanup only. Preserve the first source failure and refuse while
 * active or while any reached diagnostic capture/owner cannot retire. No
 * query, watcher update, target assignment or source work is replayed. */
mc_entity_status mc_player_ally_alert_retire(mc_player_ally_alert_call **,
                                             mc_player_ally_alert_cleanup *);

#endif
