#include "../../block/registry/registry_internal.h"

#include <string.h>

static mc_registry_status diagnostic(mc_registry_error *error, mc_registry_status status,
                                     const char *message)
{
    if (error != NULL)
        *error = (mc_registry_error){.status = status,
                                     .message = message,
                                     .initializer_pc = -1,
                                     .block_id = -1,
                                     .constructor_pc = -1,
                                     .profile_status = MC_JAVA_RANDOM_PROFILE_OK,
                                     .active_initializer_pc = -1,
                                     .item_argument = -1};
    return status;
}

mc_registry_status mc_registry_classes_initialize_items(mc_registry_classes *classes,
                                                        mc_registry_error *error)
{
    if (classes == NULL)
        return diagnostic(error, MC_REGISTRY_INVALID_ARGUMENT, "registry owner");
    if (classes->entered != 0)
        return diagnostic(error, MC_REGISTRY_BUSY, "registry initialization entered");
    const mc_registry_class_state *state = &classes->item_states[MC_ITEM_GM_EJ];
    if (state->phase == MC_REGISTRY_READY)
        return diagnostic(error, MC_REGISTRY_OK, NULL);
    if (state->phase == MC_REGISTRY_INITIALIZING)
        return diagnostic(error, MC_REGISTRY_BUSY, "registry initialization entered");
    if (state->phase == MC_REGISTRY_FAILED) {
        if (error != NULL) {
            *error = state->failure;
            error->status = MC_REGISTRY_FAILED_INITIALIZATION;
            error->message = "Item initialization previously failed";
        }
        return MC_REGISTRY_FAILED_INITIALIZATION;
    }
    ++classes->entered;
    mc_registry_status status = mc_registry_items_active_use(classes);
    --classes->entered;
    if (status == MC_REGISTRY_OK)
        return diagnostic(error, status, NULL);
    if (error != NULL)
        *error = classes->last_error;
    return status;
}

mc_registry_status mc_registry_items_active_use(mc_registry_classes *classes)
{
    if (classes->entered == 0)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT,
                                "Item active use requires an admitted initialization flow", NULL);
    mc_registry_class_state *state = &classes->item_states[MC_ITEM_GM_EJ];
    if (state->phase == MC_REGISTRY_READY || state->phase == MC_REGISTRY_INITIALIZING)
        return MC_REGISTRY_OK;
    if (state->phase == MC_REGISTRY_FAILED) {
        classes->last_error = state->failure;
        classes->last_error.status = MC_REGISTRY_FAILED_INITIALIZATION;
        return MC_REGISTRY_FAILED_INITIALIZATION;
    }
    state->phase = MC_REGISTRY_INITIALIZING;
    state->pc = 0;
    mc_registry_initializer frame;
    mc_registry_initializer_enter(classes, &frame, "gm", "ej", &state->pc);
    mc_registry_status status = MC_REGISTRY_OK;
    mc_java_random *random = mc_registry_allocate(classes, sizeof(*random));
    if (random == NULL) {
        status = classes->last_error.status;
        goto done;
    }
    state->pc = 4;
    mc_java_random_profile_status profile =
        mc_java_random_construct_default(classes->options.random_profile, random);
    if (profile != MC_JAVA_RANDOM_PROFILE_OK) {
        status = mc_registry_fail(classes, MC_REGISTRY_PROFILE_FAILED, "Item default Random", NULL);
        classes->last_error.profile_status = profile;
        goto done;
    }
    state->pc = 7;
    classes->items.b = random;
    state->pc = 13;
    mc_registry_item_array *array = NULL;
    status = mc_registry_item_array_create(classes, 32000u, &array);
    if (status != MC_REGISTRY_OK)
        goto done;
    state->pc = 16;
    classes->items.c = array;
    status = mc_registry_register_items(classes);
    if (status != MC_REGISTRY_OK)
        goto done;
    classes->current_item = NULL;
    state->pc = 2664;
    status = mc_registry_statistic_other_items(classes);
    if (status == MC_REGISTRY_OK)
        state->pc = 2667;
done:
    state->phase = status == MC_REGISTRY_OK ? MC_REGISTRY_READY : MC_REGISTRY_FAILED;
    if (status != MC_REGISTRY_OK)
        state->failure = classes->last_error;
    mc_registry_initializer_leave(classes, &frame);
    return status;
}

mc_registry_status mc_registry_classes_inspect_items(const mc_registry_classes *classes,
                                                     mc_registry_item_view *out,
                                                     mc_registry_error *error)
{
    if (classes == NULL || out == NULL)
        return diagnostic(error, MC_REGISTRY_INVALID_ARGUMENT, "Item view arguments");
    const mc_registry_class_state *state = &classes->item_states[MC_ITEM_GM_EJ];
    *out = (mc_registry_item_view){.phase = state->phase,
                                   .initializer_pc = state->pc,
                                   .named_items = classes->items.named_items,
                                   .b = classes->items.b,
                                   .c = classes->items.c};
    return diagnostic(error, MC_REGISTRY_OK, NULL);
}

