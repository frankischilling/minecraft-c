/* Reuse actual GAME World/Store/Chunk ownership and fixture transport. */
#define MC_COLLISION_BOXES_MAIN mc_piston_geometry_boxes_fixture_main
#include "test_boxes.c"
#undef MC_COLLISION_BOXES_MAIN

#include "../../../../src/block/registry/runtime_internal.h"

static mc_world_storage_status piston_shift(mc_world_runtime *world, const mc_block *moving,
                                            int32_t id, float progress, int32_t facing,
                                            mc_entity_bounds **out,
                                            mc_world_collision_error *error) {
    *error = mc_world_collision_empty_error();
    const mc_world_storage_status entered = mc_world_runtime_call_enter(world, NULL);
    if (entered != MC_WORLD_STORAGE_OK)
        return entered;
    const mc_world_storage_status status = mc_world_collision_piston_shifted_box_held(
        world, moving, 3, 64, 5, id, progress, facing, out, error);
    mc_world_runtime_call_leave(world);
    return status;
}

static bool piston_short_circuits(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *const world = f->worlds[side];
    mc_registry_classes *const registry = f->registry[side];
    const mc_block *moving = registry->blocks.m[36];
    mc_entity_bounds sentinel = {0};
    mc_entity_bounds *out = &sentinel;
    mc_world_collision_error error;
    CHECK(registry->direction_state.phase == MC_REGISTRY_COLD);
    mc_block **const saved_m = registry->blocks.m;
    registry->blocks.m = NULL;
    const mc_world_storage_status zero = piston_shift(world, moving, 0, 0.25f, -1, &out, &error);
    const bool zero_null = out == NULL;
    if (out != NULL && mc_entity_bounds_native_holds(f->entity_classes[side], out) != 0)
        (void)mc_entity_bounds_release(f->entity_classes[side], out);
    out = &sentinel;
    const mc_world_storage_status self =
        piston_shift(world, moving, 36, 0.25f, INT32_MAX, &out, &error);
    const bool self_null = out == NULL;
    registry->blocks.m = saved_m;
    CHECK(zero == MC_WORLD_STORAGE_OK && zero_null && self == MC_WORLD_STORAGE_OK && self_null &&
          registry->direction_state.phase == MC_REGISTRY_COLD);
    CHECK(piston_shift(world, moving, 37, 0.25f, INT32_MAX, &out, &error) == MC_WORLD_STORAGE_OK &&
          out == NULL && registry->direction_state.phase == MC_REGISTRY_COLD);
    out = &sentinel;
    CHECK(piston_shift(world, moving, -1, 0.25f, 0, &out, &error) != MC_WORLD_STORAGE_OK &&
          out == &sentinel && error.shifted_bytecode == 21 &&
          error.nearby.query.registry.status == MC_REGISTRY_INDEX_OUT_OF_RANGE &&
          registry->direction_state.phase == MC_REGISTRY_COLD);
    CHECK(piston_shift(world, moving, 97, 0.25f, 0, &out, &error) != MC_WORLD_STORAGE_OK &&
          out == &sentinel && error.shifted_bytecode == 27 &&
          registry->direction_state.phase == MC_REGISTRY_COLD);
    /* A new helper attempt keeps its enclosing caller frame while replacing
     * the previous attempt's deepest failure diagnostics. */
    error.piston_bytecode = 58;
    CHECK(mc_world_runtime_call_enter(world, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_storage_status reused = mc_world_collision_piston_shifted_box_held(
        world, moving, 3, 64, 5, 0, 0.25f, 0, &out, &error);
    mc_world_runtime_call_leave(world);
    CHECK(reused == MC_WORLD_STORAGE_OK && out == NULL && error.piston_bytecode == 58 &&
          error.nearby.query.status == MC_WORLD_STORAGE_OK && error.bounds.status == MC_ENTITY_OK);
    out = &sentinel;
    const int32_t cursor_before = f->entity_classes[side]->bounds_cursor;
    CHECK(piston_shift(world, moving, 1, 0.25f, -1, &out, &error) != MC_WORLD_STORAGE_OK &&
          out == &sentinel && error.shifted_bytecode == 50 &&
          error.nearby.query.registry.status == MC_REGISTRY_INDEX_OUT_OF_RANGE &&
          registry->direction_state.phase == MC_REGISTRY_READY &&
          f->entity_classes[side]->bounds_cursor == cursor_before + 1 &&
          f->entity_classes[side]->bounds_holds == 0);
    ok = true;
cleanup:
    if (out != NULL && mc_entity_bounds_native_holds(f->entity_classes[side], out) != 0)
        (void)mc_entity_bounds_release(f->entity_classes[side], out);
    return ok;
}

static bool piston_same_box_and_prefix(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *const world = f->worlds[side];
    mc_registry_classes *const registry = f->registry[side];
    mc_entity_classes *const classes = f->entity_classes[side];
    const mc_block *moving = registry->blocks.m[36];
    mc_entity_bounds *marker = NULL, *out = NULL;
    mc_world_collision_error error;
    static const mc_entity_bounds expected[6] = {
        {3, 64.25, 5, 4, 65.25, 6}, {3, 63.75, 5, 4, 64.75, 6}, {3, 64, 5.25, 4, 65, 6.25},
        {3, 64, 4.75, 4, 65, 5.75}, {3.25, 64, 5, 4.25, 65, 6}, {2.75, 64, 5, 3.75, 65, 6}};
    CHECK(mc_entity_bounds_pool_reset(classes, NULL) == MC_ENTITY_OK &&
          mc_entity_bounds_pool_get(classes, -9, -8, -7, -6, -5, -4, &marker, NULL) ==
              MC_ENTITY_OK);
    for (int32_t facing = 0; facing < 6; ++facing) {
        CHECK(mc_entity_bounds_pool_reset(classes, NULL) == MC_ENTITY_OK &&
              piston_shift(world, moving, 1, 0.25f, facing, &out, &error) == MC_WORLD_STORAGE_OK);
        const bool matches = out == marker && boxes_coordinates(out, &expected[facing]) &&
                             classes->bounds_cursor == 1 && classes->bounds_holds == 2;
        const mc_entity_status released = mc_entity_bounds_release(classes, out);
        out = NULL;
        CHECK(matches && released == MC_ENTITY_OK && classes->bounds_holds == 1);
    }
    /* This native projection fault reaches c only after both X stores. Restore
     * the actual source field before assertions or cleanup can branch. */
    CHECK(mc_entity_bounds_pool_reset(classes, NULL) == MC_ENTITY_OK);
    int32_t *const saved_c = registry->direction_c;
    registry->direction_c = NULL;
    mc_entity_bounds sentinel = {0};
    out = &sentinel;
    const mc_world_storage_status failed = piston_shift(world, moving, 1, 0.25f, 5, &out, &error);
    registry->direction_c = saved_c;
    const mc_entity_bounds prefix = {2.75, 64, 5, 3.75, 65, 6};
    const bool prefix_matches = boxes_coordinates(marker, &prefix);
    const bool unchanged_out = out == &sentinel;
    if (out != NULL && mc_entity_bounds_native_holds(classes, out) > (out == marker ? 1u : 0u))
        (void)mc_entity_bounds_release(classes, out);
    out = NULL;
    CHECK(failed != MC_WORLD_STORAGE_OK && unchanged_out && prefix_matches &&
          error.shifted_bytecode == 92 &&
          error.nearby.query.registry.status == MC_REGISTRY_NULL_REFERENCE &&
          classes->bounds_holds == 1 && classes->bounds_cursor == 1);
    /* Every zero direction component still multiplies Infinity. */
    uint32_t infinity_bits = UINT32_C(0x7f800000);
    float infinity;
    memcpy(&infinity, &infinity_bits, sizeof(infinity));
    CHECK(mc_entity_bounds_pool_reset(classes, NULL) == MC_ENTITY_OK &&
          piston_shift(world, moving, 1, infinity, 5, &out, &error) == MC_WORLD_STORAGE_OK);
    const uint64_t expected_bits[6] = {UINT64_C(0xfff0000000000000), UINT64_C(0xfff8000000000000),
                                       UINT64_C(0xfff8000000000000), UINT64_C(0xfff0000000000000),
                                       UINT64_C(0xfff8000000000000), UINT64_C(0xfff8000000000000)};
    _Static_assert(sizeof(*out) == sizeof(expected_bits), "six binary64 coordinates required");
    CHECK(out == marker && memcmp(out, expected_bits, sizeof(expected_bits)) == 0);
    ok = true;
cleanup:
    if (out != NULL)
        (void)mc_entity_bounds_release(classes, out);
    if (marker != NULL)
        (void)mc_entity_bounds_release(classes, marker);
    return ok;
}

static bool piston_real_tile(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *const world = f->worlds[side];
    mc_registry_classes *const registry = f->registry[side];
    mc_tile *tile = NULL;
    mc_tile_error tile_error = mc_tile_empty_error();
    mc_live_tile_error live_error = mc_live_tiles_empty_error();
    mc_world_collision_error error = mc_world_collision_empty_error();
    mc_entity_bounds *actual = NULL;
    uint16_t names[5][16];
    mc_nbt_tag fields[5] = {{0}};
    const mc_nbt_tag *children[5];
    static const char *const keys[5] = {"blockId", "blockData", "facing", "progress", "extending"};
    for (size_t i = 0; i < COUNT(fields); ++i) {
        fields[i].name = ascii(keys[i], names[i]);
        children[i] = &fields[i];
    }
    fields[0].type = fields[1].type = fields[2].type = MC_NBT_INT;
    fields[0].value.int_value = 1;
    fields[1].value.int_value = -123;
    fields[2].value.int_value = 5;
    fields[3].type = MC_NBT_FLOAT;
    fields[3].value.float_bits = UINT32_C(0x3e800000);
    fields[4].type = MC_NBT_BYTE;
    fields[4].value.byte_value = 1;
    const mc_nbt_tag input = {.type = MC_NBT_COMPOUND,
                              .value.compound = {children, COUNT(children)}};
    CHECK(mc_tile_allocate(world, MC_TILE_PISTON, &tile) == MC_TILE_OK &&
          mc_tile_piston_construct(tile, 1, -123, 5, true, true, &tile_error) == MC_TILE_OK &&
          mc_tile_read_nbt(tile, &input, &tile_error) == MC_TILE_OK);
    boxes_air(f, side);
    f->states[side]->data->blocks[boxes_index(3, 64, 5)] = 36;
    CHECK(mc_live_tiles_set(world, 3, 64, 5, tile, &live_error) == MC_WORLD_STORAGE_OK);
    mc_tile_snapshot state;
    CHECK(mc_tile_get_snapshot(tile, &state) == MC_TILE_OK && state.world == world &&
          state.x == 3 && state.y == 64 && state.z == 5 && state.piston_render_head &&
          state.piston_previous_progress_bits == UINT32_C(0x3e800000));
    CHECK(mc_entity_bounds_pool_reset(f->entity_classes[side], NULL) == MC_ENTITY_OK &&
          mc_world_runtime_call_enter(world, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_storage_status status =
        mc_world_collision_block_box_held(world, registry->blocks.m[36], 3, 64, 5, &actual, &error);
    mc_world_runtime_call_leave(world);
    const mc_entity_bounds expected = {2.25, 64, 5, 3.25, 65, 6};
    CHECK(status == MC_WORLD_STORAGE_OK && boxes_coordinates(actual, &expected));
    CHECK(mc_entity_bounds_release(f->entity_classes[side], actual) == MC_ENTITY_OK);
    actual = NULL;
    /* Full World collision collection reaches the same installed Piston and
     * retains its actual shifted pooled box in K. No Entity callback exists. */
    const mc_entity_bounds query = {2.6, 64.1, 5.1, 3.4, 64.9, 5.9};
    CHECK(boxes_query_is(world, NULL, &query, &expected, 1));
    CHECK(mc_tile_get_snapshot(tile, &state) == MC_TILE_OK && state.world == world &&
          state.piston_progress_bits == UINT32_C(0x3e800000) &&
          state.piston_previous_progress_bits == UINT32_C(0x3e800000));
    fields[4].value.byte_value = 0;
    CHECK(mc_tile_read_nbt(tile, &input, &tile_error) == MC_TILE_OK &&
          mc_tile_set_coordinates(tile, 3, 64, 5) == MC_TILE_OK);
    const mc_entity_bounds retracting = {2.75, 64, 5, 3.75, 65, 6};
    CHECK(boxes_query_is(world, NULL, &query, &retracting, 1));
    CHECK(mc_live_tiles_remove(world, 3, 64, 5, &live_error) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    if (actual != NULL)
        (void)mc_entity_bounds_release(f->entity_classes[side], actual);
    mc_tile_release(tile);
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    const uint16_t title[] = {'P', 'i', 's', 't', 'o', 'n'};
    if (argc != 3 || strcmp(argv[1], "world-piston-geometry") != 0 ||
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
        CHECK(piston_short_circuits(f, side) && piston_same_box_and_prefix(f, side) &&
              piston_real_tile(f, side));
    }
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("Piston geometry cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    if (result == 0 &&
        printf("PASS: %u Piston geometry checks (world-piston-geometry)\n", checks) < 0)
        result = 1;
    return result;
}
