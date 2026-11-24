#include "interaction_internal.h"

#include <stdlib.h>

static bool reserve(mc_container_interaction_call *c) {
    if (c->hold_count < c->hold_capacity)
        return true;
    const size_t capacity = c->hold_capacity == 0 ? 16 : c->hold_capacity * 2;
    if (capacity < c->hold_capacity || capacity > SIZE_MAX / sizeof(*c->holds))
        return inv_interaction_fail(&c->error, MC_INVENTORY_LIMIT,
                                    "Container invocation native Stack occurrence bound");
    mc_item_stack **holds = realloc(c->holds, capacity * sizeof(*holds));
    if (holds == NULL)
        return inv_interaction_fail(&c->error, MC_INVENTORY_ALLOCATION_FAILED,
                                    "Container invocation Stack occurrences");
    c->holds = holds;
    c->hold_capacity = capacity;
    return true;
}

bool inv_interaction_adopt(mc_container_interaction_call *c, mc_item_stack *value) {
    if (value == NULL)
        return true;
    if (!reserve(c))
        return false;
    c->holds[c->hold_count++] = value;
    return true;
}

bool inv_interaction_hold(mc_container_interaction_call *c, mc_item_stack *value) {
    if (value == NULL)
        return true;
    if (!reserve(c) || !inv_interaction_stack(&c->error, mc_item_stack_retain(value),
                                              "Container actual Stack native occurrence"))
        return false;
    c->holds[c->hold_count++] = value;
    return true;
}

bool inv_interaction_clone(mc_container_interaction_call *c, mc_item_stack *value,
                           mc_item_stack **out) {
    if (value == NULL)
        return inv_interaction_null(&c->error, "Container null Stack clone receiver");
    mc_item_stack_source_site source = {0};
    mc_item_stack *clone = NULL;
    if (!inv_interaction_stack(
            &c->error, mc_item_stack_source_clone(value, mc_item_stack_allocate, &clone, &source),
            "Container actual IZ.k clone")) {
        c->error.dependency = (mc_inventory_site){"iz", "k", "()Liz;", source.bytecode};
        return false;
    }
    if (!inv_interaction_adopt(c, clone)) {
        mc_item_stack_release(clone);
        return false;
    }
    *out = clone;
    return true;
}

bool inv_interaction_split(mc_container_interaction_call *c, mc_item_stack *value, int32_t amount,
                           mc_item_stack **out) {
    if (value == NULL)
        return inv_interaction_null(&c->error, "Container null Stack split receiver");
    mc_item_stack_source_error source = {0};
    mc_item_stack *split = NULL;
    if (!inv_interaction_stack(
            &c->error,
            mc_item_stack_source_split(value, amount, mc_item_stack_allocate, &split, &source),
            "Container actual IZ.a split")) {
        c->error.dependency =
            (mc_inventory_site){"iz", "a", "(I)Liz;", source.context.split.bytecode};
        return false;
    }
    if (!inv_interaction_adopt(c, split)) {
        mc_item_stack_release(split);
        return false;
    }
    *out = split;
    return true;
}

bool inv_interaction_item(mc_container_interaction_error *e, mc_registry_classes *classes,
                          mc_item_stack *stack, const mc_item **out) {
    if (stack == NULL)
        return inv_interaction_null(e, "Container null Stack Item getter invocation receiver");
    e->dependency = (mc_inventory_site){"iz", "a", "()Lgm;", 0};
    const mc_registry_item_array *array = NULL;
    if (!inv_interaction_registry(e, classes,
                                  mc_registry_item_c_get(classes, &array, &e->registry)))
        return false;
    e->dependency.pc = 4;
    int32_t id;
    if (!inv_interaction_stack(e, mc_item_stack_source_id_get(stack, &id),
                               "Container actual Stack id read"))
        return false;
    e->dependency.pc = 7;
    return inv_interaction_registry(e, classes,
                                    mc_registry_item_c_at(classes, array, id, out, &e->registry));
}

static bool item_field(mc_container_interaction_error *e, mc_registry_classes *classes,
                       mc_item_stack *stack, const char *method, const char *descriptor,
                       const mc_item **out) {
    if (!inv_interaction_item(e, classes, stack, out)) {
        e->dependency.method = method;
        e->dependency.descriptor = descriptor;
        return false;
    }
    e->dependency = (mc_inventory_site){"iz", method, descriptor, 8};
    if (*out == NULL)
        return inv_interaction_null(e, "Container null actual Item field invocation receiver");
    if ((*out)->classes != classes)
        return inv_interaction_fail(e, MC_INVENTORY_INVALID_ARGUMENT,
                                    "Container Item native Registry namespace");
    return true;
}

bool inv_interaction_max(mc_container_interaction_error *e, mc_registry_classes *classes,
                         mc_item_stack *stack, int32_t *out) {
    const mc_item *item;
    e->child = (mc_inventory_site){"iz", "c", "()I", 1};
    if (!inv_interaction_item(e, classes, stack, &item))
        return false;
    e->child.pc = 4;
    e->dependency = e->child;
    if (item == NULL)
        return inv_interaction_null(e, "Stack maximum null GM.d invocation receiver");
    if (item->classes != classes)
        return inv_interaction_fail(e, MC_INVENTORY_INVALID_ARGUMENT,
                                    "Stack maximum native Registry namespace");
    e->dependency = (mc_inventory_site){"gm", "d", "()I", 1};
    *out = item->fields.bg; /* All38 inherit GM.d field1/return4. */
    return true;
}

bool inv_interaction_subtypes(mc_container_interaction_error *e, mc_registry_classes *classes,
                              mc_item_stack *stack, bool *out) {
    const mc_item *item;
    if (!item_field(e, classes, stack, "f", "()Z", &item))
        return false;
    *out = item->fields.bj; /* All38 inherit GM.e field1/return4. */
    return true;
}

bool inv_interaction_damageable(mc_container_interaction_error *e, mc_registry_classes *classes,
                                mc_item_stack *stack, bool *out) {
    const mc_item *item;
    if (!item_field(e, classes, stack, "e", "()Z", &item))
        return false;
    *out = item->fields.a > 0; /* IZ.e tests GM.f, not GM.g. */
    return true;
}

bool inv_interaction_stackable(mc_container_interaction_error *e, mc_registry_classes *classes,
                               mc_item_stack *stack, bool *out) {
    int32_t maximum;
    if (!inv_interaction_max(e, classes, stack, &maximum))
        return false;
    *out = false;
    if (maximum <= 1)
        return true;
    bool damageable;
    if (!inv_interaction_damageable(e, classes, stack, &damageable))
        return false;
    if (!damageable) {
        *out = true;
        return true;
    }
    /* Actual IZ.g calls e again before its own damage field8. */
    if (!inv_interaction_damageable(e, classes, stack, &damageable))
        return false;
    if (!damageable) {
        *out = true;
        return true;
    }
    int32_t damage;
    if (!inv_interaction_stack(e, mc_item_stack_source_damage_get(stack, &damage),
                               "Container actual IZ.g damage field"))
        return false;
    *out = damage <= 0;
    return true;
}
