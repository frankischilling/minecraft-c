/* SK.a(NU): base coordinates were already read by the common dispatcher. */
static mc_tile_entity_status tile_entity_furnace_read(mc_tile_entity *tile,
                                                      const mc_nbt_tag *compound,
                                                      tile_entity_import_context *context,
                                                      mc_tile_entity_error *error) {
    const mc_nbt_tag *items = mc_nbt_compound_get(compound, tile_entity_items_name, 5);
    if (items != NULL && items->type != MC_NBT_LIST)
        return tile_entity_read_error(error, "Items", 8, false);
    const size_t capacity = tile->state.furnace.capacity; /* Actual a()14 arraylength. */
    if (capacity != 3 || tile->state.furnace.slots == NULL)
        return tile_entity_status(error, MC_TILE_ENTITY_STATE_ERROR, "Items", SIZE_MAX);
    mc_tile_entity_status s = tile_entity_import_admit(
        tile, context, capacity * sizeof(mc_item_stack *), error, "Items", SIZE_MAX);
    if (s != MC_TILE_ENTITY_OK)
        return s;
    mc_item_stack **slots = calloc(capacity, sizeof(*slots));
    if (slots == NULL)
        return tile_entity_status(error, MC_TILE_ENTITY_ALLOCATION_FAILED, "Items", SIZE_MAX);
    mc_item_stack **previous = tile->state.furnace.slots;
    tile->state.furnace.slots = slots; /* Replacement20 precedes record reads. */
    tile_entity_slots_release(previous, capacity);
    const size_t length = items == NULL ? 0 : items->value.list.length;
    for (size_t i = 0; i < length; ++i) {
        const mc_nbt_tag *record = items->value.list.items[i];
        if (record == NULL || record->type != MC_NBT_COMPOUND) {
            tile_entity_status(error, MC_TILE_ENTITY_INVALID_FIELD, "Items[]", i);
            if (error != NULL)
                error->read_bytecode = 38;
            return MC_TILE_ENTITY_INVALID_FIELD;
        }
        const mc_nbt_tag *tag = mc_nbt_compound_get(record, tile_entity_slot_name, 4);
        if (tag != NULL && tag->type != MC_NBT_BYTE) {
            tile_entity_read_error(error, "Slot", 47, false);
            if (error != NULL)
                error->item_index = i;
            return MC_TILE_ENTITY_INVALID_FIELD;
        }
        const int32_t slot = tag == NULL ? 0 : tag->value.byte_value;
        if (slot < 0 || (size_t)slot >= capacity)
            continue;
        s = tile_entity_import_admit(tile, context, mc_item_stack_object_bytes(), error, "Items[]",
                                     i);
        if (s != MC_TILE_ENTITY_OK)
            return s;
        mc_item_stack *stack = NULL;
        mc_item_stack_status item = mc_item_stack_allocate(&stack);
        mc_item_stack_error child = {0};
        if (item == MC_ITEM_STACK_OK)
            item = mc_item_stack_construct_nbt(stack, record, &child);
        if (item != MC_ITEM_STACK_OK) {
            mc_item_stack_release(stack);
            tile_entity_item_error(error, item, &child, i);
            if (error != NULL)
                error->read_bytecode = 79;
            return error == NULL ? tile_entity_item_status(item) : error->status;
        }
        mc_item_stack *old = slots[(size_t)slot];
        slots[(size_t)slot] = stack;
        if (context != NULL)
            context->winners[(size_t)slot] = (tile_entity_loaded_record){stack, record, i};
        mc_item_stack_release(old);
    }
    const mc_nbt_tag *burn = mc_nbt_compound_get(compound, tile_entity_burn_name, 8);
    if (burn != NULL && burn->type != MC_NBT_SHORT)
        return tile_entity_read_error(error, "BurnTime", 93, false);
    tile->state.furnace.burn_remaining = burn == NULL ? 0 : burn->value.short_value;
    const mc_nbt_tag *cook = mc_nbt_compound_get(compound, tile_entity_cook_name, 8);
    if (cook != NULL && cook->type != MC_NBT_SHORT)
        return tile_entity_read_error(error, "CookTime", 103, false);
    tile->state.furnace.cook_progress = cook == NULL ? 0 : cook->value.short_value;
    int32_t fuel;
    s = mc_tile_entity_furnace_fuel(tile->classes->parent->registry, tile->state.furnace.slots[1],
                                    &fuel, error);
    if (s != MC_TILE_ENTITY_OK) {
        if (error != NULL)
            error->read_bytecode = 117;
        return s;
    }
    tile->state.furnace.fuel_duration = fuel; /* Recomputed120, not persisted. */
    return tile_entity_status(error, MC_TILE_ENTITY_OK, NULL, SIZE_MAX);
}
