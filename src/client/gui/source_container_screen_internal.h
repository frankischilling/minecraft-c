#ifndef MC_CLIENT_GUI_SOURCE_CONTAINER_SCREEN_INTERNAL_H
#define MC_CLIENT_GUI_SOURCE_CONTAINER_SCREEN_INTERNAL_H

#include "mc/client/controller/singleplayer.h"
#include "source_inventory_screen.h"
#include "source_screen_internal.h"

typedef struct mc_inventory_screen_call {
    struct mc_inventory_screen_call *next;
    mc_player_container_call *call;
    mc_player_container_error failure, cleanup;
    mc_client_singleplayer_controller *controller;
    bool returned;
} mc_inventory_screen_call;

typedef struct mc_screen_id_player_hold {
    struct mc_screen_id_player_hold *next;
    mc_entity *player;
} mc_screen_id_player_hold;

/* The actual inherited ID subobject, shared physically by UE and OO. Only
 * the concrete UE owns mouse-position fields/Player preview state. */
typedef struct mc_client_source_id {
    mc_client_source_da base;
    mc_client_screen_id_fields id;
    mc_entity *constructor_player;
    mc_container *captured_container;
    bool player_held, container_held, container_owned;
    unsigned int last_pc;
    size_t source_calls;
    mc_inventory_screen_call *calls;
    mc_inventory_screen_call *closed_parent, *closed_direct;
    bool inherited_closed_returned, direct_closed_returned;
    mc_inventory_player *constructor_inventory;
    mc_world_runtime *constructor_world;
    bool world_held;
    mc_client_inventory_screen_error detail;
    mc_inventory_screen_call *spare_calls; /* Empty checked-retired bounded records. */
    mc_screen_id_player_hold *published_players;
} mc_client_source_id;

bool mc_screen_inventory_classes_cleanup_locked(mc_client_screen_classes *,
                                                mc_client_screen_error *);
bool mc_screen_inventory_active_locked(mc_client_screen_classes *, mc_client_render_classes *,
                                       mc_client_screen_error *);

bool mc_screen_container_construct_locked(mc_client_source_id *, mc_container *,
                                          mc_client_screen_error *);
bool mc_screen_container_initialize_locked(mc_client_source_id *, mc_client_screen_error *);
bool mc_screen_container_cleanup_locked(mc_client_source_id *, mc_client_screen_error *);
bool mc_screen_container_retirement_admit_locked(mc_client_source_id *, mc_client_screen_error *);
bool mc_screen_container_terminal_prepare_locked(mc_client_source_id *, mc_client_screen_error *);
bool mc_screen_container_closed_locked(mc_client_source_id *, mc_client_screen_error *);
bool mc_screen_container_key_locked(mc_client_source_id *, int32_t, mc_entity **,
                                    mc_client_screen_error *);
bool mc_screen_container_tick_locked(mc_client_source_id *, mc_entity **, mc_client_screen_error *);
bool mc_screen_container_mouse_after_base_locked(mc_client_source_id *, int32_t, int32_t, int32_t,
                                                 mc_client_sdk_input *, mc_client_screen_error *);
/* OO.h's second, direct virtual close. The caller first completes ID.h and
 * then freshly evaluates its own j/App.h operands. */
bool mc_screen_container_direct_close_locked(mc_client_source_id *, mc_client_screen_error *);

#endif
