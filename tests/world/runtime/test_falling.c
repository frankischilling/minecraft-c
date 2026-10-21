/* Source-only constructed native FallingSand contracts. The included fixture
 * provides actual transport/owners and terminal cleanup, never a fake backend.
 * This main uses its own serialized input and does not run the fluid trials. */
#define main mc_fluids_reference_main
#include "test_fluids.c"
#undef main

#include "../../../src/entity/entity_internal.h"
#include "mc/block/registry/falling.h"

/* Every prepared value is written before genuine GAME loading. The floor is
 * stone; separated unsupported columns avoid an artificial falling neighbor.
 * Numerical stored IDs are explicit fixture input, not a production palette. */
static input_cell falling_input(int32_t x, int32_t y, int32_t z) {
    if (y == 4)
        return (input_cell){1, 0};
    if (y == 50 && z == 4) {
        if (x == 5)
            return (input_cell){51, 0};
        if (x == 6)
            return (input_cell){9, 0};
        if (x == 7)
            return (input_cell){11, 0};
        if (x == 8)
            return (input_cell){1, 0};
    }
    if ((x == 9 && z == 9 && y == 64) || (x == 12 && z == 9 && y == 80) ||
        (x == 9 && z == 16 && y == 48) || (x == 12 && z == 16 && y == 64) ||
        (x == 9 && z == 12 && y == 0) || (x == 14 && z == 16 && y == 5))
        return (input_cell){(uint8_t)(x == 12 && z == 9 ? 13 : 12), 0};
    return (input_cell){0, 0};
}

