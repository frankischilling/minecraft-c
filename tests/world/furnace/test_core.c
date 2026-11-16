/* Same genuine Registry/World fixture; its imported main is never executed.
 * Original SK state/alias expectations come from the independent JAR packet. */
#define main mc_furnace_construction_unused_main
#include "../../entity/player/test_construction.c"
#undef main

#include "../../reference/inventory/furnace/native-observations.h"
#include "item/stack/source_internal.h"
#include "mc/world/runtime/tile_entities/furnace.h"
#include "world/runtime/runtime_internal.h"
#include "world/runtime/tile_entities/tile_entities_internal.h"
#include "world/storage/generated_internal.h"

typedef struct furnace_labels {
    const char *names[24];
    mc_item_stack *values[24];
    size_t count;
} furnace_labels;

static bool furnace_identity(furnace_labels *labels, const mc_furnace_stack_reference *expected,
                             mc_item_stack *actual) {
    if (expected->identity == NULL) {
        CHECK(actual == NULL);
        return true;
    }
    CHECK(actual != NULL);
    size_t i = 0;
    while (i < labels->count && strcmp(labels->names[i], expected->identity) != 0)
        ++i;
    if (i == labels->count) {
        CHECK(i < 24);
        for (size_t j = 0; j < i; ++j)
            CHECK(labels->values[j] != actual);
        labels->names[i] = expected->identity;
        labels->values[i] = actual;
        ++labels->count;
    } else
        CHECK(labels->values[i] == actual);
    mc_item_stack_fields fields;
    CHECK(mc_item_stack_fields_get(actual, &fields) == MC_ITEM_STACK_OK &&
          fields.id == expected->id && fields.count == expected->count &&
          fields.damage == expected->damage && fields.animation == expected->animation);
    return true;
}

static mc_item_stack *furnace_label(furnace_labels *labels, const char *name) {
    if (name == NULL)
        return NULL;
    for (size_t i = 0; i < labels->count; ++i)
        if (strcmp(labels->names[i], name) == 0)
            return labels->values[i];
    return NULL;
}

static const mc_nbt_tag *furnace_field(const mc_nbt_tag *root, const char *name) {
    if (root == NULL || root->type != MC_NBT_COMPOUND)
        return NULL;
    for (size_t i = 0; i < root->value.compound.length; ++i) {
        const mc_nbt_tag *tag = root->value.compound.items[i];
        if (tag->name.length != strlen(name))
            continue;
        size_t j = 0;
        while (j < tag->name.length && tag->name.units[j] == (unsigned char)name[j])
            ++j;
        if (j == tag->name.length)
            return tag;
    }
    return NULL;
}

static mc_nbt_string furnace_name(const char *name, uint16_t *units) {
    const size_t count = strlen(name);
    for (size_t i = 0; i < count; ++i)
        units[i] = (unsigned char)name[i];
    return (mc_nbt_string){units, count};
}

