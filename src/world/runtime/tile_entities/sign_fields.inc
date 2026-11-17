/* Concrete YK/pq owner fields. Included after the common immutable name and
 * native-budget helpers; no independent runtime or alternate registry. */
static mc_tile_entity_status tile_entity_sign_construct(mc_tile_entity *raw,
                                                        const tile_entity_import_context *context,
                                                        mc_tile_entity_error *error) {
    const mc_tile_entity_status admitted = tile_entity_import_admit(
        raw, context, 4u * sizeof(mc_tile_entity_name *), error, NULL, SIZE_MAX);
    if (admitted != MC_TILE_ENTITY_OK) {
        if (error != NULL)
            error->constructor_bytecode = 6;
        return admitted;
    }
    mc_tile_entity_name **lines = calloc(4u, sizeof(*lines));
    if (lines == NULL) {
        const mc_tile_entity_status failed =
            tile_entity_status(error, MC_TILE_ENTITY_ALLOCATION_FAILED, NULL, SIZE_MAX);
        if (error != NULL)
            error->constructor_bytecode = 6;
        return failed;
    }
    /* Native custody/accounting of the constructor-private array precedes its
     * Source field publication at29. A failed constructor's count remains0. */
    raw->state.sign.lines = lines;
    raw->state.sign.capacity = 4;
    mc_registry_error attempt = live_empty_registry();
    mc_tile_entity_status status =
        live_literal(raw->classes, "", &lines[0], &attempt, raw, context);
    if (status != MC_TILE_ENTITY_OK) {
        tile_entity_status(error, status, NULL, SIZE_MAX);
        if (error != NULL) {
            error->constructor_bytecode = 11;
            error->registry = attempt;
        }
        return status;
    }
    for (size_t i = 1; i < 4; ++i) {
        status = mc_tile_entity_name_retain(lines[0]);
        if (status != MC_TILE_ENTITY_OK) {
            tile_entity_status(error, status, NULL, SIZE_MAX);
            if (error != NULL)
                error->constructor_bytecode = 13u + 5u * (unsigned int)i;
            return status;
        }
        lines[i] = lines[0];
    }
    raw->state.sign.count = 4;        /* YK.a29. */
    raw->state.sign.edit_row = -1;    /* YK.b34. */
    raw->state.sign.read_flag = true; /* YK.c39. */
    return MC_TILE_ENTITY_OK;
}

static mc_tile_entity_status tile_entity_sign_ready(const mc_tile_entity *tile) {
    const mc_tile_entity_status ready = tile_entity_kind_ready(tile, MC_TILE_ENTITY_SIGN);
    if (ready != MC_TILE_ENTITY_OK)
        return ready;
    if (tile->state.sign.lines == NULL || tile->state.sign.capacity != 4 ||
        tile->state.sign.count != 4)
        return MC_TILE_ENTITY_STATE_ERROR;
    return MC_TILE_ENTITY_OK;
}

mc_tile_entity_status mc_tile_entity_sign_line_count(const mc_tile_entity *tile, size_t *out) {
    if (out == NULL)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    const mc_tile_entity_status ready = tile_entity_sign_ready(tile);
    if (ready != MC_TILE_ENTITY_OK)
        return ready;
    *out = tile->state.sign.count;
    return MC_TILE_ENTITY_OK;
}

mc_tile_entity_status mc_tile_entity_sign_get_line(mc_tile_entity *tile, int32_t index,
                                                   mc_tile_entity_name **out) {
    if (out == NULL)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    const mc_tile_entity_status ready = tile_entity_sign_ready(tile);
    if (ready != MC_TILE_ENTITY_OK)
        return ready;
    if (index < 0 || (size_t)index >= tile->state.sign.count)
        return MC_TILE_ENTITY_INVALID_SLOT;
    mc_tile_entity_name *name = tile->state.sign.lines[(size_t)index];
    const mc_tile_entity_status held = mc_tile_entity_name_retain(name);
    if (held != MC_TILE_ENTITY_OK)
        return held;
    *out = name;
    return MC_TILE_ENTITY_OK;
}

mc_tile_entity_status mc_tile_entity_sign_set_line(mc_tile_entity *tile, int32_t index,
                                                   mc_tile_entity_name *name) {
    const mc_tile_entity_status ready = tile_entity_sign_ready(tile);
    if (ready != MC_TILE_ENTITY_OK)
        return ready;
    if (index < 0 || (size_t)index >= tile->state.sign.count)
        return MC_TILE_ENTITY_INVALID_SLOT;
    const mc_tile_entity_status held = mc_tile_entity_name_retain(name);
    if (held != MC_TILE_ENTITY_OK)
        return held;
    mc_tile_entity_name *old = tile->state.sign.lines[(size_t)index];
    tile->state.sign.lines[(size_t)index] = name;
    mc_tile_entity_name_release(old);
    return MC_TILE_ENTITY_OK;
}

mc_tile_entity_status mc_tile_entity_sign_get_edit_row(const mc_tile_entity *tile, int32_t *out) {
    if (out == NULL)
        return MC_TILE_ENTITY_INVALID_ARGUMENT;
    const mc_tile_entity_status ready = tile_entity_kind_ready(tile, MC_TILE_ENTITY_SIGN);
    if (ready != MC_TILE_ENTITY_OK)
        return ready;
    *out = tile->state.sign.edit_row;
    return MC_TILE_ENTITY_OK;
}

mc_tile_entity_status mc_tile_entity_sign_set_edit_row(mc_tile_entity *tile, int32_t row) {
    const mc_tile_entity_status ready = tile_entity_kind_ready(tile, MC_TILE_ENTITY_SIGN);
    if (ready != MC_TILE_ENTITY_OK)
        return ready;
    tile->state.sign.edit_row = row;
    return MC_TILE_ENTITY_OK;
}
