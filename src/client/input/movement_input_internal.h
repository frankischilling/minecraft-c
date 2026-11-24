#ifndef MC_CLIENT_INPUT_MOVEMENT_INPUT_INTERNAL_H
#define MC_CLIENT_INPUT_MOVEMENT_INPUT_INTERNAL_H

#include "mc/client/input/movement_input.h"

/* Private owner state. holds is native lifetime policy, not Java object data. */
struct mc_movement_input {
    mc_movement_input_kind kind;
    size_t holds;
    mc_movement_input_fields fields;
    bool *keys;
    mc_game_settings *settings;
};

#endif
