#ifndef MC_WORLD_RUNTIME_LOADING_PLAYER_INTERNAL_H
#define MC_WORLD_RUNTIME_LOADING_PLAYER_INTERNAL_H

#include "mc/world/runtime/loading/player.h"

/* Same FD.a(GS) body. The compact compatibility adapter has no owning output
 * contract, so passes false. Actual retained Load passes its real availability.
 * A private scratch alone is never a failed wake destination. true requires out. */
mc_world_storage_status mc_world_runtime_load_player_with_child_output(
    mc_world_runtime *, mc_entity *, bool can_transfer_children, mc_world_player_load_error *out);

#endif
