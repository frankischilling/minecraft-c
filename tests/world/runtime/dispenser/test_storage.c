/* Authored CPU controls for the actual AZ owner. No original-runtime, GUI,
 * firing, redstone or gameplay parity result is supplied by this program. */
#include "../../../../src/core/java/random_profile_internal.h"
#include "../../../../src/inventory/dispenser_internal.h"
#include "../../../../src/world/runtime/tile_entities/tile_entities_internal.h"
#include "mc/resources/files/files.h"
#include "mc/statistics/registry/registry.h"
#include "mc/world/runtime/construction.h"
#include "mc/world/runtime/tile_entities/dispenser.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #value);                            \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static mc_nbt_string ascii(const char *text, uint16_t *units) {
    const size_t n = strlen(text);
    for (size_t i = 0; i < n; ++i)
        units[i] = (uint16_t)(unsigned char)text[i];
    return (mc_nbt_string){units, n};
}

typedef struct record {
    mc_nbt_tag root, fields[5], entries[4], items[4][5];
    const mc_nbt_tag *roots[5], *records[4], *children[4][5];
    uint16_t names[5][8], item_names[5][12], id[4];
} record;

static void prepare_record(record *r) {
    *r = (record){0};
    const char *keys[] = {"id", "x", "y", "z", "Items"};
    const char *item_keys[] = {"Slot", "id", "Count", "Damage", "Extra"};
    const mc_nbt_type item_types[] = {MC_NBT_BYTE, MC_NBT_SHORT, MC_NBT_BYTE, MC_NBT_SHORT,
                                      MC_NBT_LONG};
    r->root.type = MC_NBT_COMPOUND;
    r->root.value.compound.items = r->roots;
    r->root.value.compound.length = 5;
    for (size_t i = 0; i < 5; ++i) {
        r->fields[i].name = ascii(keys[i], r->names[i]);
        r->roots[i] = &r->fields[i];
    }
    r->fields[0].type = MC_NBT_STRING;
    r->fields[0].value.string = ascii("Trap", r->id);
    for (size_t i = 1; i < 4; ++i) {
        r->fields[i].type = MC_NBT_INT;
        r->fields[i].value.int_value = (int32_t)(i * 11u);
    }
    r->fields[4].type = MC_NBT_LIST;
    r->fields[4].value.list.element_type = MC_NBT_COMPOUND;
    r->fields[4].value.list.items = r->records;
    r->fields[4].value.list.length = 4;
    for (size_t i = 0; i < 5; ++i)
        (void)ascii(item_keys[i], r->item_names[i]);
    for (size_t n = 0; n < 4; ++n) {
        r->entries[n].type = MC_NBT_COMPOUND;
        r->entries[n].value.compound.items = r->children[n];
        r->entries[n].value.compound.length = 5;
        r->records[n] = &r->entries[n];
        for (size_t i = 0; i < 5; ++i) {
            r->items[n][i].name = (mc_nbt_string){r->item_names[i], strlen(item_keys[i])};
            r->items[n][i].type = item_types[i];
            r->children[n][i] = &r->items[n][i];
        }
        r->items[n][0].value.byte_value = (int8_t)(n == 3 ? -1 : (int32_t)n);
        r->items[n][1].value.short_value = (int16_t)(1u + n);
        r->items[n][2].value.byte_value = (int8_t)(70u + n);
        r->items[n][3].value.short_value = (int16_t)(90u + n);
        r->items[n][4].value.long_value = INT64_C(0x102030405060708);
    }
}

static const mc_nbt_tag *field(const mc_nbt_tag *compound, const char *key) {
    uint16_t name[16];
    const mc_nbt_string s = ascii(key, name);
    return mc_nbt_compound_get(compound, s.units, s.length);
}

static bool stack_is(const mc_item_stack *stack, int32_t id, int32_t count, int32_t damage) {
    mc_item_stack_fields value;
    return stack != NULL && mc_item_stack_fields_get(stack, &value) == MC_ITEM_STACK_OK &&
           value.id == id && value.count == count && value.damage == damage;
}

