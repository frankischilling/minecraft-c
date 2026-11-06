#ifndef MC_ENTITY_LIVING_SLEEPING_H
#define MC_ENTITY_LIVING_SLEEPING_H

#include "mc/entity/player/player.h"

typedef enum mc_entity_sleeping_status {
    MC_ENTITY_SLEEPING_OK,
    MC_ENTITY_SLEEPING_INVALID_ARGUMENT,
    MC_ENTITY_SLEEPING_SOURCE_NULL,
    MC_ENTITY_SLEEPING_ENTITY_FAILED,
    MC_ENTITY_SLEEPING_PLAYER_FAILED,
    MC_ENTITY_SLEEPING_UNIMPLEMENTED_DEPENDENCY
} mc_entity_sleeping_status;

typedef struct mc_entity_sleeping_error {
    mc_entity_sleeping_status status;
    mc_entity_status entity;
    mc_player_error player;
} mc_entity_sleeping_error;

/* Virtual Living.N on the actual canonical receiver. LocalPlayer delegates to
 * its genuine inherited GS implementation; the admitted kinds with unchanged
 * LS.N return false. Query-only Player and unknown concrete overrides remain
 * explicit dependencies. No constructor/readiness or class active use is added.
 *
 * Keep the actual receiver and its namespace live and exclusive. Out is required
 * and changes only on success; error is optional and fresh when supplied. All
 * outputs/errors are disjoint from the owner and each other. The caller owns
 * its invocation site; this dispatch helper does not supply a generic source PC. */
mc_entity_sleeping_status mc_entity_living_is_sleeping(mc_entity *, bool *out,
                                                       mc_entity_sleeping_error *);

#endif
