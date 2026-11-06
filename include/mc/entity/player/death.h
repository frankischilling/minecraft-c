#ifndef MC_ENTITY_PLAYER_DEATH_H
#define MC_ENTITY_PLAYER_DEATH_H

#include "mc/entity/player/player.h"

struct mc_player_drop_error;
struct mc_player_container_error;

typedef struct mc_player_death_state {
    mc_entity_status status, cleanup_status;
    const char *operation, *cleanup_operation, *exception_class;
    mc_player_site site, child;
    bool entered, returned, pending, active, closing;
    bool native_policy, source_exception, source_error;
    mc_entity *receiver;
    uintptr_t receiver_identity, source_identity;
    bool source_held, living_returned, ah_stored, remote, remote_read;
    bool score_returned, kill_returned, size_stored, position_returned;
    bool apple_allocated, apple_returned, inventory_entered, inventory_returned;
    bool statistic_returned, inventory_armor;
    int32_t score, inventory_slot;
    uint16_t living_pc, inventory_pc;
    size_t drops_returned, drops_retired, slots_cleared, close_calls, closes_returned;
    /* Borrow the exact pending producer until checked cleanup succeeds. */
    const struct mc_player_drop_error *drop_error;
    const struct mc_player_container_error *close_error;
} mc_player_death_state;

/* Passive view of actual Player-owned pending death/K work. It owns no hold;
 * all borrowed child views expire with successful effect cleanup. */
bool mc_player_death_inspect(const mc_entity *, mc_player_death_state *);

#endif
