/* Included by the canonical TileEntity owner. Fresh source results stay
 * separate from the native provenance-overlay storage builders above. */
mc_tile_entity_status mc_tile_entity_build_source_nbt(const mc_tile_entity *tile,
                                                      const mc_nbt_limits *requested,
                                                      mc_nbt_tree **out,
                                                      mc_tile_entity_error *error) {
    mc_nbt_limits limits;
    if (out == NULL || !tile_entity_limits(requested, &limits))
        return tile_entity_status(error, MC_TILE_ENTITY_INVALID_ARGUMENT, NULL, SIZE_MAX);
    mc_tile_entity_status status = tile_entity_ready(tile);
    if (status != MC_TILE_ENTITY_OK)
        return tile_entity_status(error, status, NULL, SIZE_MAX);
    mc_nbt_tag fields[9] = {0};
    size_t field_count = 0;
    status = tile_entity_build_fields(tile, fields, &field_count, error);
    if (status != MC_TILE_ENTITY_OK)
        return status;
    static const uint16_t slot_name[] = {'S', 'l', 'o', 't'};
    static const uint16_t id_name[] = {'i', 'd'};
    static const uint16_t count_name[] = {'C', 'o', 'u', 'n', 't'};
    static const uint16_t damage_name[] = {'D', 'a', 'm', 'a', 'g', 'e'};
    mc_nbt_tag records[36] = {0}, item_fields[36][4] = {0};
    const mc_nbt_tag *item_children[36][4] = {0}, *record_children[36] = {0};
    mc_item_stack *held[36] = {0};
    size_t occupied = 0;
    if (tile->kind == MC_TILE_ENTITY_CHEST) {
        if (tile->state.chest.capacity > 36 ||
            (tile->state.chest.capacity != 0 && tile->state.chest.slots == NULL))
            return tile_entity_status(error, MC_TILE_ENTITY_STATE_ERROR, "Items", SIZE_MAX);
        for (size_t slot = 0; slot < tile->state.chest.capacity; ++slot) {
            mc_item_stack *stack = tile->state.chest.slots[slot];
            if (stack == NULL)
                continue;
            const mc_item_stack_status retained = mc_item_stack_retain(stack);
            if (retained != MC_ITEM_STACK_OK) {
                mc_item_stack_error child = {.status = retained};
                status = tile_entity_item_error(error, retained, &child, occupied);
                goto done;
            }
            const size_t i = occupied++;
            held[i] = stack;
            mc_item_stack_fields actual;
            const mc_item_stack_status read = mc_item_stack_fields_get(stack, &actual);
            if (read != MC_ITEM_STACK_OK) {
                mc_item_stack_error child = {.status = read};
                status = tile_entity_item_error(error, read, &child, i);
                if (error != NULL)
                    error->save_bytecode = 59;
                goto done;
            }
            item_fields[i][0] = (mc_nbt_tag){.type = MC_NBT_BYTE,
                                             .name = {slot_name, 4},
                                             .value.byte_value = mc_java_i8_narrow((int32_t)slot)};
            item_fields[i][1] = (mc_nbt_tag){.type = MC_NBT_SHORT,
                                             .name = {id_name, 2},
                                             .value.short_value = mc_java_i16_narrow(actual.id)};
            item_fields[i][2] = (mc_nbt_tag){.type = MC_NBT_BYTE,
                                             .name = {count_name, 5},
                                             .value.byte_value = mc_java_i8_narrow(actual.count)};
            item_fields[i][3] =
                (mc_nbt_tag){.type = MC_NBT_SHORT,
                             .name = {damage_name, 6},
                             .value.short_value = mc_java_i16_narrow(actual.damage)};
            for (size_t j = 0; j < 4; ++j)
                item_children[i][j] = &item_fields[i][j];
            records[i] =
                (mc_nbt_tag){.type = MC_NBT_COMPOUND, .value.compound = {item_children[i], 4}};
            record_children[i] = &records[i];
        }
        fields[4].value.list.items = occupied == 0 ? NULL : record_children;
        fields[4].value.list.length = occupied;
    }
    const mc_nbt_tag *children[9] = {0};
    for (size_t i = 0; i < field_count; ++i)
        children[i] = &fields[i];
    const mc_nbt_tag root = {.type = MC_NBT_COMPOUND, .value.compound = {children, field_count}};
    mc_nbt_tree *tree = NULL;
    mc_nbt_error nbt = {0};
    const mc_nbt_status copied = mc_nbt_tree_copy_compound(&root, &limits, &tree, &nbt);
    if (copied != MC_NBT_OK)
        status = tile_entity_nbt_error(error, copied, nbt, NULL, SIZE_MAX);
    else {
        *out = tree;
        status = tile_entity_status(error, MC_TILE_ENTITY_OK, NULL, SIZE_MAX);
    }
done:
    for (size_t i = 0; i < occupied; ++i)
        mc_item_stack_release(held[i]);
    return status;
}

typedef struct source_tile_record {
    mc_nbt_tree *tree;
    struct source_tile_record *next;
} source_tile_record;

