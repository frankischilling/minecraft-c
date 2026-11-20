#ifndef MC_INVENTORY_CHEST_H
#define MC_INVENTORY_CHEST_H

#include "mc/inventory/errors.h"

typedef struct mc_entity mc_entity;
typedef struct mc_tile_entity mc_tile_entity;
typedef struct mc_inventory_chest mc_inventory_chest;

/* Actual LW receivers: a JS Chest or an ordered OG pair. The adapter owns one
 * counted occurrence of its TileEntity; OG owns its nullable children separately,
 * including two occurrences when both are the same receiver. name is a borrowed
 * nullable source literal and must outlive the pair. No World read or source
 * dirty/close occurs during allocation or native release. Access is exclusive. */
mc_inventory_status mc_inventory_chest_from_tile(mc_registry_side, mc_tile_entity *,
                                                 const mc_inventory_limits *, mc_inventory_chest **,
                                                 mc_inventory_error *);
mc_inventory_status mc_inventory_chest_large_create(mc_registry_side, const char *name,
                                                    mc_inventory_chest *left,
                                                    mc_inventory_chest *right,
                                                    const mc_inventory_limits *,
                                                    mc_inventory_chest **, mc_inventory_error *);
/* Separate actual NEW OG and one source constructor, for activation's World
 * query/cast between allocation and invokespecial. RAW and failed prefixes
 * remain owned; native release never completes/replays a constructor. */
/* Nonnull child wrappers must have completed their native constructor first;
 * this explicit graph admission keeps the passive ownership graph immutable.
 * Nullable arguments and shared completed children preserve source stores. */
mc_inventory_status mc_inventory_chest_large_allocate(mc_registry_side, const mc_inventory_limits *,
                                                      mc_inventory_chest **, mc_inventory_error *);
mc_inventory_status mc_inventory_chest_large_construct(mc_inventory_chest *, const char *name,
                                                       mc_inventory_chest *left,
                                                       mc_inventory_chest *right,
                                                       mc_inventory_error *);
mc_inventory_status mc_inventory_chest_retain(mc_inventory_chest *, mc_inventory_error *);
void mc_inventory_chest_release(mc_inventory_chest *);
mc_inventory_status mc_inventory_chest_retire(mc_inventory_chest **, mc_inventory_error *);
/* End completed native diagnostic records before outer WorldAccess/N teardown.
 * All borrowed error.chest views must have ended first. No source method/field,
 * Slot, Stack, TileEntity hold, graph edge or admission state changes. Active
 * invocations are BUSY; callers retain the complete source graph afterward. */
mc_inventory_status mc_inventory_chest_end_calls(mc_inventory_chest *, mc_inventory_error *);
mc_inventory_status mc_inventory_chest_size(mc_inventory_chest *, int32_t *, mc_inventory_error *);
mc_inventory_status mc_inventory_chest_name(mc_inventory_chest *, const char **,
                                            mc_inventory_error *);
mc_inventory_status mc_inventory_chest_limit(mc_inventory_chest *, int32_t *, mc_inventory_error *);
/* borrow returns the same nullable Stack without a hold; get/split publish one
 * counted occurrence. Failed operations leave outputs unchanged, while all
 * reached source mutations/dirty callbacks remain. OG repeats left.size on
 * right branches, preserves Java signed subtraction/addition and short circuits
 * eligibility. The native graph and all borrowed views stay live until release. */
mc_inventory_status mc_inventory_chest_borrow(mc_inventory_chest *, int32_t, const mc_item_stack **,
                                              mc_inventory_error *);
mc_inventory_status mc_inventory_chest_get(mc_inventory_chest *, int32_t, mc_item_stack **,
                                           mc_inventory_error *);
mc_inventory_status mc_inventory_chest_set(mc_inventory_chest *, int32_t, mc_item_stack *,
                                           mc_inventory_error *);
mc_inventory_status mc_inventory_chest_split(mc_inventory_chest *, int32_t, int32_t,
                                             mc_item_stack **, mc_inventory_error *);
mc_inventory_status mc_inventory_chest_changed(mc_inventory_chest *, mc_inventory_error *);
mc_inventory_status mc_inventory_chest_usable(mc_inventory_chest *, mc_entity *, bool *,
                                              mc_inventory_error *);

#endif
