#include "internal.h"

mc_inventory_status mc_container_dispenser_allocate(mc_registry_side side,
                                                    mc_registry_classes *classes,
                                                    const mc_inventory_limits *limits,
                                                    mc_container **out, mc_inventory_error *error) {
    if (side != MC_REGISTRY_CLIENT) {
        const mc_inventory_error e = {.status = MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY,
                                      .operation = "SH client source scope"};
        return inv_finish(error, &e);
    }
    return inv_container_allocate_kind(side, classes, MC_CONTAINER_DISPENSER, limits, out, error);
}

mc_inventory_status mc_container_dispenser_construct(mc_container *c,
                                                     mc_inventory_player *inventory,
                                                     mc_inventory_dispenser *dispenser,
                                                     mc_inventory_error *result) {
    mc_inventory_error e = {0};
    if (c == NULL || c->kind != MC_CONTAINER_DISPENSER || c->constructor_entered || c->active ||
        c->terminal_sealed ||
        (inventory != NULL && (inventory->side != c->side || inventory->terminal_sealed))) {
        e.status = MC_INVENTORY_INVALID_ARGUMENT;
        e.operation = "SH one source constructor native admission";
        return inv_finish(result, &e);
    }
    if (!inv_container_source_base_construct(c, &e))
        goto failed;
    /* Independent native occurrences protect nullable LW/AZ arguments. SH
     * never reads a Player field or calls AZ while constructing ordinary GP. */
    if (inventory != NULL) {
        if (inventory->containers == SIZE_MAX) {
            e.status = MC_INVENTORY_LIMIT;
            e.operation = "SH native Player Inventory binding overflow";
            goto failed;
        }
        ++inventory->containers;
        c->inventory_bound = true;
    }
    c->inventory = inventory;
    c->constructor_pc = 6;
    if (mc_inventory_dispenser_retain(dispenser, &e) != MC_INVENTORY_OK)
        goto failed;
    c->dispenser = dispenser; /* Actual SH.a store6, including null. */
    for (int32_t row = 0; row < 3; ++row)
        for (int32_t col = 0; col < 3; ++col)
            if (!inv_container_source_slot_add(c, MC_SLOT_INVENTORY_DISPENSER, dispenser,
                                               col + row * 3, 62 + col * 18, 17 + row * 18, 26, 55,
                                               &e))
                goto failed;
    for (int32_t row = 0; row < 3; ++row)
        for (int32_t col = 0; col < 9; ++col)
            if (!inv_container_source_slot_add(c, MC_SLOT_INVENTORY_PLAYER, inventory,
                                               col + row * 9 + 9, 8 + col * 18, 84 + row * 18, 88,
                                               121, &e))
                goto failed;
    for (int32_t col = 0; col < 9; ++col)
        if (!inv_container_source_slot_add(c, MC_SLOT_INVENTORY_PLAYER, inventory, col,
                                           8 + col * 18, 142, 145, 164, &e))
            goto failed;
    c->constructor_pc = 173;
    c->constructed = true;
    c->constructing = c->active = false;
    return inv_finish(result, &e);
failed:
    c->constructor_failed = true;
    c->constructing = c->active = false;
    return inv_finish(result, &e);
}

mc_inventory_status mc_container_dispenser_create(mc_registry_side side,
                                                  mc_registry_classes *classes,
                                                  mc_inventory_player *inventory,
                                                  mc_inventory_dispenser *dispenser,
                                                  const mc_inventory_limits *limits,
                                                  mc_container **out, mc_inventory_error *result) {
    mc_inventory_error e = {0};
    if (out == NULL || *out != NULL) {
        e.status = MC_INVENTORY_INVALID_ARGUMENT;
        e.operation = "SH fresh native constructor output";
    } else {
        mc_container *c = NULL;
        if (mc_container_dispenser_allocate(side, classes, limits, &c, &e) == MC_INVENTORY_OK) {
            if (mc_container_dispenser_construct(c, inventory, dispenser, &e) == MC_INVENTORY_OK)
                *out = c;
            else
                inv_container_free(c);
        }
    }
    return inv_finish(result, &e);
}

mc_inventory_status mc_container_dispenser_usable(mc_container *c, mc_entity *player, bool *out,
                                                  mc_inventory_error *result) {
    mc_inventory_error e = {0};
    if (c == NULL || c->kind != MC_CONTAINER_DISPENSER || !c->constructed || out == NULL) {
        e.status = MC_INVENTORY_INVALID_ARGUMENT;
        e.operation = "SH source usability native admission";
    } else if (c->active || c->terminal_sealed || c->interaction_active) {
        e.status = MC_INVENTORY_BUSY;
        e.operation = "SH source usability already active";
    } else {
        c->active = true;
        /* SH invokes AZ.a_(GS) at5. A null captured AZ fails at this invocation,
         * before any AZ body; preserve the caller's output on that prefix. */
        if (c->dispenser == NULL) {
            e.status = MC_INVENTORY_NULL_REFERENCE;
            e.operation = "SH null captured AZ usability receiver";
            e.source_exception = true;
            e.exception_class = "java/lang/NullPointerException";
        } else
            (void)mc_inventory_dispenser_usable(c->dispenser, player, out, &e);
        if (e.status != MC_INVENTORY_OK)
            inv_frame(&e, c->side, "sh", "sh", "b", "(Lgs;)Z", 5);
        c->active = false;
    }
    return inv_finish(result, &e);
}
