#ifndef MC_STATISTICS_REGISTRY_REGISTRY_H
#define MC_STATISTICS_REGISTRY_REGISTRY_H

#include "mc/block/registry/registry.h"

typedef struct mc_statistic mc_statistic;
typedef struct mc_statistic_formatter mc_statistic_formatter;
typedef struct mc_statistic_number_format mc_statistic_number_format;
typedef struct mc_registry_list mc_statistic_list;

typedef enum mc_statistic_kind {
    MC_STATISTIC_BASE,
    MC_STATISTIC_GENERAL,
    MC_STATISTIC_ITEM,
    MC_STATISTIC_ACHIEVEMENT,
    MC_STATISTIC_KIND_COUNT
} mc_statistic_kind;

typedef enum mc_statistic_formatter_kind {
    MC_STATISTIC_FORMAT_INTEGER,
    MC_STATISTIC_FORMAT_TIME,
    MC_STATISTIC_FORMAT_DISTANCE
} mc_statistic_formatter_kind;

typedef enum mc_statistic_number_locale {
    MC_STATISTIC_NUMBER_LOCALE_UNKNOWN,
    MC_STATISTIC_NUMBER_LOCALE_EN_US,
    MC_STATISTIC_NUMBER_LOCALE_EN
} mc_statistic_number_locale;

/* Actual base vr/ns fields. Subtypes retain their own additional storage.
 * Strings and formatter references are borrowed from the common class owner. */
typedef struct mc_statistic_fields {
    int32_t e;
    const mc_registry_string *f;
    bool g;
    const mc_registry_string *h;
    const mc_statistic_formatter *a;
} mc_statistic_fields;

/* Diagnostic numeric configuration, not Java reflection or getter-result
 * String identities. These fields belong to real constructed formatter
 * storage. Runtime formatting and additional locale profiles are qualified
 * separately from class construction and registration. */
typedef struct mc_statistic_number_format_fields {
    mc_statistic_number_locale locale;
    bool grouping_used, parse_integer_only;
    int32_t maximum_integer_digits, minimum_integer_digits;
    int32_t maximum_fraction_digits, minimum_fraction_digits;
    int32_t grouping_size, multiplier;
    bool half_even, decimal_separator_always_shown, parse_big_decimal;
    uint16_t zero_digit, decimal_separator, grouping_separator, minus_sign;
    const mc_registry_string *pattern;
    const mc_registry_string *positive_prefix, *negative_prefix;
    const mc_registry_string *positive_suffix, *negative_suffix;
    const mc_registry_string *infinity, *nan;
} mc_statistic_number_format_fields;

typedef struct mc_registry_statistic_base_view {
    mc_registry_phase phase;
    int32_t initializer_pc;
    const mc_statistic_number_format *b, *c;
    const mc_statistic_formatter *i, *j, *k;
} mc_registry_statistic_base_view;

typedef struct mc_registry_statistic_view {
    mc_registry_phase phase;
    int32_t initializer_pc;
    size_t map_size;
    const mc_statistic_list *b, *c, *d, *e;
    /* Live source f..B declaration order, followed by C/D/E/F and G/H. */
    mc_statistic *const *named;
    mc_statistic *const *C, *const *D, *const *E, *const *F;
    bool G, H;
    /* Native retained duplicate-exception message, not a successful result. */
    const mc_registry_string *duplicate_message;
} mc_registry_statistic_view;

mc_registry_status mc_registry_classes_initialize_statistic_base(mc_registry_classes *classes,
                                                                 mc_registry_error *error);
/* Runs actual jl/gg construction on the common owner. Unfinished reached
 * block/achievement/recipe dependencies fail explicitly; no empty callback
 * may publish a completed statistics bootstrap. Failed prefixes remain live. */
mc_registry_status mc_registry_classes_initialize_statistics(mc_registry_classes *classes,
                                                             mc_registry_error *error);
mc_registry_status mc_registry_classes_inspect_statistic_base(const mc_registry_classes *classes,
                                                              mc_registry_statistic_base_view *out,
                                                              mc_registry_error *error);
mc_registry_status mc_registry_classes_inspect_statistics(const mc_registry_classes *classes,
                                                          mc_registry_statistic_view *out,
                                                          mc_registry_error *error);

/* Diagnostic access never initializes a class or makes a failed class usable.
 * Missing IDs succeed with NULL. All returned identities survive until the
 * common class owner retires. Outputs must be disjoint from owner storage. */
mc_registry_status mc_registry_classes_find_statistic(const mc_registry_classes *classes,
                                                      int32_t id, const mc_statistic **out);
const mc_statistic_fields *mc_statistic_fields_get(const mc_statistic *statistic);
mc_statistic_kind mc_statistic_kind_get(const mc_statistic *statistic);
mc_registry_status mc_statistic_item_id(const mc_statistic *statistic, int32_t *out);
mc_registry_status mc_statistic_formatter_kind_get(const mc_statistic_formatter *formatter,
                                                   mc_statistic_formatter_kind *out);
const mc_statistic_number_format_fields *
mc_statistic_number_format_fields_get(const mc_statistic_number_format *format);
size_t mc_statistic_list_size(const mc_statistic_list *list);
mc_registry_status mc_statistic_list_get(const mc_statistic_list *list, size_t index,
                                         const mc_statistic **out);

#endif
