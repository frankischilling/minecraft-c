#include "recipes_internal.h"

#include "../../block/registry/registry_internal.h"
#include "../registry/stacks_internal.h"

#include <limits.h>
#include <string.h>

/* Source methods have their own operation positions without moving the
 * enclosing class initializer's call-site position. */
typedef struct recipe_call {
    mc_registry_classes *classes;
    const char *client, *server;
    int32_t pc;
} recipe_call;

typedef struct recipe_text {
    size_t length;
} recipe_text;

_Static_assert(_Alignof(recipe_text) >= _Alignof(uint16_t) &&
                   sizeof(recipe_text) % _Alignof(uint16_t) == 0,
               "Recipe text trailing UTF16 storage must stay aligned");

/* The same allocation holds this header followed by length uint16_t units.
 * Avoid a flexible-array member: the supported MSVC profiles differ in the
 * diagnostic they issue for that otherwise standard C declaration. */
static uint16_t *text_units(recipe_text *text)
{
    return (uint16_t *)(void *)(text + 1);
}

typedef struct recipe_binding {
    bool null_key;
    uint16_t key;
    mc_item_stack *value;
    struct recipe_binding *next;
} recipe_binding;

typedef struct recipe_map {
    recipe_binding *first;
} recipe_map;

static mc_registry_status own_error(const recipe_call *call)
{
    call->classes->last_error.constructor_class =
        call->classes->options.side == MC_REGISTRY_CLIENT ? call->client : call->server;
    call->classes->last_error.constructor_pc = call->pc;
    return call->classes->last_error.status;
}

static mc_registry_status fail(const recipe_call *call, mc_registry_status status,
                               const char *message)
{
    (void)mc_registry_fail(call->classes, status, message, NULL);
    return own_error(call);
}

static void *allocate(const recipe_call *call, size_t bytes)
{
    void *allocation = mc_registry_allocate(call->classes, bytes);
    if (allocation == NULL)
        (void)own_error(call);
    return allocation;
}

