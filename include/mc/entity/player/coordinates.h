#ifndef MC_ENTITY_PLAYER_COORDINATES_H
#define MC_ENTITY_PLAYER_COORDINATES_H

#include "mc/entity/player/player.h"

typedef struct mc_player_coordinate_pin mc_player_coordinate_pin;

/* Actual client GS.Q()BR: read the nullable saved-spawn field once, preserving
 * its identity. NULL returns a NULL pin. A nonnull result owns a native Player
 * hold and a pin on that actual coordinate node until release, including when
 * later source callbacks replace the field or collect old coordinate nodes.
 * The output must initially be NULL. Unowned/foreign field pointers, native
 * hold overflow and the existing Entity allocation ceiling fail explicitly;
 * these native policies follow the source field read, without dereferencing a
 * foreign coordinate. RAW and failed canonical LocalPlayers are supported. */
mc_player_status mc_player_saved_spawn_get(mc_entity *player, mc_player_coordinate_pin **out,
                                           mc_player_error *error);
const mc_player_coordinates *mc_player_coordinate_pin_value(const mc_player_coordinate_pin *pin);
void mc_player_coordinate_pin_release(mc_player_coordinate_pin **pin);

/* Actual client GS.a(BR)V: a nonnull argument is copied to a distinct BR before
 * publishing private_b at13; NULL stores NULL at21. The caller keeps a nonnull
 * argument live through this synchronous call. Values may originate in another
 * Player or an owned World result; this does not retain the argument identity.
 * Allocation failure preserves the old field and every existing pin/alias. */
mc_player_status mc_player_saved_spawn_set(mc_entity *player, const mc_player_coordinates *value,
                                           mc_player_error *error);

#endif
