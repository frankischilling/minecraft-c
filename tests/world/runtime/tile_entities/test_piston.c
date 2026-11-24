/* Import the actual GAME Registry/World/Store/TileEntity fixture, not its old main.
 * Compile this translation unit alone. No provider or TileEntity callback is replaced. */
#define main mc_piston_tile_fixture_main
#include "test_tile_entities.c"
#undef main

#include "../../../../src/world/runtime/tile_entities/tile_entities_internal.h"
#include "../../../support/java_random.h"
#include "mc/world/runtime/saving/saving.h"

/* Constructed native contracts. Piston ticking, Entity pushing and finalization
 * are not supplied as fake callbacks. A private k/l field control below is an
 * explicit raw source-state input and is restored before CHECK may branch. */
typedef struct piston_record {
    mc_nbt_tag root, fields[11];
    const mc_nbt_tag *items[11];
    uint16_t names[11][20], id[6], wrong[5];
} piston_record;

static void piston_prepare(piston_record *record) {
    memset(record, 0, sizeof(*record));
    const char *const names[] = {"id",        "x",         "y",      "z",
                                 "blockId",   "blockData", "facing", "progress",
                                 "extending", "Opaque",    "j"};
    const mc_nbt_type types[] = {MC_NBT_STRING, MC_NBT_INT,  MC_NBT_INT, MC_NBT_INT,
                                 MC_NBT_INT,    MC_NBT_INT,  MC_NBT_INT, MC_NBT_FLOAT,
                                 MC_NBT_BYTE,   MC_NBT_LONG, MC_NBT_BYTE};
    record->root.type = MC_NBT_COMPOUND;
    record->root.value.compound.items = record->items;
    record->root.value.compound.length = COUNT(record->items);
    for (size_t i = 0; i < COUNT(record->items); ++i) {
        record->fields[i].type = types[i];
        record->fields[i].name = ascii(names[i], record->names[i]);
        record->items[i] = &record->fields[i];
    }
    record->fields[0].value.string = ascii("Piston", record->id);
    record->fields[1].value.int_value = 11;
    record->fields[2].value.int_value = -22;
    record->fields[3].value.int_value = 33;
    record->fields[4].value.int_value = INT32_MIN;
    record->fields[5].value.int_value = INT32_MAX;
    record->fields[6].value.int_value = -7;
    record->fields[7].value.float_bits = UINT32_C(0x7f812345);
    record->fields[8].value.byte_value = -128;
    record->fields[9].value.long_value = INT64_C(0x1122334455667788);
    record->fields[10].value.byte_value = -91;
}

static bool piston_base_is(const mc_tile_entity_snapshot *snapshot, mc_world_runtime *world,
                           int32_t x, int32_t y, int32_t z, bool invalid) {
    return snapshot->kind == MC_TILE_ENTITY_PISTON && snapshot->world == world &&
           snapshot->x == x && snapshot->y == y && snapshot->z == z &&
           snapshot->invalid == invalid && snapshot->chest_capacity == 0 &&
           snapshot->spawner_delay == 0 && snapshot->spawner_rotation_bits == 0 &&
           snapshot->spawner_previous_rotation_bits == 0 && snapshot->spawner_name == NULL;
}

static bool piston_state_is(const mc_tile_entity_snapshot *snapshot, int32_t id, int32_t data,
                            int32_t facing, bool extending, bool render_head, uint32_t progress,
                            uint32_t previous) {
    return snapshot->piston_block_id == id && snapshot->piston_block_data == data &&
           snapshot->piston_facing == facing && snapshot->piston_extending == extending &&
           snapshot->piston_render_head == render_head &&
           snapshot->piston_progress_bits == progress &&
           snapshot->piston_previous_progress_bits == previous;
}

