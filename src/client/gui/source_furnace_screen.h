#ifndef MC_CLIENT_GUI_SOURCE_FURNACE_SCREEN_H
#define MC_CLIENT_GUI_SOURCE_FURNACE_SCREEN_H

#include "mc/inventory/furnace.h"
#include "source_inventory_screen.h"

typedef struct mc_client_source_ov mc_client_source_ov;

typedef struct mc_client_furnace_screen_state {
    mc_client_screen_phase phase;
    size_t holds, source_calls;
    mc_client_screen_id_fields id;
    mc_client_screen_ov_fields ov;
    mc_inventory_player *constructor_inventory;
    mc_inventory_furnace *constructor_furnace;
    mc_container *constructor_container;
    bool container_owned, furnace_held, inherited_closed_returned;
    unsigned int last_pc;
    const mc_client_screen_error *failure;
    const mc_client_inventory_screen_error *detail;
} mc_client_furnace_screen_state;

/* OV owns its actual NEW BM and every failed constructor prefix. Its
 * actual IX and SK arguments remain distinct. Native retirement never invokes ID.h
 * or changes Player.e; an owned BM still published there keeps retirement BUSY. */
mc_client_screen_status mc_client_screen_furnace_allocate(mc_client_screen_classes *,
                                                          mc_client_render_classes *,
                                                          const mc_client_screen_options *,
                                                          mc_client_source_ov **,
                                                          mc_client_screen_error *);
mc_client_screen_status mc_client_screen_furnace_construct(mc_client_source_ov *,
                                                           mc_inventory_player *,
                                                           mc_inventory_furnace *,
                                                           mc_client_screen_error *);
mc_client_source_da *mc_client_screen_furnace_base(mc_client_source_ov *);
mc_client_screen_status mc_client_screen_furnace_inspect(mc_client_source_ov *,
                                                         mc_client_furnace_screen_state *,
                                                         mc_client_screen_error *);
mc_client_screen_status mc_client_screen_furnace_release(mc_client_source_ov **,
                                                         mc_client_screen_error *);
mc_client_screen_status mc_client_screen_furnace_retire(mc_client_source_ov **,
                                                        mc_client_screen_error *);

#endif
