#ifndef MC_CLIENT_CONTROLLER_ATTACK_H
#define MC_CLIENT_CONTROLLER_ATTACK_H

#include "mc/client/controller/singleplayer.h"
#include "mc/entity/player/attack.h"

/* Actual inherited OB.b(GS,SN): forward the same nullable target to GS.d2.
 * No controller/App field is read. The caller keeps the genuine controller
 * live/exclusive. A returned Player call owns its effects and must be retired
 * through mc_local_player_attack_retire, including failed prefixes. */
mc_entity_status mc_client_singleplayer_attack_begin(mc_client_singleplayer_controller *,
                                                     mc_registry_classes *, mc_math_classes *,
                                                     mc_entity *player, mc_entity *target,
                                                     mc_player_attack_call **out,
                                                     mc_player_attack_error *error);

#endif
