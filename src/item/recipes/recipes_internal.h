#ifndef MC_ITEM_RECIPES_INTERNAL_H
#define MC_ITEM_RECIPES_INTERNAL_H

#include "mc/core/java/int_set.h"
#include "mc/item/recipes/recipes.h"

struct mc_registry_list;
struct mc_smelting_map;

struct mc_recipe {
    mc_registry_classes *classes;
    mc_recipe_kind kind;
    union {
        mc_shaped_recipe_fields shaped;
        mc_shapeless_recipe_fields shapeless;
    } fields;
    size_t ingredient_count; /* native shaped-array extent, not a game field */
};

struct mc_crafting_manager {
    mc_registry_classes *classes;
    struct mc_registry_list *b;
    int32_t constructor_pc; /* native source-operation diagnostic */
    bool stdout_io_failed;  /* native output diagnostic, not a game field */
};

struct mc_smelting_manager {
    mc_registry_classes *classes;
    struct mc_smelting_map *b;
    int32_t constructor_pc; /* native source-operation diagnostic */
};

typedef struct mc_registry_recipe_storage {
    mc_crafting_manager *crafting;
    mc_smelting_manager *smelting;
} mc_registry_recipe_storage;

/* Native typed representation of the reached Object[] operands. A pointer
 * tagged Item/Block/Stack/String but holding NULL still has Java null's
 * instanceof behavior. NULL arguments represent Java null Object[]; a nonnull
 * readable argument pointer with count zero represents a real empty array.
 * STRINGS.values follows the same null/nonnull distinction for String[].
 * The supplied span and every nested span must remain
 * readable and disjoint from output storage for the synchronous call. */
typedef enum mc_recipe_argument_kind {
    MC_RECIPE_ARGUMENT_NULL,
    MC_RECIPE_ARGUMENT_ITEM,
    MC_RECIPE_ARGUMENT_BLOCK,
    MC_RECIPE_ARGUMENT_STACK,
    MC_RECIPE_ARGUMENT_STRING,
    MC_RECIPE_ARGUMENT_STRINGS,
    MC_RECIPE_ARGUMENT_CHARACTER
} mc_recipe_argument_kind;

typedef struct mc_recipe_argument {
    mc_recipe_argument_kind kind;
    union {
        const mc_item *item;
        const mc_block *block;
        mc_item_stack *stack;
        const mc_registry_string *string;
        struct {
            const mc_registry_string *const *values;
            size_t count;
        } strings;
        uint16_t character;
    } value;
} mc_recipe_argument;

mc_registry_status mc_registry_crafting_active_use(mc_registry_classes *classes);
mc_registry_status mc_registry_smelting_active_use(mc_registry_classes *classes);
void mc_registry_crafting_stage(mc_crafting_manager *manager, int32_t pc);
mc_registry_status mc_registry_crafting_error(mc_crafting_manager *manager);
mc_registry_status mc_registry_recipe_shaped(mc_crafting_manager *manager, mc_item_stack *output,
                                             const mc_recipe_argument *arguments, size_t count);
mc_registry_status mc_registry_recipe_shapeless(mc_crafting_manager *manager, mc_item_stack *output,
                                                const mc_recipe_argument *arguments, size_t count);
mc_registry_status mc_registry_recipe_list_add(mc_registry_classes *classes,
                                               struct mc_registry_list *list, void *element);
mc_registry_status mc_registry_recipe_sort(mc_crafting_manager *manager);

/* Seven actual helper allocations/constructors/populate calls, then the
 * constructor's remaining explicit chains. Called only by the admitted real
 * manager constructor; these functions do not publish its static singleton. */
mc_registry_status mc_registry_recipe_families(mc_registry_classes *classes,
                                               mc_crafting_manager *manager);
mc_registry_status mc_registry_recipe_explicit(mc_registry_classes *classes,
                                               mc_crafting_manager *manager);
mc_registry_status mc_registry_smelting_construct(mc_smelting_manager *manager);
mc_registry_status mc_registry_smelting_put(mc_smelting_manager *manager, int32_t input,
                                            mc_item_stack *output);

/* Values iterate in the same Integer-key HashMap order as the shared pinned
 * collection profile. Borrowed outputs are actual registered Stack identities.
 * The native key-order snapshot is not a Java iterator or fail-fast promise. */
mc_registry_status mc_registry_smelting_keys(mc_registry_classes *classes,
                                             const mc_smelting_manager *manager, int32_t **out,
                                             size_t *count);

#endif
