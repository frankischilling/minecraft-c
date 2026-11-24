/* Original E.a(FD,III)Z and its private invokespecial E.h(FD,III)Z. The first
 * four IDs only count neighbors; every reached helper queries again. */
static bool chest_predicate_get(mc_world_placement_error *e, mc_world_runtime *world, int32_t x,
                                int32_t y, int32_t z, int32_t *out) {
    chest_placement_work *w = e->work->chest_placement;
    mc_world_chest_placement_detail *d = &w->detail;
    if (!hold_world(e, world))
        return false;
    const size_t index = d->query_count;
    if (index == 24) {
        fail(e, MC_WORLD_STORAGE_LIMIT, "Chest placement reached query extent", true);
        return false;
    }
    mc_world_runtime_acquisition_error *query = calloc(1, sizeof(*query));
    if (query == NULL) {
        fail(e, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Chest placement reached query diagnostic",
             true);
        return false;
    }
    w->queries[index] = query;
    d->queries[index] = query;
    d->query_sites[index] = e->site;
    d->query_parent_pc[index] = e->site.method[0] == 'h' ? d->caller.pc : 0;
    d->query_x[index] = x;
    d->query_y[index] = y;
    d->query_z[index] = z;
    ++d->query_count;
    if (!query_value(e, world, query, x, y, z, false, out))
        return false;
    d->query_returned[index] = true;
    d->query_values[index] = *out;
    return true;
}

static bool chest_paired(mc_world_placement_error *e, const mc_block *block,
                         mc_world_runtime *world, int32_t x, int32_t y, int32_t z, bool *out) {
    mc_world_chest_placement_detail *d = &e->work->chest_placement->detail;
    d->helper_returned = false;
    ++d->helper_count;
    d->site = e->site = site("e", "h", "(Lfd;III)Z", 5);
    int32_t id;
    if (!chest_predicate_get(e, world, x, y, z, &id))
        return false;
    if (id != block->fields.bn) {
        d->site.pc = e->site.pc = 16;
        d->helper_returned = true;
        d->helper_value = *out = false;
        return true;
    }
    const int32_t xs[] = {mc_java_i32_sub(x, 1), mc_java_i32_add(x, 1), x, x};
    const int32_t zs[] = {z, z, mc_java_i32_sub(z, 1), mc_java_i32_add(z, 1)};
    const unsigned int pcs[] = {24, 43, 62, 81}, returns[] = {35, 54, 73, 92};
    for (size_t i = 0; i < 4; ++i) {
        d->site.pc = e->site.pc = pcs[i];
        if (!chest_predicate_get(e, world, xs[i], y, zs[i], &id))
            return false;
        if (id == block->fields.bn) {
            d->site.pc = e->site.pc = returns[i];
            d->helper_returned = true;
            d->helper_value = *out = true;
            return true;
        }
    }
    d->site.pc = e->site.pc = 94;
    d->helper_returned = true;
    d->helper_value = *out = false;
    return true;
}

static bool chest_predicate_return(mc_world_placement_error *e, bool value, unsigned int pc,
                                   bool *out) {
    mc_world_chest_placement_detail *d = &e->work->chest_placement->detail;
    d->site = e->site = site("e", "a", "(Lfd;III)Z", pc);
    e->base.pc = pc;
    d->returned = e->base_returned = e->predicate_returned = true;
    d->value = e->base_value = *out = value;
    return true;
}

static bool chest_predicate(mc_world_placement_error *e, const mc_block *block,
                            mc_world_runtime *world, int32_t x, int32_t y, int32_t z, bool *out) {
    chest_placement_work *w = calloc(1, sizeof(*w));
    if (w == NULL) {
        fail(e, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Chest placement retained child allocation",
             true);
        return false;
    }
    e->work->chest_placement = w;
    mc_world_chest_placement_detail *d = &w->detail;
    e->chest_placement = d;
    const int32_t xs[] = {mc_java_i32_sub(x, 1), mc_java_i32_add(x, 1), x, x};
    const int32_t zs[] = {z, z, mc_java_i32_sub(z, 1), mc_java_i32_add(z, 1)};
    const unsigned int pcs[] = {10, 30, 50, 70};
    for (size_t i = 0; i < 4; ++i) {
        d->site = e->site = site("e", "a", "(Lfd;III)Z", pcs[i]);
        e->base.pc = pcs[i];
        int32_t id;
        if (!chest_predicate_get(e, world, xs[i], y, zs[i], &id))
            return false;
        if (id == block->fields.bn)
            ++d->neighbor_count;
    }
    if (d->neighbor_count > 1)
        return chest_predicate_return(e, false, 90, out);
    const unsigned int calls[] = {99, 115, 131, 147}, returns[] = {106, 122, 138, 154};
    for (size_t i = 0; i < 4; ++i) {
        e->base.pc = calls[i];
        d->caller = e->base;
        bool paired;
        if (!chest_paired(e, block, world, xs[i], y, zs[i], &paired))
            return false;
        if (paired)
            return chest_predicate_return(e, false, returns[i], out);
    }
    return chest_predicate_return(e, true, 156, out);
}
