/* Actual client SK inventory/scales/fuel/recipe bodies. Included by the one
 * canonical TileEntity owner so native cells and class effects stay shared. */
static mc_tile_entity_status furnace_slot(const mc_tile_entity *tile, int32_t slot) {
    const mc_tile_entity_status s = tile_entity_kind_ready(tile, MC_TILE_ENTITY_FURNACE);
    if (s != MC_TILE_ENTITY_OK)
        return s;
    if (tile->state.furnace.capacity != 3 || tile->state.furnace.slots == NULL)
        return MC_TILE_ENTITY_STATE_ERROR;
    return slot < 0 || slot >= 3 ? MC_TILE_ENTITY_INVALID_SLOT : MC_TILE_ENTITY_OK;
}

mc_tile_entity_status mc_tile_entity_furnace_size(const mc_tile_entity *tile, int32_t *out) {
    if (out == NULL)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    const mc_tile_entity_status s = furnace_slot(tile, 0);
    if (s == MC_TILE_ENTITY_OK)
        *out = 3;
    return s;
}

mc_tile_entity_status mc_tile_entity_furnace_limit(const mc_tile_entity *tile, int32_t *out) {
    if (out == NULL)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    const mc_tile_entity_status s = tile_entity_kind_ready(tile, MC_TILE_ENTITY_FURNACE);
    if (s == MC_TILE_ENTITY_OK)
        *out = 64;
    return s;
}

mc_tile_entity_status mc_tile_entity_furnace_get(mc_tile_entity *tile, int32_t slot,
                                                 mc_item_stack **out) {
    if (out == NULL)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    const mc_tile_entity_status s = furnace_slot(tile, slot);
    if (s != MC_TILE_ENTITY_OK)
        return s;
    mc_item_stack *stack = tile->state.furnace.slots[(size_t)slot];
    const mc_item_stack_status held =
        stack == NULL ? MC_ITEM_STACK_OK : mc_item_stack_retain(stack);
    if (held != MC_ITEM_STACK_OK)
        return tile_entity_item_status(held);
    *out = stack;
    return MC_TILE_ENTITY_OK;
}

mc_tile_entity_status mc_tile_entity_furnace_name(const mc_tile_entity *tile, const char **out) {
    if (out == NULL)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    const mc_tile_entity_status s = tile_entity_kind_ready(tile, MC_TILE_ENTITY_FURNACE);
    if (s == MC_TILE_ENTITY_OK)
        *out = "Furnace";
    return s;
}

mc_tile_entity_status mc_tile_entity_furnace_set(mc_tile_entity *tile, int32_t slot,
                                                 mc_item_stack *stack) {
    const mc_tile_entity_status s = furnace_slot(tile, slot);
    if (s != MC_TILE_ENTITY_OK)
        return s;
    mc_item_stack_status item = stack == NULL ? MC_ITEM_STACK_OK : mc_item_stack_retain(stack);
    if (item != MC_ITEM_STACK_OK)
        return tile_entity_item_status(item);
    mc_item_stack *old = tile->state.furnace.slots[(size_t)slot];
    tile->state.furnace.slots[(size_t)slot] = stack; /* SK6 before count read12. */
    mc_item_stack_release(old);
    if (stack != NULL) {
        int32_t count;
        item = mc_item_stack_source_count_get(stack, &count);
        if (item == MC_ITEM_STACK_OK && count > 64)
            item = mc_item_stack_source_count_set(stack, 64);
    }
    return item == MC_ITEM_STACK_OK ? MC_TILE_ENTITY_OK : tile_entity_item_status(item);
}

