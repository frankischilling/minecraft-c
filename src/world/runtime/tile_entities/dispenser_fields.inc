/* Actual AZ fields/LW leaves. Shared JS/AZ set/split cores preserve their
 * identical Source instruction order without assigning one class's identity
 * to the other. AZ's default Random is a separate real owner, not World.Random. */
static mc_tile_entity_status
tile_entity_dispenser_construct(mc_tile_entity *raw, const tile_entity_import_context *context,
                                mc_tile_entity_error *error) {
    mc_tile_entity_status status =
        tile_entity_import_admit(raw, context, 9u * sizeof(mc_item_stack *), error, NULL, SIZE_MAX);
    if (status != MC_TILE_ENTITY_OK) {
        if (error != NULL)
            error->constructor_bytecode = 7;
        return status;
    }
    mc_item_stack **slots = calloc(9u, sizeof(*slots));
    if (slots == NULL) {
        status = tile_entity_status(error, MC_TILE_ENTITY_ALLOCATION_FAILED, NULL, SIZE_MAX);
        if (error != NULL)
            error->constructor_bytecode = 7;
        return status;
    }
    raw->state.dispenser.slots = slots; /* Actual array field store10. */
    raw->state.dispenser.capacity = 9;
    status = tile_entity_import_admit(raw, context, sizeof(mc_java_random), error, NULL, SIZE_MAX);
    if (status != MC_TILE_ENTITY_OK) {
        if (error != NULL)
            error->constructor_bytecode = 14;
        return status;
    }
    raw->state.dispenser.constructor_random = malloc(sizeof(mc_java_random));
    if (raw->state.dispenser.constructor_random == NULL) {
        status = tile_entity_status(error, MC_TILE_ENTITY_ALLOCATION_FAILED, NULL, SIZE_MAX);
        if (error != NULL)
            error->constructor_bytecode = 14;
        return status;
    }
    const mc_java_random_profile_status random = mc_java_random_construct_default(
        raw->classes->parent->profile, raw->state.dispenser.constructor_random);
    if (random != MC_JAVA_RANDOM_PROFILE_OK) {
        status = tile_entity_status(error, MC_TILE_ENTITY_RANDOM_FAILED, NULL, SIZE_MAX);
        if (error != NULL) {
            error->constructor_bytecode = 18;
            error->random_status = random;
            error->operation = "AZ actual default Random native profile";
        }
        return status;
    }
    raw->state.dispenser.random = raw->state.dispenser.constructor_random; /* Store21. */
    raw->state.dispenser.constructor_random = NULL;
    return MC_TILE_ENTITY_OK;
}

mc_tile_entity_status mc_tile_entity_dispenser_size(const mc_tile_entity *tile, int32_t *out) {
    if (out == NULL)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    const mc_tile_entity_status status = tile_entity_kind_ready(tile, MC_TILE_ENTITY_DISPENSER);
    if (status == MC_TILE_ENTITY_OK)
        *out = 9; /* AZ.a is a literal, not an array-length query. */
    return status;
}

mc_tile_entity_status mc_tile_entity_dispenser_limit(const mc_tile_entity *tile, int32_t *out) {
    if (out == NULL)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    const mc_tile_entity_status status = tile_entity_kind_ready(tile, MC_TILE_ENTITY_DISPENSER);
    if (status == MC_TILE_ENTITY_OK)
        *out = 64;
    return status;
}

mc_tile_entity_status mc_tile_entity_dispenser_name(const mc_tile_entity *tile, const char **out) {
    if (out == NULL)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    const mc_tile_entity_status status = tile_entity_kind_ready(tile, MC_TILE_ENTITY_DISPENSER);
    if (status == MC_TILE_ENTITY_OK)
        *out = "Trap";
    return status;
}

mc_tile_entity_status mc_tile_entity_dispenser_get(mc_tile_entity *tile, int32_t slot,
                                                   mc_item_stack **out) {
    if (out == NULL)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    const mc_tile_entity_status status =
        tile_entity_inventory_slot(tile, MC_TILE_ENTITY_DISPENSER, slot);
    if (status != MC_TILE_ENTITY_OK)
        return status;
    mc_item_stack *stack = tile->state.dispenser.slots[(size_t)slot];
    if (stack != NULL) {
        const mc_item_stack_status held = mc_item_stack_retain(stack);
        if (held != MC_ITEM_STACK_OK)
            return tile_entity_item_status(held);
    }
    *out = stack;
    return MC_TILE_ENTITY_OK;
}

mc_tile_entity_status mc_tile_entity_dispenser_set_detailed(mc_tile_entity *tile, int32_t slot,
                                                            mc_item_stack *stack,
                                                            mc_live_tile_entity_error *error) {
    return tile_entity_inventory_set_detailed(tile, MC_TILE_ENTITY_DISPENSER, slot, stack, error);
}

mc_tile_entity_status mc_tile_entity_dispenser_set(mc_tile_entity *tile, int32_t slot,
                                                   mc_item_stack *stack) {
    return mc_tile_entity_dispenser_set_detailed(tile, slot, stack, NULL);
}

mc_tile_entity_status mc_tile_entity_dispenser_remove_detailed(mc_tile_entity *tile, int32_t slot,
                                                               int32_t amount, mc_item_stack **out,
                                                               mc_live_tile_entity_error *error) {
    return tile_entity_inventory_remove_detailed(tile, MC_TILE_ENTITY_DISPENSER, slot, amount, out,
                                                 error);
}

mc_tile_entity_status mc_tile_entity_dispenser_remove(mc_tile_entity *tile, int32_t slot,
                                                      int32_t amount, mc_item_stack **out) {
    return mc_tile_entity_dispenser_remove_detailed(tile, slot, amount, out, NULL);
}
