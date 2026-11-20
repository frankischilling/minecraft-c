#ifndef MC_ENTITY_PLAYER_ALLY_ALERT_OCCURRENCES_INTERNAL_H
#define MC_ENTITY_PLAYER_ALLY_ALERT_OCCURRENCES_INTERNAL_H

#include "mc/entity/player/ally_alert.h"

/* Native, nonconsuming validation of every retained occurrence. The caller
 * keeps the actual graph exclusive/live; no source call or release occurs.
 * Optional scratch is capped at 1MiB independently of World source budgets.
 * Its extent/allocation refusal uses the original allocation-free scan, so
 * it never introduces another cleanup refusal or changes source List order. */
bool mc_player_ally_alert_occurrences_valid(const mc_player_ally_alert_state *, bool player_held,
                                            bool target_held);

#endif
