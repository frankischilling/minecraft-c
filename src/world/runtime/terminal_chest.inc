/* A sealed Player sidecar owns its actual CD, which owns one LW root. Each
 * unique OG owns its two nullable child occurrences; each typed JS view owns
 * exactly one actual TileEntity right. No source Inventory method runs here. */
static mc_world_storage_status chest_edge(terminal_collector *c, const mc_inventory_chest *value,
                                          mc_world_storage_error *error) {
    if (value == NULL)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World terminal completed Chest inventory receiver");
    terminal_record *entry;
    bool created;
    const mc_world_storage_status status =
        record(c, value, TERMINAL_CHEST, &entry, &created, error);
    return status == MC_WORLD_STORAGE_OK ? increment(&entry->occurrences, error) : status;
}

static mc_world_storage_status collect_chest_inventory(terminal_collector *c,
                                                       const mc_entity *entity,
                                                       mc_world_storage_error *error) {
    if (!mc_local_player_terminal_sealed(entity))
        return MC_WORLD_STORAGE_OK;
    mc_inventory_player_terminal owner;
    mc_container_state container;
    if (!mc_player_terminal_inventory_inspect(entity, &owner) ||
        !mc_container_read_state(owner.active_container, &container))
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World terminal actual adopted Player Container");
    if (container.kind != MC_CONTAINER_CHEST)
        return MC_WORLD_STORAGE_OK;
    /* The Player's separate local admission already proves this is its actual
     * owned CD and matching Registry/World. Count that root once per Player,
     * not once per World List/anchor alias of the same Player. */
    mc_world_storage_status status = chest_edge(c, container.chest_inventory, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    size_t count;
    mc_inventory_error detail = {0};
    if (inv_chest_node_count(container.chest_inventory, &count, &detail) != MC_INVENTORY_OK)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, detail.operation);
    if (count > c->limits.max_entries)
        return result(error, MC_WORLD_STORAGE_LIMIT, "World terminal Chest graph extent");
    for (size_t i = 0; i < count; ++i) {
        inv_chest_node_view node;
        if (inv_chest_node_at(container.chest_inventory, i, &node, &detail) != MC_INVENTORY_OK)
            return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, detail.operation);
        if (node.owner == NULL || node.active || !node.terminal_sealed ||
            node.phase != INV_CHEST_READY || node.native_holds == 0)
            return result(error, MC_WORLD_STORAGE_BUSY,
                          "World terminal sealed inactive Chest inventory graph");
        terminal_record *entry;
        bool created;
        status = record(c, node.owner, TERMINAL_CHEST, &entry, &created, error);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
        if (entry->expanded) {
            if (entry->holds != node.native_holds)
                return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "World terminal Chest hold metadata changed");
            continue;
        }
        entry->expanded = true;
        entry->holds = node.native_holds;
        if (node.large) {
            status = chest_edge(c, node.left, error);
            if (status == MC_WORLD_STORAGE_OK)
                status = chest_edge(c, node.right, error);
        } else {
            if (node.tile == NULL)
                return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "World terminal actual JS Chest owner");
            mc_tile_entity_terminal_view tile;
            status = mc_tile_entity_terminal_read(node.tile, &tile, error);
            if (status == MC_WORLD_STORAGE_OK && tile.world != c->world)
                return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "World terminal Chest Inventory actual World");
            /* The canonical typed leaf owner identifies its one native Tile
             * right. Shared OG edges do not add duplicate Tile rights. */
            if (status == MC_WORLD_STORAGE_OK)
                status = collect_tile(c, node.owner, node.tile, error);
        }
        if (status != MC_WORLD_STORAGE_OK)
            return status;
    }
    return MC_WORLD_STORAGE_OK;
}
