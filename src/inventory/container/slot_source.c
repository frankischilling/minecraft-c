#include "interaction_internal.h"

static void slot_at(mc_container_interaction_error *e, bool nested, const char *owner,
                    const char *method, const char *descriptor, int32_t pc) {
    mc_inventory_site *s = nested ? &e->child : &e->site;
    *s = (mc_inventory_site){owner, method, descriptor, pc};
}

static bool get(inv_slot *slot, const mc_item_stack **out, mc_container_interaction_error *e,
                bool nested) {
    slot_at(e, nested, "gp", "a", "()Liz;", 8);
    if (slot == NULL || slot->state.inventory == NULL)
        return inv_interaction_null(e, "Slot actual null Inventory getter receiver");
    const int32_t index = slot->state.inventory_index;
    mc_inventory_status status;
    if (slot->state.inventory_kind == MC_SLOT_INVENTORY_PLAYER)
        status = mc_inventory_player_source_get(slot->state.inventory, index, out, &e->inventory);
    else if (slot->state.inventory_kind == MC_SLOT_INVENTORY_CRAFTING) {
        const mc_inventory_crafting *grid = slot->state.inventory;
        if (grid->slots == NULL) {
            e->dependency = (mc_inventory_site){"mq", "a", "()I", 4};
            return inv_interaction_null(e, "Crafting source null size array");
        }
        status = mc_inventory_crafting_borrow_slot(grid, index, out, &e->inventory);
    } else if (slot->state.inventory_kind == MC_SLOT_INVENTORY_RESULT) {
        const mc_inventory_result *result = slot->state.inventory;
        if (result->slots == NULL) {
            e->dependency = (mc_inventory_site){"wl", "f_", "(I)Liz;", 5};
            return inv_interaction_null(e, "Result source null getter array");
        }
        status = mc_inventory_result_borrow(result, index, out, &e->inventory);
    } else
        return inv_interaction_fail(e, MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY,
                                    "Slot actual Inventory getter type");
    if (!inv_interaction_inventory(e, status))
        return false;
    slot_at(e, nested, "gp", "a", "()Liz;", 13);
    return true;
}

bool mc_inventory_slot_source_state(const mc_inventory_slot *slot, mc_container_slot_state *out) {
    if (slot == NULL || out == NULL)
        return false;
    *out = slot->state;
    return true;
}

