#include "../classes/memory_internal.h"
#include "../construction_internal.h"
#include "entity_internal.h"
#include "inventory_internal.h"
#include "storage_internal.h"

typedef enum inventory_method {
    INVENTORY_SIZE,
    INVENTORY_NAME,
    INVENTORY_LIMIT,
    INVENTORY_CHANGED,
    INVENTORY_GET,
    INVENTORY_SPLIT,
    INVENTORY_SET
} inventory_method;

static mc_minecart_site method_site(const mc_entity *cart, inventory_method method) {
    const bool client = cart->classes->source == MC_ENTITY_CLIENT;
    const char *descriptor;
    switch (method) {
        case INVENTORY_SIZE:
            descriptor = "a()I";
            break;
        case INVENTORY_NAME:
            descriptor = "c()Ljava/lang/String;";
            break;
        case INVENTORY_LIMIT:
            descriptor = "d()I";
            break;
        case INVENTORY_CHANGED:
            descriptor = client ? "y_()V" : "i()V";
            break;
        case INVENTORY_GET:
            descriptor = client ? "f_(I)Liz;" : "d_(I)Lfy;";
            break;
        case INVENTORY_SPLIT:
            descriptor = client ? "a(II)Liz;" : "a(II)Lfy;";
            break;
        case INVENTORY_SET:
            descriptor = client ? "a(ILiz;)V" : "a(ILfy;)V";
            break;
        default:
            descriptor = NULL;
            break;
    }
    return (mc_minecart_site){true, true, client ? "yl" : "pr", descriptor, 0};
}

static mc_minecart_inventory_status finish(mc_minecart_inventory_error *error,
                                           mc_minecart_inventory_error *frame,
                                           mc_minecart_inventory_status status) {
    frame->context.status = status;
    if (error != NULL)
        *error = status == MC_MINECART_INVENTORY_OK ? (mc_minecart_inventory_error){0} : *frame;
    return status;
}

static bool begin(mc_entity *cart, inventory_method method, const char *operation,
                  mc_minecart_inventory_error *frame) {
    frame->context.operation = operation;
    if (!mc_entity_is_kind(cart, MC_ENTITY_CLASS_KIND_MINECART))
        return false;
    frame->context.current = method_site(cart, method);
    return true;
}

static mc_minecart_inventory_status slot_array(mc_entity *cart, int32_t slot, uint16_t pc,
                                               mc_minecart_inventory_error *frame,
                                               mc_minecart_stack_array **out) {
    frame->context.current.bytecode = pc;
    mc_minecart_stack_array *array = mc_minecart_inventory_array_current(cart);
    if (array == NULL)
        return MC_MINECART_INVENTORY_NULL_REFERENCE;
    if (slot < 0 || (size_t)slot >= mc_minecart_inventory_array_length(array))
        return MC_MINECART_INVENTORY_INDEX_OUT_OF_BOUNDS;
    *out = array;
    return MC_MINECART_INVENTORY_OK;
}

static mc_minecart_inventory_status stack_failure(mc_minecart_inventory_error *frame,
                                                  mc_minecart_inventory_stack_cause cause,
                                                  mc_item_stack_status status) {
    frame->context.stack_cause = cause;
    frame->stack = (mc_item_stack_error){.status = status};
    if (status == MC_ITEM_STACK_LIMIT)
        return MC_MINECART_INVENTORY_LIMIT;
    if (status == MC_ITEM_STACK_ALLOCATION_FAILED)
        return MC_MINECART_INVENTORY_ALLOCATION_FAILED;
    return MC_MINECART_INVENTORY_STACK_FAILED;
}

mc_minecart_inventory_status mc_minecart_inventory_size(mc_entity *cart, int32_t *out,
                                                        mc_minecart_inventory_error *error) {
    mc_minecart_inventory_error frame = {0};
    frame.context.operation = "minecart inventory size";
    if (out == NULL || !begin(cart, INVENTORY_SIZE, "minecart inventory size", &frame))
        return finish(error, &frame, MC_MINECART_INVENTORY_INVALID_ARGUMENT);
    *out = 27;
    return finish(error, &frame, MC_MINECART_INVENTORY_OK);
}