static mc_tile_entity_status furnace_read_input(mc_tile_entity *tile, const mc_furnace_reference *v,
                                                mc_tile_entity_error *error) {
    mc_nbt_tag fields[6] = {0}, records[8] = {0}, cells[8][4] = {0};
    const mc_nbt_tag *children[6], *items[8], *record_cells[8][4];
    uint16_t field_names[6][16], cell_names[8][4][16];
    const char *names[] = {"x", "y", "z", "Items", "BurnTime", "CookTime"};
    for (size_t i = 0; i < 6; ++i) {
        fields[i].name = furnace_name(names[i], field_names[i]);
        children[i] = &fields[i];
    }
    for (size_t i = 0; i < 3; ++i) {
        fields[i].type = MC_NBT_INT;
        fields[i].value.int_value = v->nbt_xyz[i];
    }
    for (size_t i = 0; i < v->nbt_item_count; ++i) {
        const char *keys[] = {"Slot", "id", "Count", "Damage"};
        for (size_t j = 0; j < 4; ++j) {
            cells[i][j].name = furnace_name(keys[j], cell_names[i][j]);
            cells[i][j].type = j == 0 || j == 2 ? MC_NBT_BYTE : MC_NBT_SHORT;
            if (cells[i][j].type == MC_NBT_BYTE)
                cells[i][j].value.byte_value = mc_java_i8_narrow(v->nbt_items[i][j]);
            else
                cells[i][j].value.short_value = mc_java_i16_narrow(v->nbt_items[i][j]);
            record_cells[i][j] = &cells[i][j];
        }
        records[i].type = MC_NBT_COMPOUND;
        records[i].value.compound.items = record_cells[i];
        records[i].value.compound.length = 4;
        items[i] = &records[i];
    }
    fields[3].type = MC_NBT_LIST;
    fields[3].value.list.element_type = MC_NBT_COMPOUND;
    fields[3].value.list.items = items;
    fields[3].value.list.length = v->nbt_item_count;
    fields[4].type = fields[5].type = MC_NBT_SHORT;
    fields[4].value.short_value = mc_java_i16_narrow(v->nbt_burn);
    fields[5].value.short_value = mc_java_i16_narrow(v->nbt_cook);
    if (strcmp(v->nbt_mode, "wrong-items") == 0)
        fields[3].type = MC_NBT_INT;
    if (strcmp(v->nbt_mode, "wrong-items") == 0)
        fields[3].value.int_value = 1;
    if (strcmp(v->nbt_mode, "wrong-burn") == 0)
        fields[4].type = MC_NBT_INT;
    if (strcmp(v->nbt_mode, "wrong-burn") == 0)
        fields[4].value.int_value = 1;
    if (strcmp(v->nbt_mode, "missing-items") == 0)
        fields[3].name = furnace_name("Absent", field_names[3]);
    const mc_nbt_tag root = {.type = MC_NBT_COMPOUND, .value.compound = {children, 6}};
    return mc_tile_entity_read_nbt(tile, &root, error);
}

