/* Reuse actual GAME World/Store/Chunk ownership and fixture transport. */
#define MC_COLLISION_BOXES_MAIN mc_piston_geometry_boxes_fixture_main
#include "test_boxes.c"
#undef MC_COLLISION_BOXES_MAIN

#include "../../../../src/block/registry/runtime_internal.h"
#include "../../../../src/world/runtime/tile_entities/tile_entities_internal.h"

#include <emmintrin.h>

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
    mc_tile_entity *tile_entity = NULL;
    mc_tile_entity_error tile_entity_error = mc_tile_entity_empty_error();
    mc_live_tile_entity_error live_error = mc_live_tile_entities_empty_error();
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
    CHECK(
        mc_tile_entity_allocate(world, MC_TILE_ENTITY_PISTON, &tile_entity) == MC_TILE_ENTITY_OK &&
        mc_tile_entity_piston_construct(tile_entity, 1, -123, 5, true, true, &tile_entity_error) ==
            MC_TILE_ENTITY_OK &&
        mc_tile_entity_read_nbt(tile_entity, &input, &tile_entity_error) == MC_TILE_ENTITY_OK);
    boxes_air(f, side);
    f->states[side]->data->blocks[boxes_index(3, 64, 5)] = 36;
    CHECK(mc_live_tile_entities_set(world, 3, 64, 5, tile_entity, &live_error) ==
          MC_WORLD_STORAGE_OK);
    mc_tile_entity_snapshot state;
    CHECK(mc_tile_entity_get_snapshot(tile_entity, &state) == MC_TILE_ENTITY_OK &&
          state.world == world && state.x == 3 && state.y == 64 && state.z == 5 &&
          state.piston_render_head && state.piston_previous_progress_bits == UINT32_C(0x3e800000));
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
    CHECK(mc_tile_entity_get_snapshot(tile_entity, &state) == MC_TILE_ENTITY_OK &&
          state.world == world && state.piston_progress_bits == UINT32_C(0x3e800000) &&
          state.piston_previous_progress_bits == UINT32_C(0x3e800000));
    fields[4].value.byte_value = 0;
    CHECK(mc_tile_entity_read_nbt(tile_entity, &input, &tile_entity_error) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_set_coordinates(tile_entity, 3, 64, 5) == MC_TILE_ENTITY_OK);
    const mc_entity_bounds retracting = {2.75, 64, 5, 3.75, 65, 6};
    CHECK(boxes_query_is(world, NULL, &query, &retracting, 1));
    CHECK(mc_live_tile_entities_remove(world, 3, 64, 5, &live_error) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    if (actual != NULL)
        (void)mc_entity_bounds_release(f->entity_classes[side], actual);
    mc_tile_entity_release(tile_entity);
    return ok;
}

static mc_entity_bounds piston_block_bounds(const mc_block *block) {
    return (mc_entity_bounds){block->fields.bs, block->fields.bt, block->fields.bu,
                              block->fields.bv, block->fields.bw, block->fields.bx};
}

static void piston_block_marker(mc_block *block) {
    block->fields.bs = -11;
    block->fields.bt = -12;
    block->fields.bu = -13;
    block->fields.bv = 21;
    block->fields.bw = 22;
    block->fields.bx = 23;
}

static mc_world_storage_status piston_state_shape(fixture *f, size_t side, const mc_block *block,
                                                  mc_world_collision_error *error) {
    *error = mc_world_collision_empty_error();
    mc_world_runtime *world = f->worlds[side];
    const mc_world_storage_status entered = mc_world_runtime_call_enter(world, NULL);
    if (entered != MC_WORLD_STORAGE_OK)
        return entered;
    const mc_world_storage_status status =
        mc_world_collision_block_state_shape_held(world, block, 3, 64, 5, error);
    mc_world_runtime_call_leave(world);
    return status;
}

