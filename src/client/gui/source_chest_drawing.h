#ifndef MC_CLIENT_GUI_SOURCE_CHEST_DRAWING_H
#define MC_CLIENT_GUI_SOURCE_CHEST_DRAWING_H

#include "source_chest_screen.h"
#include "source_inventory_drawing.h"

/* Actual inherited ID.a(IIF), with HP's two-part background and separate
 * Inventory receiver labels. The common draw owner retains all captured
 * Players/Stacks/graphics. No UE preview or mouse-position stores are added. */
mc_client_screen_status mc_client_source_chest_draw_begin(mc_client_source_hp *,
                                                          mc_gui_mouse_position, float,
                                                          const mc_client_manager_draw_services *,
                                                          mc_client_inventory_draw_call **,
                                                          mc_client_inventory_draw_error *);

#endif
