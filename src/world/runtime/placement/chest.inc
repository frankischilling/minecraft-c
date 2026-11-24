/* The actual E.a(FD,III,GS) body. Local JS and ordered OG occurrences remain
 * live through the enclosing call; native retirement never closes a screen. */
static bool chest_inventory_result(mc_world_placement_error *e, mc_inventory_status status) {
    e->chest = &e->work->chest_error;
    if (status == MC_INVENTORY_OK)
        return true;
    const mc_inventory_error *child = e->chest;
    mc_world_storage_status cause =
        status == MC_INVENTORY_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
        : status == MC_INVENTORY_LIMIT           ? MC_WORLD_STORAGE_LIMIT
        : status == MC_INVENTORY_BUSY            ? MC_WORLD_STORAGE_BUSY
                                                 : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    fail(e, cause, child->operation, true);
    if (child->source_exception) {
        e->source_exception = true;
        e->native_policy = false;
        e->exception_class = child->exception_class;
    }
    if (child->frame_count != 0 &&
        (status == MC_INVENTORY_NULL_REFERENCE || status == MC_INVENTORY_INDEX_OUT_OF_BOUNDS)) {
        e->source_exception = true;
        e->native_policy = false;
        e->exception_class = status == MC_INVENTORY_NULL_REFERENCE
                                 ? "java/lang/NullPointerException"
                                 : "java/lang/ArrayIndexOutOfBoundsException";
    }
    return false;
}

static bool chest_owner(mc_world_placement_error *e, mc_inventory_chest *owner) {
    if (owner == NULL)
        return true;
    mc_world_placement_work *w = e->work;
    if (w->chest_view_count == sizeof w->chest_views / sizeof *w->chest_views) {
        mc_inventory_chest_release(owner);
        fail(e, MC_WORLD_STORAGE_LIMIT, "Chest activation local inventory extent", true);
        return false;
    }
    w->chest_views[w->chest_view_count++] = owner;
    return true;
}

static bool chest_tile_get(mc_world_placement_error *e, mc_world_runtime *world, int32_t x,
                           int32_t y, int32_t z, unsigned int pc, unsigned int cast_pc,
                           mc_tile_entity **out) {
    e->site.pc = pc;
    if (!hold_world(e, world))
        return false;
    const size_t index = e->chest_tile_count;
    if (index == 5) {
        fail(e, MC_WORLD_STORAGE_LIMIT, "Chest activation TileEntity query extent", true);
        return false;
    }
    mc_live_tile_entity_error *child = calloc(1, sizeof(*child));
    if (child == NULL) {
        fail(e, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Chest activation TileEntity diagnostic", true);
        return false;
    }
    e->work->chest_tiles[index] = child;
    ++e->chest_tile_count;
    e->chest_tile = child;
    const mc_world_storage_status status =
        mc_live_tile_entities_get(world, x, y, z, &e->work->chest_receivers[index], child);
    if (status != MC_WORLD_STORAGE_OK) {
        fail(e, status, child->operation, true);
        e->cause = child->cause;
        e->registry = child->registry;
    }
    if (!capture(e)) {
        if (status == MC_WORLD_STORAGE_OK)
            fail(e, e->cleanup.status, e->cleanup.operation, true);
        return false;
    }
    if (status != MC_WORLD_STORAGE_OK) {
        classify_receipts(e);
        return false;
    }
    e->site.pc = cast_pc;
    mc_tile_entity *tile = e->work->chest_receivers[index];
    if (tile != NULL) {
        mc_tile_entity_snapshot state;
        if (mc_tile_entity_get_snapshot(tile, &state) != MC_TILE_ENTITY_OK) {
            fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Chest activation actual cast receiver",
                 true);
            return false;
        }
        if (state.kind != MC_TILE_ENTITY_CHEST) {
            fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Chest activation JS cast", false);
            e->source_exception = true;
            e->exception_class = "java/lang/ClassCastException";
            return false;
        }
    }
    *out = tile;
    return true;
}

static bool chest_view(mc_world_placement_error *e, mc_tile_entity *tile,
                       mc_inventory_chest **out) {
    if (tile == NULL) {
        *out = NULL; /* The source cast of NULL produces the same NULL LW. */
        return true;
    }
    mc_inventory_chest *view = NULL;
    if (!chest_inventory_result(e, mc_inventory_chest_from_tile(MC_REGISTRY_CLIENT, tile, NULL,
                                                                &view, &e->work->chest_error)))
        return false;
    if (!chest_owner(e, view))
        return false;
    *out = view;
    return true;
}

