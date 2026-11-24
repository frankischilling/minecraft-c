#ifndef MC_ITEM_RECIPES_RECIPES_H
#define MC_ITEM_RECIPES_RECIPES_H

#include "mc/item/registry/registry.h"
#include "mc/item/stack/stack.h"

typedef struct mc_recipe mc_recipe;
typedef struct mc_crafting_manager mc_crafting_manager;
typedef struct mc_smelting_manager mc_smelting_manager;
typedef struct mc_registry_list mc_recipe_ingredient_list;

typedef enum mc_recipe_kind {
    MC_RECIPE_SHAPED,
    MC_RECIPE_SHAPELESS,
    MC_RECIPE_KIND_COUNT
} mc_recipe_kind;

/* Actual is/ft fields: cached output ID, width, height, ingredient array and
 * output identity. Array extent is separate native inspection metadata. */
typedef struct mc_shaped_recipe_fields {
    int32_t a, b, c;
    mc_item_stack *const *d;
    const mc_item_stack *e;
} mc_shaped_recipe_fields;

/* Actual tt/ml fields. The list and its elements borrow the class lifetime. */
typedef struct mc_shapeless_recipe_fields {
    const mc_item_stack *a;
    const mc_recipe_ingredient_list *b;
} mc_shapeless_recipe_fields;

typedef struct mc_registry_crafting_view {
    mc_registry_phase phase;
    int32_t initializer_pc;
    const mc_crafting_manager *a;
} mc_registry_crafting_view;

typedef struct mc_registry_smelting_view {
    mc_registry_phase phase;
    int32_t initializer_pc;
    const mc_smelting_manager *a;
} mc_registry_smelting_view;

/* Exclusive synchronous class initialization. Same-flow admitted recursion
 * reads the real still-null singleton until its constructor has returned;
 * public reentry is BUSY. Failed classes retain their first cause and writes.
 * Inspectors never initialize, publish, repair or retire a class. */
mc_registry_status mc_registry_classes_initialize_crafting(mc_registry_classes *classes,
                                                           mc_registry_error *error);
mc_registry_status mc_registry_classes_initialize_smelting(mc_registry_classes *classes,
                                                           mc_registry_error *error);
mc_registry_status mc_registry_classes_inspect_crafting(const mc_registry_classes *classes,
                                                        mc_registry_crafting_view *out,
                                                        mc_registry_error *error);
mc_registry_status mc_registry_classes_inspect_smelting(const mc_registry_classes *classes,
                                                        mc_registry_smelting_view *out,
                                                        mc_registry_error *error);

/* Read-only native inspection of actual retained objects. No method below
 * creates a recipe or a Stack, changes a collection or executes gameplay.
 * NULL size_t queries return zero. Other invalid arguments fail unchanged.
 * Borrowed outputs remain live until class retirement; independently retain
 * an ItemStack before using it beyond that lifetime. */
mc_recipe_kind mc_recipe_kind_get(const mc_recipe *recipe);
const mc_shaped_recipe_fields *mc_recipe_shaped_fields_get(const mc_recipe *recipe);
const mc_shapeless_recipe_fields *mc_recipe_shapeless_fields_get(const mc_recipe *recipe);
size_t mc_recipe_shaped_array_length(const mc_recipe *recipe);
const mc_item_stack *mc_recipe_output(const mc_recipe *recipe);
mc_registry_status mc_recipe_size(const mc_recipe *recipe, int32_t *out);
size_t mc_recipe_ingredient_list_size(const mc_recipe_ingredient_list *list);
mc_registry_status mc_recipe_ingredient_list_get(const mc_recipe_ingredient_list *list,
                                                 size_t index, const mc_item_stack **out);
size_t mc_crafting_manager_size(const mc_crafting_manager *manager);
mc_registry_status mc_crafting_manager_get(const mc_crafting_manager *manager, size_t index,
                                           const mc_recipe **out);
size_t mc_smelting_manager_size(const mc_smelting_manager *manager);
mc_registry_status mc_smelting_manager_find(const mc_smelting_manager *manager, int32_t input,
                                            const mc_item_stack **out);
/* Complete Integer-key bucket/next traversal in the retained JDK collection
 * profile. This read-only copy allocates nothing. Insufficient capacity returns
 * LIMIT and leaves both output storage and count unchanged. A missing find
 * succeeds with NULL, as does a stored NULL value. */
mc_registry_status mc_smelting_manager_keys(const mc_smelting_manager *manager, int32_t *output,
                                            size_t capacity, size_t *count);

#endif