static bool controls(mc_world_runtime_classes *classes, mc_java_random_profile *profile,
                     const mc_java_random_constructor_record *records) {
    bool ok = false;
    mc_tile_entity *tile = NULL, *twin = NULL;
    mc_inventory_dispenser *inventory = NULL, *limited = NULL;
    mc_item_stack *a = NULL, *held = NULL, *removed = NULL;
    mc_nbt_document *saved = NULL;
    mc_nbt_tree *source = NULL;
    mc_tile_entity_error e;
    mc_inventory_error ie = {0};
    mc_tile_entity_snapshot state;
    mc_java_random_constructor_records_view before, after;
    record r;
    CHECK(mc_java_random_profile_constructor_records_get(profile, &before) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          before.failures == 0);
    CHECK(mc_tile_entity_allocate_in_classes(classes, MC_TILE_ENTITY_DISPENSER, &tile) ==
              MC_TILE_ENTITY_OK &&
          mc_tile_entity_construct_detailed(tile, &e) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_get_snapshot(tile, &state) == MC_TILE_ENTITY_OK &&
          state.kind == MC_TILE_ENTITY_DISPENSER && state.dispenser_capacity == 9 &&
          state.dispenser_random_constructed && tile->state.dispenser.constructor_random == NULL);
    CHECK(mc_java_random_profile_constructor_records_get(profile, &after) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          after.recorded == before.recorded + 1 && after.failures == 0 &&
          records[before.recorded].kind == MC_JAVA_RANDOM_CONSTRUCTOR_ORDINARY &&
          records[before.recorded].sample_returned && records[before.recorded].tail_returned &&
          records[before.recorded].destination == (uintptr_t)tile->state.dispenser.random);
    mc_java_random *const original_random = tile->state.dispenser.random;
    CHECK(mc_tile_entity_allocate_in_classes(classes, MC_TILE_ENTITY_DISPENSER, &twin) ==
              MC_TILE_ENTITY_OK &&
          mc_tile_entity_construct(twin) == MC_TILE_ENTITY_OK);
    const mc_tile_entity_class_literal *one = NULL, *two = NULL;
    mc_tile_entity_class_literal_view class_view;
    CHECK(mc_tile_entity_get_class(tile, &one) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_get_class(twin, &two) == MC_TILE_ENTITY_OK && one == two &&
          mc_tile_entity_class_literal_inspect(one, &class_view) == MC_TILE_ENTITY_OK &&
          class_view.kind == MC_TILE_ENTITY_CLASS_KIND_DISPENSER &&
          strcmp(class_view.client_name, "az") == 0);
    CHECK(mc_inventory_dispenser_from_tile(MC_REGISTRY_CLIENT, tile, NULL, &inventory, &ie) ==
          MC_INVENTORY_OK);
    int32_t size = -1, limit = -1;
    const char *name = NULL;
    CHECK(mc_inventory_dispenser_size(inventory, &size, &ie) == MC_INVENTORY_OK && size == 9 &&
          mc_inventory_dispenser_limit(inventory, &limit, &ie) == MC_INVENTORY_OK && limit == 64 &&
          mc_inventory_dispenser_name(inventory, &name, &ie) == MC_INVENTORY_OK &&
          strcmp(name, "Trap") == 0);
    for (int32_t i = 0; i < 9; ++i) {
        CHECK(mc_inventory_dispenser_get(inventory, i, &held, &ie) == MC_INVENTORY_OK &&
              held == NULL);
    }
    CHECK(mc_item_stack_allocate(&a) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(a, 1, 95, 7) == MC_ITEM_STACK_OK &&
          mc_inventory_dispenser_set(inventory, 0, a, &ie) == MC_INVENTORY_OK &&
          stack_is(a, 1, 64, 7) &&
          mc_inventory_dispenser_set(inventory, 8, a, &ie) == MC_INVENTORY_OK &&
          mc_inventory_dispenser_get(inventory, 8, &held, &ie) == MC_INVENTORY_OK && held == a);
    mc_item_stack_release(held);
    held = NULL;
    CHECK(mc_item_stack_count_set(a, 5) == MC_ITEM_STACK_OK &&
          mc_inventory_dispenser_split(inventory, 8, -2, &removed, &ie) == MC_INVENTORY_OK &&
          removed != a && stack_is(removed, 1, -2, 7) && stack_is(a, 1, 7, 7));
    mc_item_stack_release(removed);
    removed = NULL;
    CHECK(mc_inventory_dispenser_split(inventory, 8, 0, &removed, &ie) == MC_INVENTORY_OK &&
          removed != a && stack_is(removed, 1, 0, 7) && stack_is(a, 1, 7, 7));
    mc_item_stack_release(removed);
    removed = NULL;
    CHECK(mc_item_stack_count_set(a, -4) == MC_ITEM_STACK_OK &&
          mc_inventory_dispenser_split(inventory, 8, -3, &removed, &ie) == MC_INVENTORY_OK &&
          removed == a && stack_is(a, 1, -4, 7));
    mc_item_stack_release(removed);
    removed = NULL;
    CHECK(mc_inventory_dispenser_get(inventory, 0, &held, &ie) == MC_INVENTORY_OK && held == a);
    mc_item_stack_release(held);
    held = NULL;
    CHECK(mc_inventory_dispenser_set(inventory, 9, NULL, &ie) == MC_INVENTORY_INDEX_OUT_OF_BOUNDS &&
          ie.source_exception && ie.frames[0].pc == 6 &&
          mc_inventory_dispenser_get(inventory, -1, &held, &ie) ==
              MC_INVENTORY_INDEX_OUT_OF_BOUNDS &&
          held == NULL);
    CHECK(mc_inventory_dispenser_changed(inventory, &ie) == MC_INVENTORY_OK);
    bool usable = true;
    CHECK(mc_inventory_dispenser_usable(inventory, NULL, &usable, &ie) ==
              MC_INVENTORY_NULL_REFERENCE &&
          ie.source_exception && ie.frames[0].pc == 16 && usable);

    prepare_record(&r);
    r.fields[2].type = MC_NBT_SHORT;
    CHECK(mc_tile_entity_read_nbt(tile, &r.root, &e) == MC_TILE_ENTITY_INVALID_FIELD &&
          e.base_read_bytecode == 14 && e.read_bytecode == 2 &&
          mc_tile_entity_get_snapshot(tile, &state) == MC_TILE_ENTITY_OK && state.x == 11 &&
          state.y == 0);
    r.fields[2].type = MC_NBT_INT;
    r.fields[4].type = MC_NBT_BYTE;
    CHECK(mc_tile_entity_read_nbt(tile, &r.root, &e) == MC_TILE_ENTITY_INVALID_FIELD &&
          e.read_bytecode == 8 &&
          mc_tile_entity_dispenser_get(tile, 0, &held) == MC_TILE_ENTITY_OK && held == a);
    mc_item_stack_release(held);
    held = NULL;
    prepare_record(&r);
    r.items[0][0].value.byte_value = 8;
    r.entries[1].type = MC_NBT_INT;
    CHECK(mc_tile_entity_read_nbt(tile, &r.root, &e) == MC_TILE_ENTITY_INVALID_FIELD &&
          e.read_bytecode == 38 && e.item_index == 1 &&
          mc_tile_entity_dispenser_get(tile, 0, &held) == MC_TILE_ENTITY_OK && held == NULL &&
          mc_tile_entity_dispenser_get(tile, 8, &held) == MC_TILE_ENTITY_OK &&
          stack_is(held, 1, 70, 90));
    mc_item_stack_release(held);
    held = NULL;
    prepare_record(&r);
    r.items[1][0].type = MC_NBT_SHORT;
    CHECK(mc_tile_entity_read_nbt(tile, &r.root, &e) == MC_TILE_ENTITY_INVALID_FIELD &&
          e.read_bytecode == 47 && e.item_index == 1 &&
          mc_tile_entity_dispenser_get(tile, 0, &held) == MC_TILE_ENTITY_OK &&
          stack_is(held, 1, 70, 90));
    mc_item_stack_release(held);
    held = NULL;
    prepare_record(&r);
    r.items[1][1].type = MC_NBT_BYTE;
    CHECK(mc_tile_entity_read_nbt(tile, &r.root, &e) == MC_TILE_ENTITY_INVALID_FIELD &&
          e.read_bytecode == 83 && e.item_index == 1 &&
          e.item.status == MC_ITEM_STACK_INVALID_FIELD);
    prepare_record(&r);
    r.items[0][0].value.byte_value = 8;
    r.items[1][0].value.byte_value = 8; /* Duplicate winning slot. */
    r.items[2][0].value.byte_value = 9;
    r.items[2][1].type = MC_NBT_BYTE; /* Invalid slot skips this wrong Stack tag. */
    r.entries[3].value.compound.items = &r.children[3][1]; /* Missing Slot defaults0. */
    r.entries[3].value.compound.length = 4;
    CHECK(mc_tile_entity_read_nbt(tile, &r.root, &e) == MC_TILE_ENTITY_OK &&
          tile->state.dispenser.random == original_random &&
          mc_tile_entity_dispenser_get(tile, 8, &held) == MC_TILE_ENTITY_OK &&
          stack_is(held, 2, 71, 91));
    mc_item_stack_release(held);
    held = NULL;
    CHECK(mc_tile_entity_dispenser_get(tile, 0, &held) == MC_TILE_ENTITY_OK &&
          stack_is(held, 4, 73, 93));
    mc_item_stack_release(held);
    held = NULL;
    CHECK(mc_tile_entity_build(tile, NULL, &saved, &e) == MC_TILE_ENTITY_OK);
    const mc_nbt_tag *root = mc_nbt_document_root(saved), *items = field(root, "Items");
    CHECK(items != NULL && items->type == MC_NBT_LIST && items->value.list.length == 2 &&
          items->value.list.element_type == MC_NBT_COMPOUND &&
          field(root, "x")->type == MC_NBT_INT &&
          field(items->value.list.items[0], "Slot")->type == MC_NBT_BYTE &&
          field(items->value.list.items[0], "Count")->type == MC_NBT_BYTE &&
          field(items->value.list.items[0], "id")->type == MC_NBT_SHORT &&
          field(items->value.list.items[0], "Damage")->type == MC_NBT_SHORT &&
          field(items->value.list.items[0], "Extra") == NULL);
    mc_nbt_document_free(saved);
    saved = NULL;
    /* Ordinary Source Chunk saves use the fresh semantic writer, independently
     * of the provenance-overlay document builder above. */
    const mc_tile_entity_status source_status =
        mc_tile_entity_build_source_nbt(tile, NULL, &source, &e);
    if (source_status != MC_TILE_ENTITY_OK)
        fprintf(stderr, "AZ_STORAGE_SOURCE status=%d nbt=%d save_pc=%u base_save_pc=%u\n",
                (int)source_status, (int)e.nbt.status, e.save_bytecode, e.base_save_bytecode);
    CHECK(source_status == MC_TILE_ENTITY_OK && source != NULL);
    const mc_nbt_tag *source_items = field(mc_nbt_tree_root(source), "Items");
    CHECK(source_items != NULL && source_items->type == MC_NBT_LIST &&
          source_items->value.list.element_type == MC_NBT_COMPOUND &&
          source_items->value.list.length == 2 &&
          field(source_items->value.list.items[0], "Slot")->value.byte_value == 0 &&
          field(source_items->value.list.items[0], "Count")->value.byte_value == 73 &&
          field(source_items->value.list.items[1], "Slot")->value.byte_value == 8 &&
          field(source_items->value.list.items[1], "Count")->value.byte_value == 71);
    mc_nbt_tree_free(source);
    source = NULL;
    prepare_record(&r);
    r.root.value.compound.length = 4; /* Missing Items replaces the array with nine empties. */
    const mc_tile_entity_status empty_read = mc_tile_entity_read_nbt(tile, &r.root, &e);
    if (empty_read != MC_TILE_ENTITY_OK)
        fprintf(stderr, "AZ_STORAGE_EMPTY read=%d field=%s base_pc=%u read_pc=%u\n",
                (int)empty_read, e.field == NULL ? "NULL" : e.field, e.base_read_bytecode,
                e.read_bytecode);
    CHECK(empty_read == MC_TILE_ENTITY_OK);
    const mc_tile_entity_status empty_build = mc_tile_entity_build(tile, NULL, &saved, &e);
    if (empty_build != MC_TILE_ENTITY_OK)
        fprintf(stderr, "AZ_STORAGE_EMPTY build=%d field=%s save_pc=%u base_save_pc=%u\n",
                (int)empty_build, e.field == NULL ? "NULL" : e.field, e.save_bytecode,
                e.base_save_bytecode);
    CHECK(empty_build == MC_TILE_ENTITY_OK && saved != NULL);
    const mc_nbt_tag *empty_items = field(mc_nbt_document_root(saved), "Items");
    CHECK(empty_items != NULL && empty_items->type == MC_NBT_LIST);
    /* This API encodes and reads back the document. Original SP's writer sets
     * empty-list type1 at35..37; the shared codec emits the same BYTE type. */
    if (empty_items->value.list.length != 0 || empty_items->value.list.element_type != MC_NBT_BYTE)
        fprintf(stderr, "AZ_STORAGE_EMPTY length=%zu element_type=%u expected_type=%u\n",
                empty_items->value.list.length, (unsigned int)empty_items->value.list.element_type,
                (unsigned int)MC_NBT_BYTE);
    CHECK(empty_items->value.list.length == 0);
    CHECK(empty_items->value.list.element_type == MC_NBT_BYTE);
    CHECK(mc_tile_entity_build_source_nbt(tile, NULL, &source, &e) == MC_TILE_ENTITY_OK &&
          source != NULL);
    const mc_nbt_tag *empty_source_items = field(mc_nbt_tree_root(source), "Items");
    CHECK(empty_source_items != NULL && empty_source_items->type == MC_NBT_LIST &&
          empty_source_items->value.list.length == 0 &&
          empty_source_items->value.list.element_type == MC_NBT_END);
    mc_nbt_tree_free(source);
    source = NULL;
    mc_nbt_document_free(saved);
    saved = NULL;
    CHECK(mc_java_random_profile_constructor_records_get(profile, &after) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          after.recorded == before.recorded + 2 && after.failures == 0 &&
          tile->state.dispenser.random == original_random);
    const mc_inventory_limits tiny = {256, 16};
    CHECK(mc_inventory_dispenser_from_tile(MC_REGISTRY_CLIENT, tile, &tiny, &limited, &ie) ==
              MC_INVENTORY_LIMIT &&
          limited == NULL);
    CHECK(inv_dispenser_terminal_admit(inventory, false, &ie) == MC_INVENTORY_OK);
    inv_dispenser_terminal_seal(inventory);
    CHECK(mc_inventory_dispenser_set(inventory, 0, a, &ie) == MC_INVENTORY_BUSY &&
          mc_inventory_dispenser_retain(inventory, &ie) == MC_INVENTORY_BUSY &&
          mc_inventory_dispenser_retire(&inventory, &ie) == MC_INVENTORY_OK && inventory == NULL);
    ok = true;
cleanup:
    if (mc_inventory_dispenser_retire(&inventory, NULL) != MC_INVENTORY_OK)
        ok = false;
    if (mc_inventory_dispenser_retire(&limited, NULL) != MC_INVENTORY_OK)
        ok = false;
    mc_item_stack_release(held);
    mc_item_stack_release(removed);
    mc_item_stack_release(a);
    mc_nbt_document_free(saved);
    mc_nbt_tree_free(source);
    mc_tile_entity_release(tile);
    mc_tile_entity_release(twin);
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    mc_resource_files *files = NULL;
    mc_materials *materials = NULL;
    mc_java_random_profile *profile = NULL;
    mc_registry_classes *registry = NULL;
    mc_world_runtime_classes *classes = NULL;
    static mc_math_trig trig;
    mc_java_random_constructor_record records[256];
    if (argc != 2 || !mc_math_trig_init(&trig) ||
        mc_resource_files_create(argv[1], &files) != MC_REGISTRY_OK ||
        !mc_materials_create(&materials) ||
        mc_java_random_profile_create(SIZE_MAX, &profile) != MC_JAVA_RANDOM_PROFILE_OK ||
        mc_java_random_profile_bind_constructor_records(profile, records, 256) !=
            MC_JAVA_RANDOM_PROFILE_OK)
        goto cleanup;
    const mc_registry_options options = {MC_REGISTRY_CLIENT, profile, materials, 0,
                                         mc_resource_files_source(files)};
    if (mc_registry_classes_allocate(&options, &registry, NULL) != MC_REGISTRY_OK ||
        mc_registry_classes_initialize_statistics(registry, NULL) != MC_REGISTRY_OK ||
        mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, profile, &trig, &classes, NULL) !=
            MC_WORLD_STORAGE_OK ||
        mc_world_runtime_classes_bind_registry(classes, registry, NULL) != MC_REGISTRY_OK ||
        !controls(classes, profile, records))
        goto cleanup;
    printf("Dispenser storage CPU controls passed: %u\n", checks);
    result = 0;
cleanup:
    if (classes != NULL && mc_world_runtime_classes_free(classes, NULL) != MC_WORLD_STORAGE_OK)
        result = 1;
    if (registry != NULL && mc_registry_classes_free(registry, NULL) != MC_REGISTRY_OK)
        result = 1;
    if (files != NULL && mc_resource_files_free(files) != MC_REGISTRY_OK)
        result = 1;
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
    return result;
}
