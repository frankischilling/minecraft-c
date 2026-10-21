#include "../../block/registry/registry_internal.h"
#include "stacks_internal.h"

struct mc_registry_stack_allocation {
    struct mc_registry_stack_allocation *next;
    mc_item_stack *stack;
};

static mc_registry_status stack_status(mc_item_stack_status status) {
    switch (status) {
        case MC_ITEM_STACK_OK:
            return MC_REGISTRY_OK;
        case MC_ITEM_STACK_ALLOCATION_FAILED:
            return MC_REGISTRY_ALLOCATION_FAILED;
        case MC_ITEM_STACK_LIMIT:
            return MC_REGISTRY_LIMIT;
        case MC_ITEM_STACK_INVALID_ARGUMENT:
        case MC_ITEM_STACK_INVALID_PHASE:
        case MC_ITEM_STACK_INVALID_FIELD:
        case MC_ITEM_STACK_NBT_ERROR:
            return MC_REGISTRY_INVALID_ARGUMENT;
    }
    return MC_REGISTRY_INVALID_ARGUMENT;
}

static mc_registry_status constructor_fail(mc_registry_classes *classes, int32_t pc,
                                           mc_registry_status status, const char *message) {
    (void)mc_registry_fail(classes, status, message, NULL);
    classes->last_error.constructor_class =
        classes->options.side == MC_REGISTRY_CLIENT ? "iz" : "fy";
    classes->last_error.constructor_pc = pc;
    return status;
}

mc_registry_status mc_registry_stack_allocate(mc_registry_classes *classes, mc_item_stack **out) {
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (classes->entered == 0 || out == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT,
                                "Stack allocation requires admitted class access and output", NULL);
    const size_t maximum = classes->options.max_allocation_bytes;
    if (classes->allocation_bytes > maximum)
        return mc_registry_fail(classes, MC_REGISTRY_LIMIT, "Stack class allocation bound", NULL);

    /* This retained metadata and its native allocation record are charged by
     * the real arena allocator before any external owner can be acquired. */
    struct mc_registry_stack_allocation *node = mc_registry_allocate(classes, sizeof(*node));
    if (node == NULL)
        return classes->last_error.status;
    *node = (struct mc_registry_stack_allocation){0};
    const size_t bytes = mc_item_stack_object_bytes();
    if (classes->allocation_bytes > maximum || bytes > maximum - classes->allocation_bytes)
        return mc_registry_fail(classes, MC_REGISTRY_LIMIT, "Stack object allocation bound", NULL);

    mc_item_stack *stack = NULL;
    const mc_item_stack_status status = mc_item_stack_allocate(&stack);
    if (status != MC_ITEM_STACK_OK)
        return mc_registry_fail(classes, stack_status(status), mc_item_stack_status_string(status),
                                NULL);

    /* No fallible operation follows acquisition. The checked sum charges only
     * the successfully acquired external owner, once, and its hold is linked
     * before publishing the borrowed result. Failed metadata remains retained. */
    classes->allocation_bytes += bytes;
    node->stack = stack;
    node->next = classes->item_stacks;
    classes->item_stacks = node;
    *out = stack;
    return MC_REGISTRY_OK;
}

static mc_registry_status require_raw(mc_registry_classes *classes, mc_item_stack *raw) {
    if (classes->entered == 0 || raw == NULL)
        return constructor_fail(classes, 0, MC_REGISTRY_INVALID_ARGUMENT,
                                "Stack construction requires admitted class-owned RAW storage");
    struct mc_registry_stack_allocation *node = classes->item_stacks;
    while (node != NULL && node->stack != raw)
        node = node->next;
    if (node == NULL)
        return constructor_fail(classes, 0, MC_REGISTRY_INVALID_ARGUMENT,
                                "Stack construction received a foreign native owner");
    mc_item_stack_phase phase;
    const mc_item_stack_status status = mc_item_stack_phase_get(raw, &phase);
    if (status != MC_ITEM_STACK_OK)
        return constructor_fail(classes, 0, stack_status(status),
                                mc_item_stack_status_string(status));
    if (phase != MC_ITEM_STACK_RAW)
        return constructor_fail(classes, 0, MC_REGISTRY_INVALID_ARGUMENT,
                                "Stack construction requires the existing RAW phase");
    return MC_REGISTRY_OK;
}

