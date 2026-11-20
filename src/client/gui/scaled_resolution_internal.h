#ifndef MC_CLIENT_GUI_SCALED_RESOLUTION_INTERNAL_H
#define MC_CLIENT_GUI_SCALED_RESOLUTION_INTERNAL_H
#include "mc/client/gui/scaled_resolution.h"
#include "mc/core/java/java.h"
#include <math.h>

/* Exact arithmetic tails. Positive dimensions/live outputs are
 * admitted by the caller; no allocation, holds or source fields. */
static inline int32_t mc_scaled_resolution_source_factor(int32_t display_width,
                                                         int32_t display_height,
                                                         int32_t gui_scale) {
    int32_t factor = 1;
    const int32_t limit = gui_scale == 0 ? 1000 : gui_scale;
    /* qq.<init>: limit35, width44, height61, increment78. A successful
     * increment requires factor <= INT32_MAX/320, so factor+1 cannot overflow
     * in this positive physical-dimension domain. */
    while (factor < limit && display_width / (factor + 1) >= 320 &&
           display_height / (factor + 1) >= 240)
        ++factor;

    return factor;
}

static inline void mc_scaled_resolution_source_finish(int32_t display_width, int32_t display_height,
                                                      int32_t factor, mc_scaled_resolution *out) {
    mc_scaled_resolution result = {0};
    result.scale_factor = factor;
    result.width_exact = (double)display_width / (double)factor;
    result.height_exact = (double)display_height / (double)factor;
    /* qq.<init>126/129 and138/141: Math.ceil followed by Java d2i. */
    result.width = mc_java_double_to_i32(ceil(result.width_exact));
    result.height = mc_java_double_to_i32(ceil(result.height_exact));
    *out = result;
}
#endif
