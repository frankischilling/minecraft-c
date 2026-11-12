/* Reuse the real owner/transport fixture without running its old scenario or
 * changing its unchanged-region contract. All static helpers retain actual
 * implementations; the renamed complete program keeps their references. */
#define main mc_tiles_fixture_program_main
#include "../tile_entities/test_tile_entities.c"
#undef main

#include "mc/world/runtime/saving/saving.h"

/* Constructed native saves, not original/native or natural-population parity.
 * Projection controls are source inputs restored before CHECK. No phase,
 * membership/count/hold/lease value or successful backend is fabricated. */
typedef struct save_snapshot {
    mc_ws_file_bytes region, level;
    int64_t size, convenience_size;
} save_snapshot;

static uint32_t direct_stamp(size_t slot) {
    return UINT32_C(0x53000000) + (uint32_t)slot;
}

static bool open_saving_store(fixture *f, size_t slot, const char *root) {
    bool ok = false;
    char path[8192];
    uint8_t map[256];
    const uint16_t title[] = {'S', 'a', 'v', 'e'};
    const int n = snprintf(path, sizeof(path), "%s/world%zu", root, slot);
    CHECK(n > 0 && (size_t)n < sizeof(path));
    /* Saving uses identity normalization. The reused TileEntity-only fixture instead
     * erases Chest/Spawner block IDs on its loaded pair to test import order;
     * that separate control must not erase these round-trip source blocks. */
    for (size_t i = 0; i < COUNT(map); ++i)
        map[i] = (uint8_t)i;
    mc_world_store_options options = {.session_token = 137,
                                      .block_map = map,
                                      .block_map_length = sizeof(map),
                                      .limits = mc_world_store_default_limits()};
    options.limits.max_loaded_chunks = 1;
    CHECK(mc_world_store_open(path, f->regions, &options, &f->stores[slot], NULL) ==
          MC_WORLD_STORAGE_OK);
    const mc_level_info info = {.seed = 23,
                                .spawn_y = 64,
                                .time = 0,
                                .level_name = {title, COUNT(title)},
                                .version = 19132};
    CHECK(mc_world_store_save_info(f->stores[slot], &info, 123, NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    return ok;
}

static bool read_named(fixture *f, size_t slot, const char *leaf, mc_ws_file_bytes *out) {
    char *path = NULL;
    if (mc_ws_path_join(mc_world_store_root(f->stores[slot]), leaf, &path, NULL) !=
        MC_WORLD_STORAGE_OK)
        return false;
    const mc_world_storage_status status = mc_ws_read_file(path, MC_WS_FILE_MAX_BYTES, out, NULL);
    free(path);
    return status == MC_WORLD_STORAGE_OK;
}

static void snapshot_free(save_snapshot *s) {
    mc_ws_free_file_bytes(&s->region);
    mc_ws_free_file_bytes(&s->level);
}

static bool snapshot_take(fixture *f, size_t slot, save_snapshot *s) {
    mc_world_info_fields info;
    const mc_level_info *convenience = mc_world_store_info(f->stores[slot]);
    if (convenience == NULL ||
        mc_world_info_fields_get(f->worlds[slot]->fields.x, &info) != MC_WORLD_INFO_OK ||
        !read_named(f, slot, "region/r.0.0.mcr", &s->region) ||
        !read_named(f, slot, "level.dat", &s->level))
        return false;
    s->size = info.value.size_on_disk;
    s->convenience_size = convenience->size_on_disk;
    return true;
}

static bool snapshot_same(fixture *f, size_t slot, const save_snapshot *before) {
    save_snapshot after = {0};
    const bool captured = snapshot_take(f, slot, &after);
    const bool equal = captured && after.size == before->size &&
                       after.convenience_size == before->convenience_size &&
                       after.region.length == before->region.length &&
                       after.level.length == before->level.length &&
                       memcmp(after.region.data, before->region.data, before->region.length) == 0 &&
                       memcmp(after.level.data, before->level.data, before->level.length) == 0;
    snapshot_free(&after);
    return equal;
}

static bool fresh_live_save(const mc_live_tile_entity_error *e) {
    return e->status == MC_WORLD_STORAGE_OK && e->operation == NULL && e->bytecode == 0 &&
           e->chunk_bytecode == 0 && e->cause.status == MC_WORLD_STORAGE_OK &&
           e->cause.operation == NULL && e->cause.system_error == 0 && empty_tile_children(e) &&
           e->tile_entity.base_save_bytecode == 0 && e->tile_entity.save_bytecode == 0 &&
           e->tile_entity.item_save_bytecode == 0;
}

static bool fresh_save(const mc_world_game_save_error *e) {
    return e->status == MC_WORLD_STORAGE_OK && e->cause.status == MC_WORLD_STORAGE_OK &&
           e->cause.operation == NULL && e->cause.system_error == 0 && e->region_bytecode == 0 &&
           e->serializer_bytecode == 0 && e->entity_slice == SIZE_MAX &&
           e->entity_index == SIZE_MAX && fresh_live_save(&e->tile_entity) &&
           e->composition.status == MC_STORED_CHUNK_OK && e->composition.field == NULL &&
           e->composition.nbt.status == MC_NBT_OK && e->composition.nbt.offset == 0 &&
           e->encoding.status == MC_NBT_OK && e->encoding.offset == 0;
}

static bool fresh_save_codecs(const mc_world_game_save_error *e) {
    return e->composition.status == MC_STORED_CHUNK_OK && e->composition.field == NULL &&
           e->composition.nbt.status == MC_NBT_OK && e->composition.nbt.offset == 0 &&
           e->encoding.status == MC_NBT_OK && e->encoding.offset == 0;
}

static bool fresh_lifecycle(const mc_world_runtime_lifecycle_error *e) {
    return e->status == MC_CHUNK_LIFECYCLE_OK && e->cause.status == MC_WORLD_STORAGE_OK &&
           e->cause.operation == NULL && e->cause.system_error == 0 && fresh_save(&e->save);
}

static bool seed_saving(fixture *f, size_t slot) {
    bool ok = false;
    fixture_record *r = calloc(1, sizeof(*r));
    tile_entity_record tile_entities = {0};
    mc_nbt_tag item = {.type = MC_NBT_COMPOUND}, root_extra = {.type = MC_NBT_LONG},
               level_extra = {.type = MC_NBT_LONG};
    const mc_nbt_tag *item_pointer = &item, *roots[2], *levels[12];
    uint16_t extra_names[2][16];
    uint8_t *encoded = NULL;
    size_t length = 0, written = 0;
    const char *const names[] = {
        "xPos",       "zPos",      "LastUpdate",       "Blocks",   "Data",        "SkyLight",
        "BlockLight", "HeightMap", "TerrainPopulated", "Entities", "TileEntities"};
    const mc_nbt_type types[] = {MC_NBT_INT,        MC_NBT_INT,        MC_NBT_LONG,
                                 MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY,
                                 MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY, MC_NBT_BYTE,
                                 MC_NBT_LIST,       MC_NBT_LIST};
    const size_t lengths[] = {MC_CHUNK_BLOCKS, MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_NIBBLE_BYTES,
                              MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_HEIGHTMAP_BYTES};
    CHECK(r != NULL);
    prepare_tiles(&tile_entities);
    item.value.compound.items = tile_entities.item_fields;
    item.value.compound.length = COUNT(tile_entities.item_fields);
    tile_entities.chest[4].value.list.items = &item_pointer;
    tile_entities.records[0] = &tile_entities.tile_entities[0];
    tile_entities.records[1] = &tile_entities.tile_entities[1];
    root_extra.name = ascii("RootExtra", extra_names[0]);
    root_extra.value.long_value = 901 + (int64_t)slot;
    level_extra.name = ascii("LevelExtra", extra_names[1]);
    level_extra.value.long_value = 902 + (int64_t)slot;
    r->root.type = r->level.type = MC_NBT_COMPOUND;
    r->level.name = ascii("Level", r->names[11]);
    for (size_t i = 0; i < COUNT(names); ++i) {
        r->fields[i].type = types[i];
        r->fields[i].name = ascii(names[i], r->names[i]);
        levels[i + 1] = &r->fields[i];
    }
    for (size_t i = 0; i < COUNT(lengths); ++i) {
        r->fields[i + 3].value.byte_array.data = r->arrays[i];
        r->fields[i + 3].value.byte_array.length = lengths[i];
    }
    if (slot >= 2) {
        r->arrays[0][cell(8, 64, 8)] = (uint8_t)f->chest_id[slot % 2];
        r->arrays[0][cell(9, 64, 8)] = (uint8_t)f->spawner_id[slot % 2];
    }
    r->fields[8].value.byte_value = 1;
    r->fields[9].value.list.element_type = r->fields[10].value.list.element_type = MC_NBT_COMPOUND;
    r->fields[10].value.list.items = slot < 2 ? NULL : tile_entities.records;
    r->fields[10].value.list.length = slot < 2 ? 0 : 2;
    levels[0] = &level_extra;
    r->level.value.compound.items = levels;
    r->level.value.compound.length = COUNT(levels);
    roots[0] = &root_extra;
    roots[1] = &r->level;
    r->root.value.compound.items = roots;
    r->root.value.compound.length = COUNT(roots);
    CHECK(mc_nbt_measure(&r->root, NULL, &length, NULL) == MC_NBT_OK);
    encoded = malloc(length);
    CHECK(encoded != NULL &&
          mc_nbt_write(&r->root, NULL, encoded, length, &written, NULL) == MC_NBT_OK &&
          written == length);
    uint32_t growth = 0;
    CHECK(mc_world_regions_write(f->regions, mc_world_store_root(f->stores[slot]), 0, 0, encoded,
                                 length, fixture_timestamp, &growth, NULL) == MC_WORLD_STORAGE_OK &&
          baseline(f, slot));
    ok = true;
cleanup:
    free(encoded);
    free(r);
    return ok;
}

static bool load_saving(fixture *f, size_t slot) {
    bool ok = false;
    mc_chunk_state *loaded = NULL;
    const mc_chunk_acquire_status status = mc_world_runtime_get(f->worlds[slot], 0, 0, &loaded);
    if (f->providers[slot].cache->insertion_count != 0)
        f->states[slot] = f->providers[slot].cache->insertions[0];
    CHECK(status == MC_CHUNK_ACQUIRE_OK && loaded == f->states[slot] && loaded != NULL &&
          loaded->active);
    f->chunks[slot] = mc_world_store_chunk_object(f->stores[slot], loaded);
    CHECK(f->chunks[slot] != NULL);
    if (slot < 2)
        CHECK(install_primary(f, slot));
    else
        CHECK(mc_live_tile_entities_get(f->worlds[slot], 8, 64, 8, &f->aliases[slot], NULL) ==
                  MC_WORLD_STORAGE_OK &&
              f->aliases[slot] != NULL);
    ok = true;
cleanup:
    return ok;
}

static bool decoded_save(fixture *f, size_t slot) {
    bool ok = false;
    mc_compression_buffer bytes = {0};
    mc_nbt_document *doc = NULL;
    size_t consumed = 0;
    CHECK(mc_world_regions_read(f->regions, mc_world_store_root(f->stores[slot]), 0, 0, &bytes,
                                NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_nbt_read(bytes.data, bytes.length, NULL, &doc, &consumed, NULL) == MC_NBT_OK &&
          consumed == bytes.length);
    const mc_nbt_tag *root = mc_nbt_document_root(doc), *level = field(root, "Level");
    const mc_nbt_tag *root_extra = field(root, "RootExtra"),
                     *level_extra = field(level, "LevelExtra"),
                     *update = field(level, "LastUpdate");
    CHECK(root != NULL && root->value.compound.length == 2 && level != NULL && root_extra != NULL &&
          root_extra->type == MC_NBT_LONG && root_extra->value.long_value == 901 + (int64_t)slot &&
          level_extra != NULL && level_extra->type == MC_NBT_LONG &&
          level_extra->value.long_value == 902 + (int64_t)slot &&
          root->value.compound.items[0] == root_extra &&
          level->value.compound.items[0] == level_extra);
    int64_t now = 0;
    CHECK(mc_world_info_time_get(f->worlds[slot]->fields.x, &now) == MC_WORLD_INFO_OK &&
          update != NULL && update->type == MC_NBT_LONG && update->value.long_value == now);
    mc_world_chunk *chunk = f->chunks[slot];
    const char *const array_names[] = {"Blocks", "Data", "SkyLight", "BlockLight", "HeightMap"};
    const uint8_t *const arrays[] = {chunk->data->blocks, chunk->e->a, chunk->f->a, chunk->g->a,
                                     chunk->h};
    const size_t lengths[] = {MC_CHUNK_BLOCKS, MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_NIBBLE_BYTES,
                              MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_HEIGHTMAP_BYTES};
    for (size_t i = 0; i < COUNT(lengths); ++i) {
        const mc_nbt_tag *array = field(level, array_names[i]);
        CHECK(array != NULL && array->type == MC_NBT_BYTE_ARRAY &&
              array->value.byte_array.length == lengths[i] &&
              memcmp(array->value.byte_array.data, arrays[i], lengths[i]) == 0);
    }
    const mc_nbt_tag *entities_tag = field(level, "Entities"),
                     *tile_entities = field(level, "TileEntities");
    CHECK(entities_tag != NULL && entities_tag->type == MC_NBT_LIST &&
          entities_tag->value.list.length == 0 && tile_entities != NULL &&
          tile_entities->type == MC_NBT_LIST &&
          tile_entities->value.list.length == chunk->l->count);
    for (size_t i = 0; i < tile_entities->value.list.length; ++i) {
        mc_tile_entity *tile_entity = NULL;
        mc_tile_entity_snapshot state;
        CHECK(mc_live_tile_entities_map_at(chunk, i, &tile_entity) == MC_WORLD_STORAGE_OK &&
              mc_tile_entity_get_snapshot(tile_entity, &state) == MC_TILE_ENTITY_OK);
        const mc_nbt_tag *record = tile_entities->value.list.items[i];
        const mc_nbt_tag *x = field(record, "x"), *y = field(record, "y"), *z = field(record, "z");
        CHECK(x != NULL && y != NULL && z != NULL && x->type == MC_NBT_INT &&
              y->type == MC_NBT_INT && z->type == MC_NBT_INT && x->value.int_value == state.x &&
              y->value.int_value == state.y && z->value.int_value == state.z);
        const mc_nbt_tag *source_extra = field(mc_tile_entity_source(tile_entity), "TileExtra");
        if (source_extra != NULL)
            CHECK(field(record, "TileExtra") != NULL &&
                  field(record, "TileExtra")->value.int_value == source_extra->value.int_value);
        if (state.kind == MC_TILE_ENTITY_SPAWNER) {
            const mc_nbt_tag *id = field(record, "EntityId"), *delay = field(record, "Delay");
            mc_tile_entity_name_view view;
            CHECK(mc_tile_entity_name_get_view(state.spawner_name, &view) == MC_TILE_ENTITY_OK &&
                  !view.is_null && id != NULL && id->type == MC_NBT_STRING &&
                  id->value.string.length == view.text.length &&
                  memcmp(id->value.string.units, view.text.units,
                         view.text.length * sizeof(uint16_t)) == 0 &&
                  delay != NULL && delay->type == MC_NBT_SHORT &&
                  delay->value.short_value == state.spawner_delay);
        } else {
            const mc_nbt_tag *items = field(record, "Items");
            size_t occupied = 0;
            CHECK(items != NULL && items->type == MC_NBT_LIST);
            for (size_t index = 0; index < state.chest_capacity; ++index) {
                mc_item_stack *stack = NULL;
                const mc_tile_entity_status got =
                    mc_tile_entity_chest_get(tile_entity, (int32_t)index, &stack);
                if (got != MC_TILE_ENTITY_OK)
                    CHECK(false);
                if (stack == NULL)
                    continue;
                mc_item_stack_fields actual;
                const mc_item_stack_status inspected = mc_item_stack_fields_get(stack, &actual);
                const mc_nbt_tag *unknown = field(mc_item_stack_source(stack), "ItemExtra");
                const bool had_unknown = unknown != NULL;
                const int64_t extra_value = had_unknown ? unknown->value.long_value : 0;
                mc_item_stack_release(stack);
                CHECK(inspected == MC_ITEM_STACK_OK && occupied < items->value.list.length);
                const mc_nbt_tag *saved = items->value.list.items[occupied++];
                const mc_nbt_tag *saved_slot = field(saved, "Slot"), *saved_id = field(saved, "id"),
                                 *saved_count = field(saved, "Count"),
                                 *saved_damage = field(saved, "Damage");
                CHECK(saved_slot != NULL && saved_id != NULL && saved_count != NULL &&
                      saved_damage != NULL && saved_slot->type == MC_NBT_BYTE &&
                      saved_id->type == MC_NBT_SHORT && saved_count->type == MC_NBT_BYTE &&
                      saved_damage->type == MC_NBT_SHORT &&
                      saved_slot->value.byte_value == (int8_t)index &&
                      saved_id->value.short_value == mc_java_i16_narrow(actual.id) &&
                      saved_count->value.byte_value == mc_java_i8_narrow(actual.count) &&
                      saved_damage->value.short_value == mc_java_i16_narrow(actual.damage));
                if (had_unknown)
                    CHECK(field(saved, "ItemExtra") != NULL &&
                          field(saved, "ItemExtra")->value.long_value == extra_value);
            }
            CHECK(occupied == items->value.list.length);
        }
    }
    ok = true;
cleanup:
    mc_nbt_document_free(doc);
    mc_compression_buffer_free(&bytes);
    return ok;
}

static bool write_success(fixture *f, size_t slot, uint32_t stamp, bool grows) {
    bool ok = false;
    save_snapshot before = {0}, after = {0};
    mc_world_game_save_error error;
    uint32_t growth = UINT32_MAX;
    CHECK(snapshot_take(f, slot, &before));
    /* A real independent outer lease is balanced separately from this save. */
    CHECK(mc_world_store_lease_chunk(f->stores[slot], f->states[slot], NULL) ==
          MC_WORLD_STORAGE_OK);
    const mc_world_storage_status saved =
        mc_world_runtime_write_game_chunk(f->worlds[slot], f->states[slot], stamp, &growth, &error);
    const mc_world_storage_status released =
        mc_world_store_release_chunk(f->stores[slot], f->states[slot], NULL);
    CHECK(saved == MC_WORLD_STORAGE_OK && released == MC_WORLD_STORAGE_OK && growth != UINT32_MAX &&
          fresh_save(&error) && !f->states[slot]->has_entities && snapshot_take(f, slot, &after));
    CHECK(after.region.length >= before.region.length &&
          growth == after.region.length - before.region.length && (!grows || growth != 0) &&
          after.size ==
              mc_java_i64_add(before.size, (int64_t)mc_java_i32_narrow((int64_t)growth)) &&
          after.convenience_size == before.convenience_size &&
          after.level.length == before.level.length &&
          memcmp(after.level.data, before.level.data, before.level.length) == 0 &&
          after.region.length >= 8192 &&
          (((uint32_t)after.region.data[4096] << 24u) | ((uint32_t)after.region.data[4097] << 16u) |
           ((uint32_t)after.region.data[4098] << 8u) | after.region.data[4099]) == stamp &&
          decoded_save(f, slot));
    ok = true;
cleanup:
    snapshot_free(&before);
    snapshot_free(&after);
    return ok;
}

static bool edit_live(fixture *f, size_t slot) {
    bool ok = false, changed = false;
    mc_item_stack *stack = NULL;
    mc_tile_entity *spawner = NULL;
    mc_tile_entity_name *name = NULL;
    mc_world_mutation_error mutation;
    mc_live_tile_entity_error detail;
    mc_tile_entity_snapshot chest;
    CHECK(mc_tile_entity_get_snapshot(f->aliases[slot], &chest) == MC_TILE_ENTITY_OK &&
          chest.chest_capacity == (slot < 2 ? 36u : 27u));
    if (slot >= 2)
        CHECK(mc_tile_entity_chest_get(f->aliases[slot], 0, &stack) == MC_TILE_ENTITY_OK &&
              stack != NULL);
    else
        CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
              mc_item_stack_construct(stack, 262, 99, 7) == MC_ITEM_STACK_OK);
    CHECK(mc_item_stack_count_set(stack, 99) == MC_ITEM_STACK_OK &&
          mc_tile_entity_chest_set(f->aliases[slot], slot < 2 ? 0 : 1, stack) == MC_TILE_ENTITY_OK);
    if (slot < 2)
        CHECK(mc_tile_entity_chest_set(f->aliases[slot], 35, stack) == MC_TILE_ENTITY_OK);
    else
        CHECK(mc_tile_entity_chest_set(f->aliases[slot], 0, NULL) == MC_TILE_ENTITY_OK);
    mc_item_stack_fields clamped;
    CHECK(mc_item_stack_fields_get(stack, &clamped) == MC_ITEM_STACK_OK && clamped.count == 64);
    CHECK(mc_world_runtime_block_set_quiet(f->worlds[slot], 12, 64, 8, f->chest_id[slot % 2],
                                           &changed, &mutation) == MC_WORLD_STORAGE_OK &&
          changed &&
          mc_live_tile_entities_set(f->worlds[slot], 12, 64, 8, f->aliases[slot], &detail) ==
              MC_WORLD_STORAGE_OK);
    CHECK(mc_live_tile_entities_get(f->worlds[slot], 9, 64, 8, &spawner, &detail) ==
              MC_WORLD_STORAGE_OK &&
          spawner != NULL &&
          mc_live_tile_entities_literal(f->worlds[slot], "Spider", &name, &detail) ==
              MC_WORLD_STORAGE_OK &&
          mc_tile_entity_spawner_set_name(spawner, name) == MC_TILE_ENTITY_OK);
    /* Deterministic current source nibble bytes force actual compressed growth.
     * This is a labeled fixture input, not a copied voxel oracle or RNG state. */
    uint32_t bits = UINT32_C(0x8c1430a7) + (uint32_t)slot;
    for (size_t i = 0; i < MC_CHUNK_NIBBLE_BYTES; ++i) {
        bits ^= bits << 13u;
        bits ^= bits >> 17u;
        bits ^= bits << 5u;
        f->chunks[slot]->e->a[i] = (uint8_t)bits;
    }
    CHECK(mc_world_info_time_set(f->worlds[slot]->fields.x, 10000 + (int64_t)slot) ==
              MC_WORLD_INFO_OK &&
          write_success(f, slot, UINT32_C(0x51000000) + (uint32_t)slot, true) &&
          write_success(f, slot, UINT32_C(0x52000000) + (uint32_t)slot, false));
    ok = true;
cleanup:
    mc_item_stack_release(stack);
    mc_tile_entity_release(spawner);
    mc_tile_entity_name_release(name);
    return ok;
}

static bool set_q_by_source(fixture *f, size_t slot) {
    mc_world_entity_error error;
    if (mc_world_runtime_chunk_add_entity(f->chunks[slot], NULL, &error) == MC_WORLD_STORAGE_OK ||
        !f->states[slot]->has_entities)
        return false;
    for (size_t i = 0; i < CHUNK_ENTITY_SLICES; ++i) {
        size_t count = SIZE_MAX;
        if (mc_world_chunk_entity_count(&f->chunks[slot]->entities[i], &count) !=
                MC_WORLD_STORAGE_OK ||
            count != 0)
            return false;
    }
    return true;
}

static bool failure_prefixes(fixture *f, size_t slot) {
    bool ok = false;
    mc_world_chunk *chunk = f->chunks[slot];
    save_snapshot before = {0};
    mc_ws_file_bytes session = {0};
    char *session_path = NULL;
    mc_world_game_save_error error;
    uint32_t growth = UINT32_MAX;
    CHECK(snapshot_take(f, slot, &before) && set_q_by_source(f, slot) &&
          mc_ws_path_join(mc_world_store_root(f->stores[slot]), "session.lock", &session_path,
                          NULL) == MC_WORLD_STORAGE_OK &&
          mc_ws_read_file(session_path, MC_WS_FILE_MAX_BYTES, &session, NULL) ==
              MC_WORLD_STORAGE_OK);
    const uint8_t wrong_token[8] = {0, 0, 0, 0, 0, 0, 0, 138};
    CHECK(mc_ws_write_file(session_path, wrong_token, sizeof(wrong_token), false, NULL) ==
          MC_WORLD_STORAGE_OK);
    const mc_world_storage_status bad_session = mc_world_runtime_write_game_chunk(
        f->worlds[slot], f->states[slot], direct_stamp(slot), &growth, &error);
    const mc_world_storage_status restored =
        mc_ws_write_file(session_path, session.data, session.length, false, NULL);
    CHECK(restored == MC_WORLD_STORAGE_OK && bad_session == MC_WORLD_STORAGE_SESSION_MISMATCH &&
          error.status == bad_session && error.cause.status == bad_session &&
          fresh_save_codecs(&error) && error.region_bytecode == 1 &&
          error.serializer_bytecode == 0 && error.entity_slice == SIZE_MAX &&
          error.entity_index == SIZE_MAX && fresh_live_save(&error.tile_entity) &&
          f->states[slot]->has_entities && growth == UINT32_MAX && snapshot_same(f, slot, &before));
    for (unsigned int control = 0; control < 5; ++control) {
        CHECK(set_q_by_source(f, slot));
        mc_world_chunk_nibble *saved_e = chunk->e;
        uint8_t *saved_a = saved_e->a, *saved_h = chunk->h;
        mc_world_chunk_list_array *saved_m = chunk->m;
        mc_world_chunk_tile_entity_map *saved_l = chunk->l;
        if (control == 0)
            chunk->e = NULL;
        if (control == 1)
            saved_e->a = NULL;
        if (control == 2)
            chunk->h = NULL;
        if (control == 3)
            chunk->m = NULL;
        if (control == 4)
            chunk->l = NULL;
        growth = UINT32_MAX;
        const mc_world_storage_status status = mc_world_runtime_write_game_chunk(
            f->worlds[slot], f->states[slot], direct_stamp(slot), &growth, &error);
        chunk->e = saved_e;
        saved_e->a = saved_a;
        chunk->h = saved_h;
        chunk->m = saved_m;
        chunk->l = saved_l;
        CHECK(status != MC_WORLD_STORAGE_OK && error.status == status &&
              error.cause.status == status && growth == UINT32_MAX &&
              f->states[slot]->has_entities == (control == 0) && error.entity_slice == SIZE_MAX &&
              error.entity_index == SIZE_MAX && fresh_live_save(&error.tile_entity) &&
              snapshot_same(f, slot, &before));
        if (control == 1 || control == 2)
            CHECK(error.region_bytecode == 57 && error.serializer_bytecode == 0 &&
                  error.composition.status != MC_STORED_CHUNK_OK &&
                  error.composition.nbt.status != MC_NBT_OK && error.encoding.status == MC_NBT_OK &&
                  error.encoding.offset == 0);
        else
            CHECK(error.region_bytecode == 51 &&
                  error.serializer_bytecode == (control == 0   ? 51u
                                                : control == 3 ? 125u
                                                               : 224u) &&
                  error.composition.status == MC_STORED_CHUNK_OK &&
                  error.composition.field == NULL && error.composition.nbt.status == MC_NBT_OK &&
                  error.composition.nbt.offset == 0 && error.encoding.status == MC_NBT_OK &&
                  error.encoding.offset == 0);
    }
    ok = true;
cleanup:
    free(session_path);
    mc_ws_free_file_bytes(&session);
    snapshot_free(&before);
    return ok;
}

static bool spawner_and_lifecycle(fixture *f, size_t slot) {
    bool ok = false;
    mc_tile_entity *spawner = NULL;
    mc_tile_entity_name *old = NULL;
    save_snapshot before = {0};
    mc_world_game_save_error error;
    mc_world_runtime_lifecycle_error lifecycle;
    uint32_t growth = UINT32_MAX;
    CHECK(mc_live_tile_entities_get(f->worlds[slot], 9, 64, 8, &spawner, NULL) ==
              MC_WORLD_STORAGE_OK &&
          spawner != NULL && mc_tile_entity_spawner_get_name(spawner, &old) == MC_TILE_ENTITY_OK &&
          snapshot_take(f, slot, &before) && set_q_by_source(f, slot) &&
          mc_tile_entity_spawner_set_name(spawner, NULL) == MC_TILE_ENTITY_OK);
    const mc_world_storage_status failed = mc_world_runtime_write_game_chunk(
        f->worlds[slot], f->states[slot], direct_stamp(slot), &growth, &error);
    const mc_tile_entity_status restored = mc_tile_entity_spawner_set_name(spawner, old);
    CHECK(restored == MC_TILE_ENTITY_OK && failed != MC_WORLD_STORAGE_OK && growth == UINT32_MAX &&
          error.status == failed && error.cause.status == failed && fresh_save_codecs(&error) &&
          error.entity_slice == SIZE_MAX && error.entity_index == SIZE_MAX &&
          !f->states[slot]->has_entities && error.region_bytecode == 51 &&
          error.serializer_bytecode == 271 && error.tile_entity.bytecode == 271 &&
          error.tile_entity.tile_entity.save_bytecode == 12 &&
          error.tile_entity.tile_entity.base_save_bytecode == 0 &&
          error.tile_entity.tile_entity.item_save_bytecode == 0 &&
          error.tile_entity.tile_entity.status == MC_TILE_ENTITY_INVALID_ARGUMENT &&
          error.tile_entity.tile_entity.field != NULL &&
          strcmp(error.tile_entity.tile_entity.field, "EntityId") == 0 &&
          error.tile_entity.tile_entity.item_index == SIZE_MAX &&
          fresh_registry(&error.tile_entity.tile_entity.registry, MC_REGISTRY_OK) &&
          snapshot_same(f, slot, &before));
    CHECK(mc_world_info_time_set(f->worlds[slot]->fields.x, 20000 + (int64_t)slot) ==
              MC_WORLD_INFO_OK &&
          mc_tile_entity_spawner_set_name(spawner, NULL) == MC_TILE_ENTITY_OK);
    bool completed = true;
    const mc_chunk_lifecycle_status failed_pass =
        mc_world_runtime_save_chunks_detailed(f->worlds[slot], true, &completed, &lifecycle);
    const mc_tile_entity_status restored_pass = mc_tile_entity_spawner_set_name(spawner, old);
    CHECK(restored_pass == MC_TILE_ENTITY_OK && failed_pass != MC_CHUNK_LIFECYCLE_OK && completed &&
          lifecycle.status == failed_pass && lifecycle.save.serializer_bytecode == 271 &&
          lifecycle.save.tile_entity.tile_entity.save_bytecode == 12 &&
          fresh_save_codecs(&lifecycle.save) && lifecycle.save.entity_slice == SIZE_MAX &&
          lifecycle.save.entity_index == SIZE_MAX &&
          f->states[slot]->last_save_time == 20000 + (int64_t)slot &&
          snapshot_same(f, slot, &before));
    CHECK(mc_world_runtime_save_chunks_detailed(f->worlds[slot], true, &completed, &lifecycle) ==
              MC_CHUNK_LIFECYCLE_OK &&
          completed && fresh_lifecycle(&lifecycle) && !f->states[slot]->dirty &&
          decoded_save(f, slot));
    ok = true;
cleanup:
    mc_tile_entity_release(spawner);
    mc_tile_entity_name_release(old);
    snapshot_free(&before);
    return ok;
}

static bool late_depth(fixture *f, size_t slot) {
    bool ok = false, changed = false;

    /* Default depth is 64: TileEntity result reaches 64, outer Level reaches 65.
     * Import alone reaches 62, so this input must fail only at composition. */
    enum { DEPTH_NODES = 61 };

    mc_nbt_tag deep[DEPTH_NODES] = {0}, values[5] = {0}, root = {.type = MC_NBT_COMPOUND};
    const mc_nbt_tag *links[DEPTH_NODES], *fields[5];
    uint16_t names[6][12], chest_name[5];
    mc_tile_entity *tile_entity = NULL;
    save_snapshot before = {0};
    mc_world_mutation_error mutation;
    mc_live_tile_entity_error tile_entity_error;
    const char *const labels[] = {"id", "x", "y", "z", "Deep"};
    for (size_t i = 0; i < COUNT(values); ++i) {
        values[i].name = ascii(labels[i], names[i]);
        values[i].type = i == 0 ? MC_NBT_STRING : MC_NBT_INT;
        fields[i] = &values[i];
    }
    values[0].value.string = ascii("Chest", chest_name);
    values[1].value.int_value = 13;
    values[2].value.int_value = 64;
    values[3].value.int_value = 8;
    for (size_t i = 0; i < DEPTH_NODES; ++i) {
        deep[i].name = ascii("Deep", names[5]);
        deep[i].type = i + 1 == DEPTH_NODES ? MC_NBT_BYTE : MC_NBT_COMPOUND;
        if (i + 1 != DEPTH_NODES) {
            links[i] = &deep[i + 1];
            deep[i].value.compound.items = &links[i];
            deep[i].value.compound.length = 1;
        }
    }
    values[4] = deep[0];
    root.value.compound.items = fields;
    root.value.compound.length = COUNT(fields);
    CHECK(mc_live_tile_entities_factory(f->worlds[slot], &root, NULL, true, &tile_entity,
                                        &tile_entity_error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_block_set_quiet(f->worlds[slot], 13, 64, 8, f->chest_id[slot % 2],
                                           &changed, &mutation) == MC_WORLD_STORAGE_OK &&
          changed);
    CHECK(mc_live_tile_entities_set(f->worlds[slot], 13, 64, 8, tile_entity, &tile_entity_error) ==
          MC_WORLD_STORAGE_OK);
    CHECK(set_q_by_source(f, slot) && snapshot_take(f, slot, &before));
    uint32_t growth = UINT32_MAX;
    mc_world_game_save_error error;
    const mc_world_storage_status status = mc_world_runtime_write_game_chunk(
        f->worlds[slot], f->states[slot], direct_stamp(slot), &growth, &error);
    CHECK(status == MC_WORLD_STORAGE_LIMIT && growth == UINT32_MAX &&
          !f->states[slot]->has_entities && error.region_bytecode == 57 &&
          error.serializer_bytecode == 0 && error.composition.status == MC_STORED_CHUNK_LIMIT &&
          error.composition.nbt.status == MC_NBT_LIMIT && error.encoding.status == MC_NBT_OK &&
          fresh_live_save(&error.tile_entity) && snapshot_same(f, slot, &before));
    CHECK(mc_live_tile_entities_remove(f->worlds[slot], 13, 64, 8, &tile_entity_error) ==
              MC_WORLD_STORAGE_OK &&
          write_success(f, slot, direct_stamp(slot), false));
    ok = true;
cleanup:
    mc_tile_entity_release(tile_entity);
    snapshot_free(&before);
    return ok;
}

static bool reload_same_world(fixture *f, size_t slot, int32_t chest_x, bool empty) {
    bool ok = false;
    if (slot % 2 == 0)
        return true; /* No invented client/base unload route. */
    mc_world_runtime *const continuing = f->worlds[slot];
    mc_world_runtime_lifecycle_error detail;
    bool provider_result = true;
    /* Both installed GAME constructors bind BASE policy. Exercise the public
     * dedicated coordinator with explicit native descriptor inputs and the
     * actual runtime callbacks, owners, references and collections. Never
     * mutate the World policy or claim a natural dedicated-World constructor. */
    mc_chunk_acquire_context drop = continuing->acquisition;
    mc_chunk_lifecycle_context unload = continuing->lifecycle;
    drop.policy = MC_CHUNK_ACQUIRE_DEDICATED;
    unload.policy = MC_CHUNK_ACQUIRE_DEDICATED;
    CHECK(mc_world_info_spawn_set(continuing->fields.x, 100000, 64, 100000) == MC_WORLD_INFO_OK &&
          mc_chunk_acquire_mark_drop(&drop, 0, 0) == MC_CHUNK_ACQUIRE_OK &&
          mc_chunk_pending_contains(f->providers[slot].pending, mc_chunk_cache_key(0, 0)));
    CHECK(mc_chunk_lifecycle_maintain(&unload, &provider_result) == MC_CHUNK_LIFECYCLE_OK &&
          !f->states[slot]->active && f->providers[slot].cache->count == 0 &&
          f->providers[slot].cache->insertion_count == 0 && f->providers[slot].pending->count == 0);
    CHECK(mc_world_runtime_maintain_detailed(continuing, &provider_result, &detail) ==
              MC_CHUNK_LIFECYCLE_OK &&
          fresh_lifecycle(&detail));
    mc_tile_entity_snapshot old;
    CHECK(mc_tile_entity_get_snapshot(f->aliases[slot], &old) == MC_TILE_ENTITY_OK && old.invalid &&
          old.world == continuing);
    CHECK(mc_world_store_retire_chunk(f->stores[slot], f->states[slot], NULL) ==
          MC_WORLD_STORAGE_OK);
    f->states[slot] = NULL;
    f->chunks[slot] = NULL;
    mc_chunk_state *loaded = NULL;
    const mc_chunk_acquire_status acquired = mc_world_runtime_get(continuing, 0, 0, &loaded);
    if (f->providers[slot].cache->insertion_count != 0)
        f->states[slot] = f->providers[slot].cache->insertions[0];
    CHECK(acquired == MC_CHUNK_ACQUIRE_OK && loaded != NULL && f->states[slot] == loaded);
    f->chunks[slot] = mc_world_store_chunk_object(f->stores[slot], loaded);
    CHECK(f->worlds[slot] == continuing && f->chunks[slot] != NULL && loaded->active &&
          continuing->construction.phase == MC_WORLD_RUNTIME_READY);
    if (empty)
        CHECK(f->chunks[slot]->l->count == 0);
    else {
        mc_tile_entity *current = NULL;
        const mc_world_storage_status got =
            mc_live_tile_entities_get(continuing, chest_x, 64, 8, &current, NULL);
        mc_tile_entity_snapshot state = {0};
        const mc_tile_entity_status inspected = current == NULL
                                                    ? MC_TILE_ENTITY_INVALID_ARGUMENT
                                                    : mc_tile_entity_get_snapshot(current, &state);
        const bool distinct = current != f->aliases[slot];
        mc_item_stack *carried = NULL;
        mc_item_stack_fields fields = {0};
        const mc_tile_entity_status carried_status =
            mc_tile_entity_chest_get(current, slot < 2 ? 0 : 1, &carried);
        const mc_item_stack_status carried_fields = mc_item_stack_fields_get(carried, &fields);
        mc_item_stack *unchanged = carried;
        const mc_tile_entity_status high_slot = mc_tile_entity_chest_get(current, 35, &unchanged);
        const bool output_unchanged = unchanged == carried;
        mc_item_stack_release(carried);
        mc_tile_entity_release(current);
        CHECK(got == MC_WORLD_STORAGE_OK && inspected == MC_TILE_ENTITY_OK && distinct &&
              state.chest_capacity == 27 && f->chunks[slot]->l->count == 2 &&
              carried_status == MC_TILE_ENTITY_OK && carried_fields == MC_ITEM_STACK_OK &&
              fields.count == 64 && high_slot == MC_TILE_ENTITY_INVALID_SLOT && output_unchanged);
    }
    /* The prior disk contains both original Map occurrences of one alias,
     * each with its current coordinates. Real import has now replaced that
     * duplicate key. Save the actual two-node Map before comparing its output. */
    CHECK(write_success(f, slot, direct_stamp(slot), false));
    ok = true;
cleanup:
    return ok;
}

static bool empty_records(fixture *f, size_t slot) {
    bool ok = false;
    const int32_t xs[] = {8, 9, 12};
    for (size_t i = 0; i < COUNT(xs); ++i)
        CHECK(mc_live_tile_entities_remove(f->worlds[slot], xs[i], 64, 8, NULL) ==
              MC_WORLD_STORAGE_OK);
    CHECK(f->chunks[slot]->l->count == 0 && f->chunks[slot]->l->backing != NULL &&
          !mc_world_chunk_storage_empty(f->chunks[slot]) &&
          write_success(f, slot, direct_stamp(slot), false) &&
          reload_same_world(f, slot, 12, true) &&
          write_success(f, slot, direct_stamp(slot), false));
    ok = true;
cleanup:
    return ok;
}

static bool entity_boundary(fixture *f, size_t slot) {
    bool ok = false, ordinary = false;
    mc_entity *entity = NULL;
    mc_item_stack *stack = NULL;
    mc_entity_error entity_error;
    mc_world_entity_error publication;
    save_snapshot before = {0};
    CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(stack, 262, 1, 3) == MC_ITEM_STACK_OK &&
          mc_item_entity_allocate(f->entity_classes[slot % 2], &entity, &entity_error) ==
              MC_ENTITY_OK &&
          mc_item_entity_construct(entity, f->worlds[slot], 8.25, 64.0, 8.25, stack,
                                   &entity_error) == MC_ENTITY_OK &&
          mc_world_runtime_insert_entity(f->worlds[slot], entity, &ordinary, &publication) ==
              MC_WORLD_STORAGE_OK &&
          ordinary && snapshot_take(f, slot, &before));
    uint32_t growth = UINT32_MAX;
    mc_world_game_save_error error;
    /* A genuine late writer failure now replaces the former absent Entity
     * boundary. Keep the real Stack hold and restore the nullable source
     * projection before any CHECK or native finalization. */
    mc_item_stack *held_stack = entity->stack;
    entity->stack = NULL;
    const mc_world_storage_status status = mc_world_runtime_write_game_chunk(
        f->worlds[slot], f->states[slot], direct_stamp(slot), &growth, &error);
    entity->stack = held_stack;
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && growth == UINT32_MAX &&
          f->states[slot]->has_entities && error.region_bytecode == 51 &&
          error.serializer_bytecode == 183 && error.entity_slice == 4 && error.entity_index == 0 &&
          error.entity.status == MC_ENTITY_NBT_NULL_REFERENCE &&
          error.entity.wrapper_bytecode == 27 && error.entity.name_bytecode == 7 &&
          error.entity.item_bytecode == 37 && error.entity.stack_bytecode == 0 &&
          fresh_save_codecs(&error) && fresh_live_save(&error.tile_entity) &&
          snapshot_same(f, slot, &before));
    ok = true;
cleanup:
    mc_entity_release(entity);
    mc_item_stack_release(stack);
    snapshot_free(&before);
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    const uint16_t title[] = {'S', 'a', 'v', 'e'};
    if (argc != 3 || strcmp(argv[1], "world-saving") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL);
    f->root = root;
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_materials_create(&f->materials) &&
          mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
              MC_WORLD_INFO_OK);
    for (size_t side = 0; side < 2; ++side) {
        resources *r = &f->sources[side];
        r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1};
        r->streams[1] = r->streams[2] = (stream){.bytes = language};
        r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
        const mc_registry_options options = {side == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                                             f->profile, f->materials, 0, &r->api};
        CHECK(mc_registry_classes_allocate(&options, &f->registry[side], NULL) == MC_REGISTRY_OK &&
              mc_world_runtime_classes_create(
                  side == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER, f->profile, &trig,
                  &f->classes[side], NULL) == MC_WORLD_STORAGE_OK &&
              mc_world_runtime_classes_bind_registry(f->classes[side], f->registry[side], NULL) ==
                  MC_REGISTRY_OK &&
              mc_entity_classes_create(side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile,
                                       NULL, &f->entity_classes[side]) == MC_ENTITY_OK &&
              mc_world_runtime_classes_bind_entities(f->classes[side], f->entity_classes[side]) ==
                  MC_ENTITY_OK &&
              mc_registry_classes_initialize_blocks(f->registry[side], NULL) == MC_REGISTRY_OK &&
              r->opens == 3 && r->closes == 1 && !r->bad);
        const mc_block *chest = NULL, *spawner = NULL;
        CHECK(mc_registry_block_static_get(f->registry[side], "av", &chest, NULL) ==
                  MC_REGISTRY_OK &&
              mc_registry_block_static_get(f->registry[side], "at", &spawner, NULL) ==
                  MC_REGISTRY_OK &&
              chest != NULL && spawner != NULL);
        f->chest_id[side] = mc_block_fields_get(chest)->bn;
        f->spawner_id[side] = mc_block_fields_get(spawner)->bn;
    }
    for (size_t slot = 0; slot < WORLD_COUNT; ++slot)
        CHECK(open_saving_store(f, slot, root) && create_world(f, slot) && seed_saving(f, slot) &&
              load_saving(f, slot));
    for (size_t slot = 0; slot < WORLD_COUNT; ++slot)
        CHECK(edit_live(f, slot) && failure_prefixes(f, slot) && spawner_and_lifecycle(f, slot) &&
              late_depth(f, slot) && reload_same_world(f, slot, 12, false) &&
              empty_records(f, slot));
    for (size_t slot = 2; slot < WORLD_COUNT; ++slot)
        CHECK(entity_boundary(f, slot));
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("saving cleanup refused; surviving real owners retained\n", stderr);
        return 1;
    }
    if (result == 0 &&
        printf("PASS: %u live TileEntity save integration checks (world-saving)\n", checks) < 0)
        result = 1;
    return result;
}
