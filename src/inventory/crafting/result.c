#include "../internal.h"

static mc_inventory_error result_admit(const mc_inventory_result *inventory, const void *out) {
    mc_inventory_error error = {0};
    if (inventory == NULL) {
        error.status = MC_INVENTORY_NULL_REFERENCE;
        error.operation = "Result inventory null receiver";
    } else if (out == NULL) {
        error.status = MC_INVENTORY_INVALID_ARGUMENT;
        error.operation = "Result inventory output";
    }
    return error;
}

static void result_index(mc_inventory_error *error, const mc_inventory_result *inventory,
                         const char *method, const char *client, const char *server, int32_t pc) {
    error->status = MC_INVENTORY_INDEX_OUT_OF_BOUNDS;
    error->operation = "Result inventory array index";
    inv_frame(error, inventory->side, "wl", "of", method,
              inv_class(inventory->side, client, server), pc);
}

mc_inventory_status inv_result_new(mc_registry_side side, inv_budget *budget,
                                   mc_inventory_result **out, mc_inventory_error *error) {
    mc_inventory_result *inventory = inv_alloc(budget, sizeof(*inventory), error);
    if (inventory == NULL)
        return error->status;
    inventory->budget = budget;
    inventory->side = side;
    inventory->slots = inv_alloc(budget, sizeof(*inventory->slots), error);
    if (inventory->slots == NULL) {
        inv_frame(error, side, "wl", "of", "<init>", "()V", 6);
        inv_result_free(inventory);
        return error->status;
    }
    *out = inventory;
    return MC_INVENTORY_OK;
}

void inv_result_free(mc_inventory_result *inventory) {
    if (inventory == NULL)
        return;
    inv_budget *budget = inventory->budget;
    if (inventory->slots != NULL)
        mc_item_stack_release(inventory->slots[0]);
    inv_free(budget, inventory->slots);
    inv_free(budget, inventory);
}

mc_inventory_status mc_inventory_result_create(mc_registry_side side,
                                               const mc_inventory_limits *limits,
                                               mc_inventory_result **out,
                                               mc_inventory_error *result) {
    mc_inventory_error error = {0};
    inv_budget budget;
    if (!inv_side_valid(side) || out == NULL || !inv_limits(limits, &budget)) {
        error.status = MC_INVENTORY_INVALID_ARGUMENT;
        error.operation = "Result inventory construction admission";
    } else {
        mc_inventory_result *inventory = NULL;
        if (inv_result_new(side, &budget, &inventory, &error) == MC_INVENTORY_OK) {
            inventory->local_budget = budget;
            inventory->budget = &inventory->local_budget;
            *out = inventory;
        }
    }
    return inv_finish(result, &error);
}

mc_inventory_status mc_inventory_result_retire(mc_inventory_result **owner,
                                               mc_inventory_error *result) {
    mc_inventory_error error = {0};
    if (owner == NULL)
        error.status = MC_INVENTORY_INVALID_ARGUMENT;
    else if (*owner != NULL && (*owner)->container_owned) {
        error.status = MC_INVENTORY_BUSY;
        error.operation = "Result inventory retained by concrete Container";
    } else {
        inv_result_free(*owner);
        *owner = NULL;
    }
    return inv_finish(result, &error);
}

mc_inventory_status mc_inventory_result_borrow(const mc_inventory_result *inventory, int32_t slot,
                                               const mc_item_stack **out,
                                               mc_inventory_error *result) {
    mc_inventory_error error = result_admit(inventory, out);
    if (error.status == MC_INVENTORY_OK) {
        if (slot != 0)
            result_index(&error, inventory, inv_class(inventory->side, "f_", "d_"), "(I)Liz;",
                         "(I)Lfy;", 6);
        else
            *out = inventory->slots[0];
    }
    return inv_finish(result, &error);
}

mc_inventory_status mc_inventory_result_get(mc_inventory_result *inventory, int32_t slot,
                                            mc_item_stack **out, mc_inventory_error *result) {
    mc_inventory_error error = result_admit(inventory, out);
    const mc_item_stack *value = NULL;
    if (error.status == MC_INVENTORY_OK &&
        mc_inventory_result_borrow(inventory, slot, &value, &error) == MC_INVENTORY_OK)
        inv_publish_hold(value, out, &error);
    return inv_finish(result, &error);
}

mc_inventory_status mc_inventory_result_set(mc_inventory_result *inventory, int32_t slot,
                                            mc_item_stack *value, mc_inventory_error *result) {
    mc_inventory_error error = result_admit(inventory, inventory);
    if (error.status == MC_INVENTORY_OK) {
        if (slot != 0)
            result_index(&error, inventory, "a", "(ILiz;)V", "(ILfy;)V", 6);
        else if (inv_hold(value, &error) == MC_INVENTORY_OK) {
            mc_item_stack *old = inventory->slots[0];
            inventory->slots[0] = value;
            mc_item_stack_release(old);
            error.mutation_reached = true;
        }
    }
    return inv_finish(result, &error);
}

mc_inventory_status mc_inventory_result_split(mc_inventory_result *inventory, int32_t slot,
                                              int32_t amount, mc_item_stack **out,
                                              mc_inventory_error *result) {
    (void)amount;
    mc_inventory_error error = result_admit(inventory, out);
    if (error.status == MC_INVENTORY_OK) {
        if (slot != 0)
            result_index(&error, inventory, "a", "(II)Liz;", "(II)Lfy;", 5);
        else {
            *out = inventory->slots[0];
            inventory->slots[0] = NULL;
            error.mutation_reached = *out != NULL;
        }
    }
    return inv_finish(result, &error);
}

mc_inventory_status mc_inventory_result_size(const mc_inventory_result *inventory, int32_t *out,
                                             mc_inventory_error *result) {
    mc_inventory_error error = result_admit(inventory, out);
    if (error.status == MC_INVENTORY_OK)
        *out = 1;
    return inv_finish(result, &error);
}

mc_inventory_status mc_inventory_result_name(const mc_inventory_result *inventory, const char **out,
                                             mc_inventory_error *result) {
    mc_inventory_error error = result_admit(inventory, out);
    if (error.status == MC_INVENTORY_OK)
        *out = "Result";
    return inv_finish(result, &error);
}

mc_inventory_status mc_inventory_result_limit(const mc_inventory_result *inventory, int32_t *out,
                                              mc_inventory_error *result) {
    mc_inventory_error error = result_admit(inventory, out);
    if (error.status == MC_INVENTORY_OK)
        *out = 64;
    return inv_finish(result, &error);
}

mc_inventory_status mc_inventory_result_changed(mc_inventory_result *inventory,
                                                mc_inventory_error *result) {
    mc_inventory_error error = result_admit(inventory, inventory);
    return inv_finish(result, &error);
}

mc_inventory_status mc_inventory_result_usable(const mc_inventory_result *inventory,
                                               const void *player, bool *out,
                                               mc_inventory_error *result) {
    (void)player;
    mc_inventory_error error = result_admit(inventory, out);
    if (error.status == MC_INVENTORY_OK)
        *out = true;
    return inv_finish(result, &error);
}
