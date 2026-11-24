#ifndef MC_CLIENT_RENDER_ENGINE_COLOR_ARRAY_INTERNAL_H
#define MC_CLIENT_RENDER_ENGINE_COLOR_ARRAY_INTERNAL_H
#include "color_array.h"

/* The actual native array owner. Tests may declare an active-call guard fixture
 * on this real storage; they must not copy a layout or fabricate an array. */
struct mc_client_color_array {
    size_t holds, active_calls, length, allocations, bytes;
    uint32_t *bits;
    mc_client_color_array_phase phase;
    mc_client_color_array_error failure;
};
#endif
