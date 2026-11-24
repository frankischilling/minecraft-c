/* Genuine GAME fixture; neither retained fixture entry point is executed. */
#define MC_PISTON_FINALIZE_MAIN mc_piston_tick_finalize_fixture_main
#include "test_piston_finalize.c"
#undef MC_PISTON_FINALIZE_MAIN

#include "../../../../src/block/registry/runtime_internal.h"
#include "../../../../src/entity/bounds/bounds_internal.h"
#include "../../../../src/world/runtime/collision/blocks_internal.h"
#include "../../../../src/world/runtime/collision/entities_internal.h"
#include "../../../../src/world/runtime/tile_entities/piston_list_internal.h"
#include <xmmintrin.h>

/* Constructed native contracts, not original tick/push replay qualification.
 * Raw progress/flags are source inputs. Temporary native ownership/descriptor,
 * FP and nullable-field faults restore before any CHECK can leave their scope.
 * Every query/publication/move uses the actual GAME owners and implementations. */
typedef struct tick_entities {
    mc_entity *items[64];
    size_t count;
} tick_entities;

static bool tick_empty(const mc_piston_tick_error *e) {
    return e->status == MC_WORLD_STORAGE_OK && e->operation == NULL && e->tick_bytecode == 0 &&
           e->push_bytecode == 0 && e->entity_index == SIZE_MAX &&
           e->entity.status == MC_ENTITY_OK && e->movement.status == MC_ENTITY_OK &&
           e->nearby.query.status == MC_WORLD_STORAGE_OK &&
           e->collision.nearby.query.status == MC_WORLD_STORAGE_OK &&
           fresh_registry(&e->registry, MC_REGISTRY_OK) && empty_tile_children(&e->tile_entity) &&
           finalize_empty(&e->mutation);
}

static bool tick_unused_slots_null(const mc_tile_entity_piston_class_snapshot *s) {
    if (s->list_identity == NULL || s->list_count > s->list_capacity ||
        (s->list_capacity != 0 && s->list_identity->items == NULL))
        return false;
    for (size_t i = s->list_count; i < s->list_capacity; ++i)
        if (s->list_identity->items[i] != NULL)
            return false;
    return true;
}

static bool tick_pre_world(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime_classes *classes = NULL;
    mc_tile_entity *tile_entity = NULL;
    mc_tile_entity_piston_class_snapshot s;
    mc_tile_entity_snapshot state;
    mc_world_runtime_class_state world_class;
    mc_registry_block_view blocks;
    mc_piston_tick_error error;
    CHECK(mc_tile_entity_allocate_in_classes(NULL, MC_TILE_ENTITY_PISTON, &tile_entity) ==
              MC_TILE_ENTITY_INVALID_ARGUMENT &&
          tile_entity == NULL);
    CHECK(
        mc_world_runtime_classes_create(side == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER,
                                        f->profile, &trig, &classes, NULL) == MC_WORLD_STORAGE_OK &&
        mc_world_runtime_classes_bind_registry(classes, f->registry[side], NULL) == MC_REGISTRY_OK);
    CHECK(mc_tile_entity_allocate_in_classes(classes, MC_TILE_ENTITY_PISTON, NULL) ==
              MC_TILE_ENTITY_INVALID_ARGUMENT &&
          mc_tile_entity_allocate_in_classes(classes, (mc_tile_entity_kind)99, &tile_entity) ==
              MC_TILE_ENTITY_INVALID_KIND &&
          tile_entity == NULL &&
          mc_tile_entity_piston_class_get_snapshot(classes, &s) == MC_TILE_ENTITY_OK &&
          s.base_phase == MC_TILE_ENTITY_CLASS_COLD && s.piston_phase == MC_TILE_ENTITY_CLASS_COLD);
    CHECK(mc_tile_entity_allocate_in_classes(classes, MC_TILE_ENTITY_PISTON, &tile_entity) ==
              MC_TILE_ENTITY_OK &&
          mc_tile_entity_piston_construct(tile_entity, 0, 3, 5, false, false, NULL) ==
              MC_TILE_ENTITY_OK &&
          mc_tile_entity_piston_class_get_snapshot(classes, &s) == MC_TILE_ENTITY_OK &&
          s.base_phase == MC_TILE_ENTITY_CLASS_READY &&
          s.piston_phase == MC_TILE_ENTITY_CLASS_READY && s.base_name_count == 8 &&
          s.base_class_count == 8 && s.live_tile_entities == 1 && s.list_count == 0 &&
          s.list_capacity == 0 && s.list_mod_count_bits == 0);
    CHECK(mc_tile_entity_piston_tick(tile_entity, NULL, &error) == MC_WORLD_STORAGE_OK &&
          tick_empty(&error) &&
          mc_tile_entity_get_snapshot(tile_entity, &state) == MC_TILE_ENTITY_OK &&
          state.world == NULL && state.piston_progress_bits == UINT32_C(0x3f000000) &&
          state.piston_previous_progress_bits == 0 && !state.invalid);
    CHECK(mc_world_runtime_classes_get(classes, &world_class) == MC_WORLD_STORAGE_OK &&
          !world_class.initialized && world_class.live_worlds == 0 &&
          world_class.enqueue_depth == 0 &&
          mc_registry_classes_inspect_blocks(f->registry[side], &blocks, NULL) == MC_REGISTRY_OK &&
          blocks.phase == MC_REGISTRY_COLD && f->sources[side].opens == 0 &&
          f->sources[side].closes == 0);
    CHECK(mc_world_runtime_classes_free(classes, NULL) == MC_WORLD_STORAGE_BUSY);
    mc_tile_entity_release(tile_entity);
    tile_entity = NULL;
    CHECK(mc_tile_entity_piston_class_get_snapshot(classes, &s) == MC_TILE_ENTITY_OK &&
          s.live_tile_entities == 0 && s.list_mod_count_bits == 0 && tick_unused_slots_null(&s));
    ok = true;
cleanup:
    mc_tile_entity_release(tile_entity);
    if (mc_world_runtime_classes_free(classes, NULL) != MC_WORLD_STORAGE_OK)
        ok = false;
    return ok;
}

/* Four actual empty serialized records. The reused seed_record intentionally
 * loads nonempty Chest/Spawner records for slots2/3, so it is not used here. */
