#ifndef MC_STATISTICS_ACHIEVEMENTS_INTERNAL_H
#define MC_STATISTICS_ACHIEVEMENTS_INTERNAL_H

#include "mc/item/registry/registry.h"
#include "mc/statistics/achievements/achievements.h"

mc_registry_status mc_registry_achievements_active_use(mc_registry_classes *classes);

/* Private admitted constructors. A supplied stack borrows a lifetime through
 * class retirement; bootstrap icons are held by this owner's real stack ledger.
 * These methods preserve live ep/cy active uses at the actual field operations. */
mc_registry_status mc_registry_achievement_construct_stack(mc_statistic *statistic, int32_t raw_id,
                                                           const mc_registry_string *suffix,
                                                           int32_t x, int32_t y,
                                                           mc_item_stack *icon,
                                                           const mc_statistic *parent);
mc_registry_status mc_registry_achievement_construct_item(mc_statistic *statistic, int32_t raw_id,
                                                          const mc_registry_string *suffix,
                                                          int32_t x, int32_t y, const mc_item *icon,
                                                          const mc_statistic *parent);
mc_registry_status mc_registry_achievement_construct_block(mc_statistic *statistic, int32_t raw_id,
                                                           const mc_registry_string *suffix,
                                                           int32_t x, int32_t y,
                                                           const mc_block *icon,
                                                           const mc_statistic *parent);
mc_registry_status mc_registry_achievement_flag(mc_statistic *statistic);
mc_registry_status mc_registry_achievement_special(mc_statistic *statistic);
mc_registry_status mc_registry_achievement_register(mc_statistic *statistic);

#endif