mc_tile_entity_status mc_tile_entity_furnace_remove(mc_tile_entity *tile, int32_t slot,
                                                    int32_t amount, mc_item_stack **out) {
    if (out == NULL)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    const mc_tile_entity_status s = furnace_slot(tile, slot);
    if (s != MC_TILE_ENTITY_OK)
        return s;
    mc_item_stack *stack = tile->state.furnace.slots[(size_t)slot];
    if (stack == NULL) {
        *out = NULL;
        return MC_TILE_ENTITY_OK;
    }
    int32_t count;
    mc_item_stack_status item = mc_item_stack_source_count_get(stack, &count);
    if (item != MC_ITEM_STACK_OK)
        return tile_entity_item_status(item);
    mc_item_stack *removed = NULL;
    if (count <= amount) {
        removed = stack;
        tile->state.furnace.slots[(size_t)slot] = NULL; /* Transfer slot right at35. */
    } else {
        mc_item_stack_source_error child;
        item = mc_item_stack_source_split(stack, amount, mc_item_stack_allocate, &removed, &child);
        if (item != MC_ITEM_STACK_OK)
            return tile_entity_item_status(item);
        item = mc_item_stack_source_count_get(tile->state.furnace.slots[(size_t)slot], &count);
        if (item != MC_ITEM_STACK_OK) {
            mc_item_stack_release(removed);
            return tile_entity_item_status(item);
        }
        if (count == 0) {
            mc_item_stack_release(tile->state.furnace.slots[(size_t)slot]);
            tile->state.furnace.slots[(size_t)slot] = NULL;
        }
    }
    *out = removed;
    return MC_TILE_ENTITY_OK;
}

mc_tile_entity_status mc_tile_entity_furnace_fields_get(const mc_tile_entity *tile,
                                                        mc_furnace_fields *out) {
    if (out == NULL)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    const mc_tile_entity_status s = tile_entity_kind_ready(tile, MC_TILE_ENTITY_FURNACE);
    if (s == MC_TILE_ENTITY_OK)
        *out = (mc_furnace_fields){tile->state.furnace.burn_remaining,
                                   tile->state.furnace.fuel_duration,
                                   tile->state.furnace.cook_progress};
    return s;
}

mc_tile_entity_status mc_tile_entity_furnace_field_set(mc_tile_entity *tile, mc_furnace_field field,
                                                       int32_t value) {
    const mc_tile_entity_status s = tile_entity_kind_ready(tile, MC_TILE_ENTITY_FURNACE);
    if (s != MC_TILE_ENTITY_OK)
        return s;
    switch (field) {
        case MC_FURNACE_BURN_REMAINING:
            tile->state.furnace.burn_remaining = value;
            break;
        case MC_FURNACE_FUEL_DURATION:
            tile->state.furnace.fuel_duration = value;
            break;
        case MC_FURNACE_COOK_PROGRESS:
            tile->state.furnace.cook_progress = value;
            break;
        default:
            return MC_TILE_ENTITY_INVALID_FIELD;
    }
    return MC_TILE_ENTITY_OK;
}

mc_tile_entity_status mc_tile_entity_furnace_burning(const mc_tile_entity *tile, bool *out) {
    if (out == NULL)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    const mc_tile_entity_status s = tile_entity_kind_ready(tile, MC_TILE_ENTITY_FURNACE);
    if (s == MC_TILE_ENTITY_OK)
        *out = tile->state.furnace.burn_remaining > 0;
    return s;
}

mc_tile_entity_status mc_tile_entity_furnace_cook_scale(const mc_tile_entity *tile, int32_t scale,
                                                        int32_t *out) {
    if (out == NULL)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    const mc_tile_entity_status s = tile_entity_kind_ready(tile, MC_TILE_ENTITY_FURNACE);
    if (s == MC_TILE_ENTITY_OK)
        (void)mc_java_i32_div(mc_java_i32_mul(tile->state.furnace.cook_progress, scale), 200, out);
    return s;
}

mc_tile_entity_status mc_tile_entity_furnace_burn_scale(mc_tile_entity *tile, int32_t scale,
                                                        int32_t *out) {
    if (out == NULL)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    const mc_tile_entity_status s = tile_entity_kind_ready(tile, MC_TILE_ENTITY_FURNACE);
    if (s != MC_TILE_ENTITY_OK)
        return s;
    if (tile->state.furnace.fuel_duration == 0)
        tile->state.furnace.fuel_duration = 200;
    (void)mc_java_i32_div(mc_java_i32_mul(tile->state.furnace.burn_remaining, scale),
                          tile->state.furnace.fuel_duration, out);
    return MC_TILE_ENTITY_OK;
}

static mc_tile_entity_status furnace_registry_error(mc_tile_entity_error *e, mc_registry_status s,
                                                    unsigned int pc) {
    const mc_tile_entity_status result = s == MC_REGISTRY_LIMIT ? MC_TILE_ENTITY_LIMIT
                                         : s == MC_REGISTRY_ALLOCATION_FAILED
                                             ? MC_TILE_ENTITY_ALLOCATION_FAILED
                                             : MC_TILE_ENTITY_STATE_ERROR;
    if (e != NULL) {
        e->status = result;
        e->getter_bytecode = pc;
        e->operation = e->registry.message;
    }
    return result;
}

