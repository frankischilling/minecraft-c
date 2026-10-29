#ifndef MC_CLIENT_WORLD_HELD_ITEM_TICK_H
#define MC_CLIENT_WORLD_HELD_ITEM_TICK_H

#include "mc/client/world/entity_renderer.h"
#include "mc/inventory/player_current.h"

typedef struct mc_client_held_item_tick_call mc_client_held_item_tick_call;

typedef enum mc_client_held_item_tick_status {
    MC_CLIENT_HELD_ITEM_TICK_OK,
    MC_CLIENT_HELD_ITEM_TICK_ARGUMENT,
    MC_CLIENT_HELD_ITEM_TICK_PHASE,
    MC_CLIENT_HELD_ITEM_TICK_BUSY,
    MC_CLIENT_HELD_ITEM_TICK_ALLOCATION,
    MC_CLIENT_HELD_ITEM_TICK_OWNER,
    MC_CLIENT_HELD_ITEM_TICK_SOURCE_NULL,
    MC_CLIENT_HELD_ITEM_TICK_PLAYER,
    MC_CLIENT_HELD_ITEM_TICK_INVENTORY,
    MC_CLIENT_HELD_ITEM_TICK_STACK,
    MC_CLIENT_HELD_ITEM_TICK_FLOATING
} mc_client_held_item_tick_status;

typedef struct mc_client_held_item_tick_error {
    mc_client_held_item_tick_status status;
    const char *operation;
    mc_client_graphics_site site, child;
    bool native_policy;
    mc_entity_status player;
    mc_item_stack_status stack;
    mc_inventory_current_error inventory;
    mc_client_entity_renderer_error owner;
} mc_client_held_item_tick_error;

typedef struct mc_client_held_item_tick_state {
    mc_client_held_item_tick_status status;
    bool active, d_stored, current_returned, equivalent_replaced, c_stored, b_stored, g_stored;
    bool source_returned, same;
    unsigned int source_pc;
    mc_client_held_item *owner; /* Borrowed, retained by this native call. */
    const mc_client_source_ra *receiver;
    client_application *app;
    mc_entity *player;
    const mc_item_stack *selected, *previous;
    int32_t selected_slot, after_g;
    float before_c, after_c, after_d, delta;
    const mc_client_held_item_tick_error *error;
} mc_client_held_item_tick_state;

/* Execute unchanged source ra.a()V on this owner's actual RA. RAW and failed
 * constructor prefixes are valid source receivers; only active, constructing
 * and native RETIRING owners are refused. A genuine successful construction
 * is still required for ordinary runtime use. No RA/Player/Inventory substitute
 * or copied Settings is introduced. The same complete App graph stays live
 * and exclusive through retirement; snapshots borrow App/Player/Inventory.
 *
 * Out must start NULL. Native frame allocation and one ordinary RA occurrence
 * precede source effects. An allocated failed call is returned for inspection,
 * never replayed. The call holds its selected/previous Stack observations;
 * RA.b separately owns its published Stack occurrence across Inventory edits.
 * Exact binary32 subtraction/clamps/add preserve Java NaN comparison behavior;
 * unsupported floating control stops before the first arithmetic after d5 and
 * any reached equivalent-Stack replacement118. Other fields remain unchanged.
 * Optional error and every output must be live and disjoint from owners. */
mc_client_held_item_tick_status mc_client_held_item_tick_begin(mc_client_held_item *,
                                                               mc_client_held_item_tick_call **,
                                                               mc_client_held_item_tick_error *);
bool mc_client_held_item_tick_inspect(const mc_client_held_item_tick_call *,
                                      mc_client_held_item_tick_state *);
/* Native cleanup only. Active callbacks preserve the slot and report BUSY.
 * Success consumes it and ends borrowed diagnostic views; no source reset,
 * save, input operation or field rollback occurs. NULL slot value is harmless. */
mc_client_held_item_tick_status mc_client_held_item_tick_retire(mc_client_held_item_tick_call **,
                                                                mc_client_held_item_tick_error *);

#endif