static bool tick_load(fixture *f, size_t slot) {
    bool ok = false;
    fixture_record *r = calloc(1, sizeof(*r));
    uint8_t *encoded = NULL;
    size_t size = 0, written = 0;
    const char *names[] = {
        "xPos",       "zPos",      "LastUpdate",       "Blocks",   "Data",        "SkyLight",
        "BlockLight", "HeightMap", "TerrainPopulated", "Entities", "TileEntities"};
    const mc_nbt_type types[] = {MC_NBT_INT,        MC_NBT_INT,        MC_NBT_LONG,
                                 MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY,
                                 MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY, MC_NBT_BYTE,
                                 MC_NBT_LIST,       MC_NBT_LIST};
    const size_t lengths[] = {MC_CHUNK_BLOCKS, MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_NIBBLE_BYTES,
                              MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_HEIGHTMAP_BYTES};
    CHECK(r != NULL);
    r->root.type = r->level.type = MC_NBT_COMPOUND;
    r->level.name = ascii("Level", r->names[11]);
    for (size_t i = 0; i < COUNT(names); ++i) {
        r->fields[i].type = types[i];
        r->fields[i].name = ascii(names[i], r->names[i]);
        r->level_fields[i] = &r->fields[i];
    }
    for (size_t i = 0; i < COUNT(lengths); ++i) {
        r->fields[i + 3].value.byte_array.data = r->arrays[i];
        r->fields[i + 3].value.byte_array.length = lengths[i];
    }
    r->fields[2].value.long_value = -123;
    r->fields[8].value.byte_value = 1;
    r->fields[9].value.list.element_type = r->fields[10].value.list.element_type = MC_NBT_COMPOUND;
    r->level.value.compound.items = r->level_fields;
    r->level.value.compound.length = COUNT(r->level_fields);
    r->root_fields[0] = &r->level;
    r->root.value.compound.items = r->root_fields;
    r->root.value.compound.length = 1;
    CHECK(mc_nbt_measure(&r->root, NULL, &size, NULL) == MC_NBT_OK && size != 0);
    encoded = malloc(size);
    CHECK(encoded != NULL &&
          mc_nbt_write(&r->root, NULL, encoded, size, &written, NULL) == MC_NBT_OK &&
          written == size);
    uint32_t growth = 0;
    CHECK(mc_world_regions_write(f->regions, mc_world_store_root(f->stores[slot]), 0, 0, encoded,
                                 size, fixture_timestamp, &growth, NULL) == MC_WORLD_STORAGE_OK &&
          baseline(f, slot));
    mc_chunk_state *state = NULL;
    const mc_chunk_acquire_status loaded = mc_world_runtime_get(f->worlds[slot], 0, 0, &state);
    if (f->providers[slot].cache->insertion_count != 0)
        f->states[slot] = f->providers[slot].cache->insertions[0];
    CHECK(loaded == MC_CHUNK_ACQUIRE_OK && state != NULL && state == f->states[slot]);
    f->chunks[slot] = mc_world_store_chunk_object(f->stores[slot], state);
    CHECK(f->chunks[slot] != NULL && state->active && state->terrain_populated && !state->dirty &&
          f->chunks[slot]->l->count == 0 && mc_live_tile_entities_map_valid(f->chunks[slot]));
    ok = true;
cleanup:
    free(encoded);
    free(r);
    return ok;
}

static bool tick_new(fixture *f, size_t slot, int32_t id, int32_t facing, bool extending, bool bind,
                     mc_tile_entity **out) {
    bool ok = false;
    mc_tile_entity *tile_entity = NULL;
    CHECK(mc_tile_entity_allocate(f->worlds[slot], MC_TILE_ENTITY_PISTON, &tile_entity) ==
              MC_TILE_ENTITY_OK &&
          mc_tile_entity_piston_construct(tile_entity, id, 7, facing, extending, true, NULL) ==
              MC_TILE_ENTITY_OK);
    if (bind)
        CHECK(mc_tile_entity_bind_world(tile_entity, f->worlds[slot]) == MC_TILE_ENTITY_OK);
    CHECK(mc_tile_entity_set_coordinates(tile_entity, 8, 40, 6) == MC_TILE_ENTITY_OK);
    *out = tile_entity;
    tile_entity = NULL;
    ok = true;
cleanup:
    mc_tile_entity_release(tile_entity);
    return ok;
}

static bool tick_cell(fixture *f, size_t slot, int32_t id, int32_t data) {
    bool changed = false;
    return mc_world_runtime_block_metadata_set_quiet(f->worlds[slot], 8, 40, 6, id, data, &changed,
                                                     NULL) == MC_WORLD_STORAGE_OK;
}

static bool tick_entity_input(mc_entity *e, double x, double y, double z) {
    /* Explicit instance source-state inputs, not fabricated holds or phases. */
    e->base.offset = e->base.y_size = e->base.bp = e->base.bk = 0;
    e->base.aX = e->base.aY = e->base.aZ = e->base.ba = false;
    e->base.bb = e->base.bc = e->base.be = false;
    e->base.bd = true;
    e->base.bq = e->base.bx = e->base.bC = false;
    e->base.bu = 5;
    e->base.bv = 0;
    return mc_entity_set_size(e, .25f, .25f) == MC_ENTITY_OK &&
           mc_entity_set_position(e, x, y, z) == MC_ENTITY_OK;
}

static bool tick_entity_new(fixture *f, tick_entities *owned, size_t slot, bool falling, double x,
                            double y, double z, mc_entity **out) {
    bool ok = false, published = false;
    mc_entity *e = NULL;
    CHECK(owned->count < COUNT(owned->items));
    CHECK((falling
               ? mc_falling_entity_allocate(f->entity_classes[slot % 2], &e, NULL)
               : mc_item_entity_allocate(f->entity_classes[slot % 2], &e, NULL)) == MC_ENTITY_OK);
    owned->items[owned->count++] = e;
    CHECK((falling ? mc_falling_entity_construct(e, f->worlds[slot], x, y, z, 12, NULL)
                   : mc_item_entity_construct(e, f->worlds[slot], x, y, z, NULL, NULL)) ==
              MC_ENTITY_OK &&
          tick_entity_input(e, x, y, z));
    CHECK(mc_world_runtime_insert_entity(f->worlds[slot], e, &published, NULL) ==
              MC_WORLD_STORAGE_OK &&
          published && e->base.world == f->worlds[slot] && e->holds >= 3);
    *out = e;
    ok = true;
cleanup:
    return ok;
}

static bool tick_query(mc_world_runtime *world, const mc_entity_bounds *box,
                       mc_world_entity_list_view *view, size_t expected) {
    size_t count = SIZE_MAX;
    return mc_world_runtime_nearby_entities(world, NULL, box, view, NULL) == MC_WORLD_STORAGE_OK &&
           mc_world_entity_list_view_count(view, &count) == MC_WORLD_STORAGE_OK &&
           count == expected;
}

static bool tick_snapshot(fixture *f, size_t side, mc_tile_entity_piston_class_snapshot *out) {
    return mc_tile_entity_piston_class_get_snapshot(f->classes[side], out) == MC_TILE_ENTITY_OK;
}

static void tick_release_box(mc_entity_classes *code, mc_entity_classes *source,
                             mc_entity_bounds *box) {
    if (box != NULL && mc_entity_bounds_native_holds(code, box) != 0)
        (void)mc_entity_bounds_release(code, box);
    else if (box != NULL && mc_entity_bounds_native_holds(source, box) != 0)
        (void)mc_entity_bounds_release(source, box);
}

