#ifndef MC_CLIENT_GUI_SOURCE_WORKBENCH_SCREEN_H
#define MC_CLIENT_GUI_SOURCE_WORKBENCH_SCREEN_H

#include "source_inventory_screen.h"

typedef struct mc_client_source_oo mc_client_source_oo;

typedef struct mc_client_workbench_screen_state {
    mc_client_screen_phase phase;
    size_t holds, source_calls;
    mc_client_screen_id_fields id;
    mc_inventory_player *constructor_inventory;
    mc_world_runtime *constructor_world;
    mc_container *constructor_container;
    bool container_owned, inherited_closed_returned, direct_closed_returned;
    unsigned int last_pc;
    const mc_client_screen_error *failure;
    const mc_client_inventory_screen_error *detail;
} mc_client_workbench_screen_state;

/* ID superclass active use precedes NEW OO. OO construction owns its exact
 * NEW IT and every failed prefix. Inventory/Registry argument owners remain
 * live/exclusive through retirement; nullable arguments are not substituted.
 * Native retirement is BUSY while any actual ID.b publication receiver still
 * has Player.e pointing at this owned IT. Admission and all native rights stay
 * intact in that case; ordinary source close must end that alias. No live-open
 * World terminal join, implicit source close or Player.e repair is claimed. */
mc_client_screen_status mc_client_screen_workbench_allocate(mc_client_screen_classes *,
                                                            mc_client_render_classes *,
                                                            const mc_client_screen_options *,
                                                            mc_client_source_oo **,
                                                            mc_client_screen_error *);
mc_client_screen_status mc_client_screen_workbench_construct(mc_client_source_oo *,
                                                             mc_inventory_player *,
                                                             mc_world_runtime *, int32_t x,
                                                             int32_t y, int32_t z,
                                                             mc_client_screen_error *);
mc_client_source_da *mc_client_screen_workbench_base(mc_client_source_oo *);
mc_client_screen_status mc_client_screen_workbench_inspect(mc_client_source_oo *,
                                                           mc_client_workbench_screen_state *,
                                                           mc_client_screen_error *);
mc_client_screen_status mc_client_screen_workbench_release(mc_client_source_oo **,
                                                           mc_client_screen_error *);
mc_client_screen_status mc_client_screen_workbench_retire(mc_client_source_oo **,
                                                          mc_client_screen_error *);

#endif