static bool piston_constructors(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *const w = f->worlds[side];
    mc_tile_entity *raw = NULL, *full = NULL, *shared = NULL;
    mc_tile_entity_snapshot snapshot;
    mc_tile_entity_error error;
    const size_t bindings = w->persistent_bindings;
    const mc_java_random random = *w->fields.r;
    CHECK(mc_tile_entity_allocate(w, MC_TILE_ENTITY_PISTON, &raw) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_get_snapshot(raw, &snapshot) == MC_TILE_ENTITY_OK &&
          snapshot.phase == MC_TILE_ENTITY_RAW && piston_base_is(&snapshot, NULL, 0, 0, 0, false) &&
          piston_state_is(&snapshot, 0, 0, 0, false, false, 0, 0) &&
          w->persistent_bindings == bindings);
    CHECK(mc_tile_entity_construct_detailed(raw, &error) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_get_snapshot(raw, &snapshot) == MC_TILE_ENTITY_OK &&
          snapshot.phase == MC_TILE_ENTITY_READY &&
          piston_base_is(&snapshot, NULL, 0, 0, 0, false) &&
          piston_state_is(&snapshot, 0, 0, 0, false, false, 0, 0) &&
          mc_tile_entity_construct(raw) == MC_TILE_ENTITY_INVALID_PHASE);
    CHECK(mc_tile_entity_allocate(w, MC_TILE_ENTITY_PISTON, &full) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_piston_construct(full, INT32_MIN, INT32_MAX, -7, true, true, &error) ==
              MC_TILE_ENTITY_OK &&
          mc_tile_entity_get_snapshot(full, &snapshot) == MC_TILE_ENTITY_OK &&
          snapshot.phase == MC_TILE_ENTITY_READY &&
          piston_base_is(&snapshot, NULL, 0, 0, 0, false) &&
          piston_state_is(&snapshot, INT32_MIN, INT32_MAX, -7, true, true, 0, 0) &&
          mc_tile_entity_piston_construct(full, 1, 2, 3, false, false, &error) ==
              MC_TILE_ENTITY_INVALID_PHASE);
    int32_t id = 0, data = 0, facing = 0;
    bool extending = false, render_head = false;
    uint32_t progress = 1, previous = 1;
    CHECK(mc_tile_entity_piston_block_id(full, &id) == MC_TILE_ENTITY_OK && id == INT32_MIN &&
          mc_tile_entity_piston_block_data(full, &data) == MC_TILE_ENTITY_OK && data == INT32_MAX &&
          mc_tile_entity_piston_facing(full, &facing) == MC_TILE_ENTITY_OK && facing == -7 &&
          mc_tile_entity_piston_extending(full, &extending) == MC_TILE_ENTITY_OK && extending &&
          mc_tile_entity_piston_render_head(full, &render_head) == MC_TILE_ENTITY_OK &&
          render_head &&
          mc_tile_entity_piston_progress_bits(full, &progress, &previous) == MC_TILE_ENTITY_OK &&
          progress == 0 && previous == 0);
    CHECK(mc_tile_entity_allocate(f->worlds[side + 2u], MC_TILE_ENTITY_PISTON, &shared) ==
              MC_TILE_ENTITY_OK &&
          mc_tile_entity_construct(shared) == MC_TILE_ENTITY_OK &&
          mc_test_java_random_equal(w->fields.r, &random) && w->persistent_bindings == bindings &&
          mc_tile_entity_source(raw) == NULL && mc_tile_entity_source_bytes(raw) == 0);
    size_t graph = SIZE_MAX;
    CHECK(mc_tile_entity_storage_bytes(raw, &graph) == MC_TILE_ENTITY_OK &&
          graph == mc_tile_entity_native_bytes(raw));
    CHECK(mc_tile_entity_retain(full) == MC_TILE_ENTITY_OK);
    mc_tile_entity_release(full);
    CHECK(mc_tile_entity_get_snapshot(full, &snapshot) == MC_TILE_ENTITY_OK &&
          piston_state_is(&snapshot, INT32_MIN, INT32_MAX, -7, true, true, 0, 0));
    ok = true;
cleanup:
    mc_tile_entity_release(raw);
    mc_tile_entity_release(full);
    mc_tile_entity_release(shared);
    return ok;
}

static bool piston_read_prefixes(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *const w = f->worlds[side];
    mc_tile_entity *tile_entity = NULL;
    mc_tile_entity_snapshot snapshot;
    mc_tile_entity_error error;
    const char *const names[] = {"x",         "y",      "z",        "blockId",
                                 "blockData", "facing", "progress", "extending"};
    for (size_t wrong = 0; wrong < COUNT(names); ++wrong) {
        piston_record record;
        piston_prepare(&record);
        record.fields[wrong + 1u].type = MC_NBT_STRING;
        record.fields[wrong + 1u].value.string = ascii("wrong", record.wrong);
        CHECK(mc_tile_entity_allocate(w, MC_TILE_ENTITY_PISTON, &tile_entity) ==
                  MC_TILE_ENTITY_OK &&
              mc_tile_entity_piston_construct(tile_entity, 101, 202, 303, false, true, &error) ==
                  MC_TILE_ENTITY_OK &&
              mc_tile_entity_bind_world(tile_entity, w) == MC_TILE_ENTITY_OK &&
              mc_tile_entity_set_coordinates(tile_entity, -1, -2, -3) == MC_TILE_ENTITY_OK &&
              mc_tile_entity_set_invalid(tile_entity, true) == MC_TILE_ENTITY_OK);
        CHECK(mc_tile_entity_read_nbt(tile_entity, &record.root, &error) ==
                  MC_TILE_ENTITY_INVALID_FIELD &&
              error.field != NULL && strcmp(error.field, names[wrong]) == 0 &&
              error.base_read_bytecode == (wrong < 3 ? 4u + 10u * (unsigned int)wrong : 0u) &&
              error.read_bytecode == (wrong < 3    ? 2u
                                      : wrong == 3 ? 9u
                                      : wrong == 4 ? 19u
                                      : wrong == 5 ? 29u
                                      : wrong == 6 ? 40u
                                                   : 54u) &&
              mc_tile_entity_get_snapshot(tile_entity, &snapshot) == MC_TILE_ENTITY_OK &&
              snapshot.phase == MC_TILE_ENTITY_READY &&
              piston_base_is(&snapshot, w, wrong > 0 ? 11 : -1, wrong > 1 ? -22 : -2,
                             wrong > 2 ? 33 : -3, true) &&
              piston_state_is(&snapshot, wrong > 3 ? INT32_MIN : 101, wrong > 4 ? INT32_MAX : 202,
                              wrong > 5 ? -7 : 303, false, true,
                              wrong > 6 ? UINT32_C(0x7f812345) : 0,
                              wrong > 6 ? UINT32_C(0x7f812345) : 0));
        mc_tile_entity_release(tile_entity);
        tile_entity = NULL;
    }
    piston_record complete;
    piston_prepare(&complete);
    CHECK(mc_tile_entity_allocate(w, MC_TILE_ENTITY_PISTON, &tile_entity) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_piston_construct(tile_entity, 1, 2, 3, false, true, &error) ==
              MC_TILE_ENTITY_OK &&
          mc_tile_entity_bind_world(tile_entity, w) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_set_invalid(tile_entity, true) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_read_nbt(tile_entity, &complete.root, &error) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_get_snapshot(tile_entity, &snapshot) == MC_TILE_ENTITY_OK &&
          piston_base_is(&snapshot, w, 11, -22, 33, true) &&
          piston_state_is(&snapshot, INT32_MIN, INT32_MAX, -7, true, true, UINT32_C(0x7f812345),
                          UINT32_C(0x7f812345)));
    const mc_nbt_tag empty = {.type = MC_NBT_COMPOUND};
    CHECK(mc_tile_entity_read_nbt(tile_entity, &empty, &error) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_get_snapshot(tile_entity, &snapshot) == MC_TILE_ENTITY_OK &&
          piston_base_is(&snapshot, w, 0, 0, 0, true) &&
          piston_state_is(&snapshot, 0, 0, 0, false, true, 0, 0));
    const int8_t booleans[] = {0, 1, -1, 2, -128, 127};
    const uint32_t progress_bits[] = {0,
                                      UINT32_C(0x80000000),
                                      UINT32_C(0x3f000000),
                                      UINT32_C(0x7f800000),
                                      UINT32_C(0xff800000),
                                      UINT32_C(0x7f812345),
                                      UINT32_C(0xffc12345)};
    for (size_t i = 0; i < COUNT(booleans); ++i) {
        complete.fields[8].value.byte_value = booleans[i];
        for (size_t j = 0; j < COUNT(progress_bits); ++j) {
            complete.fields[7].value.float_bits = progress_bits[j];
            CHECK(mc_tile_entity_read_nbt(tile_entity, &complete.root, &error) ==
                      MC_TILE_ENTITY_OK &&
                  mc_tile_entity_get_snapshot(tile_entity, &snapshot) == MC_TILE_ENTITY_OK &&
                  piston_state_is(&snapshot, INT32_MIN, INT32_MAX, -7, booleans[i] != 0, true,
                                  progress_bits[j], progress_bits[j]));
        }
    }
    const size_t numeric_fields[] = {4, 7, 8};
    const mc_nbt_type numeric_wrong_types[] = {MC_NBT_SHORT, MC_NBT_DOUBLE, MC_NBT_INT};
    const unsigned int numeric_pcs[] = {9, 40, 54};
    for (size_t i = 0; i < COUNT(numeric_fields); ++i) {
        piston_record typed;
        piston_prepare(&typed);
        typed.fields[numeric_fields[i]].type = numeric_wrong_types[i];
        if (i == 0)
            typed.fields[numeric_fields[i]].value.short_value = 1;
        else if (i == 1)
            typed.fields[numeric_fields[i]].value.double_bits = UINT64_C(0x3ff0000000000000);
        else
            typed.fields[numeric_fields[i]].value.int_value = 1;
        CHECK(mc_tile_entity_read_nbt(tile_entity, &typed.root, &error) ==
                  MC_TILE_ENTITY_INVALID_FIELD &&
              error.field != NULL && strcmp(error.field, names[numeric_fields[i] - 1u]) == 0 &&
              error.read_bytecode == numeric_pcs[i]);
    }
    ok = true;
cleanup:
    mc_tile_entity_release(tile_entity);
    return ok;
}

