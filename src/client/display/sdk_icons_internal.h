#ifndef MC_CLIENT_DISPLAY_SDK_ICONS_INTERNAL_H
#define MC_CLIENT_DISPLAY_SDK_ICONS_INTERNAL_H

#include "sdk_icons.h"

struct mc_client_sdk_icons {
    const mc_registry_resources *source;
    mc_java_properties_input stream;
    size_t holds;
    mc_client_sdk_icons_phase phase;
    bool active, stream_open;
    uint8_t rgba16[16 * 16 * 4], rgba32[32 * 32 * 4];
    mc_client_sdk_icons_error failure;
};

mc_client_sdk_icons_status mc_client_sdk_icons_fail(mc_client_sdk_icons_error *,
                                                    mc_client_sdk_icons_status, const char *);

#endif
