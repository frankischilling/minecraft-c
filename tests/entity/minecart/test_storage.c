/* Native storage contracts. The included GAME fixture supplies real Worlds,
 * stores, resource streams and cleanup; its old main is never invoked here. */
#define main mc_minecart_storage_old_tile_main
#include "../../world/runtime/tile_entities/test_tile_entities.c"
#undef main

#include "../../../src/entity/minecart/entity_internal.h"
#include "../../../src/entity/minecart/inventory_internal.h"
#include "../../../src/entity/minecart/storage_internal.h"
#include "mc/entity/minecart/inventory.h"
#include "mc/entity/nbt/storage.h"
#include "mc/world/runtime/saving/saving.h"

/* The unchanged TileEntity fixture has a single main-renaming layer. Reusing the
 * Entity test itself would nest conflicting main macros. These transport and
 * setup helpers follow that Entity fixture's actual public operations. */
typedef struct ge_disk {
    mc_compression_buffer bytes;
    mc_nbt_document *document;
} ge_disk;

static uint64_t ge_double_bits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool ge_text(mc_nbt_string value, const char *text) {
    const size_t length = strlen(text);
    if (value.length != length || (length != 0 && value.units == NULL))
        return false;
    for (size_t i = 0; i < length; ++i)
        if (value.units[i] != (unsigned char)text[i])
            return false;
    return true;
}

static bool ge_setup(fixture **out, const char *root) {
    bool ok = false;
    fixture *f = calloc(1, sizeof(*f));
    *out = f;
    const uint16_t title[] = {'S', 't', 'o', 'r', 'a', 'g', 'e'};
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
    }
    for (size_t slot = 0; slot < WORLD_COUNT; ++slot) {
        char path[8192];
        uint8_t map[256];
        const int n = snprintf(path, sizeof(path), "%s/world%zu", root, slot);
        CHECK(n > 0 && (size_t)n < sizeof(path));
        for (size_t i = 0; i < COUNT(map); ++i)
            map[i] = (uint8_t)i;
        mc_world_store_options options = {.session_token = 137,
                                          .block_map = map,
                                          .block_map_length = sizeof(map),
                                          .limits = mc_world_store_default_limits()};
        options.limits.max_loaded_chunks = 1;
        const mc_level_info info = {
            .seed = 23, .spawn_y = 64, .level_name = {title, COUNT(title)}, .version = 19132};
        CHECK(mc_world_store_open(path, f->regions, &options, &f->stores[slot], NULL) ==
                  MC_WORLD_STORAGE_OK &&
              mc_world_store_save_info(f->stores[slot], &info, 123, NULL) == MC_WORLD_STORAGE_OK &&
              create_world(f, slot));
    }
    ok = true;
cleanup:
    return ok;
}

static void ge_disk_free(ge_disk *disk) {
    mc_nbt_document_free(disk->document);
    mc_compression_buffer_free(&disk->bytes);
    memset(disk, 0, sizeof(*disk));
}

static bool ge_read(fixture *f, size_t slot, ge_disk *disk) {
    size_t consumed = 0;
    return mc_world_regions_read(f->regions, mc_world_store_root(f->stores[slot]), 0, 0,
                                 &disk->bytes, NULL) == MC_WORLD_STORAGE_OK &&
           mc_nbt_read(disk->bytes.data, disk->bytes.length, NULL, &disk->document, &consumed,
                       NULL) == MC_NBT_OK &&
           consumed == disk->bytes.length;
}