static bool tick_raw_and_null(fixture *f, size_t side) {
    bool ok = false;
    mc_tile_entity *tile_entity = NULL, *raw = NULL, *chest = NULL;
    mc_piston_tick_error e;
    mc_tile_entity_snapshot s;
    CHECK(mc_tile_entity_piston_tick(NULL, NULL, &e) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          e.tile_entity.tile_entity.status == MC_TILE_ENTITY_INVALID_ARGUMENT &&
          e.tick_bytecode == 0);
    CHECK(mc_tile_entity_allocate(f->worlds[side], MC_TILE_ENTITY_PISTON, &raw) ==
              MC_TILE_ENTITY_OK &&
          mc_tile_entity_piston_tick(raw, NULL, &e) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          e.tile_entity.tile_entity.status == MC_TILE_ENTITY_INVALID_PHASE &&
          raw->phase == MC_TILE_ENTITY_RAW);
    CHECK(mc_tile_entity_allocate(f->worlds[side], MC_TILE_ENTITY_CHEST, &chest) ==
              MC_TILE_ENTITY_OK &&
          mc_tile_entity_construct(chest) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_piston_tick(chest, NULL, &e) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          e.tile_entity.tile_entity.status == MC_TILE_ENTITY_INVALID_KIND);
    CHECK(tick_new(f, side, 0, 5, false, false, &tile_entity));

    static const struct {
        uint32_t current, next;
        bool completed;
    } rows[] = {{0, UINT32_C(0x3f000000), false},
                {UINT32_C(0x80000000), UINT32_C(0x3f000000), false},
                {UINT32_C(0x3f400000), UINT32_C(0x3f800000), false},
                {UINT32_C(0xbf800000), UINT32_C(0xbf000000), false},
                {UINT32_C(0xff800000), UINT32_C(0xff800000), false},
                {UINT32_C(0x7fc12345), UINT32_C(0x7fc12345), false},
                {UINT32_C(0xff812345), UINT32_C(0xffc12345), false},
                {UINT32_C(0x3f800000), UINT32_C(0x3f800000), true},
                {UINT32_C(0x3fc00000), UINT32_C(0x3fc00000), true},
                {UINT32_C(0x7f800000), UINT32_C(0x7f800000), true}};

    for (size_t i = 0; i < COUNT(rows); ++i) {
        const uint32_t k = tile_entity->state.piston.progress_bits,
                       l = tile_entity->state.piston.previous_progress_bits;
        tile_entity->state.piston.progress_bits = rows[i].current;
        tile_entity->state.piston.previous_progress_bits = UINT32_C(0x41200000);
        const mc_tile_entity_status made_invalid = mc_tile_entity_set_invalid(tile_entity, true);
        const mc_world_storage_status status = mc_tile_entity_piston_tick(tile_entity, NULL, &e);
        const mc_tile_entity_status observed = mc_tile_entity_get_snapshot(tile_entity, &s);
        tile_entity->state.piston.progress_bits = k;
        tile_entity->state.piston.previous_progress_bits = l;
        const mc_tile_entity_status valid = mc_tile_entity_set_invalid(tile_entity, false);
        CHECK(made_invalid == MC_TILE_ENTITY_OK && observed == MC_TILE_ENTITY_OK &&
              valid == MC_TILE_ENTITY_OK && s.invalid &&
              s.piston_previous_progress_bits == rows[i].current &&
              s.piston_progress_bits == rows[i].next && tile_entity->holds == 1);
        if (rows[i].completed)
            CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && e.tick_bytecode == 40 &&
                  e.push_bytecode == 0 && e.tile_entity.status == status);
        else
            CHECK(status == MC_WORLD_STORAGE_OK && tick_empty(&e));
    }
    const uint32_t k = tile_entity->state.piston.progress_bits,
                   l = tile_entity->state.piston.previous_progress_bits;
    const unsigned int csr = _mm_getcsr();
    tile_entity->state.piston.progress_bits = UINT32_C(0x7f812345);
    tile_entity->state.piston.previous_progress_bits = 0;
    _mm_setcsr(csr | 0x8000u);
    const mc_world_storage_status fp = mc_tile_entity_piston_tick(tile_entity, NULL, &e);
    const mc_tile_entity_status observed = mc_tile_entity_get_snapshot(tile_entity, &s);
    _mm_setcsr(csr);
    tile_entity->state.piston.progress_bits = k;
    tile_entity->state.piston.previous_progress_bits = l;
    CHECK(fp == MC_WORLD_STORAGE_UNSUPPORTED_STATE && e.tick_bytecode == 111 &&
          observed == MC_TILE_ENTITY_OK &&
          s.piston_previous_progress_bits == UINT32_C(0x7f812345) &&
          s.piston_progress_bits == UINT32_C(0x7f812345));
    const size_t tile_entity_holds = tile_entity->holds;
    tile_entity->holds = SIZE_MAX; /* Native retain ceiling precedes every source store. */
    const mc_world_storage_status retain = mc_tile_entity_piston_tick(tile_entity, NULL, &e);
    tile_entity->holds = tile_entity_holds;
    CHECK(retain == MC_WORLD_STORAGE_LIMIT && e.tick_bytecode == 0 && e.push_bytecode == 0 &&
          e.tile_entity.tile_entity.status == MC_TILE_ENTITY_LIMIT &&
          tile_entity->state.piston.progress_bits == k &&
          tile_entity->state.piston.previous_progress_bits == l);
    mc_tile_entity_release(tile_entity);
    tile_entity = NULL;
    static const int32_t null_ids[] = {0, 36, 37, 50};
    for (size_t i = 0; i < COUNT(null_ids); ++i) {
        CHECK(tick_new(f, side, null_ids[i], INT32_MAX, true, false, &tile_entity));
        mc_entity_classes *const binding = f->classes[side]->entities;
        f->classes[side]->entities = NULL; /* Native unreachable-factory binding fault. */
        const mc_world_storage_status status = mc_tile_entity_piston_tick(tile_entity, NULL, &e);
        f->classes[side]->entities = binding;
        CHECK(status == MC_WORLD_STORAGE_OK && tick_empty(&e) && tile_entity->world == NULL &&
              tile_entity->state.piston.progress_bits == UINT32_C(0x3f000000) &&
              !tile_entity->invalid);
        mc_tile_entity_release(tile_entity);
        tile_entity = NULL;
    }
    CHECK(tick_new(f, side, 1, 5, true, false, &tile_entity));
    const int32_t cursor = f->entity_classes[side]->bounds_cursor;
    const size_t holds = f->entity_classes[side]->bounds_holds;
    CHECK(mc_tile_entity_piston_tick(tile_entity, NULL, &e) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          e.tick_bytecode == 153 && e.push_bytecode == 60 &&
          tile_entity->state.piston.progress_bits == UINT32_C(0x3f000000) &&
          tile_entity->state.piston.previous_progress_bits == 0 && !tile_entity->invalid &&
          f->entity_classes[side]->bounds_cursor == cursor + 1 &&
          f->entity_classes[side]->bounds_holds == holds);
    mc_tile_entity_release(tile_entity);
    tile_entity = NULL;
    CHECK(tick_new(f, side, 65, 5, true, false, &tile_entity));
    const int32_t before = f->entity_classes[side]->bounds_cursor;
    CHECK(mc_tile_entity_piston_tick(tile_entity, NULL, &e) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          e.tick_bytecode == 153 && e.push_bytecode == 46 && e.collision.block_bytecode == 5 &&
          e.collision.nearby.query.status == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          f->entity_classes[side]->bounds_cursor == before && !tile_entity->invalid);
    mc_tile_entity_release(tile_entity);
    tile_entity = NULL;
    CHECK(tick_new(f, side, 1, 5, false, false, &tile_entity));
    const uint32_t original_k = tile_entity->state.piston.progress_bits,
                   original_l = tile_entity->state.piston.previous_progress_bits;
    tile_entity->state.piston.progress_bits = UINT32_C(0x40000000); /* Deliberate source input. */
    const mc_world_storage_status completed = mc_tile_entity_piston_tick(tile_entity, NULL, &e);
    const mc_tile_entity_status completed_seen = mc_tile_entity_get_snapshot(tile_entity, &s);
    tile_entity->state.piston.progress_bits = original_k;
    tile_entity->state.piston.previous_progress_bits = original_l;
    CHECK(completed == MC_WORLD_STORAGE_INVALID_ARGUMENT && completed_seen == MC_TILE_ENTITY_OK &&
          e.tick_bytecode == 21 && e.push_bytecode == 60 && !tile_entity->invalid &&
          s.piston_progress_bits == UINT32_C(0x40000000) &&
          s.piston_previous_progress_bits == UINT32_C(0x40000000));
    ok = true;
