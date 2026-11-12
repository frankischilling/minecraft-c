#ifndef MC_CLIENT_GUI_SCALED_RESOLUTION_H
#define MC_CLIENT_GUI_SCALED_RESOLUTION_H

#include <stdbool.h>
#include <stdint.h>

typedef struct mc_scaled_resolution {
    int32_t width, height;
    int32_t scale_factor;
    double width_exact, height_exact;
} mc_scaled_resolution;

typedef struct mc_gui_mouse_position {
    int32_t x, y;
} mc_gui_mouse_position;

/* qq(GameSettings,int,int): positive physical dimensions, with any Java int
 * guiScale. Zero selects the Auto limit of 1000; negative settings keep factor
 * one. Exact binary64 dimensions serve projection; integer dimensions are their
 * ceilings. Uses the project's strict binary64 floating environment.
 * No allocation or global state. Invalid arguments leave *out unchanged. */
bool mc_scaled_resolution_init(int32_t display_width, int32_t display_height, int32_t gui_scale,
                               mc_scaled_resolution *out);

/* da.f(): map actual LWJGL bottom-origin event pixels using the screen's integer
 * width/height and the current physical dimensions. Event coordinates may be
 * any Java ints: multiplication/subtraction wrap and division truncates toward
 * zero. Coordinates are not clamped or converted from a native top-origin API.
 * Only resolution width/height are read; both and the display dimensions must
 * be positive. Output must be disjoint from resolution. Invalid arguments leave
 * *out unchanged. No allocation, global state or platform input calls. */
bool mc_scaled_resolution_map_mouse(const mc_scaled_resolution *resolution, int32_t display_width,
                                    int32_t display_height, int32_t event_x, int32_t event_y,
                                    mc_gui_mouse_position *out);

#endif