mc_minecart_inventory_status mc_minecart_inventory_name(mc_entity *cart, mc_nbt_string *out,
                                                        mc_minecart_inventory_error *error) {
    static const uint16_t name[] = {'M', 'i', 'n', 'e', 'c', 'a', 'r', 't'};
    mc_minecart_inventory_error frame = {0};
    frame.context.operation = "minecart inventory name";
    if (out == NULL || !begin(cart, INVENTORY_NAME, "minecart inventory name", &frame))
        return finish(error, &frame, MC_MINECART_INVENTORY_INVALID_ARGUMENT);
    *out = (mc_nbt_string){name, sizeof(name) / sizeof(name[0])};
    return finish(error, &frame, MC_MINECART_INVENTORY_OK);
}

mc_minecart_inventory_status mc_minecart_inventory_limit(mc_entity *cart, int32_t *out,
                                                         mc_minecart_inventory_error *error) {
    mc_minecart_inventory_error frame = {0};
    frame.context.operation = "minecart inventory limit";
    if (out == NULL || !begin(cart, INVENTORY_LIMIT, "minecart inventory limit", &frame))
        return finish(error, &frame, MC_MINECART_INVENTORY_INVALID_ARGUMENT);
    *out = 64;
    return finish(error, &frame, MC_MINECART_INVENTORY_OK);
}

mc_minecart_inventory_status mc_minecart_inventory_changed(mc_entity *cart,
                                                           mc_minecart_inventory_error *error) {
    mc_minecart_inventory_error frame = {0};
    if (!begin(cart, INVENTORY_CHANGED, "minecart inventory changed", &frame))
        return finish(error, &frame, MC_MINECART_INVENTORY_INVALID_ARGUMENT);
    return finish(error, &frame, MC_MINECART_INVENTORY_OK);
}

mc_minecart_inventory_status mc_minecart_inventory_get(mc_entity *cart, int32_t slot,
                                                       mc_item_stack **out,
                                                       mc_minecart_inventory_error *error) {
    mc_minecart_inventory_error frame = {0};
    frame.context.operation = "minecart inventory get";
    if (out == NULL || !begin(cart, INVENTORY_GET, "minecart inventory get", &frame))
        return finish(error, &frame, MC_MINECART_INVENTORY_INVALID_ARGUMENT);
    frame.context.slot_selected = true;
    frame.context.slot = slot;
    mc_minecart_stack_array *array;
    mc_minecart_inventory_status status = slot_array(cart, slot, 5, &frame, &array);
    if (status != MC_MINECART_INVENTORY_OK)
        return finish(error, &frame, status);
    mc_item_stack *value = mc_minecart_inventory_array_borrowed_at(array, (size_t)slot);
    if (value != NULL) {
        const mc_item_stack_status lower = mc_item_stack_retain(value);
        if (lower != MC_ITEM_STACK_OK)
            return finish(error, &frame,
                          stack_failure(&frame, MC_MINECART_INVENTORY_STACK_RETAIN, lower));
    }
    *out = value;
    return finish(error, &frame, MC_MINECART_INVENTORY_OK);
}

mc_minecart_inventory_status mc_minecart_inventory_set(mc_entity *cart, int32_t slot,
                                                       mc_item_stack *value,
                                                       mc_minecart_inventory_error *error) {
    mc_minecart_inventory_error frame = {0};
    if (!begin(cart, INVENTORY_SET, "minecart inventory set", &frame))
        return finish(error, &frame, MC_MINECART_INVENTORY_INVALID_ARGUMENT);
    frame.context.slot_selected = true;
    frame.context.slot = slot;
    mc_minecart_stack_array *array;
    mc_minecart_inventory_status status = slot_array(cart, slot, 6, &frame, &array);
    if (status != MC_MINECART_INVENTORY_OK)
        return finish(error, &frame, status);
    if (value != NULL) {
        const mc_item_stack_status lower = mc_item_stack_retain(value);
        if (lower != MC_ITEM_STACK_OK)
            return finish(error, &frame,
                          stack_failure(&frame, MC_MINECART_INVENTORY_STACK_RETAIN, lower));
    }
    mc_minecart_inventory_array_store_owned(array, (size_t)slot, value);
    if (value != NULL) {
        int32_t count;
        frame.context.current.bytecode = 12;
        mc_item_stack_status lower = mc_item_stack_source_count_get(value, &count);
        if (lower != MC_ITEM_STACK_OK)
            return finish(error, &frame,
                          stack_failure(&frame, MC_MINECART_INVENTORY_STACK_FIELDS, lower));
        if (count > 64) {
            frame.context.current.bytecode = 27;
            lower = mc_item_stack_source_count_set(value, 64);
            if (lower != MC_ITEM_STACK_OK)
                return finish(
                    error, &frame,
                    stack_failure(&frame, MC_MINECART_INVENTORY_STACK_COUNT_STORE, lower));
        }
    }
    return finish(error, &frame, MC_MINECART_INVENTORY_OK);
}