cleanup:
    mc_tile_entity_release(tile_entity);
    mc_tile_entity_release(raw);
    mc_tile_entity_release(chest);
    return ok;
}

static bool tick_code_context(fixture *f, size_t side) {
    bool ok = false;
    const size_t source_side = 1u - side;
    mc_world_runtime *const world = f->worlds[source_side];
    mc_entity_classes *const pool = f->entity_classes[side];
    mc_entity_classes *const source_pool = f->entity_classes[source_side];
    const mc_block *const ladder = f->registry[side]->blocks.m[65];
    const mc_block *const moving = f->registry[side]->blocks.m[36];
    mc_entity_bounds marker = {0}, *out = &marker;
    mc_world_collision_error e = mc_world_collision_empty_error();
    CHECK(tick_cell(f, side, 1, 2) && tick_cell(f, source_side, 1, 5));
    const int32_t code_cursor = pool->bounds_cursor, source_cursor = source_pool->bounds_cursor;
    const size_t code_holds = pool->bounds_holds, source_holds = source_pool->bounds_holds;
    /* The client/server defining classes and source World are deliberately
     * different real namespaces. Source metadata5 selects the thin X slab;
     * code World metadata2 would instead select the thin Z slab. */
    const mc_world_storage_status direct = mc_world_collision_block_box_in_classes_held(
        f->classes[side], world, ladder, 8, 40, 6, &out, &e);
    CHECK(direct == MC_WORLD_STORAGE_OK && out != &marker && out != NULL &&
          mc_entity_bounds_native_holds(pool, out) == 1 &&
          mc_entity_bounds_native_holds(source_pool, out) == 0 && out->min_x == 8 &&
          out->max_x == 8.125 && out->min_y == 40 && out->max_y == 41 && out->min_z == 6 &&
          out->max_z == 7 && pool->bounds_cursor == code_cursor + 1 &&
          pool->bounds_holds == code_holds + 1u && source_pool->bounds_cursor == source_cursor &&
          source_pool->bounds_holds == source_holds && world->active_calls == 0);
    const mc_entity_status released = mc_entity_bounds_release(pool, out);
    out = &marker;
    CHECK(released == MC_ENTITY_OK && pool->bounds_holds == code_holds);
    e = mc_world_collision_empty_error();
    CHECK(mc_world_collision_piston_shifted_box_in_classes_held(f->classes[side], world, moving, 8,
                                                                40, 6, 65, .25f, 5, &out,
                                                                &e) == MC_WORLD_STORAGE_OK &&
          out != &marker && out != NULL && mc_entity_bounds_native_holds(pool, out) == 1 &&
          mc_entity_bounds_native_holds(source_pool, out) == 0 && out->min_x == 7.75 &&
          out->max_x == 7.875 && out->min_y == 40 && out->max_y == 41 && out->min_z == 6 &&
          out->max_z == 7 && source_pool->bounds_cursor == source_cursor &&
          world->active_calls == 0 && world->local_leases == 0);
    const mc_entity_status shifted_release = mc_entity_bounds_release(pool, out);
    out = &marker;
    CHECK(shifted_release == MC_ENTITY_OK);
    const int32_t before_failure = pool->bounds_cursor;
    const size_t source_cache = f->providers[source_side].cache->insertion_count;
    const size_t alternate_cache = f->providers[source_side + 2u].cache->insertion_count;
    mc_chunk_provider *const provider = world->fields.v;
    world->fields.v = f->worlds[source_side + 2u]->fields.v; /* Actual alternate provider owner. */
    e = mc_world_collision_empty_error();
    const mc_world_storage_status foreign = mc_world_collision_piston_shifted_box_in_classes_held(
        f->classes[side], world, moving, 8, 40, 6, 65, .25f, 5, &out, &e);
    world->fields.v = provider;
    CHECK(foreign == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out == &marker &&
          e.block_bytecode == 5 && e.nearby.query.bytecode == 48 &&
          e.nearby.query.provider_bytecode == 6 && e.nearby.query.cause.status == foreign &&
          strcmp(e.nearby.query.operation, "Collision replacement provider binding") == 0 &&
          pool->bounds_cursor == before_failure && pool->bounds_holds == code_holds &&
          source_pool->bounds_holds == source_holds && world->active_calls == 0 &&
          world->local_leases == 0 &&
          f->providers[source_side].cache->insertion_count == source_cache &&
          f->providers[source_side + 2u].cache->insertion_count == alternate_cache);
    /* Native entry-overflow fault: actual source-unused classes still finish;
     * the metadata carried Block reaches World admission at its own getter PC5. */
    const size_t calls = world->active_calls;
    world->active_calls = SIZE_MAX;
    e = mc_world_collision_empty_error();
    const mc_world_storage_status null_box = mc_world_collision_piston_shifted_box_in_classes_held(
        f->classes[side], world, moving, 8, 40, 6, 50, .25f, INT32_MAX, &out, &e);
    const bool completed_null = out == NULL;
    mc_entity_bounds *const null_result = out;
    out = &marker;
    e = mc_world_collision_empty_error();
    const mc_world_storage_status base = mc_world_collision_piston_shifted_box_in_classes_held(
        f->classes[side], world, moving, 8, 40, 6, 1, .25f, 5, &out, &e);
    world->active_calls = calls;
    const bool completed_base = out != &marker && out != NULL &&
                                mc_entity_bounds_native_holds(pool, out) == 1 &&
                                mc_entity_bounds_native_holds(source_pool, out) == 0 &&
                                out->min_x == 7.75 && out->max_x == 8.75;
    if (null_result != &marker)
        tick_release_box(pool, source_pool, null_result);
    if (out != &marker)
        tick_release_box(pool, source_pool, out);
    out = &marker;
    CHECK(null_box == MC_WORLD_STORAGE_OK && completed_null && base == MC_WORLD_STORAGE_OK &&
          completed_base && world->active_calls == calls && pool->bounds_holds == code_holds);
    world->active_calls = SIZE_MAX;
    e = mc_world_collision_empty_error();
    const mc_world_storage_status overflow = mc_world_collision_piston_shifted_box_in_classes_held(
        f->classes[side], world, moving, 8, 40, 6, 65, .25f, 5, &out, &e);
    world->active_calls = calls;
    CHECK(overflow == MC_WORLD_STORAGE_LIMIT && out == &marker && e.block_bytecode == 5 &&
          e.nearby.query.cause.status == overflow && e.nearby.query.provider_bytecode == 0 &&
          strcmp(e.nearby.query.operation, "World operation admission") == 0 &&
          world->active_calls == calls && pool->bounds_holds == code_holds &&
          source_pool->bounds_holds == source_holds);
    CHECK(tick_cell(f, side, 0, 0) && tick_cell(f, source_side, 0, 0));
    ok = true;
cleanup:
    if (out != &marker)
        tick_release_box(pool, source_pool, out);
    return ok;
}

