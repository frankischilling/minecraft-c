#ifndef MC_CLIENT_GUI_SOURCE_INVENTORY_DRAWING_H
#define MC_CLIENT_GUI_SOURCE_INVENTORY_DRAWING_H

#include "client/render/item_gui.h"
#include "client/render/item_overlay.h"
#include "client/render_engine/lighting.h"
#include "item/render/name.h"
#include "mc/client/gui/scaled_resolution.h"
#include "mc/client/render/manager/draw.h"
#include "source_inventory_screen.h"

typedef struct mc_client_inventory_draw_call mc_client_inventory_draw_call;

typedef struct mc_client_inventory_draw_error {
    mc_client_screen_error screen;
    mc_client_display_error display;
    mc_client_graphics_error graphics;
    mc_client_lighting_error lighting;
    mc_client_texture_lookup_error texture;
    mc_client_item_gui_error item;
    mc_client_item_overlay_context overlay;
    mc_client_manager_error manager;
    mc_client_manager_draw_error player;
    mc_container_interaction_error slot;
    mc_inventory_error inventory;
    mc_client_bitmap_font_error font;
    mc_item_render_error name;
    mc_entity_status entity;
    mc_item_stack_status stack;
} mc_client_inventory_draw_error;

typedef struct mc_client_inventory_draw_state {
    bool active, returned, background_returned, preview_returned, pose_restored;
    bool foreground_returned, cursor_drawn, tooltip_drawn, mouse_stored;
    int32_t origin_x, origin_y, hovered_slot;
    size_t slots_drawn, item_draws, overlay_draws, font_measurements;
    unsigned int source_pc;
    mc_client_source_ue *receiver;
    mc_client_source_da *screen_receiver;
    bool workbench, id_returned;
    const mc_client_manager_draw_state *preview;
    const mc_client_inventory_draw_error *failure;
} mc_client_inventory_draw_state;

/* UE.a(IIF), including ID.a and the full TH/DS Player preview. The existing
 * PX overlay projection must already be installed on the actual Display.
 * Services, App and its complete source graph stay live/exclusive through
 * checked retirement. A native invocation owns the exact screen receiver,
 * captured Players/Stacks/TH and failed child work. No input, frame setup,
 * swap, source GL unwind or failed-pose restoration is inserted. out starts
 * NULL; once allocated it is returned on source/native failure too. */
mc_client_screen_status
mc_client_source_inventory_draw_begin(mc_client_source_ue *, mc_gui_mouse_position,
                                      float partial_tick, const mc_client_manager_draw_services *,
                                      mc_client_inventory_draw_call **,
                                      mc_client_inventory_draw_error *);
bool mc_client_source_inventory_draw_inspect(const mc_client_inventory_draw_call *,
                                             mc_client_inventory_draw_state *);
mc_client_screen_status mc_client_source_inventory_draw_retire(mc_client_inventory_draw_call **,
                                                               mc_client_inventory_draw_error *);

#endif