#define FURNACE_CASE_CHECK(c)                                                                      \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c);                                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static bool furnace_core_case(fixture *f, const mc_furnace_reference *v) {
    mc_tile_entity *tile = NULL, *fresh = NULL;
    mc_item_stack *inputs[8] = {0}, *returned = NULL;
    mc_nbt_tree *tree = NULL;
    bool passed = false;
    FURNACE_CASE_CHECK(mc_tile_entity_allocate(f->world, MC_TILE_ENTITY_FURNACE, &tile) ==
                           MC_TILE_ENTITY_OK &&
                       mc_tile_entity_construct(tile) == MC_TILE_ENTITY_OK);
    furnace_labels labels = {0};
    for (size_t i = 0; i < v->input_stack_count; ++i) {
        const mc_furnace_stack_reference *input = &v->input_stacks[i];
        FURNACE_CASE_CHECK(
            mc_item_stack_allocate(&inputs[i]) == MC_ITEM_STACK_OK &&
            mc_item_stack_construct(inputs[i], input->id, input->count, input->damage) ==
                MC_ITEM_STACK_OK &&
            mc_item_stack_source_animation_set(inputs[i], input->animation) == MC_ITEM_STACK_OK);
        labels.names[labels.count] = input->identity;
        labels.values[labels.count++] = inputs[i];
    }
    /* Declared actual source cells, matching the original reflection fixture.
     * Do not call SK setter here: its clamp is the operation under test. */
    for (size_t i = 0; i < 3; ++i) {
        mc_item_stack *stack = furnace_label(&labels, v->before_tile[i].identity);
        FURNACE_CASE_CHECK(stack == NULL || mc_item_stack_retain(stack) == MC_ITEM_STACK_OK);
        tile->state.furnace.slots[i] = stack;
    }
    FURNACE_CASE_CHECK(mc_tile_entity_furnace_field_set(tile, MC_FURNACE_BURN_REMAINING,
                                                        v->fields[0]) == MC_TILE_ENTITY_OK &&
                       mc_tile_entity_furnace_field_set(tile, MC_FURNACE_FUEL_DURATION,
                                                        v->fields[1]) == MC_TILE_ENTITY_OK &&
                       mc_tile_entity_furnace_field_set(tile, MC_FURNACE_COOK_PROGRESS,
                                                        v->fields[2]) == MC_TILE_ENTITY_OK);
    if (strcmp(v->operation, "construct") != 0)
        FURNACE_CASE_CHECK(mc_tile_entity_set_coordinates(tile, v->xyz[0], v->xyz[1], v->xyz[2]) ==
                           MC_TILE_ENTITY_OK);
    mc_tile_entity_status status = MC_TILE_ENTITY_OK;
    mc_tile_entity_error error;
    int32_t number = INT32_MIN;
    bool boolean = false;
    const char *text = NULL;
    const char *op = v->operation;
    if (strcmp(op, "size") == 0)
        status = mc_tile_entity_furnace_size(tile, &number);
    else if (strcmp(op, "limit") == 0)
        status = mc_tile_entity_furnace_limit(tile, &number);
    else if (strcmp(op, "name") == 0)
        status = mc_tile_entity_furnace_name(tile, &text);
    else if (strcmp(op, "get") == 0)
        status = mc_tile_entity_furnace_get(tile, v->index, &returned);
    else if (strcmp(op, "set") == 0)
        status = mc_tile_entity_furnace_set(tile, v->index, furnace_label(&labels, v->argument));
    else if (strcmp(op, "remove") == 0)
        status = mc_tile_entity_furnace_remove(tile, v->index, v->amount, &returned);
    else if (strcmp(op, "burning") == 0)
        status = mc_tile_entity_furnace_burning(tile, &boolean);
    else if (strcmp(op, "cook-scale") == 0)
        status = mc_tile_entity_furnace_cook_scale(tile, v->amount, &number);
    else if (strcmp(op, "burn-scale") == 0)
        status = mc_tile_entity_furnace_burn_scale(tile, v->amount, &number);
    else if (strcmp(op, "fuel") == 0)
        status = mc_tile_entity_furnace_fuel(f->registry, furnace_label(&labels, v->argument),
                                             &number, &error);
    else if (strcmp(op, "can-smelt") == 0)
        status = mc_tile_entity_furnace_can_smelt(tile, f->registry, &boolean, &error);
    else if (strcmp(op, "smelt") == 0)
        status = mc_tile_entity_furnace_smelt(tile, f->registry, &error);
    else if (strcmp(op, "read") == 0)
        status = furnace_read_input(tile, v, &error);
    else if (strcmp(op, "write") == 0 || strcmp(op, "roundtrip") == 0) {
        status = mc_tile_entity_build_source_nbt(tile, NULL, &tree, &error);
        if (status == MC_TILE_ENTITY_OK) {
            FURNACE_CASE_CHECK(furnace_field(mc_nbt_tree_root(tree), "BurnTime") != NULL &&
                               furnace_field(mc_nbt_tree_root(tree), "BurnTime")->type ==
                                   MC_NBT_SHORT &&
                               furnace_field(mc_nbt_tree_root(tree), "CookTime") != NULL);
            if (strcmp(op, "roundtrip") == 0) {
                FURNACE_CASE_CHECK(mc_tile_entity_allocate(f->world, MC_TILE_ENTITY_FURNACE,
                                                           &fresh) == MC_TILE_ENTITY_OK &&
                                   mc_tile_entity_construct(fresh) == MC_TILE_ENTITY_OK);
                status = mc_tile_entity_read_nbt(fresh, mc_nbt_tree_root(tree), &error);
                mc_tile_entity_release(tile);
                tile = fresh;
                fresh = NULL;
            }
        }
        mc_nbt_tree_free(tree);
        tree = NULL;
    }
    if ((status == MC_TILE_ENTITY_OK) != v->returned)
        fprintf(stderr, "Furnace %s op=%s status=%d expected-return=%u original=%s\n", v->profile,
                op, (int)status, v->returned ? 1u : 0u, v->exception == NULL ? "-" : v->exception);
    FURNACE_CASE_CHECK((status == MC_TILE_ENTITY_OK) == v->returned);
    if (status == MC_TILE_ENTITY_OK && v->result_kind == 1)
        FURNACE_CASE_CHECK(number == v->result_int);
    if (status == MC_TILE_ENTITY_OK && v->result_kind == 2)
        FURNACE_CASE_CHECK(boolean == (v->result_int != 0));
    if (status == MC_TILE_ENTITY_OK && strcmp(op, "name") == 0)
        FURNACE_CASE_CHECK(text != NULL && strcmp(text, v->result_text) == 0);
    if (status == MC_TILE_ENTITY_OK && (strcmp(op, "get") == 0 || strcmp(op, "remove") == 0)) {
        mc_furnace_stack_reference expected = {0};
        for (size_t i = 0; i < v->tracked_stack_count; ++i)
            if (v->result_text != NULL &&
                strcmp(v->tracked_stacks[i].identity, v->result_text) == 0)
                expected = v->tracked_stacks[i];
        FURNACE_CASE_CHECK(furnace_identity(&labels, &expected, returned));
    }
    mc_furnace_fields fields;
    mc_tile_entity_snapshot snapshot;
    FURNACE_CASE_CHECK(mc_tile_entity_furnace_fields_get(tile, &fields) == MC_TILE_ENTITY_OK &&
                       fields.burn_remaining == v->after_fields[0] &&
                       fields.fuel_duration == v->after_fields[1] &&
                       fields.cook_progress == v->after_fields[2] &&
                       mc_tile_entity_get_snapshot(tile, &snapshot) == MC_TILE_ENTITY_OK &&
                       snapshot.x == v->after_xyz[0] && snapshot.y == v->after_xyz[1] &&
                       snapshot.z == v->after_xyz[2]);
    for (size_t i = 0; i < 3; ++i)
        FURNACE_CASE_CHECK(
            furnace_identity(&labels, &v->after_tile[i], tile->state.furnace.slots[i]));
    for (size_t i = 0; i < v->tracked_stack_count; ++i)
        FURNACE_CASE_CHECK(furnace_identity(&labels, &v->tracked_stacks[i],
                                            furnace_label(&labels, v->tracked_stacks[i].identity)));
    passed = true;