static bool tick_installed(fixture *f, size_t side) {
    bool ok = false;
    mc_tile_entity *tile_entity = NULL;
    mc_piston_tick_error e;
    CHECK(tick_new(f, side, 1, 5, true, false, &tile_entity) &&
          finalize_install(f, side, tile_entity, 8, 2));
    CHECK(mc_tile_entity_piston_tick(tile_entity, NULL, &e) == MC_WORLD_STORAGE_OK &&
          tick_empty(&e) && tile_entity->state.piston.progress_bits == UINT32_C(0x3f000000) &&
          tile_entity->state.piston.previous_progress_bits == 0 && !tile_entity->invalid &&
          tile_entity->holds == 3 && finalize_cell_is(f, side, 8, 36, 2));
    CHECK(mc_tile_entity_piston_tick(tile_entity, NULL, &e) == MC_WORLD_STORAGE_OK &&
          tick_empty(&e) && tile_entity->state.piston.progress_bits == UINT32_C(0x3f800000) &&
          tile_entity->state.piston.previous_progress_bits == UINT32_C(0x3f000000) &&
          !tile_entity->invalid && tile_entity->holds == 3);
    CHECK(mc_tile_entity_piston_tick(tile_entity, NULL, &e) == MC_WORLD_STORAGE_OK &&
          tick_empty(&e) && tile_entity->invalid && tile_entity->holds == 1 &&
          tile_entity->state.piston.progress_bits == UINT32_C(0x3f800000) &&
          tile_entity->state.piston.previous_progress_bits == UINT32_C(0x3f800000) &&
          finalize_cell_is(f, side, 8, 1, 7) && f->chunks[side]->l->count == 0);
    mc_tile_entity_release(tile_entity);
    tile_entity = NULL;
    static const uint32_t completed_inputs[] = {UINT32_C(0x3fc00000), UINT32_C(0x7f800000)};
    for (size_t i = 0; i < COUNT(completed_inputs); ++i) {
        CHECK(tick_new(f, side, 1, 5, false, false, &tile_entity) &&
              finalize_install(f, side, tile_entity, 8, 2));
        const uint32_t k = tile_entity->state.piston.progress_bits,
                       l = tile_entity->state.piston.previous_progress_bits;
        tile_entity->state.piston.progress_bits = completed_inputs[i];
        const mc_world_storage_status status = mc_tile_entity_piston_tick(tile_entity, NULL, &e);
        mc_tile_entity_snapshot s;
        const mc_tile_entity_status seen = mc_tile_entity_get_snapshot(tile_entity, &s);
        tile_entity->state.piston.progress_bits = k;
        tile_entity->state.piston.previous_progress_bits = l;
        CHECK(status == MC_WORLD_STORAGE_OK && tick_empty(&e) && seen == MC_TILE_ENTITY_OK &&
              s.invalid && s.piston_progress_bits == completed_inputs[i] &&
              s.piston_previous_progress_bits == completed_inputs[i] &&
              finalize_cell_is(f, side, 8, 1, 7));
        mc_tile_entity_release(tile_entity);
        tile_entity = NULL;
    }
    CHECK(tick_new(f, side, 1, 5, false, false, &tile_entity) &&
          finalize_install(f, side, tile_entity, 8, 2));
    const uint32_t original_k = tile_entity->state.piston.progress_bits,
                   original_l = tile_entity->state.piston.previous_progress_bits;
    tile_entity->state.piston.progress_bits = UINT32_C(0x40000000);
    mc_world_reference_list *const world_accesses = f->worlds[side]->fields.u;
    f->worlds[side]->fields.u = NULL; /* Reached native WorldAccess receiver fault. */
    const mc_world_storage_status late = mc_tile_entity_piston_tick(tile_entity, NULL, &e);
    f->worlds[side]->fields.u = world_accesses;
    mc_tile_entity_snapshot completed;
    const mc_tile_entity_status completed_seen =
        mc_tile_entity_get_snapshot(tile_entity, &completed);
    tile_entity->state.piston.progress_bits = original_k;
    tile_entity->state.piston.previous_progress_bits = original_l;
    CHECK(late == MC_WORLD_STORAGE_UNSUPPORTED_STATE && completed_seen == MC_TILE_ENTITY_OK &&
          e.tick_bytecode == 99 && e.mutation.status == late && tile_entity->invalid &&
          tile_entity->holds == 1 && completed.piston_progress_bits == UINT32_C(0x40000000) &&
          completed.piston_previous_progress_bits == UINT32_C(0x40000000) &&
          finalize_cell_is(f, side, 8, 1, 7) && f->chunks[side]->l->count == 0 &&
          f->worlds[side]->active_calls == 0);
    CHECK(tick_cell(f, side, 0, 0));
    ok = true;
cleanup:
    mc_tile_entity_release(tile_entity);
    return ok;
}

