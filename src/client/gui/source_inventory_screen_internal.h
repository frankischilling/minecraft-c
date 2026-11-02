#ifndef MC_CLIENT_GUI_SOURCE_INVENTORY_SCREEN_INTERNAL_H
#define MC_CLIENT_GUI_SOURCE_INVENTORY_SCREEN_INTERNAL_H

#include "mc/client/controller/singleplayer.h"
#include "source_inventory_screen.h"
#include "source_screen_internal.h"

typedef struct mc_inventory_screen_call {
    mc_player_container_call *call;
    mc_player_container_error failure, cleanup;
    mc_client_singleplayer_controller *controller;
    bool returned;
} mc_inventory_screen_call;

struct mc_client_source_ue {
    mc_client_source_da base;
    mc_client_screen_id_fields id;
    mc_client_screen_ue_fields fields;
    mc_entity *constructor_player;
    mc_container *captured_container;
    bool player_held, container_held;
    unsigned int last_pc;
    size_t source_calls;
    mc_inventory_screen_call *calls;
    mc_client_inventory_screen_error detail;
};

bool mc_screen_inventory_classes_cleanup_locked(mc_client_screen_classes *,
                                                mc_client_screen_error *);
bool mc_screen_inventory_active_locked(mc_client_screen_classes *, mc_client_render_classes *,
                                       mc_client_screen_error *);
bool mc_screen_inventory_initialize_locked(mc_client_source_ue *, mc_client_screen_error *);
bool mc_screen_inventory_cleanup_locked(mc_client_source_ue *, mc_client_screen_error *);
/* Terminal native prefix only. End admission before releasing borrowed source
 * identities; the screen and scalar failure receipt remain until retirement. */
bool mc_screen_inventory_terminal_prepare_locked(mc_client_source_ue *, mc_client_screen_error *);
bool mc_screen_inventory_closed_locked(mc_client_source_ue *, mc_client_screen_error *);
/* Return the actual captured DC receiver when ID reaches r(). The enclosing
 * event/tick owns the monitor release, synchronous router and reacquisition. */
bool mc_screen_inventory_key_locked(mc_client_source_ue *, int32_t, mc_entity **,
                                    mc_client_screen_error *);
bool mc_screen_inventory_tick_locked(mc_client_source_ue *, mc_entity **, mc_client_screen_error *);
bool mc_screen_inventory_mouse_after_base_locked(mc_client_source_ue *, int32_t, int32_t, int32_t,
                                                 mc_client_sdk_input *, mc_client_screen_error *);

#endif