static mc_tile_entity_status furnace_item(mc_registry_classes *classes, const mc_item_stack *stack,
                                          const mc_item **out, unsigned int pc,
                                          mc_tile_entity_error *e) {
    if (stack == NULL) {
        e->registry.status = MC_REGISTRY_NULL_REFERENCE;
        e->registry.message = "Furnace null Stack Item invocation receiver";
        return furnace_registry_error(e, e->registry.status, pc);
    }
    const mc_registry_item_array *array = NULL;
    mc_registry_status s = mc_registry_item_c_get(classes, &array, &e->registry);
    if (s == MC_REGISTRY_OK) {
        int32_t id;
        const mc_item_stack_status item = mc_item_stack_source_id_get(stack, &id);
        if (item != MC_ITEM_STACK_OK) {
            tile_entity_item_error(e, item, NULL, SIZE_MAX);
            e->getter_bytecode = pc;
            return e->status;
        }
        s = mc_registry_item_c_at(classes, array, id, out, &e->registry);
    }
    return s == MC_REGISTRY_OK ? MC_TILE_ENTITY_OK : furnace_registry_error(e, s, pc);
}

mc_tile_entity_status mc_tile_entity_furnace_fuel(mc_registry_classes *classes,
                                                  const mc_item_stack *stack, int32_t *out,
                                                  mc_tile_entity_error *result) {
    mc_tile_entity_error e;
    mc_tile_entity_error_init(&e);
    if (out == NULL)
        return tile_entity_status(result, MC_TILE_ENTITY_INVALID_ARGUMENT, NULL, SIZE_MAX);
    int32_t value = 0;
    if (stack != NULL) {
        const mc_item *item = NULL;
        if (furnace_item(classes, stack, &item, 6, &e) != MC_TILE_ENTITY_OK)
            goto done;
        int32_t id;
        mc_registry_status s = mc_registry_item_source_id(classes, item, &id, &e.registry);
        if (s != MC_REGISTRY_OK) {
            furnace_registry_error(&e, s, 10);
            goto done;
        }
        if (id < 256) {
            mc_registry_block_m_view array;
            s = mc_registry_block_m_get(classes, &array, &e.registry);
            if (s != MC_REGISTRY_OK) {
                furnace_registry_error(&e, s, 21);
                goto done;
            }
            const mc_block *block = NULL;
            s = mc_registry_block_m_at(&array, id, &block, &e.registry);
            if (s != MC_REGISTRY_OK) {
                furnace_registry_error(&e, s, 25);
                goto done;
            }
            if (block == NULL) {
                e.registry.status = MC_REGISTRY_NULL_REFERENCE;
                e.registry.message = "Furnace fuel null Block material receiver";
                furnace_registry_error(&e, e.registry.status, 26);
                goto done;
            }
            if (block->fields.bA == mc_materials_get(classes->options.materials, MC_MATERIAL_d)) {
                value = 300;
                goto success;
            }
        }
        const char *names[] = {"B", "k", "aw"};
        const int32_t values[] = {100, 1600, 20000};
        const unsigned int pcs[] = {40, 53, 67};
        for (size_t i = 0; i < 3; ++i) {
            const mc_item *selected = NULL;
            s = mc_registry_item_static_get(classes, names[i], &selected, &e.registry);
            if (s != MC_REGISTRY_OK) {
                furnace_registry_error(&e, s, pcs[i]);
                goto done;
            }
            int32_t selected_id;
            s = mc_registry_item_source_id(classes, selected, &selected_id, &e.registry);
            if (s != MC_REGISTRY_OK) {
                furnace_registry_error(&e, s, pcs[i] + 3);
                goto done;
            }
            if (id == selected_id) {
                value = values[i];
                goto success;
            }
        }
        const mc_block *sapling = NULL;
        s = mc_registry_block_static_get(classes, "z", &sapling, &e.registry);
        if (s != MC_REGISTRY_OK) {
            furnace_registry_error(&e, s, 81);
            goto done;
        }
        if (sapling == NULL) {
            e.registry.status = MC_REGISTRY_NULL_REFERENCE;
            e.registry.message = "Furnace fuel null sapling ID receiver";
            furnace_registry_error(&e, e.registry.status, 84);
            goto done;
        }
        if (id == sapling->fields.bn)
            value = 100;
    }
success:
    *out = value;
done:
    if (result != NULL)
        *result = e;
    return e.status;
}