static mc_registry_status admit(mc_crafting_manager *manager)
{
    if (manager == NULL || manager->classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (manager->classes->entered == 0)
        return mc_registry_fail(manager->classes, MC_REGISTRY_INVALID_ARGUMENT,
                                "Recipe registration requires admitted construction", NULL);
    return MC_REGISTRY_OK;
}

static bool nonnull(const mc_recipe_argument *argument)
{
    switch (argument->kind) {
        case MC_RECIPE_ARGUMENT_NULL:
            return false;
        case MC_RECIPE_ARGUMENT_ITEM:
            return argument->value.item != NULL;
        case MC_RECIPE_ARGUMENT_BLOCK:
            return argument->value.block != NULL;
        case MC_RECIPE_ARGUMENT_STACK:
            return argument->value.stack != NULL;
        case MC_RECIPE_ARGUMENT_STRING:
            return argument->value.string != NULL;
        case MC_RECIPE_ARGUMENT_STRINGS:
            return argument->value.strings.values != NULL;
        case MC_RECIPE_ARGUMENT_CHARACTER:
            return true;
    }
    return true;
}

static bool instance(const mc_recipe_argument *argument, mc_recipe_argument_kind kind)
{
    return argument->kind == kind && nonnull(argument);
}

static mc_registry_status argument_at(recipe_call *call, const mc_recipe_argument *arguments,
                                      size_t count, size_t index, const mc_recipe_argument **out)
{
    if (arguments == NULL)
        return fail(call, MC_REGISTRY_NULL_REFERENCE, "Recipe Object[] is null");
    if (index >= count)
        return fail(call, MC_REGISTRY_INDEX_OUT_OF_RANGE, "Recipe Object[] index");
    if ((unsigned int)arguments[index].kind > (unsigned int)MC_RECIPE_ARGUMENT_CHARACTER)
        return fail(call, MC_REGISTRY_INVALID_ARGUMENT, "Invalid native recipe argument tag");
    *out = &arguments[index];
    return MC_REGISTRY_OK;
}

/* Native retained UTF16 concatenations represent the parser's successive
 * Strings; no StringBuilder heap-allocation equivalence is implied. */
static mc_registry_status append_row(recipe_call *call, const mc_registry_string *row,
                                     recipe_text **text, int32_t *width, int32_t *height,
                                     bool array_rows)
{
    *height = mc_java_i32_add(*height, 1);
    call->pc = array_rows ? 60 : 124;
    if (row == NULL)
        return fail(call, MC_REGISTRY_NULL_REFERENCE, "Recipe String row is null");
    if (row->length > (size_t)INT32_MAX)
        return fail(call, MC_REGISTRY_LIMIT, "Recipe native String length bound");
    *width = (int32_t)row->length;
    const size_t old_length = *text == NULL ? 0 : (*text)->length;
    call->pc = array_rows ? 65 : 126;
    if (row->length > (size_t)INT32_MAX - old_length)
        return fail(call, MC_REGISTRY_LIMIT, "Recipe native concatenation length bound");
    const size_t length = old_length + row->length;
    if (length > (SIZE_MAX - sizeof(recipe_text)) / sizeof(uint16_t))
        return fail(call, MC_REGISTRY_LIMIT, "Recipe native concatenation allocation bound");
    recipe_text *joined = allocate(call, sizeof(*joined) + length * sizeof(uint16_t));
    if (joined == NULL)
        return call->classes->last_error.status;
    joined->length = length;
    if (old_length != 0)
        memcpy(text_units(joined), text_units(*text), old_length * sizeof(uint16_t));
    if (row->length != 0)
        memcpy(text_units(joined) + old_length, row->units, row->length * sizeof(uint16_t));
    *text = joined;
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_recipe_list_add(mc_registry_classes *classes, mc_registry_list *list,
                                               void *element)
{
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (classes->entered == 0)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT,
                                "Recipe list mutation requires admitted construction", NULL);
    if (list == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE,
                                "Recipe List.add null receiver", NULL);
    if (list->count >= (size_t)INT32_MAX)
        return mc_registry_fail(classes, MC_REGISTRY_LIMIT, "Recipe native list count bound", NULL);
    if (list->count == list->capacity) {
        size_t capacity = list->capacity == 0 ? 10u : list->capacity;
        if (list->capacity != 0) {
            size_t growth = capacity / 2u;
            if (growth == 0)
                growth = 1;
            capacity =
                growth > (size_t)INT32_MAX - capacity ? (size_t)INT32_MAX : capacity + growth;
        }
        if (capacity > SIZE_MAX / sizeof(void *))
            return mc_registry_fail(classes, MC_REGISTRY_LIMIT,
                                    "Recipe native list allocation bound", NULL);
        void **elements = mc_registry_allocate(classes, capacity * sizeof(*elements));
        if (elements == NULL)
            return classes->last_error.status;
        for (size_t i = 0; i < list->count; ++i)
            elements[i] = list->elements[i];
        list->elements = elements;
        list->capacity = capacity;
    }
    list->elements[list->count] = element;
    ++list->count;
    return MC_REGISTRY_OK;
}

static recipe_binding *find_binding(recipe_map *map, bool null_key, uint16_t key)
{
    for (recipe_binding *binding = map->first; binding != NULL; binding = binding->next)
        if (binding->null_key == null_key && (null_key || binding->key == key))
            return binding;
    return NULL;
}

static mc_registry_status put_binding(recipe_call *call, recipe_map *map, bool null_key,
                                      uint16_t key, mc_item_stack *value)
{
    recipe_binding *binding = find_binding(map, null_key, key);
    if (binding == NULL) {
        binding = allocate(call, sizeof(*binding));
        if (binding == NULL)
            return call->classes->last_error.status;
        *binding = (recipe_binding){.null_key = null_key, .key = key, .next = map->first};
        map->first = binding;
    }
    binding->value = value;
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_recipe_shaped(mc_crafting_manager *manager, mc_item_stack *output,
                                             const mc_recipe_argument *arguments, size_t count)
{
    mc_registry_status status = admit(manager);
    if (status != MC_REGISTRY_OK)
        return status;
    recipe_call call = {manager->classes, "hk", "ey", 15};
    if (count > (size_t)INT32_MAX)
        return fail(&call, MC_REGISTRY_LIMIT, "Recipe native Object[] count bound");
    const mc_recipe_argument *argument = NULL;
    status = argument_at(&call, arguments, count, 0, &argument);
    if (status != MC_REGISTRY_OK)
        return status;
    size_t index = 0;
    int32_t width = 0, height = 0;
    recipe_text *text = NULL;
    if (instance(argument, MC_RECIPE_ARGUMENT_STRINGS)) {
        ++index;
        call.pc = 44;
        if (argument->value.strings.count > (size_t)INT32_MAX)
            return fail(&call, MC_REGISTRY_LIMIT, "Recipe native String[] count bound");
        for (size_t row = 0; row < argument->value.strings.count; ++row) {
            status = append_row(&call, argument->value.strings.values[row], &text, &width, &height,
                                true);
            if (status != MC_REGISTRY_OK)
                return status;
        }
    } else {
        for (;;) {
            call.pc = 97;
            status = argument_at(&call, arguments, count, index, &argument);
            if (status != MC_REGISTRY_OK)
                return status;
            if (!instance(argument, MC_RECIPE_ARGUMENT_STRING))
                break;
            ++index;
            status = append_row(&call, argument->value.string, &text, &width, &height, false);
            if (status != MC_REGISTRY_OK)
                return status;
        }
    }
    call.pc = 149;
    recipe_map *map = allocate(&call, sizeof(*map));
    if (map == NULL)
        return call.classes->last_error.status;
    *map = (recipe_map){0};
    while (index < count) {
        call.pc = 169;
        status = argument_at(&call, arguments, count, index, &argument);
        if (status != MC_REGISTRY_OK)
            return status;
        const bool null_key = !nonnull(argument);
        if (!null_key && argument->kind != MC_RECIPE_ARGUMENT_CHARACTER)
            return fail(&call, MC_REGISTRY_ILLEGAL_ARGUMENT, "Recipe Character checkcast");
        const uint16_t key = null_key ? 0 : argument->value.character;
        call.pc = 182;
        status = argument_at(&call, arguments, count, index + 1, &argument);
        if (status != MC_REGISTRY_OK)
            return status;
        mc_item_stack *value = NULL;
        if (instance(argument, MC_RECIPE_ARGUMENT_ITEM)) {
            call.pc = 189;
            status = mc_registry_stack_allocate(call.classes, &value);
            if (status != MC_REGISTRY_OK)
                return own_error(&call);
            call.pc = 202;
            status = mc_registry_stack_construct_item(call.classes, value, argument->value.item);
            if (status != MC_REGISTRY_OK)
                return status;
        } else if (instance(argument, MC_RECIPE_ARGUMENT_BLOCK)) {
            call.pc = 222;
            status = mc_registry_stack_allocate(call.classes, &value);
            if (status != MC_REGISTRY_OK)
                return own_error(&call);
            call.pc = 237;
            status = mc_registry_stack_construct_block_values(call.classes, value,
                                                              argument->value.block, 1, -1);
            if (status != MC_REGISTRY_OK)
                return status;
        } else if (instance(argument, MC_RECIPE_ARGUMENT_STACK)) {
            value = argument->value.stack;
        }
        call.pc = 274;
        status = put_binding(&call, map, null_key, key, value);
        if (status != MC_REGISTRY_OK)
            return status;
        index += 2;
    }
    call.pc = 291;
    const int32_t product = mc_java_i32_mul(width, height);
    if (product < 0)
        return fail(&call, MC_REGISTRY_ILLEGAL_ARGUMENT, "Recipe negative ingredient-array length");
    const size_t length = (size_t)product;
    if (length > SIZE_MAX / sizeof(mc_item_stack *))
        return fail(&call, MC_REGISTRY_LIMIT, "Recipe native ingredient-array allocation bound");
    /* Retain a distinct real empty array too; its native placeholder is not a
     * Java element and is excluded from the inspection extent. */
    mc_item_stack **cells = allocate(&call, (length == 0 ? 1 : length) * sizeof(*cells));
    if (cells == NULL)
        return call.classes->last_error.status;
    for (size_t cell = 0; cell < length; ++cell) {
        call.pc = 312;
        if (text == NULL || cell >= text->length)
            return fail(&call, MC_REGISTRY_INDEX_OUT_OF_RANGE, "Recipe String.charAt index");
        recipe_binding *binding = find_binding(map, false, text_units(text)[cell]);
        if (binding != NULL) {
            call.pc = 351;
            if (binding->value == NULL)
                return fail(&call, MC_REGISTRY_NULL_REFERENCE,
                            "Recipe mapped Stack.copy null receiver");
            status = mc_registry_stack_copy(call.classes, binding->value, &cells[cell]);
            if (status != MC_REGISTRY_OK)
                return status;
        } else {
            cells[cell] = NULL;
        }
    }
    call.pc = 374;
    mc_recipe *recipe = allocate(&call, sizeof(*recipe));
    if (recipe == NULL)
        return call.classes->last_error.status;
    *recipe = (mc_recipe){.classes = call.classes, .kind = MC_RECIPE_SHAPED};
    recipe_call constructor = {call.classes, "is", "ft", 7};
    if (output == NULL)
        return fail(&constructor, MC_REGISTRY_NULL_REFERENCE,
                    "Shaped recipe output.id null receiver");
    mc_item_stack_fields fields;
    if (mc_item_stack_fields_get(output, &fields) != MC_ITEM_STACK_OK)
        return fail(&constructor, MC_REGISTRY_INVALID_ARGUMENT,
                    "Invalid native shaped output Stack");
    recipe->fields.shaped.a = fields.id; /*10*/
    recipe->fields.shaped.b = width;     /*15*/
    recipe->fields.shaped.c = height;    /*20*/
    recipe->fields.shaped.d = cells;     /*25*/
    recipe->fields.shaped.e = output;    /*31*/
    recipe->ingredient_count = length;
    call.pc = 388;
    status = mc_registry_recipe_list_add(call.classes, manager->b, recipe);
    return status == MC_REGISTRY_OK ? status : own_error(&call);
}

mc_registry_status mc_registry_recipe_shapeless(mc_crafting_manager *manager, mc_item_stack *output,
                                                const mc_recipe_argument *arguments, size_t count)
{
    mc_registry_status status = admit(manager);
    if (status != MC_REGISTRY_OK)
        return status;
    recipe_call call = {manager->classes, "hk", "ey", 0};
    mc_registry_list *ingredients = allocate(&call, sizeof(*ingredients));
    if (ingredients == NULL)
        return call.classes->last_error.status;
    *ingredients = (mc_registry_list){0};
    call.pc = 13;
    if (arguments == NULL)
        return fail(&call, MC_REGISTRY_NULL_REFERENCE, "Shapeless recipe Object[] is null");
    if (count > (size_t)INT32_MAX)
        return fail(&call, MC_REGISTRY_LIMIT, "Shapeless native Object[] count bound");
    for (size_t index = 0; index < count; ++index) {
        call.pc = 30;
        const mc_recipe_argument *argument = NULL;
        status = argument_at(&call, arguments, count, index, &argument);
        if (status != MC_REGISTRY_OK)
            return status;
        mc_item_stack *value = NULL;
        if (instance(argument, MC_RECIPE_ARGUMENT_STACK)) {
            call.pc = 47;
            status = mc_registry_stack_copy(call.classes, argument->value.stack, &value);
            if (status != MC_REGISTRY_OK)
                return status;
            call.pc = 50;
        } else if (instance(argument, MC_RECIPE_ARGUMENT_ITEM)) {
            call.pc = 68;
            status = mc_registry_stack_allocate(call.classes, &value);
            if (status != MC_REGISTRY_OK)
                return own_error(&call);
            call.pc = 77;
            status = mc_registry_stack_construct_item(call.classes, value, argument->value.item);
            if (status != MC_REGISTRY_OK)
                return status;
            call.pc = 80;
        } else if (instance(argument, MC_RECIPE_ARGUMENT_BLOCK)) {
            call.pc = 98;
            status = mc_registry_stack_allocate(call.classes, &value);
            if (status != MC_REGISTRY_OK)
                return own_error(&call);
            call.pc = 107;
            status = mc_registry_stack_construct_block(call.classes, value, argument->value.block);
            if (status != MC_REGISTRY_OK)
                return status;
            call.pc = 110;
        } else {
            call.pc = 128;
            return fail(&call, MC_REGISTRY_ILLEGAL_ARGUMENT, "Invalid shapeless recipy!");
        }
        status = mc_registry_recipe_list_add(call.classes, ingredients, value);
        if (status != MC_REGISTRY_OK)
            return own_error(&call);
    }
    call.pc = 139;
    mc_recipe *recipe = allocate(&call, sizeof(*recipe));
    if (recipe == NULL)
        return call.classes->last_error.status;
    *recipe = (mc_recipe){.classes = call.classes, .kind = MC_RECIPE_SHAPELESS};
    recipe->fields.shapeless.a = output;      /*tt/ml6: null is a valid stored reference*/
    recipe->fields.shapeless.b = ingredients; /*11*/
    call.pc = 148;
    status = mc_registry_recipe_list_add(call.classes, manager->b, recipe);
    return status == MC_REGISTRY_OK ? status : own_error(&call);
}

mc_recipe_kind mc_recipe_kind_get(const mc_recipe *recipe)
{
    return recipe == NULL ? MC_RECIPE_KIND_COUNT : recipe->kind;
}

const mc_shaped_recipe_fields *mc_recipe_shaped_fields_get(const mc_recipe *recipe)
{
    return recipe != NULL && recipe->kind == MC_RECIPE_SHAPED ? &recipe->fields.shaped : NULL;
}

const mc_shapeless_recipe_fields *mc_recipe_shapeless_fields_get(const mc_recipe *recipe)
{
    return recipe != NULL && recipe->kind == MC_RECIPE_SHAPELESS ? &recipe->fields.shapeless : NULL;
}

size_t mc_recipe_shaped_array_length(const mc_recipe *recipe)
{
    return recipe != NULL && recipe->kind == MC_RECIPE_SHAPED ? recipe->ingredient_count : 0;
}

const mc_item_stack *mc_recipe_output(const mc_recipe *recipe)
{
    if (recipe == NULL)
        return NULL;
    if (recipe->kind == MC_RECIPE_SHAPED)
        return recipe->fields.shaped.e;
    return recipe->kind == MC_RECIPE_SHAPELESS ? recipe->fields.shapeless.a : NULL;
}

mc_registry_status mc_recipe_size(const mc_recipe *recipe, int32_t *out)
{
    if (recipe == NULL || out == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    int32_t value;
    if (recipe->kind == MC_RECIPE_SHAPED) {
        value = mc_java_i32_mul(recipe->fields.shaped.b, recipe->fields.shaped.c);
    } else if (recipe->kind == MC_RECIPE_SHAPELESS) {
        if (recipe->fields.shapeless.b == NULL)
            return MC_REGISTRY_NULL_REFERENCE;
        if (recipe->fields.shapeless.b->count > (size_t)INT32_MAX)
            return MC_REGISTRY_LIMIT;
        value = (int32_t)recipe->fields.shapeless.b->count;
    } else {
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    *out = value;
    return MC_REGISTRY_OK;
}

size_t mc_recipe_ingredient_list_size(const mc_recipe_ingredient_list *list)
{
    return list == NULL ? 0 : list->count;
}

mc_registry_status mc_recipe_ingredient_list_get(const mc_recipe_ingredient_list *list,
                                                 size_t index, const mc_item_stack **out)
{
    if (list == NULL || out == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (index >= list->count)
        return MC_REGISTRY_INDEX_OUT_OF_RANGE;
    *out = list->elements[index];
    return MC_REGISTRY_OK;
}

size_t mc_crafting_manager_size(const mc_crafting_manager *manager)
{
    return manager == NULL || manager->b == NULL ? 0 : manager->b->count;
}

mc_registry_status mc_crafting_manager_get(const mc_crafting_manager *manager, size_t index,
                                           const mc_recipe **out)
{
    if (manager == NULL || out == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (manager->b == NULL)
        return MC_REGISTRY_NULL_REFERENCE;
    if (index >= manager->b->count)
        return MC_REGISTRY_INDEX_OUT_OF_RANGE;
    *out = manager->b->elements[index];
    return MC_REGISTRY_OK;
}

static int compare(const mc_recipe *left, const mc_recipe *right)
{
    if (left->kind != right->kind)
        return left->kind == MC_RECIPE_SHAPED ? -1 : 1;
    int32_t a = 0, b = 0;
    (void)mc_recipe_size(right, &b); /*ky/gx calls right size before left*/
    (void)mc_recipe_size(left, &a);
    return b < a ? -1 : b > a ? 1 : 0;
}

mc_registry_status mc_registry_recipe_sort(mc_crafting_manager *manager)
{
    mc_registry_status status = admit(manager);
    if (status != MC_REGISTRY_OK)
        return status;
    recipe_call call = {manager->classes, "hk", "ey", 3201};
    if (manager->b == NULL)
        return fail(&call, MC_REGISTRY_NULL_REFERENCE, "Recipe sort null list");
    const size_t count = manager->b->count;
    if (count > (size_t)INT32_MAX)
        return fail(&call, MC_REGISTRY_LIMIT, "Recipe native sort count bound");
    for (size_t i = 0; i < count; ++i) {
        const mc_recipe *recipe = manager->b->elements[i];
        int32_t size;
        if (recipe == NULL)
            return fail(&call, MC_REGISTRY_NULL_REFERENCE, "Recipe comparator null value");
        if (mc_recipe_size(recipe, &size) != MC_REGISTRY_OK)
            return fail(&call, MC_REGISTRY_INVALID_ARGUMENT, "Invalid native comparator recipe");
    }
    /* Native stable insertion sort over the actual list storage. Only the
     * pure ky/gx ordering and tie stability are modeled, not TimSort heap
     * requests or its sequence of comparator calls. */
    for (size_t i = 1; i < count; ++i) {
        mc_recipe *recipe = manager->b->elements[i];
        size_t position = i;
        while (position != 0 && compare(manager->b->elements[position - 1], recipe) > 0) {
            manager->b->elements[position] = manager->b->elements[position - 1];
            --position;
        }
        manager->b->elements[position] = recipe;
    }
    return MC_REGISTRY_OK;
}
