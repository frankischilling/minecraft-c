#ifndef MC_CLIENT_GUI_SOURCE_FURNACE_DRAWING_H
#define MC_CLIENT_GUI_SOURCE_FURNACE_DRAWING_H

#include "source_furnace_screen.h"
#include "source_inventory_drawing.h"

/* Actual inherited ID.a(IIF) on OV, with OV background/foreground overrides.
 * The existing Inventory draw owner/error/retirement API also owns this call.
 * It keeps the same App/Container/Font/Engine/graphics graph live/exclusive;
 * no UE preview, UE mouse stores, input, projection, swap or unwind is added.
 * Successful state.receiver is NULL, screen_receiver is the actual OV's DA,
 * furnace/id_returned are true and the source return is ID.a490. */
mc_client_screen_status mc_client_source_furnace_draw_begin(mc_client_source_ov *,
                                                            mc_gui_mouse_position,
                                                            float partial_tick,
                                                            const mc_client_manager_draw_services *,
                                                            mc_client_inventory_draw_call **,
                                                            mc_client_inventory_draw_error *);

#endif
