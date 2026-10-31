#ifndef MC_CLIENT_GUI_SOURCE_SCREEN_DRAWING_H
#define MC_CLIENT_GUI_SOURCE_SCREEN_DRAWING_H

#include "gui_main_menu.h"
#include "source_screen_initialize.h"

typedef enum mc_client_source_screen_draw_status {
    MC_CLIENT_SOURCE_SCREEN_DRAW_OK,
    MC_CLIENT_SOURCE_SCREEN_DRAW_ARGUMENT,
    MC_CLIENT_SOURCE_SCREEN_DRAW_UNSUPPORTED,
    MC_CLIENT_SOURCE_SCREEN_DRAW_OWNER_FAILED,
    MC_CLIENT_SOURCE_SCREEN_DRAW_FONT_FAILED,
    MC_CLIENT_SOURCE_SCREEN_DRAW_RENDER_FAILED
} mc_client_source_screen_draw_status;

typedef struct mc_client_source_screen_draw_error {
    mc_client_source_screen_draw_status status;
    const char *operation;
    size_t button_count, font_measurements;
    mc_client_screen_error screen;
    mc_client_bitmap_font_error font;
    mc_gui_main_menu_error renderer;
} mc_client_source_screen_draw_error;

/* Draw the actual initialized FU/DA owner through the existing OpenGL3.3 menu
 * kernels. No replacement screen, button, label, splash, Random or date state
 * is constructed. Temporary frame records are borrowed views of the actual
 * source fields. The captured canonical da.g Font measures every reached
 * nonnull text; disagreement with native glyph widths stops the draw.
 *
 * This finite bridge requires the same Display/resource-provider/Registry and
 * anaglyph setting as the renderer, captured da.g equal to current App.q,
 * a completed initialized FU, no World and
 * the selected default texture pack. Other screen classes/pack/background
 * profiles remain explicit dependencies. Source c/d provide integer geometry;
 * caller projection exacts provide the current frame's orthographic extents.
 * No tick, input dispatch, swap, capture, sleep or menu action is added.
 *
 * Caller owns and exclusively borrows screen, App, canonical Font, renderer,
 * projection and resource namespaces through the whole call. Clock/resource
 * callbacks must not mutate/reenter/retire them. Error is mandatory, live and
 * disjoint; retain it on the heap when embedding this call in larger owners.
 * Drawing failures preserve reached backend resource/GL effects, not rollback. */
bool mc_client_source_screen_draw(mc_client_source_da *, mc_gui_main_menu *,
                                  const mc_scaled_resolution *projection,
                                  mc_gui_mouse_position mouse, float partial_tick,
                                  mc_gui_main_menu_millis_fn, void *clock_context,
                                  mc_client_source_screen_draw_error *);

#endif
