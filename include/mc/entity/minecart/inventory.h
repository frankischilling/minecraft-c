#ifndef MC_ENTITY_MINECART_INVENTORY_H
#define MC_ENTITY_MINECART_INVENTORY_H

#include "mc/entity/entity.h"
#include "mc/entity/minecart/inventory_errors.h"

/* Exact canonical Minecart, every live construction phase and unrestricted
 * signed Type. No World/READY/Player/Registry admission. Callers hold owners
 * exclusively; outputs/errors/owners and borrowed input spans are disjoint.
 * Outputs are required and unchanged on failure. Optional errors are fresh;
 * success clears the entire attempt. Size27 does not bound actual h36 slots. */
mc_minecart_inventory_status mc_minecart_inventory_size(mc_entity *cart, int32_t *out,
                                                        mc_minecart_inventory_error *error);
mc_minecart_inventory_status mc_minecart_inventory_name(mc_entity *cart, mc_nbt_string *out,
                                                        mc_minecart_inventory_error *error);
mc_minecart_inventory_status mc_minecart_inventory_limit(mc_entity *cart, int32_t *out,
                                                         mc_minecart_inventory_error *error);
mc_minecart_inventory_status mc_minecart_inventory_changed(mc_entity *cart,
                                                           mc_minecart_inventory_error *error);

/* get adds one independent hold to a nonnull loaded occurrence. split returns
 * NULL, transfers the removed occurrence, or publishes a new Stack's initial
 * hold. Its requested amount is unrestricted; failure after the descendant's
 * count store retains that wrapped decrement. Neither changes cart phase. */
mc_minecart_inventory_status mc_minecart_inventory_get(mc_entity *cart, int32_t slot,
                                                       mc_item_stack **out,
                                                       mc_minecart_inventory_error *error);
mc_minecart_inventory_status mc_minecart_inventory_split(mc_entity *cart, int32_t slot,
                                                         int32_t amount, mc_item_stack **out,
                                                         mc_minecart_inventory_error *error);

/* Caller keeps its hold. Acquire one occurrence hold before the identity6
 * store and release the replaced occurrence after that store. Clamp only
 * count>64, on that same supplied identity, at27; no lower clamp or clone. */
mc_minecart_inventory_status mc_minecart_inventory_set(mc_entity *cart, int32_t slot,
                                                       mc_item_stack *value,
                                                       mc_minecart_inventory_error *error);

#endif
