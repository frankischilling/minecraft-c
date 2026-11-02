#include "interaction_internal.h"

static void at(mc_container_interaction_call *c, int32_t pc) {
    c->error.site = (mc_inventory_site){"dw", "a", "(IIZLgs;)Liz;", pc};
    c->state.source_pc = pc;
    ++c->state.steps;
}

static bool count(mc_container_interaction_call *c, mc_item_stack *stack, int32_t *out) {
    return stack != NULL
               ? inv_interaction_stack(&c->error, mc_item_stack_source_count_get(stack, out),
                                       "Container source Stack count")
               : inv_interaction_null(&c->error, "Container null Stack count receiver");
}

static bool id(mc_container_interaction_call *c, mc_item_stack *stack, int32_t *out) {
    return stack != NULL ? inv_interaction_stack(&c->error, mc_item_stack_source_id_get(stack, out),
                                                 "Container source Stack id")
                         : inv_interaction_null(&c->error, "Container null Stack id receiver");
}

static bool set_count(mc_container_interaction_call *c, mc_item_stack *stack, int32_t value) {
    return stack != NULL
               ? inv_interaction_stack(&c->error, mc_item_stack_source_count_set(stack, value),
                                       "Container source Stack count store")
               : inv_interaction_null(&c->error, "Container null Stack count store receiver");
}

static bool same_damage(mc_container_interaction_call *c, mc_item_stack *a, mc_item_stack *b,
                        bool *out) {
    int32_t left, right;
    if (!inv_interaction_stack(&c->error, mc_item_stack_source_damage_get(a, &left),
                               "Container left Stack damage") ||
        !inv_interaction_stack(&c->error, mc_item_stack_source_damage_get(b, &right),
                               "Container right Stack damage"))
        return false;
    *out = left == right;
    return true;
}

static bool slot(mc_container_interaction_call *c, int32_t index, inv_slot **out) {
    mc_container_interaction_error e;
    if (mc_container_source_slot(c->state.container, index, out, &e) == MC_INVENTORY_OK)
        return true;
    c->error.source_exception = e.source_exception;
    c->error.source_error = e.source_error;
    c->error.exception_class = e.exception_class;
    return inv_interaction_fail(&c->error, e.status, e.operation);
}

bool inv_interaction_cursor_get(mc_container_interaction_call *c, mc_inventory_player *inventory,
                                mc_item_stack **out) {
    c->error.child = (mc_inventory_site){"ix", "i", "()Liz;", 1};
    if (inventory == NULL)
        return inv_interaction_null(&c->error, "Container null Inventory cursor getter receiver");
    const mc_item_stack *borrow = NULL;
    if (!inv_interaction_inventory(&c->error, mc_inventory_player_source_cursor_get(
                                                  inventory, &borrow, &c->error.inventory)) ||
        !inv_interaction_hold(c, (mc_item_stack *)borrow))
        return false;
    *out = (mc_item_stack *)borrow;
    return true;
}

bool inv_interaction_cursor_put(mc_container_interaction_call *c, mc_inventory_player *inventory,
                                mc_item_stack *stack) {
    c->error.child = (mc_inventory_site){"ix", "b", "(Liz;)V", 4};
    if (inventory == NULL)
        return inv_interaction_null(&c->error, "Container null Inventory cursor setter receiver");
    mc_entity *player = NULL;
    if (!inv_interaction_inventory(&c->error, mc_inventory_player_source_cursor_store(
                                                  inventory, stack, &player, &c->error.inventory)))
        return false;
    c->error.child.pc = 10;
    if (c->host.cursor_changed == NULL)
        return inv_interaction_fail(&c->error, MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY,
                                    "Inventory cursor actual GS.b child");
    return c->host.cursor_changed(c->host.context, player, stack, &c->error) == MC_INVENTORY_OK;
}

static bool drop(mc_container_interaction_call *c, mc_item_stack *stack) {
    if (c->host.drop == NULL)
        return inv_interaction_fail(&c->error, MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY,
                                    "Container actual GS.a(Stack) drop child");
    ++c->state.drop_calls;
    return c->host.drop(c->host.context, c->state.player, stack, &c->error) == MC_INVENTORY_OK;
}

