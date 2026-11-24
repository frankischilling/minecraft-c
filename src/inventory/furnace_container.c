#include "../item/stack/source_internal.h"
#include "internal.h"
#include "mc/inventory/container_interaction.h"

static mc_inventory_status failure(mc_inventory_error *e, mc_inventory_status status,
                                   const char *operation, const char *owner, int32_t pc,
                                   bool source_null) {
    e->status = status;
    e->operation = operation;
    if (source_null) {
        e->source_exception = true;
        e->exception_class = "java/lang/NullPointerException";
    }
    inv_frame(e, MC_REGISTRY_CLIENT, owner, owner, "a", "()V", pc);
    return status;
}

static bool equal(mc_item_stack *a, const mc_item_stack *b, bool *out, mc_inventory_error *e) {
    if (a == NULL || b == NULL) {
        *out = a == b;
        return true;
    }
    int32_t ac, bc, ai, bi, ad, bd;
    if (inv_stack(e, mc_item_stack_source_count_get(a, &ac), "DW snapshot left Stack count") !=
            MC_INVENTORY_OK ||
        inv_stack(e, mc_item_stack_source_count_get(b, &bc), "DW snapshot right Stack count") !=
            MC_INVENTORY_OK)
        return false;
    if (ac != bc) {
        *out = false;
        return true;
    }
    if (inv_stack(e, mc_item_stack_source_id_get(a, &ai), "DW snapshot left Stack id") !=
            MC_INVENTORY_OK ||
        inv_stack(e, mc_item_stack_source_id_get(b, &bi), "DW snapshot right Stack id") !=
            MC_INVENTORY_OK)
        return false;
    if (ai != bi) {
        *out = false;
        return true;
    }
    if (inv_stack(e, mc_item_stack_source_damage_get(a, &ad), "DW snapshot left Stack damage") !=
            MC_INVENTORY_OK ||
        inv_stack(e, mc_item_stack_source_damage_get(b, &bd), "DW snapshot right Stack damage") !=
            MC_INVENTORY_OK)
        return false;
    *out = ac == bc && ai == bi && ad == bd;
    return true;
}

mc_inventory_status mc_container_furnace_progress(mc_container *c, int32_t index, int32_t value,
                                                  mc_inventory_error *result) {
    mc_inventory_error e = {0};
    if (c == NULL || c->kind != MC_CONTAINER_FURNACE || !c->constructed)
        e.status = MC_INVENTORY_INVALID_ARGUMENT;
    else if (c->active || c->terminal_sealed || c->interaction_active)
        e.status = MC_INVENTORY_BUSY;
    else if (index >= 0 && index <= 2) {
        c->active = true;
        const int32_t pc = index == 0 ? 9 : index == 1 ? 22 : 35;
        if (c->furnace == NULL) {
            e.status = MC_INVENTORY_NULL_REFERENCE;
            e.operation = "BM progress null SK field receiver";
            e.source_exception = true;
            e.exception_class = "java/lang/NullPointerException";
            inv_frame(&e, c->side, "bm", "bm", "a", "(II)V", pc);
        } else if (inv_furnace_progress(c->furnace, index, value, &e) != MC_INVENTORY_OK)
            inv_frame(&e, c->side, "bm", "bm", "a", "(II)V", pc);
        c->active = false;
    }
    return inv_finish(result, &e);
}

