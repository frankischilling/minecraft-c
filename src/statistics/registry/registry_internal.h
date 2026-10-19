#ifndef MC_STATISTICS_REGISTRY_INTERNAL_H
#define MC_STATISTICS_REGISTRY_INTERNAL_H

#include "mc/statistics/registry/registry.h"

struct mc_registry_list;
typedef struct mc_statistic_map mc_statistic_map;

struct mc_statistic_number_format {
    mc_registry_classes *classes;
    mc_statistic_number_format_fields fields;
};

struct mc_statistic_formatter {
    mc_registry_classes *classes;
    mc_statistic_formatter_kind kind;
};

struct mc_statistic {
    mc_registry_classes *classes;
    mc_statistic_kind kind;
    mc_statistic_fields fields;
    int32_t associated_id;
    const char *source_class;
    int32_t source_pc;
};

typedef struct mc_registry_statistic_base_storage {
    mc_statistic_number_format *b, *c;
    mc_statistic_formatter *i, *j, *k;
} mc_registry_statistic_base_storage;

typedef struct mc_registry_statistic_storage {
    mc_statistic_map *a;
    struct mc_registry_list *b, *c, *d, *e;
    mc_statistic *named[23];
    mc_statistic **C, **D, **E, **F;
    bool G, H;
    const mc_registry_string *duplicate_message;
} mc_registry_statistic_storage;

mc_registry_status mc_registry_number_format_create(mc_registry_classes *classes, bool integer_us,
                                                    mc_statistic_number_format **out);
mc_registry_status mc_registry_statistic_base_active_use(mc_registry_classes *classes);
mc_registry_status mc_registry_statistics_active_use(mc_registry_classes *classes);
mc_registry_status mc_registry_statistic_collections(mc_registry_classes *classes);
mc_registry_status mc_registry_statistic_allocate(mc_registry_classes *classes,
                                                  mc_statistic_kind kind, mc_statistic **out);
mc_registry_status mc_registry_statistic_construct(mc_statistic *statistic, int32_t id,
                                                   const mc_registry_string *name,
                                                   const mc_statistic_formatter *formatter,
                                                   int32_t associated_id);
mc_registry_status mc_registry_statistic_flag(mc_statistic *statistic);
mc_registry_status mc_registry_statistic_register(mc_statistic *statistic);
mc_registry_status mc_registry_statistic_list_add(mc_registry_classes *classes,
                                                  struct mc_registry_list *list,
                                                  mc_statistic *statistic);

#endif
