#ifndef MC_CLIENT_CONTROLLER_SINGLEPLAYER_INTERNAL_H
#define MC_CLIENT_CONTROLLER_SINGLEPLAYER_INTERNAL_H

#include "mc/client/controller/singleplayer.h"

struct mc_client_singleplayer_controller {
    mc_entity_classes *classes;
    mc_java_random_profile *profile;
    mc_singleplayer_controller_snapshot fields;
};

#endif
