#include "../internal.h"

static mc_inventory_error player_admit(const mc_inventory_player *inventory, const void *out) {
    mc_inventory_error error = {0};
    if (inventory == NULL) {
        error.status = MC_INVENTORY_NULL_REFERENCE;
        error.operation = "Player inventory null receiver";
    } else if (out == NULL) {
        error.status = MC_INVENTORY_INVALID_ARGUMENT;
        error.operation = "Player inventory output";
    }
    return error;
}

static void player_free(mc_inventory_player *inventory) {
    if (inventory == NULL)
        return;
    if (inventory->main_slots != NULL)
        for (size_t i = 0; i < 36u; ++i)
            mc_item_stack_release(inventory->main_slots[i]);
    if (inventory->armor_slots != NULL)
        for (size_t i = 0; i < 4u; ++i)
            mc_item_stack_release(inventory->armor_slots[i]);
    mc_item_stack_release(inventory->cursor);
    inv_free(&inventory->local_budget, inventory->main_slots);
    inv_free(&inventory->local_budget, inventory->armor_slots);
    inv_free(&inventory->local_budget, inventory);
}

mc_inventory_status mc_inventory_player_create(mc_registry_side side, mc_entity *player,
                                               const mc_inventory_limits *limits,
                                               mc_inventory_player **out,
                                               mc_inventory_error *result) {
    mc_inventory_error error = {0};
    inv_budget budget;
    if (!inv_side_valid(side) || out == NULL || !inv_limits(limits, &budget)) {
        error.status = MC_INVENTORY_INVALID_ARGUMENT;
        error.operation = "Player inventory construction admission";
        return inv_finish(result, &error);
    }
    mc_inventory_player *inventory = inv_alloc(&budget, sizeof(*inventory), &error);
    if (inventory == NULL)
        return inv_finish(result, &error);
    inventory->local_budget = budget;
    inventory->side = side;
    if (budget.max_slots < 36u) {
        error.status = MC_INVENTORY_LIMIT;
        error.operation = "Player inventory native array bound";
    } else
        inventory->main_slots =
            inv_alloc(&inventory->local_budget, 36u * sizeof(*inventory->main_slots), &error);
    if (error.status != MC_INVENTORY_OK) {
        inv_frame(&error, side, "ix", "fx", "<init>", inv_class(side, "(Lgs;)V", "(Lem;)V"), 7);
        player_free(inventory);
        return inv_finish(result, &error);
    }
    inventory->armor_slots =
        inv_alloc(&inventory->local_budget, 4u * sizeof(*inventory->armor_slots), &error);
    if (inventory->armor_slots == NULL) {
        inv_frame(&error, side, "ix", "fx", "<init>", inv_class(side, "(Lgs;)V", "(Lem;)V"), 15);
        player_free(inventory);
        return inv_finish(result, &error);
    }
    inventory->selected = 0;
    inventory->changed = false;
    inventory->player = player;
    *out = inventory;
    return inv_finish(result, &error);
}

mc_inventory_status mc_inventory_player_retire(mc_inventory_player **owner,
                                               mc_inventory_error *result) {
    mc_inventory_error error = {0};
    if (owner == NULL)
        error.status = MC_INVENTORY_INVALID_ARGUMENT;
    else if (*owner != NULL && ((*owner)->containers != 0 || (*owner)->active)) {
        error.status = MC_INVENTORY_BUSY;
        error.operation = "Player inventory retained by concrete Container";
    } else {
        player_free(*owner);
        *owner = NULL;
    }
    return inv_finish(result, &error);
}

bool mc_inventory_player_read_state(const mc_inventory_player *inventory,
                                    mc_inventory_player_state *out) {
    if (inventory == NULL || out == NULL)
        return false;
    *out = (mc_inventory_player_state){inventory->player,     inventory->selected,
                                       inventory->changed,    inventory->cursor,
                                       inventory->main_slots, inventory->armor_slots,
                                       inventory->containers, inventory->local_budget.used};
    return true;
}

static mc_item_stack **player_cell(const mc_inventory_player *inventory, int32_t slot) {
    if (slot >= 36)
        return slot < 40 ? &inventory->armor_slots[(size_t)(slot - 36)] : NULL;
    return slot >= 0 ? &inventory->main_slots[(size_t)slot] : NULL;
}

static void player_index(mc_inventory_error *error, const mc_inventory_player *inventory,
                         const char *method, const char *client, const char *server, int32_t pc) {
    error->status = MC_INVENTORY_INDEX_OUT_OF_BOUNDS;
    error->operation = "Player inventory array index";
    inv_frame(error, inventory->side, "ix", "fx", method,
              inv_class(inventory->side, client, server), pc);
}

