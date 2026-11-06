#ifndef MC_CLIENT_CONTROLLER_SINGLEPLAYER_INTERNAL_H
#define MC_CLIENT_CONTROLLER_SINGLEPLAYER_INTERNAL_H

#include "mc/client/controller/singleplayer.h"

struct mc_client_singleplayer_controller {
    mc_entity_classes *classes;
    mc_java_random_profile *profile;
    mc_singleplayer_controller_snapshot fields;
};

typedef struct mc_singleplayer_controller_terminal_view {
    client_application *app;
    mc_entity_classes *classes;
    mc_java_random_profile *profile;
    mc_entity *const *failed_player_slot;
    size_t holds;
} mc_singleplayer_controller_terminal_view;

/* Passive native inspection of this exact READY owner. No hold allowance is
 * inferred: the enclosing owner must account for every reported occurrence.
 * The returned slot is the actual owning failed-candidate field, including
 * when its value is NULL; keep the controller live and exclusive through use.
 * Its view expires on the next mutation or retirement. No App/class/source
 * work, retain/release or diagnostic-owner mutation occurs. out is required;
 * error is optional. Both must be live and disjoint from each other and c.
 * Failure preserves *out. An entered owner is BUSY; other phases are refused. */
mc_singleplayer_controller_status
mc_client_singleplayer_controller_terminal_inspect(const mc_client_singleplayer_controller *c,
                                                   mc_singleplayer_controller_terminal_view *out,
                                                   mc_singleplayer_controller_error *error);

#endif
