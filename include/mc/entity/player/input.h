#ifndef MC_ENTITY_PLAYER_INPUT_H
#define MC_ENTITY_PLAYER_INPUT_H

#include "mc/client/input/movement_input.h"
#include "mc/entity/player/player.h"

/* Independent compact diagnostics: outer dc field/invocation site plus the
 * exact existing uo/lr child error. No second complete Player constructor frame
 * is embedded through shared Entity/World mutation contexts. */
typedef struct mc_player_input_error {
    mc_player_status status;
    const char *operation;
    mc_player_site site;
    mc_entity_status entity;
    mc_movement_input_error input;
} mc_player_input_error;

/* Direct actual dc.a field assignment, corresponding to Minecraft.a(World,
 * String,Player) putfield205. This is the store operation alone, not the other
 * Minecraft/controller/reset/publication work of that method. Retain the new
 * exact input before publication, replace the field, then release the old
 * field's hold. NULL clears the field; repeated/same/shared identities work.
 * Native retain failure preserves the old field and both source inputs.
 * Input options still BORROWS settings: keep settings live through the last
 * input hold, including Player fields. Snapshot input pointers are borrowed.
 *
 * Real RAW/failed/READY LocalPlayer owners are supported; there is no source
 * construction-phase test. Native null Player reaches putfield205. Calls hold
 * the Entity temporarily and require exclusive/live/disjoint owners and error
 * storage. Final Entity disposal releases this actual input occurrence without
 * invoking source reset or retiring borrowed settings. No tick is implied. */
mc_player_status mc_local_player_set_movement_input(mc_entity *player, mc_movement_input *input,
                                                    mc_player_input_error *error);

/* Actual dc.a(IZ)V: read dc.a at1 and dynamically invoke uo/lr.a(IZ)V at6.
 * NULL input fails6 before entering uo/lr; nested options failures retain their
 * exact existing lr getfield site and completed prefix. No field update/tick. */
mc_player_status mc_local_player_input_key(mc_entity *player, int32_t code, bool pressed,
                                           mc_player_input_error *error);
/* Actual dc.o_()V: read dc.a at1, invoke dynamic uo/lr.a()V at4. Existing
 * source public fields stay unchanged; only the genuine options key cells reset. */
mc_player_status mc_local_player_input_reset(mc_entity *player, mc_player_input_error *error);
/* Actual dc.t()Z: read input at1, input.e at4, and only if true read Player.u
 * at11. It returns input.sneak && !sleeping, not watcher bit1 or a key-cell
 * value. NULL Player/input preserve output and report reached sites1/4. */
mc_player_status mc_local_player_is_sneaking(mc_entity *player, bool *out,
                                             mc_player_input_error *error);

#endif
