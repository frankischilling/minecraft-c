/* Authored native owner/semantic controls. These independent UTF-16 copies do
 * not establish original String-reference aliases, factory reflection catches,
 * mutable Compound prefixes or original wire-order equality. */
static bool sign_line_units(mc_tile_entity *tile, int32_t index, const uint16_t *units,
                            size_t length) {
    mc_tile_entity_name *name = NULL;
    mc_tile_entity_name_view view;
    if (mc_tile_entity_sign_get_line(tile, index, &name) != MC_TILE_ENTITY_OK)
        return false;
    const bool matches =
        mc_tile_entity_name_get_view(name, &view) == MC_TILE_ENTITY_OK && !view.is_null &&
        view.text.length == length &&
        (length == 0 || memcmp(view.text.units, units, length * sizeof(*units)) == 0);
    mc_tile_entity_name_release(name);
    return matches;
}

static bool sign_components(fixture *f, size_t side) {
    bool ok = false;
    mc_tile_entity *sign = NULL, *fresh = NULL, *imported = NULL;
    mc_tile_entity_classes *tile_classes = NULL;
    mc_tile_entity_name *first = NULL, *same = NULL, *overlong = NULL;
    mc_nbt_tree *source = NULL;
    mc_nbt_document *stored = NULL;
    mc_live_tile_entity_error live_error;
    mc_tile_entity_error error;
    mc_tile_entity_snapshot state;
    mc_nbt_tag fields[9] = {0}, input = {.type = MC_NBT_COMPOUND};
    const mc_nbt_tag *children[9];
    uint16_t names[9][16], id[8], text[4][32];
    const char *const keys[] = {"id", "x", "y", "z", "Text1", "Text2", "Text3", "Text4", "Extra"};
    for (size_t i = 0; i < COUNT(fields); ++i) {
        fields[i].name = ascii(keys[i], names[i]);
        children[i] = &fields[i];
    }
    fields[0].type = MC_NBT_STRING;
    fields[0].value.string = ascii("Sign", id);
    for (size_t i = 1; i <= 3; ++i) {
        fields[i].type = MC_NBT_INT;
        fields[i].value.int_value = (int32_t)(i * 11u);
    }
    for (size_t i = 0; i < 4; ++i) {
        for (size_t j = 0; j < COUNT(text[i]); ++j)
            text[i][j] = (uint16_t)('A' + i);
        fields[4 + i].type = MC_NBT_STRING;
        fields[4 + i].value.string = (mc_nbt_string){text[i], 14u + i};
    }
    /* Text3 is cut through the pair; Text4 retains an unpaired low surrogate. */
    text[2][14] = UINT16_C(0xd83d);
    text[2][15] = UINT16_C(0xde80);
    text[3][0] = UINT16_C(0xdc00);
    fields[8].type = MC_NBT_INT;
    fields[8].value.int_value = 42;
    input.value.compound.items = children;
    input.value.compound.length = COUNT(children);

    CHECK(mc_live_tile_entities_new(f->worlds[side], MC_TILE_ENTITY_SIGN, &sign, &live_error) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_tile_entity_classes_borrow(f->classes[side], &tile_classes) == MC_TILE_ENTITY_OK);
    CHECK(mc_tile_entity_get_snapshot(sign, &state) == MC_TILE_ENTITY_OK &&
          state.kind == MC_TILE_ENTITY_SIGN && state.phase == MC_TILE_ENTITY_READY &&
          state.world == NULL && !state.invalid && state.x == 0 && state.y == 0 && state.z == 0 &&
          state.sign_line_count == 4 && state.sign_edit_row == -1 && state.sign_read_flag);
    const mc_tile_entity_class_literal *literal = NULL;
    mc_tile_entity_class_literal_view class_view;
    CHECK(mc_tile_entity_get_class(sign, &literal) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_class_literal_inspect(literal, &class_view) == MC_TILE_ENTITY_OK &&
          class_view.kind == MC_TILE_ENTITY_CLASS_KIND_SIGN);
    size_t count = SIZE_MAX, before = 0, after = 0;
    CHECK(mc_tile_entity_sign_line_count(sign, &count) == MC_TILE_ENTITY_OK && count == 4 &&
          mc_tile_entity_sign_get_line(sign, 0, &first) == MC_TILE_ENTITY_OK && first != NULL);
    for (int32_t i = 0; i < 4; ++i) {
        CHECK(mc_tile_entity_sign_get_line(sign, i, &same) == MC_TILE_ENTITY_OK && same == first &&
              sign_line_units(sign, i, NULL, 0));
        mc_tile_entity_name_release(same);
        same = NULL;
    }
    same = first; /* Borrowed sentinel, not another hold. */
    const bool negative_preserved =
        mc_tile_entity_sign_get_line(sign, -1, &same) == MC_TILE_ENTITY_INVALID_SLOT &&
        same == first;
    const bool high_preserved =
        mc_tile_entity_sign_get_line(sign, 4, &same) == MC_TILE_ENTITY_INVALID_SLOT &&
        same == first;
    same = NULL;
    CHECK(negative_preserved && high_preserved);
    CHECK(mc_tile_entity_name_create(text[0], 31, &overlong) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_storage_bytes(sign, &before) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_sign_set_line(sign, 0, overlong) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_storage_bytes(sign, &after) == MC_TILE_ENTITY_OK &&
          after == before + mc_tile_entity_name_native_bytes(overlong));
    before = after;
    CHECK(mc_tile_entity_sign_set_line(sign, 1, overlong) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_storage_bytes(sign, &after) == MC_TILE_ENTITY_OK && after == before &&
          mc_tile_entity_name_get_view(first, &(mc_tile_entity_name_view){0}) ==
              MC_TILE_ENTITY_OK &&
          mc_tile_entity_sign_get_line(sign, 0, &same) == MC_TILE_ENTITY_OK && same == overlong);
    mc_tile_entity_name_release(same);
    same = NULL;
    CHECK(mc_tile_entity_build_source_nbt(sign, NULL, &source, &error) == MC_TILE_ENTITY_OK);
    const mc_nbt_tag *tag = mc_nbt_compound_get(mc_nbt_tree_root(source), names[4], 5);
    CHECK(tag != NULL && tag->type == MC_NBT_STRING && tag->value.string.length == 31 &&
          memcmp(tag->value.string.units, text[0], 31u * sizeof(uint16_t)) == 0);
    mc_nbt_tree_free(source);
    source = NULL;

    CHECK(mc_tile_entity_sign_set_edit_row(sign, INT32_MIN) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_set_invalid(sign, true) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_read_nbt(sign, &input, &error) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_get_snapshot(sign, &state) == MC_TILE_ENTITY_OK && !state.sign_read_flag &&
          state.sign_edit_row == INT32_MIN && state.invalid && state.x == 11 && state.y == 22 &&
          state.z == 33);
    for (size_t i = 0; i < 4; ++i)
        CHECK(sign_line_units(sign, (int32_t)i, text[i], i == 0 ? 14 : 15));
    CHECK(mc_tile_entity_name_get_view(overlong, &(mc_tile_entity_name_view){0}) ==
          MC_TILE_ENTITY_OK);
    /* Each failing getter retains all earlier assignments and the old suffix. */
    for (size_t failure = 0; failure < 4; ++failure) {
        CHECK(mc_tile_entity_read_nbt(sign, &input, &error) == MC_TILE_ENTITY_OK);
        uint16_t previous_first[4];
        for (size_t i = 0; i < 4; ++i)
            previous_first[i] = text[i][0];
        for (size_t i = 0; i < 4; ++i)
            text[i][0] = (uint16_t)('K' + i);
        fields[4 + failure].type = MC_NBT_BYTE;
        CHECK(mc_tile_entity_read_nbt(sign, &input, &error) == MC_TILE_ENTITY_INVALID_FIELD &&
              error.read_bytecode == 44 && strcmp(error.field, keys[4 + failure]) == 0);
        fields[4 + failure].type = MC_NBT_STRING;
        for (size_t i = 0; i < 4; ++i) {
            const uint16_t current = text[i][0];
            if (i >= failure)
                text[i][0] = previous_first[i];
            CHECK(sign_line_units(sign, (int32_t)i, text[i], i == 0 ? 14 : 15));
            text[i][0] = current;
        }
        for (size_t i = 0; i < 4; ++i)
            text[i][0] = previous_first[i];
    }
    for (size_t failure = 1; failure <= 3; ++failure) {
        CHECK(mc_live_tile_entities_new(f->worlds[side], MC_TILE_ENTITY_SIGN, &fresh,
                                        &live_error) == MC_WORLD_STORAGE_OK);
        fields[failure].type = MC_NBT_SHORT;
        CHECK(mc_tile_entity_read_nbt(fresh, &input, &error) == MC_TILE_ENTITY_INVALID_FIELD &&
              error.base_read_bytecode == (failure == 1   ? 4u
                                           : failure == 2 ? 14u
                                                          : 24u) &&
              error.read_bytecode == 7 &&
              mc_tile_entity_get_snapshot(fresh, &state) == MC_TILE_ENTITY_OK &&
              !state.sign_read_flag && state.sign_edit_row == -1 &&
              state.x == (failure > 1 ? 11 : 0) && state.y == (failure > 2 ? 22 : 0) &&
              state.z == 0 && sign_line_units(fresh, 0, NULL, 0));
        fields[failure].type = MC_NBT_INT;
        mc_tile_entity_release(fresh);
        fresh = NULL;
    }
    CHECK(mc_live_tile_entities_new(f->worlds[side], MC_TILE_ENTITY_SIGN, &fresh, &live_error) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_tile_entity_read_nbt(fresh, NULL, &error) == MC_TILE_ENTITY_INVALID_ARGUMENT &&
          error.base_read_bytecode == 4 && error.read_bytecode == 7 &&
          mc_tile_entity_get_snapshot(fresh, &state) == MC_TILE_ENTITY_OK && !state.sign_read_flag);
    mc_tile_entity_release(fresh);
    fresh = NULL;
    const mc_nbt_tag missing = {.type = MC_NBT_COMPOUND};
    CHECK(mc_tile_entity_read_nbt(sign, &missing, &error) == MC_TILE_ENTITY_OK);
    for (int32_t i = 0; i < 4; ++i)
        CHECK(sign_line_units(sign, i, NULL, 0));
    CHECK(mc_tile_entity_sign_set_line(sign, 2, NULL) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_sign_get_line(sign, 2, &same) == MC_TILE_ENTITY_OK && same == NULL &&
          mc_tile_entity_build_source_nbt(sign, NULL, &source, &error) ==
              MC_TILE_ENTITY_INVALID_ARGUMENT &&
          source == NULL && error.save_bytecode == 38 && strcmp(error.field, "Text3") == 0);

    /* Real factory chooses LIVE_SIGN; preservation and fresh Source results
     * intentionally have different unknown-field retention contracts. */
    CHECK(mc_live_tile_entities_factory(f->worlds[side], &input, NULL, true, &imported,
                                        &live_error) == MC_WORLD_STORAGE_OK &&
          imported != NULL && mc_tile_entity_source(imported) != NULL);
    CHECK(mc_tile_entity_get_snapshot(imported, &state) == MC_TILE_ENTITY_OK &&
          state.kind == MC_TILE_ENTITY_SIGN && !state.sign_read_flag && state.sign_edit_row == -1 &&
          sign_line_units(imported, 2, text[2], 15));
    CHECK(mc_tile_entity_build(imported, NULL, &stored, &error) == MC_TILE_ENTITY_OK);
    tag = mc_nbt_compound_get(mc_nbt_document_root(stored), names[8], 5);
    CHECK(tag != NULL && tag->type == MC_NBT_INT && tag->value.int_value == 42);
    CHECK(mc_tile_entity_build_source_nbt(imported, NULL, &source, &error) == MC_TILE_ENTITY_OK &&
          mc_nbt_compound_get(mc_nbt_tree_root(source), names[8], 5) == NULL);
    text[2][0] = 'Z';
    tag = mc_nbt_compound_get(mc_nbt_document_root(stored), names[6], 5);
    CHECK(tag != NULL && tag->value.string.units[0] == 'C' &&
          tag->value.string.units[14] == UINT16_C(0xd83d));
    text[2][0] = 'C';
    mc_tile_entity_release(imported);
    imported = NULL;
    CHECK(mc_live_tile_entities_factory(f->worlds[side], &input, NULL, false, &imported,
                                        &live_error) == MC_WORLD_STORAGE_OK &&
          mc_tile_entity_source(imported) == NULL);
    mc_nbt_limits limits = mc_nbt_default_limits();
    limits.max_allocation_bytes = mc_tile_entity_native_bytes(imported) - 1u;
    fresh = sign; /* Borrowed sentinel; failed import must not publish over it. */
    const mc_tile_entity_status limited = mc_tile_entity_import_storage(
        f->worlds[side], MC_TILE_ENTITY_SIGN, &input, &limits, &fresh, &error);
    const bool preserved = fresh == sign;
    fresh = NULL;
    CHECK(limited == MC_TILE_ENTITY_LIMIT && preserved);
    ok = true;
cleanup:
    mc_nbt_document_free(stored);
    mc_nbt_tree_free(source);
    mc_tile_entity_name_release(overlong);
    mc_tile_entity_name_release(same);
    mc_tile_entity_name_release(first);
    mc_tile_entity_release(imported);
    mc_tile_entity_release(fresh);
    mc_tile_entity_release(sign);
    if (mc_tile_entity_classes_release(&tile_classes) != MC_TILE_ENTITY_OK)
        return false;
    return ok;
}