mc_registry_status mc_registry_classes_inspect_item_static(const mc_registry_classes *classes,
                                                           const char *name, const mc_item **out,
                                                           mc_registry_error *error)
{
    if (classes == NULL || name == NULL || out == NULL)
        return diagnostic(error, MC_REGISTRY_INVALID_ARGUMENT, "Item static view arguments");
    for (size_t i = 0; i < 106; ++i)
        if (strcmp(mc_registry_item_static_name(i), name) == 0) {
            *out = classes->items.named[i];
            return diagnostic(error, MC_REGISTRY_OK, NULL);
        }
    return diagnostic(error, MC_REGISTRY_INVALID_ARGUMENT, "unknown Item static name");
}

const mc_item_fields *mc_item_fields_get(const mc_item *item)
{
    return item == NULL ? NULL : &item->fields;
}

size_t mc_registry_item_array_length(const mc_registry_item_array *array)
{
    return array == NULL ? 0 : array->length;
}

mc_registry_status mc_registry_item_array_get(const mc_registry_item_array *array, size_t index,
                                              const mc_item **out, mc_registry_error *error)
{
    if (array == NULL || out == NULL || (array->length != 0 && array->elements == NULL))
        return diagnostic(error, MC_REGISTRY_INVALID_ARGUMENT, "Item array view arguments");
    if (index >= array->length)
        return diagnostic(error, MC_REGISTRY_INDEX_OUT_OF_RANGE, "Item array view index");
    *out = array->elements[index];
    return diagnostic(error, MC_REGISTRY_OK, NULL);
}

mc_registry_status mc_registry_item_allocate(mc_registry_classes *classes, mc_item_kind kind,
                                             mc_item **out)
{
    if ((unsigned int)kind >= (unsigned int)MC_ITEM_KIND_COUNT || out == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT, "Item allocation kind",
                                NULL);
    classes->current_item = NULL;
    mc_registry_status status = mc_registry_item_class_initialize(classes, kind);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_item *item = mc_registry_allocate(classes, sizeof(*item));
    if (item == NULL)
        return classes->last_error.status;
    item->classes = classes;
    item->kind = kind;
    item->phase = MC_ITEM_RAW;
    item->requested_argument = -1;
    item->constructor_pc = -1;
    classes->current_item = item;
    *out = item;
    return MC_REGISTRY_OK;
}

void mc_registry_item_stage(mc_item *item, const char *client, const char *server, int32_t pc)
{
    item->constructor_class = item->classes->options.side == MC_REGISTRY_CLIENT ? client : server;
    item->constructor_pc = pc;
}

mc_registry_status mc_registry_item_base(mc_registry_classes *classes, mc_item *item, int32_t id)
{
    mc_registry_item_stage(item, "gm", "ej", 7);
    item->fields.bg = 64;
    mc_registry_item_stage(item, "gm", "ej", 12);
    item->fields.a = 0;
    mc_registry_item_stage(item, "gm", "ej", 17);
    item->fields.bi = false;
    mc_registry_item_stage(item, "gm", "ej", 22);
    item->fields.bj = false;
    mc_registry_item_stage(item, "gm", "ej", 27);
    item->fields.bk = NULL;
    mc_registry_item_stage(item, "gm", "ej", 36);
    item->fields.bf = mc_java_i32_add(256, id);
    mc_registry_item_stage(item, "gm", "ej", 39);
    mc_registry_status status = mc_registry_items_active_use(classes);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_registry_item_array *first = classes->items.c;
    int32_t first_index = mc_java_i32_add(256, id);
    mc_item *existing = NULL;
    mc_registry_item_stage(item, "gm", "ej", 47);
    status = mc_registry_item_array_load(classes, first, first_index, &existing);
    if (status != MC_REGISTRY_OK)
        return status;
    if (existing != NULL) {
        mc_registry_item_stage(item, "gm", "ej", 51);
        return mc_registry_fail(classes, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY,
                                "Item conflict diagnostic requires actual PrintStream behavior",
                                NULL);
    }
    mc_registry_item_stage(item, "gm", "ej", 76);
    status = mc_registry_items_active_use(classes);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_registry_item_array *second = classes->items.c;
    int32_t second_index = mc_java_i32_add(256, id);
    mc_registry_item_stage(item, "gm", "ej", 85);
    return mc_registry_item_array_store(classes, second, second_index, item);
}

void mc_registry_item_icon(mc_item *item, int32_t x, int32_t y)
{
    item->fields.bh = mc_java_i32_add(x, mc_java_i32_mul(y, 16));
}

void mc_registry_item_icon_direct(mc_item *item, int32_t icon)
{
    item->fields.bh = icon;
}

void mc_registry_item_stack_limit(mc_item *item, int32_t count)
{
    item->fields.bg = count;
}

void mc_registry_item_subtypes(mc_item *item, bool value)
{
    item->fields.bj = value;
}

void mc_registry_item_durability(mc_item *item, int32_t maximum)
{
    item->fields.a = maximum;
}

void mc_registry_item_flag(mc_item *item)
{
    item->fields.bi = true;
}

