#ifndef MC_CLIENT_GUI_SOURCE_CHEST_SCREEN_H
#define MC_CLIENT_GUI_SOURCE_CHEST_SCREEN_H

#include "mc/inventory/chest.h"
#include "source_inventory_screen.h"

typedef struct mc_client_source_hp mc_client_source_hp;

typedef struct mc_client_chest_screen_state {
    mc_client_screen_phase phase;
    size_t holds, source_calls;
    mc_client_screen_id_fields id;
    mc_client_screen_hp_fields hp;
    mc_inventory_player *constructor_inventory;
    mc_inventory_chest *constructor_chest;
    mc_container *constructor_container;
    bool container_owned, chest_held, inherited_closed_returned;
    unsigned int last_pc;
    const mc_client_screen_error *failure;
    const mc_client_inventory_screen_error *detail;
} mc_client_chest_screen_state;

/* HP owns its actual NEW CD and every failed constructor prefix. Its two
 * nullable LW arguments remain distinct. Native retirement never invokes ID.h
 * or changes Player.e; an owned CD still published there keeps retirement BUSY. */
mc_client_screen_status mc_client_screen_chest_allocate(mc_client_screen_classes *,
                                                        mc_client_render_classes *,
                                                        const mc_client_screen_options *,
                                                        mc_client_source_hp **,
                                                        mc_client_screen_error *);
mc_client_screen_status mc_client_screen_chest_construct(mc_client_source_hp *,
                                                         mc_inventory_player *,
                                                         mc_inventory_chest *,
                                                         mc_client_screen_error *);
mc_client_source_da *mc_client_screen_chest_base(mc_client_source_hp *);
mc_client_screen_status mc_client_screen_chest_inspect(mc_client_source_hp *,
                                                       mc_client_chest_screen_state *,
                                                       mc_client_screen_error *);
mc_client_screen_status mc_client_screen_chest_release(mc_client_source_hp **,
                                                       mc_client_screen_error *);
mc_client_screen_status mc_client_screen_chest_retire(mc_client_source_hp **,
                                                      mc_client_screen_error *);

#endif
