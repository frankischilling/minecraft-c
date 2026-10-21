#include "../../block/registry/registry_internal.h"
#include "registry_internal.h"

#include "mc/core/java/int_set.h"
#include "mc/core/java/java.h"

#include <limits.h>

/* Helper-method diagnostics use the native source-operation channel. These
 * methods are not class initializers and never change the caller's frame PC. */
static mc_registry_status array_error(mc_registry_classes *classes, int32_t pc) {
    classes->last_error.constructor_class =
        classes->options.side == MC_REGISTRY_CLIENT ? "jl" : "gg";
    classes->last_error.constructor_pc = pc;
    return classes->last_error.status;
}

static mc_registry_status array_fail(mc_registry_classes *classes, int32_t pc,
                                     mc_registry_status status, const char *message) {
    (void)mc_registry_fail(classes, status, message, NULL);
    return array_error(classes, pc);
}

static mc_registry_status block_load(mc_registry_classes *classes, int32_t index, int32_t access_pc,
                                     mc_block **out) {
    mc_registry_status status = mc_registry_blocks_active_use(classes);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_block **array = classes->blocks.m;
    if (array == NULL)
        return array_fail(classes, access_pc, MC_REGISTRY_NULL_REFERENCE,
                          "Statistics live Block array is null");
    if (index < 0 || (size_t)index >= classes->blocks.m_length)
        return array_fail(classes, access_pc, MC_REGISTRY_INDEX_OUT_OF_RANGE,
                          "Statistics live Block array index");
    *out = array[(size_t)index];
    return MC_REGISTRY_OK;
}

static mc_registry_status block_length(mc_registry_classes *classes, int32_t access_pc,
                                       int32_t *out) {
    mc_registry_status status = mc_registry_blocks_active_use(classes);
    if (status != MC_REGISTRY_OK)
        return status;
    if (classes->blocks.m == NULL)
        return array_fail(classes, access_pc, MC_REGISTRY_NULL_REFERENCE,
                          "Statistics live Block array length on null");
    if (classes->blocks.m_length > (size_t)INT32_MAX)
        return array_fail(classes, access_pc, MC_REGISTRY_LIMIT,
                          "Statistics native Block extent exceeds Java array bound");
    *out = (int32_t)classes->blocks.m_length;
    return MC_REGISTRY_OK;
}

static mc_registry_status item_load(mc_registry_classes *classes, int32_t index, int32_t access_pc,
                                    mc_item **out) {
    mc_registry_status status = mc_registry_items_active_use(classes);
    if (status != MC_REGISTRY_OK)
        return status;
    status = mc_registry_item_array_load(classes, classes->items.c, index, out);
    return status == MC_REGISTRY_OK ? status : array_error(classes, access_pc);
}

static mc_registry_status new_array(mc_registry_classes *classes, size_t length, int32_t pc,
                                    mc_statistic ***out) {
    if (length > SIZE_MAX / sizeof(mc_statistic *))
        return array_fail(classes, pc, MC_REGISTRY_LIMIT,
                          "Statistics native array allocation bound");
    mc_statistic **array = mc_registry_allocate(classes, length * sizeof(*array));
    if (array == NULL)
        return array_error(classes, pc);
    /* Registry allocation is zeroed; this is the actual fresh reference array. */
    *out = array;
    return MC_REGISTRY_OK;
}

static mc_registry_status slot_load(mc_registry_classes *classes, mc_statistic **array,
                                    size_t length, int32_t index, int32_t pc, mc_statistic **out) {
    if (array == NULL)
        return array_fail(classes, pc, MC_REGISTRY_NULL_REFERENCE,
                          "Statistics local array is null");
    if (index < 0 || (size_t)index >= length)
        return array_fail(classes, pc, MC_REGISTRY_INDEX_OUT_OF_RANGE,
                          "Statistics local array index");
    *out = array[(size_t)index];
    return MC_REGISTRY_OK;
}

static mc_registry_status slot_store(mc_registry_classes *classes, mc_statistic **array,
                                     size_t length, int32_t index, mc_statistic *value,
                                     int32_t pc) {
    if (array == NULL)
        return array_fail(classes, pc, MC_REGISTRY_NULL_REFERENCE,
                          "Statistics local array is null");
    if (index < 0 || (size_t)index >= length)
        return array_fail(classes, pc, MC_REGISTRY_INDEX_OUT_OF_RANGE,
                          "Statistics local array index");
    array[(size_t)index] = value;
    return MC_REGISTRY_OK;
}