mc_inventory_status mc_inventory_player_borrow(const mc_inventory_player *inventory, int32_t slot,
                                               const mc_item_stack **out,
                                               mc_inventory_error *result) {
    mc_inventory_error error = player_admit(inventory, out);
    if (error.status == MC_INVENTORY_OK) {
        mc_item_stack **cell = player_cell(inventory, slot);
        if (cell == NULL)
            player_index(&error, inventory, inv_class(inventory->side, "f_", "d_"), "(I)Liz;",
                         "(I)Lfy;", 23);
        else
            *out = *cell;
    }
    return inv_finish(result, &error);
}

mc_inventory_status mc_inventory_player_get(mc_inventory_player *inventory, int32_t slot,
                                            mc_item_stack **out, mc_inventory_error *result) {
    mc_inventory_error error = player_admit(inventory, out);
    const mc_item_stack *value = NULL;
    if (error.status == MC_INVENTORY_OK &&
        mc_inventory_player_borrow(inventory, slot, &value, &error) == MC_INVENTORY_OK)
        inv_publish_hold(value, out, &error);
    return inv_finish(result, &error);
}

mc_inventory_status mc_inventory_player_set(mc_inventory_player *inventory, int32_t slot,
                                            mc_item_stack *value, mc_inventory_error *result) {
    mc_inventory_error error = player_admit(inventory, inventory);
    if (error.status == MC_INVENTORY_OK) {
        mc_item_stack **cell = player_cell(inventory, slot);
        if (cell == NULL)
            player_index(&error, inventory, "a", "(ILiz;)V", "(ILfy;)V", 24);
        else if (inv_hold(value, &error) == MC_INVENTORY_OK) {
            mc_item_stack *old = *cell;
            *cell = value;
            mc_item_stack_release(old);
            error.mutation_reached = true;
        }
    }
    return inv_finish(result, &error);
}

mc_inventory_status mc_inventory_player_split(mc_inventory_player *inventory, int32_t slot,
                                              int32_t amount, mc_item_stack **out,
                                              mc_inventory_error *result) {
    mc_inventory_error error = player_admit(inventory, out);
    if (error.status != MC_INVENTORY_OK)
        return inv_finish(result, &error);
    mc_item_stack **cell = player_cell(inventory, slot);
    if (cell == NULL) {
        player_index(&error, inventory, "a", "(II)Liz;", "(II)Lfy;", 29);
        return inv_finish(result, &error);
    }
    mc_item_stack *value = *cell;
    if (value == NULL)
        *out = NULL;
    else {
        mc_item_stack_fields fields;
        inv_stack(&error, mc_item_stack_fields_get(value, &fields),
                  "Player inventory Stack fields");
        if (error.status == MC_INVENTORY_OK && fields.count <= amount) {
            *cell = NULL;
            *out = value;
            error.mutation_reached = true;
        } else if (error.status == MC_INVENTORY_OK) {
            mc_item_stack *removed = NULL;
            inv_stack(&error, mc_item_stack_split(value, amount, &removed),
                      "Player inventory Stack.split");
            if (error.status == MC_INVENTORY_OK) {
                mc_item_stack_fields_get(value, &fields);
                if (fields.count == 0) {
                    *cell = NULL;
                    mc_item_stack_release(value);
                }
                *out = removed;
                error.mutation_reached = true;
            } else {
                error.mutation_reached = error.stack_status == MC_ITEM_STACK_ALLOCATION_FAILED ||
                                         error.stack_status == MC_ITEM_STACK_LIMIT;
                if (error.mutation_reached)
                    inv_frame(&error, inventory->side, "iz", "fy", "a",
                              inv_class(inventory->side, "(I)Liz;", "(I)Lfy;"), 10);
                inv_frame(&error, inventory->side, "ix", "fx", "a",
                          inv_class(inventory->side, "(II)Liz;", "(II)Lfy;"), 59);
            }
        }
    }
    return inv_finish(result, &error);
}

mc_inventory_status mc_inventory_player_size(const mc_inventory_player *inventory, int32_t *out,
                                             mc_inventory_error *result) {
    mc_inventory_error error = player_admit(inventory, out);
    if (error.status == MC_INVENTORY_OK)
        *out = 40;
    return inv_finish(result, &error);
}

mc_inventory_status mc_inventory_player_changed(mc_inventory_player *inventory,
                                                mc_inventory_error *result) {
    mc_inventory_error error = player_admit(inventory, inventory);
    if (error.status == MC_INVENTORY_OK) {
        inventory->changed = true;
        error.mutation_reached = true;
    }
    return inv_finish(result, &error);
}

mc_inventory_status mc_inventory_player_name(const mc_inventory_player *inventory, const char **out,
                                             mc_inventory_error *result) {
    mc_inventory_error error = player_admit(inventory, out);
    if (error.status == MC_INVENTORY_OK)
        *out = "Inventory";
    return inv_finish(result, &error);
}

mc_inventory_status mc_inventory_player_limit(const mc_inventory_player *inventory, int32_t *out,
                                              mc_inventory_error *result) {
    mc_inventory_error error = player_admit(inventory, out);
    if (error.status == MC_INVENTORY_OK)
        *out = 64;
    return inv_finish(result, &error);
}