static bool piston_class_failure(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime_classes *classes = NULL;
    mc_world_runtime *world = NULL;
    mc_tile_entity *chest = NULL, *tile_entity = NULL;
    mc_tile_entity_piston_class_snapshot before, failed, later;
    CHECK(
        mc_world_runtime_classes_create(side == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER,
                                        f->profile, &trig, &classes, NULL) == MC_WORLD_STORAGE_OK &&
        mc_world_runtime_classes_bind_registry(classes, f->registry[side], NULL) ==
            MC_REGISTRY_OK &&
        mc_world_runtime_classes_bind_entities(classes, f->entity_classes[side]) == MC_ENTITY_OK &&
        mc_world_runtime_allocate_world(side == 0 ? MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA
                                                  : MC_WORLD_RUNTIME_SERVER_LOAD_OR_CREATE,
                                        classes, NULL, &world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_tile_entity_piston_class_get_snapshot(classes, &before) == MC_TILE_ENTITY_OK &&
          before.base_phase == MC_TILE_ENTITY_CLASS_COLD &&
          before.piston_phase == MC_TILE_ENTITY_CLASS_COLD && before.list_identity == NULL &&
          before.live_tile_entities == 0);
    CHECK(mc_tile_entity_allocate(world, MC_TILE_ENTITY_CHEST, &chest) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_piston_class_get_snapshot(classes, &before) == MC_TILE_ENTITY_OK &&
          before.base_phase == MC_TILE_ENTITY_CLASS_READY &&
          before.base_initializer_bytecode == 76 && before.base_failure == MC_TILE_ENTITY_OK &&
          before.base_name_count == 8 && before.base_class_count == 8 &&
          before.piston_phase == MC_TILE_ENTITY_CLASS_COLD && before.list_identity == NULL &&
          before.live_tile_entities == 1);
    mc_tile_entity_release(chest);
    chest = NULL;
    CHECK(
        mc_tile_entity_piston_class_configure(world, sizeof(mc_world_reference_list) - 1u) ==
            MC_TILE_ENTITY_OK &&
        mc_tile_entity_allocate(world, MC_TILE_ENTITY_PISTON, &tile_entity) ==
            MC_TILE_ENTITY_LIMIT &&
        tile_entity == NULL &&
        mc_tile_entity_piston_class_get_snapshot(classes, &failed) == MC_TILE_ENTITY_OK &&
        failed.base_phase == MC_TILE_ENTITY_CLASS_READY && failed.base_initializer_bytecode == 76 &&
        failed.base_failure == MC_TILE_ENTITY_OK && failed.base_name_count == 8 &&
        failed.base_class_count == 8 && failed.piston_phase == MC_TILE_ENTITY_CLASS_FAILED &&
        failed.piston_initializer_bytecode == 0 && failed.piston_failure == MC_TILE_ENTITY_LIMIT &&
        failed.list_identity == NULL && failed.list_count == 0 && failed.list_capacity == 0 &&
        failed.allocation_bytes == 0 && failed.live_tile_entities == 0);
    CHECK(mc_tile_entity_piston_class_configure(world, SIZE_MAX) == MC_TILE_ENTITY_INVALID_PHASE &&
          mc_tile_entity_allocate(world, MC_TILE_ENTITY_PISTON, &tile_entity) ==
              MC_TILE_ENTITY_LIMIT &&
          tile_entity == NULL &&
          mc_tile_entity_piston_class_get_snapshot(classes, &later) == MC_TILE_ENTITY_OK &&
          later.piston_phase == failed.piston_phase &&
          later.piston_failure == failed.piston_failure &&
          later.piston_initializer_bytecode == failed.piston_initializer_bytecode &&
          later.list_identity == NULL && later.allocation_bytes == failed.allocation_bytes &&
          later.base_name_count == 8 && later.base_class_count == 8);
    piston_record record;
    piston_prepare(&record);
    mc_live_tile_entity_error error;
    CHECK(mc_live_tile_entities_factory(world, &record.root, NULL, false, &tile_entity, &error) ==
              MC_WORLD_STORAGE_LIMIT &&
          tile_entity == NULL && error.tile_entity.status == MC_TILE_ENTITY_LIMIT &&
          mc_tile_entity_piston_class_get_snapshot(classes, &later) == MC_TILE_ENTITY_OK &&
          later.piston_phase == MC_TILE_ENTITY_CLASS_FAILED &&
          later.piston_failure == MC_TILE_ENTITY_LIMIT && later.list_identity == NULL &&
          later.live_tile_entities == 0);
    CHECK(mc_tile_entity_allocate(world, MC_TILE_ENTITY_CHEST, &chest) == MC_TILE_ENTITY_OK);
    mc_tile_entity_release(chest);
    chest = NULL;
    CHECK(mc_world_runtime_free(world, NULL) == MC_WORLD_STORAGE_OK);
    world = NULL;
    CHECK(mc_world_runtime_classes_free(classes, NULL) == MC_WORLD_STORAGE_OK);
    classes = NULL;
    ok = true;
cleanup:
    mc_tile_entity_release(chest);
    mc_tile_entity_release(tile_entity);
    if (world != NULL && mc_world_runtime_free(world, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    if (classes != NULL && mc_world_runtime_classes_free(classes, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    return ok;
}

static bool piston_class_identity(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime_classes *classes = NULL;
    mc_world_runtime *world = NULL;
    mc_tile_entity *tile_entity = NULL;
    mc_tile_entity_piston_class_snapshot first, shared, other;
    CHECK(mc_tile_entity_piston_class_get_snapshot(f->classes[side], &first) == MC_TILE_ENTITY_OK &&
          first.base_phase == MC_TILE_ENTITY_CLASS_READY &&
          first.piston_phase == MC_TILE_ENTITY_CLASS_READY &&
          first.piston_initializer_bytecode == 10 && first.piston_failure == MC_TILE_ENTITY_OK &&
          first.list_identity != NULL && first.list_count == 0 && first.list_capacity == 0 &&
          first.list_identity->items == NULL &&
          first.allocation_bytes == sizeof(mc_world_reference_list) &&
          first.live_tile_entities == 0 && first.list_identity != f->worlds[side]->fields.R &&
          first.list_identity != f->worlds[side]->fields.K);
    CHECK(mc_tile_entity_allocate(f->worlds[side + 2u], MC_TILE_ENTITY_PISTON, &tile_entity) ==
              MC_TILE_ENTITY_OK &&
          mc_tile_entity_piston_class_get_snapshot(f->classes[side], &shared) ==
              MC_TILE_ENTITY_OK &&
          shared.list_identity == first.list_identity && shared.live_tile_entities == 1 &&
          shared.allocation_bytes == first.allocation_bytes);
    mc_tile_entity_release(tile_entity);
    tile_entity = NULL;
    CHECK(
        mc_world_runtime_classes_create(side == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER,
                                        f->profile, &trig, &classes, NULL) == MC_WORLD_STORAGE_OK &&
        mc_world_runtime_classes_bind_registry(classes, f->registry[side], NULL) ==
            MC_REGISTRY_OK &&
        mc_world_runtime_classes_bind_entities(classes, f->entity_classes[side]) == MC_ENTITY_OK &&
        mc_world_runtime_allocate_world(side == 0 ? MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA
                                                  : MC_WORLD_RUNTIME_SERVER_LOAD_OR_CREATE,
                                        classes, NULL, &world, NULL) == MC_WORLD_STORAGE_OK &&
        mc_tile_entity_piston_class_configure(world, sizeof(mc_world_reference_list)) ==
            MC_TILE_ENTITY_OK &&
        mc_tile_entity_allocate(world, MC_TILE_ENTITY_PISTON, &tile_entity) == MC_TILE_ENTITY_OK &&
        mc_tile_entity_piston_class_get_snapshot(classes, &other) == MC_TILE_ENTITY_OK &&
        other.base_phase == MC_TILE_ENTITY_CLASS_READY &&
        other.piston_phase == MC_TILE_ENTITY_CLASS_READY && other.list_identity != NULL &&
        other.list_identity != first.list_identity &&
        other.allocation_bytes == sizeof(mc_world_reference_list) &&
        other.live_tile_entities == 1 &&
        other.max_allocation_bytes == sizeof(mc_world_reference_list));
    CHECK(mc_world_runtime_free(world, NULL) == MC_WORLD_STORAGE_OK);
    world = NULL;
    CHECK(mc_world_runtime_classes_free(classes, NULL) == MC_WORLD_STORAGE_BUSY &&
          mc_tile_entity_piston_class_get_snapshot(classes, &other) == MC_TILE_ENTITY_OK &&
          other.live_tile_entities == 1 && other.list_identity != NULL);
    mc_tile_entity_release(tile_entity);
    tile_entity = NULL;
    CHECK(mc_world_runtime_classes_free(classes, NULL) == MC_WORLD_STORAGE_OK);
    classes = NULL;
    ok = true;
cleanup:
    mc_tile_entity_release(tile_entity);
    if (world != NULL && mc_world_runtime_free(world, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    if (classes != NULL && mc_world_runtime_classes_free(classes, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    return ok;
}

static bool piston_string_is(const mc_nbt_tag *tag, const char *text) {
    if (tag == NULL || tag->type != MC_NBT_STRING || tag->value.string.length != strlen(text))
        return false;
    for (size_t i = 0; i < tag->value.string.length; ++i)
        if (tag->value.string.units[i] != (unsigned char)text[i])
            return false;
    return true;
}

static bool piston_written_is(const mc_nbt_tag *record, int32_t x, int32_t y, int32_t z,
                              int32_t block_id, int32_t block_data, int32_t facing, bool extending,
                              uint32_t progress, bool preserved) {
    if (record == NULL || record->type != MC_NBT_COMPOUND ||
        record->value.compound.length != (preserved ? 11u : 9u) ||
        !piston_string_is(field(record, "id"), "Piston"))
        return false;
    const char *const names[] = {"x", "y", "z", "blockId", "blockData", "facing"};
    const int32_t values[] = {x, y, z, block_id, block_data, facing};
    for (size_t i = 0; i < COUNT(names); ++i) {
        const mc_nbt_tag *value = field(record, names[i]);
        if (value == NULL || value->type != MC_NBT_INT || value->value.int_value != values[i])
            return false;
    }
    const mc_nbt_tag *fraction = field(record, "progress");
    const mc_nbt_tag *flag = field(record, "extending");
    if (fraction == NULL || fraction->type != MC_NBT_FLOAT ||
        fraction->value.float_bits != progress || flag == NULL || flag->type != MC_NBT_BYTE ||
        flag->value.byte_value != (extending ? 1 : 0))
        return false;
    const mc_nbt_tag *extra = field(record, "Opaque"), *j = field(record, "j");
    return preserved ? extra != NULL && extra->type == MC_NBT_LONG &&
                           extra->value.long_value == INT64_C(0x1122334455667788) && j != NULL &&
                           j->type == MC_NBT_BYTE && j->value.byte_value == -91
                     : extra == NULL && j == NULL;
}

static bool piston_writer_and_provenance(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *const w = f->worlds[side];
    mc_tile_entity *semantic = NULL, *stored = NULL;
    mc_nbt_document *plain = NULL, *preserved = NULL;
    mc_tile_entity_snapshot snapshot;
    mc_tile_entity_error error;
    piston_record record;
    piston_prepare(&record);
    record.fields[7].value.float_bits = UINT32_C(0x3e800000);
    CHECK(mc_tile_entity_allocate(w, MC_TILE_ENTITY_PISTON, &semantic) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_piston_construct(semantic, 1, 2, 3, false, true, &error) ==
              MC_TILE_ENTITY_OK &&
          mc_tile_entity_read_nbt(semantic, &record.root, &error) == MC_TILE_ENTITY_OK);
    const uint32_t old_current = semantic->state.piston.progress_bits;
    const uint32_t old_previous = semantic->state.piston.previous_progress_bits;
    /* Raw reflected source-state input, not an invented tick/progress setter.
     * Build must write previous l=.5 while current k=.75 is different. */
    semantic->state.piston.progress_bits = UINT32_C(0x3f400000);
    semantic->state.piston.previous_progress_bits = UINT32_C(0x3f000000);
    const mc_tile_entity_status written = mc_tile_entity_build(semantic, NULL, &plain, &error);
    semantic->state.piston.progress_bits = old_current;
    semantic->state.piston.previous_progress_bits = old_previous;
    CHECK(written == MC_TILE_ENTITY_OK &&
          piston_written_is(mc_nbt_document_root(plain), 11, -22, 33, INT32_MIN, INT32_MAX, -7,
                            true, UINT32_C(0x3f000000), false) &&
          mc_tile_entity_get_snapshot(semantic, &snapshot) == MC_TILE_ENTITY_OK &&
          piston_state_is(&snapshot, INT32_MIN, INT32_MAX, -7, true, true, old_current,
                          old_previous));
    mc_nbt_limits limits = mc_nbt_default_limits();
    limits.max_allocation_bytes = 0;
    CHECK(mc_tile_entity_import_storage(w, MC_TILE_ENTITY_PISTON, &record.root, &limits, &stored,
                                        &error) == MC_TILE_ENTITY_LIMIT &&
          stored == NULL);
    CHECK(mc_tile_entity_import_storage(w, MC_TILE_ENTITY_PISTON, &record.root, NULL, &stored,
                                        &error) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_source(stored) != NULL && mc_tile_entity_source(stored) != &record.root &&
          mc_tile_entity_source_bytes(stored) != 0 &&
          mc_tile_entity_get_snapshot(stored, &snapshot) == MC_TILE_ENTITY_OK &&
          piston_base_is(&snapshot, NULL, 11, -22, 33, false) &&
          piston_state_is(&snapshot, INT32_MIN, INT32_MAX, -7, true, false, UINT32_C(0x3e800000),
                          UINT32_C(0x3e800000)));
    size_t graph = SIZE_MAX;
    CHECK(mc_tile_entity_storage_bytes(stored, &graph) == MC_TILE_ENTITY_OK &&
          graph == mc_tile_entity_native_bytes(stored) + mc_tile_entity_source_bytes(stored));
    record.fields[4].value.int_value = 99;
    record.fields[9].value.long_value = 99;
    record.fields[10].value.byte_value = 0;
    const mc_nbt_tag *source_opaque = field(mc_tile_entity_source(stored), "Opaque");
    const mc_nbt_tag *source_j = field(mc_tile_entity_source(stored), "j");
    CHECK(source_opaque != NULL && source_opaque->type == MC_NBT_LONG && source_j != NULL &&
          source_j->type == MC_NBT_BYTE &&
          source_opaque->value.long_value == INT64_C(0x1122334455667788) &&
          source_j->value.byte_value == -91);
    limits = mc_nbt_default_limits();
    limits.max_allocation_bytes = graph;
    CHECK(mc_tile_entity_build(stored, &limits, &preserved, &error) == MC_TILE_ENTITY_LIMIT &&
          preserved == NULL && mc_tile_entity_storage_bytes(stored, &graph) == MC_TILE_ENTITY_OK);
    CHECK(mc_tile_entity_build(stored, NULL, &preserved, &error) == MC_TILE_ENTITY_OK &&
          piston_written_is(mc_nbt_document_root(preserved), 11, -22, 33, INT32_MIN, INT32_MAX, -7,
                            true, UINT32_C(0x3e800000), true));
    mc_tile_entity_release(stored);
    stored = NULL;
    CHECK(piston_written_is(mc_nbt_document_root(preserved), 11, -22, 33, INT32_MIN, INT32_MAX, -7,
                            true, UINT32_C(0x3e800000), true));
    ok = true;
cleanup:
    mc_nbt_document_free(plain);
    mc_nbt_document_free(preserved);
    mc_tile_entity_release(semantic);
    mc_tile_entity_release(stored);
    return ok;
}

static bool piston_load_empty_chunk(fixture *f, size_t slot) {
    bool ok = false;
    CHECK(seed_record(f, slot));
    mc_chunk_state *state = NULL;
    const mc_chunk_acquire_status status = mc_world_runtime_get(f->worlds[slot], 0, 0, &state);
    if (f->providers[slot].cache->insertion_count != 0)
        f->states[slot] = f->providers[slot].cache->insertions[0];
    CHECK(status == MC_CHUNK_ACQUIRE_OK && state != NULL && state == f->states[slot] &&
          state->active && state->terrain_populated && !state->dirty);
    f->chunks[slot] = mc_world_store_chunk_object(f->stores[slot], state);
    CHECK(f->chunks[slot] != NULL && f->chunks[slot]->l->count == 0 &&
          f->worlds[slot]->fields.c->count == 0 &&
          mc_live_tile_entities_map_valid(f->chunks[slot]) &&
          mc_live_tile_entities_lists_valid(f->worlds[slot]));
    ok = true;
cleanup:
    return ok;
}

static uint32_t piston_be32(const uint8_t *bytes) {
    return ((uint32_t)bytes[0] << 24u) | ((uint32_t)bytes[1] << 16u) | ((uint32_t)bytes[2] << 8u) |
           bytes[3];
}

static bool piston_persistence(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *const world = f->worlds[side];
    const size_t dimension_binding = world->persistent_bindings;
    mc_tile_entity *tile_entity = NULL, *lookup = NULL, *loaded = NULL;
    mc_nbt_tree *records = NULL, *raw_progress_records = NULL;
    mc_nbt_document *document = NULL, *canonical_progress = NULL;
    mc_world_regions *disk = NULL;
    mc_compression_buffer payload = {0};
    mc_ws_file_bytes raw = {0};
    char *region = NULL;
    mc_live_tile_entity_error error;
    mc_tile_entity_snapshot snapshot;
    piston_record source;
    piston_prepare(&source);
    CHECK(dimension_binding == 1); /* Actual bound Dimension precedes this TileEntity. */
    source.fields[1].value.int_value = 8;
    source.fields[2].value.int_value = 64;
    source.fields[3].value.int_value = 8;
    source.fields[4].value.int_value = 1;
    source.fields[5].value.int_value = 9;
    source.fields[6].value.int_value = 5;
    source.fields[7].value.float_bits = UINT32_C(0x3e800000);
    source.fields[8].value.byte_value = 0;
    CHECK(mc_live_tile_entities_factory(world, &source.root, NULL, true, &tile_entity, &error) ==
              MC_WORLD_STORAGE_OK &&
          tile_entity != NULL &&
          mc_tile_entity_get_snapshot(tile_entity, &snapshot) == MC_TILE_ENTITY_OK &&
          piston_base_is(&snapshot, NULL, 8, 64, 8, false) &&
          piston_state_is(&snapshot, 1, 9, 5, false, false, UINT32_C(0x3e800000),
                          UINT32_C(0x3e800000)));
    /* Real fixture cell state, not a moving-piston added/tick callback. Its
     * ordinary lazy factory is genuinely null; this actual registered NBT
     * factory supplied the positive TileEntity installed below. */
    f->states[side]->data->blocks[cell(8, 64, 8)] = 36;
    CHECK(mc_live_tile_entities_set(world, 8, 64, 8, tile_entity, &error) == MC_WORLD_STORAGE_OK &&
          f->chunks[side]->l->count == 1 && world->fields.c->count == 1 &&
          mc_live_tile_entities_map_valid(f->chunks[side]) &&
          mc_live_tile_entities_lists_valid(world) &&
          mc_live_tile_entities_get(world, 8, 64, 8, &lookup, &error) == MC_WORLD_STORAGE_OK &&
          lookup == tile_entity &&
          mc_tile_entity_get_snapshot(tile_entity, &snapshot) == MC_TILE_ENTITY_OK &&
          piston_base_is(&snapshot, world, 8, 64, 8, false));
    mc_tile_entity_release(lookup);
    lookup = NULL;
    CHECK(tile_entity->holds == 3 && world->persistent_bindings == dimension_binding + 1u);
    uint32_t refused_growth = UINT32_MAX;
    mc_world_storage_error refusal = {0};
    CHECK(mc_world_store_write_runtime_chunk(f->stores[side], f->states[side], 2000 + (int64_t)side,
                                             UINT32_C(0x12345678), &refused_growth,
                                             &refusal) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          refused_growth == UINT32_MAX && refusal.operation != NULL &&
          strcmp(refusal.operation, "opaque entity or TileEntity runtime save") == 0);
    CHECK(mc_world_info_time_set(world->fields.x, 2000 + (int64_t)side) == MC_WORLD_INFO_OK);
    const size_t graph_before =
        mc_tile_entity_native_bytes(tile_entity) + mc_tile_entity_source_bytes(tile_entity);
    size_t graph = 0;
    CHECK(mc_tile_entity_storage_bytes(tile_entity, &graph) == MC_TILE_ENTITY_OK &&
          graph == graph_before);
    const uint32_t current = tile_entity->state.piston.progress_bits;
    const uint32_t previous = tile_entity->state.piston.previous_progress_bits;
    tile_entity->state.piston.progress_bits = UINT32_C(0x3f400000);
    tile_entity->state.piston.previous_progress_bits = UINT32_C(0x7f812345);
    const mc_world_storage_status raw_built =
        mc_live_tile_entities_build_records(f->chunks[side], NULL, &raw_progress_records, &error);
    mc_tile_entity_error codec_error;
    const mc_tile_entity_status canonical_built =
        mc_tile_entity_build(tile_entity, NULL, &canonical_progress, &codec_error);
    tile_entity->state.piston.previous_progress_bits = UINT32_C(0x3f000000);
    const mc_world_storage_status built =
        mc_live_tile_entities_build_records(f->chunks[side], NULL, &records, &error);
    uint32_t growth = UINT32_MAX;
    mc_world_storage_status written = built;
    mc_world_game_save_error save_error = {0};
    if (built == MC_WORLD_STORAGE_OK)
        written = mc_world_runtime_write_game_chunk(world, f->states[side], UINT32_C(0x12345678),
                                                    &growth, &save_error);
    tile_entity->state.piston.progress_bits = current;
    tile_entity->state.piston.previous_progress_bits = previous;
    CHECK(raw_built == MC_WORLD_STORAGE_OK);
    CHECK(canonical_built == MC_TILE_ENTITY_OK);
    CHECK(built == MC_WORLD_STORAGE_OK);
    CHECK(written == MC_WORLD_STORAGE_OK && save_error.status == MC_WORLD_STORAGE_OK);
    CHECK(mc_tile_entity_storage_bytes(tile_entity, &graph) == MC_TILE_ENTITY_OK &&
          graph == graph_before &&
          mc_tile_entity_get_snapshot(tile_entity, &snapshot) == MC_TILE_ENTITY_OK &&
          piston_state_is(&snapshot, 1, 9, 5, false, false, current, previous));
    /* Semantic writer bits and ordinary encoded-Float NaN policy are separate
     * joins. Raw previous-l SNaN stays in the tree; encode/read canonicalizes
     * it. Neither operation mutates the TileEntity's independent source provenance. */
    const mc_nbt_tag *raw_list = field(mc_nbt_tree_root(raw_progress_records), "TileEntities");
    const mc_nbt_tag *source_progress = field(mc_tile_entity_source(tile_entity), "progress");
    CHECK(raw_list != NULL && raw_list->type == MC_NBT_LIST && raw_list->value.list.length == 1 &&
          piston_written_is(raw_list->value.list.items[0], 8, 64, 8, 1, 9, 5, false,
                            UINT32_C(0x7f812345), true) &&
          piston_written_is(mc_nbt_document_root(canonical_progress), 8, 64, 8, 1, 9, 5, false,
                            UINT32_C(0x7fc00000), true) &&
          source_progress != NULL && source_progress->type == MC_NBT_FLOAT &&
          source_progress->value.float_bits == UINT32_C(0x3e800000));
    const mc_nbt_tag *list = field(mc_nbt_tree_root(records), "TileEntities");
    CHECK(list != NULL && list->type == MC_NBT_LIST &&
          list->value.list.element_type == MC_NBT_COMPOUND && list->value.list.length == 1 &&
          piston_written_is(list->value.list.items[0], 8, 64, 8, 1, 9, 5, false,
                            UINT32_C(0x3f000000), true));
    CHECK(mc_ws_path_join(mc_world_store_root(f->stores[side]), "region/r.0.0.mcr", &region,
                          NULL) == MC_WORLD_STORAGE_OK &&
          mc_ws_read_file(region, 1024u * 1024u, &raw, NULL) == MC_WORLD_STORAGE_OK &&
          raw.length >= MC_REGION_HEADER_BYTES &&
          piston_be32(raw.data + MC_REGION_SECTOR_BYTES) == UINT32_C(0x12345678) &&
          mc_world_regions_create(NULL, &disk, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_regions_read(disk, mc_world_store_root(f->stores[side]), 0, 0, &payload, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_nbt_read_compound(payload.data, payload.length, NULL, &document, NULL, NULL) ==
              MC_NBT_OK);
    const mc_nbt_tag *level = field(mc_nbt_document_root(document), "Level");
    const mc_nbt_tag *time = field(level, "LastUpdate"), *blocks = field(level, "Blocks");
    const mc_nbt_tag *entities = field(level, "Entities");
    const mc_nbt_tag *persisted = field(level, "TileEntities");
    CHECK(time != NULL && time->type == MC_NBT_LONG &&
          time->value.long_value == 2000 + (int64_t)side && blocks != NULL &&
          blocks->type == MC_NBT_BYTE_ARRAY && blocks->value.byte_array.length == MC_CHUNK_BLOCKS &&
          blocks->value.byte_array.data[cell(8, 64, 8)] == 36 && entities != NULL &&
          entities->type == MC_NBT_LIST && entities->value.list.length == 0 && persisted != NULL &&
          persisted->type == MC_NBT_LIST && persisted->value.list.length == 1 &&
          piston_written_is(persisted->value.list.items[0], 8, 64, 8, 1, 9, 5, false,
                            UINT32_C(0x3f000000), true));
    CHECK(mc_live_tile_entities_factory(world, persisted->value.list.items[0], NULL, false, &loaded,
                                        &error) == MC_WORLD_STORAGE_OK &&
          loaded != NULL && loaded != tile_entity &&
          mc_tile_entity_get_snapshot(loaded, &snapshot) == MC_TILE_ENTITY_OK &&
          piston_base_is(&snapshot, NULL, 8, 64, 8, false) &&
          piston_state_is(&snapshot, 1, 9, 5, false, false, UINT32_C(0x3f000000),
                          UINT32_C(0x3f000000)) &&
          mc_tile_entity_source(loaded) == NULL);
    /* Keep one actual external alias for the subsequent joint retirement
     * checks. This transfers the caller hold, without manufacturing any hold. */
    f->aliases[side] = tile_entity;
    tile_entity = NULL;
    ok = true;
cleanup:
    mc_tile_entity_release(tile_entity);
    mc_tile_entity_release(lookup);
    mc_tile_entity_release(loaded);
    mc_nbt_tree_free(records);
    mc_nbt_tree_free(raw_progress_records);
    mc_nbt_document_free(document);
    mc_nbt_document_free(canonical_progress);
    mc_compression_buffer_free(&payload);
    mc_world_regions_free(disk);
    mc_ws_free_file_bytes(&raw);
    free(region);
    return ok;
}

static bool piston_joint_retirement(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *const world = f->worlds[side];
    mc_tile_entity *const tile_entity = f->aliases[side];
    mc_tile_entity_snapshot before, after;
    mc_tile_entity_piston_class_snapshot classes;
    CHECK(tile_entity != NULL &&
          mc_tile_entity_get_snapshot(tile_entity, &before) == MC_TILE_ENTITY_OK &&
          mc_world_store_lease_chunk(f->stores[side], f->states[side], NULL) ==
              MC_WORLD_STORAGE_OK);
    const mc_world_storage_status busy = mc_world_runtime_retire_entity_containers(world, NULL);
    const mc_world_storage_status released =
        mc_world_store_release_chunk(f->stores[side], f->states[side], NULL);
    CHECK(busy == MC_WORLD_STORAGE_BUSY && released == MC_WORLD_STORAGE_OK &&
          f->chunks[side]->l->count == 1 && world->fields.c->count == 1 && tile_entity->holds == 3);
    const mc_world_storage_status retired = mc_world_runtime_retire_entity_containers(world, NULL);
    if (retired == MC_WORLD_STORAGE_OK)
        f->terminal[side] = true;
    CHECK(retired == MC_WORLD_STORAGE_OK && f->chunks[side]->l->count == 0 &&
          world->fields.c->count == 0 && world->fields.G->count == 0 && tile_entity->holds == 1 &&
          world->persistent_bindings == 2 &&
          mc_tile_entity_get_snapshot(tile_entity, &after) == MC_TILE_ENTITY_OK &&
          piston_base_is(&after, world, before.x, before.y, before.z, before.invalid) &&
          piston_state_is(&after, before.piston_block_id, before.piston_block_data,
                          before.piston_facing, before.piston_extending, before.piston_render_head,
                          before.piston_progress_bits, before.piston_previous_progress_bits) &&
          mc_world_runtime_free(world, NULL) == MC_WORLD_STORAGE_BUSY &&
          mc_tile_entity_piston_class_get_snapshot(f->classes[side], &classes) ==
              MC_TILE_ENTITY_OK &&
          classes.piston_phase == MC_TILE_ENTITY_CLASS_READY && classes.live_tile_entities == 1 &&
          classes.list_identity != NULL && mc_live_tile_entities_map_valid(f->chunks[side]) &&
          mc_live_tile_entities_lists_valid(world));
    ok = true;
cleanup:
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    const uint16_t title[] = {'P', 'i', 's', 't', 'o', 'n', 's'};
    if (argc != 3 || strcmp(argv[1], "world-piston-tile-entities") != 0 ||
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
        r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
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
        f->chest_id[side] = 54;
        f->spawner_id[side] = 52;
    }
    for (size_t slot = 0; slot < WORLD_COUNT; ++slot)
        CHECK(open_store(f, slot, root) && create_world(f, slot));
    for (size_t side = 0; side < 2; ++side)
        CHECK(piston_class_failure(f, side) && piston_constructors(f, side) &&
              piston_class_identity(f, side) && piston_read_prefixes(f, side) &&
              piston_writer_and_provenance(f, side) && piston_load_empty_chunk(f, side) &&
              piston_persistence(f, side));
    for (size_t side = 0; side < 2; ++side)
        CHECK(piston_joint_retirement(f, side));
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("Piston TileEntity cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    if (result == 0 &&
        printf("PASS: %u Piston TileEntity checks (world-piston-tile-entities)\n", checks) < 0)
        result = 1;
    return result;
}
