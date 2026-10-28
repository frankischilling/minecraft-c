#ifndef MC_CLIENT_STARTUP_APPLICATION_INPUT_H
#define MC_CLIENT_STARTUP_APPLICATION_INPUT_H

#include "../display/sdk_input.h"
#include "../input/mouse_helper.h"

typedef struct mc_client_startup_input_call mc_client_startup_input_call;

/* Actual C is the sole published field. The candidate is the native counted
 * occurrence, and the remaining state is passive progress/error metadata. */
struct mc_client_startup_input_call {
    bool active, keyboard_call_completed, mouse_call_completed, C_stored, retired;
    bool controllers_call_completed;
    unsigned int last_pc;
    mc_client_source_vy *candidate;
    mc_client_sdk_input_error input;
    mc_client_mouse_helper_error mouse_helper, retirement;
};

#endif
