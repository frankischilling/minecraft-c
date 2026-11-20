#ifndef MC_ENTITY_PLAYER_TICK_INTERNAL_H
#define MC_ENTITY_PLAYER_TICK_INTERNAL_H

#include "mc/entity/player/tick.h"

/* Same GS.w_ implementation. A containing caller supplies whether its actual
 * externally retained result can receive an owned child. Its internal scratch
 * error alone is not a destination. false still executes every earlier source
 * effect and refuses only if wake53/73 is actually reached. true requires out.
 * No source context/dependency is fabricated and no failed source work retries. */
mc_entity_status mc_player_tick_with_child_output(mc_entity *, const mc_player_action_context *,
                                                  const mc_player_tick_limits *,
                                                  bool can_transfer_children,
                                                  mc_player_tick_error *out);

#endif
