/* Reuse genuine GAME constructors, resources, transport and joint teardown.
 * The old Tile scenario is compiled under another name, never executed here. */
#define main mc_game_entities_tile_fixture_main
#include "../tiles/test_tiles.c"
#undef main

#include "mc/entity/nbt/storage.h"
#include "mc/world/runtime/saving/saving.h"

/* Constructed native integration contracts, not original/native parity.
 * No phase, ID, RNG, container count, hold or lease is assigned. The temporary
 * dead/NULL-Stack controls change actual source fields and are restored before
 * CHECK; the RAW Stack control transfers one real freshly allocated hold. */
typedef struct ge_record {
    mc_nbt_tag root, fields[8], position[3], motion[3], rotation[2], stack[4];
    const mc_nbt_tag *children[8], *positions[3], *motions[3], *rotations[2], *stack_children[4];
    uint16_t names[12][24], id[20];
} ge_record;

typedef struct ge_disk {
    mc_compression_buffer bytes;
    mc_nbt_document *document;
} ge_disk;

enum { GE_MIXED, GE_BAD_ITEM, GE_BAD_TILES, GE_ONE_ITEM };
enum { GE_ORIGINAL_STACK, GE_REPLACED_STACK, GE_RAW_STACK };

static uint64_t ge_double_bits(double value)
{
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool ge_text(mc_nbt_string value, const char *text)
{
    const size_t length = strlen(text);
    if (value.length != length || (length != 0 && value.units == NULL))
        return false;
    for (size_t i = 0; i < length; ++i)
        if (value.units[i] != (unsigned char)text[i])
            return false;
    return true;
}

static void ge_prepare(ge_record *r, int64_t marker, bool unknown, bool bad_count, bool item_extra)
{
    memset(r, 0, sizeof(*r));
    r->root.type = MC_NBT_COMPOUND;
    r->root.value.compound.items = r->children;
    r->root.value.compound.length = unknown ? 2 : COUNT(r->fields);
    for (size_t i = 0; i < COUNT(r->fields); ++i)
        r->children[i] = &r->fields[i];
    r->fields[0] = (mc_nbt_tag){.type = MC_NBT_STRING,
                                .name = ascii("id", r->names[0]),
                                .value.string = ascii(unknown ? "NativeUnknown" : "Item", r->id)};
    if (unknown) {
        r->fields[1] = (mc_nbt_tag){.type = MC_NBT_LONG,
                                    .name = ascii("UnknownExtra", r->names[1]),
                                    .value.long_value = marker};
        return;
    }
    r->fields[1] = (mc_nbt_tag){.type = MC_NBT_LIST,
                                .name = ascii("Pos", r->names[1]),
                                .value.list = {MC_NBT_DOUBLE, r->positions, 3}};
    /* Encoded input is A then B; real ascending-slice save order is B then A. */
    const double position[] = {marker == 101 ? 8.25 : 9.25, marker == 101 ? 64.0 : 16.0, 8.25};
    for (size_t i = 0; i < COUNT(position); ++i) {
        r->position[i].type = MC_NBT_DOUBLE;
        r->position[i].value.double_bits = ge_double_bits(position[i]);
        r->positions[i] = &r->position[i];
    }
    r->fields[2] = (mc_nbt_tag){
        .type = MC_NBT_SHORT, .name = ascii("Health", r->names[2]), .value.short_value = 7};
    r->fields[3] = (mc_nbt_tag){
        .type = MC_NBT_SHORT, .name = ascii("Age", r->names[3]), .value.short_value = 19};
    r->fields[4] = (mc_nbt_tag){.type = MC_NBT_COMPOUND,
                                .name = ascii("Item", r->names[4]),
                                .value.compound = {r->stack_children, item_extra ? 4 : 3}};
    r->fields[5] = (mc_nbt_tag){
        .type = MC_NBT_LONG, .name = ascii("EntityExtra", r->names[5]), .value.long_value = marker};
    r->fields[6] = (mc_nbt_tag){.type = MC_NBT_LIST,
                                .name = ascii("Motion", r->names[10]),
                                .value.list = {MC_NBT_DOUBLE, r->motions, 3}};
    r->fields[7] = (mc_nbt_tag){.type = MC_NBT_LIST,
                                .name = ascii("Rotation", r->names[11]),
                                .value.list = {MC_NBT_FLOAT, r->rotations, 2}};
    for (size_t i = 0; i < COUNT(r->motion); ++i) {
        r->motion[i].type = MC_NBT_DOUBLE;
        r->motions[i] = &r->motion[i];
    }
    for (size_t i = 0; i < COUNT(r->rotation); ++i) {
        r->rotation[i].type = MC_NBT_FLOAT;
        r->rotations[i] = &r->rotation[i];
    }
    const char *const names[] = {"id", "Count", "Damage", "ItemExtra"};
    for (size_t i = 0; i < COUNT(names); ++i) {
        r->stack[i].name = ascii(names[i], r->names[6 + i]);
        r->stack_children[i] = &r->stack[i];
    }
    r->stack[0].type = r->stack[2].type = MC_NBT_SHORT;
    r->stack[0].value.short_value = marker == 101 ? 262 : 263;
    r->stack[1].type = bad_count ? MC_NBT_SHORT : MC_NBT_BYTE;
    if (bad_count)
        r->stack[1].value.short_value = 6;
    else
        r->stack[1].value.byte_value = marker == 101 ? 5 : 6;
    r->stack[2].value.short_value = marker == 101 ? 7 : 8;
    r->stack[3].type = MC_NBT_LONG;
    r->stack[3].value.long_value = marker + 1000;
}

static bool ge_open(fixture *f, size_t slot, const char *root, size_t max_tags,
                    bool normalize_marker)
{
    bool ok = false;
    char path[8192];
    uint8_t map[256];
    const uint16_t title[] = {'E', 'n', 't', 'i', 't', 'i', 'e', 's'};
    const int n = snprintf(path, sizeof(path), "%s/world%zu", root, slot);
    CHECK(n > 0 && (size_t)n < sizeof(path));
    for (size_t i = 0; i < COUNT(map); ++i)
        map[i] = (uint8_t)i;
    if (normalize_marker)
        map[200] = 0;
    mc_world_store_options options = {.session_token = 137,
                                      .block_map = map,
                                      .block_map_length = sizeof(map),
                                      .limits = mc_world_store_default_limits()};
    options.limits.max_loaded_chunks = 1;
    options.limits.nbt.max_tags = max_tags;
    CHECK(mc_world_store_open(path, f->regions, &options, &f->stores[slot], NULL) ==
          MC_WORLD_STORAGE_OK);
    const mc_level_info info = {
        .seed = 23, .spawn_y = 64, .level_name = {title, COUNT(title)}, .version = 19132};
    CHECK(mc_world_store_save_info(f->stores[slot], &info, 123, NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    return ok;
}

static bool ge_setup(fixture **out, const char *root, size_t max_tags, bool normalize_marker)
{
    bool ok = false;
    fixture *f = calloc(1, sizeof(*f));
    *out = f; /* Preserve partial genuine ownership for the common cleanup. */
    CHECK(f != NULL);
    const uint16_t title[] = {'E', 'n', 't', 'i', 't', 'i', 'e', 's'};
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
    }
    for (size_t slot = 0; slot < WORLD_COUNT; ++slot)
        CHECK(ge_open(f, slot, root, max_tags, normalize_marker) && create_world(f, slot));
    ok = true;
cleanup:
    return ok;
}

static bool ge_seed(fixture *f, size_t slot, int scenario)
{
    bool ok = false;
    fixture_record *r = calloc(1, sizeof(*r));
    ge_record records[5];
    const mc_nbt_tag *entities[5], *roots[2], *levels[12];
    mc_nbt_tag root_extra = {.type = MC_NBT_LONG, .value.long_value = 901},
               level_extra = {.type = MC_NBT_LONG, .value.long_value = 902};
    uint16_t extra_names[2][24];
    uint8_t *encoded = NULL;
    size_t length = 0, written = 0, count = 1;
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
    if (scenario == GE_MIXED) {
        count = 5;
        ge_prepare(&records[0], 10, true, false, false);
        ge_prepare(&records[1], 101, false, false, true);
        ge_prepare(&records[2], 20, true, false, false);
        ge_prepare(&records[3], 202, false, false, true);
        ge_prepare(&records[4], 30, true, false, false);
    } else {
        ge_prepare(&records[0], scenario == GE_BAD_TILES ? 10 : 101, scenario == GE_BAD_TILES,
                   false, true);
        if (scenario == GE_BAD_ITEM) {
            count = 3;
            ge_prepare(&records[1], 20, true, false, false);
            ge_prepare(&records[2], 202, false, true, true);
        }
    }
    for (size_t i = 0; i < count; ++i)
        entities[i] = &records[i].root;
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
    r->arrays[0][0] = 200;
    r->fields[8].value.byte_value = 1;
    r->fields[9].value.list.element_type = MC_NBT_COMPOUND;
    r->fields[9].value.list.items = entities;
    r->fields[9].value.list.length = count;
    r->fields[10].value.list.element_type = MC_NBT_COMPOUND;
    if (scenario == GE_BAD_ITEM || scenario == GE_BAD_TILES) {
        r->fields[10].type = MC_NBT_BYTE; /* Later semantic getter, valid transport. */
        r->fields[10].value.byte_value = 1;
    }
    root_extra.name = ascii("RootEntityExtra", extra_names[0]);
    level_extra.name = ascii("LevelEntityExtra", extra_names[1]);
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
    uint32_t growth = UINT32_MAX;
    CHECK(mc_world_regions_write(f->regions, mc_world_store_root(f->stores[slot]), 0, 0, encoded,
                                 length, fixture_timestamp, &growth, NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    free(encoded);
    free(r);
    return ok;
}

static void ge_disk_free(ge_disk *disk)
{
    mc_nbt_document_free(disk->document);
    mc_compression_buffer_free(&disk->bytes);
    memset(disk, 0, sizeof(*disk));
}

static bool ge_read(fixture *f, size_t slot, ge_disk *disk)
{
    size_t consumed = 0;
    return mc_world_regions_read(f->regions, mc_world_store_root(f->stores[slot]), 0, 0,
                                 &disk->bytes, NULL) == MC_WORLD_STORAGE_OK &&
           mc_nbt_read(disk->bytes.data, disk->bytes.length, NULL, &disk->document, &consumed,
                       NULL) == MC_NBT_OK &&
           consumed == disk->bytes.length;
}

static bool ge_same_disk(fixture *f, size_t slot, const ge_disk *before)
{
    ge_disk after = {0};
    const bool read = ge_read(f, slot, &after);
    const bool equal = read && after.bytes.length == before->bytes.length &&
                       memcmp(after.bytes.data, before->bytes.data, before->bytes.length) == 0;
    ge_disk_free(&after);
    return equal;
}

static bool ge_output(fixture *f, size_t slot, const int64_t *markers, size_t count, int mode)
{
    bool ok = false;
    ge_disk disk = {0};
    CHECK(ge_read(f, slot, &disk));
    const mc_nbt_tag *root = mc_nbt_document_root(disk.document), *level = field(root, "Level"),
                     *list = field(level, "Entities"), *tiles = field(level, "TileEntities");
    CHECK(field(root, "RootEntityExtra") != NULL &&
          field(root, "RootEntityExtra")->value.long_value == 901 &&
          field(level, "LevelEntityExtra") != NULL &&
          field(level, "LevelEntityExtra")->value.long_value == 902 && list != NULL &&
          list->type == MC_NBT_LIST && list->value.list.element_type == MC_NBT_COMPOUND &&
          list->value.list.length == count && tiles != NULL && tiles->type == MC_NBT_LIST &&
          tiles->value.list.element_type == MC_NBT_BYTE && tiles->value.list.length == 0);
    for (size_t i = 0; i < count; ++i) {
        const mc_nbt_tag *record = list->value.list.items[i], *id = field(record, "id"),
                         *extra = field(record, markers[i] < 100 ? "UnknownExtra" : "EntityExtra");
        CHECK(record != NULL && record->type == MC_NBT_COMPOUND && id != NULL &&
              id->type == MC_NBT_STRING &&
              ge_text(id->value.string, markers[i] < 100 ? "NativeUnknown" : "Item") &&
              extra != NULL && extra->type == MC_NBT_LONG && extra->value.long_value == markers[i]);
        if (markers[i] < 100)
            continue;
        const mc_nbt_tag *item = field(record, "Item"), *item_id = field(item, "id"),
                         *item_count = field(item, "Count"), *damage = field(item, "Damage"),
                         *unknown = field(item, "ItemExtra");
        const bool first = markers[i] == 101;
        const int32_t expected_id = !first                      ? 263
                                    : mode == GE_RAW_STACK      ? 0
                                    : mode == GE_REPLACED_STACK ? 5
                                                                : 262;
        const int32_t expected_count = !first                      ? 6
                                       : mode == GE_RAW_STACK      ? 0
                                       : mode == GE_REPLACED_STACK ? 9
                                                                   : 5;
        const int32_t expected_damage = !first                      ? 8
                                        : mode == GE_RAW_STACK      ? 0
                                        : mode == GE_REPLACED_STACK ? 17
                                                                    : 7;
        CHECK(item != NULL && item->type == MC_NBT_COMPOUND && item_id != NULL &&
              item_id->type == MC_NBT_SHORT && item_id->value.short_value == expected_id &&
              item_count != NULL && item_count->type == MC_NBT_BYTE &&
              item_count->value.byte_value == expected_count && damage != NULL &&
              damage->type == MC_NBT_SHORT && damage->value.short_value == expected_damage);
        if (first && mode != GE_ORIGINAL_STACK)
            CHECK(unknown == NULL); /* Never resurrect the previous Stack's provenance. */
        else
            CHECK(unknown != NULL && unknown->type == MC_NBT_LONG &&
                  unknown->value.long_value == markers[i] + 1000);
    }
    ok = true;
cleanup:
    ge_disk_free(&disk);
    return ok;
}

static bool ge_load(fixture *f, size_t slot, size_t expected)
{
    bool ok = false;
    mc_chunk_state *loaded = NULL;
    mc_world_runtime_acquisition_error error;
    const mc_chunk_acquire_status status =
        mc_world_runtime_get_detailed(f->worlds[slot], 0, 0, &loaded, &error);
    if (f->providers[slot].cache->insertion_count != 0)
        f->states[slot] = f->providers[slot].cache->insertions[0];
    CHECK(status == MC_CHUNK_ACQUIRE_OK && loaded != NULL && loaded == f->states[slot] &&
          loaded->active && loaded->has_entities && error.load_entity.index == SIZE_MAX &&
          mc_world_store_game_stage_count(f->stores[slot]) == 0);
    f->chunks[slot] = mc_world_store_chunk_object(f->stores[slot], loaded);
    CHECK(f->chunks[slot] != NULL);
    size_t count = 0;
    for (size_t i = 0; i < CHUNK_ENTITY_SLICES; ++i)
        count += f->chunks[slot]->entities[i].count;
    CHECK(count == expected && f->worlds[slot]->fields.b->count == expected &&
          mc_world_chunk_source_enabled(f->chunks[slot]));
    ok = true;
cleanup:
    return ok;
}

static bool ge_hold(fixture *f, size_t slot, size_t index, mc_entity **out)
{
    /* Fixture identity selectors: A, B, the second A occurrence. These do not
     * replace or reorder the real lists; disk assertions check source order. */
    const size_t slice = index == 1 ? 1 : 4, offset = index == 2 ? 1 : 0;
    mc_entity *borrowed = NULL;
    if (index > 2 ||
        mc_world_chunk_entity_at(&f->chunks[slot]->entities[slice], offset, &borrowed) !=
            MC_WORLD_STORAGE_OK ||
        borrowed == NULL || mc_entity_retain(borrowed) != MC_ENTITY_OK)
        return false;
    *out = borrowed;
    return true;
}

static bool ge_write(fixture *f, size_t slot)
{
    bool ok = false;
    uint32_t growth = UINT32_MAX;
    mc_world_game_save_error error;
    CHECK(mc_world_runtime_write_game_chunk(f->worlds[slot], f->states[slot],
                                            fixture_timestamp + (uint32_t)slot, &growth,
                                            &error) == MC_WORLD_STORAGE_OK &&
          growth != UINT32_MAX && error.status == MC_WORLD_STORAGE_OK &&
          error.entity_slice == SIZE_MAX && error.entity_index == SIZE_MAX &&
          error.entity.status == MC_ENTITY_NBT_OK && error.entity_records.status == MC_NBT_OK &&
          f->states[slot]->has_entities);
    ok = true;
cleanup:
    return ok;
}

static bool ge_save_controls(fixture *f, size_t slot)
{
    bool ok = false;
    mc_entity *a = NULL, *b = NULL;
    mc_item_stack *old_stack = NULL, *raw = NULL;
    ge_disk before = {0};
    mc_world_game_save_error error;
    mc_world_entity_error insertion;
    const int64_t original[] = {10, 202, 20, 101, 30}, duplicate[] = {10, 202, 20, 101, 30, 101},
                  only_b[] = {10, 202, 20, 30}, only_a[] = {10, 101, 20, 101, 30},
                  none[] = {10, 20, 30};
    CHECK(ge_hold(f, slot, 0, &a) && ge_hold(f, slot, 1, &b));
    CHECK(a != b && a->base.world == f->worlds[slot] && b->base.world == f->worlds[slot] &&
          a->base.bF && a->base.bG == 0 && a->base.bH == 4 && a->base.bI == 0 &&
          field(mc_entity_storage_source(a), "EntityExtra") != NULL &&
          mc_item_entity_stack_get(a, &old_stack) == MC_ENTITY_OK && old_stack != NULL &&
          field(mc_item_stack_source(old_stack), "ItemExtra") != NULL && ge_write(f, slot) &&
          ge_output(f, slot, original, COUNT(original), GE_ORIGINAL_STACK));
    const size_t holds = a->holds;
    CHECK(mc_world_runtime_chunk_add_entity(f->chunks[slot], a, &insertion) ==
              MC_WORLD_STORAGE_OK &&
          a->holds == holds + 1 && f->chunks[slot]->entities[4].count == 2 &&
          f->chunks[slot]->entities[1].count == 1 && f->worlds[slot]->fields.b->count == 2 &&
          ge_write(f, slot) && ge_output(f, slot, duplicate, COUNT(duplicate), GE_ORIGINAL_STACK));

    /* The earlier live slice1 result survives both false slice4 occurrences. */
    const bool prior_a_dead = a->base.be, prior_b_dead = b->base.be;
    a->base.be = true;
    uint32_t growth = UINT32_MAX;
    const mc_world_storage_status skipped = mc_world_runtime_write_game_chunk(
        f->worlds[slot], f->states[slot], fixture_timestamp, &growth, &error);
    a->base.be = prior_a_dead;
    CHECK(skipped == MC_WORLD_STORAGE_OK && growth != UINT32_MAX &&
          error.entity.status == MC_ENTITY_NBT_OK && f->states[slot]->has_entities &&
          ge_output(f, slot, only_b, COUNT(only_b), GE_ORIGINAL_STACK));
    b->base.be = true;
    growth = UINT32_MAX;
    const mc_world_storage_status false_first = mc_world_runtime_write_game_chunk(
        f->worlds[slot], f->states[slot], fixture_timestamp, &growth, &error);
    b->base.be = prior_b_dead;
    CHECK(false_first == MC_WORLD_STORAGE_OK && growth != UINT32_MAX &&
          f->states[slot]->has_entities &&
          ge_output(f, slot, only_a, COUNT(only_a), GE_ORIGINAL_STACK));
    a->base.be = b->base.be = true;
    growth = UINT32_MAX;
    const mc_world_storage_status all_false = mc_world_runtime_write_game_chunk(
        f->worlds[slot], f->states[slot], fixture_timestamp, &growth, &error);
    a->base.be = prior_a_dead;
    b->base.be = prior_b_dead;
    CHECK(all_false == MC_WORLD_STORAGE_OK && growth != UINT32_MAX &&
          f->states[slot]->has_entities &&
          ge_output(f, slot, none, COUNT(none), GE_ORIGINAL_STACK));

    ge_record replacement;
    ge_prepare(&replacement, 101, false, false, false);
    replacement.stack[0].value.short_value = 5;
    replacement.stack[1].value.byte_value = 9;
    replacement.stack[2].value.short_value = 17;
    mc_entity_nbt_error semantic;
    CHECK(mc_entity_read_nbt(a, &replacement.root, &semantic) == MC_ENTITY_NBT_OK &&
          a->stack != old_stack && mc_item_stack_source(a->stack) == NULL &&
          field(mc_item_stack_source(old_stack), "ItemExtra") != NULL && ge_write(f, slot) &&
          ge_output(f, slot, duplicate, COUNT(duplicate), GE_REPLACED_STACK));

    /* Slice1's B serializes before the later slice4 A fails. No region commit. */
    CHECK(ge_read(f, slot, &before));
    mc_item_stack *const saved_stack = a->stack;
    const size_t a_holds = a->holds, b_holds = b->holds;
    a->stack = NULL;
    growth = UINT32_MAX;
    const mc_world_storage_status null_stack = mc_world_runtime_write_game_chunk(
        f->worlds[slot], f->states[slot], fixture_timestamp, &growth, &error);
    a->stack = saved_stack;
    CHECK(null_stack != MC_WORLD_STORAGE_OK && growth == UINT32_MAX &&
          error.serializer_bytecode == 183 && error.entity_slice == 4 && error.entity_index == 0 &&
          error.entity.status == MC_ENTITY_NBT_NULL_REFERENCE &&
          error.entity.wrapper_bytecode == 27 && error.entity.item_bytecode == 37 &&
          f->states[slot]->has_entities && a->holds == a_holds && b->holds == b_holds &&
          ge_same_disk(f, slot, &before));
    ge_disk_free(&before);

    /* Transfer a real allocation, not a fabricated RAW phase or hold count. */
    CHECK(mc_item_stack_allocate(&raw) == MC_ITEM_STACK_OK);
    mc_item_stack *const previous = a->stack;
    a->stack = raw;
    raw = NULL;
    mc_item_stack_release(previous);
    mc_item_stack_phase phase;
    CHECK(mc_item_stack_phase_get(a->stack, &phase) == MC_ITEM_STACK_OK &&
          phase == MC_ITEM_STACK_RAW && ge_write(f, slot) &&
          ge_output(f, slot, duplicate, COUNT(duplicate), GE_RAW_STACK));
    ok = true;
cleanup:
    ge_disk_free(&before);
    mc_item_stack_release(raw);
    mc_item_stack_release(old_stack);
    mc_entity_release(a);
    mc_entity_release(b);
    return ok;
}

static bool ge_copy_reload(fixture *f, size_t source, size_t target)
{
    bool ok = false;
    ge_disk disk = {0};
    mc_entity *old = NULL, *a = NULL, *b = NULL, *duplicate = NULL;
    uint32_t growth = UINT32_MAX;
    const int64_t expected[] = {10, 202, 20, 101, 30, 101};
    CHECK(ge_read(f, source, &disk) && ge_hold(f, source, 0, &old));
    CHECK(f->worlds[source] != f->worlds[target] && f->states[target] == NULL &&
          mc_world_regions_write(f->regions, mc_world_store_root(f->stores[target]), 0, 0,
                                 disk.bytes.data, disk.bytes.length, fixture_timestamp, &growth,
                                 NULL) == MC_WORLD_STORAGE_OK &&
          ge_load(f, target, 3) && ge_hold(f, target, 0, &a) && ge_hold(f, target, 1, &b) &&
          ge_hold(f, target, 2, &duplicate));
    CHECK(a != old && a != b && a != duplicate && b != duplicate &&
          a->base.world == f->worlds[target] && duplicate->base.world == f->worlds[target] &&
          old->base.world == f->worlds[source] && a->stack != duplicate->stack &&
          a->storage_source != duplicate->storage_source &&
          a->classes == f->entity_classes[target % 2] &&
          field(mc_entity_storage_source(a), "EntityExtra") != NULL &&
          field(mc_item_stack_source(a->stack), "ItemExtra") == NULL && ge_write(f, target) &&
          ge_output(f, target, expected, COUNT(expected), GE_RAW_STACK));
    ok = true;
cleanup:
    mc_entity_release(old);
    mc_entity_release(a);
    mc_entity_release(b);
    mc_entity_release(duplicate);
    ge_disk_free(&disk);
    return ok;
}

static bool ge_failed_import(fixture *f, size_t slot, bool bad_item)
{
    bool ok = false;
    mc_world_store_game_stage *stage = NULL;
    mc_world_store_game_stage_view view = {0};
    mc_entity_class_snapshot before, after, retry;
    mc_world_runtime_acquisition_error error;
    CHECK(ge_seed(f, slot, bad_item ? GE_BAD_ITEM : GE_BAD_TILES) &&
          mc_entity_classes_get_snapshot(f->entity_classes[slot % 2], &before) == MC_ENTITY_OK);
    mc_chunk_state sentinel = {0};
    mc_chunk_state *out = &sentinel;
    const mc_chunk_acquire_status status =
        mc_world_runtime_get_detailed(f->worlds[slot], 0, 0, &out, &error);
    CHECK(status != MC_CHUNK_ACQUIRE_OK && out == &sentinel &&
          f->providers[slot].cache->count == 0 && f->providers[slot].cache->insertion_count == 0 &&
          mc_world_store_game_stage_count(f->stores[slot]) == 1 &&
          mc_world_store_game_stage_at(f->stores[slot], 0, &stage, &view, NULL) ==
              MC_WORLD_STORAGE_OK);
    f->states[slot] = view.state;
    f->chunks[slot] = view.chunk;
    CHECK(view.phase == MC_WORLD_STORE_GAME_FAILED && !view.state->active &&
          view.state->has_entities && !mc_world_chunk_source_enabled(view.chunk) &&
          view.chunk->data->blocks[0] == 200 &&
          view.chunk->entities[4].count == (bad_item ? 1u : 0u) &&
          f->worlds[slot]->fields.b->count == 0 &&
          mc_entity_classes_get_snapshot(f->entity_classes[slot % 2], &after) == MC_ENTITY_OK &&
          after.live_entities == before.live_entities + (bad_item ? 1u : 0u) &&
          after.next_id == before.next_id + (bad_item ? 2 : 0));
    mc_entity *retained = NULL;
    if (bad_item) {
        CHECK(mc_world_chunk_entity_at(&view.chunk->entities[4], 0, &retained) ==
                  MC_WORLD_STORAGE_OK &&
              retained != NULL && retained->holds == 1 &&
              field(mc_entity_storage_source(retained), "EntityExtra") != NULL &&
              field(mc_item_stack_source(retained->stack), "ItemExtra") != NULL);
        CHECK(error.load_builder_bytecode == 269 && error.load_entity.index == 2 &&
              error.load_entity.factory.factory_bytecode == 66 &&
              error.load_entity.factory.stack.field != NULL &&
              strcmp(error.load_entity.factory.stack.field, "Count") == 0 &&
              error.load_entity.provenance.status == MC_ENTITY_NBT_OK &&
              error.load_entity.insertion.status == MC_WORLD_STORAGE_OK);
    } else
        CHECK(error.load_builder_bytecode == 301 &&
              error.load_entity.factory.status == MC_ENTITY_NBT_OK &&
              error.load_entity.provenance.status == MC_ENTITY_NBT_OK);
    /* Retained failed capacity prevents replay; no original occurrence retries. */
    const mc_chunk_acquire_status repeated =
        mc_world_runtime_get_detailed(f->worlds[slot], 0, 0, &out, &error);
    CHECK(repeated != MC_CHUNK_ACQUIRE_OK && out == &sentinel &&
          mc_world_store_game_stage_count(f->stores[slot]) == 1 &&
          mc_entity_classes_get_snapshot(f->entity_classes[slot % 2], &retry) == MC_ENTITY_OK &&
          retry.next_id == after.next_id && retry.live_entities == after.live_entities &&
          (retained == NULL || retained->holds == 1));
    uint32_t growth = UINT32_MAX;
    mc_world_game_save_error save_error;
    CHECK(mc_world_runtime_write_game_chunk(f->worlds[slot], view.state, fixture_timestamp, &growth,
                                            &save_error) != MC_WORLD_STORAGE_OK &&
          growth == UINT32_MAX && view.chunk->entities[4].count == (bad_item ? 1u : 0u));
    ok = true;
cleanup:
    return ok;
}

static bool ge_session_and_quota(fixture *f, size_t slot)
{
    bool ok = false;
    mc_entity *a = NULL;
    ge_disk before = {0};
    mc_ws_file_bytes token = {0};
    char *path = NULL;
    mc_world_game_save_error error;
    mc_world_entity_error insertion;
    uint32_t growth = UINT32_MAX;
    CHECK(ge_seed(f, slot, GE_MIXED) && ge_load(f, slot, 2) && ge_write(f, slot) &&
          ge_read(f, slot, &before) && ge_hold(f, slot, 0, &a) &&
          mc_ws_path_join(mc_world_store_root(f->stores[slot]), "session.lock", &path, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_ws_read_file(path, MC_WS_FILE_MAX_BYTES, &token, NULL) == MC_WORLD_STORAGE_OK);
    const uint8_t wrong[8] = {0};
    CHECK(mc_ws_write_file(path, wrong, sizeof(wrong), false, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_storage_status mismatch = mc_world_runtime_write_game_chunk(
        f->worlds[slot], f->states[slot], fixture_timestamp, &growth, &error);
    const mc_world_storage_status restored =
        mc_ws_write_file(path, token.data, token.length, false, NULL);
    CHECK(restored == MC_WORLD_STORAGE_OK && mismatch == MC_WORLD_STORAGE_SESSION_MISMATCH &&
          growth == UINT32_MAX && error.region_bytecode == 1 && error.serializer_bytecode == 0 &&
          error.entity_slice == SIZE_MAX && error.entity_index == SIZE_MAX &&
          error.entity.status == MC_ENTITY_NBT_OK && ge_same_disk(f, slot, &before));
    CHECK(mc_world_runtime_chunk_add_entity(f->chunks[slot], a, &insertion) ==
              MC_WORLD_STORAGE_OK &&
          f->chunks[slot]->entities[4].count == 2 && f->chunks[slot]->entities[1].count == 1);
    growth = UINT32_MAX;
    const mc_world_storage_status quota = mc_world_runtime_write_game_chunk(
        f->worlds[slot], f->states[slot], fixture_timestamp, &growth, &error);
    CHECK(quota == MC_WORLD_STORAGE_LIMIT && growth == UINT32_MAX &&
          error.entity.status == MC_ENTITY_NBT_OK &&
          error.composition.status == MC_STORED_CHUNK_LIMIT &&
          f->chunks[slot]->entities[4].count == 2 && f->chunks[slot]->entities[1].count == 1 &&
          f->states[slot]->has_entities && ge_same_disk(f, slot, &before));
    ok = true;
cleanup:
    free(path);
    mc_ws_free_file_bytes(&token);
    ge_disk_free(&before);
    mc_entity_release(a);
    return ok;
}

static bool ge_journal_protocol(fixture *f, size_t slot)
{
    bool ok = false, entered = false, begun = false;
    mc_world_store_game_stage *stage = NULL;
    mc_world_store_game_stage_view view = {0};
    mc_entity *entity = NULL;
    mc_entity_nbt_error semantic;
    mc_world_chunk_entity_error insertion;
    mc_world_storage_error journal;
    CHECK(ge_seed(f, slot, GE_ONE_ITEM));
    CHECK(mc_world_runtime_call_enter(f->worlds[slot], NULL) == MC_WORLD_STORAGE_OK);
    entered = true;
    CHECK(mc_world_store_game_stage_begin(f->stores[slot], f->worlds[slot], 0, 0, 0, &stage,
                                          NULL) == MC_WORLD_STORAGE_OK);
    begun = true;
    CHECK(mc_world_store_game_stage_view_get(f->stores[slot], stage, &view, NULL) ==
          MC_WORLD_STORAGE_OK);
    f->states[slot] = view.state;
    f->chunks[slot] = view.chunk;
    CHECK(mc_world_store_game_stage_imports_begin(f->stores[slot], stage, NULL) ==
          MC_WORLD_STORAGE_OK);
    const mc_nbt_tag *records = field(view.level, "Entities"), *record = NULL;
    CHECK(records != NULL && records->value.list.length == 1);
    record = records->value.list.items[0];
    CHECK(mc_entity_factory_nbt(f->entity_classes[slot % 2], f->worlds[slot], record, &entity,
                                &semantic) == MC_ENTITY_NBT_OK &&
          entity != NULL &&
          mc_world_chunk_insert_entity(view.chunk, entity, &insertion) == MC_WORLD_STORAGE_OK &&
          mc_entity_preserve_source(entity, record, &view.limits, &semantic) == MC_ENTITY_NBT_OK &&
          mc_world_store_game_stage_record_entity_result(f->stores[slot], stage, 0, true,
                                                         &journal) == MC_WORLD_STORAGE_OK);
    const size_t holds = entity->holds;
    const mc_world_storage_status extra =
        mc_world_store_game_stage_record_entity_result(f->stores[slot], stage, 1, false, &journal);
    CHECK(extra == MC_WORLD_STORAGE_INVALID_ARGUMENT && journal.status == extra &&
          entity->holds == holds && view.chunk->entities[4].count == 1 &&
          mc_world_store_game_stage_fail(f->stores[slot], stage, extra, NULL) == extra);
    begun = false;
    CHECK(mc_world_store_game_stage_view_get(f->stores[slot], stage, &view, NULL) ==
              MC_WORLD_STORAGE_OK &&
          view.phase == MC_WORLD_STORE_GAME_FAILED &&
          mc_world_store_game_stage_count(f->stores[slot]) == 1 &&
          !mc_world_chunk_source_enabled(view.chunk) && view.state->has_entities &&
          entity->holds == holds && field(mc_entity_storage_source(entity), "EntityExtra") != NULL);
    ok = true;
cleanup:
    if (begun)
        (void)mc_world_store_game_stage_fail(f->stores[slot], stage,
                                             MC_WORLD_STORAGE_UNSUPPORTED_STATE, NULL);
    mc_entity_release(entity);
    if (entered)
        mc_world_runtime_call_leave(f->worlds[slot]);
    return ok;
}

int main(int argc, char **argv)
{
    int result = 1;
    fixture *f = NULL;
    char base[8192], root[8192];
    if (argc != 3 || strcmp(argv[1], "world-game-entities") != 0 ||
        !decode_path(argv[2], base, sizeof(base)) || !mc_math_trig_init(&trig))
        return 1;
    for (unsigned int epoch = 0; epoch < 3; ++epoch) {
        const int n = snprintf(root, sizeof(root), "%s/epoch%u", base, epoch);
        CHECK(n > 0 && (size_t)n < sizeof(root));
        /* The quota is a caller-selected tag ceiling, not an internal byte
         * estimate. The real two-Item save must pass before adding a duplicate. */
        CHECK(ge_setup(&f, root, epoch == 2 ? 80 : mc_nbt_default_limits().max_tags, epoch == 1));
        if (epoch == 0) {
            for (size_t side = 0; side < 2; ++side)
                CHECK(ge_seed(f, side, GE_MIXED) && ge_load(f, side, 2) &&
                      ge_save_controls(f, side) && ge_copy_reload(f, side, side + 2));
        } else if (epoch == 1) {
            for (size_t slot = 0; slot < WORLD_COUNT; ++slot)
                CHECK(ge_failed_import(f, slot, slot < 2));
        } else {
            for (size_t side = 0; side < 2; ++side)
                CHECK(ge_session_and_quota(f, side) && ge_journal_protocol(f, side + 2));
        }
        if (!cleanup_fixture(f)) {
            fputs("GAME Entity epoch cleanup refused; surviving real owners retained\n", stderr);
            return 1;
        }
        f = NULL;
    }
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("GAME Entity cleanup refused; surviving real owners retained\n", stderr);
        return 1;
    }
    if (result == 0 &&
        printf("PASS: %u GAME Entity disk integration checks (world-game-entities)\n", checks) < 0)
        result = 1;
    return result;
}
