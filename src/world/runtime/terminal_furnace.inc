/* A sealed Player's actual BM owns one SK adapter. Each distinct adapter owns
 * exactly one Tile occurrence, even when another adapter targets the same SK. */
static mc_world_storage_status collect_furnace_inventory(terminal_collector *c,
                                                         const mc_entity *entity,
                                                         mc_world_storage_error *error) {
    if (!mc_local_player_terminal_sealed(entity))
        return MC_WORLD_STORAGE_OK;
    mc_inventory_player_terminal player;
    mc_container_state container;
    if (!mc_player_terminal_inventory_inspect(entity, &player) ||
        !mc_container_read_state(player.active_container, &container))
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World terminal actual adopted Player Furnace Container");
    if (container.kind != MC_CONTAINER_FURNACE)
        return MC_WORLD_STORAGE_OK;
    inv_furnace_node_view view;
    mc_inventory_error detail = {0};
    if (inv_furnace_node_get(container.furnace_inventory, &view, &detail) != MC_INVENTORY_OK)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, detail.operation);
    if (view.owner == NULL || view.tile == NULL || view.active || !view.terminal_sealed ||
        view.native_holds == 0)
        return result(error, MC_WORLD_STORAGE_BUSY,
                      "World terminal sealed inactive Furnace inventory owner");
    terminal_record *entry;
    bool created;
    mc_world_storage_status status =
        record(c, view.owner, TERMINAL_FURNACE, &entry, &created, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    status = increment(&entry->occurrences, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (entry->expanded)
        return entry->holds == view.native_holds
                   ? MC_WORLD_STORAGE_OK
                   : result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                            "World terminal Furnace hold metadata changed");
    entry->expanded = true;
    entry->holds = view.native_holds;
    mc_tile_entity_terminal_view tile;
    status = mc_tile_entity_terminal_read(view.tile, &tile, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (tile.world != c->world)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World terminal Furnace inventory actual World");
    return collect_tile(c, view.owner, view.tile, error);
}