mc_world_storage_status
mc_live_tile_entities_build_source_records(const mc_world_chunk *chunk,
                                           const mc_nbt_limits *requested, mc_nbt_tree **out,
                                           mc_live_tile_entity_error *error) {
    mc_live_tile_entity_error frame;
    mc_live_tile_entities_error_init(&frame);
    mc_nbt_limits limits;
    if (out == NULL || chunk == NULL || !tile_entity_limits(requested, &limits) ||
        !mc_live_tile_entities_map_valid(chunk)) {
        live_fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                  "fresh source TileEntity Map admission");
        return live_finish(NULL, frame, error);
    }
    const mc_tile_entity_map_backing *map = chunk->l->backing;
    if ((map == NULL && chunk->l->release_backing != NULL) ||
        (map != NULL && map->journal_started && !map->journal_complete)) {
        live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                  "source save incomplete TileEntity import witness policy");
        return live_finish(NULL, frame, error);
    }
    if (map != NULL && map->journal_complete) {
        const mc_nbt_tag *records =
            map->journal == NULL
                ? NULL
                : mc_nbt_compound_get(mc_nbt_tree_root(map->journal), live_records_name, 12);
        if (records == NULL || records->type != MC_NBT_LIST ||
            records->value.list.length != map->record_count ||
            (map->record_count != 0 && map->consumed == NULL)) {
            live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "source save TileEntity import witness policy");
            return live_finish(NULL, frame, error);
        }
        for (size_t i = 0; i < map->record_count; ++i)
            if (!map->consumed[i]) {
                live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "source save unconsumed opaque TileEntity record policy");
                return live_finish(NULL, frame, error);
            }
    }
    source_tile_record *first = NULL, *last = NULL;
    const mc_nbt_tag **items = NULL;
    mc_nbt_tree *built = NULL;
    size_t count = 0, live = 0;
    for (size_t i = 0; i < chunk->l->count; ++i) {
        mc_tile_entity *tile = NULL;
        frame.bytecode = 260;
        if (mc_live_tile_entities_map_at(chunk, i, &tile) != MC_WORLD_STORAGE_OK) {
            live_fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "source TileEntity Map.values iteration");
            goto done;
        }
        frame.bytecode = 271;
        mc_nbt_limits remaining = limits;
        remaining.max_allocation_bytes =
            live <= limits.max_allocation_bytes ? limits.max_allocation_bytes - live : 0;
        mc_nbt_tree *record = NULL;
        const mc_tile_entity_status saved =
            mc_tile_entity_build_source_nbt(tile, &remaining, &record, &frame.tile_entity);
        if (saved != MC_TILE_ENTITY_OK) {
            live_tile_entity_error(&frame, saved, "actual source TileEntity writer");
            frame.bytecode = 271;
            goto done;
        }
        frame.bytecode = 278;
        const size_t bytes = mc_nbt_tree_allocation_bytes(record);
        if (count >= (size_t)INT32_MAX || count >= limits.max_elements ||
            live > limits.max_allocation_bytes || bytes > limits.max_allocation_bytes - live ||
            sizeof(source_tile_record) > limits.max_allocation_bytes - live - bytes) {
            mc_nbt_tree_free(record);
            live_fail(&frame, MC_WORLD_STORAGE_LIMIT, "source TileEntity append budget");
            goto done;
        }
        source_tile_record *node = malloc(sizeof(*node));
        if (node == NULL) {
            mc_nbt_tree_free(record);
            live_fail(&frame, MC_WORLD_STORAGE_ALLOCATION_FAILED, "source TileEntity append owner");
            goto done;
        }
        *node = (source_tile_record){.tree = record};
        if (last == NULL)
            first = node;
        else
            last->next = node;
        last = node;
        ++count;
        live += bytes + sizeof(*node);
    }
    frame.bytecode = 289;
    if (count > SIZE_MAX / sizeof(*items) || live > limits.max_allocation_bytes ||
        count * sizeof(*items) > limits.max_allocation_bytes - live) {
        live_fail(&frame, MC_WORLD_STORAGE_LIMIT, "source TileEntity list pointer budget");
        goto done;
    }
    const size_t pointer_bytes = count * sizeof(*items);
    if (count != 0) {
        items = malloc(pointer_bytes);
        if (items == NULL) {
            live_fail(&frame, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                      "source TileEntity list pointers");
            goto done;
        }
        size_t i = 0;
        for (source_tile_record *node = first; node != NULL; node = node->next)
            items[i++] = mc_nbt_tree_root(node->tree);
    }
    const mc_nbt_tag list = {
        .type = MC_NBT_LIST,
        .name = {live_records_name, 12},
        .value.list = {.element_type = MC_NBT_COMPOUND, .items = items, .length = count}};
    const mc_nbt_tag *child = &list;
    const mc_nbt_tag root = {.type = MC_NBT_COMPOUND, .value.compound = {&child, 1}};
    mc_nbt_limits remaining = limits;
    remaining.max_allocation_bytes -= live + pointer_bytes;
    mc_nbt_error nbt = {0};
    const mc_nbt_status copied = mc_nbt_tree_copy_compound(&root, &remaining, &built, &nbt);
    if (copied != MC_NBT_OK) {
        frame.tile_entity.nbt = nbt;
        live_fail(&frame,
                  copied == MC_NBT_LIMIT               ? MC_WORLD_STORAGE_LIMIT
                  : copied == MC_NBT_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                                       : MC_WORLD_STORAGE_INVALID_DATA,
                  "source TileEntity independent list tree");
    }
done:
    free(items);
    while (first != NULL) {
        source_tile_record *next = first->next;
        mc_nbt_tree_free(first->tree);
        free(first);
        first = next;
    }
    if (frame.status == MC_WORLD_STORAGE_OK)
        *out = built;
    else
        mc_nbt_tree_free(built);
    return live_finish(NULL, frame, error);
}
