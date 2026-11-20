#ifndef MC_ITEM_MINING_DESTROY_H
#define MC_ITEM_MINING_DESTROY_H

#include "mc/item/combat/combat.h"

typedef struct mc_item_mining_destroy_error {
    mc_entity_status status;
    const char *operation;
    bool native_policy, source_exception, source_error;
    const char *exception_class;
    mc_item_combat_site site;
    mc_registry_error registry;
    mc_item_stack_status stack;
    mc_item_combat_error item;
    mc_player_statistic_error statistic;
    bool item_returned, item_result, used_statistic_returned;
} mc_item_mining_destroy_error;

/* Actual client Item block-destruction virtual across all represented Classes.
 * DQ/QD reuse IZ.a(int,SN) with amount1/2 and return true. BL damages only
 * Leaves/Cobweb and still returns GM's false; other Classes inherit false.
 * All arguments unused by the resolved body may be NULL/arbitrary integers.
 * Keep actual owners and client Registry live/exclusive. No phase gate, close,
 * inventory clear or extra statistic is added. Earlier mutations survive errors. */
mc_entity_status mc_item_mining_destroy(mc_registry_classes *, const mc_item *, mc_item_stack *,
                                        int32_t block_id, int32_t x, int32_t y, int32_t z,
                                        mc_entity *player, bool *, mc_item_combat_error *);

/* Actual IZ.a(IIIILgs;)V. Item dispatch precedes used-statistic active use28,
 * late Stack.id32 and Player invocation37, which run only for a true Item
 * return. BL's real damage can therefore occur without a used statistic.
 * No count check or Inventory clearing occurs in this wrapper. */
mc_entity_status mc_item_stack_mining_destroy(mc_registry_classes *, mc_item_stack *,
                                              int32_t block_id, int32_t x, int32_t y, int32_t z,
                                              mc_entity *player, mc_item_mining_destroy_error *);

#endif
