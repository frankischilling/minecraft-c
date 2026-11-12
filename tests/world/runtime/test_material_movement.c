/* Reuse actual dual-side GAME/Registry/Store/loaded Chunk fixture and teardown.
 * Imported mains are named and never executed; no source callback is replaced. */
#define MC_COLLISION_BOXES_MAIN mc_material_boxes_fixture_main
#include "collision/test_boxes.c"
#undef MC_COLLISION_BOXES_MAIN
#include "../../../src/entity/vector/vector_internal.h"
#include "mc/entity/movement/material.h"
#include "mc/entity/player/player.h"
#include "mc/world/runtime/material_movement.h"
#include <emmintrin.h>
#include <inttypes.h>
#include <math.h>

static uint64_t bits(double x) {
    uint64_t v;
    memcpy(&v, &x, sizeof(v));
    return v;
}

static void clear_cells(fixture *f, size_t side) {
    memset(f->states[side]->data->blocks, 0, MC_CHUNK_BLOCKS);
    memset(f->states[side]->data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
}

static bool flow_case(fixture *f, size_t side, mc_entity *entity, int c) {
    bool ok = false, value = false;
    mc_block *stair = NULL, *wrapped = NULL;
    mc_world_runtime *world = f->worlds[side];
    mc_entity_classes *classes = f->entity_classes[side];
    clear_cells(f, side);
    uint8_t id = c >= 11 ? 53 : c == 5 ? 9 : c == 6 ? 10 : c == 7 ? 11 : c == 8 ? 1 : 8;
    uint8_t neighbor_id = c == 12 ? 8 : id;
    if (c == 12) {
        const mc_block *a, *b;
        CHECK(mc_registry_block_get(f->registry[side], 53, &a, NULL) == MC_REGISTRY_OK);
        CHECK(mc_registry_block_get(f->registry[side], 8, &b, NULL) == MC_REGISTRY_OK);
        stair = (mc_block *)a;
        wrapped = stair->subtype.ss.a;
        stair->subtype.ss.a = (mc_block *)b;
    }
    CHECK(boxes_cell(f, side, 6, 64, 6, id, c == 3 || c == 4 || c == 9 ? 8 : 0));
    if (c != 3 && c != 4 && c != 9) {
        CHECK(boxes_cell(f, side, 5, 64, 6, neighbor_id, c == 2 ? 7 : 0));
        CHECK(boxes_cell(f, side, 6, 64, 5, neighbor_id, 0));
        CHECK(boxes_cell(f, side, 6, 64, 7, neighbor_id, c == 1 ? 7 : 0));
        if (c == 10)
            CHECK(boxes_cell(f, side, 7, 63, 6, id, 5));
        else
            CHECK(boxes_cell(f, side, 7, 64, 6, neighbor_id, 7));
    }
    if (c == 3)
        CHECK(boxes_cell(f, side, 6, 64, 5, 1, 0));
    if (c == 4)
        CHECK(boxes_cell(f, side, 6, 64, 5, 79, 0));
    const mc_block *selected;
    CHECK(mc_registry_block_get(f->registry[side], id, &selected, NULL) == MC_REGISTRY_OK);
    mc_material *m = mc_block_fields_get(selected)->bA;
    mc_entity_base_fields *fields = mc_entity_runtime_fields(entity);
    fields->velocity_x = fields->velocity_y = fields->velocity_z = 0.0;
    CHECK(mc_entity_vector_pool_reset(classes, NULL) == MC_ENTITY_OK);
    mc_entity_bounds bounds = {6.1, 64.1, 6.1, 6.2, 64.2, 6.2};
    mc_world_material_movement_error error;
    CHECK(mc_world_runtime_material_accelerate(world, &bounds, m, entity, NULL, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value);
    mc_entity_vector_snapshot pool;
    CHECK(mc_entity_vector_get_snapshot(classes, &pool) == MC_ENTITY_OK && pool.holds == 0);
    CHECK(printf("FLOW %zu %d %d %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %" PRId32 "\n", side,
                 c, value, bits(fields->velocity_x), bits(fields->velocity_y),
                 bits(fields->velocity_z), pool.cursor) > 0);
    if (c == 2 || c == 4 || c == 8 || c == 9 || c == 11)
        CHECK(fields->velocity_x == 0 && fields->velocity_y == 0 && fields->velocity_z == 0);
    if (c == 3)
        CHECK(fields->velocity_y == -0.014 && fields->velocity_x == 0 && fields->velocity_z == 0);
    if (c == 0 || c == 5 || c == 6 || c == 7 || c == 10 || c == 12)
        CHECK(fields->velocity_x == 0.014);
    ok = true;
cleanup:
    if (stair != NULL)
        stair->subtype.ss.a = wrapped;
    return ok;
}

static bool scans(fixture *f, size_t side, mc_entity *entity) {
    bool ok = false, v = false;
    clear_cells(f, side);
    mc_world_material_movement_error e;
    mc_material *water = mc_materials_get(f->materials, MC_MATERIAL_g),
                *air = mc_materials_get(f->materials, MC_MATERIAL_a);
    CHECK(boxes_cell(f, side, 6, 64, 6, 8, 7));
    const mc_entity_bounds boxes[] = {{6.1, 64.1, 6.1, 6.2, 64.2, 6.2},
                                      {6.1, 65.0, 6.1, 6.2, 65.2, 6.2},
                                      {6.9, 64.9, 6.9, 6.1, 64.1, 6.1},
                                      {6, 64, 6, 5, 63, 5},
                                      {16, 64, 6, 16.2, 64.2, 6.2},
                                      {6, -0.2, 6, 6.2, -0.1, 6.2},
                                      {-32000000.2, 64, 6, -32000000.1, 64.2, 6.2},
                                      {6, 64, -32000000.2, 6.2, 64.2, -32000000.1},
                                      {NAN, 64, 6, -1, 64.2, 6.2},
                                      {6, INFINITY, 6, 6.2, INFINITY, 6.2},
                                      {6, -INFINITY, 6, 6.2, -INFINITY, 6.2}};
    for (size_t i = 0; i < COUNT(boxes); ++i) {
        if (i == 4) {
            mc_entity_vector_snapshot before, after;
            CHECK(mc_entity_vector_get_snapshot(f->entity_classes[side], &before) == MC_ENTITY_OK);
            CHECK(mc_world_runtime_material_accelerate(f->worlds[side], &boxes[i], water, NULL,
                                                       NULL, &v, &e) == MC_WORLD_STORAGE_OK &&
                  !v);
            CHECK(mc_entity_vector_get_snapshot(f->entity_classes[side], &after) == MC_ENTITY_OK &&
                  before.cursor == after.cursor);
            CHECK(printf("SCAN %zu %zu 2 %d\n", side, i, v) > 0);
            continue;
        }
        CHECK(mc_world_runtime_contains_material(f->worlds[side], &boxes[i], water, NULL, &v, &e) ==
              MC_WORLD_STORAGE_OK);
        if (i == 6 || i == 7)
            CHECK(e.cell_work == 1);
        CHECK(printf("SCAN %zu %zu 0 %d\n", side, i, v) > 0);
        CHECK(mc_world_runtime_contains_liquid(f->worlds[side], &boxes[i], NULL, &v, &e) ==
              MC_WORLD_STORAGE_OK);
        if (i == 6 || i == 7)
            CHECK(e.cell_work == 2);
        CHECK(printf("SCAN %zu %zu 1 %d\n", side, i, v) > 0);
    }
    const mc_entity_bounds b = {6.1, 64.1, 6.1, 6.2, 64.2, 6.2};
    CHECK(mc_world_runtime_contains_material(f->worlds[side], &b, air, NULL, &v, &e) ==
              MC_WORLD_STORAGE_OK &&
          !v);
    const mc_world_material_movement_limits no_work = {0};
    v = true;
    CHECK(mc_world_runtime_contains_material(f->worlds[side], &b, water, &no_work, &v, &e) ==
              MC_WORLD_STORAGE_LIMIT &&
          v && e.bytecode == 91);
    CHECK(mc_world_runtime_material_accelerate(f->worlds[side], &b, water, NULL, NULL, &v, &e) ==
              MC_WORLD_STORAGE_OK &&
          v);
    CHECK(boxes_cell(f, side, 7, 64, 6, 8, 0));
    CHECK(mc_world_runtime_material_accelerate(f->worlds[side], &b, water, NULL, NULL, &v, &e) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          e.bytecode == 246);
    CHECK(mc_entity_set_position(entity, 6.1, 64.0, 6.1) == MC_ENTITY_OK);
    for (int level = 0; level < 16; ++level) {
        CHECK(boxes_cell(f, side, 6, 64, 6, 8, level));
        const double ys[] = {64.0, 64.2, 64.5, 64.9, 65.0, NAN, INFINITY, -INFINITY};
        for (size_t i = 0; i < COUNT(ys); ++i) {
            CHECK(mc_entity_set_position(entity, 6.1, ys[i], 6.1) == MC_ENTITY_OK);
            mc_entity_material_error eye;
            CHECK(mc_entity_inside_material(entity, water, &v, &eye) == MC_ENTITY_OK);
            CHECK(printf("EYE %zu %d %zu %d\n", side, level, i, v) > 0);
        }
    }
    ok = true;
cleanup:
    return ok;
}

static bool failure_prefixes(fixture *f, size_t side) {
    bool ok = false, value = true;
    mc_world_material_movement_error error, second;
    mc_world_runtime *world = f->worlds[side];
    mc_entity_classes *classes = f->entity_classes[side];
    mc_material *water = mc_materials_get(f->materials, MC_MATERIAL_g);
    mc_entity *null_world = NULL;
    clear_cells(f, side);
    CHECK(mc_world_runtime_contains_material(world, NULL, water, NULL, &value, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          value && error.bytecode == 1);
    CHECK(boxes_cell(f, side, 6, 64, 6, 8, 0));
    const mc_entity_bounds b = {6.1, 64.1, 6.1, 6.2, 64.2, 6.2};
    const mc_block *block;
    CHECK(mc_registry_block_get(f->registry[side], 8, &block, NULL) == MC_REGISTRY_OK);
    mc_block *mutable_block = (mc_block *)block;
    mc_material *saved_material = mutable_block->fields.bA;
    mutable_block->fields.bA = NULL;
    const mc_world_storage_status matched =
        mc_world_runtime_contains_material(world, &b, NULL, NULL, &value, &error);
    bool liquid = true;
    const mc_world_storage_status missing =
        mc_world_runtime_contains_liquid(world, &b, NULL, &liquid, &second);
    mutable_block->fields.bA = saved_material;
    CHECK(matched == MC_WORLD_STORAGE_OK && value &&
          missing == MC_WORLD_STORAGE_UNSUPPORTED_STATE && liquid && second.bytecode == 151 &&
          second.registry.status == MC_REGISTRY_NULL_REFERENCE);
    mc_entity_vector_snapshot before, after;
    CHECK(mc_entity_vector_get_snapshot(classes, &before) == MC_ENTITY_OK);
    mc_chunk_provider *saved_provider = world->fields.v;
    world->fields.v = NULL;
    const mc_entity_bounds high = {6, 128, 6, 6.2, 128.2, 6.2};
    const mc_world_storage_status early =
        mc_world_runtime_material_accelerate(world, &high, water, NULL, NULL, &value, &error);
    bool pending = true;
    const mc_world_storage_status failed =
        mc_world_runtime_material_accelerate(world, &b, water, NULL, NULL, &pending, &second);
    world->fields.v = saved_provider;
    CHECK(early == MC_WORLD_STORAGE_OK && !value && failed != MC_WORLD_STORAGE_OK && pending &&
          second.bytecode == 73);
    CHECK(mc_entity_vector_get_snapshot(classes, &after) == MC_ENTITY_OK &&
          before.cursor == after.cursor && after.holds == 0);
    CHECK(mc_falling_entity_allocate(classes, &null_world, NULL) == MC_ENTITY_OK);
    value = true;
    CHECK(mc_entity_inside_material(null_world, water, &value, NULL) == MC_ENTITY_INVALID_PHASE &&
          value);
    CHECK(mc_falling_entity_construct_world(null_world, NULL, NULL) == MC_ENTITY_OK);
    mc_entity_material_error eye;
    CHECK(mc_entity_inside_material(null_world, water, &value, &eye) == MC_ENTITY_WORLD_FAILED &&
          value && eye.bytecode == 49);
    mc_entity_release(null_world);
    null_world = NULL;
    CHECK(mc_registry_block_get(f->registry[side], 53, &block, NULL) == MC_REGISTRY_OK);
    mc_block *stair = (mc_block *)block, *wrapped = stair->subtype.ss.a;
    CHECK(boxes_cell(f, side, 6, 64, 6, 53, 0));
    CHECK(mc_entity_vector_pool_reset(classes, NULL) == MC_ENTITY_OK);
    value = true;
    stair->subtype.ss.a = NULL;
    const mc_world_storage_status no_receiver = mc_world_runtime_material_accelerate(
        world, &b, stair->fields.bA, NULL, NULL, &value, &error);
    stair->subtype.ss.a = wrapped;
    CHECK(no_receiver == MC_WORLD_STORAGE_UNSUPPORTED_STATE && value && error.bytecode == 201 &&
          error.stair_bytecode == 13 && error.delegation_depth == 1);
    CHECK(mc_entity_vector_get_snapshot(classes, &after) == MC_ENTITY_OK && after.cursor == 1 &&
          after.holds == 0);
    const mc_world_material_movement_limits bounded = {3};
    stair->subtype.ss.a = stair;
    const mc_world_storage_status cycle = mc_world_runtime_material_accelerate(
        world, &b, stair->fields.bA, NULL, &bounded, &value, &error);
    stair->subtype.ss.a = wrapped;
    CHECK(cycle == MC_WORLD_STORAGE_LIMIT && value && error.cell_work == 3 &&
          error.delegation_depth == 2 && error.stair_bytecode == 13);
    CHECK(world->active_calls == 0 && world->local_leases == 0 &&
          f->providers[side].cache->count == 1);
    ok = true;
cleanup:
    if (null_world != NULL)
        mc_entity_release(null_world);
    return ok;
}

static bool player_eye(fixture *f) {
    bool ok = false, value = false;
    mc_entity *player = NULL;
    mc_player_session_classes *classes = NULL;
    mc_player_session *session = NULL;
    CHECK(mc_player_session_classes_create(f->entity_classes[0], f->registry[0], &classes, NULL) ==
          MC_PLAYER_OK);
    CHECK(mc_player_session_create(classes, NULL, NULL, &session, NULL) == MC_PLAYER_OK);
    CHECK(mc_local_player_allocate(f->entity_classes[0], &player, NULL) == MC_ENTITY_OK);
    CHECK(mc_local_player_construct(player, f->worlds[0], NULL, session, 0, NULL, NULL) ==
          MC_PLAYER_OK);
    clear_cells(f, 0);
    const double ys[] = {64.0, 64.2, 64.5, 64.9, 65.0, NAN, INFINITY, -INFINITY};
    for (int level = 0; level < 16; ++level) {
        CHECK(boxes_cell(f, 0, 6, 64, 6, 8, level));
        for (size_t i = 0; i < COUNT(ys); ++i) {
            CHECK(mc_entity_set_position(player, 6.1, ys[i], 6.1) == MC_ENTITY_OK);
            CHECK(mc_entity_inside_material(player, mc_materials_get(f->materials, MC_MATERIAL_g),
                                            &value, NULL) == MC_ENTITY_OK);
            CHECK(printf("PLAYER 0 %d %zu %d\n", level, i, value) > 0);
        }
    }
    ok = true;
cleanup:
    if (player != NULL)
        mc_entity_release(player);
    mc_player_session_retire(&session);
    if (mc_player_session_classes_free(&classes, NULL) != MC_PLAYER_OK)
        ok = false;
    return ok;
}

static bool vector_failures(mc_entity_classes *existing) {
    bool ok = false;
    mc_entity_classes *classes = NULL;
    mc_entity_vector *v = NULL;
    mc_entity_vector *sentinel = (mc_entity_vector *)&classes;
    mc_entity_vector_error error;
    mc_entity_vector_snapshot before, after;
    mc_entity_limits limits = {sizeof(*classes), SIZE_MAX};
    CHECK(mc_entity_classes_create(existing->source, existing->profile, &limits, &classes) ==
          MC_ENTITY_OK);
    v = sentinel;
    CHECK(mc_entity_vector_pool_get(classes, 1, 2, 3, &v, &error) == MC_ENTITY_LIMIT &&
          v == sentinel);
    CHECK(mc_entity_vector_get_snapshot(classes, &after) == MC_ENTITY_OK &&
          after.phase == MC_ENTITY_CLASS_FAILED);
    CHECK(mc_entity_vector_pool_get(classes, 1, 2, 3, &v, &error) ==
              MC_ENTITY_CLASS_FAILED_STATUS &&
          v == sentinel);
    CHECK(mc_entity_classes_free(classes) == MC_ENTITY_OK);
    classes = NULL;
    v = NULL;
    limits.max_allocation_bytes =
        sizeof(*classes) + sizeof(mc_entity_vector_pool) + sizeof(mc_entity_vector_object);
    CHECK(mc_entity_classes_create(existing->source, existing->profile, &limits, &classes) ==
          MC_ENTITY_OK);
    v = sentinel;
    CHECK(mc_entity_vector_pool_get(classes, 1, 2, 3, &v, &error) == MC_ENTITY_LIMIT &&
          v == sentinel && error.bytecode == 23);
    CHECK(mc_entity_vector_get_snapshot(classes, &after) == MC_ENTITY_OK &&
          after.phase == MC_ENTITY_CLASS_READY && after.cursor == 0 && after.size == 0);
    classes->limits.max_allocation_bytes = SIZE_MAX; /* Declared native allocator fault removal. */
    CHECK(mc_entity_vector_pool_get(classes, 1, 2, 3, &v, &error) == MC_ENTITY_OK);
    CHECK(mc_entity_vector_release(classes, v) == MC_ENTITY_OK);
    v = NULL;
    CHECK(mc_entity_vector_get_snapshot(classes, &before) == MC_ENTITY_OK);
    const unsigned csr = _mm_getcsr();
    _mm_setcsr(csr | 0x8000u);
    mc_entity_vector input = {1, 2, 3};
    double length = 19;
    const mc_entity_status bad = mc_entity_vector_length(&input, &length, &error);
    _mm_setcsr(csr);
    CHECK(bad == MC_ENTITY_UNSUPPORTED_FP && length == 19);
    CHECK(mc_entity_vector_get_snapshot(classes, &after) == MC_ENTITY_OK &&
          before.cursor == after.cursor && before.holds == after.holds);
    ok = true;
cleanup:
    if (v != NULL && v != sentinel)
        mc_entity_vector_release(classes, v);
    if (classes != NULL && mc_entity_classes_free(classes) != MC_ENTITY_OK)
        ok = false;
    return ok;
}

static bool vectors(mc_entity_classes *classes) {
    bool ok = false;
    mc_entity_vector *a = NULL, *b = NULL, *c = NULL;
    mc_entity_vector_snapshot snap;
    CHECK(mc_entity_vector_create(classes, -0.0, -0.0, -0.0, &a, NULL) == MC_ENTITY_OK &&
          bits(a->x) == 0);
    CHECK(mc_entity_vector_pool_reset(classes, NULL) == MC_ENTITY_OK);
    CHECK(mc_entity_vector_pool_get(classes, -0.0, 2, 3, &b, NULL) == MC_ENTITY_OK &&
          bits(b->x) == UINT64_C(0x8000000000000000));
    CHECK(mc_entity_vector_pool_reset(classes, NULL) == MC_ENTITY_OK);
    CHECK(mc_entity_vector_pool_get(classes, 4, 5, 6, &c, NULL) == MC_ENTITY_OK && b == c &&
          b->x == 4);
    CHECK(mc_entity_classes_free(classes) == MC_ENTITY_BUSY);
    CHECK(mc_entity_vector_release(classes, a) == MC_ENTITY_OK);
    a = NULL;
    CHECK(mc_entity_vector_release(classes, b) == MC_ENTITY_OK);
    b = NULL;
    CHECK(mc_entity_vector_release(classes, c) == MC_ENTITY_OK);
    c = NULL;
    CHECK(mc_entity_vector_get_snapshot(classes, &snap) == MC_ENTITY_OK && snap.holds == 0);
    const double values[][3] = {{0, 0, 0},          {3, 4, 12},
                                {0.00009, 0, 0},    {0.0001, 0, 0},
                                {-0.0, -0.0, -0.0}, {NAN, 0, 0},
                                {INFINITY, 0, 0},   {1.7976931348623157e308, 0, 0},
                                {NAN, NAN, 0},      {1e-300, 2e-300, 3e-300}};
    for (size_t i = 0; i < COUNT(values); ++i) {
        double vx = values[i][0], vy = values[i][1];
        if (i == 8) {
            uint64_t bx = UINT64_C(0xfff8000000000123), by = UINT64_C(0x7ff8000000000456);
            memcpy(&vx, &bx, sizeof(vx));
            memcpy(&vy, &by, sizeof(vy));
        }
        CHECK(mc_entity_vector_pool_reset(classes, NULL) == MC_ENTITY_OK);
        CHECK(mc_entity_vector_pool_get(classes, vx, vy, values[i][2], &b, NULL) == MC_ENTITY_OK);
        double length;
        CHECK(mc_entity_vector_length(b, &length, NULL) == MC_ENTITY_OK);
        CHECK(mc_entity_vector_normalize(classes, b, &c, NULL) == MC_ENTITY_OK);
        CHECK(mc_entity_vector_get_snapshot(classes, &snap) == MC_ENTITY_OK);
        CHECK(printf("VEC %d %zu %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64
                     " %" PRId32 "\n",
                     (int)classes->source, i, bits(length), bits(c->x), bits(c->y), bits(c->z),
                     snap.cursor) > 0);
        CHECK(mc_entity_vector_release(classes, b) == MC_ENTITY_OK);
        b = NULL;
        CHECK(mc_entity_vector_release(classes, c) == MC_ENTITY_OK);
        c = NULL;
    }
    if (classes->source == MC_ENTITY_CLIENT) {
        CHECK(mc_entity_vector_pool_get(classes, 8, 9, 10, &b, NULL) == MC_ENTITY_OK);
        CHECK(mc_entity_vector_pool_clear_client(classes, NULL) == MC_ENTITY_OK && b->x == 8);
        CHECK(mc_entity_vector_get_snapshot(classes, &snap) == MC_ENTITY_OK && snap.cursor == 0 &&
              snap.size == 0 && snap.holds == 1);
        CHECK(mc_entity_vector_release(classes, b) == MC_ENTITY_OK);
        b = NULL;
    }
    ok = true;
cleanup:
    if (a)
        mc_entity_vector_release(classes, a);
    if (b)
        mc_entity_vector_release(classes, b);
    if (c)
        mc_entity_vector_release(classes, c);
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    const uint16_t title[] = {'M', 'a', 't', 'e', 'r', 'i', 'a', 'l'};
    if (argc != 3 || strcmp(argv[1], "material-movement") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_item_stack_allocate(&f->stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(f->stack, 262, 5, 7) == MC_ITEM_STACK_OK);
    for (size_t side = 0; side < 2; ++side) {
        resources *r = &f->sources[side];
        r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1};
        r->streams[1] = (stream){.bytes = language};
        r->streams[2] = (stream){.bytes = language};
        r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
        const mc_registry_options options = {side == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                                             f->profile, f->materials, 0, &r->api};
        CHECK(mc_registry_classes_allocate(&options, &f->registry[side], NULL) == MC_REGISTRY_OK);
        CHECK(mc_world_runtime_classes_create(
                  side == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER, f->profile, &trig,
                  &f->classes[side], NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_classes_bind_registry(f->classes[side], f->registry[side], NULL) ==
              MC_REGISTRY_OK);
        CHECK(mc_entity_classes_create(side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile,
                                       NULL, &f->entity_classes[side]) == MC_ENTITY_OK);
        CHECK(mc_world_runtime_classes_bind_entities(f->classes[side], f->entity_classes[side]) ==
              MC_ENTITY_OK);
        CHECK(mc_registry_classes_initialize_blocks(f->registry[side], NULL) == MC_REGISTRY_OK);
        CHECK(boxes_open_store(f, side, root) && boxes_create_world(f, side));
        mc_entity *entity = NULL;
        CHECK(mc_falling_entity_allocate(f->entity_classes[side], &entity, NULL) == MC_ENTITY_OK);
        f->entities[f->entity_count++] = entity;
        CHECK(mc_falling_entity_construct(entity, f->worlds[side], 6.1, 64, 6.1, 1, NULL) ==
              MC_ENTITY_OK);
        mc_entity_vector_snapshot cold;
        CHECK(mc_entity_vector_get_snapshot(f->entity_classes[side], &cold) == MC_ENTITY_OK &&
              cold.phase == MC_ENTITY_CLASS_COLD);
        const mc_entity_bounds absent = {16, 64, 6, 16.2, 64.2, 6.2};
        bool present = true;
        CHECK(mc_world_runtime_material_accelerate(
                  f->worlds[side], &absent, mc_materials_get(f->materials, MC_MATERIAL_g), NULL,
                  NULL, &present, NULL) == MC_WORLD_STORAGE_OK &&
              !present);
        CHECK(mc_entity_vector_get_snapshot(f->entity_classes[side], &cold) == MC_ENTITY_OK &&
              cold.phase == MC_ENTITY_CLASS_COLD && cold.pool_identity == NULL);
        for (int i = 0; i < 13; ++i)
            CHECK(flow_case(f, side, entity, i));
        CHECK(scans(f, side, entity));
        CHECK(vectors(f->entity_classes[side]));
        CHECK(vector_failures(f->entity_classes[side]));
        CHECK(failure_prefixes(f, side));
    }
    CHECK(player_eye(f));
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("material cleanup refused\n", stderr);
        return 1;
    }
    if (result == 0 && printf("Material movement: %u checks\n", checks) < 0)
        result = 1;
    return result;
}