static mc_tile_entity_status furnace_recipe(mc_tile_entity *tile, mc_registry_classes *classes,
                                            const mc_item_stack **out, unsigned int active_pc,
                                            unsigned int item_pc, mc_tile_entity_error *e) {
    mc_registry_status s = mc_registry_classes_initialize_smelting(classes, &e->registry);
    if (s != MC_REGISTRY_OK) {
        return furnace_registry_error(e, s, active_pc);
    }
    mc_smelting_manager *manager = classes->recipes.smelting;
    const mc_item *item = NULL;
    if (furnace_item(classes, tile->state.furnace.slots[0], &item, item_pc, e) != MC_TILE_ENTITY_OK)
        return e->status;
    int32_t id;
    s = mc_registry_item_source_id(classes, item, &id, &e->registry);
    if (s != MC_REGISTRY_OK)
        return furnace_registry_error(e, s, item_pc + 3);
    if (manager == NULL) {
        e->registry.status = MC_REGISTRY_NULL_REFERENCE;
        e->registry.message = "Furnace null EY recipe receiver";
        return furnace_registry_error(e, e->registry.status, item_pc + 6);
    }
    s = mc_smelting_manager_find(manager, id, out);
    if (s != MC_REGISTRY_OK) {
        e->registry.status = s;
        e->registry.message = "Furnace actual EY lookup";
        return furnace_registry_error(e, s, item_pc + 6);
    }
    return MC_TILE_ENTITY_OK;
}

static mc_tile_entity_status furnace_max(mc_registry_classes *classes, const mc_item_stack *stack,
                                         int32_t *out, unsigned int pc, mc_tile_entity_error *e) {
    const mc_item *item = NULL;
    if (furnace_item(classes, stack, &item, pc, e) != MC_TILE_ENTITY_OK)
        return e->status;
    if (item == NULL) {
        e->registry.status = MC_REGISTRY_NULL_REFERENCE;
        e->registry.message = "Furnace null Item maximum receiver";
        return furnace_registry_error(e, e->registry.status, pc);
    }
    *out = item->fields.bg;
    return MC_TILE_ENTITY_OK;
}

static bool furnace_stack_read(const mc_item_stack *stack, int field, int32_t *out, unsigned int pc,
                               mc_tile_entity_error *e) {
    const mc_item_stack_status s = field == 0   ? mc_item_stack_source_id_get(stack, out)
                                   : field == 1 ? mc_item_stack_source_damage_get(stack, out)
                                                : mc_item_stack_source_count_get(stack, out);
    if (s == MC_ITEM_STACK_OK)
        return true;
    tile_entity_item_error(e, s, NULL, SIZE_MAX);
    e->getter_bytecode = pc;
    return false;
}

static bool furnace_stack_count_store(mc_item_stack *stack, int32_t value, unsigned int pc,
                                      mc_tile_entity_error *e) {
    const mc_item_stack_status s = mc_item_stack_source_count_set(stack, value);
    if (s == MC_ITEM_STACK_OK)
        return true;
    tile_entity_item_error(e, s, NULL, SIZE_MAX);
    e->getter_bytecode = pc;
    return false;
}

