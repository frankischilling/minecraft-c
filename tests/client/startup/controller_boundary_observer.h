#ifndef MC_TEST_CONTROLLER_BOUNDARY_OBSERVER_H
#define MC_TEST_CONTROLLER_BOUNDARY_OBSERVER_H

#include "client/display/sdk_input.h"

typedef struct mc_test_controller_boundary_state {
    bool armed, injection_attempted, injected, missing_gl_function;
    size_t create_calls, invalid_enum_calls;
    mc_client_sdk_input_status create_status;
    const mc_client_sdk_input *input;
    const mc_client_display *display;
    char lookup_error[256];
} mc_test_controller_boundary_state;

/* Qualification-only borrowed observation. Parent startup stays active through
 * the real call. Disarm before retiring Input/Display; retain no child owner. */
bool mc_test_controller_boundary_arm(void);
bool mc_test_controller_boundary_inspect(mc_test_controller_boundary_state *);
void mc_test_controller_boundary_disarm(void);

#endif