static bool merge(mc_container_interaction_call *c, mc_item_stack *incoming, int32_t first,
                  int32_t last, bool reverse) {
    mc_registry_classes *classes = c->state.container->classes;
    bool stackable;
    if (!inv_interaction_stackable(&c->error, classes, incoming, &stackable))
        return false;
    int32_t index = reverse ? mc_java_i32_sub(last, 1) : first, n;
    if (stackable) {
        while (count(c, incoming, &n) && n > 0 && (reverse ? index >= first : index < last)) {
            inv_slot *s;
            mc_item_stack *existing;
            if (!slot(c, index, &s) || !inv_interaction_slot_get(c, s, &existing))
                return false;
            if (existing != NULL) {
                int32_t a, b;
                bool subtype, damage_equal = true;
                if (!id(c, existing, &a) || !id(c, incoming, &b))
                    return false;
                if (a == b) {
                    if (!inv_interaction_subtypes(&c->error, classes, incoming, &subtype))
                        return false;
                    if (subtype && !same_damage(c, incoming, existing, &damage_equal))
                        return false;
                    if (damage_equal) {
                        int32_t have, add, maximum;
                        if (!count(c, existing, &have) || !count(c, incoming, &add))
                            return false;
                        const int32_t total = mc_java_i32_add(have, add);
                        if (!inv_interaction_max(&c->error, classes, incoming, &maximum))
                            return false;
                        if (total <= maximum) {
                            if (!set_count(c, incoming, 0) || !set_count(c, existing, total) ||
                                !inv_interaction_slot_changed(c, s))
                                return false;
                        } else {
                            if (!count(c, existing, &have) ||
                                !inv_interaction_max(&c->error, classes, incoming, &maximum))
                                return false;
                            if (have < maximum) {
                                if (!count(c, incoming, &add) ||
                                    !inv_interaction_max(&c->error, classes, incoming, &maximum) ||
                                    !count(c, existing, &have) ||
                                    !set_count(
                                        c, incoming,
                                        mc_java_i32_sub(add, mc_java_i32_sub(maximum, have))) ||
                                    !inv_interaction_max(&c->error, classes, incoming, &maximum) ||
                                    !set_count(c, existing, maximum) ||
                                    !inv_interaction_slot_changed(c, s))
                                    return false;
                            }
                        }
                    }
                }
            }
            index = mc_java_i32_add(index, reverse ? -1 : 1);
        }
        if (c->error.status != MC_INVENTORY_OK)
            return false;
    }
    if (!count(c, incoming, &n))
        return false;
    if (n <= 0)
        return true;
    index = reverse ? mc_java_i32_sub(last, 1) : first;
    while (reverse ? index >= first : index < last) {
        inv_slot *s;
        mc_item_stack *existing;
        if (!slot(c, index, &s) || !inv_interaction_slot_get(c, s, &existing))
            return false;
        if (existing == NULL) {
            mc_item_stack *copy;
            if (!inv_interaction_clone(c, incoming, &copy) ||
                !inv_interaction_slot_put(c, s, copy) || !inv_interaction_slot_changed(c, s) ||
                !set_count(c, incoming, 0))
                return false;
            break; /* No invented Slot validity/limit call in DW merge. */
        }
        index = mc_java_i32_add(index, reverse ? -1 : 1);
    }
    return true;
}

static bool transfer(mc_container_interaction_call *c, int32_t index, mc_item_stack **out) {
    ++c->state.shift_calls;
    if (c->state.container->kind != MC_CONTAINER_PLAYER)
        return inv_interaction_fail(&c->error, MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY,
                                    "Container concrete non-AA Shift body");
    inv_slot *s;
    if (!slot(c, index, &s))
        return false;
    *out = NULL;
    if (s == NULL)
        return true;
    mc_item_stack *value;
    if (!inv_interaction_slot_get(c, s, &value))
        return false;
    if (value == NULL)
        return true;
    if (!inv_interaction_slot_get(c, s, &value))
        return false;
    mc_item_stack *copy;
    if (!inv_interaction_clone(c, value, &copy))
        return false;
    if (!merge(c, value, index >= 9 && index < 36 ? 36 : 9, index >= 36 && index < 45 ? 36 : 45,
               index == 0))
        return false;
    int32_t remaining, original;
    if (!count(c, value, &remaining))
        return false;
    if (remaining == 0) {
        if (!inv_interaction_slot_put(c, s, NULL))
            return false;
    } else if (!inv_interaction_slot_changed(c, s))
        return false;
    if (!count(c, value, &remaining) || !count(c, copy, &original))
        return false;
    if (remaining == original)
        return true;
    if (!inv_interaction_slot_pickup(c, s, value))
        return false;
    *out = copy;
    return true;
}

