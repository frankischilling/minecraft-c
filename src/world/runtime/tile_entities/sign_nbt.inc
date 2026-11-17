/* Direct YK/pq reader semantics. Native independent UTF-16 ownership does not
 * qualify original String-object aliases, Compound retention or wire order. */
static const uint16_t tile_entity_sign_text_names[4][5] = {{'T', 'e', 'x', 't', '1'},
                                                           {'T', 'e', 'x', 't', '2'},
                                                           {'T', 'e', 'x', 't', '3'},
                                                           {'T', 'e', 'x', 't', '4'}};
static const char *const tile_entity_sign_text_fields[4] = {"Text1", "Text2", "Text3", "Text4"};

static mc_tile_entity_status
tile_entity_sign_string_copy(mc_tile_entity *tile, const tile_entity_import_context *context,
                             mc_nbt_string text, const char *field, unsigned int pc,
                             mc_tile_entity_name **out, mc_tile_entity_error *error) {
    mc_tile_entity_status status;
    if (text.units == NULL && text.length != 0)
        status = MC_TILE_ENTITY_INVALID_ARGUMENT;
    else if (text.length > (SIZE_MAX - sizeof(mc_tile_entity_name)) / sizeof(uint16_t))
        status = MC_TILE_ENTITY_LIMIT;
    else {
        const size_t bytes = sizeof(mc_tile_entity_name) + text.length * sizeof(uint16_t);
        status = tile_entity_import_admit(tile, context, bytes, error, field, SIZE_MAX);
        if (status == MC_TILE_ENTITY_OK)
            status = mc_tile_entity_name_create(text.units, text.length, out);
    }
    if (status != MC_TILE_ENTITY_OK) {
        tile_entity_status(error, status, field, SIZE_MAX);
        if (error != NULL)
            error->read_bytecode = pc;
    }
    return status;
}

static mc_tile_entity_status tile_entity_sign_read(mc_tile_entity *tile, const mc_nbt_tag *compound,
                                                   tile_entity_import_context *context,
                                                   mc_tile_entity_error *error) {
    const mc_tile_entity_status ready = tile_entity_sign_ready(tile);
    if (ready != MC_TILE_ENTITY_OK)
        return tile_entity_status(error, ready, NULL, SIZE_MAX);
    for (size_t i = 0; i < 4; ++i) {
        const char *field = tile_entity_sign_text_fields[i];
        const mc_nbt_tag *tag = mc_nbt_compound_get(compound, tile_entity_sign_text_names[i], 5);
        if (tag != NULL && tag->type != MC_NBT_STRING)
            return tile_entity_read_error(error, field, 44, false);
        mc_tile_entity_name *name = NULL;
        mc_tile_entity_status status;
        if (tag == NULL) {
            mc_registry_error attempt = live_empty_registry();
            status = live_literal(tile->classes, "", &name, &attempt, tile, context);
            if (status != MC_TILE_ENTITY_OK) {
                tile_entity_status(error, status, field, SIZE_MAX);
                if (error != NULL) {
                    error->read_bytecode = 44;
                    error->registry = attempt;
                }
                return status;
            }
        } else {
            status = tile_entity_sign_string_copy(tile, context, tag->value.string, field, 44,
                                                  &name, error);
            if (status != MC_TILE_ENTITY_OK)
                return status;
        }
        /* Actual assignment47 precedes length54 and substring76. Each native
         * copied value has its own hold; no input Compound span is borrowed. */
        mc_tile_entity_name *old = tile->state.sign.lines[i];
        tile->state.sign.lines[i] = name;
        mc_tile_entity_name_release(old);
        if (name->text.length > 15) {
            mc_tile_entity_name *prefix = NULL;
            status = tile_entity_sign_string_copy(
                tile, context, (mc_nbt_string){name->text.units, 15}, field, 76, &prefix, error);
            if (status != MC_TILE_ENTITY_OK)
                return status;
            tile->state.sign.lines[i] = prefix; /* Reached substring replacement79. */
            mc_tile_entity_name_release(name);
        }
    }
    return tile_entity_status(error, MC_TILE_ENTITY_OK, NULL, SIZE_MAX);
}

static mc_tile_entity_status tile_entity_sign_build_fields(const mc_tile_entity *tile,
                                                           mc_nbt_tag fields[9],
                                                           mc_tile_entity_error *error) {
    const mc_tile_entity_status ready = tile_entity_sign_ready(tile);
    if (ready != MC_TILE_ENTITY_OK)
        return tile_entity_status(error, ready, NULL, SIZE_MAX);
    for (size_t i = 0; i < 4; ++i) {
        mc_tile_entity_name *name = tile->state.sign.lines[i];
        if (name == NULL) {
            const mc_tile_entity_status status = tile_entity_status(
                error, MC_TILE_ENTITY_INVALID_ARGUMENT, tile_entity_sign_text_fields[i], SIZE_MAX);
            if (error != NULL)
                error->save_bytecode = 14u + 12u * (unsigned int)i;
            return status;
        }
        fields[4 + i].type = MC_NBT_STRING;
        fields[4 + i].name = (mc_nbt_string){tile_entity_sign_text_names[i], 5};
        fields[4 + i].value.string = name->text; /* Writer never truncates. */
    }
    return MC_TILE_ENTITY_OK;
}