static bool snapshots(mc_container *c, mc_inventory_error *e) {
    for (size_t i = 0; i < c->slots->size; ++i) {
        mc_container_interaction_error child;
        const mc_item_stack *value = NULL;
        if (mc_inventory_slot_source_get(c->slots->values[i], &value, &child) != MC_INVENTORY_OK) {
            *e = child.inventory;
            e->status = child.status;
            e->operation = child.operation;
            e->source_exception = child.source_exception;
            e->exception_class = child.exception_class;
            inv_frame(e, c->side, "dw", "dw", "a", "()V", 28);
            return false;
        }
        if (c->snapshots == NULL || i >= c->snapshots->size) {
            (void)failure(e,
                          c->snapshots == NULL ? MC_INVENTORY_NULL_REFERENCE
                                               : MC_INVENTORY_INDEX_OUT_OF_BOUNDS,
                          "DW actual snapshot List.get", "dw", 37, c->snapshots == NULL);
            return false;
        }
        bool same;
        if (!equal(c->snapshots->values[i], value, &same, e))
            return false;
        if (same)
            continue;
        mc_item_stack *copy = NULL;
        if (value != NULL) {
            mc_item_stack_source_site source = {0};
            if (inv_stack(e,
                          mc_item_stack_source_clone(value, mc_item_stack_allocate, &copy, &source),
                          "DW actual changed Stack clone") != MC_INVENTORY_OK) {
                inv_frame(e, c->side, "dw", "dw", "a", "()V", 63);
                return false;
            }
        }
        mc_item_stack *old = c->snapshots->values[i];
        c->snapshots->values[i] = copy; /* Actual List.set73 precedes listeners. */
        mc_item_stack_release(old);
        if (c->listeners == NULL) {
            (void)failure(e, MC_INVENTORY_NULL_REFERENCE, "DW null listener List.size", "dw", 88,
                          true);
            return false;
        }
        if (c->listeners->size != 0) {
            const bool null = c->listeners->values[0] == NULL;
            (void)failure(
                e, null ? MC_INVENTORY_NULL_REFERENCE : MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY,
                "DW reached EC changed-Slot callback", "dw", null ? 113 : 107, null);
            return false;
        }
    }
    return true;
}

mc_inventory_status mc_container_furnace_detect_changes(mc_container *c,
                                                        mc_inventory_error *result) {
    mc_inventory_error e = {0};
    if (c == NULL || c->kind != MC_CONTAINER_FURNACE || !c->constructed) {
        e.status = MC_INVENTORY_INVALID_ARGUMENT;
        e.operation = "BM change detection native admission";
    } else if (c->active || c->terminal_sealed || c->interaction_active) {
        e.status = MC_INVENTORY_BUSY;
        e.operation = "BM change detection already active";
    } else {
        c->active = true;
        if (c->slots == NULL) {
            (void)failure(&e, MC_INVENTORY_NULL_REFERENCE, "DW null Slot List.size", "dw", 7, true);
        } else if (!snapshots(c, &e)) {
            inv_frame(&e, c->side, "bm", "bm", "a", "()V", 1);
        } else if (c->listeners == NULL) {
            (void)failure(&e, MC_INVENTORY_NULL_REFERENCE, "BM null listener List.size", "bm", 11,
                          true);
        } else {
            int32_t cook, burn, fuel;
            for (size_t i = 0; i < c->listeners->size && e.status == MC_INVENTORY_OK; ++i) {
                if (c->listeners->values[i] != NULL) {
                    (void)failure(&e, MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY,
                                  "BM reached EC progress listener type", "bm", 29, false);
                    break;
                }
                if (c->furnace == NULL) {
                    (void)failure(&e, MC_INVENTORY_NULL_REFERENCE, "BM null SK cook field", "bm",
                                  41, true);
                    break;
                }
                if (inv_furnace_counters(c->furnace, &cook, &burn, &fuel, &e) != MC_INVENTORY_OK)
                    break;
                const int32_t pc = c->furnace_cook != cook   ? 57
                                   : c->furnace_burn != burn ? 86
                                   : c->furnace_fuel != fuel ? 115
                                                             : 0;
                if (pc != 0)
                    (void)failure(&e, MC_INVENTORY_NULL_REFERENCE,
                                  "BM null EC progress callback receiver", "bm", pc, true);
            }
            if (e.status == MC_INVENTORY_OK) {
                if (c->furnace == NULL)
                    (void)failure(&e, MC_INVENTORY_NULL_REFERENCE, "BM null SK final cook field",
                                  "bm", 131, true);
                else if (inv_furnace_counters(c->furnace, &cook, &burn, &fuel, &e) ==
                         MC_INVENTORY_OK) {
                    c->furnace_cook = cook; /* Actual cache stores134/145/156. */
                    c->furnace_burn = burn;
                    c->furnace_fuel = fuel;
                }
            }
        }
        c->active = false;
    }
    return inv_finish(result, &e);
}