mc_minecart_inventory_status mc_minecart_inventory_split_with_allocator(
    mc_entity *cart, int32_t slot, int32_t amount, mc_item_stack_source_allocator allocate,
    mc_item_stack **out, mc_minecart_inventory_error *error) {
    mc_minecart_inventory_error frame = {0};
    frame.context.operation = "minecart inventory split";
    if (out == NULL || allocate == NULL ||
        !begin(cart, INVENTORY_SPLIT, "minecart inventory split", &frame))
        return finish(error, &frame, MC_MINECART_INVENTORY_INVALID_ARGUMENT);
    frame.context.slot_selected = true;
    frame.context.slot = slot;
    mc_minecart_stack_array *array;
    mc_minecart_inventory_status status = slot_array(cart, slot, 5, &frame, &array);
    if (status != MC_MINECART_INVENTORY_OK)
        return finish(error, &frame, status);
    if (mc_minecart_inventory_array_borrowed_at(array, (size_t)slot) == NULL) {
        *out = NULL;
        return finish(error, &frame, MC_MINECART_INVENTORY_OK);
    }
    status = slot_array(cart, slot, 14, &frame, &array);
    if (status != MC_MINECART_INVENTORY_OK)
        return finish(error, &frame, status);
    mc_item_stack *value = mc_minecart_inventory_array_borrowed_at(array, (size_t)slot);
    int32_t count;
    frame.context.current.bytecode = 15;
    mc_item_stack_status lower = mc_item_stack_source_count_get(value, &count);
    if (lower != MC_ITEM_STACK_OK)
        return finish(error, &frame,
                      stack_failure(&frame, MC_MINECART_INVENTORY_STACK_FIELDS, lower));
    if (count <= amount) {
        status = slot_array(cart, slot, 27, &frame, &array);
        if (status != MC_MINECART_INVENTORY_OK)
            return finish(error, &frame, status);
        value = mc_minecart_inventory_array_borrowed_at(array, (size_t)slot);
        status = slot_array(cart, slot, 35, &frame, &array);
        if (status != MC_MINECART_INVENTORY_OK)
            return finish(error, &frame, status);
        /* The slot's existing hold transfers to the result, without retain. */
        (void)mc_minecart_inventory_array_take(array, (size_t)slot);
        *out = value;
        return finish(error, &frame, MC_MINECART_INVENTORY_OK);
    }
    status = slot_array(cart, slot, 43, &frame, &array);
    if (status != MC_MINECART_INVENTORY_OK)
        return finish(error, &frame, status);
    value = mc_minecart_inventory_array_borrowed_at(array, (size_t)slot);
    frame.context.current.bytecode = 45;
    mc_item_stack_source_error child = {0};
    mc_item_stack *result = NULL;
    lower = mc_item_stack_source_split(value, amount, allocate, &result, &child);
    if (lower != MC_ITEM_STACK_OK) {
        status = stack_failure(&frame, MC_MINECART_INVENTORY_STACK_SPLIT, lower);
        mc_minecart_inventory_stack_context(&frame.context, cart->classes->source,
                                            MC_MINECART_INVENTORY_STACK_SPLIT, &child);
        frame.stack = child.cause;
        return finish(error, &frame, status);
    }
    status = slot_array(cart, slot, 54, &frame, &array);
    if (status == MC_MINECART_INVENTORY_OK) {
        frame.context.current.bytecode = 55;
        lower = mc_item_stack_source_count_get(
            mc_minecart_inventory_array_borrowed_at(array, (size_t)slot), &count);
        if (lower != MC_ITEM_STACK_OK)
            status = stack_failure(&frame, MC_MINECART_INVENTORY_STACK_FIELDS, lower);
        else if (count == 0) {
            status = slot_array(cart, slot, 67, &frame, &array);
            if (status == MC_MINECART_INVENTORY_OK)
                mc_minecart_inventory_array_store_owned(array, (size_t)slot, NULL);
        }
    }
    if (status != MC_MINECART_INVENTORY_OK) {
        mc_item_stack_release(result);
        return finish(error, &frame, status);
    }
    *out = result;
    return finish(error, &frame, MC_MINECART_INVENTORY_OK);
}