cleanup:
    mc_nbt_tree_free(tree);
    mc_tile_entity_release(fresh);
    mc_item_stack_release(returned);
    mc_tile_entity_release(tile);
    for (size_t i = 0; i < v->input_stack_count; ++i)
        mc_item_stack_release(inputs[i]);
    return passed;
}

static bool furnace_bound_ownership(fixture *f) {
    mc_world_chunk *chunk = NULL;
    mc_chunk_state *state = NULL;
    const mc_chunk_acquire_context *acquisition = NULL;
    mc_tile_entity *furnace = NULL, *chest = NULL;
    mc_item_stack *shared = NULL, *removed = NULL;
    mc_nbt_tree *records = NULL;
    bool passed = false;
    uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
    FURNACE_CASE_CHECK(blocks != NULL);
    blocks[((size_t)3 * 16u + 3u) * 128u + 64u] = 61;
    blocks[((size_t)4 * 16u + 3u) * 128u + 64u] = 54;
    FURNACE_CASE_CHECK(mc_world_store_create_generated(f->store, f->world, 0, 0, 0, &blocks,
                                                       MC_CHUNK_BLOCKS, &chunk,
                                                       NULL) == MC_WORLD_STORAGE_OK &&
                       blocks == NULL);
    state = mc_world_chunk_state(chunk);
    acquisition = mc_world_runtime_acquisition(f->world);
    FURNACE_CASE_CHECK(state != NULL && acquisition != NULL &&
                       mc_chunk_cache_publish(acquisition->cache, 0, 0, state));
    FURNACE_CASE_CHECK(
        mc_tile_entity_allocate(f->world, MC_TILE_ENTITY_FURNACE, &furnace) == MC_TILE_ENTITY_OK &&
        mc_tile_entity_construct(furnace) == MC_TILE_ENTITY_OK &&
        mc_tile_entity_allocate(f->world, MC_TILE_ENTITY_CHEST, &chest) == MC_TILE_ENTITY_OK &&
        mc_tile_entity_construct(chest) == MC_TILE_ENTITY_OK &&
        mc_live_tile_entities_set(f->world, 3, 64, 3, furnace, NULL) == MC_WORLD_STORAGE_OK &&
        mc_live_tile_entities_set(f->world, 4, 64, 3, chest, NULL) == MC_WORLD_STORAGE_OK);
    FURNACE_CASE_CHECK(mc_item_stack_allocate(&shared) == MC_ITEM_STACK_OK &&
                       mc_item_stack_construct(shared, 263, 5, 1) == MC_ITEM_STACK_OK);
    /* Explicit clean input on a real bound Map owner. A mistakenly added
     * OW.y_ would dirty this actual Chunk; the Chest positive control proves it. */
    state->dirty = false;
    FURNACE_CASE_CHECK(
        mc_tile_entity_furnace_set(furnace, 1, shared) == MC_TILE_ENTITY_OK && !state->dirty &&
        mc_tile_entity_furnace_remove(furnace, 1, -2, &removed) == MC_TILE_ENTITY_OK &&
        !state->dirty);
    mc_item_stack_fields fields;
    FURNACE_CASE_CHECK(
        removed != NULL && removed != shared &&
        mc_item_stack_fields_get(removed, &fields) == MC_ITEM_STACK_OK && fields.count == -2 &&
        mc_item_stack_fields_get(shared, &fields) == MC_ITEM_STACK_OK && fields.count == 7);
    mc_item_stack_release(removed);
    removed = NULL;
    FURNACE_CASE_CHECK(mc_tile_entity_furnace_set(furnace, 0, shared) == MC_TILE_ENTITY_OK &&
                       mc_tile_entity_furnace_set(furnace, 2, shared) == MC_TILE_ENTITY_OK &&
                       !state->dirty);
    size_t stack_bytes, bytes;
    FURNACE_CASE_CHECK(mc_item_stack_native_bytes(shared, &stack_bytes) == MC_ITEM_STACK_OK &&
                       mc_tile_entity_storage_bytes(furnace, &bytes) == MC_TILE_ENTITY_OK &&
                       bytes == mc_tile_entity_native_bytes(furnace) + stack_bytes);
    FURNACE_CASE_CHECK(mc_tile_entity_chest_set(chest, 0, shared) == MC_TILE_ENTITY_OK &&
                       state->dirty);
    FURNACE_CASE_CHECK(
        mc_live_tile_entities_build_records(chunk, NULL, &records, NULL) == MC_WORLD_STORAGE_OK &&
        furnace_field(mc_nbt_tree_root(records), "TileEntities") != NULL &&
        furnace_field(mc_nbt_tree_root(records), "TileEntities")->value.list.length == 2);
    mc_nbt_tree_free(records);
    records = NULL;
    FURNACE_CASE_CHECK(
        mc_live_tile_entities_build_source_records(chunk, NULL, &records, NULL) ==
            MC_WORLD_STORAGE_OK &&
        furnace_field(mc_nbt_tree_root(records), "TileEntities") != NULL &&
        furnace_field(mc_nbt_tree_root(records), "TileEntities")->value.list.length == 2);
    mc_nbt_tree_free(records);
    records = NULL;
    passed = true;
cleanup:
    free(blocks);
    mc_nbt_tree_free(records);
    mc_item_stack_release(removed);
    mc_item_stack_release(shared);
    mc_tile_entity_release(furnace);
    mc_tile_entity_release(chest);
    if (state != NULL)
        CHECK(mc_world_runtime_retire_entity_containers(f->world, NULL) == MC_WORLD_STORAGE_OK &&
              mc_chunk_cache_init(acquisition->cache, acquisition->cache->entries,
                                  acquisition->cache->capacity, acquisition->cache->insertions,
                                  acquisition->cache->insertion_capacity) &&
              mc_world_store_retire_chunk(f->store, state, NULL) == MC_WORLD_STORAGE_OK);
    return passed;
}

