#ifndef MC_CLIENT_GUI_SOURCE_DEATH_DRAWING_H
#define MC_CLIENT_GUI_SOURCE_DEATH_DRAWING_H

#include "source_death_screen.h"
#include "source_screen_drawing.h"

typedef struct mc_client_death_draw_error {
    mc_client_screen_error screen;
    mc_client_bitmap_font_error font;
    mc_gui_main_menu_error renderer;
    size_t font_measurements, buttons_drawn;
    int32_t score;
    bool background_returned, title_drawn, score_read, score_drawn, source_returned, monitor_lost;
    mc_client_source_da *pending_receiver;
} mc_client_death_draw_error;

/* Genuine CH.a(IIF)V, using the same App/Font/default-Pack canvas. Original
 * gradient, doubled title, live Player score and inherited Button loop retain
 * their order. No tick, event, capture or buffer swap is inserted. The exclusive
 * graph stays live through checked retirement after a monitor-recovery failure. */
bool mc_client_source_death_draw(mc_client_source_ch *, mc_gui_main_menu *,
                                 const mc_scaled_resolution *, mc_gui_mouse_position,
                                 float partial_tick, mc_client_death_draw_error *);
mc_client_screen_status mc_client_source_death_draw_retire(mc_client_death_draw_error *);

#endif