static mc_registry_status list_remove(mc_registry_classes *classes, mc_registry_list *list,
                                      mc_statistic *value, int32_t pc) {
    if (list == NULL)
        return array_fail(classes, pc, MC_REGISTRY_NULL_REFERENCE,
                          "Statistics List.remove null receiver");
    /* Stat does not override equals. NULL removes the first NULL element too. */
    for (size_t i = 0; i < list->count; ++i) {
        if (list->elements[i] == value) {
            for (size_t j = i + 1; j < list->count; ++j)
                list->elements[j - 1] = list->elements[j];
            --list->count;
            list->elements[list->count] = NULL;
            break;
        }
    }
    return MC_REGISTRY_OK;
}

/* jl/gg.a(Stat[],int,int): preserve the reloads and b,e,c removal order.
 * In particular first==second does not eliminate the removal branch. */
static mc_registry_status pair(mc_registry_classes *classes, mc_statistic **array, size_t length,
                               int32_t first, int32_t second) {
    mc_statistic *value = NULL;
    mc_registry_status status = slot_load(classes, array, length, first, 2, &value);
    if (status != MC_REGISTRY_OK)
        return status;
    if (value != NULL) {
        status = slot_load(classes, array, length, second, 8, &value);
        if (status != MC_REGISTRY_OK)
            return status;
        if (value == NULL) {
            status = slot_load(classes, array, length, first, 16, &value);
            if (status != MC_REGISTRY_OK)
                return status;
            return slot_store(classes, array, length, second, value, 17);
        }
    }

    mc_registry_list *list = classes->statistics.b;
    status = slot_load(classes, array, length, first, 24, &value);
    if (status != MC_REGISTRY_OK)
        return status;
    status = list_remove(classes, list, value, 25);
    if (status != MC_REGISTRY_OK)
        return status;
    list = classes->statistics.e;
    status = slot_load(classes, array, length, first, 36, &value);
    if (status != MC_REGISTRY_OK)
        return status;
    status = list_remove(classes, list, value, 37);
    if (status != MC_REGISTRY_OK)
        return status;
    list = classes->statistics.c;
    status = slot_load(classes, array, length, first, 48, &value);
    if (status != MC_REGISTRY_OK)
        return status;
    status = list_remove(classes, list, value, 49);
    if (status != MC_REGISTRY_OK)
        return status;
    status = slot_load(classes, array, length, second, 59, &value);
    if (status != MC_REGISTRY_OK)
        return status;
    return slot_store(classes, array, length, first, value, 60);
}

static mc_registry_status unify(mc_registry_classes *classes, mc_statistic **array, size_t length) {
    static const struct {
        const char *first;
        const char *second;
    } pairs[] = {{"C", "B"},   {"E", "E"},   {"bg", "bb"}, {"aD", "aC"}, {"aP", "aO"}, {"bj", "bi"},
                 {"aR", "aQ"}, {"ah", "ag"}, {"ak", "al"}, {"v", "w"},   {"aB", "w"}};

    for (size_t i = 0; i < sizeof(pairs) / sizeof(*pairs); ++i) {
        /* Each group is sixteen bytecodes apart. Static named-cell reads
         * independently perform Block active use before the following bn read. */
        const int32_t pc = (int32_t)(i * 16u);
        mc_block *block = NULL;
        mc_registry_status status = mc_registry_block_static_load(classes, pairs[i].first, &block);
        if (status != MC_REGISTRY_OK)
            return status;
        if (block == NULL)
            return array_fail(classes, pc + 4, MC_REGISTRY_NULL_REFERENCE,
                              "Statistics first unification Block is null");
        const int32_t first = block->fields.bn;
        status = mc_registry_block_static_load(classes, pairs[i].second, &block);
        if (status != MC_REGISTRY_OK)
            return status;
        if (block == NULL)
            return array_fail(classes, pc + 10, MC_REGISTRY_NULL_REFERENCE,
                              "Statistics second unification Block is null");
        const int32_t second = block->fields.bn;
        status = pair(classes, array, length, first, second);
        if (status != MC_REGISTRY_OK)
            return status;
    }
    return MC_REGISTRY_OK;
}