static bool falling_seed(fixture *f, size_t slot, int32_t cx, int32_t cz) {
    bool ok = false;
    fixture_record *r = calloc(1, sizeof(*r));
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
    r->root.type = MC_NBT_COMPOUND;
    r->level.type = MC_NBT_COMPOUND;
    r->level.name = ascii("Level", r->names[11]);
    for (size_t i = 0; i < COUNT(names); ++i) {
        r->fields[i].type = types[i];
        r->fields[i].name = ascii(names[i], r->names[i]);
        r->level_fields[i] = &r->fields[i];
    }
    r->fields[0].value.int_value = cx;
    r->fields[1].value.int_value = cz;
    r->fields[2].value.long_value = -123;
    r->fields[8].value.byte_value = 1;
    for (int32_t x = 0; x < 16; ++x)
        for (int32_t z = 0; z < 16; ++z)
            for (int32_t y = 0; y < 128; ++y)
                r->arrays[0][cell(x, y, z)] = falling_input(cx * 16 + x, y, cz * 16 + z).id;
    for (size_t i = 0; i < COUNT(lengths); ++i) {
        r->fields[i + 3u].value.byte_array.data = r->arrays[i];
        r->fields[i + 3u].value.byte_array.length = lengths[i];
    }
    r->fields[9].value.list.element_type = MC_NBT_COMPOUND;
    r->fields[10].value.list.element_type = MC_NBT_COMPOUND;
    r->level.value.compound.items = r->level_fields;
    r->level.value.compound.length = COUNT(r->level_fields);
    r->root_fields[0] = &r->level;
    r->root.value.compound.items = r->root_fields;
    r->root.value.compound.length = 1;
    CHECK(mc_nbt_measure(&r->root, NULL, &length, NULL) == MC_NBT_OK && length != 0);
    encoded = malloc(length);
    CHECK(encoded != NULL &&
          mc_nbt_write(&r->root, NULL, encoded, length, &written, NULL) == MC_NBT_OK &&
          written == length);
    uint32_t growth = 0;
    CHECK(mc_world_regions_write(f->regions, mc_world_store_root(f->stores[slot]), cx, cz, encoded,
                                 length, fixture_timestamp, &growth, NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    free(encoded);
    free(r);
    return ok;
}

static bool falling_load(fixture *f, size_t slot) {
    bool ok = false;
    for (int32_t x = 0; x < 2; ++x)
        for (int32_t z = 0; z < 2; ++z)
            CHECK(falling_seed(f, slot, x, z));
    for (int32_t x = 0; x < 2; ++x)
        for (int32_t z = 0; z < 2; ++z) {
            mc_chunk_state *state = NULL;
            const mc_chunk_acquire_status status =
                mc_world_runtime_get(f->worlds[slot], x, z, &state);
            CHECK(remember(f, slot));
            CHECK(status == MC_CHUNK_ACQUIRE_OK && state != NULL && state->active &&
                  state->terrain_populated && state->x == x && state->z == z);
            f->chunks[slot][x][z] = state;
        }
    CHECK(f->providers[slot].cache->count == 4 && f->providers[slot].cache->insertion_count == 4 &&
          mc_world_store_chunk_count(f->stores[slot]) == 4);
    ok = true;
cleanup:
    return ok;
}

/* These are internal held bodies. Acquire a genuine World call hold, start a
 * fresh real frame, and leave on both success and failure. No source effects,
 * output rewriting or fallback happen in this adapter. */
static bool falling_call(mc_world_runtime *w, const mc_block *block, int32_t x, int32_t y,
                         int32_t z, mc_java_random *random, mc_world_mutation_error *error) {
    if (mc_world_runtime_call_enter(w, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    *error = mc_world_mutation_empty_error();
    const bool ok = mc_world_mutation_falling_tick(w, block, x, y, z, random, error);
    mc_world_runtime_call_leave(w);
    return ok;
}

static bool falling_pass(mc_world_runtime *w, mc_registry_classes *classes, int32_t x, int32_t y,
                         int32_t z, bool *out, mc_world_mutation_error *error) {
    if (mc_world_runtime_call_enter(w, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    *error = mc_world_mutation_empty_error();
    const bool ok = mc_world_mutation_falling_can_pass(w, classes, x, y, z, out, error);
    mc_world_runtime_call_leave(w);
    return ok;
}

static bool falling_hook(mc_world_runtime *w, const mc_block *block, int32_t x, int32_t y,
                         int32_t z, bool neighbor, mc_world_mutation_error *error) {
    if (mc_world_runtime_call_enter(w, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    *error = mc_world_mutation_empty_error();
    const bool ok = neighbor
                        ? mc_world_mutation_falling_neighbor(w, block, x, y, z, INT32_MIN, error)
                        : mc_world_mutation_falling_added(w, block, x, y, z, error);
    mc_world_runtime_call_leave(w);
    return ok;
}

static bool falling_predicates(fixture *f, size_t side) {
    bool ok = false, out = false;
    mc_world_runtime *w = f->worlds[side];
    mc_registry_classes *r = f->registry[side];
    mc_world_mutation_error error;
    const mc_block *fire = NULL;
    CHECK(mc_registry_block_static_get(r, "as", &fire, NULL) == MC_REGISTRY_OK && fire != NULL &&
          fire->fields.bn == 51 && r->blocks.m[1] != NULL);
    size_t fire_index = 0;
    while (fire_index < COUNT(r->blocks.named) && r->blocks.named[fire_index] != fire)
        ++fire_index;
    CHECK(fire_index < COUNT(r->blocks.named));
    mc_block *stone = r->blocks.m[1];
    mc_block **const array = r->blocks.m;
    mc_material *const material = stone->fields.bA;
    bool flag = false;
    CHECK(mc_registry_falling_flag_get(r, &flag, NULL) == MC_REGISTRY_OK);
    CHECK(mc_registry_falling_flag_set(r, true, NULL) == MC_REGISTRY_OK);

    /* Air skips fire and m; matching live fire skips m. Restore every actual
     * field control before checking the resulting contract. */
    r->blocks.named[fire_index] = NULL;
    r->blocks.m = NULL;
    const bool air = falling_pass(w, r, 4, 50, 4, &out, &error);
    r->blocks.named[fire_index] = (mc_block *)fire;
    r->blocks.m = array;
    bool retained_flag = false;
    const mc_registry_status flag_status = mc_registry_falling_flag_get(r, &retained_flag, NULL);
    const mc_registry_status flag_restored = mc_registry_falling_flag_set(r, flag, NULL);
    CHECK(air && out && error.status == MC_WORLD_STORAGE_OK && flag_status == MC_REGISTRY_OK &&
          retained_flag && flag_restored == MC_REGISTRY_OK);
    out = false;
    r->blocks.m = NULL;
    const bool fire_ok = falling_pass(w, r, 5, 50, 4, &out, &error);
    r->blocks.m = array;
    CHECK(fire_ok && out);
    out = false;
    r->blocks.named[fire_index] = stone;
    r->blocks.m = NULL;
    const bool live_fire = falling_pass(w, r, 8, 50, 4, &out, &error);
    r->blocks.named[fire_index] = (mc_block *)fire;
    r->blocks.m = array;
    CHECK(live_fire && out);
    out = false;
    CHECK(falling_pass(w, r, 6, 50, 4, &out, &error) && out);
    out = false;
    CHECK(falling_pass(w, r, 7, 50, 4, &out, &error) && out);
    out = true;
    CHECK(falling_pass(w, r, 8, 50, 4, &out, &error) && !out);

    out = true;
    stone->fields.bA = NULL;
    const bool nullable = falling_pass(w, r, 8, 50, 4, &out, &error);
    stone->fields.bA = material;
    CHECK(nullable && !out && error.status == MC_WORLD_STORAGE_OK);

    out = true;
    r->blocks.named[fire_index] = NULL;
    const bool no_fire = falling_pass(w, r, 8, 50, 4, &out, &error);
    r->blocks.named[fire_index] = (mc_block *)fire;
    CHECK(!no_fire && out && error.falling_predicate_bytecode == 21 &&
          error.registry.status == MC_REGISTRY_NULL_REFERENCE);
    r->blocks.m = NULL;
    const bool no_array = falling_pass(w, r, 8, 50, 4, &out, &error);
    r->blocks.m = array;
    CHECK(!no_array && out && error.falling_predicate_bytecode == 34 &&
          error.registry.status == MC_REGISTRY_NULL_REFERENCE);
    r->blocks.m[1] = NULL;
    const bool no_block = falling_pass(w, r, 8, 50, 4, &out, &error);
    r->blocks.m[1] = stone;
    CHECK(!no_block && out && error.falling_predicate_bytecode == 35 &&
          error.registry.status == MC_REGISTRY_NULL_REFERENCE);
    bool after = !flag;
    CHECK(mc_registry_falling_flag_get(r, &after, NULL) == MC_REGISTRY_OK && after == flag &&
          f->providers[side].cache->count == 4 && w->active_calls == 0 && w->local_leases == 0);
    ok = true;
cleanup:
    return ok;
}

static bool falling_moves(fixture *f, size_t side) {
    bool ok = false, flag = false;
    mc_world_runtime *w = f->worlds[side];
    mc_registry_classes *r = f->registry[side];
    const mc_block *sand = NULL, *gravel = NULL;
    mc_world_mutation_error error;
    mc_java_random supplied;
    mc_java_random_seed(&supplied, INT64_C(-78031));
    const mc_java_random original = supplied, world_random = *w->fields.r;
    const mc_java_random math = math_snapshot(f);
    CHECK(mc_registry_block_get(r, 12, &sand, NULL) == MC_REGISTRY_OK && sand != NULL &&
          mc_registry_block_get(r, 13, &gravel, NULL) == MC_REGISTRY_OK && gravel != NULL &&
          mc_registry_falling_flag_get(r, &flag, NULL) == MC_REGISTRY_OK);
    CHECK(at(w, 9, 64, 9, 12, 0) && at(w, 12, 80, 9, 13, 0));
    CHECK(mc_registry_falling_flag_set(r, true, NULL) == MC_REGISTRY_OK);
    const bool moved = falling_call(w, sand, 9, 64, 9, &supplied, &error);
    const mc_registry_status restored = mc_registry_falling_flag_set(r, flag, NULL);
    CHECK(restored == MC_REGISTRY_OK && moved && at(w, 9, 64, 9, 0, 0) && at(w, 9, 5, 9, 12, 0) &&
          at(w, 9, 4, 9, 1, 0));

    /* False flag reaches real radius32 absence in the four-Chunk footprint,
     * then the same immediate body. NULL Random is genuinely unused. */
    CHECK(mc_registry_falling_flag_set(r, false, NULL) == MC_REGISTRY_OK);
    const bool absent = falling_call(w, gravel, 12, 80, 9, NULL, &error);
    const mc_registry_status restored_absent = mc_registry_falling_flag_set(r, flag, NULL);
    CHECK(restored_absent == MC_REGISTRY_OK && absent && at(w, 12, 80, 9, 0, 0) &&
          at(w, 12, 5, 9, 13, 0) && f->providers[side].cache->count == 4);

    CHECK(mc_registry_falling_flag_set(r, true, NULL) == MC_REGISTRY_OK);
    const bool y_zero = falling_call(w, sand, 9, 0, 12, NULL, &error);
    const mc_registry_status restored_zero = mc_registry_falling_flag_set(r, flag, NULL);
    CHECK(restored_zero == MC_REGISTRY_OK && y_zero && at(w, 9, 0, 12, 0, 0));
    CHECK(falling_call(w, sand, 9, -1, 12, NULL, &error));
    /* Literal NULL is a legal source argument until its reached World query.
     * Keep genuine held Block/Registry owners; no World owner exists to hold.
     * This distinguishes predicate-before-Y from an early negative-Y return. */
    error = mc_world_mutation_empty_error();
    const bool negative_null = mc_world_mutation_falling_tick(NULL, sand, 9, -1, 12, NULL, &error);
    CHECK(!negative_null && error.status == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          error.falling_tick_bytecode == 6 && error.falling_bytecode == 19 &&
          error.falling_predicate_bytecode == 4);
    CHECK(falling_call(w, sand, 14, 5, 16, NULL, &error) && at(w, 14, 5, 16, 12, 0));

    /* All notifying setters return ordinary false outside World X bounds, but
     * the source must continue to Y0 and return normally without an Entity. */
    mc_entity_class_snapshot before = {0}, after = {0};
    CHECK(mc_entity_classes_get_snapshot(f->entities[side], &before) == MC_ENTITY_OK &&
          mc_registry_falling_flag_set(r, true, NULL) == MC_REGISTRY_OK);
    const bool discarded_false = falling_call(w, sand, INT32_MAX, 2, 9, NULL, &error);
    const mc_registry_status restored_false = mc_registry_falling_flag_set(r, flag, NULL);
    CHECK(restored_false == MC_REGISTRY_OK && discarded_false &&
          mc_entity_classes_get_snapshot(f->entities[side], &after) == MC_ENTITY_OK &&
          before.next_id == after.next_id && before.live_entities == after.live_entities &&
          mc_test_java_random_equal(&supplied, &original) &&
          mc_test_java_random_equal(w->fields.r, &world_random) && math_is(f, &math) &&
          w->active_calls == 0 && w->local_leases == 0);
    ok = true;
cleanup:
    return ok;
}

static bool falling_schedules(fixture *f, size_t side) {
    bool ok = false, flag = false;
    mc_world_runtime *w = f->worlds[side];
    mc_registry_classes *r = f->registry[side];
    const mc_block *sand = NULL;
    mc_world_mutation_error error;
    int64_t time = 0;
    CHECK(mc_registry_block_get(r, 12, &sand, NULL) == MC_REGISTRY_OK && sand != NULL &&
          mc_registry_falling_flag_get(r, &flag, NULL) == MC_REGISTRY_OK &&
          mc_world_info_time_get(w->fields.x, &time) == MC_WORLD_INFO_OK);
    schedule_state before = {0};
    CHECK(scheduled(w, &before));
    CHECK(mc_world_info_time_set(w->fields.x, INT64_MAX - 1) == MC_WORLD_INFO_OK &&
          mc_registry_falling_flag_set(r, true, NULL) == MC_REGISTRY_OK);
    const bool added = falling_hook(w, sand, 18, 70, 18, false, &error);
    const bool neighbor = falling_hook(w, sand, 18, 70, 18, true, &error);
    const mc_world_info_status restored_time = mc_world_info_time_set(w->fields.x, time);
    const mc_registry_status restored_flag = mc_registry_falling_flag_set(r, flag, NULL);
    CHECK(restored_time == MC_WORLD_INFO_OK && restored_flag == MC_REGISTRY_OK && added &&
          neighbor && delta(w, &before, 2, 1));
    const mc_scheduled_tick *entries[128];
    size_t written = 0, matches = 0;
    CHECK(mc_scheduled_ordered_snapshot(w->fields.E, entries, COUNT(entries), &written) ==
          MC_SCHEDULED_ORDERED_OK);
    for (size_t i = 0; i < written; ++i) {
        mc_scheduled_tick_fields record;
        CHECK(mc_scheduled_tick_fields_get(entries[i], &record) == MC_SCHEDULED_TICK_OK);
        if (record.x == 18 && record.y == 70 && record.z == 18 && record.block == 12) {
            CHECK(record.due_time == mc_java_i64_add(INT64_MAX - 1, 3) &&
                  record.sequence == before.next);
            ++matches;
        }
    }
    CHECK(matches == 1);

    /* Real source NULL E is reached only after successful F publication. The
     * independent original E owner is restored; the escaped F record remains. */
    CHECK(scheduled(w, &before));
    mc_scheduled_ordered *ordered = w->fields.E;
    w->fields.E = NULL;
    const bool no_e = falling_hook(w, sand, 19, 71, 18, false, &error);
    w->fields.E = ordered;
    schedule_state after;
    CHECK(!no_e && error.schedule.bytecode == 231 &&
          error.schedule.ordered == MC_SCHEDULED_ORDERED_INVALID_ARGUMENT && scheduled(w, &after) &&
          after.next == mc_java_i64_add(before.next, 1) && after.e == before.e &&
          after.f == before.f + 1);

    /* World.a selects immediate execution, independently of gk_a. Source Info,
     * F/E and Random are not used when the actual below block stops the tick. */
    CHECK(scheduled(w, &before));
    const bool immediate = w->fields.a;
    mc_world_info *info = w->fields.x;
    mc_scheduled_membership *membership = w->fields.F;
    mc_java_random *random = w->fields.r;
    w->fields.a = true;
    w->fields.x = NULL;
    w->fields.F = NULL;
    w->fields.E = NULL;
    w->fields.r = NULL;
    const bool immediate_ok = falling_hook(w, sand, 14, 5, 16, true, &error);
    w->fields.a = immediate;
    w->fields.x = info;
    w->fields.F = membership;
    w->fields.E = ordered;
    w->fields.r = random;
    CHECK(immediate_ok && delta(w, &before, 1, 0) && at(w, 14, 5, 16, 12, 0));

    /* The nested tick fails at fire.bn21 BEFORE testing stone's Material.
     * Keep scheduler142 and private/tick/predicate sites simultaneously. */
    const mc_block *fire = NULL;
    CHECK(mc_registry_block_static_get(r, "as", &fire, NULL) == MC_REGISTRY_OK && fire != NULL);
    size_t fire_index = 0;
    while (fire_index < COUNT(r->blocks.named) && r->blocks.named[fire_index] != fire)
        ++fire_index;
    CHECK(fire_index < COUNT(r->blocks.named) && scheduled(w, &before));
    w->fields.a = true;
    r->blocks.named[fire_index] = NULL;
    const bool nested = falling_hook(w, sand, 14, 5, 16, false, &error);
    r->blocks.named[fire_index] = (mc_block *)fire;
    w->fields.a = immediate;
    CHECK(!nested && error.schedule.bytecode == 142 && error.falling_tick_bytecode == 6 &&
          error.falling_bytecode == 19 && error.falling_predicate_bytecode == 21 &&
          error.registry.status == MC_REGISTRY_NULL_REFERENCE && delta(w, &before, 1, 0) &&
          w->active_calls == 0 && w->local_leases == 0);
    ok = true;
cleanup:
    return ok;
}

static bool falling_failure_prefix(fixture *f, size_t side) {
    bool ok = false, flag = false;
    mc_world_runtime *w = f->worlds[side];
    mc_registry_classes *r = f->registry[side];
    const mc_block *sand = NULL;
    mc_world_mutation_error error;
    CHECK(mc_registry_block_get(r, 12, &sand, NULL) == MC_REGISTRY_OK && sand != NULL &&
          mc_registry_falling_flag_get(r, &flag, NULL) == MC_REGISTRY_OK &&
          at(w, 12, 64, 16, 12, 0));
    mc_world_reference_list *observers = w->fields.u;
    CHECK(mc_registry_falling_flag_set(r, true, NULL) == MC_REGISTRY_OK);
    w->fields.u = NULL;
    const bool completed = falling_call(w, sand, 12, 64, 16, NULL, &error);
    w->fields.u = observers;
    const mc_registry_status restored = mc_registry_falling_flag_set(r, flag, NULL);
    CHECK(restored == MC_REGISTRY_OK && !completed && error.status != MC_WORLD_STORAGE_OK &&
          error.falling_tick_bytecode == 6 && error.falling_bytecode == 79 &&
          error.observer_bytecode == 9 && at(w, 12, 64, 16, 0, 0) && at(w, 12, 5, 16, 0, 0) &&
          w->active_calls == 0 && w->local_leases == 0);
    ok = true;
cleanup:
    return ok;
}

static bool falling_quota(fixture *f, size_t slot) {
    bool ok = false, flag = false;
    const size_t side = slot % 2u;
    mc_world_runtime *w = f->worlds[slot];
    mc_registry_classes *r = f->registry[side];
    const mc_block *sand = NULL;
    mc_world_mutation_error error;
    schedule_state before = {0};
    CHECK(mc_registry_block_get(r, 12, &sand, NULL) == MC_REGISTRY_OK && sand != NULL &&
          scheduled(w, &before) && before.e == 0 && before.f == 0);
    const bool queued = falling_hook(w, sand, 18, 70, 18, false, &error);
    CHECK(!queued && error.status == MC_WORLD_STORAGE_LIMIT && error.schedule.bytecode == 219 &&
          error.schedule.membership == MC_SCHEDULED_MEMBERSHIP_LIMIT && delta(w, &before, 1, 0));

    /* Constructor-configured zero-entry F/E budget, no live limit mutation.
     * Final placement stores sand BEFORE added's real scheduler failure. */
    CHECK(scheduled(w, &before) && mc_registry_falling_flag_get(r, &flag, NULL) == MC_REGISTRY_OK &&
          mc_registry_falling_flag_set(r, true, NULL) == MC_REGISTRY_OK);
    const bool moved = falling_call(w, sand, 9, 64, 9, NULL, &error);
    const mc_registry_status restored = mc_registry_falling_flag_set(r, flag, NULL);
    CHECK(restored == MC_REGISTRY_OK && !moved && error.status == MC_WORLD_STORAGE_LIMIT &&
          error.falling_tick_bytecode == 6 && error.falling_bytecode == 119 &&
          error.schedule.bytecode == 219 &&
          error.schedule.membership == MC_SCHEDULED_MEMBERSHIP_LIMIT && at(w, 9, 64, 9, 0, 0) &&
          at(w, 9, 5, 9, 12, 0) && delta(w, &before, 1, 0) && w->active_calls == 0 &&
          w->local_leases == 0);
    ok = true;
cleanup:
    return ok;
}

static bool falling_entities(fixture *f, size_t side) {
    bool ok = false, published = false, flag = false;
    mc_entity *entity = NULL, *world_only = NULL;
    mc_world_runtime *w = f->worlds[side];
    mc_registry_classes *r = f->registry[side];
    const mc_block *sand = NULL;
    const mc_entity_class_literal *literal = NULL;
    mc_entity_snapshot snapshot;
    mc_world_entity_error publication;
    const mc_java_random math = math_snapshot(f);
    CHECK(mc_entity_class_literal_get(f->entities[side], MC_ENTITY_CLASS_KIND_FALLING_SAND,
                                      &literal) == MC_ENTITY_OK);
    CHECK(mc_falling_entity_allocate(f->entities[side], &world_only, NULL) == MC_ENTITY_OK &&
          mc_falling_entity_construct_world(world_only, NULL, NULL) == MC_ENTITY_OK &&
          mc_entity_get_snapshot(world_only, &snapshot) == MC_ENTITY_OK &&
          snapshot.class_literal == literal && snapshot.falling_bytecode == 10 &&
          snapshot.item_bytecode == 0 && snapshot.falling.a == 0 && snapshot.falling.b == 0 &&
          snapshot.base.width == 0.6f && snapshot.base.height == 1.8f && snapshot.stack == NULL);
    mc_entity_release(world_only);
    world_only = NULL;
    CHECK(mc_falling_entity_allocate(f->entities[side], &entity, NULL) == MC_ENTITY_OK &&
          mc_falling_entity_construct(entity, w, 9.5, 64.5, 9.5, 12, NULL) == MC_ENTITY_OK &&
          mc_entity_get_snapshot(entity, &snapshot) == MC_ENTITY_OK &&
          snapshot.class_literal == literal && snapshot.falling_bytecode == 80 &&
          snapshot.item_bytecode == 0 && snapshot.falling.a == 12 && snapshot.falling.b == 0 &&
          snapshot.stack == NULL && snapshot.base.aF && snapshot.base.world == w &&
          snapshot.base.width == 0.98f && snapshot.base.height == 0.98f &&
          snapshot.base.offset == snapshot.base.height / 2.0f && snapshot.base.x == 9.5 &&
          snapshot.base.y == 64.5 && snapshot.base.z == 9.5 && snapshot.base.aJ == 9.5 &&
          snapshot.base.aK == 64.5 && snapshot.base.aL == 9.5);
    CHECK(w->fields.b->count == 0 &&
          mc_world_runtime_insert_entity(w, entity, &published, &publication) ==
              MC_WORLD_STORAGE_OK &&
          published && w->fields.b->count == 1 && w->fields.b->items[0] == entity);
    mc_world_chunk *chunk = mc_world_store_chunk_object(f->stores[side], f->chunks[side][0][0]);
    mc_world_chunk_fields fields;
    const mc_world_chunk_entity_list *slice = NULL;
    mc_entity *occurrence = NULL;
    size_t count = 0;
    CHECK(chunk != NULL && mc_world_chunk_fields_get(chunk, &fields) == MC_WORLD_STORAGE_OK &&
          fields.q && mc_world_chunk_slice_at(fields.m, 4, &slice) == MC_WORLD_STORAGE_OK &&
          mc_world_chunk_entity_count(slice, &count) == MC_WORLD_STORAGE_OK && count == 1 &&
          mc_world_chunk_entity_at(slice, 0, &occurrence) == MC_WORLD_STORAGE_OK &&
          occurrence == entity && math_is(f, &math));
    mc_entity_release(entity);
    entity = NULL;

    /* INT_MAX +/-32 wraps to a reversed X Chunk interval. The genuine source
     * region loop is empty/true; no injected presence or 25-Chunk fiction.
     * Constructor executes, then actual nonplayer World.b returns false for
     * the absent target Chunk. Its ignored false must not fail the Block tick. */
    mc_entity_class_snapshot before = {0}, after = {0};
    CHECK(mc_registry_block_get(r, 12, &sand, NULL) == MC_REGISTRY_OK && sand != NULL &&
          mc_registry_falling_flag_get(r, &flag, NULL) == MC_REGISTRY_OK &&
          mc_entity_classes_get_snapshot(f->entities[side], &before) == MC_ENTITY_OK &&
          mc_registry_falling_flag_set(r, false, NULL) == MC_REGISTRY_OK);
    mc_world_mutation_error error;
    /* The same wrapped-X path with Z0 aliases the genuine cached Chunk0,0.
     * Keep its real wrong-location failure after construction/publication87. */
    const bool aliased = falling_call(w, sand, INT32_MAX, 64, 9, NULL, &error);
    CHECK(!aliased && error.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.falling_bytecode == 162 && error.publication.bytecode == 87 &&
          error.publication.chunk_bytecode == 45 &&
          mc_entity_classes_get_snapshot(f->entities[side], &after) == MC_ENTITY_OK &&
          after.next_id == mc_java_i32_add(before.next_id, 1) &&
          after.live_entities == before.live_entities &&
          after.allocation_bytes == before.allocation_bytes && w->fields.b->count == 1 &&
          math_is(f, &math) && w->active_calls == 0 && w->local_leases == 0);
    before = after;
    /* Chunk X aliases zero in the source packed key. Select an absent Z10
     * as well, so publication really takes the intended absent-chunk branch. */
    const bool wrapped = falling_call(w, sand, INT32_MAX, 64, 160, NULL, &error);
    const mc_registry_status restored = mc_registry_falling_flag_set(r, flag, NULL);
    CHECK(restored == MC_REGISTRY_OK && wrapped && error.status == MC_WORLD_STORAGE_OK &&
          mc_entity_classes_get_snapshot(f->entities[side], &after) == MC_ENTITY_OK &&
          after.next_id == mc_java_i32_add(before.next_id, 1) &&
          after.live_entities == before.live_entities &&
          after.allocation_bytes == before.allocation_bytes && w->fields.b->count == 1 &&
          math_is(f, &math) && f->providers[side].cache->count == 4 &&
          mc_world_store_chunk_count(f->stores[side]) == 4 && w->active_calls == 0 &&
          w->local_leases == 0);
    ok = true;
cleanup:
    mc_entity_release(world_only);
    mc_entity_release(entity);
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    const uint16_t title[] = {'F', 'a', 'l', 'l', 'i', 'n', 'g'};
    if (argc != 3 || strcmp(argv[1], "world-falling") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    for (size_t i = 0; i < COUNT(f->stores); ++i)
        CHECK(open_store(f, i, root));
    for (size_t side = 0; side < 2; ++side) {
        resources *r = &f->sources[side];
        r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
        r->streams[1] = (stream){.bytes = language};
        r->streams[2] = (stream){.bytes = language};
        r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
        const mc_registry_options options = {side == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                                             f->profile, f->materials, 0, &r->api};
        CHECK(mc_registry_classes_allocate(&options, &f->registry[side], NULL) == MC_REGISTRY_OK &&
              mc_entity_classes_create(side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile,
                                       NULL, &f->entities[side]) == MC_ENTITY_OK &&
              mc_world_runtime_classes_create(
                  side == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER, f->profile, &trig,
                  &f->classes[side], NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_classes_bind_registry(f->classes[side], f->registry[side], NULL) ==
                  MC_REGISTRY_OK &&
              mc_world_runtime_classes_bind_entities(f->classes[side], f->entities[side]) ==
                  MC_ENTITY_OK &&
              mc_registry_classes_initialize_blocks(f->registry[side], NULL) == MC_REGISTRY_OK);
    }
    for (size_t i = 0; i < COUNT(f->worlds); ++i)
        CHECK(create_world(f, i) && falling_load(f, i));
    double warmup;
    CHECK(mc_java_math_random(f->profile, &warmup) == MC_JAVA_RANDOM_PROFILE_OK);
    for (size_t side = 0; side < 2; ++side) {
        CHECK(falling_predicates(f, side));
        CHECK(falling_moves(f, side));
        CHECK(falling_schedules(f, side));
        CHECK(falling_failure_prefix(f, side));
        CHECK(falling_quota(f, side + 2u));
        CHECK(falling_entities(f, side));
    }
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("falling cleanup refused; live dependencies retained\n", stderr);
        return 1;
    }
    if (result == 0 && printf("PASS: %u World falling checks (world-falling)\n", checks) < 0)
        result = 1;
    return result;
}