static bool chest_activate(mc_world_placement_error *e, const mc_block *block,
                           mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                           mc_entity *player, const mc_world_block_activation_context *context) {
    e->site = site("e", "a", "(Lfd;IIILgs;)Z", 5);
    mc_tile_entity *tile = NULL;
    if (!chest_tile_get(e, world, x, y, z, 5, 8, &tile))
        return false;
    e->site.pc = 20;
    const int32_t above = mc_java_i32_add(y, 1);
    bool blocked;
    if (!torch_solid(e, world, x, above, z, &blocked))
        return false;
    if (blocked) {
        e->chest_blocked = true;
        e->site.pc = 27;
        return true;
    }
    const int32_t xs[] = {mc_java_i32_sub(x, 1), mc_java_i32_add(x, 1), x, x};
    const int32_t zs[] = {z, z, mc_java_i32_sub(z, 1), mc_java_i32_add(z, 1)};
    const unsigned int id_pc[] = {35, 69, 103, 137};
    const unsigned int above_pc[] = {54, 88, 122, 156};
    const unsigned int blocked_pc[] = {61, 95, 129, 163};
    for (size_t i = 0; i < 4; ++i) {
        int32_t id;
        e->site.pc = id_pc[i];
        if (!block_get(e, world, i, xs[i], y, zs[i], &id))
            return false;
        /* The actual Block.bn read is repeated after each World getter. */
        if (id == block->fields.bn) {
            e->site.pc = above_pc[i];
            if (!torch_solid(e, world, xs[i], above, zs[i], &blocked))
                return false;
            if (blocked) {
                e->chest_blocked = true;
                e->site.pc = blocked_pc[i];
                return true;
            }
        }
    }
    mc_inventory_chest *inventory = NULL;
    if (!chest_view(e, tile, &inventory))
        return false;
    e->chest_inventory = inventory;
    const unsigned int join_id_pc[] = {171, 214, 257, 300};
    const unsigned int new_pc[] = {181, 224, 267, 310};
    const unsigned int tile_pc[] = {194, 239, 280, 325};
    const unsigned int cast_pc[] = {197, 242, 283, 328};
    const unsigned int construct_pc[] = {202, 245, 288, 331};
    for (size_t i = 0; i < 4; ++i) {
        int32_t id;
        e->site.pc = join_id_pc[i];
        if (!block_get(e, world, i + 4, xs[i], y, zs[i], &id))
            return false;
        if (id != block->fields.bn)
            continue;
        e->site.pc = new_pc[i];
        mc_inventory_chest *joined = NULL;
        if (!chest_inventory_result(e, mc_inventory_chest_large_allocate(MC_REGISTRY_CLIENT, NULL,
                                                                         &joined,
                                                                         &e->work->chest_error)) ||
            !chest_owner(e, joined))
            return false;
        mc_tile_entity *neighbor = NULL;
        if (!chest_tile_get(e, world, xs[i], y, zs[i], tile_pc[i], cast_pc[i], &neighbor))
            return false;
        mc_inventory_chest *other = NULL;
        if (!chest_view(e, neighbor, &other))
            return false;
        e->site.pc = construct_pc[i];
        const bool prepend = i == 0 || i == 2;
        if (!chest_inventory_result(e, mc_inventory_chest_large_construct(
                                           joined, "Large chest", prepend ? other : inventory,
                                           prepend ? inventory : other, &e->work->chest_error)))
            return false;
        inventory = joined;
        e->chest_inventory = inventory;
        ++e->chest_join_count;
    }
    e->site.pc = 337;
    e->remote_read = true;
    e->remote = world->fields.B;
    if (e->remote) {
        e->site.pc = 344;
        return true;
    }
    e->site.pc = 349;
    if (player == NULL) {
        null_reference(e, "E actual Player Chest invocation null receiver");
        return false;
    }
    if (context == NULL || context->chest == NULL) {
        fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "E reached Player Chest display child", true);
        return false;
    }
    e->caller = e->site;
    e->chest_entered = true;
    const mc_world_storage_status status = context->chest(context->context, player, inventory, e);
    if (status != MC_WORLD_STORAGE_OK) {
        e->status = status;
        return false;
    }
    e->chest_returned = true;
    e->site = site("e", "a", "(Lfd;IIILgs;)Z", 353);
    return true;
}
