#ifndef MC_CLIENT_GUI_SOURCE_CREATE_WORLD_DRAWING_H
#define MC_CLIENT_GUI_SOURCE_CREATE_WORLD_DRAWING_H
#include "source_create_world.h"
#include "source_screen_drawing.h"

typedef struct mc_client_create_world_draw_error {
    mc_client_screen_error screen;
    mc_client_bitmap_font_error font;
    mc_gui_main_menu_error renderer;
    mc_client_display_error display;
    mc_registry_error registry;
    mc_player_string *strings[3];
    mc_client_source_da *pending_receiver;
    size_t font_measurements, labels_drawn, fields_drawn, buttons_drawn, rectangles_drawn;
    bool active, monitor_lost, source_returned;
} mc_client_create_world_draw_error;

/* FJ.a(IIF) and RO.c use the actual screen/RO Font, UTF16 labels and cursor.
 * The existing GL3.3 canvas supplies background, rectangles and button kernels;
 * source Font shadow calls draw all FJ/RO captions. Start error storage zeroed,
 * and retire a failed call before reuse or ending its screen/App dependencies.
 * Completed GL/source effects survive failure and are never replayed. */
bool mc_client_source_create_world_draw(mc_client_source_fj *, mc_gui_main_menu *,
                                        const mc_scaled_resolution *, mc_gui_mouse_position, float,
                                        mc_client_create_world_draw_error *);
mc_client_screen_status
mc_client_source_create_world_draw_retire(mc_client_create_world_draw_error *);
#endif
