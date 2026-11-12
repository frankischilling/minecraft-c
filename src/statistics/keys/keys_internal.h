#ifndef MC_STATISTICS_KEYS_INTERNAL_H
#define MC_STATISTICS_KEYS_INTERNAL_H

#include "mc/statistics/keys/keys.h"

typedef struct mc_registry_key_storage {
    mc_statistic_keys *a;
    /* Retained native construction metadata, never the source static a. */
    mc_statistic_keys *pending;
} mc_registry_key_storage;

mc_registry_status mc_registry_statistic_keys_active_use(mc_registry_classes *classes);
mc_registry_status mc_registry_statistic_key(mc_registry_classes *classes, int32_t id,
                                             const mc_registry_string **out);

#endif
