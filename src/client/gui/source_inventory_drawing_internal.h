#ifndef MC_CLIENT_GUI_SOURCE_INVENTORY_DRAWING_INTERNAL_H
#define MC_CLIENT_GUI_SOURCE_INVENTORY_DRAWING_INTERNAL_H

#include "client/application_state.h"
#include "client/display/camera_surface.h"
#include "client/display/fog_surface.h"
#include "client/display/source_surface.h"
#include "client/render/manager/internal.h"
#include "entity/living/fields_internal.h"
#include "entity/player/player_internal.h"
#include "entity/player/tick_math_internal.h"
#include "inventory/player/source_internal.h"
#include "mc/core/java/java.h"
#include "source_inventory_drawing.h"
#include "source_inventory_screen_internal.h"

struct mc_client_inventory_draw_call {
    mc_client_inventory_draw_state state;
    mc_client_inventory_draw_error error, cleanup;
    mc_client_manager_draw_services services;
    mc_gui_mouse_position mouse;
    float partial;
    mc_client_source_id *owner;
    const char *id_descriptor;
    bool screen_entered;
    mc_entity *players[24];
    size_t player_count;
    mc_item_stack *stacks[64];
    size_t stack_count;
    mc_client_render_manager *store_manager;
    mc_client_render_manager_draw_call *preview;
    mc_client_manager_draw_state preview_state;
    mc_client_item_gui_call *item;
    mc_client_render_engine *pending_lookup;
    mc_item_name_workspace names;
};

bool mc_inventory_draw_fail(mc_client_inventory_draw_call *, mc_client_screen_status, const char *,
                            const char *, const char *, const char *, unsigned int, bool);
const char *mc_inventory_draw_descriptor(const mc_client_inventory_draw_call *, const char *);
bool mc_inventory_draw_gl(mc_client_inventory_draw_call *, bool, const char *, unsigned int);
bool mc_inventory_draw_fp(mc_client_inventory_draw_call *, const char *, unsigned int);
mc_entity *mc_inventory_draw_player(mc_client_inventory_draw_call *, const char *, unsigned int);
bool mc_inventory_draw_player_capture(mc_client_inventory_draw_call *, const char *, unsigned int,
                                      mc_entity **);
bool mc_inventory_draw_texture(mc_client_inventory_draw_call *, mc_client_render_engine *,
                               const char *, const char *, unsigned int, mc_client_texture *);
bool mc_inventory_draw_gradient(mc_client_inventory_draw_call *, int32_t, int32_t, int32_t, int32_t,
                                uint32_t, uint32_t);
bool mc_inventory_draw_tile(mc_client_inventory_draw_call *, int32_t, int32_t, int32_t, int32_t,
                            int32_t, int32_t);
bool mc_inventory_draw_background(mc_client_inventory_draw_call *);
bool mc_workbench_draw_background(mc_client_inventory_draw_call *);
bool mc_workbench_draw_foreground(mc_client_inventory_draw_call *);
mc_client_screen_status mc_inventory_draw_begin_id(mc_client_source_id *, mc_client_screen_kind,
                                                   mc_gui_mouse_position, float,
                                                   const mc_client_manager_draw_services *,
                                                   mc_client_inventory_draw_call **,
                                                   mc_client_inventory_draw_error *);

#endif