static mc_registry_status item_statistic(mc_registry_classes *classes, int32_t allocation_pc,
                                         int32_t base, int32_t index,
                                         const mc_registry_string *name, mc_statistic **out) {
    mc_statistic *statistic = NULL;
    mc_registry_status status =
        mc_registry_statistic_allocate(classes, MC_STATISTIC_ITEM, &statistic);
    if (status != MC_REGISTRY_OK) {
        /* A failed real ancestor active use owns its nested first cause. */
        if (classes->statistic_base_state.phase == MC_REGISTRY_FAILED)
            return status;
        return array_error(classes, allocation_pc);
    }
    status = mc_registry_statistic_construct(statistic, mc_java_i32_add(base, index), name,
                                             classes->statistic_base.i, index);
    if (status != MC_REGISTRY_OK)
        return status;
    status = mc_registry_statistic_register(statistic);
    if (status != MC_REGISTRY_OK)
        return status;
    *out = statistic;
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_statistic_block_array(mc_registry_classes *classes,
                                                     const mc_registry_string *key, int32_t base,
                                                     mc_statistic ***out) {
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (classes->entered == 0 || out == NULL)
        return array_fail(classes, 0, MC_REGISTRY_INVALID_ARGUMENT,
                          "Block statistics require admitted initialization and output");
    mc_statistic **array = NULL;
    mc_registry_status status = new_array(classes, 256u, 3, &array);
    if (status != MC_REGISTRY_OK)
        return status;
    /* Source bounded loop: 0..255 with terminal i==256; plain ++ cannot wrap. */
    for (int32_t i = 0; i < 256; ++i) {
        mc_block *block = NULL;
        status = block_load(classes, i, 20, &block);
        if (status != MC_REGISTRY_OK)
            return status;
        if (block == NULL)
            continue;
        status = block_load(classes, i, 28, &block);
        if (status != MC_REGISTRY_OK)
            return status;
        if (block == NULL)
            return array_fail(classes, 29, MC_REGISTRY_NULL_REFERENCE,
                              "Block statistics predicate null receiver");
        bool admitted = false;
        status = mc_registry_statistic_block_admitted(classes, block, &admitted);
        if (status != MC_REGISTRY_OK)
            return status;
        if (!admitted)
            continue;
        status = block_load(classes, i, 46, &block);
        if (status != MC_REGISTRY_OK)
            return status;
        if (block == NULL)
            return array_fail(classes, 47, MC_REGISTRY_NULL_REFERENCE,
                              "Block statistics name null receiver");
        const mc_registry_string *argument = NULL;
        status = mc_registry_statistic_block_name(classes, block, &argument);
        if (status != MC_REGISTRY_OK)
            return status;
        const mc_registry_string *name = NULL;
        status = mc_registry_translate_formatted(classes, key, &argument, 1u, &name);
        if (status != MC_REGISTRY_OK)
            return status;

        /* The destination array is this local owner, captured before new58. */
        mc_statistic *statistic = NULL;
        status = item_statistic(classes, 58, base, i, name, &statistic);
        if (status != MC_REGISTRY_OK)
            return status;
        status = slot_store(classes, array, 256u, i, statistic, 74);
        if (status != MC_REGISTRY_OK)
            return status;
        mc_registry_list *list = classes->statistics.e;
        status = slot_load(classes, array, 256u, i, 80, &statistic);
        if (status != MC_REGISTRY_OK)
            return status;
        status = mc_registry_statistic_list_add(classes, list, statistic);
        if (status != MC_REGISTRY_OK)
            return array_error(classes, 84);
    }
    status = unify(classes, array, 256u);
    if (status != MC_REGISTRY_OK)
        return status;
    *out = array;
    return MC_REGISTRY_OK;
}

static mc_registry_status item_array(mc_registry_classes *classes, mc_statistic **array,
                                     const mc_registry_string *key, int32_t base, int32_t begin,
                                     int32_t end, bool breaking, mc_statistic ***out) {
    mc_registry_status status;
    if (array == NULL) {
        status = new_array(classes, 32000u, 7, &array);
        if (status != MC_REGISTRY_OK)
            return status;
    }
    /* All callers supply a nonnegative real Block extent or zero and end
     * extent/32000. Successful Item aaload bounds each reached index. */
    for (int32_t i = begin; i < end; i = mc_java_i32_add(i, 1)) {
        mc_item *item = NULL;
        status = item_load(classes, i, 26, &item);
        if (status != MC_REGISTRY_OK)
            return status;
        if (item == NULL)
            continue;
        if (breaking) {
            status = item_load(classes, i, 35, &item);
            if (status != MC_REGISTRY_OK)
                return status;
            if (item == NULL)
                return array_fail(classes, 36, MC_REGISTRY_NULL_REFERENCE,
                                  "Break statistics durability null receiver");
            bool damageable = false;
            status = mc_registry_statistic_item_damageable(classes, item, &damageable);
            if (status != MC_REGISTRY_OK)
                return status;
            if (!damageable)
                continue;
        }
        status = item_load(classes, i, breaking ? 54 : 42, &item);
        if (status != MC_REGISTRY_OK)
            return status;
        if (item == NULL)
            return array_fail(classes, breaking ? 55 : 43, MC_REGISTRY_NULL_REFERENCE,
                              "Item statistics name null receiver");
        const mc_registry_string *argument = NULL;
        status = mc_registry_statistic_item_name(classes, item, &argument);
        if (status != MC_REGISTRY_OK)
            return status;
        const mc_registry_string *name = NULL;
        status = mc_registry_translate_formatted(classes, key, &argument, 1u, &name);
        if (status != MC_REGISTRY_OK)
            return status;
        mc_statistic *statistic = NULL;
        status = item_statistic(classes, breaking ? 67 : 55, base, i, name, &statistic);
        if (status != MC_REGISTRY_OK)
            return status;
        status = slot_store(classes, array, 32000u, i, statistic, breaking ? 85 : 73);
        if (status != MC_REGISTRY_OK)
            return status;
        if (!breaking) {
            int32_t length = 0;
            status = block_length(classes, 79, &length);
            if (status != MC_REGISTRY_OK)
                return status;
            if (i >= length) {
                mc_registry_list *list = classes->statistics.d;
                status = slot_load(classes, array, 32000u, i, 89, &statistic);
                if (status != MC_REGISTRY_OK)
                    return status;
                status = mc_registry_statistic_list_add(classes, list, statistic);
                if (status != MC_REGISTRY_OK)
                    return array_error(classes, 93);
            }
        }
    }
    status = unify(classes, array, 32000u);
    if (status != MC_REGISTRY_OK)
        return status;
    *out = array;
    return MC_REGISTRY_OK;
}

static void *set_allocate(void *context, size_t bytes) {
    return mc_registry_allocate(context, bytes);
}

static mc_registry_status recipe_set_error(mc_registry_classes *classes,
                                           mc_java_int_set_status status, int32_t pc) {
    if (status == MC_JAVA_INT_SET_ALLOCATION_FAILED &&
        (classes->last_error.status == MC_REGISTRY_LIMIT ||
         classes->last_error.status == MC_REGISTRY_ALLOCATION_FAILED))
        return array_error(classes, pc);
    return array_fail(classes, pc,
                      status == MC_JAVA_INT_SET_LIMIT               ? MC_REGISTRY_LIMIT
                      : status == MC_JAVA_INT_SET_ALLOCATION_FAILED ? MC_REGISTRY_ALLOCATION_FAILED
                                                                    : MC_REGISTRY_INVALID_ARGUMENT,
                      "Statistics recipe Integer HashSet operation");
}

static mc_registry_status recipe_add_output(mc_registry_classes *classes, mc_java_int_set *set,
                                            const mc_item_stack *output, int32_t read_pc,
                                            int32_t add_pc) {
    if (output == NULL)
        return array_fail(classes, read_pc, MC_REGISTRY_NULL_REFERENCE,
                          "Statistics recipe output ID read on null");
    mc_item_stack_fields fields;
    if (mc_item_stack_fields_get(output, &fields) != MC_ITEM_STACK_OK)
        return array_fail(classes, read_pc, MC_REGISTRY_INVALID_ARGUMENT,
                          "Statistics recipe output native owner");
    bool changed = false;
    const mc_java_int_set_status status = mc_java_int_set_add(set, fields.id, &changed);
    return status == MC_JAVA_INT_SET_OK ? MC_REGISTRY_OK
                                        : recipe_set_error(classes, status, add_pc);
}

mc_registry_status mc_registry_statistic_recipes(mc_registry_classes *classes) {
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (classes->entered == 0)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT,
                                "Statistics recipe population requires admitted class access",
                                NULL);
    if (!classes->statistics.G)
        return MC_REGISTRY_OK;
    if (!classes->statistics.H)
        return MC_REGISTRY_OK;
    /* new13/constructor17: a real default Integer HashSet owner, with every
     * native allocation retained and charged by the common class arena.
     * Combined native allocation does not claim Java allocation/OOM timing. */
    const mc_java_int_set_allocator allocator = {classes, set_allocate, NULL};
    mc_java_int_set *set = NULL;
    mc_java_int_set_status set_status =
        mc_java_int_set_create_with_allocator(NULL, &allocator, &set);
    if (set_status != MC_JAVA_INT_SET_OK) {
        if (set_status == MC_JAVA_INT_SET_ALLOCATION_FAILED)
            return array_error(classes, 17); /* actual arena LIMIT/allocation cause */
        return array_fail(classes, 17,
                          set_status == MC_JAVA_INT_SET_LIMIT ? MC_REGISTRY_LIMIT
                                                              : MC_REGISTRY_INVALID_ARGUMENT,
                          "Statistics empty Integer HashSet construction failed");
    }
    mc_registry_status status = mc_registry_crafting_active_use(classes); /*21*/
    if (status != MC_REGISTRY_OK)
        return status;
    mc_crafting_manager *crafting = classes->recipes.crafting;
    if (crafting == NULL)
        return array_fail(classes, 24, MC_REGISTRY_NULL_REFERENCE,
                          "Statistics crafting List getter null receiver");
    mc_registry_list *recipes = crafting->b; /* getter24 returns the actual list */
    if (recipes == NULL)
        return array_fail(classes, 27, MC_REGISTRY_NULL_REFERENCE,
                          "Statistics crafting List.iterator null receiver");
    for (size_t i = 0; i < recipes->count; ++i) {
        const mc_recipe *recipe = recipes->elements[i];
        if (recipe == NULL)
            return array_fail(classes, 54, MC_REGISTRY_NULL_REFERENCE,
                              "Statistics recipe output getter null receiver");
        if (mc_recipe_kind_get(recipe) >= MC_RECIPE_KIND_COUNT)
            return array_fail(classes, 48, MC_REGISTRY_ILLEGAL_ARGUMENT,
                              "Statistics native recipe type");
        status = recipe_add_output(classes, set, mc_recipe_output(recipe), 59, 65);
        if (status != MC_REGISTRY_OK)
            return status;
    }
    status = mc_registry_smelting_active_use(classes); /*72*/
    if (status != MC_REGISTRY_OK)
        return status;
    mc_smelting_manager *smelting = classes->recipes.smelting;
    if (smelting == NULL)
        return array_fail(classes, 75, MC_REGISTRY_NULL_REFERENCE,
                          "Statistics smelting Map getter null receiver");
    if (smelting->b == NULL)
        return array_fail(classes, 78, MC_REGISTRY_NULL_REFERENCE,
                          "Statistics smelting Map.values null receiver");
    int32_t *keys = NULL;
    size_t count = 0;
    status = mc_registry_smelting_keys(classes, smelting, &keys, &count);
    if (status != MC_REGISTRY_OK)
        return array_error(classes, 83);
    for (size_t i = 0; i < count; ++i) {
        const mc_item_stack *output = NULL;
        status = mc_smelting_manager_find(smelting, keys[i], &output);
        if (status != MC_REGISTRY_OK)
            return array_error(classes, 99);
        status = recipe_add_output(classes, set, output, 110, 116);
        if (status != MC_REGISTRY_OK)
            return status;
    }
    mc_statistic **array = NULL;
    status = new_array(classes, 32000u, 126, &array);
    if (status != MC_REGISTRY_OK)
        return status;
    classes->statistics.D = array; /*129, before the set iterator133*/
    count = mc_java_int_set_size(set);
    if (count > SIZE_MAX / sizeof(*keys))
        return array_fail(classes, 133, MC_REGISTRY_LIMIT, "Statistics recipe traversal bound");
    keys = NULL;
    if (count != 0) {
        keys = mc_registry_allocate(classes, count * sizeof(*keys));
        if (keys == NULL)
            return array_error(classes, 133);
    }
    size_t copied = 0;
    set_status = mc_java_int_set_copy_order(set, keys, count, &copied);
    if (set_status != MC_JAVA_INT_SET_OK)
        return recipe_set_error(classes, set_status, 133);
    if (copied != count)
        return array_fail(classes, 133, MC_REGISTRY_INVALID_ARGUMENT,
                          "Statistics recipe traversal size invariant");
    for (size_t i = 0; i < count; ++i) {
        const int32_t id = keys[i];
        mc_item *item = NULL;
        status = item_load(classes, id, 163, &item);
        if (status != MC_REGISTRY_OK)
            return status;
        if (item == NULL)
            continue;
        const mc_registry_string *key = NULL;
        status = mc_registry_literal_ascii(classes, "stat.craftItem", &key);
        if (status != MC_REGISTRY_OK)
            return array_error(classes, 167);
        /* The name expression reloads current Item.c independently at175. */
        status = item_load(classes, id, 182, &item);
        if (status != MC_REGISTRY_OK)
            return status;
        if (item == NULL)
            return array_fail(classes, 183, MC_REGISTRY_NULL_REFERENCE,
                              "Statistics craft Item name null receiver");
        const mc_registry_string *argument = NULL;
        status = mc_registry_statistic_item_name(classes, item, &argument);
        if (status != MC_REGISTRY_OK)
            return status;
        const mc_registry_string *name = NULL;
        status = mc_registry_translate_formatted(classes, key, &argument, 1u, &name);
        if (status != MC_REGISTRY_OK)
            return status;
        /* Source captures D191/index195 before new198/constructor214 and
         * virtual registration217. A nested call can replace static D without
         * changing this outer store's actual captured array. */
        mc_statistic **captured = classes->statistics.D;
        const int32_t index = id;
        mc_statistic *statistic = NULL;
        status = item_statistic(classes, 198, 16842752, id, name, &statistic);
        if (status != MC_REGISTRY_OK)
            return status;
        status = slot_store(classes, captured, 32000u, index, statistic, 220);
        if (status != MC_REGISTRY_OK)
            return status;
    }
    return unify(classes, classes->statistics.D, 32000u); /*current D224/call227*/
}

