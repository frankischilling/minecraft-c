/* Reuse actual GAME Registry/World/Store/Chunk construction and transport.
 * The imported standalone fixture main has a distinct external name. */
#define MC_COLLISION_BOXES_MAIN mc_world_movement_boxes_fixture_main
#include "../../world/runtime/collision/test_boxes.c"
#undef MC_COLLISION_BOXES_MAIN

#include "../../../src/core/java/random_profile_internal.h"
#include "mc/world/dimension/biome_runtime.h"
#include "mc/world/runtime/movement.h"

#include <emmintrin.h>

/* These are constructed native contracts. Temporary nullable references,
 * mutable fields, descriptor/hold and MXCSR controls are native invariants;
 * each is restored before CHECK can branch. No World, Chunk, Biome, collision
 * result, callback or source method body is fabricated. Fixture voxel and
 * height bytes are explicit prepared source inputs on actual loaded owners. */

static float world_move_float(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static bool world_move_idle(fixture *f, size_t side) {
    return f->worlds[side]->active_calls == 0 && f->worlds[side]->local_leases == 0 &&
           f->providers[side].cache->count == 1 && f->providers[side].cache->insertion_count == 1 &&
           mc_world_store_chunk_count(f->stores[side]) == 1 &&
           mc_chunk_cache_validate(f->providers[side].cache);
}

static mc_world_storage_status world_move_rain(fixture *f, size_t side, int32_t x, int32_t y,
                                               int32_t z, float previous, float current, bool *out,
                                               mc_world_movement_error *error) {
    mc_world_runtime *world = f->worlds[side];
    const float saved_i = world->fields.i, saved_j = world->fields.j;
    world->fields.i = previous;
    world->fields.j = current;
    const mc_world_storage_status status = mc_world_runtime_rain_at(world, x, y, z, out, error);
    world->fields.i = saved_i;
    world->fields.j = saved_j;
    return status;
}

static bool world_move_area(fixture *f, size_t side) {
    bool ok = false, value = false;
    mc_world_runtime *world = f->worlds[side];
    mc_world_movement_error error;
    const mc_java_random before = *world->fields.r;
    CHECK(mc_world_runtime_area_loaded(world, 0, 0, 0, 15, 127, 15, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value && error.bytecode == 0 && world_move_idle(f, side));
    CHECK(mc_world_runtime_area_loaded(world, 16, 0, 0, 31, 127, 15, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          !value && world_move_idle(f, side));
    CHECK(mc_world_runtime_area_loaded(world, 0, 0, 0, 16, 127, 15, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          !value && world_move_idle(f, side));
    CHECK(mc_world_runtime_area_loaded(world, 0, 100, 0, 15, 10, 15, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value);
    CHECK(mc_chunk_cache_key(0, 0) == mc_chunk_cache_key(32768, 0));
    CHECK(mc_world_runtime_area_loaded(world, 524288, 0, 0, 524303, 127, 15, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value && world_move_idle(f, side));
    CHECK(f->states[side]->x == 0 && f->states[side]->z == 0 &&
          mc_test_java_random_equal(&before, world->fields.r));

    /* Actual nullable provider and bad FP controls. Early Y and reversed X/Z
     * loops must finish without either provider use or FP admission. */
    mc_world_provider *saved_v = world->fields.v;
    const unsigned int saved_csr = _mm_getcsr();
    world->fields.v = NULL;
    _mm_setcsr(saved_csr | 0x8000u);
    bool below = true, above = true, reverse_x = false, reverse_z = false;
    const mc_world_storage_status a =
        mc_world_runtime_area_loaded(world, 0, 0, 0, 15, -1, 15, &below, &error);
    const mc_world_storage_status b =
        mc_world_runtime_area_loaded(world, 0, 128, 0, 15, 129, 15, &above, &error);
    const mc_world_storage_status c =
        mc_world_runtime_area_loaded(world, 16, 0, 0, 0, 127, 15, &reverse_x, &error);
    const mc_world_storage_status d =
        mc_world_runtime_area_loaded(world, 0, 0, 16, 15, 127, 0, &reverse_z, &error);
    value = true;
    const mc_world_storage_status reached =
        mc_world_runtime_area_loaded(world, 0, 0, 0, 15, 127, 15, &value, &error);
    _mm_setcsr(saved_csr);
    world->fields.v = saved_v;
    CHECK(a == MC_WORLD_STORAGE_OK && !below && b == MC_WORLD_STORAGE_OK && !above &&
          c == MC_WORLD_STORAGE_OK && reverse_x && d == MC_WORLD_STORAGE_OK && reverse_z);
    CHECK(reached == MC_WORLD_STORAGE_UNSUPPORTED_STATE && value && error.bytecode == 69 &&
          error.area_bytecode == 69 && error.presence_bytecode == 6 &&
          error.provider_bytecode == 6 && world_move_idle(f, side));
    CHECK(mc_test_java_random_equal(&before, world->fields.r));
    ok = true;
cleanup:
    return ok;
}

static bool world_move_rain_threshold(fixture *f, size_t side) {
    bool ok = false, value = true;
    mc_world_runtime *world = f->worlds[side];
    mc_world_movement_error error;
    mc_world_provider *saved_v = world->fields.v;
    world->fields.v = NULL;
    bool dry = true, below = true, nan = true;
    const mc_world_storage_status a = world_move_rain(f, side, 8, 64, 8, 1.0f, 0.0f, &dry, &error);
    const mc_world_storage_status b = world_move_rain(
        f, side, 8, 64, 8, 0.0f, world_move_float(UINT32_C(0x3e4ccccc)), &below, &error);
    const mc_world_storage_status c = world_move_rain(
        f, side, 8, 64, 8, 0.0f, world_move_float(UINT32_C(0x7fc12345)), &nan, &error);
    /* float 0.2 is strictly above double 0.2, so it reaches sky acquisition. */
    const mc_world_storage_status threshold =
        world_move_rain(f, side, 8, 64, 8, 0.0f, 0.2f, &value, &error);
    world->fields.v = saved_v;
    CHECK(a == MC_WORLD_STORAGE_OK && !dry && b == MC_WORLD_STORAGE_OK && !below &&
          c == MC_WORLD_STORAGE_OK && !nan);
    CHECK(threshold == MC_WORLD_STORAGE_UNSUPPORTED_STATE && value && error.bytecode == 13 &&
          error.query.bytecode == 7 && error.query.provider_bytecode == 6 &&
          error.registry.status == MC_REGISTRY_OK && world_move_idle(f, side));

    world->fields.v = f->worlds[1u - side]->fields.v;
    value = true;
    const mc_world_storage_status foreign =
        world_move_rain(f, side, 8, 64, 8, 0.0f, 1.0f, &value, &error);
    world->fields.v = saved_v;
    CHECK(foreign == MC_WORLD_STORAGE_UNSUPPORTED_STATE && value && error.bytecode == 13 &&
          error.query.bytecode == 7 && error.query.provider_bytecode == 6 &&
          error.query.acquisition == MC_CHUNK_ACQUIRE_OK && world_move_idle(f, side));
    ok = true;
cleanup:
    return ok;
}

static bool world_move_rain_sources(fixture *f, size_t side) {
    bool ok = false, value = false;
    mc_world_runtime *world = f->worlds[side];
    mc_world_dimension *fixed = NULL;
    mc_world_movement_error error;
    boxes_air(f, side);
    CHECK(world_move_rain(f, side, 8, 64, 8, 0.0f, 1.0f, &value, &error) == MC_WORLD_STORAGE_OK);
    mc_world_dimension_fields normal = {0};
    mc_world_biome_game_fields sampled = {0};
    CHECK(mc_world_dimension_fields_get(f->dimensions[side], &normal) == MC_WORLD_DIMENSION_OK &&
          mc_world_dimension_biome_game_fields_get(normal.b, &sampled) == MC_WORLD_DIMENSION_OK &&
          sampled.d != NULL && mc_biome_reference_array_length(sampled.d) >= 1);
    const mc_biome *biome = mc_biome_reference_array_data(sampled.d)[0];
    const mc_biome_fields *actual = mc_biome_fields_get(biome);
    CHECK(actual != NULL && value == (!actual->v && actual->w));
    /* Actual shared Biome flag controls, never a substituted sampled result. */
    mc_biome_fields *flags = (mc_biome_fields *)actual;
    const bool saved_v = flags->v, saved_w = flags->w;
    flags->v = true;
    flags->w = true;
    value = true;
    const mc_world_storage_status snow =
        world_move_rain(f, side, 8, 64, 8, 0.0f, 1.0f, &value, &error);
    flags->v = saved_v;
    flags->w = saved_w;
    CHECK(snow == MC_WORLD_STORAGE_OK && !value);
    flags->v = false;
    flags->w = false;
    value = true;
    const mc_world_storage_status disabled =
        world_move_rain(f, side, 8, 64, 8, 0.0f, 1.0f, &value, &error);
    flags->v = saved_v;
    flags->w = saved_w;
    CHECK(disabled == MC_WORLD_STORAGE_OK && !value);
    flags->v = false;
    flags->w = true;
    const mc_world_storage_status enabled =
        world_move_rain(f, side, 8, 64, 8, 0.0f, 1.0f, &value, &error);
    flags->v = saved_v;
    flags->w = saved_w;
    CHECK(enabled == MC_WORLD_STORAGE_OK && value);

    const size_t column = (8u << 4u) | 8u;
    uint8_t *height = f->chunks[side]->h;
    const uint8_t saved_height = height[column];
    mc_world_dimension *saved_t = world->fields.t;
    height[column] = 65;
    world->fields.t = NULL;
    value = true;
    const mc_world_storage_status hidden =
        world_move_rain(f, side, 8, 64, 8, 0.0f, 1.0f, &value, &error);
    world->fields.t = saved_t;
    height[column] = saved_height;
    CHECK(hidden == MC_WORLD_STORAGE_OK && !value);

    const size_t stone_cell = boxes_index(8, 65, 8);
    uint8_t *blocks = f->states[side]->data->blocks;
    const uint8_t saved_cell = blocks[stone_cell];
    blocks[stone_cell] = 1;
    height[column] = 0;
    world->fields.t = NULL;
    value = true;
    const mc_world_storage_status roof =
        world_move_rain(f, side, 8, 64, 8, 0.0f, 1.0f, &value, &error);
    world->fields.t = saved_t;
    height[column] = saved_height;
    blocks[stone_cell] = saved_cell;
    CHECK(roof == MC_WORLD_STORAGE_OK && !value);

    world->fields.t = NULL;
    value = true;
    const mc_world_storage_status dimension_null =
        world_move_rain(f, side, 8, 64, 8, 0.0f, 1.0f, &value, &error);
    world->fields.t = saved_t;
    CHECK(dimension_null == MC_WORLD_STORAGE_UNSUPPORTED_STATE && value && error.bytecode == 34 &&
          error.dimension_bytecode == 4 && world_move_idle(f, side));

    const mc_world_dimension_registry_binding binding = {
        f->registry[side], side == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER, f->profile};
    CHECK(mc_world_dimension_create(1, NULL, &fixed, NULL) == MC_WORLD_DIMENSION_OK &&
          mc_world_dimension_bind_game(fixed, world, &binding, NULL) == MC_WORLD_DIMENSION_OK);
    mc_world_dimension_fields fixed_fields = {0};
    mc_world_biome_game_fields fixed_before = {0}, fixed_after = {0};
    CHECK(mc_world_dimension_fields_get(fixed, &fixed_fields) == MC_WORLD_DIMENSION_OK &&
          mc_world_dimension_biome_game_fields_get(fixed_fields.b, &fixed_before) ==
              MC_WORLD_DIMENSION_OK &&
          fixed_before.fixed_e != NULL);
    const mc_biome_fields *fixed_flags = mc_biome_fields_get(fixed_before.fixed_e);
    CHECK(fixed_flags != NULL);
    world->fields.t = fixed;
    const mc_world_storage_status fixed_rain =
        world_move_rain(f, side, 8, 64, 8, 0.0f, 1.0f, &value, &error);
    world->fields.t = saved_t;
    CHECK(fixed_rain == MC_WORLD_STORAGE_OK && value == (!fixed_flags->v && fixed_flags->w) &&
          mc_world_dimension_biome_game_fields_get(fixed_fields.b, &fixed_after) ==
              MC_WORLD_DIMENSION_OK &&
          fixed_after.d == fixed_before.d && fixed_after.fixed_e == fixed_before.fixed_e &&
          world_move_idle(f, side));
    /* A positive flag control on this genuine fixed Biome distinguishes the
     * selected constant receiver from a dry constant or the normal manager. */
    mc_biome_fields *fixed_mutable = (mc_biome_fields *)fixed_flags;
    const bool fixed_v = fixed_mutable->v, fixed_w = fixed_mutable->w;
    fixed_mutable->v = false;
    fixed_mutable->w = true;
    world->fields.t = fixed;
    value = false;
    const mc_world_storage_status fixed_positive =
        world_move_rain(f, side, 8, 64, 8, 0.0f, 1.0f, &value, &error);
    world->fields.t = saved_t;
    fixed_mutable->v = fixed_v;
    fixed_mutable->w = fixed_w;
    CHECK(fixed_positive == MC_WORLD_STORAGE_OK && value && world_move_idle(f, side));
    ok = true;
cleanup:
    mc_world_dimension_release(fixed);
    return ok;
}

static bool world_move_burning(fixture *f, size_t side) {
    bool ok = false, value = true;
    mc_world_runtime *world = f->worlds[side];
    mc_registry_classes *registry = f->registry[side];
    mc_world_movement_error error;
    const mc_entity_bounds box = {8.25, 64.25, 8.25, 8.75, 64.75, 8.75};
    const mc_entity_bounds empty = {9, 64, 8, 8, 64, 8};
    const mc_entity_bounds missing = {24, 64, 8, 24.5, 64.5, 8.5};
    const mc_entity_bounds below = {8, -3, 8, 8.5, -2, 8.5};
    boxes_air(f, side);
    CHECK(mc_world_runtime_burning(world, &box, &value, &error) == MC_WORLD_STORAGE_OK && !value);
    CHECK(mc_world_runtime_burning(world, &empty, &value, &error) == MC_WORLD_STORAGE_OK && !value);
    CHECK(mc_world_runtime_burning(world, &missing, &value, &error) == MC_WORLD_STORAGE_OK &&
          !value && world_move_idle(f, side));
    CHECK(mc_world_runtime_burning(world, &below, &value, &error) == MC_WORLD_STORAGE_OK && !value);
    const char *const names[] = {"as", "D", "E"};
    for (size_t i = 0; i < COUNT(names); ++i) {
        const mc_block *block = NULL;
        CHECK(mc_registry_block_static_get(registry, names[i], &block, NULL) == MC_REGISTRY_OK);
        const mc_block_fields *fields = mc_block_fields_get(block);
        CHECK(fields != NULL && fields->bn > 0 && fields->bn <= UINT8_MAX &&
              registry->blocks.m[fields->bn] == block);
        CHECK(boxes_cell(f, side, 8, 64, 8, (uint8_t)fields->bn, 0));
        CHECK(mc_world_runtime_burning(world, &box, &value, &error) == MC_WORLD_STORAGE_OK &&
              value);
        boxes_air(f, side);
        /* Native mutable-ID invariant: actual static bn=0 makes air match at
         * this exact comparison, proving no hardcoded fire/lava ID palette. */
        mc_block *mutable = registry->blocks.m[fields->bn];
        const int32_t saved_id = mutable->fields.bn;
        mutable->fields.bn = 0;
        value = false;
        const mc_world_storage_status changed =
            mc_world_runtime_burning(world, &box, &value, &error);
        mutable->fields.bn = saved_id;
        CHECK(changed == MC_WORLD_STORAGE_OK && value);
    }
    /* Native entered-owner invariant: even all air reaches real Block active
     * use. This is not a fabricated Java initializer failure or callback. */
    const unsigned int saved_entered = registry->entered;
    registry->entered = 1;
    bool empty_value = true, absent_value = true;
    const mc_world_storage_status no_scan =
        mc_world_runtime_burning(world, &empty, &empty_value, &error);
    const mc_world_storage_status absent =
        mc_world_runtime_burning(world, &missing, &absent_value, &error);
    value = true;
    const mc_world_storage_status active = mc_world_runtime_burning(world, &box, &value, &error);
    registry->entered = saved_entered;
    CHECK(no_scan == MC_WORLD_STORAGE_OK && !empty_value && absent == MC_WORLD_STORAGE_OK &&
          !absent_value);
    CHECK(active == MC_WORLD_STORAGE_BUSY && value && error.bytecode == 120 &&
          error.registry.status == MC_REGISTRY_BUSY);

    uint8_t *saved_blocks = f->states[side]->data->blocks;
    f->states[side]->data->blocks = NULL;
    value = true;
    const mc_world_storage_status getter = mc_world_runtime_burning(world, &box, &value, &error);
    f->states[side]->data->blocks = saved_blocks;
    CHECK(getter == MC_WORLD_STORAGE_UNSUPPORTED_STATE && value && error.bytecode == 113 &&
          error.query.bytecode == 60 && error.query.chunk_bytecode == 15 &&
          error.block.nearby.query.chunk_bytecode == 15 &&
          error.registry.status == MC_REGISTRY_OK && world_move_idle(f, side));

    mc_world_provider *saved_v = world->fields.v;
    world->fields.v = f->worlds[1u - side]->fields.v;
    value = true;
    const mc_world_storage_status foreign = mc_world_runtime_burning(world, &box, &value, &error);
    world->fields.v = saved_v;
    CHECK(foreign == MC_WORLD_STORAGE_UNSUPPORTED_STATE && value && error.bytecode == 113 &&
          error.query.provider_bytecode == 6 && world_move_idle(f, side));

    const unsigned int saved_csr = _mm_getcsr();
    _mm_setcsr(saved_csr | 0x8000u);
    value = true;
    const mc_world_storage_status null_bounds =
        mc_world_runtime_burning(world, NULL, &value, &error);
    const unsigned int null_pc = error.bytecode;
    const mc_world_storage_status fp = mc_world_runtime_burning(world, &box, &value, &error);
    _mm_setcsr(saved_csr);
    CHECK(null_bounds == MC_WORLD_STORAGE_UNSUPPORTED_STATE && null_pc == 1 &&
          fp == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.bytecode == 4 && value);
    CHECK(mc_world_runtime_burning(world, &box, &value, &error) == MC_WORLD_STORAGE_OK && !value &&
          error.bytecode == 0 && error.registry.status == MC_REGISTRY_OK &&
          world_move_idle(f, side));
    ok = true;
cleanup:
    return ok;
}

static bool world_move_sound(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *world = f->worlds[side];
    mc_entity *entity = NULL;
    mc_world_movement_error error;
    CHECK(make_entity(f, side, world, 8.5, 64.0, 8.5, &entity));
    CHECK(entity != NULL && entity->random != NULL && world->fields.u != NULL &&
          world->fields.u->count == 0 && f->profile->math != NULL);
    const mc_java_random world_random = *world->fields.r, entity_random = *entity->random;
    const uint64_t uniquifier = atomic_load_explicit(&f->profile->uniquifier, memory_order_relaxed);
    const uint64_t math_state =
        atomic_load_explicit(&f->profile->math->state, memory_order_relaxed);
    const size_t saved_holds = entity->holds;
    const unsigned int saved_csr = _mm_getcsr();
    _mm_setcsr(saved_csr | 0x8000u);
    const mc_world_storage_status empty =
        mc_world_runtime_entity_sound(world, NULL, NULL, 0.7f, 1.0f, &error);
    entity->holds = SIZE_MAX;
    const mc_world_storage_status no_hold =
        mc_world_runtime_entity_sound(world, entity, NULL, 0.7f, 1.0f, &error);
    entity->holds = saved_holds;
    _mm_setcsr(saved_csr);
    CHECK(empty == MC_WORLD_STORAGE_OK && no_hold == MC_WORLD_STORAGE_OK && error.bytecode == 0);

    mc_world_reference_list *list = world->fields.u;
    void **saved_items = list->items;
    const size_t saved_count = list->count, saved_capacity = list->capacity;
    void *items[] = {NULL};
    list->items = items;
    list->count = list->capacity = 1;
    const mc_world_storage_status null_entity =
        mc_world_runtime_entity_sound(world, NULL, NULL, 0.7f, 1.0f, &error);
    const unsigned int entity_pc = error.bytecode;
    const mc_world_storage_status invocation =
        mc_world_runtime_entity_sound(world, entity, NULL, 0.7f, 1.0f, &error);
    const unsigned int invocation_pc = error.bytecode;
    const unsigned int presentation_pc = error.presentation.observer_bytecode;
    _mm_setcsr(saved_csr | 0x8000u);
    const mc_world_storage_status fp =
        mc_world_runtime_entity_sound(world, entity, NULL, 0.7f, 1.0f, &error);
    const unsigned int fp_pc = error.bytecode;
    items[0] = entity; /* Actual non-observer owner; never dereferenced as pm/jp. */
    const mc_world_storage_status cast =
        mc_world_runtime_entity_sound(world, NULL, NULL, 0.7f, 1.0f, &error);
    _mm_setcsr(saved_csr);
    list->items = saved_items;
    list->count = saved_count;
    list->capacity = saved_capacity;
    CHECK(null_entity == MC_WORLD_STORAGE_UNSUPPORTED_STATE && entity_pc == 33);
    CHECK(invocation == MC_WORLD_STORAGE_UNSUPPORTED_STATE && invocation_pc == 53 &&
          presentation_pc == 53 && entity->holds == saved_holds);
    CHECK(fp == MC_WORLD_STORAGE_UNSUPPORTED_STATE && fp_pc == 44);
    CHECK(cast == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.bytecode == 28 &&
          error.observer_bytecode == 28 && error.presentation.observer_bytecode == 28);
    CHECK(mc_test_java_random_equal(&world_random, world->fields.r) &&
          mc_test_java_random_equal(&entity_random, entity->random) &&
          atomic_load_explicit(&f->profile->uniquifier, memory_order_relaxed) == uniquifier &&
          atomic_load_explicit(&f->profile->math->state, memory_order_relaxed) == math_state);
    CHECK(world_move_idle(f, side) && entity->holds == saved_holds && world->fields.u == list &&
          list->count == 0);
    ok = true;
cleanup:
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    const uint16_t title[] = {'M', 'o', 'v', 'e'};
    if (argc != 3 || strcmp(argv[1], "world-movement-dependencies") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    for (size_t side = 0; side < 2; ++side) {
        resources *r = &f->sources[side];
        r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
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
        CHECK(mc_registry_classes_initialize_blocks(f->registry[side], NULL) == MC_REGISTRY_OK &&
              r->opens == 3 && r->closes == 1 && !r->bad);
        CHECK(boxes_open_store(f, side, root) && boxes_create_world(f, side));
    }
    for (size_t side = 0; side < 2; ++side)
        CHECK(world_move_area(f, side) && world_move_rain_threshold(f, side) &&
              world_move_rain_sources(f, side) && world_move_burning(f, side) &&
              world_move_sound(f, side));
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("World movement dependency cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    if (result == 0 &&
        printf("PASS: %u World movement checks (world-movement-dependencies)\n", checks) < 0)
        result = 1;
    return result;
}
