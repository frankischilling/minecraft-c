#ifndef MC_INVENTORY_RUNTIME_SCATTER_INTERNAL_H
#define MC_INVENTORY_RUNTIME_SCATTER_INTERNAL_H

#include "mc/entity/player/death.h"
#include "mc/entity/player/drop.h"

/* The enclosing Player death work owns the reached drop until its checked
 * retirement, including after failure. IX arrays/cells are read afresh at each
 * Source site, with clears only after the genuine scatter child returns. */
mc_entity_status mc_inventory_player_scatter_entered(mc_inventory_player *, mc_registry_classes *,
                                                     mc_math_classes *, mc_player_death_state *,
                                                     mc_player_drop_call **, mc_player_drop_error *,
                                                     mc_player_drop_error *);

#endif
