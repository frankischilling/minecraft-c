#ifndef MC_INVENTORY_RESULT_H
#define MC_INVENTORY_RESULT_H

#include "mc/inventory/errors.h"

typedef struct mc_inventory_result mc_inventory_result;

mc_inventory_status mc_inventory_result_create(mc_registry_side side,
                                               const mc_inventory_limits *limits,
                                               mc_inventory_result **out,
                                               mc_inventory_error *error);
mc_inventory_status mc_inventory_result_retire(mc_inventory_result **result,
                                               mc_inventory_error *error);
mc_inventory_status mc_inventory_result_borrow(const mc_inventory_result *result, int32_t slot,
                                               const mc_item_stack **out,
                                               mc_inventory_error *error);
mc_inventory_status mc_inventory_result_get(mc_inventory_result *result, int32_t slot,
                                            mc_item_stack **out, mc_inventory_error *error);
mc_inventory_status mc_inventory_result_set(mc_inventory_result *result, int32_t slot,
                                            mc_item_stack *value, mc_inventory_error *error);
/* Ignores amount, transfers the complete occurrence, sends no notification. */
mc_inventory_status mc_inventory_result_split(mc_inventory_result *result, int32_t slot,
                                              int32_t amount, mc_item_stack **out,
                                              mc_inventory_error *error);
mc_inventory_status mc_inventory_result_size(const mc_inventory_result *result, int32_t *out,
                                             mc_inventory_error *error);
mc_inventory_status mc_inventory_result_name(const mc_inventory_result *result, const char **out,
                                             mc_inventory_error *error);
mc_inventory_status mc_inventory_result_limit(const mc_inventory_result *result, int32_t *out,
                                              mc_inventory_error *error);
mc_inventory_status mc_inventory_result_changed(mc_inventory_result *result,
                                                mc_inventory_error *error);
mc_inventory_status mc_inventory_result_usable(const mc_inventory_result *result,
                                               const void *player, bool *out,
                                               mc_inventory_error *error);

#endif
