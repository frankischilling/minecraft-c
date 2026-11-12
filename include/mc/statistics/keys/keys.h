#ifndef MC_STATISTICS_KEYS_KEYS_H
#define MC_STATISTICS_KEYS_KEYS_H

#include "mc/block/registry/registry.h"

typedef struct mc_statistic_keys mc_statistic_keys;

typedef struct mc_registry_statistic_key_view {
    mc_registry_phase phase;
    int32_t initializer_pc;
    const mc_statistic_keys *a;
    /* Native retained construction diagnostics, not a source publication. */
    const mc_statistic_keys *construction;
    bool map_published;
    int32_t constructor_pc;
    mc_registry_status caught_status;
    int32_t caught_pc;
} mc_registry_statistic_key_view;

/* Execute the actual shared b initializer and /achievement/map.txt reader on
 * the same class owner as Block/Item/Translation. The source default charset,
 * comma split, integer parsing, duplicate replacement and successful close
 * are reached in source order. Caught Exception.printStackTrace remains an
 * explicit unfinished effect; a failed prefix cannot publish singleton a. */
mc_registry_status mc_registry_classes_initialize_statistic_keys(mc_registry_classes *classes,
                                                                 mc_registry_error *error);
mc_registry_status mc_registry_classes_inspect_statistic_keys(const mc_registry_classes *classes,
                                                              mc_registry_statistic_key_view *out,
                                                              mc_registry_error *error);

/* Actual b.a(int), including initialization. Missing IDs return NULL without
 * a fallback. Values and overwritten String owners remain in the common
 * retained arena. Failed initialization cannot be retried on that owner. */
mc_registry_status mc_registry_classes_statistic_key(mc_registry_classes *classes, int32_t id,
                                                     const mc_registry_string **out,
                                                     mc_registry_error *error);

/* Read reached map storage for diagnostics, including a failed unpublished
 * constructor prefix. Entry order is native first insertion order, not Java
 * HashMap iteration. Outputs remain unchanged on invalid arguments/index. */
size_t mc_statistic_keys_size(const mc_statistic_keys *keys);
mc_registry_status mc_statistic_keys_entry(const mc_statistic_keys *keys, size_t index, int32_t *id,
                                           const mc_registry_string **value);

#endif