bool inv_interaction_click_body(mc_container_interaction_call *c, int32_t index, int32_t button,
                                bool shift, unsigned int depth, mc_item_stack **out) {
    *out = NULL;
    if (depth > 1024)
        return inv_interaction_fail(&c->error, MC_INVENTORY_LIMIT,
                                    "Container native recursive click scope");
    at(c, 3);
    if (button != 0 && button != 1)
        return true;
    at(c, 14);
    mc_inventory_player *inventory = NULL;
    if (c->host.inventory == NULL)
        return inv_interaction_fail(&c->error, MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY,
                                    "Container actual GS.c field dependency");
    if (c->host.inventory(c->host.context, c->state.player, &inventory, &c->error) !=
        MC_INVENTORY_OK)
        return false;
    mc_item_stack *saved = NULL, *cursor = NULL;
    if (index == -999) {
        at(c, 28);
        if (!inv_interaction_cursor_get(c, inventory, &cursor))
            return false;
        if (cursor == NULL)
            return true;
        if (button == 0) {
            at(c, 49);
            if (!inv_interaction_cursor_get(c, inventory, &cursor))
                return false;
            at(c, 52);
            if (!drop(c, cursor))
                return false;
            at(c, 58);
            if (!inv_interaction_cursor_put(c, inventory, NULL))
                return false;
            c->state.cursor_cleared = true;
        }
        if (button == 1) {
            at(c, 70);
            if (!inv_interaction_cursor_get(c, inventory, &cursor))
                return false;
            mc_item_stack *one;
            at(c, 74);
            if (!inv_interaction_split(c, cursor, 1, &one))
                return false;
            at(c, 77);
            if (!drop(c, one))
                return false;
            at(c, 82);
            int32_t n;
            if (!inv_interaction_cursor_get(c, inventory, &cursor) || !count(c, cursor, &n))
                return false;
            if (n == 0) {
                at(c, 94);
                if (!inv_interaction_cursor_put(c, inventory, NULL))
                    return false;
                c->state.cursor_cleared = true;
            }
        }
        return true;
    }
    if (shift) {
        at(c, 106);
        mc_item_stack *moved;
        if (!transfer(c, index, &moved))
            return false;
        if (moved != NULL) {
            int32_t original;
            at(c, 118);
            if (!count(c, moved, &original))
                return false;
            at(c, 125);
            if (!inv_interaction_clone(c, moved, &saved))
                return false;
            inv_slot *s;
            at(c, 135);
            if (!slot(c, index, &s))
                return false;
            if (s != NULL) {
                mc_item_stack *present;
                at(c, 152);
                if (!inv_interaction_slot_get(c, s, &present))
                    return false;
                if (present != NULL) {
                    at(c, 160);
                    if (!inv_interaction_slot_get(c, s, &present))
                        return false;
                    int32_t n;
                    if (!count(c, present, &n))
                        return false;
                    if (n < original) {
                        at(c, 181);
                        mc_item_stack *discard;
                        if (!inv_interaction_click_body(c, index, button, shift, depth + 1,
                                                        &discard))
                            return false;
                    }
                }
            }
        }
        *out = saved;
        return true;
    }
    inv_slot *s;
    at(c, 193);
    if (!slot(c, index, &s))
        return false;
    if (s == NULL)
        return true;
    at(c, 210);
    if (!inv_interaction_slot_changed(c, s))
        return false;
    mc_item_stack *existing;
    at(c, 215);
    if (!inv_interaction_slot_get(c, s, &existing))
        return false;
    at(c, 222);
    if (!inv_interaction_cursor_get(c, inventory, &cursor))
        return false;
    if (existing != NULL) {
        at(c, 234);
        if (!inv_interaction_clone(c, existing, &saved))
            return false;
    }
    if (existing == NULL) {
        if (cursor == NULL)
            goto returned;
        bool valid;
        at(c, 253);
        if (!inv_interaction_slot_valid(c, s, cursor, &valid))
            return false;
        if (!valid)
            goto returned;
        int32_t amount = 1, maximum;
        if (button == 0 && !count(c, cursor, &amount))
            return false;
        at(c, 278);
        if (!inv_interaction_slot_limit(c, s, &maximum))
            return false;
        if (amount > maximum) {
            at(c, 286);
            if (!inv_interaction_slot_limit(c, s, &amount))
                return false;
        }
        mc_item_stack *placed;
        at(c, 297);
        if (!inv_interaction_split(c, cursor, amount, &placed))
            return false;
        at(c, 300);
        if (!inv_interaction_slot_put(c, s, placed))
            return false;
        int32_t n;
        at(c, 305);
        if (!count(c, cursor, &n))
            return false;
        if (n == 0) {
            at(c, 314);
            if (!inv_interaction_cursor_put(c, inventory, NULL))
                return false;
        }
    } else if (cursor == NULL) {
        int32_t amount;
        if (!count(c, existing, &amount))
            return false;
        if (button == 1) {
            if (!mc_java_i32_div(mc_java_i32_add(amount, 1), 2, &amount))
                return inv_interaction_fail(&c->error, MC_INVENTORY_STACK_FAILED,
                                            "Container fixed half divisor");
        }
        mc_item_stack *picked;
        at(c, 352);
        if (!inv_interaction_slot_split(c, s, amount, &picked))
            return false;
        at(c, 361);
        if (!inv_interaction_cursor_put(c, inventory, picked))
            return false;
        int32_t n;
        at(c, 366);
        if (!count(c, existing, &n))
            return false;
        if (n == 0) {
            at(c, 375);
            if (!inv_interaction_slot_put(c, s, NULL))
                return false;
        }
        at(c, 382);
        if (!inv_interaction_cursor_get(c, inventory, &cursor))
            return false;
        at(c, 385);
        if (!inv_interaction_slot_pickup(c, s, cursor))
            return false;
    } else {
        bool valid;
        at(c, 395);
        if (!inv_interaction_slot_valid(c, s, cursor, &valid))
            return false;
        int32_t a, b;
        if (valid) {
            if (!id(c, existing, &a) || !id(c, cursor, &b))
                return false;
            bool equal = a == b;
            if (equal) {
                bool subtypes;
                at(c, 416);
                if (!inv_interaction_subtypes(&c->error, c->state.container->classes, existing,
                                              &subtypes))
                    return false;
                if (subtypes && !same_damage(c, existing, cursor, &equal))
                    return false;
            }
            if (!equal) {
                int32_t n, maximum;
                if (!count(c, cursor, &n) || !inv_interaction_slot_limit(c, s, &maximum))
                    return false;
                if (n <= maximum) {
                    at(c, 456);
                    if (!inv_interaction_slot_put(c, s, cursor))
                        return false;
                    at(c, 463);
                    if (!inv_interaction_cursor_put(c, inventory, existing))
                        return false;
                }
            } else {
                int32_t amount = 1, maximum, have;
                if (button == 0 && !count(c, cursor, &amount))
                    return false;
                at(c, 488);
                if (!inv_interaction_slot_limit(c, s, &maximum) || !count(c, existing, &have))
                    return false;
                if (amount > mc_java_i32_sub(maximum, have)) {
                    at(c, 502);
                    if (!inv_interaction_slot_limit(c, s, &maximum) || !count(c, existing, &have))
                        return false;
                    amount = mc_java_i32_sub(maximum, have);
                }
                at(c, 517);
                if (!inv_interaction_max(&c->error, c->state.container->classes, cursor,
                                         &maximum) ||
                    !count(c, existing, &have))
                    return false;
                if (amount > mc_java_i32_sub(maximum, have)) {
                    at(c, 531);
                    if (!inv_interaction_max(&c->error, c->state.container->classes, cursor,
                                             &maximum) ||
                        !count(c, existing, &have))
                        return false;
                    amount = mc_java_i32_sub(maximum, have);
                }
                mc_item_stack *discard;
                at(c, 546);
                if (!inv_interaction_split(c, cursor, amount, &discard))
                    return false;
                int32_t n;
                if (!count(c, cursor, &n))
                    return false;
                if (n == 0) {
                    at(c, 561);
                    if (!inv_interaction_cursor_put(c, inventory, NULL))
                        return false;
                }
                at(c, 567);
                if (!count(c, existing, &n))
                    return false;
                at(c, 573);
                if (!set_count(c, existing, mc_java_i32_add(n, amount)))
                    return false;
            }
        } else {
            if (!id(c, existing, &a) || !id(c, cursor, &b))
                return false;
            if (a != b)
                goto returned;
            int32_t maximum;
            at(c, 594);
            if (!inv_interaction_max(&c->error, c->state.container->classes, cursor, &maximum))
                return false;
            if (maximum <= 1)
                goto returned;
            bool subtypes, equal = true;
            at(c, 603);
            if (!inv_interaction_subtypes(&c->error, c->state.container->classes, existing,
                                          &subtypes))
                return false;
            if (subtypes && !same_damage(c, existing, cursor, &equal))
                return false;
            if (!equal)
                goto returned;
            int32_t amount, n;
            if (!count(c, existing, &amount))
                return false;
            if (amount <= 0)
                goto returned;
            if (!count(c, cursor, &n) ||
                !inv_interaction_max(&c->error, c->state.container->classes, cursor, &maximum))
                return false;
            if (mc_java_i32_add(amount, n) > maximum)
                goto returned;
            at(c, 653);
            if (!count(c, cursor, &n))
                return false;
            at(c, 659);
            if (!set_count(c, cursor, mc_java_i32_add(n, amount)))
                return false;
            mc_item_stack *discard;
            at(c, 666);
            if (!inv_interaction_split(c, existing, amount, &discard))
                return false;
            if (!count(c, existing, &n))
                return false;
            if (n == 0) {
                at(c, 681);
                if (!inv_interaction_slot_put(c, s, NULL))
                    return false;
            }
            at(c, 688);
            if (!inv_interaction_cursor_get(c, inventory, &cursor))
                return false;
            at(c, 691);
            if (!inv_interaction_slot_pickup(c, s, cursor))
                return false;
        }
    }
returned:
    at(c, 696);
    *out = saved;
    return true;
}