static bool piston_state_carriers(fixture *f, size_t side) {
    bool ok = false;
    mc_registry_classes *registry = f->registry[side];
    mc_world_collision_error error;

    /* Concrete source setter tuples, including invalid-direction preservation,
     * the inherited powered-rail dispatch, standing Sign's early return, and
     * cake's deliberately inverted bounds at large metadata. */
    static const struct {
        int32_t id, metadata;
        mc_entity_bounds expected;
    } rows[] = {{1, 0, {-11, -12, -13, 21, 22, 23}},
                {90, 0, {0.375, 0, 0, 0.625, 1, 1}},
                {70, 1, {0.0625, 0, 0.0625, 0.9375, 0.03125, 0.9375}},
                {72, 2, {0.0625, 0, 0.0625, 0.9375, 0.0625, 0.9375}},
                {34, 5, {0.75, 0, 0, 1, 1, 1}},
                {34, 6, {-11, -12, -13, 21, 22, 23}},
                {29, 9, {0, 0, 0, 1, 0.75, 1}},
                {33, 14, {-11, -12, -13, 21, 22, 23}},
                {33, 2, {0, 0, 0, 1, 1, 1}},
                {78, 2, {0, 0, 0, 1, 0.375, 1}},
                {78, 15, {0, 0, 0, 1, 1, 1}},
                {64, 0, {0, 0, 0, 0.1875, 1, 1}},
                {71, 4, {0, 0, 0, 1, 1, 0.1875}},
                {77, 9, {0, 0.375, 0.3125, 0.0625, 0.625, 0.6875}},
                {77, 4, {0.3125, 0.375, 0.875, 0.6875, 0.625, 1}},
                {96, 6, {0.8125, 0, 0, 1, 1, 1}},
                {66, 2, {0, 0, 0, 1, 0.625, 1}},
                {27, 9, {0, 0, 0, 1, 0.125, 1}},
                {53, 0, {0, 0, 0, 1, 1, 1}},
                {63, 2, {-11, -12, -13, 21, 22, 23}},
                {68, 2, {0, 0.28125, 0.875, 1, 0.78125, 1}},
                {92, 15, {1.9375, 0, 0.0625, 0.9375, 0.5, 0.9375}},
                {26, 0, {0, 0, 0, 1, 0.5625, 1}},
                {69, 6, {0.25, 0, 0.25, 0.75, (double)0.6f, 0.75}},
                {69, 1, {0, (double)0.2f, 0.3125, 0.375, (double)0.8f, 0.6875}}};

    for (size_t i = 0; i < COUNT(rows); ++i) {
        bool changed;
        CHECK(mc_world_runtime_metadata_set_quiet(f->worlds[side], 3, 64, 5, rows[i].metadata,
                                                  &changed, NULL) == MC_WORLD_STORAGE_OK);
        mc_block *block = registry->blocks.m[(size_t)rows[i].id];
        CHECK(block != NULL);
        const mc_block_fields saved = block->fields;
        piston_block_marker(block);
        const mc_world_storage_status status = piston_state_shape(f, side, block, &error);
        const mc_entity_bounds actual = piston_block_bounds(block);
        block->fields = saved;
        CHECK(status == MC_WORLD_STORAGE_OK && boxes_coordinates(&actual, &rows[i].expected));
    }
    /* Portal's matching X neighbor selects its X-spanning setter tuple. */
    mc_block *portal = registry->blocks.m[90];
    const mc_block_fields saved_portal = portal->fields;
    f->states[side]->data->blocks[boxes_index(2, 64, 5)] = 90;
    const mc_world_storage_status portal_status = piston_state_shape(f, side, portal, &error);
    const mc_entity_bounds portal_actual = piston_block_bounds(portal);
    portal->fields = saved_portal;
    f->states[side]->data->blocks[boxes_index(2, 64, 5)] = 0;
    const mc_entity_bounds portal_expected = {0, 0, 0.375, 1, 1, 0.625};
    CHECK(portal_status == MC_WORLD_STORAGE_OK &&
          boxes_coordinates(&portal_actual, &portal_expected));
    CHECK(mc_world_collision_block_state_shape_in_classes_held(f->classes[side], NULL,
                                                               registry->blocks.m[1], 3, 64, 5,
                                                               &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_collision_block_state_shape_in_classes_held(f->classes[side], NULL,
                                                               registry->blocks.m[36], 3, 64, 5,
                                                               &error) != MC_WORLD_STORAGE_OK &&
          error.piston_bytecode == 6 && error.block_bytecode == 5);
    ok = true;
cleanup:
    return ok;
}

static bool piston_state_updates(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *world = f->worlds[side];
    mc_registry_classes *registry = f->registry[side];
    mc_block *moving = registry->blocks.m[36], *carried_block = registry->blocks.m[1];
    const mc_block_fields saved_moving = moving->fields, saved_carrier = carried_block->fields;
    mc_tile_entity *tile_entity = NULL, *chest = NULL;
    mc_world_collision_error error;
    mc_live_tile_entity_error live = mc_live_tile_entities_empty_error();
    const mc_entity_bounds marker = {-11, -12, -13, 21, 22, 23};
    static const mc_entity_bounds expected[6] = {
        {0.125, 0.5, 0.375, 0.625, 1, 0.875},     {0.125, 0, 0.375, 0.625, 0.5, 0.875},
        {0.125, 0.25, 0.625, 0.625, 0.75, 1.125}, {0.125, 0.25, 0.125, 0.625, 0.75, 0.625},
        {0.375, 0.25, 0.375, 0.875, 0.75, 0.875}, {-0.125, 0.25, 0.375, 0.375, 0.75, 0.875}};
    CHECK(mc_tile_entity_allocate(world, MC_TILE_ENTITY_PISTON, &tile_entity) ==
              MC_TILE_ENTITY_OK &&
          mc_tile_entity_piston_construct(tile_entity, 1, -123, 5, false, true, NULL) ==
              MC_TILE_ENTITY_OK);
    /* Explicit native input controls set source fields without advancing a
     * tick. This tests a real installed Piston and original getter ordering. */
    tile_entity->state.piston.progress_bits = UINT32_C(0x3f400000);
    tile_entity->state.piston.previous_progress_bits = UINT32_C(0x3e800000);
    boxes_air(f, side);
    f->states[side]->data->blocks[boxes_index(3, 64, 5)] = 36;
    CHECK(mc_live_tile_entities_set(world, 3, 64, 5, tile_entity, &live) == MC_WORLD_STORAGE_OK);
    mc_registry_block_bounds(carried_block, 0.125f, 0.25f, 0.375f, 0.625f, 0.75f, 0.875f);
    const size_t tile_entity_holds = tile_entity->holds;
    const int32_t cursor = f->entity_classes[side]->bounds_cursor;
    const size_t bounds_holds = f->entity_classes[side]->bounds_holds;
    for (int32_t facing = 0; facing < 6; ++facing) {
        tile_entity->state.piston.facing = facing;
        CHECK(piston_state_shape(f, side, moving, &error) == MC_WORLD_STORAGE_OK);
        const mc_entity_bounds actual = piston_block_bounds(moving);
        CHECK(boxes_coordinates(&actual, &expected[(size_t)facing]) &&
              tile_entity->holds == tile_entity_holds &&
              f->entity_classes[side]->bounds_cursor == cursor &&
              f->entity_classes[side]->bounds_holds == bounds_holds);
    }
    tile_entity->state.piston.extending = true;
    CHECK(piston_state_shape(f, side, moving, &error) == MC_WORLD_STORAGE_OK);
    const mc_entity_bounds extended = piston_block_bounds(moving);
    const mc_entity_bounds extended_expected = {-0.625, 0.25, 0.375, -0.125, 0.75, 0.875};
    CHECK(boxes_coordinates(&extended, &extended_expected));
    tile_entity->state.piston.extending = false;

    /* Null and actual same-object carriers preserve the complete singleton.
     * A different object at the same numeric ID still executes the full body. */
    piston_block_marker(moving);
    tile_entity->state.piston.block_id = 0;
    CHECK(piston_state_shape(f, side, moving, &error) == MC_WORLD_STORAGE_OK);
    mc_entity_bounds actual = piston_block_bounds(moving);
    CHECK(boxes_coordinates(&actual, &marker));
    registry->blocks.m[1] = moving;
    tile_entity->state.piston.block_id = 1;
    const mc_world_storage_status self_status = piston_state_shape(f, side, moving, &error);
    registry->blocks.m[1] = carried_block;
    actual = piston_block_bounds(moving);
    CHECK(self_status == MC_WORLD_STORAGE_OK && boxes_coordinates(&actual, &marker));
    registry->blocks.m[36] = carried_block;
    tile_entity->state.piston.block_id = 36;
    const mc_world_storage_status same_id = piston_state_shape(f, side, moving, &error);
    registry->blocks.m[36] = moving;
    actual = piston_block_bounds(moving);
    CHECK(same_id == MC_WORLD_STORAGE_OK && boxes_coordinates(&actual, &expected[5]));
    registry->blocks.m[0] = carried_block;
    tile_entity->state.piston.block_id = 0;
    const mc_world_storage_status nonnull_zero = piston_state_shape(f, side, moving, &error);
    registry->blocks.m[0] = NULL;
    actual = piston_block_bounds(moving);
    CHECK(nonnull_zero == MC_WORLD_STORAGE_OK && boxes_coordinates(&actual, &expected[5]));

    tile_entity->state.piston.block_id = -1;
    piston_block_marker(moving);
    CHECK(piston_state_shape(f, side, moving, &error) != MC_WORLD_STORAGE_OK &&
          error.piston_bytecode == 24 &&
          error.nearby.query.registry.status == MC_REGISTRY_INDEX_OUT_OF_RANGE);
    actual = piston_block_bounds(moving);
    CHECK(boxes_coordinates(&actual, &marker) && tile_entity->holds == tile_entity_holds);
    mc_block **saved_array = registry->blocks.m;
    registry->blocks.m = NULL;
    const mc_world_storage_status null_array = piston_state_shape(f, side, moving, &error);
    registry->blocks.m = saved_array;
    CHECK(null_array != MC_WORLD_STORAGE_OK && error.piston_bytecode == 24 &&
          error.nearby.query.registry.status == MC_REGISTRY_NULL_REFERENCE);

    tile_entity->state.piston.block_id = 1;
    int32_t *saved_direction = registry->direction_c;
    registry->direction_c = NULL;
    const mc_world_storage_status partial = piston_state_shape(f, side, moving, &error);
    registry->direction_c = saved_direction;
    actual = piston_block_bounds(moving);
    const mc_entity_bounds prefix = {-0.125, -12, -13, 21, 22, 23};
    CHECK(partial != MC_WORLD_STORAGE_OK && error.piston_bytecode == 110 &&
          error.nearby.query.registry.status == MC_REGISTRY_NULL_REFERENCE &&
          boxes_coordinates(&actual, &prefix) && tile_entity->holds == tile_entity_holds);
    piston_block_marker(moving);
    saved_direction = registry->direction_d;
    registry->direction_d = NULL;
    const mc_world_storage_status later_partial = piston_state_shape(f, side, moving, &error);
    registry->direction_d = saved_direction;
    actual = piston_block_bounds(moving);
    const mc_entity_bounds later_prefix = {-0.125, 0.25, -13, 21, 22, 23};
    CHECK(later_partial != MC_WORLD_STORAGE_OK && error.piston_bytecode == 131 &&
          error.nearby.query.registry.status == MC_REGISTRY_NULL_REFERENCE &&
          boxes_coordinates(&actual, &later_prefix));

    /* The carried Block setter completes before interpolation rejects a native
     * floating environment. Moving fields and pooled bounds stay untouched. */
    tile_entity->state.piston.block_id = 53;
    mc_block *stairs = registry->blocks.m[53];
    const mc_block_fields saved_stairs = stairs->fields;
    piston_block_marker(stairs);
    piston_block_marker(moving);
    const unsigned int mxcsr = _mm_getcsr();
    _mm_setcsr(mxcsr ^ 0x2000u);
    const mc_world_storage_status fp = piston_state_shape(f, side, moving, &error);
    _mm_setcsr(mxcsr);
    const mc_entity_bounds stairs_actual = piston_block_bounds(stairs);
    stairs->fields = saved_stairs;
    const mc_entity_bounds cube = {0, 0, 0, 1, 1, 1};
    actual = piston_block_bounds(moving);
    CHECK(fp != MC_WORLD_STORAGE_OK && error.piston_bytecode == 52 &&
          error.nearby.tile_entity.tile_entity.status == MC_TILE_ENTITY_UNSUPPORTED_FP &&
          boxes_coordinates(&stairs_actual, &cube) && boxes_coordinates(&actual, &marker));

    /* Every zero Direction component still multiplies NaN, and the original
     * binary32 payload widens before each binary64 subtraction. */
    tile_entity->state.piston.block_id = 1;
    tile_entity->state.piston.progress_bits = UINT32_C(0x7fc12345);
    tile_entity->state.piston.previous_progress_bits = UINT32_C(0x7fc12345);
    CHECK(piston_state_shape(f, side, moving, &error) == MC_WORLD_STORAGE_OK);
    actual = piston_block_bounds(moving);
    uint64_t nan_bits[6];
    memcpy(nan_bits, &actual, sizeof(nan_bits));
    for (size_t i = 0; i < COUNT(nan_bits); ++i)
        CHECK(nan_bits[i] == UINT64_C(0x7ff82468a0000000));
    CHECK(tile_entity->holds == tile_entity_holds &&
          f->entity_classes[side]->bounds_cursor == cursor &&
          f->entity_classes[side]->bounds_holds == bounds_holds);
    CHECK(mc_live_tile_entities_remove(world, 3, 64, 5, &live) == MC_WORLD_STORAGE_OK);
    piston_block_marker(moving);
    CHECK(piston_state_shape(f, side, moving, &error) == MC_WORLD_STORAGE_OK);
    actual = piston_block_bounds(moving);
    CHECK(boxes_coordinates(&actual, &marker));
    CHECK(mc_tile_entity_allocate(world, MC_TILE_ENTITY_CHEST, &chest) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_construct(chest) == MC_TILE_ENTITY_OK &&
          mc_live_tile_entities_set(world, 3, 64, 5, chest, &live) == MC_WORLD_STORAGE_OK);
    const size_t chest_holds = chest->holds;
    CHECK(piston_state_shape(f, side, moving, &error) == MC_WORLD_STORAGE_OK);
    actual = piston_block_bounds(moving);
    CHECK(boxes_coordinates(&actual, &marker) && chest->holds == chest_holds);
    CHECK(mc_tile_entity_set_invalid(chest, true) == MC_TILE_ENTITY_OK &&
          piston_state_shape(f, side, moving, &error) == MC_WORLD_STORAGE_OK);
    actual = piston_block_bounds(moving);
    CHECK(boxes_coordinates(&actual, &marker) && chest->invalid &&
          chest->holds == chest_holds - 1u && mc_live_tile_entities_map_valid(f->chunks[side]));
    ok = true;
cleanup:
    moving->fields = saved_moving;
    carried_block->fields = saved_carrier;
    mc_tile_entity_release(tile_entity);
    mc_tile_entity_release(chest);
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
              piston_real_tile(f, side) && piston_state_carriers(f, side) &&
              piston_state_updates(f, side));
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
