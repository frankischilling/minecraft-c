#ifndef MC_ENTITY_PLAYER_CLOSE_H
#define MC_ENTITY_PLAYER_CLOSE_H

#include "mc/entity/player/player.h"

/* Synchronous whole actual DC.r()V child. Diagnostics contain only scalar
 * values and static text; no GUI types, retained producer views or child
 * ownership crosses this boundary. The host retains its own interrupted
 * source prefix until its independently checked native retirement. */
typedef struct mc_player_close_error {
    mc_entity_status status;
    const char *operation, *exception_class;
    mc_player_site site, parent_site;
    bool native_policy, source_exception, source_error, source_returned;
} mc_player_close_error;

typedef struct mc_player_close_host {
    void *context;
    mc_entity_status (*close)(void *, mc_entity *, mc_player_close_error *);
} mc_player_close_host;

#endif
