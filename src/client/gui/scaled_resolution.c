#include "mc/client/gui/scaled_resolution.h"
#include "scaled_resolution_internal.h"

#include "mc/core/java/java.h"

#include <math.h>
#include <stddef.h>

bool mc_scaled_resolution_init(int32_t display_width, int32_t display_height, int32_t gui_scale,
                               mc_scaled_resolution *out) {
    if (out == NULL || display_width <= 0 || display_height <= 0)
        return false;

    const int32_t factor =
        mc_scaled_resolution_source_factor(display_width, display_height, gui_scale);
    mc_scaled_resolution_source_finish(display_width, display_height, factor, out);
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
