#ifndef MC_CLIENT_DISPLAY_SDK_CONTROLLERS_NATIVE_H
#define MC_CLIENT_DISPLAY_SDK_CONTROLLERS_NATIVE_H
#include "sdk_input.h"

/* Concrete unique native provider. No completed Controller/Component is
 * supplied by callback, and no Java plugin/exception completion is implied. */
typedef struct mc_client_sdk_controller_environment {
    mc_client_sdk_controller_environment_phase phase;
    bool active, reference_owned, initialization_attempted, enumeration_returned;
    bool before_drop_retained, after_drop_retained;
    uint32_t *ids;
    int count;
    size_t init_calls, enumeration_calls, quit_calls;
    mc_client_sdk_input_error failure;
} mc_client_sdk_controller_environment;

uint32_t mc_client_sdk_controller_native_backends(void);
mc_client_sdk_input_status
mc_client_sdk_controller_environment_allocate(mc_client_sdk_controller_environment **,
                                              mc_client_sdk_input_error *);
mc_client_sdk_input_status
mc_client_sdk_controller_environment_discover(mc_client_sdk_controller_environment *,
                                              mc_client_sdk_input_error *);
/* Cold release needs no port. Any attempted activation requires the actual
 * live sole event owner through both retention steps. BUSY/failure preserves
 * this occurrence and completed drop progress; a successful drop consumes it. */
mc_client_sdk_input_status
mc_client_sdk_controller_environment_retire(mc_client_sdk_controller_environment **,
                                            mc_client_event_port *, mc_client_sdk_input_error *);
#endif
