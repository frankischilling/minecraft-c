/* Actual AZ.a(NU), after OW coordinates. No set/dirty/clamp belongs to NBT. */
static mc_tile_entity_status tile_entity_dispenser_read(mc_tile_entity *tile,
                                                        const mc_nbt_tag *compound,
                                                        tile_entity_import_context *context,
                                                        mc_tile_entity_error *error) {
    const mc_nbt_tag *items = mc_nbt_compound_get(compound, tile_entity_items_name, 5);
    if (items != NULL && items->type != MC_NBT_LIST)
        return tile_entity_read_error(error, "Items", 8, false);
    mc_tile_entity_status status = tile_entity_import_admit(
        tile, context, 9u * sizeof(mc_item_stack *), error, "Items", SIZE_MAX);
    if (status != MC_TILE_ENTITY_OK) {
        if (error != NULL)
            error->read_bytecode = 17;
        return status;
    }
    mc_item_stack **slots = calloc(9u, sizeof(*slots));
    if (slots == NULL) {
        status = tile_entity_status(error, MC_TILE_ENTITY_ALLOCATION_FAILED, "Items", SIZE_MAX);
        if (error != NULL)
            error->read_bytecode = 17;
        return status;
    }
    mc_item_stack **previous = tile->state.dispenser.slots;
    const size_t previous_capacity = tile->state.dispenser.capacity;
    tile->state.dispenser.slots = slots; /* Replacement20 precedes every record. */
    tile->state.dispenser.capacity = 9;
    tile_entity_slots_release(previous, previous_capacity);
    const size_t length = items == NULL ? 0 : items->value.list.length;
    for (size_t i = 0; i < length; ++i) {
        const mc_nbt_tag *record = items->value.list.items[i];
        if (record == NULL || record->type != MC_NBT_COMPOUND) {
            status = tile_entity_status(error, MC_TILE_ENTITY_INVALID_FIELD, "Items[]", i);
            if (error != NULL)
                error->read_bytecode = 38;
            return status;
        }
        const mc_nbt_tag *tag = mc_nbt_compound_get(record, tile_entity_slot_name, 4);
        if (tag != NULL && tag->type != MC_NBT_BYTE) {
            status = tile_entity_read_error(error, "Slot", 47, false);
            if (error != NULL)
                error->item_index = i;
            return status;
        }
        const uint32_t slot =
            tag == NULL ? 0u : (uint32_t)(int32_t)tag->value.byte_value & UINT32_C(255);
        if (slot >= 9u)
            continue;
        status = tile_entity_import_admit(tile, context, mc_item_stack_object_bytes(), error,
                                          "Items[]", i);
        if (status != MC_TILE_ENTITY_OK) {
            if (error != NULL)
                error->read_bytecode = 77;
            return status;
        }
        mc_item_stack *stack = NULL;
        mc_item_stack_status item = mc_item_stack_allocate(&stack);
        mc_item_stack_error child = {0};
        if (item == MC_ITEM_STACK_OK)
            item = mc_item_stack_construct_nbt(stack, record, &child);
        if (item != MC_ITEM_STACK_OK) {
            mc_item_stack_release(stack);
            status = tile_entity_item_error(error, item, &child, i);
            if (error != NULL)
                error->read_bytecode = 83;
            return status;
        }
        mc_item_stack *old = slots[slot];
        slots[slot] = stack; /* Actual assignment86, after constructor success. */
        if (context != NULL)
            context->winners[slot] = (tile_entity_loaded_record){stack, record, i};
        mc_item_stack_release(old);
    }
    return tile_entity_status(error, MC_TILE_ENTITY_OK, NULL, SIZE_MAX);
}
