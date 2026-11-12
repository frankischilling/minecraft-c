#ifndef MC_ENTITY_MOB_TICK_WORKSPACE_INTERNAL_H
#define MC_ENTITY_MOB_TICK_WORKSPACE_INTERNAL_H

#include "mc/entity/mob/tick.h"

/* Fixed diagnostic storage for one exclusive outer World invocation. The
 * environment call returns before the action call starts. This union MUST
 * remain disjoint from the working Mob error (especially its detail union),
 * every owner/dependency and every other entered invocation's diagnostics.
 * Selected failures are copied into the working error before reuse. Pointers
 * inside those values borrow actual retained producers, never this scratch.
 * No caller may retain a pointer to the union's active member after return. */
typedef union mc_mob_tick_child_workspace {
    mc_entity_tick_error environment;
    mc_player_action_error action;
} mc_mob_tick_child_workspace;

/* Internal World caller: working is its exact independent Mob child output,
 * fresh or already retired. It is initialized on EVERY new attempt, including
 * rejected admission. scratch is a separate optional, fixed-size reservation;
 * NULL keeps the old independently allocated environment/action children.
 * There is no source resume/replay or owner/global cache. Nested callers use
 * independent outputs/reservations; public Mob entries remain independent.
 * NULL working takes the legacy independent-output runner, ignoring scratch.
 * Full initializers, source work, holds, failure precedence and selected
 * cleanup/borrow contracts are unchanged. */
mc_entity_status mt_tick_in_place(mc_entity *, mc_registry_classes *, mc_math_classes *,
                                  const mc_mob_tick_limits *, mc_mob_tick_error *working,
                                  mc_mob_tick_child_workspace *scratch);

#endif