bool inv_interaction_close_body(mc_container_interaction_call *c) {
    c->error.site = (mc_inventory_site){"dw", "a", "(Lgs;)V", 1};
    mc_inventory_player *inventory = NULL;
    if (c->host.inventory == NULL)
        return inv_interaction_fail(&c->error, MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY,
                                    "Container close actual GS.c field dependency");
    if (c->host.inventory(c->host.context, c->state.player, &inventory, &c->error) !=
        MC_INVENTORY_OK)
        return false;
    mc_item_stack *cursor;
    c->error.site.pc = 6;
    if (!inv_interaction_cursor_get(c, inventory, &cursor))
        return false;
    if (cursor != NULL) {
        c->error.site.pc = 14;
        if (!inv_interaction_cursor_get(c, inventory, &cursor))
            return false;
        c->error.site.pc = 17;
        if (!drop(c, cursor))
            return false;
        c->error.site.pc = 22;
        if (!inv_interaction_cursor_put(c, inventory, NULL))
            return false;
        c->state.cursor_cleared = true;
    }
    if (c->state.container->kind != MC_CONTAINER_PLAYER)
        return inv_interaction_fail(&c->error, MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY,
                                    "Container concrete non-AA close continuation");
    for (int32_t i = 0; i < 4; ++i) {
        c->error.site = (mc_inventory_site){"aa", "a", "(Lgs;)V", 17};
        const mc_item_stack *value = NULL;
        if (c->state.container->crafting == NULL)
            return inv_interaction_null(&c->error, "Container close null Crafting getter receiver");
        c->error.dependency = (mc_inventory_site){"mq", "a", "()I", 4};
        if (c->state.container->crafting->slots == NULL)
            return inv_interaction_null(&c->error,
                                        "Container close null Crafting array length receiver");
        if (!inv_interaction_inventory(
                &c->error, mc_inventory_crafting_borrow_slot(c->state.container->crafting, i,
                                                             &value, &c->error.inventory)) ||
            !inv_interaction_hold(c, (mc_item_stack *)value))
            return false;
        if (value != NULL) {
            c->error.site.pc = 27;
            if (!drop(c, (mc_item_stack *)value))
                return false;
            c->error.site.pc = 36;
            if (c->state.container->crafting == NULL)
                return inv_interaction_null(&c->error,
                                            "Container close fresh null Crafting setter receiver");
            c->error.dependency = (mc_inventory_site){"mq", "a", "(ILiz;)V", 6};
            if (c->state.container->crafting->slots == NULL)
                return inv_interaction_null(
                    &c->error, "Container close fresh null Crafting array store receiver");
            if (!inv_interaction_inventory(&c->error,
                                           mc_inventory_crafting_set(c->state.container->crafting,
                                                                     i, NULL, &c->error.inventory)))
                return false;
            ++c->state.crafting_cells_cleared;
        }
    }
    c->error.site.pc = 45;
    return true;
}