#undef FURNACE_CASE_CHECK

static bool furnace_core_controls(fixture *f) {
    size_t compared = 0, outside = 0;
    for (size_t i = 0; i < sizeof mc_furnace_references / sizeof *mc_furnace_references; ++i) {
        const mc_furnace_reference *v = &mc_furnace_references[i];
        if (v->array_null || v->tile_null || strncmp(v->operation, "bm-", 3) == 0 ||
            strcmp(v->operation, "tick-null") == 0 || strcmp(v->operation, "usable-null") == 0) {
            ++outside;
            continue;
        }
        const bool passed = furnace_core_case(f, v);
        if (!passed)
            fprintf(stderr, "Furnace original profile: %s (%s)\n", v->profile, v->operation);
        CHECK(passed);
        ++compared;
    }
    printf("Furnace original core projections: %zu compared; %zu explicit "
           "non-core/native-admission profiles; exception-class equivalence not asserted\n",
           compared, outside);
    CHECK(furnace_bound_ownership(f));
    return true;
}

/* Each completed release clears its fixture right, so partial setup and a
 * refused cleanup can both be diagnosed without replaying earlier releases. */
static bool furnace_core_teardown(fixture *f) {
    if (f->sessions != NULL)
        CHECK(mc_player_session_classes_free(&f->sessions, NULL) == MC_PLAYER_OK);
    if (f->world != NULL) {
        CHECK(mc_world_runtime_retire_entity_containers(f->world, NULL) == MC_WORLD_STORAGE_OK);
        mc_world_provider_unbind(f->dimension);
        CHECK(mc_world_runtime_retire_world(f->world, NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_free(f->world, NULL) == MC_WORLD_STORAGE_OK);
        f->world = NULL;
    }
    mc_world_provider_release(f->dimension);
    f->dimension = NULL;
    if (f->store != NULL) {
        CHECK(mc_world_store_chunk_count(f->store) == 0);
        CHECK(mc_world_store_free_checked(&f->store, NULL) == MC_WORLD_STORAGE_OK);
    }
    mc_world_regions_free(f->regions);
    f->regions = NULL;
    mc_world_info_name_release(f->name);
    f->name = NULL;
    if (f->world_classes != NULL) {
        CHECK(mc_world_runtime_classes_free(f->world_classes, NULL) == MC_WORLD_STORAGE_OK);
        f->world_classes = NULL;
    }
    if (f->registry != NULL) {
        CHECK(mc_registry_classes_free(f->registry, NULL) == MC_REGISTRY_OK);
        f->registry = NULL;
    }
    if (f->entities != NULL) {
        CHECK(mc_entity_classes_free(f->entities) == MC_ENTITY_OK);
        f->entities = NULL;
    }
    mc_java_random_profile_free(f->profile);
    f->profile = NULL;
    mc_materials_destroy(f->materials);
    f->materials = NULL;
    mc_resource_files_free(f->files);
    f->files = NULL;
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fputs("usage: furnace-core RESOURCES EMPTY_WORLD_DIRECTORY\n", stderr);
        return 2;
    }
    mc_math_trig_init(&trig);
    fixture f = {0};
    const bool setup_ok = setup(&f, argv[1], argv[2]);
    const bool controls_ok = setup_ok && furnace_core_controls(&f);
    const bool teardown_ok = furnace_core_teardown(&f);
    if (!setup_ok || !controls_ok || !teardown_ok)
        return 1;
    printf("PASS: %u Furnace core source/ownership checks\n", checks);
    return 0;
}
