#ifndef MC_CLIENT_GUI_SOURCE_PAUSE_DRAWING_H
#define MC_CLIENT_GUI_SOURCE_PAUSE_DRAWING_H

#include "mc/core/math/classes.h"
#include "mc/world/runtime/saving/saving.h"
#include "source_pause_screen.h"
#include "source_screen_drawing.h"

typedef struct mc_client_pause_draw_error {
    mc_client_screen_error screen;
    mc_client_bitmap_font_error font;
    mc_gui_main_menu_error renderer;
    mc_world_runtime_lifecycle_error save;
    mc_math_class_error math;
    int32_t pass;
    bool background_returned, counter_stored, save_returned, save_completed;
    bool saving_label_drawn, source_returned, monitor_lost;
    uint32_t saving_color;
    size_t buttons_drawn, font_measurements;
    mc_client_source_da *pending_receiver;
} mc_client_pause_draw_error;

/* Actual oz.a(IIF): background i1, actual World.a(pass++)22, optional saving
 * pulse/text120, title140, inherited button draw147. The GL3.3 bridge only
 * supplies the existing quad/glyph/Button kernels; it never draws a substitute
 * World, skips saving or inserts FU decorations. Actual Save metadata/Chunk
 * effects survive failure. World must be a disposable copy for write tests.
 *
 * Caller keeps the screen/App/World/Math/Font/default-Pack graph live/exclusive
 * through draw and checked monitor recovery. No implicit input, tick, swap or
 * capture. Error is required, disjoint and preferably embedded on the heap.
 * Its native pending receiver remains live until draw_retire succeeds after
 * a failed final class-monitor reacquisition. No source operation is replayed. */
bool mc_client_source_pause_draw(mc_client_source_oz *, mc_gui_main_menu *,
                                 const mc_scaled_resolution *, mc_gui_mouse_position,
                                 float partial_tick, mc_math_classes *,
                                 mc_client_pause_draw_error *);
mc_client_screen_status mc_client_source_pause_draw_retire(mc_client_pause_draw_error *);

#endif