static bool ge_load(fixture *f, size_t slot, size_t expected) {
    bool ok = false;
    mc_chunk_state *loaded = NULL;
    mc_world_runtime_acquisition_error error;
    const mc_chunk_acquire_status status =
        mc_world_runtime_get_detailed(f->worlds[slot], 0, 0, &loaded, &error);
    if (f->providers[slot].cache->insertion_count != 0)
        f->states[slot] = f->providers[slot].cache->insertions[0];
    CHECK(status == MC_CHUNK_ACQUIRE_OK && loaded != NULL && loaded == f->states[slot] &&
          loaded->active && loaded->has_entities &&
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

static bool ge_write(fixture *f, size_t slot) {
    bool ok = false;
    uint32_t growth = UINT32_MAX;
    mc_world_game_save_error error;
    CHECK(mc_world_runtime_write_game_chunk(f->worlds[slot], f->states[slot], fixture_timestamp,
                                            &growth, &error) == MC_WORLD_STORAGE_OK &&
          growth != UINT32_MAX && error.status == MC_WORLD_STORAGE_OK &&
          error.entity.status == MC_ENTITY_NBT_OK);
    ok = true;
cleanup:
    return ok;
}

static unsigned int storage_cases;

enum {
    MS_POS,
    MS_MOTION,
    MS_ROTATION,
    MS_FALL,
    MS_FIRE,
    MS_AIR,
    MS_GROUND,
    MS_TYPE,
    MS_ITEMS,
    MS_X,
    MS_Z,
    MS_FUEL,
    MS_ID,
    MS_EXTRA,
    MS_ITEM,
    MS_NEAR
};

typedef struct ms_record {
    mc_nbt_tag root, fields[16], vectors[8], entries[8], values[8][5];
    const mc_nbt_tag *children[16], *pos[3], *motion[3], *rotation[2], *items[8], *parts[8][5];
    uint16_t names[16][20], id[16];
} ms_record;

typedef struct ms_leaf {
    mc_java_random_profile *profile;
    mc_entity_classes *classes;
    mc_entity *cart;
} ms_leaf;

static void ms_record_init(ms_record *r, int32_t type) {
    static const char *const names[] = {
        "Pos",   "Motion", "Rotation", "FallDistance", "Fire", "Air",       "OnGround", "Type",
        "Items", "PushX",  "PushZ",    "Fuel",         "id",   "RootExtra", "Item",     "Items"};
    static const mc_nbt_type types[] = {MC_NBT_LIST,   MC_NBT_LIST,   MC_NBT_LIST,   MC_NBT_FLOAT,
                                        MC_NBT_SHORT,  MC_NBT_SHORT,  MC_NBT_BYTE,   MC_NBT_INT,
                                        MC_NBT_LIST,   MC_NBT_DOUBLE, MC_NBT_DOUBLE, MC_NBT_SHORT,
                                        MC_NBT_STRING, MC_NBT_LONG,   MC_NBT_LONG,   MC_NBT_LONG};
    static const uint16_t slot[] = {'S', 'l', 'o', 't'}, id[] = {'i', 'd'},
                          count[] = {'C', 'o', 'u', 'n', 't'},
                          damage[] = {'D', 'a', 'm', 'a', 'g', 'e'},
                          occurrence[] = {'O', 'c', 'c', 'u', 'r', 'r', 'e', 'n', 'c', 'e'};
    memset(r, 0, sizeof(*r));
    r->root =
        (mc_nbt_tag){.type = MC_NBT_COMPOUND, .value.compound = {r->children, COUNT(r->children)}};
    for (size_t i = 0; i < COUNT(r->fields); ++i) {
        r->fields[i].type = types[i];
        r->fields[i].name = ascii(names[i], r->names[i]);
        r->children[i] = &r->fields[i];
    }
    /* A distinct UTF-16 name, including NUL, must not be filtered as Items. */
    r->names[MS_NEAR][5] = 0;
    r->fields[MS_NEAR].name.length = 6;
    for (size_t i = 0; i < 6; ++i) {
        r->vectors[i].type = MC_NBT_DOUBLE;
        r->vectors[i].value.double_bits = ge_double_bits(i < 3 ? 8.0 : 0.0);
    }
    for (size_t i = 0; i < 3; ++i) {
        r->pos[i] = &r->vectors[i];
        r->motion[i] = &r->vectors[i + 3u];
    }
    r->vectors[1].value.double_bits = ge_double_bits(64.0);
    r->vectors[6].type = r->vectors[7].type = MC_NBT_FLOAT;
    r->rotation[0] = &r->vectors[6];
    r->rotation[1] = &r->vectors[7];
    r->fields[MS_POS].value.list.element_type = MC_NBT_DOUBLE;
    r->fields[MS_POS].value.list.items = r->pos;
    r->fields[MS_POS].value.list.length = 3;
    r->fields[MS_MOTION].value.list.element_type = MC_NBT_DOUBLE;
    r->fields[MS_MOTION].value.list.items = r->motion;
    r->fields[MS_MOTION].value.list.length = 3;
    r->fields[MS_ROTATION].value.list.element_type = MC_NBT_FLOAT;
    r->fields[MS_ROTATION].value.list.items = r->rotation;
    r->fields[MS_ROTATION].value.list.length = 2;
    r->fields[MS_AIR].value.short_value = 300;
    r->fields[MS_TYPE].value.int_value = type;
    r->fields[MS_ITEMS].value.list.element_type = MC_NBT_COMPOUND;
    r->fields[MS_ITEMS].value.list.items = r->items;
    r->fields[MS_X].value.double_bits = UINT64_C(0x7ff8000000000042);
    r->fields[MS_Z].value.double_bits = UINT64_C(0x8000000000000000);
    r->fields[MS_FUEL].value.short_value = -7;
    r->fields[MS_ID].value.string = ascii("Minecart", r->id);
    r->fields[MS_EXTRA].value.long_value = 111;
    r->fields[MS_ITEM].value.long_value = 222;
    r->fields[MS_NEAR].value.long_value = 333;
    for (size_t i = 0; i < COUNT(r->entries); ++i) {
        r->values[i][0] = (mc_nbt_tag){.type = MC_NBT_BYTE, .name = {slot, COUNT(slot)}};
        r->values[i][1] = (mc_nbt_tag){.type = MC_NBT_SHORT, .name = {id, COUNT(id)}};
        r->values[i][2] = (mc_nbt_tag){.type = MC_NBT_BYTE, .name = {count, COUNT(count)}};
        r->values[i][3] = (mc_nbt_tag){.type = MC_NBT_SHORT, .name = {damage, COUNT(damage)}};
        r->values[i][4] =
            (mc_nbt_tag){.type = MC_NBT_LONG, .name = {occurrence, COUNT(occurrence)}};
        for (size_t j = 0; j < 5; ++j)
            r->parts[i][j] = &r->values[i][j];
        r->entries[i] = (mc_nbt_tag){.type = MC_NBT_COMPOUND, .value.compound = {r->parts[i], 5}};
        r->items[i] = &r->entries[i];
    }
}

static void ms_entry(ms_record *r, size_t i, int32_t slot, int32_t id, int32_t count,
                     int64_t marker) {
    r->values[i][0].value.byte_value = mc_java_i8_narrow(slot);
    r->values[i][1].value.short_value = mc_java_i16_narrow(id);
    r->values[i][2].value.byte_value = mc_java_i8_narrow(count);
    r->values[i][3].value.short_value = 6;
    r->values[i][4].value.long_value = marker;
    if (r->fields[MS_ITEMS].value.list.length <= i)
        r->fields[MS_ITEMS].value.list.length = i + 1u;
}

static void ms_winners(ms_record *r) {
    ms_record_init(r, 1);
    ms_entry(r, 0, 0, 1, 12, 11);
    ms_entry(r, 1, 1, 2, 8, 22);
    ms_entry(r, 2, 0, 3, 10, 33);
    /* Missing Slot defaults to0; this is the later successful winner. */
    r->entries[2].value.compound.items = r->parts[2] + 1;
    r->entries[2].value.compound.length = 4;
    ms_entry(r, 3, 27, 4, 2, 55);
    ms_entry(r, 4, -1, 5, 2, 66);
    ms_entry(r, 5, 2, 6, 2, 44);
    /* Valid transport, skipped before any Stack read by the semantic reader. */
    r->values[3][2].type = r->values[4][2].type = MC_NBT_SHORT;
    r->values[3][2].value.short_value = -9;
    r->values[4][2].value.short_value = -10;
}

static bool ms_open(ms_leaf *f, mc_entity_source side) {
    return mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK &&
           mc_entity_classes_create(side, f->profile, NULL, &f->classes) == MC_ENTITY_OK &&
           mc_minecart_entity_allocate(f->classes, &f->cart, NULL) == MC_ENTITY_OK &&
           mc_minecart_entity_construct(f->cart, NULL, 8.0, 64.0, 8.0, 1, NULL) == MC_ENTITY_OK;
}

static bool ms_close(ms_leaf *f) {
    mc_entity_release(f->cart);
    f->cart = NULL;
    if (mc_entity_classes_free(f->classes) != MC_ENTITY_OK)
        return false;
    f->classes = NULL;
    mc_java_random_profile_free(f->profile);
    f->profile = NULL;
    return true;
}

static bool ms_long(const mc_nbt_tag *tag, const char *name, int64_t expected) {
    const mc_nbt_tag *v = field(tag, name);
    return v != NULL && v->type == MC_NBT_LONG && v->value.long_value == expected;
}

static const mc_nbt_tag *ms_slot(const mc_nbt_tag *root, int32_t slot) {
    const mc_nbt_tag *list = field(root, "Items");
    if (list == NULL || list->type != MC_NBT_LIST)
        return NULL;
    for (size_t i = 0; i < list->value.list.length; ++i) {
        const mc_nbt_tag *v = list->value.list.items[i], *s = field(v, "Slot");
        if (s != NULL && s->type == MC_NBT_BYTE && ((int32_t)s->value.byte_value & 255) == slot)
            return v;
    }
    return NULL;
}

static bool ms_known(const mc_nbt_tag *root, int32_t slot, int32_t id, int32_t count) {
    const mc_nbt_tag *v = ms_slot(root, slot), *a = field(v, "id"), *b = field(v, "Count"),
                     *c = field(v, "Damage");
    return v != NULL && a != NULL && a->type == MC_NBT_SHORT && a->value.short_value == id &&
           b != NULL && b->type == MC_NBT_BYTE && b->value.byte_value == count && c != NULL &&
           c->type == MC_NBT_SHORT && c->value.short_value == 6;
}

static bool ms_save(mc_entity *cart, mc_nbt_tree **out) {
    bool saved = false;
    return mc_entity_save_storage_nbt(cart, NULL, &saved, out, NULL) == MC_ENTITY_NBT_OK && saved;
}

static bool ms_measure_owner(const mc_nbt_tree *tree) {
    mc_nbt_limits limits = mc_nbt_default_limits();
    size_t bytes = 0;
    limits.max_allocation_bytes = mc_nbt_tree_allocation_bytes(tree);
    return mc_nbt_measure(mc_nbt_tree_root(tree), &limits, &bytes, NULL) == MC_NBT_OK;
}

static bool ms_add(size_t *value, size_t add) {
    if (add > SIZE_MAX - *value)
        return false;
    *value += add;
    return true;
}

static bool ms_semantic_peaks(mc_entity_source side) {
    bool ok = false, saved = false;
    ms_leaf f = {0};
    ms_record r;
    mc_nbt_tree *full = NULL, *own = NULL, *named = NULL, *result = NULL;
    mc_entity_nbt_error error;
    size_t expected = 0;
    ++storage_cases;
    CHECK(ms_open(&f, side));
    ms_record_init(&r, 1);
    ms_entry(&r, 0, 0, 3, 10, 33);
    CHECK(mc_entity_read_nbt(f.cart, &r.root, NULL) == MC_ENTITY_NBT_OK);
    CHECK(mc_entity_build_nbt(f.cart, NULL, &full, NULL) == MC_ENTITY_NBT_OK);
    const mc_nbt_tag *selected[] = {field(mc_nbt_tree_root(full), "Type"),
                                    field(mc_nbt_tree_root(full), "Items")};
    const mc_nbt_tag view = {.type = MC_NBT_COMPOUND, .value.compound = {selected, 2}};
    CHECK(selected[0] != NULL && selected[1] != NULL &&
          mc_nbt_tree_copy_compound(&view, NULL, &own, NULL) == MC_NBT_OK);
    const size_t F = mc_nbt_tree_allocation_bytes(full), O = mc_nbt_tree_allocation_bytes(own);
    CHECK(F > 0 && O > 0 && ms_measure_owner(full) && ms_measure_owner(own));
    CHECK(ms_add(&expected, O) && ms_add(&expected, F));
    mc_nbt_limits limits = mc_nbt_default_limits();
    limits.max_allocation_bytes = expected - 1u;
    mc_nbt_tree *unchanged = full;
    CHECK(mc_entity_build_nbt(f.cart, &limits, &unchanged, &error) == MC_ENTITY_NBT_LIMIT);
    CHECK(unchanged == full && error.base_bytecode == 148 && error.minecart_write.selected &&
          error.minecart_write.entered && error.minecart_write.bytecode == 137);
    limits.max_allocation_bytes = expected;
    CHECK(mc_entity_build_nbt(f.cart, &limits, &result, &error) == MC_ENTITY_NBT_OK &&
          result != full && mc_nbt_tree_allocation_bytes(result) == F);
    mc_nbt_tree_free(result);
    result = NULL;
    CHECK(mc_entity_save_nbt(f.cart, NULL, &saved, &named, NULL) == MC_ENTITY_NBT_OK && saved);
    const size_t G = mc_nbt_tree_allocation_bytes(named);
    expected = 0;
    CHECK(G > O && ms_measure_owner(named) && ms_add(&expected, F) && ms_add(&expected, G));
    limits.max_allocation_bytes = expected - 1u;
    for (unsigned int initial = 0; initial < 2; ++initial) {
        const bool prior = initial != 0;
        unchanged = full;
        saved = prior;
        CHECK(mc_entity_save_nbt(f.cart, &limits, &saved, &unchanged, &error) ==
              MC_ENTITY_NBT_LIMIT);
        CHECK(saved == prior && unchanged == full && error.wrapper_bytecode == 27 &&
              strcmp(error.operation, "Entity NBT Boolean save independent tree") == 0);
    }
    limits.max_allocation_bytes = expected;
    CHECK(mc_entity_save_nbt(f.cart, &limits, &saved, &result, &error) == MC_ENTITY_NBT_OK &&
          saved && result != named && result != full && mc_nbt_tree_allocation_bytes(result) == G);
    mc_item_stack_fields unchanged_fields;
    CHECK(f.cart->minecart->root_generation == NULL &&
          f.cart->minecart->fields.h->storage == NULL &&
          mc_item_stack_fields_get(f.cart->minecart->fields.h->elements[0], &unchanged_fields) ==
              MC_ITEM_STACK_OK &&
          unchanged_fields.id == 3 && unchanged_fields.count == 10 && unchanged_fields.damage == 6);
    ok = true;
cleanup:
    mc_nbt_tree_free(result);
    mc_nbt_tree_free(named);
    mc_nbt_tree_free(own);
    mc_nbt_tree_free(full);
    if (!ms_close(&f))
        ok = false;
    return ok;
}

static bool ms_prepare_alias(ms_leaf *f, ms_record *r, mc_item_stack **held) {
    if (mc_entity_read_nbt(f->cart, &r->root, NULL) != MC_ENTITY_NBT_OK ||
        mc_minecart_inventory_get(f->cart, 0, held, NULL) != MC_MINECART_INVENTORY_OK ||
        *held == NULL ||
        mc_minecart_inventory_set(f->cart, 1, *held, NULL) != MC_MINECART_INVENTORY_OK ||
        mc_minecart_inventory_set(f->cart, 2, NULL, NULL) != MC_MINECART_INVENTORY_OK)
        return false;
    return mc_entity_preserve_source(f->cart, &r->root, NULL, NULL) == MC_ENTITY_NBT_OK;
}

static mc_item_stack_status ms_refuse(mc_item_stack **out) {
    (void)out;
    return MC_ITEM_STACK_ALLOCATION_FAILED;
}

static bool ms_graph_bytes(const mc_entity *cart, size_t *out);

static bool ms_alias_occurrences(mc_entity_source side) {
    bool ok = false;
    ms_leaf f = {0};
    ms_record r;
    mc_item_stack *held = NULL, *split = NULL, *removed = NULL;
    mc_nbt_tree *tree = NULL;
    mc_minecart_inventory_error error;
    ++storage_cases;
    CHECK(ms_open(&f, side));
    ms_winners(&r);
    CHECK(ms_prepare_alias(&f, &r, &held));
    mc_minecart_stack_array *h = f.cart->minecart->fields.h;
    mc_minecart_storage_generation *generation = f.cart->minecart->root_generation;
    CHECK(h->length == 27 && h->elements[0] == held && h->elements[1] == held &&
          h->elements[2] == NULL && h->storage != NULL && h->storage->generation == generation &&
          generation->references == 2 && f.cart->storage_source == generation->tree);
    CHECK(ms_long(h->storage->records[0], "Occurrence", 33) &&
          ms_long(h->storage->records[1], "Occurrence", 22) && h->storage->records[2] == NULL);
    const mc_nbt_tag *raw = mc_entity_storage_source(f.cart), *raw_list = field(raw, "Items");
    CHECK(raw_list != NULL && raw_list->value.list.length == 6 &&
          ms_long(raw_list->value.list.items[0], "Occurrence", 11) &&
          ms_long(raw_list->value.list.items[3], "Occurrence", 55));
    /* Real Stack source metadata is counted but never becomes slot fallback. */
    uint16_t fallback_name[20];
    mc_nbt_tag extra = {
        .type = MC_NBT_LONG, .name = ascii("StackOnly", fallback_name), .value.long_value = 99};
    const mc_nbt_tag *extra_items[] = {&extra};
    const mc_nbt_tag extra_root = {.type = MC_NBT_COMPOUND, .value.compound = {extra_items, 1}};
    CHECK(mc_item_stack_preserve_source(held, &extra_root, NULL, NULL) == MC_ITEM_STACK_OK);
    const mc_nbt_tag *stack_source = mc_item_stack_source(held);
    size_t actual_bytes = 0, expected_bytes = 0;
    CHECK(mc_entity_storage_native_bytes(f.cart, &actual_bytes, NULL) == MC_ENTITY_NBT_OK &&
          ms_graph_bytes(f.cart, &expected_bytes) && actual_bytes == expected_bytes);
    CHECK(ms_save(f.cart, &tree));
    const mc_nbt_tag *saved = mc_nbt_tree_root(tree), *list = field(saved, "Items");
    CHECK(list != NULL && list->value.list.length == 2 && ms_known(saved, 0, 3, 10) &&
          ms_known(saved, 1, 3, 10) && ms_long(ms_slot(saved, 0), "Occurrence", 33) &&
          ms_long(ms_slot(saved, 1), "Occurrence", 22) && ms_slot(saved, 2) == NULL &&
          ms_slot(saved, 27) == NULL && ms_slot(saved, 255) == NULL);
    CHECK(field(ms_slot(saved, 0), "StackOnly") == NULL &&
          field(ms_slot(saved, 1), "StackOnly") == NULL && ms_long(saved, "RootExtra", 111) &&
          ms_long(saved, "Item", 222) && field(saved, "PushX") == NULL &&
          field(saved, "PushZ") == NULL && field(saved, "Fuel") == NULL);
    CHECK(mc_nbt_compound_get(saved, r.fields[MS_NEAR].name.units, 6) != NULL);
    mc_nbt_tree_free(tree);
    tree = NULL;
    const mc_nbt_tag *a = h->storage->records[0], *b = h->storage->records[1];
    CHECK(mc_minecart_inventory_split_with_allocator(f.cart, 0, 2, ms_refuse, &split, &error) ==
          MC_MINECART_INVENTORY_ALLOCATION_FAILED);
    CHECK(split == NULL && h->storage->records[0] == a && h->storage->records[1] == b &&
          error.context.stack_split.bytecode == 10);
    CHECK(mc_minecart_inventory_split(f.cart, 0, 2, &split, NULL) == MC_MINECART_INVENTORY_OK &&
          split != NULL && split != held && h->storage->records[0] == a &&
          h->storage->records[1] == b);
    CHECK(mc_minecart_inventory_set(f.cart, 3, split, NULL) == MC_MINECART_INVENTORY_OK);
    CHECK(ms_save(f.cart, &tree));
    saved = mc_nbt_tree_root(tree);
    CHECK(ms_known(saved, 0, 3, 6) && ms_known(saved, 1, 3, 6) && ms_known(saved, 3, 3, 2) &&
          ms_long(ms_slot(saved, 0), "Occurrence", 33) &&
          ms_long(ms_slot(saved, 1), "Occurrence", 22) &&
          field(ms_slot(saved, 3), "Occurrence") == NULL &&
          field(ms_slot(saved, 3), "StackOnly") == NULL);
    mc_nbt_tree_free(tree);
    tree = NULL;
    /* Same-identity set commits a replacement occurrence and clears ONLY A. */
    CHECK(mc_minecart_inventory_set(f.cart, 0, held, NULL) == MC_MINECART_INVENTORY_OK);
    CHECK(h->storage != NULL && h->storage->records[0] == NULL && h->storage->records[1] == b);
    CHECK(ms_save(f.cart, &tree));
    saved = mc_nbt_tree_root(tree);
    CHECK(field(ms_slot(saved, 0), "Occurrence") == NULL &&
          field(ms_slot(saved, 0), "StackOnly") == NULL &&
          ms_long(ms_slot(saved, 1), "Occurrence", 22));
    mc_nbt_tree_free(tree);
    tree = NULL;
    CHECK(mc_minecart_inventory_split(f.cart, 1, INT32_MAX, &removed, NULL) ==
              MC_MINECART_INVENTORY_OK &&
          removed == held && h->elements[1] == NULL && h->storage == NULL &&
          generation->references == 1);
    CHECK(mc_minecart_inventory_set(f.cart, 4, removed, NULL) == MC_MINECART_INVENTORY_OK &&
          mc_minecart_inventory_set(f.cart, 0, NULL, NULL) == MC_MINECART_INVENTORY_OK &&
          ms_save(f.cart, &tree));
    saved = mc_nbt_tree_root(tree);
    CHECK(ms_slot(saved, 0) == NULL && ms_slot(saved, 1) == NULL && ms_slot(saved, 2) == NULL &&
          field(ms_slot(saved, 4), "Occurrence") == NULL &&
          field(ms_slot(saved, 4), "StackOnly") == NULL &&
          mc_item_stack_source(held) == stack_source && mc_entity_storage_source(f.cart) == raw);
    ok = true;
cleanup:
    mc_nbt_tree_free(tree);
    mc_item_stack_release(removed);
    mc_item_stack_release(split);
    mc_item_stack_release(held);
    if (!ms_close(&f))
        ok = false;
    return ok;
}

static bool ms_graph_bytes(const mc_entity *cart, size_t *out) {
    const mc_minecart_stack_array *h = cart->minecart->fields.h;
    const mc_minecart_storage_generation *root = cart->minecart->root_generation;
    size_t total = mc_entity_native_bytes(cart);
    if (root != NULL && (!ms_add(&total, sizeof(*root)) ||
                         !ms_add(&total, mc_nbt_tree_allocation_bytes(root->tree))))
        return false;
    if (h != NULL) {
        if (h->storage != NULL) {
            const mc_minecart_storage_sidecar *s = h->storage;
            if (h->length > (SIZE_MAX - sizeof(*s)) / sizeof(*s->records) ||
                !ms_add(&total, sizeof(*s) + h->length * sizeof(*s->records)))
                return false;
            if (s->generation != root &&
                (!ms_add(&total, sizeof(*s->generation)) ||
                 !ms_add(&total, mc_nbt_tree_allocation_bytes(s->generation->tree))))
                return false;
        }
        for (size_t i = 0; i < h->length; ++i) {
            const mc_item_stack *v = h->elements[i];
            bool seen = false;
            size_t bytes = 0;
            for (size_t j = 0; j < i; ++j)
                seen = seen || h->elements[j] == v;
            if (v != NULL && !seen &&
                (mc_item_stack_native_bytes(v, &bytes) != MC_ITEM_STACK_OK ||
                 !ms_add(&total, bytes)))
                return false;
        }
    }
    *out = total;
    return true;
}

static bool ms_atomic_peaks(mc_entity_source side) {
    bool ok = false;
    ms_leaf f = {0};
    ms_record r;
    mc_item_stack *held = NULL;
    mc_nbt_tree *copy = NULL;
    mc_entity_nbt_error error;
    size_t L = 0, T = 0, expected = 0, peak = 0;
    ++storage_cases;
    CHECK(ms_open(&f, side));
    ms_winners(&r);
    CHECK(ms_prepare_alias(&f, &r, &held));
    mc_minecart_stack_array *h = f.cart->minecart->fields.h;
    mc_minecart_storage_generation *old = f.cart->minecart->root_generation;
    mc_minecart_storage_sidecar *old_side = h->storage;
    const mc_nbt_tag *a = old_side->records[0], *b = old_side->records[1];
    CHECK(mc_entity_storage_native_bytes(f.cart, &L, NULL) == MC_ENTITY_NBT_OK &&
          ms_graph_bytes(f.cart, &expected) && expected == L);
    CHECK(mc_nbt_tree_copy_compound(&r.root, NULL, &copy, NULL) == MC_NBT_OK &&
          ms_measure_owner(copy));
    T = mc_nbt_tree_allocation_bytes(copy);
    const size_t G = sizeof(mc_minecart_storage_generation),
                 S = sizeof(mc_minecart_storage_sidecar) + h->length * sizeof(*old_side->records);
    CHECK(ms_add(&peak, L) && ms_add(&peak, G) && ms_add(&peak, T) && ms_add(&peak, S));
    const size_t cuts[] = {L + G - 1u, L + G + T - 1u, peak - 1u};
    for (size_t i = 0; i < COUNT(cuts); ++i) {
        mc_nbt_limits limits = mc_nbt_default_limits();
        limits.max_allocation_bytes = cuts[i];
        CHECK(mc_entity_preserve_source(f.cart, &r.root, &limits, &error) == MC_ENTITY_NBT_LIMIT);
        const char *const operations[] = {"Minecart storage new generation budget",
                                          "Minecart storage independent input tree",
                                          "Minecart storage prepared sidecar budget"};
        CHECK(error.operation != NULL && strcmp(error.operation, operations[i]) == 0);
        size_t after = 0;
        CHECK(f.cart->minecart->root_generation == old && f.cart->storage_source == old->tree &&
              h->storage == old_side && old_side->records[0] == a && old_side->records[1] == b &&
              old->references == 2 &&
              mc_entity_storage_native_bytes(f.cart, &after, NULL) == MC_ENTITY_NBT_OK &&
              after == L && error.base_bytecode == 0 && !error.minecart_read.selected);
    }
    /* The final prepare request, not just tree validation, must fit. */
    mc_nbt_limits limits = mc_nbt_default_limits();
    limits.max_allocation_bytes = peak;
    CHECK(mc_entity_preserve_source(f.cart, &r.root, &limits, &error) == MC_ENTITY_NBT_OK);
    CHECK(f.cart->minecart->root_generation->references == 2 && h->storage != NULL &&
          h->storage->generation == f.cart->minecart->root_generation &&
          ms_long(h->storage->records[0], "Occurrence", 33));
    /* Do not compare the old owners after their last release. */
    old = f.cart->minecart->root_generation;
    old_side = h->storage;
    a = old_side->records[0];
    b = old_side->records[1];
    r.values[1][0].type = MC_NBT_SHORT;
    CHECK(mc_entity_preserve_source(f.cart, &r.root, NULL, &error) == MC_ENTITY_NBT_WRONG_TAG &&
          error.index == 1 && strcmp(error.field, "Slot") == 0);
    CHECK(f.cart->minecart->root_generation == old && h->storage == old_side &&
          old_side->records[0] == a && old_side->records[1] == b && old->references == 2);
    ok = true;
cleanup:
    mc_nbt_tree_free(copy);
    mc_item_stack_release(held);
    if (!ms_close(&f))
        ok = false;
    return ok;
}

static bool ms_generations(mc_entity_source side) {
    bool ok = false;
    ms_leaf f = {0};
    ms_record r, transition;
    mc_item_stack *held = NULL, *raw = NULL;
    mc_nbt_tree *tree = NULL;
    size_t actual = 0, expected = 0;
    mc_entity_nbt_error error;
    ++storage_cases;
    CHECK(ms_open(&f, side));
    ms_winners(&r);
    CHECK(ms_prepare_alias(&f, &r, &held));
    mc_minecart_stack_array *h = f.cart->minecart->fields.h;
    mc_minecart_storage_generation *G1 = h->storage->generation;
    const mc_nbt_tag *borrowed = mc_nbt_tree_root(G1->tree);
    ms_record_init(&transition, 2);
    transition.fields[MS_EXTRA].value.long_value = 444;
    transition.fields[MS_ITEMS].type = MC_NBT_INT; /* Type2 ignores this valid tag. */
    transition.fields[MS_ITEMS].value.int_value = 77;
    CHECK(mc_entity_read_nbt(f.cart, &transition.root, NULL) == MC_ENTITY_NBT_OK &&
          f.cart->minecart->fields.h == h &&
          mc_entity_preserve_source(f.cart, &transition.root, NULL, NULL) == MC_ENTITY_NBT_OK);
    CHECK(f.cart->minecart->root_generation != G1 && h->storage->generation == G1 &&
          G1->references == 1 && h->elements[0] == held && h->elements[1] == held);
    CHECK(ms_graph_bytes(f.cart, &expected) &&
          mc_entity_storage_native_bytes(f.cart, &actual, NULL) == MC_ENTITY_NBT_OK &&
          actual == expected);
    CHECK(ms_save(f.cart, &tree));
    const mc_nbt_tag *root = mc_nbt_tree_root(tree), *x = field(root, "PushX"),
                     *z = field(root, "PushZ"), *fuel = field(root, "Fuel");
    CHECK(field(root, "Items") == NULL && x != NULL && x->type == MC_NBT_DOUBLE &&
          x->value.double_bits == transition.fields[MS_X].value.double_bits && z != NULL &&
          z->type == MC_NBT_DOUBLE && z->value.double_bits == UINT64_C(0x8000000000000000) &&
          fuel != NULL && fuel->type == MC_NBT_SHORT && fuel->value.short_value == -7 &&
          ms_long(root, "RootExtra", 444) && ms_long(root, "Item", 222));
    mc_nbt_tree_free(tree);
    tree = NULL;
    ms_record_init(&transition, 0);
    transition.fields[MS_EXTRA].value.long_value = 555;
    CHECK(mc_entity_read_nbt(f.cart, &transition.root, NULL) == MC_ENTITY_NBT_OK &&
          mc_entity_preserve_source(f.cart, &transition.root, NULL, NULL) == MC_ENTITY_NBT_OK &&
          h->storage->generation == G1 && G1->references == 1 && ms_save(f.cart, &tree));
    root = mc_nbt_tree_root(tree);
    CHECK(field(root, "Items") == NULL && field(root, "PushX") == NULL &&
          field(root, "PushZ") == NULL && field(root, "Fuel") == NULL &&
          ms_long(root, "RootExtra", 555));
    mc_nbt_tree_free(tree);
    tree = NULL;
    mc_minecart_storage_generation *current = f.cart->minecart->root_generation;
    /* Input exists solely in the old h generation. The semantic reader must
     * keep that detached h and its sidecar alive through every borrowed read.
     * After return G1 and borrowed may be retired: neither is read again. */
    CHECK(mc_entity_read_nbt(f.cart, borrowed, &error) == MC_ENTITY_NBT_OK);
    h = f.cart->minecart->fields.h;
    CHECK(h->length == 27 && h->storage == NULL && f.cart->minecart->root_generation == current &&
          ms_save(f.cart, &tree));
    root = mc_nbt_tree_root(tree);
    CHECK(ms_known(root, 0, 3, 10) && field(ms_slot(root, 0), "Occurrence") == NULL &&
          ms_long(root, "RootExtra", 555));
    mc_nbt_tree_free(tree);
    tree = NULL;
    /* A new explicit preserve binds current winners, not the older raw Items. */
    CHECK(mc_entity_preserve_source(f.cart, &r.root, NULL, NULL) == MC_ENTITY_NBT_OK);
    const mc_nbt_tag *input = mc_entity_storage_source(f.cart);
    CHECK(mc_entity_preserve_source(f.cart, input, NULL, NULL) == MC_ENTITY_NBT_OK);
    CHECK(h->storage != NULL && h->storage->generation == f.cart->minecart->root_generation &&
          ms_long(h->storage->records[0], "Occurrence", 33));
    /* A separate caller-owned malformed input tests failed-read retirement of
     * an old sidecar, without mutating its immutable retained tree. */
    ms_record_init(&transition, 2);
    CHECK(mc_entity_read_nbt(f.cart, &transition.root, NULL) == MC_ENTITY_NBT_OK &&
          mc_entity_preserve_source(f.cart, &transition.root, NULL, NULL) == MC_ENTITY_NBT_OK);
    current = f.cart->minecart->root_generation;
    ms_winners(&r);
    r.values[1][3].type = MC_NBT_INT;
    CHECK(mc_entity_read_nbt(f.cart, &r.root, &error) == MC_ENTITY_NBT_STACK_FAILED &&
          error.minecart_read.bytecode == 137 && error.index == 1 && error.stack_bytecode == 24);
    h = f.cart->minecart->fields.h;
    CHECK(h->storage == NULL && f.cart->minecart->root_generation == current &&
          h->elements[0] != NULL && h->elements[1] == NULL);
    ms_winners(&r);
    CHECK(mc_entity_read_nbt(f.cart, &r.root, NULL) == MC_ENTITY_NBT_OK &&
          mc_entity_preserve_source(f.cart, &r.root, NULL, NULL) == MC_ENTITY_NBT_OK);
    h = f.cart->minecart->fields.h;
    current = f.cart->minecart->root_generation;
    CHECK(mc_item_stack_allocate(&raw) == MC_ITEM_STACK_OK &&
          mc_minecart_inventory_set(f.cart, 0, raw, NULL) == MC_MINECART_INVENTORY_OK);
    const mc_nbt_tag *b = h->storage->records[1];
    CHECK(mc_entity_preserve_source(f.cart, &r.root, NULL, &error) == MC_ENTITY_NBT_STACK_FAILED &&
          error.stack.status == MC_ITEM_STACK_INVALID_PHASE &&
          f.cart->minecart->root_generation == current && h->storage->records[0] == NULL &&
          h->storage->records[1] == b);
    /* Type2 does not read Items and admits a retained RAW occurrence. */
    ms_record_init(&transition, 2);
    CHECK(mc_entity_read_nbt(f.cart, &transition.root, NULL) == MC_ENTITY_NBT_OK &&
          mc_entity_preserve_source(f.cart, &transition.root, NULL, NULL) == MC_ENTITY_NBT_OK);
    const int32_t type = f.cart->minecart->fields.d;
    f.cart->minecart->fields.d = 1;
    const bool wrote = ms_save(f.cart, &tree);
    f.cart->minecart->fields.d = type;
    CHECK(wrote && ms_slot(mc_nbt_tree_root(tree), 0) != NULL &&
          field(ms_slot(mc_nbt_tree_root(tree), 0), "Occurrence") == NULL &&
          ms_long(ms_slot(mc_nbt_tree_root(tree), 1), "Occurrence", 22));
    mc_item_stack_phase phase;
    CHECK(mc_item_stack_phase_get(raw, &phase) == MC_ITEM_STACK_OK && phase == MC_ITEM_STACK_RAW);
    ok = true;
cleanup:
    mc_nbt_tree_free(tree);
    mc_item_stack_release(raw);
    mc_item_stack_release(held);
    if (!ms_close(&f))
        ok = false;
    return ok;
}

static bool ms_constructor_and_false(mc_entity_source side) {
    bool ok = false, saved = true;
    ms_leaf f = {0};
    mc_item_stack *raw = NULL, *failed = NULL;
    ms_record r;
    mc_nbt_tree *tree = NULL;
    mc_entity_nbt_error error;
    mc_nbt_limits tiny = mc_nbt_default_limits();
    mc_item_stack_error stack_error;
    mc_item_stack_phase phase;
    ++storage_cases;
    CHECK(ms_open(&f, side) && f.cart->minecart->fields.h->length == 36);
    CHECK(mc_item_stack_allocate(&raw) == MC_ITEM_STACK_OK &&
          mc_minecart_inventory_set(f.cart, 35, raw, NULL) == MC_MINECART_INVENTORY_OK);
    ms_record_init(&r, 1);
    ms_entry(&r, 0, 0, 7, 3, 88);
    r.values[0][3].type = MC_NBT_INT;
    CHECK(mc_item_stack_allocate(&failed) == MC_ITEM_STACK_OK);
    CHECK(mc_item_stack_construct_nbt(failed, &r.entries[0], &stack_error) ==
          MC_ITEM_STACK_INVALID_FIELD);
    CHECK(stack_error.status == MC_ITEM_STACK_INVALID_FIELD && stack_error.field != NULL &&
          strcmp(stack_error.field, "Damage") == 0 && stack_error.nbt.status == MC_NBT_OK &&
          stack_error.nbt.offset == 0);
    CHECK(mc_item_stack_phase_get(failed, &phase) == MC_ITEM_STACK_OK);
    CHECK(phase == MC_ITEM_STACK_FAILED_CONSTRUCTION);
    CHECK(mc_minecart_inventory_set(f.cart, 34, failed, NULL) == MC_MINECART_INVENTORY_OK);
    CHECK(ms_save(f.cart, &tree));
    const mc_nbt_tag *root = mc_nbt_tree_root(tree), *slot = ms_slot(root, 35);
    CHECK(slot != NULL && slot->value.compound.length == 4 && field(slot, "Occurrence") == NULL &&
          f.cart->minecart->fields.h->storage == NULL && f.cart->minecart->root_generation == NULL);
    const mc_nbt_tag *partial = ms_slot(root, 34), *partial_id = field(partial, "id"),
                     *partial_count = field(partial, "Count");
    CHECK(partial != NULL && field(partial, "Occurrence") == NULL && partial_id != NULL &&
          partial_id->value.short_value == 7 && partial_count != NULL &&
          partial_count->value.byte_value == 3 &&
          mc_item_stack_phase_get(failed, &phase) == MC_ITEM_STACK_OK &&
          phase == MC_ITEM_STACK_FAILED_CONSTRUCTION);
    tiny.max_allocation_bytes = 0;
    mc_nbt_tree *unchanged = tree;
    const bool dead = f.cart->base.be;
    f.cart->base.be = true;
    const mc_entity_nbt_status status =
        mc_entity_save_storage_nbt(f.cart, &tiny, &saved, &unchanged, &error);
    f.cart->base.be = dead;
    CHECK(status == MC_ENTITY_NBT_OK && !saved && unchanged == tree &&
          error.status == MC_ENTITY_NBT_OK);
    saved = true;
    CHECK(mc_entity_save_storage_nbt(f.cart, &tiny, &saved, &unchanged, &error) ==
              MC_ENTITY_NBT_LIMIT &&
          saved && unchanged == tree && f.cart->minecart->fields.h->elements[35] == raw);
    ok = true;
cleanup:
    mc_nbt_tree_free(tree);
    mc_item_stack_release(failed);
    mc_item_stack_release(raw);
    if (!ms_close(&f))
        ok = false;
    return ok;
}

static bool ms_seed_game(fixture *f, size_t slot) {
    bool ok = false;
    fixture_record *chunk = calloc(1, sizeof(*chunk));
    ms_record record;
    const mc_nbt_tag *entities[] = {&record.root};
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
    CHECK(chunk != NULL);
    ms_winners(&record);
    record.fields[MS_ITEMS].value.list.length = 5; /* No live slot2 in this GAME input. */
    chunk->root = (mc_nbt_tag){.type = MC_NBT_COMPOUND, .value.compound = {chunk->root_fields, 1}};
    chunk->level = (mc_nbt_tag){.type = MC_NBT_COMPOUND,
                                .name = ascii("Level", chunk->names[11]),
                                .value.compound = {chunk->level_fields, 11}};
    chunk->root_fields[0] = &chunk->level;
    for (size_t i = 0; i < COUNT(names); ++i) {
        chunk->fields[i].type = types[i];
        chunk->fields[i].name = ascii(names[i], chunk->names[i]);
        chunk->level_fields[i] = &chunk->fields[i];
    }
    for (size_t i = 0; i < COUNT(lengths); ++i) {
        chunk->fields[i + 3u].value.byte_array.data = chunk->arrays[i];
        chunk->fields[i + 3u].value.byte_array.length = lengths[i];
    }
    chunk->fields[8].value.byte_value = 1;
    chunk->fields[9].value.list.element_type = MC_NBT_COMPOUND;
    chunk->fields[9].value.list.items = entities;
    chunk->fields[9].value.list.length = 1;
    chunk->fields[10].value.list.element_type = MC_NBT_COMPOUND;
    CHECK(mc_nbt_measure(&chunk->root, NULL, &length, NULL) == MC_NBT_OK);
    encoded = malloc(length);
    CHECK(encoded != NULL &&
          mc_nbt_write(&chunk->root, NULL, encoded, length, &written, NULL) == MC_NBT_OK &&
          written == length);
    uint32_t growth = UINT32_MAX;
    CHECK(mc_world_regions_write(f->regions, mc_world_store_root(f->stores[slot]), 0, 0, encoded,
                                 length, fixture_timestamp, &growth, NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    free(encoded);
    free(chunk);
    return ok;
}

static bool ms_game_hold(fixture *f, size_t slot, mc_entity **out) {
    mc_entity *found = NULL;
    for (size_t slice = 0; slice < CHUNK_ENTITY_SLICES; ++slice)
        for (size_t i = 0; i < f->chunks[slot]->entities[slice].count; ++i) {
            mc_entity *v = NULL;
            if (mc_world_chunk_entity_at(&f->chunks[slot]->entities[slice], i, &v) !=
                    MC_WORLD_STORAGE_OK ||
                v == NULL || !mc_entity_is_kind(v, MC_ENTITY_CLASS_KIND_MINECART) || found != NULL)
                return false;
            found = v;
        }
    if (found == NULL || mc_entity_retain(found) != MC_ENTITY_OK)
        return false;
    *out = found;
    return true;
}

static bool ms_game_roundtrip(fixture *f, size_t side) {
    bool ok = false;
    mc_entity *old = NULL, *loaded = NULL;
    mc_item_stack *held = NULL, *reload0 = NULL, *reload1 = NULL;
    ge_disk disk = {0};
    const size_t target = side + 2u;
    mc_entity_nbt_error error;
    ++storage_cases;
    CHECK(ms_seed_game(f, side) && ge_load(f, side, 1) && ms_game_hold(f, side, &old));
    CHECK(old->base.world == f->worlds[side] && old->minecart->fields.h->length == 27 &&
          old->minecart->root_generation != NULL && old->minecart->fields.h->storage != NULL);
    const mc_nbt_tag *archived = mc_entity_storage_source(old);
    CHECK(field(archived, "Items")->value.list.length == 5 &&
          mc_minecart_inventory_get(old, 0, &held, NULL) == MC_MINECART_INVENTORY_OK &&
          held != NULL &&
          mc_minecart_inventory_set(old, 1, held, NULL) == MC_MINECART_INVENTORY_OK &&
          mc_entity_preserve_source(old, archived, NULL, &error) == MC_ENTITY_NBT_OK &&
          ge_write(f, side));
    CHECK(ge_read(f, side, &disk));
    const mc_nbt_tag *level = field(mc_nbt_document_root(disk.document), "Level"),
                     *list = field(level, "Entities");
    CHECK(list != NULL && list->type == MC_NBT_LIST && list->value.list.length == 1);
    const mc_nbt_tag *record = list->value.list.items[0], *items = field(record, "Items"),
                     *id = field(record, "id");
    CHECK(id != NULL && id->type == MC_NBT_STRING && ge_text(id->value.string, "Minecart") &&
          ms_long(record, "RootExtra", 111) && ms_long(record, "Item", 222) && items != NULL &&
          items->value.list.length == 2 && ms_known(record, 0, 3, 10) &&
          ms_known(record, 1, 3, 10) && ms_long(ms_slot(record, 0), "Occurrence", 33) &&
          ms_long(ms_slot(record, 1), "Occurrence", 22) && ms_slot(record, 27) == NULL);
    uint32_t growth = UINT32_MAX;
    CHECK(mc_world_regions_write(f->regions, mc_world_store_root(f->stores[target]), 0, 0,
                                 disk.bytes.data, disk.bytes.length, fixture_timestamp, &growth,
                                 NULL) == MC_WORLD_STORAGE_OK &&
          ge_load(f, target, 1) && ms_game_hold(f, target, &loaded));
    CHECK(loaded != old && loaded->base.world == f->worlds[target] &&
          loaded->base.world != old->base.world && loaded->minecart->fields.h->length == 27 &&
          mc_entity_storage_source(loaded) != NULL &&
          mc_minecart_inventory_get(loaded, 0, &reload0, NULL) == MC_MINECART_INVENTORY_OK &&
          mc_minecart_inventory_get(loaded, 1, &reload1, NULL) == MC_MINECART_INVENTORY_OK &&
          reload0 != NULL && reload1 != NULL && reload0 != reload1 && reload0 != held &&
          reload1 != held);
    CHECK(ms_long(loaded->minecart->fields.h->storage->records[0], "Occurrence", 33) &&
          ms_long(loaded->minecart->fields.h->storage->records[1], "Occurrence", 22) &&
          ge_write(f, target));
    ge_disk_free(&disk);
    CHECK(ge_read(f, target, &disk));
    list = field(field(mc_nbt_document_root(disk.document), "Level"), "Entities");
    CHECK(list != NULL && list->value.list.length == 1 &&
          ms_long(ms_slot(list->value.list.items[0], 0), "Occurrence", 33) &&
          ms_long(ms_slot(list->value.list.items[0], 1), "Occurrence", 22));
    ok = true;
cleanup:
    ge_disk_free(&disk);
    mc_item_stack_release(reload1);
    mc_item_stack_release(reload0);
    mc_item_stack_release(held);
    mc_entity_release(loaded);
    mc_entity_release(old);
    return ok;
}

static bool ms_staged_native_failure(fixture *f, size_t side) {
    bool ok = false, entered = false, begun = false;
    mc_entity *cart = NULL;
    mc_world_store_game_stage *stage = NULL;
    mc_world_store_game_stage_view view = {0};
    mc_entity_nbt_error provenance;
    mc_world_chunk_entity_error insertion;
    mc_world_runtime_acquisition_error retry_error;
    mc_entity_class_snapshot before, after;
    ++storage_cases;
    CHECK(ms_seed_game(f, side) &&
          mc_world_runtime_call_enter(f->worlds[side], NULL) == MC_WORLD_STORAGE_OK);
    entered = true;
    CHECK(mc_world_store_game_stage_begin(f->stores[side], f->worlds[side], 0, 0, 0, &stage,
                                          NULL) == MC_WORLD_STORAGE_OK);
    begun = true;
    CHECK(mc_world_store_game_stage_view_get(f->stores[side], stage, &view, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_store_game_stage_imports_begin(f->stores[side], stage, NULL) ==
              MC_WORLD_STORAGE_OK);
    f->states[side] = view.state;
    f->chunks[side] = view.chunk;
    const mc_nbt_tag *list = field(view.level, "Entities");
    CHECK(list != NULL && list->type == MC_NBT_LIST && list->value.list.length == 1 &&
          mc_entity_factory_nbt(f->entity_classes[side], f->worlds[side], list->value.list.items[0],
                                &cart, &provenance) == MC_ENTITY_NBT_OK &&
          cart != NULL &&
          mc_world_chunk_insert_entity(view.chunk, cart, &insertion) == MC_WORLD_STORAGE_OK);
    /* Explicit native staged-prefix control: this operation ceiling is NOT a
     * changed Store decoder limit or an injected full-loader failure. */
    mc_nbt_limits tiny = mc_nbt_default_limits();
    tiny.max_allocation_bytes = 0;
    CHECK(mc_entity_preserve_source(cart, list->value.list.items[0], &tiny, &provenance) ==
          MC_ENTITY_NBT_LIMIT);
    size_t occurrences = 0;
    for (size_t i = 0; i < CHUNK_ENTITY_SLICES; ++i)
        occurrences += view.chunk->entities[i].count;
    CHECK(occurrences == 1 && cart->holds == 2 && mc_entity_storage_source(cart) == NULL &&
          cart->minecart->fields.h->storage == NULL && f->worlds[side]->fields.b->count == 0 &&
          mc_world_store_game_stage_fail(f->stores[side], stage, MC_WORLD_STORAGE_LIMIT, NULL) ==
              MC_WORLD_STORAGE_LIMIT);
    begun = false;
    mc_entity_release(cart);
    cart = NULL;
    mc_world_runtime_call_leave(f->worlds[side]);
    entered = false;
    CHECK(mc_world_store_game_stage_view_get(f->stores[side], stage, &view, NULL) ==
              MC_WORLD_STORAGE_OK &&
          view.phase == MC_WORLD_STORE_GAME_FAILED && !mc_world_chunk_source_enabled(view.chunk) &&
          !view.state->active && f->worlds[side]->fields.b->count == 0 &&
          ms_game_hold(f, side, &cart) && cart->holds == 2 &&
          mc_entity_storage_source(cart) == NULL &&
          mc_entity_classes_get_snapshot(f->entity_classes[side], &before) == MC_ENTITY_OK);
    mc_chunk_state sentinel = {0}, *out = &sentinel;
    CHECK(mc_world_runtime_get_detailed(f->worlds[side], 0, 0, &out, &retry_error) !=
              MC_CHUNK_ACQUIRE_OK &&
          out == &sentinel &&
          mc_entity_classes_get_snapshot(f->entity_classes[side], &after) == MC_ENTITY_OK &&
          before.next_id == after.next_id && before.live_entities == after.live_entities &&
          cart->holds == 2 && f->providers[side].cache->count == 0 &&
          mc_world_store_game_stage_count(f->stores[side]) == 1);
    ok = true;
cleanup:
    if (begun)
        (void)mc_world_store_game_stage_fail(f->stores[side], stage, MC_WORLD_STORAGE_LIMIT, NULL);
    mc_entity_release(cart);
    if (entered)
        mc_world_runtime_call_leave(f->worlds[side]);
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char base[8192], root[8192];
    if (argc != 3 || strcmp(argv[1], "entity-minecart-storage") != 0 ||
        !decode_path(argv[2], base, sizeof(base)) || !mc_math_trig_init(&trig))
        return 1;
    for (size_t side = 0; side < 2; ++side) {
        const mc_entity_source source = side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER;
        CHECK(ms_semantic_peaks(source) && ms_alias_occurrences(source) &&
              ms_atomic_peaks(source) && ms_generations(source) &&
              ms_constructor_and_false(source));
    }
    for (unsigned int epoch = 0; epoch < 2; ++epoch) {
        const int n = snprintf(root, sizeof(root), "%s/storage%u", base, epoch);
        CHECK(n > 0 && (size_t)n < sizeof(root) && ge_setup(&f, root));
        for (size_t side = 0; side < 2; ++side)
            CHECK(epoch == 0 ? ms_game_roundtrip(f, side) : ms_staged_native_failure(f, side));
        if (!cleanup_fixture(f)) {
            fputs("Minecart storage cleanup refused; live owners retained\n", stderr);
            return 1;
        }
        f = NULL;
    }
    CHECK(storage_cases == 14);
    printf("Minecart storage: %u checks, %u source cases\n", checks, storage_cases);
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("Minecart storage cleanup refused; live owners retained\n", stderr);
        return 1;
    }
    return result;
}
