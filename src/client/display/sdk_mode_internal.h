#ifndef MC_CLIENT_DISPLAY_SDK_MODE_INTERNAL_H
#define MC_CLIENT_DISPLAY_SDK_MODE_INTERNAL_H

#include "staged.h"

struct mc_client_display_mode {
    size_t holds;
    bool constructed, fullscreen_capable;
    int32_t width, height, bits_per_pixel, frequency, last_store_pc;
};

bool mc_client_display_mode_platform(int32_t, int32_t, int32_t, int32_t, mc_client_display_mode **,
                                     mc_client_display_stage_error *);

#endif