mc_registry_status mc_registry_item_name(mc_registry_classes *classes, mc_item *item,
                                         const char *suffix)
{
    const mc_registry_string *name = NULL;
    mc_registry_status status =
        mc_registry_string_ascii(classes, "item.", suffix == NULL ? "null" : suffix, &name);
    if (status != MC_REGISTRY_OK)
        return status;
    item->fields.bl = name;
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_item_container(mc_registry_classes *classes, mc_item *item,
                                              mc_item *container)
{
    mc_registry_item_stage(item, "gm", "ej", 5);
    if (item->fields.bg > 1) {
        mc_registry_item_stage(item, "gm", "ej", 8);
        return mc_registry_fail(classes, MC_REGISTRY_ILLEGAL_ARGUMENT,
                                "Max stack size must be 1 for items with crafting results", NULL);
    }
    mc_registry_item_stage(item, "gm", "ej", 20);
    item->fields.bk = container;
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_item_array_create(mc_registry_classes *classes, size_t length,
                                                 mc_registry_item_array **out)
{
    if (length > SIZE_MAX / sizeof(mc_item *) || length > INT32_MAX)
        return mc_registry_fail(classes, MC_REGISTRY_LIMIT, "Item array length", NULL);
    mc_registry_item_array *array = mc_registry_allocate(classes, sizeof(*array));
    if (array == NULL)
        return classes->last_error.status;
    if (length != 0) {
        array->elements = mc_registry_allocate(classes, length * sizeof(*array->elements));
        if (array->elements == NULL)
            return classes->last_error.status;
    }
    array->length = length;
    *out = array;
    return MC_REGISTRY_OK;
}

static mc_registry_status item_index(mc_registry_classes *classes, mc_registry_item_array *array,
                                     int32_t index)
{
    if (array == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE, "Item array reference", NULL);
    if (index < 0 || (size_t)index >= array->length)
        return mc_registry_fail(classes, MC_REGISTRY_INDEX_OUT_OF_RANGE, "Item array index", NULL);
    if (array->elements == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT, "Item native array storage",
                                NULL);
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_item_array_load(mc_registry_classes *classes,
                                               mc_registry_item_array *array, int32_t index,
                                               mc_item **out)
{
    mc_registry_status status = item_index(classes, array, index);
    if (status == MC_REGISTRY_OK)
        *out = array->elements[index];
    return status;
}

mc_registry_status mc_registry_item_array_store(mc_registry_classes *classes,
                                                mc_registry_item_array *array, int32_t index,
                                                mc_item *item)
{
    mc_registry_status status = item_index(classes, array, index);
    if (status == MC_REGISTRY_OK)
        array->elements[index] = item;
    return status;
}

mc_registry_status mc_registry_block_array_create(mc_registry_classes *classes, size_t length,
                                                  mc_registry_block_array **out)
{
    if (length > SIZE_MAX / sizeof(mc_block *) || length > INT32_MAX)
        return mc_registry_fail(classes, MC_REGISTRY_LIMIT, "Block operand array length", NULL);
    mc_registry_block_array *array = mc_registry_allocate(classes, sizeof(*array));
    if (array == NULL)
        return classes->last_error.status;
    if (length != 0) {
        array->elements = mc_registry_allocate(classes, length * sizeof(*array->elements));
        if (array->elements == NULL)
            return classes->last_error.status;
    }
    array->length = length;
    *out = array;
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_block_array_load(mc_registry_classes *classes,
                                                mc_registry_block_array *array, int32_t index,
                                                mc_block **out)
{
    if (array == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE, "Block operand array", NULL);
    if (index < 0 || (size_t)index >= array->length)
        return mc_registry_fail(classes, MC_REGISTRY_INDEX_OUT_OF_RANGE,
                                "Block operand array index", NULL);
    if (array->elements == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT,
                                "Block native operand array storage", NULL);
    *out = array->elements[index];
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_block_static_load(mc_registry_classes *classes, const char *name,
                                                 mc_block **out)
{
    mc_registry_status status = mc_registry_blocks_active_use(classes);
    if (status != MC_REGISTRY_OK)
        return status;
    for (size_t i = 0; i < 96; ++i)
        if (strcmp(mc_registry_block_static_name(i), name) == 0) {
            *out = classes->blocks.named[i];
            return MC_REGISTRY_OK;
        }
    return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT, "unknown Block static operand",
                            NULL);
}

mc_registry_status mc_registry_item_static_load(mc_registry_classes *classes, const char *name,
                                                mc_item **out)
{
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (name == NULL || out == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT, "Item static load arguments",
                                NULL);
    mc_registry_status status = mc_registry_items_active_use(classes);
    if (status != MC_REGISTRY_OK)
        return status;
    for (size_t i = 0; i < 106; ++i)
        if (strcmp(mc_registry_item_static_name(i), name) == 0) {
            *out = classes->items.named[i];
            return MC_REGISTRY_OK;
        }
    return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT, "unknown Item static operand",
                            NULL);
}
