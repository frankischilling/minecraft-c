#ifndef MC_ITEM_USE_FLINT_AND_STEEL_INTERNAL_H
#define MC_ITEM_USE_FLINT_AND_STEEL_INTERNAL_H

#include "mc/item/use/use.h"

/* Actual entered YE.a body on its genuine code-owner Item receiver. Nullable
 * Stack/Player/World arguments are read only at their reached Source sites.
 * This facade adds neither IZ resolution nor its later used-statistic tail.
 * The returned call retains the same exact reached graph and checked cleanup
 * contract as the public IZ facade. */
mc_item_use_status mc_item_use_flint_and_steel_begin(mc_registry_classes *, const mc_item *receiver,
                                                     mc_item_stack *, mc_entity *,
                                                     mc_world_runtime *, int32_t x, int32_t y,
                                                     int32_t z, int32_t face, mc_item_use_call **,
                                                     mc_item_use_error *);

#endif
