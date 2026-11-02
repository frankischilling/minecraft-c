#ifndef MC_INVENTORY_CONTAINER_INTERACTION_H
#define MC_INVENTORY_CONTAINER_INTERACTION_H

#include "mc/inventory/container.h"

typedef struct inv_slot mc_inventory_slot;
typedef struct mc_container_interaction_call mc_container_interaction_call;

typedef struct mc_container_interaction_error {
    mc_inventory_status status;
    const char *operation, *exception_class;
    mc_inventory_site site, child, dependency;
    bool native_policy, source_exception, source_error;
    mc_inventory_error inventory;
    mc_registry_error registry;
    mc_item_stack_status stack;
} mc_container_interaction_error;

/* Explicit reached source dependencies supplied by the simulation adapter.
 * inventory reads GS.c; cursor_changed invokes GS.b(Stack); drop invokes the
 * actual GS.a(Stack); crafted executes YV's IZ.b(World,Player)/achievement
 * prefix. They run synchronously, may mutate source fields and must preserve
 * exact failed prefixes. A failed drop may retain work in its owning adapter.
 * context and all owners remain live through checked call retirement. */
typedef struct mc_container_interaction_host {
    void *context;
    mc_inventory_status (*inventory)(void *, mc_entity *, mc_inventory_player **,
                                     mc_container_interaction_error *);
    mc_inventory_status (*cursor_changed)(void *, mc_entity *, mc_item_stack *,
                                          mc_container_interaction_error *);
    mc_inventory_status (*drop)(void *, mc_entity *, mc_item_stack *,
                                mc_container_interaction_error *);
    mc_inventory_status (*crafted)(void *, mc_entity *, mc_item_stack *,
                                   mc_container_interaction_error *);
} mc_container_interaction_host;

typedef struct mc_container_interaction_state {
    mc_container *container;
    mc_entity *player;
    const mc_item_stack *result; /* Actual source return clone, borrowed while call lives. */
    const mc_container_interaction_error *error;
    int32_t slot, button;
    bool shift, close, active, returned, cursor_cleared;
    size_t crafting_cells_cleared, drop_calls, shift_calls, steps;
    int32_t source_pc;
} mc_container_interaction_state;

void mc_container_interaction_error_init(mc_container_interaction_error *);

/* Native holds do not call a constructor, initialize classes, retain Player,
 * or execute source close. The owner/its source graph stays live/exclusive.
 * retire is BUSY while any screen/call has such a hold. release balances one
 * hold; it never retires or frees the Container. */
mc_inventory_status mc_container_retain(mc_container *, mc_inventory_error *);
mc_inventory_status mc_container_release(mc_container *, mc_inventory_error *);
mc_inventory_status mc_container_window_id(const mc_container *, int32_t *, mc_inventory_error *);

/* Capture the actual DW.e List element. Its nullable Slot and all following
 * borrows expire only when the caller's Container hold ends. The enclosing
 * caller owns its actual List.get invocation PC. Source GP operations below
 * report their own declaring methods and actual Inventory children. */
mc_inventory_status mc_container_source_slot(mc_container *, int32_t, mc_inventory_slot **,
                                             mc_container_interaction_error *);
mc_inventory_status mc_container_source_slots_size(mc_container *, int32_t *,
                                                   mc_container_interaction_error *);
bool mc_inventory_slot_source_state(const mc_inventory_slot *, mc_container_slot_state *);
mc_inventory_status mc_inventory_slot_source_get(mc_inventory_slot *, const mc_item_stack **,
                                                 mc_container_interaction_error *);
mc_inventory_status mc_inventory_slot_source_has(mc_inventory_slot *, bool *,
                                                 mc_container_interaction_error *);
mc_inventory_status mc_inventory_slot_source_background(mc_inventory_slot *, int32_t *,
                                                        mc_container_interaction_error *);

/* Actual client DW.a(IIZ,GS) and virtual AA.a(GS) close. begin allocates one
 * owned invocation before source work; even a failed returned call must be
 * retired. Invalid buttons return before the GS.c field read. No mutation is
 * rolled back or replayed by retirement. Source Stack clones/splits retain
 * their exact identities and fields, including RAW/failed source storage.
 * All access is exclusive. out starts NULL; optional error never skips work.
 * The supplied host is copied; its context must outlive the call. */
mc_inventory_status mc_container_click_begin(mc_container *, mc_entity *, int32_t slot,
                                             int32_t button, bool shift,
                                             const mc_container_interaction_host *,
                                             mc_container_interaction_call **,
                                             mc_container_interaction_error *);
mc_inventory_status mc_container_close_begin(mc_container *, mc_entity *,
                                             const mc_container_interaction_host *,
                                             mc_container_interaction_call **,
                                             mc_container_interaction_error *);
bool mc_container_interaction_inspect(const mc_container_interaction_call *,
                                      mc_container_interaction_state *);
/* Transfer the actual successfully returned source clone, including NULL.
 * No second clone is made. A failed/active call preserves out. */
mc_inventory_status mc_container_interaction_take_result(mc_container_interaction_call *,
                                                         mc_item_stack **);
mc_inventory_status mc_container_interaction_retire(mc_container_interaction_call **,
                                                    mc_container_interaction_error *);

#endif