static bool tick_axes(fixture *f, tick_entities *owned, size_t side) {
    bool ok = false;
    mc_tile_entity *tile_entity = NULL;
    mc_entity *item = NULL, *falling = NULL;
    mc_piston_tick_error e;
    const int32_t directions[6][3] = {{0, -1, 0}, {0, 1, 0},  {0, 0, -1},
                                      {0, 0, 1},  {-1, 0, 0}, {1, 0, 0}};
    CHECK(tick_entity_new(f, owned, side, false, 8, 40.25, 6.5, &item) &&
          tick_entity_new(f, owned, side, true, 8, 40.25, 6.5, &falling));
    mc_entity_bounds *const a = item->bounds, *const b = falling->bounds;
    for (size_t facing = 0; facing < 6; ++facing) {
        const double x = 8.5 - directions[facing][0] * .5;
        const double y = 40.25 - directions[facing][1] * .5;
        const double z = 6.5 - directions[facing][2] * .5;
        CHECK(tick_entity_input(item, x, y, z) && tick_entity_input(falling, x, y, z) &&
              tick_new(f, side, 1, (int32_t)facing, true, true, &tile_entity));
        const bool clip = item->base.bq;
        item->base.bq = true; /* Actual no-clip Item move; Falling uses normal World movement. */
        const mc_world_storage_status status = mc_tile_entity_piston_tick(tile_entity, NULL, &e);
        item->base.bq = clip;
        CHECK(status == MC_WORLD_STORAGE_OK && tick_empty(&e) && item->bounds == a &&
              falling->bounds == b && item->base.x == x + directions[facing][0] * .5625 &&
              item->base.y == y + directions[facing][1] * .5625 &&
              item->base.z == z + directions[facing][2] * .5625 &&
              falling->base.x == item->base.x && falling->base.y == item->base.y &&
              falling->base.z == item->base.z);
        mc_tile_entity_piston_class_snapshot s;
        CHECK(tick_snapshot(f, side, &s) && s.list_count == 0 && s.list_capacity == 10 &&
              f->worlds[side]->active_calls == 0 && f->worlds[side]->local_leases == 0);
        mc_tile_entity_release(tile_entity);
        tile_entity = NULL;
    }
    CHECK(tick_entity_input(item, 8, 40.25, 6.5) && tick_entity_input(falling, 12, 80, 12));
    bool published = false;
    CHECK(mc_world_runtime_insert_entity(f->worlds[side], item, &published, NULL) ==
              MC_WORLD_STORAGE_OK &&
          published && tick_new(f, side, 1, 5, true, true, &tile_entity));
    const bool clip = item->base.bq;
    item->base.bq = true;
    const mc_world_storage_status duplicate = mc_tile_entity_piston_tick(tile_entity, NULL, &e);
    item->base.bq = clip;
    CHECK(duplicate == MC_WORLD_STORAGE_OK && tick_empty(&e) && item->base.x == 9.125 &&
          item->bounds == a);
    mc_tile_entity_release(tile_entity);
    tile_entity = NULL;
    CHECK(tick_entity_input(item, 12, 80, 12) && tick_entity_input(falling, 8.5, 40.25, 6.5) &&
          tick_new(f, side, 1, 5, false, true, &tile_entity));
    const uint32_t original_k = tile_entity->state.piston.progress_bits,
                   original_l = tile_entity->state.piston.previous_progress_bits;
    tile_entity->state.piston.progress_bits =
        UINT32_C(0x3fa00000); /* Completed retract source input. */
    const mc_world_storage_status retract = mc_tile_entity_piston_tick(tile_entity, NULL, &e);
    mc_tile_entity_snapshot completed;
    const mc_tile_entity_status completed_seen =
        mc_tile_entity_get_snapshot(tile_entity, &completed);
    tile_entity->state.piston.progress_bits = original_k;
    tile_entity->state.piston.previous_progress_bits = original_l;
    CHECK(retract == MC_WORLD_STORAGE_OK && completed_seen == MC_TILE_ENTITY_OK && tick_empty(&e) &&
          tile_entity->invalid && completed.piston_progress_bits == UINT32_C(0x3fa00000) &&
          completed.piston_previous_progress_bits == UINT32_C(0x3fa00000) &&
          falling->base.x == 8.75);
    CHECK(tick_entity_input(falling, 12, 80, 12));
    ok = true;
cleanup:
    mc_tile_entity_release(tile_entity);
    return ok;
}

static bool tick_leftovers(fixture *f, tick_entities *owned, size_t side) {
    bool ok = false;
    mc_tile_entity *first = NULL, *null_box = NULL, *other = NULL;
    mc_entity *old = NULL, *added = NULL;
    mc_piston_tick_error e;
    mc_tile_entity_piston_class_snapshot s;
    const mc_entity_move_limits zero = {0};
    CHECK(tick_entity_new(f, owned, side, false, 8, 40.25, 6.5, &old) &&
          tick_new(f, side, 1, 5, true, true, &first));
    CHECK(mc_tile_entity_piston_tick(first, &zero, &e) == MC_WORLD_STORAGE_LIMIT &&
          e.tick_bytecode == 153 && e.push_bytecode == 156 && e.entity_index == 0 &&
          e.movement.status == MC_ENTITY_LIMIT && e.movement.bytecode == 1841 &&
          e.movement.loop_work == 0 && e.movement.max_loop_work == 0 && old->base.x == 8.5625 &&
          tick_snapshot(f, side, &s) && s.list_count == 1 && s.list_identity->items[0] == old);
    CHECK(tick_new(f, side, 0, INT32_MAX, true, false, &null_box) &&
          mc_tile_entity_piston_tick(null_box, NULL, &e) == MC_WORLD_STORAGE_OK && tick_empty(&e) &&
          tick_snapshot(f, side, &s) && s.list_count == 1 && s.list_identity->items[0] == old);
    CHECK(tick_new(f, side + 2u, 1, 5, true, true, &other) &&
          mc_tile_entity_piston_tick(other, NULL, &e) == MC_WORLD_STORAGE_OK && tick_empty(&e) &&
          tick_snapshot(f, side, &s) && s.list_count == 1 && s.list_identity->items[0] == old);
    CHECK(tick_entity_new(f, owned, side + 2u, true, 8.5, 40.25, 6.5, &added));
    const bool old_clip = old->base.bq;
    old->base.bq = true;
    const mc_world_storage_status second = mc_tile_entity_piston_tick(other, &zero, &e);
    old->base.bq = old_clip;
    CHECK(second == MC_WORLD_STORAGE_LIMIT && e.tick_bytecode == 153 && e.push_bytecode == 156 &&
          e.entity_index == 1 && e.movement.bytecode == 1841 && old->base.x == 9.125 &&
          added->base.x == 9.0625 && tick_snapshot(f, side, &s) && s.list_count == 2 &&
          s.list_identity->items[0] == old && s.list_identity->items[1] == added);
    const bool added_clip = added->base.bq;
    old->base.bq = added->base.bq = true;
    const mc_world_storage_status recovered = mc_tile_entity_piston_tick(other, NULL, &e);
    old->base.bq = old_clip;
    added->base.bq = added_clip;
    CHECK(recovered == MC_WORLD_STORAGE_OK && tick_empty(&e) && old->base.x == 9.375 &&
          added->base.x == 9.5625 && other->invalid && tick_snapshot(f, side, &s) &&
          s.list_count == 0 && old->base.world == f->worlds[side] &&
          added->base.world == f->worlds[side + 2u] && f->worlds[side]->active_calls == 0 &&
          f->worlds[side + 2u]->active_calls == 0);
    CHECK(tick_entity_input(old, 12, 80, 12) && tick_entity_input(added, 12, 80, 12));
    ok = true;
cleanup:
    if (!ok && first != NULL)
        (void)mc_tile_entity_piston_entities_clear(first, NULL);
    mc_tile_entity_release(first);
    mc_tile_entity_release(null_box);
    mc_tile_entity_release(other);
    return ok;
}