mc_minecart_inventory_status mc_minecart_inventory_split(mc_entity *cart, int32_t slot,
                                                         int32_t amount, mc_item_stack **out,
                                                         mc_minecart_inventory_error *error) {
    return mc_minecart_inventory_split_with_allocator(cart, slot, amount, mc_item_stack_allocate,
                                                      out, error);
}

mc_minecart_stack_array *mc_minecart_inventory_array_current(const mc_entity *cart) {
    return cart->minecart->fields.h;
}

size_t mc_minecart_inventory_array_length(const mc_minecart_stack_array *array) {
    return array->length;
}

mc_item_stack *mc_minecart_inventory_array_borrowed_at(const mc_minecart_stack_array *array,
                                                       size_t index) {
    return array->elements[index];
}

mc_entity_status mc_minecart_inventory_array_allocate(mc_entity *cart, size_t length,
                                                      const char *operation, uint16_t bytecode,
                                                      mc_minecart_stack_array **out,
                                                      mc_entity_error *allocation) {
    mc_entity_error local = {0};
    if (allocation == NULL)
        allocation = &local;
    if (!mc_entity_is_kind(cart, MC_ENTITY_CLASS_KIND_MINECART) || out == NULL)
        return mc_entity_error_set(allocation, MC_ENTITY_INVALID_ARGUMENT, operation, bytecode);
    if (length > (SIZE_MAX - sizeof(mc_minecart_stack_array)) / sizeof(mc_item_stack *))
        return mc_entity_error_set(allocation, MC_ENTITY_LIMIT, operation, bytecode);
    const size_t bytes = sizeof(mc_minecart_stack_array) + length * sizeof(mc_item_stack *);
    mc_minecart_stack_array *array =
        mc_entity_construction_allocate(cart, bytes, allocation, operation, bytecode);
    if (array == NULL)
        return allocation->status;
    array->length = length;
    array->storage = NULL;
    for (size_t i = 0; i < length; ++i)
        array->elements[i] = NULL;
    *out = array;
    return mc_entity_error_set(allocation, MC_ENTITY_OK, NULL, 0);
}

mc_minecart_stack_array *mc_minecart_inventory_array_swap(mc_entity *cart,
                                                          mc_minecart_stack_array *replacement) {
    mc_minecart_stack_array *old = cart->minecart->fields.h;
    cart->minecart->fields.h = replacement;
    return old;
}

void mc_minecart_inventory_array_store_owned(mc_minecart_stack_array *array, size_t index,
                                             mc_item_stack *owned) {
    mc_item_stack *old = array->elements[index];
    array->elements[index] = owned;
    mc_minecart_storage_occurrence_clear(array, index);
    mc_item_stack_release(old);
}

mc_item_stack *mc_minecart_inventory_array_take(mc_minecart_stack_array *array, size_t index) {
    mc_item_stack *old = array->elements[index];
    array->elements[index] = NULL;
    mc_minecart_storage_occurrence_clear(array, index);
    return old;
}

void mc_minecart_inventory_array_retire(mc_entity *cart, mc_minecart_stack_array *detached) {
    if (detached == NULL)
        return;
    const size_t length = detached->length;
    mc_minecart_storage_array_sources_retire(detached);
    for (size_t i = 0; i < length; ++i)
        mc_item_stack_release(detached->elements[i]);
    mc_entity_construction_retire(cart, detached,
                                  sizeof(*detached) + length * sizeof(*detached->elements));
}
