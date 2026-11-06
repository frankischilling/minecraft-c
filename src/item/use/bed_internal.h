#ifndef MC_ITEM_USE_BED_INTERNAL_H
#define MC_ITEM_USE_BED_INTERNAL_H
#include "mc/item/use/use.h"

/* Actual direct LP.a body, sharing the production IZ dispatch implementation.
 * Source arguments remain nullable; this entered facade adds no IZ resolution
 * or statistic tail. The receiver is the actual code-owner Item355 object.
 * Returned call ownership/checked retirement matches the public IZ facade. */
mc_item_use_status mc_item_use_bed_begin(mc_registry_classes *, const mc_item *receiver,
                                         mc_item_stack *, mc_entity *, mc_world_runtime *,
                                         int32_t x, int32_t y, int32_t z, int32_t face,
                                         const mc_item_use_context *, mc_item_use_call **,
                                         mc_item_use_error *);
#endif
