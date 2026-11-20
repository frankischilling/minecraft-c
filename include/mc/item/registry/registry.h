#ifndef MC_ITEM_REGISTRY_REGISTRY_H
#define MC_ITEM_REGISTRY_REGISTRY_H

#include "mc/block/registry/registry.h"

typedef struct mc_item mc_item;
typedef struct mc_registry_item_array mc_registry_item_array;

/* Eight actual gm/ej instance fields. Same-letter inherited/subtype fields
 * have separate storage. All references remain borrowed from the class owner. */
typedef struct mc_item_fields {
    int32_t bf;
    int32_t bg;
    int32_t a;
    int32_t bh;
    bool bi;
    bool bj;
    mc_item *bk;
    const mc_registry_string *bl;
} mc_item_fields;

typedef struct mc_registry_item_view {
    mc_registry_phase phase;
    int32_t initializer_pc;
    size_t named_items;
    const mc_java_random *b;
    const mc_registry_item_array *c;
} mc_registry_item_view;

/* Uses the same class owner, dependencies, exclusive-access/FP requirements and
 * retained-allocation lifetime as Block initialization. The actual Item/Block
 * active-use cycle executes on one internal initialization flow. Public calls
 * cannot reenter that flow. An unfinished reached dependency fails explicitly;
 * this entry does not provide a completed global bootstrap. */
mc_registry_status mc_registry_classes_initialize_items(mc_registry_classes *classes,
                                                        mc_registry_error *error);
mc_registry_status mc_registry_classes_inspect_items(const mc_registry_classes *classes,
                                                     mc_registry_item_view *out,
                                                     mc_registry_error *error);
mc_registry_status mc_registry_classes_inspect_item_static(const mc_registry_classes *classes,
                                                           const char *name, const mc_item **out,
                                                           mc_registry_error *error);
const mc_item_fields *mc_item_fields_get(const mc_item *item);

/* Diagnostic reads do not perform initialization or mutate the current c cell.
 * A saved array view retains that actual array identity through later work;
 * it is not a detached ID table. Outputs stay unchanged on native failure.
 * NULL length is zero; array_get requires a nonnull view and valid index. */
size_t mc_registry_item_array_length(const mc_registry_item_array *array);
mc_registry_status mc_registry_item_array_get(const mc_registry_item_array *array, size_t index,
                                              const mc_item **out, mc_registry_error *error);

#endif