mc_tile_entity_status mc_tile_entity_furnace_can_smelt(mc_tile_entity *tile,
                                                       mc_registry_classes *classes, bool *out,
                                                       mc_tile_entity_error *result) {
    mc_tile_entity_error e;
    mc_tile_entity_error_init(&e);
    mc_tile_entity_status s = out == NULL ? MC_TILE_ENTITY_INVALID_ARGUMENT : furnace_slot(tile, 0);
    if (s != MC_TILE_ENTITY_OK)
        return tile_entity_status(result, s, NULL, SIZE_MAX);
    bool value = false;
    if (tile->state.furnace.slots[0] == NULL)
        goto success;
    const mc_item_stack *recipe = NULL;
    if (furnace_recipe(tile, classes, &recipe, 11, 20, &e) != MC_TILE_ENTITY_OK)
        goto done;
    if (recipe == NULL)
        goto success;
    mc_item_stack *output = tile->state.furnace.slots[2];
    if (output == NULL) {
        value = true;
        goto success;
    }
    int32_t id, recipe_id, damage, recipe_damage, count;
    if (!furnace_stack_read(output, 0, &id, 54, &e) ||
        !furnace_stack_read(recipe, 0, &recipe_id, 54, &e))
        goto done;
    if (id != recipe_id)
        goto success;
    if (!furnace_stack_read(output, 1, &damage, 54, &e) ||
        !furnace_stack_read(recipe, 1, &recipe_damage, 54, &e))
        goto done;
    if (damage != recipe_damage)
        goto success;
    if (!furnace_stack_read(output, 2, &count, 68, &e))
        goto done;
    int32_t maximum;
    if (count < 64) {
        if (!furnace_stack_read(tile->state.furnace.slots[2], 2, &count, 84, &e))
            goto done;
        if (furnace_max(classes, tile->state.furnace.slots[2], &maximum, 93, &e) !=
            MC_TILE_ENTITY_OK)
            goto done;
        if (count < maximum) {
            value = true;
            goto success;
        }
    }
    if (!furnace_stack_read(tile->state.furnace.slots[2], 2, &count, 107, &e))
        goto done;
    if (furnace_max(classes, recipe, &maximum, 111, &e) != MC_TILE_ENTITY_OK)
        goto done;
    value = count < maximum;
success:
    *out = value;
done:
    if (result != NULL)
        *result = e;
    return e.status;
}

mc_tile_entity_status mc_tile_entity_furnace_smelt(mc_tile_entity *tile,
                                                   mc_registry_classes *classes,
                                                   mc_tile_entity_error *result) {
    mc_tile_entity_error e;
    mc_tile_entity_error_init(&e);
    bool possible;
    if (mc_tile_entity_furnace_can_smelt(tile, classes, &possible, &e) != MC_TILE_ENTITY_OK)
        goto done;
    if (!possible)
        goto done;
    const mc_item_stack *recipe = NULL;
    if (furnace_recipe(tile, classes, &recipe, 8, 17, &e) != MC_TILE_ENTITY_OK)
        goto done;
    mc_item_stack_status item;
    if (tile->state.furnace.slots[2] == NULL) {
        if (recipe == NULL) {
            e.registry.status = MC_REGISTRY_NULL_REFERENCE;
            e.registry.message = "Furnace null recipe Stack copy receiver";
            furnace_registry_error(&e, e.registry.status, 42);
            goto done;
        }
        mc_item_stack *copy = NULL;
        mc_item_stack_source_site at;
        item = mc_item_stack_source_clone(recipe, mc_item_stack_allocate, &copy, &at);
        if (item != MC_ITEM_STACK_OK) {
            tile_entity_item_error(&e, item, NULL, SIZE_MAX);
            e.getter_bytecode = 42;
            goto done;
        }
        tile->state.furnace.slots[2] = copy;
    } else {
        int32_t id, recipe_id, count;
        if (!furnace_stack_read(tile->state.furnace.slots[2], 0, &id, 55, &e))
            goto done;
        if (recipe == NULL) {
            e.registry.status = MC_REGISTRY_NULL_REFERENCE;
            e.registry.message = "Furnace null recipe Stack ID receiver";
            furnace_registry_error(&e, e.registry.status, 58);
            goto done;
        }
        if (!furnace_stack_read(recipe, 0, &recipe_id, 58, &e))
            goto done;
        if (id == recipe_id) {
            if (!furnace_stack_read(tile->state.furnace.slots[2], 2, &count, 72, &e) ||
                !furnace_stack_count_store(tile->state.furnace.slots[2], mc_java_i32_add(count, 1),
                                           77, &e))
                goto done;
        }
    }
    int32_t count;
    if (!furnace_stack_read(tile->state.furnace.slots[0], 2, &count, 87, &e))
        goto done;
    count = mc_java_i32_sub(count, 1);
    if (!furnace_stack_count_store(tile->state.furnace.slots[0], count, 92, &e) ||
        !furnace_stack_read(tile->state.furnace.slots[0], 2, &count, 104, &e))
        goto done;
    if (count <= 0) {
        mc_item_stack_release(tile->state.furnace.slots[0]);
        tile->state.furnace.slots[0] = NULL;
    }
done:
    if (result != NULL)
        *result = e;
    return e.status;
}
