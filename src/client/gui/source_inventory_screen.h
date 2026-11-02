#ifndef MC_CLIENT_GUI_SOURCE_INVENTORY_SCREEN_H
#define MC_CLIENT_GUI_SOURCE_INVENTORY_SCREEN_H

#include "../../entity/player/container_interaction.h"
#include "../display/sdk_input.h"
#include "mc/client/render/model.h"
#include "mc/client/settings/game_settings.h"
#include "source_screen.h"

typedef struct mc_client_source_ue mc_client_source_ue;

struct mc_client_inventory_screen_error {
    mc_client_model_error model;
    mc_player_statistic_error statistic;
    mc_player_container_error container;
    mc_player_container_error cleanup_container;
    mc_player_container_error inherited_close, direct_close;
    mc_inventory_error ownership;
    mc_inventory_error cleanup_ownership;
    mc_client_model_status cleanup_model;
    mc_game_settings_error settings;
    mc_client_sdk_input_error input;
    mc_entity_status entity;
    mc_client_screen_site caller, child;
};

typedef struct mc_client_inventory_screen_state {
    mc_client_screen_phase phase;
    size_t holds, source_calls;
    mc_client_screen_id_fields id;
    mc_client_screen_ue_fields ue;
    mc_entity *constructor_player;
    mc_container *constructor_container;
    bool container_held, player_held;
    unsigned int last_pc;
    const mc_client_screen_error *failure;
    const mc_client_inventory_screen_error *detail;
} mc_client_inventory_screen_state;

typedef struct mc_client_inventory_class_state {
    mc_client_screen_class_phase phase;
    mc_client_item_renderer *renderer, *pending_renderer;
    mc_client_render_classes *models;
    unsigned int last_pc, last_store_pc;
    const mc_client_screen_error *failure;
} mc_client_inventory_class_state;

/* Actual superclass ID active use precedes NEW UE and the caller's Player
 * argument evaluation. The distinct BB and a failed constructor prefix belong
 * to this screen namespace. No Player/Container is created or rebound. */
mc_client_screen_status mc_client_screen_inventory_active_use(mc_client_screen_classes *,
                                                              mc_client_render_classes *,
                                                              mc_client_screen_error *);
mc_client_screen_status mc_client_screen_inventory_allocate(mc_client_screen_classes *,
                                                            mc_client_render_classes *,
                                                            const mc_client_screen_options *,
                                                            mc_client_source_ue **,
                                                            mc_client_screen_error *);
mc_client_screen_status mc_client_screen_inventory_construct(mc_client_source_ue *, mc_entity *,
                                                             mc_client_screen_error *);
mc_client_source_da *mc_client_screen_inventory_base(mc_client_source_ue *);
/* Returned Player/Container views are borrowed until native terminal preparation
 * or retirement. RETIRING inspection hides ID.j after those holds have ended;
 * the actual source fields and copied failure sites remain unchanged. */
mc_client_screen_status mc_client_screen_inventory_inspect(mc_client_source_ue *,
                                                           mc_client_inventory_screen_state *,
                                                           mc_client_screen_error *);
mc_client_screen_status mc_client_screen_inventory_class_inspect(mc_client_screen_classes *,
                                                                 mc_client_inventory_class_state *,
                                                                 mc_client_screen_error *);
/* Borrow the actual ID.l; caller retains the screen namespace through draw. */
mc_client_item_renderer *mc_client_screen_inventory_item_renderer(mc_client_screen_classes *);
mc_client_screen_status mc_client_screen_inventory_release(mc_client_source_ue **,
                                                           mc_client_screen_error *);
mc_client_screen_status mc_client_screen_inventory_retire(mc_client_source_ue **,
                                                          mc_client_screen_error *);

#endif