static bool tick_list_contracts(fixture *f, tick_entities *owned, size_t side) {
    bool ok = false;
    mc_tile_entity *tile_entity = NULL;
    mc_entity *entity = NULL, *selected = NULL;
    mc_world_entity_list_view view = {0}, empty = {0};
    mc_piston_list_error e;
    mc_piston_entity_iterator iterator, later;
    mc_tile_entity_piston_class_snapshot s, before, after;
    const mc_entity_bounds query = {7.5, 40, 6, 8.5, 41, 7}, missing = {12, 40, 12, 13, 41, 13};
    CHECK(tick_entity_new(f, owned, side, false, 8, 40.25, 6.5, &entity) &&
          tick_new(f, side, 0, 5, false, false, &tile_entity) &&
          tick_query(f->worlds[side], &query, &view, 1));
    const size_t baseline_holds = entity->holds;
    CHECK(tick_snapshot(f, side, &before));
    const uint32_t initial_mod = before.list_mod_count_bits;
    for (size_t i = 0; i < 10; ++i)
        CHECK(mc_tile_entity_piston_entities_append(tile_entity, &view, &e) ==
                  MC_WORLD_STORAGE_OK &&
              tick_snapshot(f, side, &s) && s.list_count == i + 1u && s.list_capacity == 10 &&
              s.list_mod_count_bits == initial_mod + (uint32_t)i + UINT32_C(1) &&
              tick_unused_slots_null(&s) && entity->holds == baseline_holds + i + 1u);
    CHECK(s.allocation_bytes ==
              sizeof(mc_world_reference_list) + 10u * (sizeof(void *) + sizeof(mc_entity *)) &&
          mc_tile_entity_piston_entities_iterator(tile_entity, &iterator, &e) ==
              MC_WORLD_STORAGE_OK);
    const uint32_t mod = iterator.expected_mod_count;
    const mc_world_storage_status growth =
        mc_tile_entity_piston_entities_append(tile_entity, &view, &e);
    CHECK(mc_tile_entity_piston_entities_iterator(tile_entity, &later, NULL) ==
              MC_WORLD_STORAGE_OK &&
          later.expected_mod_count == mod + UINT32_C(1) && tick_snapshot(f, side, &after) &&
          after.list_mod_count_bits == later.expected_mod_count && tick_unused_slots_null(&after));
    if (side == 1)
        CHECK(growth == MC_WORLD_STORAGE_LIMIT &&
              strcmp(e.operation, "Piston addAll growth peak budget") == 0 &&
              after.list_count == 10 && after.list_capacity == 10 &&
              after.allocation_bytes == s.allocation_bytes &&
              entity->holds == baseline_holds + 10u);
    else {
        CHECK(growth == MC_WORLD_STORAGE_OK && after.list_count == 11 && after.list_capacity == 15);
        for (size_t i = 11; i < 16; ++i)
            CHECK(mc_tile_entity_piston_entities_append(tile_entity, &view, &e) ==
                  MC_WORLD_STORAGE_OK);
        CHECK(tick_snapshot(f, side, &s) && s.list_count == 16 && s.list_capacity == 22 &&
              tick_unused_slots_null(&s));
    }
    bool has = false;
    CHECK(mc_tile_entity_piston_entities_has_next(&iterator, &has, &e) == MC_WORLD_STORAGE_OK &&
          has &&
          mc_tile_entity_piston_entities_next(&iterator, &selected, &e) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          selected == NULL && iterator.cursor == 0 &&
          strcmp(e.operation, "Piston List iterator concurrent modification") == 0);
    CHECK(tick_snapshot(f, side, &before));
    CHECK(mc_tile_entity_piston_entities_clear(tile_entity, &e) == MC_WORLD_STORAGE_OK &&
          tick_snapshot(f, side, &after) && after.list_count == 0 &&
          after.list_mod_count_bits == before.list_mod_count_bits + UINT32_C(1) &&
          tick_unused_slots_null(&after));
    CHECK(entity->holds == baseline_holds &&
          mc_tile_entity_piston_entities_append(tile_entity, &view, &e) == MC_WORLD_STORAGE_OK &&
          tick_snapshot(f, side, &before) &&
          mc_tile_entity_piston_entities_iterator(tile_entity, &iterator, &e) ==
              MC_WORLD_STORAGE_OK);
    const size_t real_holds = entity->holds;
    entity->holds = SIZE_MAX; /* Explicit native retain-overflow fault, not gameplay. */
    const mc_world_storage_status snapshot_failure =
        mc_tile_entity_piston_entities_append(tile_entity, &view, &e);
    entity->holds = real_holds;
    CHECK(snapshot_failure == MC_WORLD_STORAGE_LIMIT && e.entity_status == MC_ENTITY_LIMIT &&
          e.index == 0 && strcmp(e.operation, "Piston addAll source snapshot Entity hold") == 0 &&
          tick_snapshot(f, side, &after) && after.list_count == before.list_count &&
          after.list_mod_count_bits == before.list_mod_count_bits &&
          after.allocation_bytes == before.allocation_bytes &&
          mc_tile_entity_piston_entities_iterator(tile_entity, &later, NULL) ==
              MC_WORLD_STORAGE_OK &&
          later.expected_mod_count == iterator.expected_mod_count);
    CHECK(mc_tile_entity_piston_entities_next(&iterator, &selected, &e) == MC_WORLD_STORAGE_OK &&
          selected == entity && iterator.cursor == 1);
    mc_entity_release(selected);
    selected = NULL;
    CHECK(mc_tile_entity_piston_entities_iterator(tile_entity, &iterator, &e) ==
          MC_WORLD_STORAGE_OK);
    mc_world_reference_list *const list = (mc_world_reference_list *)after.list_identity;
    void *const actual = list->items[0];
    list->items[0] = NULL; /* Native public-projection corruption; ledger is untouched. */
    const mc_world_storage_status forged =
        mc_tile_entity_piston_entities_next(&iterator, &selected, &e);
    list->items[0] = actual;
    CHECK(forged == MC_WORLD_STORAGE_UNSUPPORTED_STATE && selected == NULL &&
          iterator.cursor == 1 && e.index == 0 &&
          strcmp(e.operation, "Piston List iterator selected occurrence") == 0 &&
          entity->holds == real_holds);
    CHECK(mc_tile_entity_piston_entities_iterator(tile_entity, &iterator, &e) ==
          MC_WORLD_STORAGE_OK);
    mc_piston_entity_iterator foreign = iterator;
    foreign.identity = f->worlds[side]->fields.c;
    has = true;
    CHECK(mc_tile_entity_piston_entities_has_next(&foreign, &has, &e) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          has && foreign.cursor == 0);
    CHECK(tick_query(f->worlds[side], &missing, &empty, 0));
    const size_t no_view_holds = entity->holds;
    CHECK(mc_tile_entity_piston_entities_append(tile_entity, &empty, &e) == MC_WORLD_STORAGE_OK &&
          entity->holds == no_view_holds && tick_snapshot(f, side, &s) &&
          s.list_mod_count_bits == iterator.expected_mod_count + UINT32_C(1) &&
          tick_unused_slots_null(&s) &&
          mc_tile_entity_piston_entities_has_next(&iterator, &has, &e) == MC_WORLD_STORAGE_OK &&
          has &&
          mc_tile_entity_piston_entities_next(&iterator, &selected, &e) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          selected == NULL && iterator.cursor == 0);
    CHECK(mc_tile_entity_piston_class_discard_entities(f->classes[side], NULL) ==
              MC_WORLD_STORAGE_BUSY &&
          tick_snapshot(f, side, &s) && s.list_count == 1);
    const uint32_t before_clear = s.list_mod_count_bits;
    CHECK(mc_tile_entity_piston_entities_clear(tile_entity, &e) == MC_WORLD_STORAGE_OK &&
          tick_snapshot(f, side, &s) && s.list_count == 0 &&
          s.list_mod_count_bits == before_clear + UINT32_C(1) && tick_unused_slots_null(&s) &&
          s.list_capacity == (side == 0 ? 22u : 10u));
    CHECK(tick_entity_input(entity, 12, 80, 12));
    ok = true;
cleanup:
    mc_entity_release(selected);
    mc_world_entity_list_view_release(&view);
    mc_world_entity_list_view_release(&empty);
    if (tile_entity != NULL)
        (void)mc_tile_entity_piston_entities_clear(tile_entity, NULL);
    mc_tile_entity_release(tile_entity);
    return ok;
}

