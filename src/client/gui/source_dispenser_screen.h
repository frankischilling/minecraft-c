#ifndef MC_CLIENT_GUI_SOURCE_DISPENSER_SCREEN_H
#define MC_CLIENT_GUI_SOURCE_DISPENSER_SCREEN_H

#include "mc/inventory/dispenser.h"
#include "source_inventory_screen.h"

typedef struct mc_client_source_gq mc_client_source_gq;

typedef struct mc_client_dispenser_screen_state {
    mc_client_screen_phase phase;
    size_t holds, source_calls;
    mc_client_screen_id_fields id;
    mc_inventory_player *constructor_inventory;
    mc_inventory_dispenser *constructor_dispenser;
    mc_container *constructor_container;
    bool container_owned, dispenser_held, inherited_closed_returned;
    unsigned int last_pc;
    const mc_client_screen_error *failure;
    const mc_client_inventory_screen_error *detail;
} mc_client_dispenser_screen_state;

/* GQ owns its actual NEW SH and every failed constructor prefix. Its
 * actual IX and AZ arguments remain distinct. Native retirement never invokes ID.h
 * or changes Player.e; an owned SH still published there keeps retirement BUSY. */
mc_client_screen_status mc_client_screen_dispenser_allocate(mc_client_screen_classes *,
                                                            mc_client_render_classes *,
                                                            const mc_client_screen_options *,
                                                            mc_client_source_gq **,
                                                            mc_client_screen_error *);
mc_client_screen_status mc_client_screen_dispenser_construct(mc_client_source_gq *,
                                                             mc_inventory_player *,
                                                             mc_inventory_dispenser *,
                                                             mc_client_screen_error *);
mc_client_source_da *mc_client_screen_dispenser_base(mc_client_source_gq *);
mc_client_screen_status mc_client_screen_dispenser_inspect(mc_client_source_gq *,
                                                           mc_client_dispenser_screen_state *,
                                                           mc_client_screen_error *);
mc_client_screen_status mc_client_screen_dispenser_release(mc_client_source_gq **,
                                                           mc_client_screen_error *);
mc_client_screen_status mc_client_screen_dispenser_retire(mc_client_source_gq **,
                                                          mc_client_screen_error *);

#endif