static mc_registry_status construct_numeric(mc_registry_classes *classes, mc_item_stack *raw,
                                            int32_t id, int32_t count, int32_t damage,
                                            int32_t invocation_pc) {
    const mc_item_stack_status status = mc_item_stack_construct(raw, id, count, damage);
    if (status != MC_ITEM_STACK_OK)
        return constructor_fail(classes, invocation_pc, stack_status(status),
                                mc_item_stack_status_string(status));
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_stack_construct_numeric(mc_registry_classes *classes,
                                                       mc_item_stack *raw, int32_t id,
                                                       int32_t count, int32_t damage) {
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    const mc_registry_status status = require_raw(classes, raw);
    if (status != MC_REGISTRY_OK)
        return status;
    return construct_numeric(classes, raw, id, count, damage, 1);
}

mc_registry_status mc_registry_stack_construct_item_values(mc_registry_classes *classes,
                                                           mc_item_stack *raw, const mc_item *item,
                                                           int32_t count, int32_t damage) {
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    const mc_registry_status status = require_raw(classes, raw);
    if (status != MC_REGISTRY_OK)
        return status;
    if (item == NULL)
        return constructor_fail(classes, 2, MC_REGISTRY_NULL_REFERENCE,
                                "ItemStack Item constructor bf read on null");
    const int32_t id = item->fields.bf;
    return construct_numeric(classes, raw, id, count, damage, 7);
}

mc_registry_status mc_registry_stack_construct_block_values(mc_registry_classes *classes,
                                                            mc_item_stack *raw,
                                                            const mc_block *block, int32_t count,
                                                            int32_t damage) {
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    const mc_registry_status status = require_raw(classes, raw);
    if (status != MC_REGISTRY_OK)
        return status;
    /* Each reached Block overload forwards its supplied count/damage to the
     * actual bn read, then invokes the numeric constructor at7. */
    if (block == NULL)
        return constructor_fail(classes, 2, MC_REGISTRY_NULL_REFERENCE,
                                "ItemStack Block constructor bn read on null");
    const int32_t id = block->fields.bn;
    return construct_numeric(classes, raw, id, count, damage, 7);
}

mc_registry_status mc_registry_stack_construct_item(mc_registry_classes *classes,
                                                    mc_item_stack *raw, const mc_item *item) {
    return mc_registry_stack_construct_item_values(classes, raw, item, 1, 0);
}

mc_registry_status mc_registry_stack_construct_block(mc_registry_classes *classes,
                                                     mc_item_stack *raw, const mc_block *block) {
    return mc_registry_stack_construct_block_values(classes, raw, block, 1, 0);
}

mc_registry_status mc_registry_stack_copy(mc_registry_classes *classes, const mc_item_stack *source,
                                          mc_item_stack **out) {
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (source == NULL || out == NULL || classes->entered == 0)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT,
                                "Stack copy requires admitted owner, source and output", NULL);
    /* Actual k/j allocates at0 before c5/a9/d13, then invokes (III)V16.
     * No Java phase test or NBT copy appears in that method. */
    mc_item_stack *copy = NULL;
    mc_registry_status status = mc_registry_stack_allocate(classes, &copy);
    if (status != MC_REGISTRY_OK) {
        if (classes->last_error.constructor_class == NULL) {
            classes->last_error.constructor_class =
                classes->options.side == MC_REGISTRY_CLIENT ? "iz" : "fy";
            classes->last_error.constructor_pc = 0;
        }
        return status;
    }
    mc_item_stack_fields fields;
    const mc_item_stack_status read_status = mc_item_stack_fields_get(source, &fields);
    if (read_status != MC_ITEM_STACK_OK)
        return constructor_fail(classes, 5, stack_status(read_status),
                                mc_item_stack_status_string(read_status));
    status = construct_numeric(classes, copy, fields.id, fields.count, fields.damage, 16);
    if (status != MC_REGISTRY_OK)
        return status;
    *out = copy;
    return MC_REGISTRY_OK;
}

void mc_registry_stacks_retire(mc_registry_classes *classes) {
    if (classes == NULL)
        return;
    struct mc_registry_stack_allocation *node = classes->item_stacks;
    classes->item_stacks = NULL;
    while (node != NULL) {
        struct mc_registry_stack_allocation *next = node->next;
        mc_item_stack *stack = node->stack;
        node->stack = NULL;
        mc_item_stack_release(stack);
        node = next;
    }
}

size_t mc_registry_stack_count(const mc_registry_classes *classes) {
    size_t count = 0;
    if (classes != NULL)
        for (const struct mc_registry_stack_allocation *node = classes->item_stacks; node != NULL;
             node = node->next)
            ++count;
    return count;
}

mc_registry_status mc_registry_stack_get(const mc_registry_classes *classes, size_t index,
                                         const mc_item_stack **out) {
    if (classes == NULL || out == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    const struct mc_registry_stack_allocation *node = classes->item_stacks;
    for (size_t i = 0; node != NULL && i < index; ++i)
        node = node->next;
    if (node == NULL)
        return MC_REGISTRY_INDEX_OUT_OF_RANGE;
    *out = node->stack;
    return MC_REGISTRY_OK;
}
