#ifndef MC_CLIENT_DISPLAY_SDK_CONTROLLERS_INTERNAL_H
#define MC_CLIENT_DISPLAY_SDK_CONTROLLERS_INTERNAL_H
#include "sdk_input_internal.h"

/* Actual returned Controller[] and temporary filtering/cursor owners. This
 * increment publishes no element unless its genuine target body exists. */
typedef struct mc_client_sdk_controller_array {
    size_t count;
    void **entries;
} mc_client_sdk_controller_array;

typedef struct mc_client_sdk_controller_iterator {
    mc_client_sdk_empty_list *list;
    size_t index;
} mc_client_sdk_controller_iterator;

typedef struct mc_client_sdk_controller_attempt {
    mc_client_sdk_controller_array *returned;
    mc_client_sdk_empty_list *filtered;
    mc_client_sdk_controller_iterator *iterator;
} mc_client_sdk_controller_attempt;

mc_client_sdk_input_status mc_client_sdk_controllers_native_cleanup(mc_client_sdk_input *,
                                                                    mc_client_sdk_input_error *);
#endif
