#ifndef MC_ITEM_USE_SIGN_INTERNAL_H
#define MC_ITEM_USE_SIGN_INTERNAL_H

#include "mc/item/use/use.h"

/* Enter the actual VC body on its genuine Item receiver, without the IZ
 * resolution/statistic wrapper. Nullable arguments fail only when reached;
 * face0 returns false before reading any of them. Shares normal inspection
 * and checked retirement, including failed placement/editor prefixes. */
mc_item_use_status mc_item_use_sign_begin(mc_registry_classes *, const mc_item *receiver,
                                          mc_item_stack *, mc_entity *, mc_world_runtime *,
                                          int32_t x, int32_t y, int32_t z, int32_t face,
                                          const mc_item_use_context *, mc_item_use_call **,
                                          mc_item_use_error *);

#endif
