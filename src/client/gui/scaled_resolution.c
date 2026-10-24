#include "mc/client/gui/scaled_resolution.h"

#include "mc/core/java/java.h"

#include <math.h>
#include <stddef.h>

bool mc_scaled_resolution_init(int32_t display_width, int32_t display_height, int32_t gui_scale,
                               mc_scaled_resolution *out) {
    if (out == NULL || display_width <= 0 || display_height <= 0)
        return false;

    int32_t factor = 1;
    const int32_t limit = gui_scale == 0 ? 1000 : gui_scale;
    /* qq.<init>: limit35, width44, height61, increment78. A successful
     * increment requires factor <= INT32_MAX/320, so factor+1 cannot overflow
     * in this positive physical-dimension domain. */
    while (factor < limit && display_width / (factor + 1) >= 320 &&
           display_height / (factor + 1) >= 240)
        ++factor;

    mc_scaled_resolution result = {0};
    result.scale_factor = factor;
    result.width_exact = (double)display_width / (double)factor;
    result.height_exact = (double)display_height / (double)factor;
    /* qq.<init>126/129 and138/141: Math.ceil followed by Java d2i. */
    result.width = mc_java_double_to_i32(ceil(result.width_exact));
    result.height = mc_java_double_to_i32(ceil(result.height_exact));
    *out = result;
    return true;
}

bool mc_scaled_resolution_map_mouse(const mc_scaled_resolution *resolution, int32_t display_width,
                                    int32_t display_height, int32_t event_x, int32_t event_y,
                                    mc_gui_mouse_position *out) {
    if (resolution == NULL || out == NULL || resolution->width <= 0 || resolution->height <= 0 ||
        display_width <= 0 || display_height <= 0)
        return false;

    const int32_t width = resolution->width, height = resolution->height;
    int32_t x, y;
    /* da.f press PCs13/21 and34/42; release PCs66/74 and87/95. Do not widen
     * these products before division: the original imul can overflow. */
    if (!mc_java_i32_div(mc_java_i32_mul(event_x, width), display_width, &x) ||
        !mc_java_i32_div(mc_java_i32_mul(event_y, height), display_height, &y))
        return false;
    const mc_gui_mouse_position result = {x, mc_java_i32_sub(mc_java_i32_sub(height, y), 1)};
    *out = result;
    return true;
}