mc_inventory_status mc_inventory_slot_source_get(mc_inventory_slot *slot, const mc_item_stack **out,
                                                 mc_container_interaction_error *error) {
    mc_container_interaction_error e;
    mc_container_interaction_error_init(&e);
    if (out == NULL)
        (void)inv_interaction_fail(&e, MC_INVENTORY_INVALID_ARGUMENT, "Slot native getter output");
    else
        (void)get(slot, out, &e, false);
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_inventory_status mc_inventory_slot_source_has(mc_inventory_slot *slot, bool *out,
                                                 mc_container_interaction_error *error) {
    mc_container_interaction_error e;
    mc_container_interaction_error_init(&e);
    e.site = (mc_inventory_site){"gp", "b", "()Z", 1};
    const mc_item_stack *stack = NULL;
    if (out == NULL)
        (void)inv_interaction_fail(&e, MC_INVENTORY_INVALID_ARGUMENT, "Slot native has output");
    else if (get(slot, &stack, &e, true)) {
        *out = stack != NULL;
        e.site.pc = 12;
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_inventory_status mc_inventory_slot_source_background(mc_inventory_slot *slot, int32_t *out,
                                                        mc_container_interaction_error *error) {
    mc_container_interaction_error e;
    mc_container_interaction_error_init(&e);
    e.site = (mc_inventory_site){"gp", "e", "()I", 0};
    if (out == NULL)
        (void)inv_interaction_fail(&e, MC_INVENTORY_INVALID_ARGUMENT, "Slot native icon output");
    else if (slot == NULL)
        (void)inv_interaction_null(&e, "Slot null background invocation receiver");
    else {
        *out = -1; /* GP.e literal0/return1, inherited by YV and PV. */
        e.site.pc = 1;
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

bool inv_interaction_slot_get(mc_container_interaction_call *c, inv_slot *slot,
                              mc_item_stack **out) {
    const mc_item_stack *value = NULL;
    if (!get(slot, &value, &c->error, true) || !inv_interaction_hold(c, (mc_item_stack *)value))
        return false;
    *out = (mc_item_stack *)value;
    return true;
}

bool inv_interaction_slot_changed(mc_container_interaction_call *c, inv_slot *slot) {
    mc_container_interaction_error *e = &c->error;
    e->child = (mc_inventory_site){"gp", "c", "()V", 4};
    if (slot == NULL || slot->state.inventory == NULL)
        return inv_interaction_null(e, "Slot null Inventory changed invocation receiver");
    mc_inventory_status status;
    if (slot->state.inventory_kind == MC_SLOT_INVENTORY_PLAYER)
        status = mc_inventory_player_changed((mc_inventory_player *)slot->state.inventory,
                                             &e->inventory);
    else if (slot->state.inventory_kind == MC_SLOT_INVENTORY_CRAFTING)
        status = mc_inventory_crafting_changed((mc_inventory_crafting *)slot->state.inventory,
                                               &e->inventory);
    else if (slot->state.inventory_kind == MC_SLOT_INVENTORY_RESULT)
        status = mc_inventory_result_changed((mc_inventory_result *)slot->state.inventory,
                                             &e->inventory);
    else
        return inv_interaction_fail(e, MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY,
                                    "Slot actual changed Inventory type");
    return inv_interaction_inventory(e, status);
}

bool inv_interaction_slot_put(mc_container_interaction_call *c, inv_slot *slot,
                              mc_item_stack *value) {
    mc_container_interaction_error *e = &c->error;
    e->child = (mc_inventory_site){"gp", "c", "(Liz;)V", 9};
    if (slot == NULL || slot->state.inventory == NULL)
        return inv_interaction_null(e, "Slot null Inventory setter invocation receiver");
    const int32_t index = slot->state.inventory_index;
    mc_inventory_status status;
    if (slot->state.inventory_kind == MC_SLOT_INVENTORY_PLAYER)
        status = mc_inventory_player_source_set((mc_inventory_player *)slot->state.inventory, index,
                                                value, &e->inventory);
    else if (slot->state.inventory_kind == MC_SLOT_INVENTORY_CRAFTING) {
        e->dependency = (mc_inventory_site){"mq", "a", "(ILiz;)V", 6};
        if (((const mc_inventory_crafting *)slot->state.inventory)->slots == NULL)
            return inv_interaction_null(e, "Slot setter null Crafting array store receiver");
        status = mc_inventory_crafting_set((mc_inventory_crafting *)slot->state.inventory, index,
                                           value, &e->inventory);
    } else if (slot->state.inventory_kind == MC_SLOT_INVENTORY_RESULT) {
        e->dependency = (mc_inventory_site){"wl", "a", "(ILiz;)V", 6};
        if (((const mc_inventory_result *)slot->state.inventory)->slots == NULL)
            return inv_interaction_null(e, "Slot setter null Result array store receiver");
        status = mc_inventory_result_set((mc_inventory_result *)slot->state.inventory, index, value,
                                         &e->inventory);
    } else
        return inv_interaction_fail(e, MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY,
                                    "Slot actual setter Inventory type");
    if (!inv_interaction_inventory(e, status))
        return false;
    e->child.pc = 15;
    return inv_interaction_slot_changed(c, slot);
}

bool inv_interaction_slot_limit(mc_container_interaction_call *c, inv_slot *slot, int32_t *out) {
    mc_container_interaction_error *e = &c->error;
    if (slot == NULL)
        return inv_interaction_null(e, "Slot null limit invocation receiver");
    if (slot->state.kind == MC_SLOT_ARMOR) {
        e->child = (mc_inventory_site){"pv", "d", "()I", 1};
        *out = 1;
        return true;
    }
    e->child = (mc_inventory_site){"gp", "d", "()I", 4};
    if (slot->state.inventory == NULL)
        return inv_interaction_null(e, "Slot null Inventory limit invocation receiver");
    *out = 64; /* IX, MQ and WL all literal64; no constructor/field access. */
    return true;
}

bool inv_interaction_slot_valid(mc_container_interaction_call *c, inv_slot *slot,
                                mc_item_stack *stack, bool *out) {
    mc_container_interaction_error *e = &c->error;
    if (slot == NULL)
        return inv_interaction_null(e, "Slot null validity invocation receiver");
    if (slot->state.kind != MC_SLOT_ARMOR) {
        e->child = (mc_inventory_site){slot->state.kind == MC_SLOT_CRAFTING ? "yv" : "gp", "b",
                                       "(Liz;)Z", 1};
        *out = slot->state.kind != MC_SLOT_CRAFTING;
        return true;
    }
    e->child = (mc_inventory_site){"pv", "b", "(Liz;)Z", 1};
    if (stack == NULL)
        return inv_interaction_null(e, "Armor Slot null Stack Item invocation receiver");
    const mc_item *item = NULL;
    if (!inv_interaction_item(e, c->state.container->classes, stack, &item))
        return false;
    if (item != NULL && item->kind == MC_ITEM_ARMOR) {
        e->child.pc = 11;
        if (!inv_interaction_item(e, c->state.container->classes, stack, &item))
            return false;
        e->child.pc = 17;
        if (item == NULL)
            return inv_interaction_null(e, "Armor Slot fresh null Armor type receiver");
        if (item->kind != MC_ITEM_ARMOR)
            return inv_interaction_fail(e, MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY,
                                        "Armor Slot actual fresh Item checkcast");
        *out = item->subtype.wa.bk == slot->state.armor_type;
        return true;
    }
    e->child.pc = 34;
    if (!inv_interaction_item(e, c->state.container->classes, stack, &item))
        return false;
    e->child.pc = 37;
    if (item == NULL)
        return inv_interaction_null(e, "Armor Slot null actual Item id receiver");
    const int32_t id = item->fields.bf;
    const mc_block *pumpkin = NULL;
    e->child.pc = 40;
    if (!inv_interaction_registry(e, c->state.container->classes,
                                  mc_registry_block_static_get(c->state.container->classes, "bb",
                                                               &pumpkin, &e->registry)))
        return false;
    e->child.pc = 43;
    if (pumpkin == NULL)
        return inv_interaction_null(e, "Armor Slot null captured Pumpkin id receiver");
    *out = id == pumpkin->fields.bn && slot->state.armor_type == 0;
    return true;
}

static bool split_inventory(mc_container_interaction_call *c, inv_slot *slot, int32_t amount,
                            mc_item_stack **out, bool through_slot) {
    mc_container_interaction_error *e = &c->error;
    if (through_slot)
        e->child = (mc_inventory_site){"gp", "a", "(I)Liz;", 9};
    if (slot == NULL || slot->state.inventory == NULL)
        return inv_interaction_null(e, "Slot null Inventory split invocation receiver");
    int32_t index = slot->state.inventory_index, length;
    mc_item_stack **array;
    mc_inventory_crafting *grid = NULL;
    bool result = false;
    if (slot->state.inventory_kind == MC_SLOT_INVENTORY_PLAYER) {
        mc_inventory_player *inventory = (mc_inventory_player *)slot->state.inventory;
        array = inventory->main_slots;
        e->dependency = (mc_inventory_site){"ix", "a", "(II)Liz;", 10};
        if (array == NULL)
            return inv_interaction_null(e, "Inventory split null main array length");
        length = 36;
        if (index >= length) {
            array = inventory->armor_slots;
            index = mc_java_i32_sub(index, length);
            length = 4;
        }
        e->dependency.pc = 29;
    } else if (slot->state.inventory_kind == MC_SLOT_INVENTORY_CRAFTING) {
        grid = (mc_inventory_crafting *)slot->state.inventory;
        array = grid->slots;
        length = (int32_t)grid->size;
        e->dependency = (mc_inventory_site){"mq", "a", "(II)Liz;", 5};
    } else if (slot->state.inventory_kind == MC_SLOT_INVENTORY_RESULT) {
        array = ((mc_inventory_result *)slot->state.inventory)->slots;
        length = 1;
        result = true;
        e->dependency = (mc_inventory_site){"wl", "a", "(II)Liz;", 5};
    } else
        return inv_interaction_fail(e, MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY,
                                    "Slot actual split Inventory type");
    if (array == NULL)
        return inv_interaction_null(e, "Slot split null selected Inventory array");
    if (index < 0 || index >= length) {
        e->source_exception = true;
        e->exception_class = "java/lang/ArrayIndexOutOfBoundsException";
        return inv_interaction_fail(e, MC_INVENTORY_INDEX_OUT_OF_BOUNDS,
                                    "Slot split selected signed array index");
    }
    mc_item_stack *value = array[index];
    if (value == NULL) {
        *out = NULL;
        return true;
    }
    int32_t count;
    if (!inv_interaction_stack(e, mc_item_stack_source_count_get(value, &count),
                               "Inventory split actual Stack count"))
        return false;
    mc_item_stack *removed = NULL;
    const bool whole = result || count <= amount;
    if (whole) {
        /* Transfer the array's owning occurrence into the invocation ledger. */
        if (!inv_interaction_adopt(c, value))
            return false;
        removed = value;
        array[index] = NULL;
    } else {
        if (!inv_interaction_split(c, value, amount, &removed))
            return false;
        if (!inv_interaction_stack(e, mc_item_stack_source_count_get(array[index], &count),
                                   "Inventory split fresh source count"))
            return false;
        if (count == 0) {
            mc_item_stack_release(array[index]);
            array[index] = NULL;
        }
    }
    if (grid != NULL) {
        const mc_inventory_status status = inv_container_notify(grid->handler, &e->inventory);
        e->inventory.mutation_reached = true;
        e->inventory.notification_reached = true;
        if (status != MC_INVENTORY_OK)
            inv_frame(&e->inventory, grid->side, "mq", "hz", "a",
                      inv_class(grid->side, "(II)Liz;", "(II)Lfy;"), whole ? 41 : 81);
        if (!inv_interaction_inventory(e, status))
            return false;
    } else
        e->inventory.mutation_reached = true;
    *out = removed;
    return true;
}

bool inv_interaction_slot_split(mc_container_interaction_call *c, inv_slot *slot, int32_t amount,
                                mc_item_stack **out) {
    return split_inventory(c, slot, amount, out, true);
}

bool inv_interaction_slot_pickup(mc_container_interaction_call *c, inv_slot *slot,
                                 mc_item_stack *stack) {
    mc_container_interaction_error *e = &c->error;
    if (slot == NULL)
        return inv_interaction_null(e, "Slot null pickup invocation receiver");
    if (slot->state.kind != MC_SLOT_CRAFTING) {
        e->child = (mc_inventory_site){"gp", "a", "(Liz;)V", 1};
        return inv_interaction_slot_changed(c, slot);
    }
    e->child = (mc_inventory_site){"yv", "a", "(Liz;)V", 12};
    if (c->host.crafted == NULL)
        return inv_interaction_fail(e, MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY,
                                    "Crafting Slot actual Player statistic/achievement child");
    if (c->host.crafted(c->host.context, slot->state.player, stack, e) != MC_INVENTORY_OK)
        return false;
    for (int32_t index = 0;; index = mc_java_i32_add(index, 1)) {
        e->child = (mc_inventory_site){"yv", "a", "(Liz;)V", 235};
        const mc_inventory_crafting *grid = slot->state.crafting_input;
        if (grid == NULL)
            return inv_interaction_null(e, "Crafting pickup null input size receiver");
        e->dependency = (mc_inventory_site){"mq", "a", "()I", 4};
        if (grid->slots == NULL)
            return inv_interaction_null(e, "Crafting pickup null input array length receiver");
        if (index >= (int32_t)grid->size)
            return true;
        e->child.pc = 248;
        const mc_item_stack *borrow = NULL;
        if (!inv_interaction_inventory(
                e, mc_inventory_crafting_borrow_slot(grid, index, &borrow, &e->inventory)) ||
            !inv_interaction_hold(c, (mc_item_stack *)borrow))
            return false;
        mc_item_stack *ingredient = (mc_item_stack *)borrow;
        if (ingredient == NULL)
            continue;
        inv_slot input = {.owner = c->state.container,
                          .state = {.inventory_kind = MC_SLOT_INVENTORY_CRAFTING,
                                    .inventory = slot->state.crafting_input,
                                    .inventory_index = index}};
        e->child.pc = 264;
        mc_item_stack *discard = NULL;
        if (!split_inventory(c, &input, 1, &discard, false))
            return false;
        e->child = (mc_inventory_site){"yv", "a", "(Liz;)V", 271};
        const mc_item *item = NULL;
        if (!inv_interaction_item(e, c->state.container->classes, ingredient, &item))
            return false;
        e->child.pc = 274;
        if (item == NULL)
            return inv_interaction_null(e, "Crafting pickup null Item container predicate");
        if (item->fields.bk == NULL)
            continue; /* All38 inherit GM.j, actual field1/null branch4. */
        e->child.pc = 285;
        mc_item_stack *raw = NULL;
        if (!inv_interaction_stack(e, mc_item_stack_allocate(&raw),
                                   "Crafting pickup actual new return-container Stack"))
            return false;
        if (!inv_interaction_adopt(c, raw)) {
            mc_item_stack_release(raw);
            return false;
        }
        e->child.pc = 290;
        if (!inv_interaction_item(e, c->state.container->classes, ingredient, &item))
            return false;
        e->child.pc = 293;
        if (item == NULL)
            return inv_interaction_null(e, "Crafting pickup null Item container getter");
        const mc_item *container_item = item->fields.bk;
        e->child.pc = 296;
        if (container_item == NULL)
            return inv_interaction_null(e, "Crafting pickup null return-container Item id");
        if (!inv_interaction_stack(e, mc_item_stack_construct(raw, container_item->fields.bf, 1, 0),
                                   "Crafting pickup actual return-container Stack constructor"))
            return false;
        e->child.pc = 299;
        e->dependency = (mc_inventory_site){"mq", "a", "(ILiz;)V", 6};
        if (slot->state.crafting_input == NULL)
            return inv_interaction_null(e, "Crafting pickup null return-container setter receiver");
        if (slot->state.crafting_input->slots == NULL)
            return inv_interaction_null(
                e, "Crafting pickup null return-container array store receiver");
        if (!inv_interaction_inventory(
                e, mc_inventory_crafting_set((mc_inventory_crafting *)slot->state.crafting_input,
                                             index, raw, &e->inventory)))
            return false;
    }
}
