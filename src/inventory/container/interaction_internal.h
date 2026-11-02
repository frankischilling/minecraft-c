#ifndef MC_INVENTORY_CONTAINER_INTERACTION_INTERNAL_H
#define MC_INVENTORY_CONTAINER_INTERACTION_INTERNAL_H

#include "../../block/registry/registry_internal.h"
#include "../../item/stack/source_internal.h"
#include "../internal.h"
#include "../player/source_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/inventory/container_interaction.h"
#include "mc/item/registry/runtime.h"

struct mc_container_interaction_call {
    mc_container_interaction_state state;
    mc_container_interaction_error error;
    mc_container_interaction_host host;
    mc_item_stack **holds;
    size_t hold_count, hold_capacity;
    mc_item_stack *result;
    bool container_held, result_taken;
};

bool inv_interaction_fail(mc_container_interaction_error *, mc_inventory_status, const char *);
bool inv_interaction_null(mc_container_interaction_error *, const char *);
bool inv_interaction_inventory(mc_container_interaction_error *, mc_inventory_status);
bool inv_interaction_stack(mc_container_interaction_error *, mc_item_stack_status, const char *);
bool inv_interaction_registry(mc_container_interaction_error *, mc_registry_classes *,
                              mc_registry_status);
bool inv_interaction_hold(mc_container_interaction_call *, mc_item_stack *);
bool inv_interaction_adopt(mc_container_interaction_call *, mc_item_stack *);
bool inv_interaction_clone(mc_container_interaction_call *, mc_item_stack *, mc_item_stack **);
bool inv_interaction_split(mc_container_interaction_call *, mc_item_stack *, int32_t,
                           mc_item_stack **);
bool inv_interaction_item(mc_container_interaction_error *, mc_registry_classes *, mc_item_stack *,
                          const mc_item **);
bool inv_interaction_max(mc_container_interaction_error *, mc_registry_classes *, mc_item_stack *,
                         int32_t *);
bool inv_interaction_subtypes(mc_container_interaction_error *, mc_registry_classes *,
                              mc_item_stack *, bool *);
bool inv_interaction_damageable(mc_container_interaction_error *, mc_registry_classes *,
                                mc_item_stack *, bool *);
bool inv_interaction_stackable(mc_container_interaction_error *, mc_registry_classes *,
                               mc_item_stack *, bool *);
bool inv_interaction_slot_get(mc_container_interaction_call *, inv_slot *, mc_item_stack **);
bool inv_interaction_slot_put(mc_container_interaction_call *, inv_slot *, mc_item_stack *);
bool inv_interaction_slot_changed(mc_container_interaction_call *, inv_slot *);
bool inv_interaction_slot_limit(mc_container_interaction_call *, inv_slot *, int32_t *);
bool inv_interaction_slot_valid(mc_container_interaction_call *, inv_slot *, mc_item_stack *,
                                bool *);
bool inv_interaction_slot_split(mc_container_interaction_call *, inv_slot *, int32_t,
                                mc_item_stack **);
bool inv_interaction_slot_pickup(mc_container_interaction_call *, inv_slot *, mc_item_stack *);
bool inv_interaction_cursor_get(mc_container_interaction_call *, mc_inventory_player *,
                                mc_item_stack **);
bool inv_interaction_cursor_put(mc_container_interaction_call *, mc_inventory_player *,
                                mc_item_stack *);
bool inv_interaction_click_body(mc_container_interaction_call *, int32_t, int32_t, bool,
                                unsigned int, mc_item_stack **);
bool inv_interaction_close_body(mc_container_interaction_call *);

#endif