static void tick_release_side(tick_entities *owned, mc_entity_classes *classes) {
    for (size_t i = 0; i < owned->count; ++i)
        if (owned->items[i] != NULL && owned->items[i]->classes == classes) {
            mc_entity_release(owned->items[i]);
            owned->items[i] = NULL;
        }
}

static bool tick_terminal_cycle(fixture *f, tick_entities *owned, size_t side) {
    bool ok = false;
    mc_tile_entity *tile_entity = NULL;
    mc_entity *entity = NULL;
    mc_piston_tick_error e;
    mc_tile_entity_piston_class_snapshot s;
    const mc_entity_move_limits zero = {0};
    CHECK(tick_entity_new(f, owned, side, true, 8, 40.25, 6.5, &entity) &&
          tick_new(f, side, 1, 5, true, true, &tile_entity));
    CHECK(mc_tile_entity_piston_tick(tile_entity, &zero, &e) == MC_WORLD_STORAGE_LIMIT &&
          e.push_bytecode == 156 && e.movement.loop_work == 0 && tick_snapshot(f, side, &s) &&
          s.list_count == 1);
    mc_tile_entity_release(tile_entity);
    tile_entity = NULL;
    for (size_t slot = side; slot < WORLD_COUNT; slot += 2u) {
        const mc_world_storage_status status =
            mc_world_runtime_retire_entity_containers(f->worlds[slot], NULL);
        if (status == MC_WORLD_STORAGE_OK)
            f->terminal[slot] = true;
        CHECK(status == MC_WORLD_STORAGE_OK);
    }
    tick_release_side(owned, f->entity_classes[side]);
    CHECK(entity->holds == 1 && entity->base.world == f->worlds[side] &&
          tick_snapshot(f, side, &s) && s.live_tile_entities == 0 && s.list_count == 1 &&
          s.list_identity->items[0] == entity && f->worlds[side]->persistent_bindings != 0);
    CHECK(retire_descriptors(f, side));
    const mc_world_storage_status retired = mc_world_runtime_retire_world(f->worlds[side], NULL);
    if (retired == MC_WORLD_STORAGE_OK)
        f->retired[side] = true;
    CHECK(retired == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_free(f->worlds[side], NULL) == MC_WORLD_STORAGE_BUSY);
    CHECK(mc_tile_entity_piston_class_discard_entities(f->classes[side], NULL) ==
              MC_WORLD_STORAGE_OK &&
          tick_snapshot(f, side, &s) && s.list_count == 0 && s.live_tile_entities == 0 &&
          f->worlds[side]->persistent_bindings == 0);
    entity = NULL; /* Its sole real m hold was released. Never dereference it again. */
    ok = true;
cleanup:
    mc_tile_entity_release(tile_entity);
    return ok;
}

static bool tick_cleanup(fixture *f, tick_entities *owned) {
    if (f == NULL)
        return true;
    for (size_t slot = 0; slot < WORLD_COUNT; ++slot)
        if (f->worlds[slot] != NULL && f->bound[slot] && !f->terminal[slot]) {
            if (mc_world_runtime_retire_entity_containers(f->worlds[slot], NULL) !=
                MC_WORLD_STORAGE_OK)
                return false;
            f->terminal[slot] = true;
        }
    for (size_t side = 0; side < 2; ++side) {
        if (f->classes[side] != NULL && mc_tile_entity_piston_class_discard_entities(
                                            f->classes[side], NULL) != MC_WORLD_STORAGE_OK)
            return false;
        tick_release_side(owned, f->entity_classes[side]);
    }
    return cleanup_fixture(f);
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    tick_entities owned = {0};
    char root[8192];
    const uint16_t title[] = {'T', 'i', 'c', 'k'};
    if (argc != 3 || strcmp(argv[1], "world-piston-tick") != 0 ||
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
                  MC_ENTITY_OK);
        CHECK(tick_pre_world(f, side));
        CHECK(mc_registry_classes_initialize_blocks(f->registry[side], NULL) == MC_REGISTRY_OK);
        CHECK(r->opens == 3 && r->closes == 1 && !r->bad);
    }
    for (size_t slot = 0; slot < WORLD_COUNT; ++slot)
        CHECK(open_store(f, slot, root) && create_world(f, slot) && tick_load(f, slot));
    CHECK(mc_tile_entity_piston_class_configure(f->worlds[1], sizeof(mc_world_reference_list) +
                                                                  40u * sizeof(void *)) ==
          MC_TILE_ENTITY_OK);
    for (size_t side = 0; side < 2; ++side)
        CHECK(tick_raw_and_null(f, side) && tick_code_context(f, side) && tick_installed(f, side) &&
              tick_axes(f, &owned, side) && tick_leftovers(f, &owned, side) &&
              tick_list_contracts(f, &owned, side));
    for (size_t slot = 0; slot < WORLD_COUNT; ++slot)
        CHECK(finalize_disk_unchanged(f, slot));
    for (size_t side = 0; side < 2; ++side)
        CHECK(tick_terminal_cycle(f, &owned, side));
    result = 0;
cleanup:
    if (!tick_cleanup(f, &owned)) {
        fputs("Piston tick cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    if (result == 0 && printf("PASS: %u Piston tick checks (world-piston-tick)\n", checks) < 0)
        result = 1;
    return result;
}