static mc_registry_status populate_items(mc_registry_classes *classes, bool other) {
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (classes->entered == 0)
        return array_fail(classes, 0, MC_REGISTRY_INVALID_ARGUMENT,
                          "Item statistics require admitted initialization");
    mc_registry_status status = mc_registry_statistics_active_use(classes);
    if (status != MC_REGISTRY_OK)
        return status;

    mc_statistic **previous = classes->statistics.E; /* getstatic0 before ldc3 */
    const mc_registry_string *key = NULL;
    status = mc_registry_literal_ascii(classes, "stat.useItem", &key);
    if (status != MC_REGISTRY_OK)
        return array_error(classes, 3);
    int32_t length = 0;
    status = block_length(classes, other ? 10 : 11, &length);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_statistic **result = NULL;
    status = item_array(classes, previous, key, 16908288, other ? length : 0,
                        other ? 32000 : length, false, &result);
    if (status != MC_REGISTRY_OK)
        return status;
    classes->statistics.E = result; /* b15 / c17 */

    previous = classes->statistics.F; /* b18 / c20 before key literal */
    status = mc_registry_literal_ascii(classes, "stat.breakItem", &key);
    if (status != MC_REGISTRY_OK)
        return array_error(classes, other ? 23 : 21);
    status = block_length(classes, other ? 30 : 29, &length);
    if (status != MC_REGISTRY_OK)
        return status;
    status = item_array(classes, previous, key, 16973824, other ? length : 0,
                        other ? 32000 : length, true, &result);
    if (status != MC_REGISTRY_OK)
        return status;
    classes->statistics.F = result; /* b33 / c37 */
    if (other)
        classes->statistics.H = true; /* c41 */
    else
        classes->statistics.G = true;              /* b37 */
    return mc_registry_statistic_recipes(classes); /* b40 / c44 */
}

mc_registry_status mc_registry_statistic_block_items(mc_registry_classes *classes) {
    return populate_items(classes, false);
}

mc_registry_status mc_registry_statistic_other_items(mc_registry_classes *classes) {
    return populate_items(classes, true);
}
