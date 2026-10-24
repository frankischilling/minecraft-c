/* Actual GAME fixture; its named main is retained but never executed. */
#define MC_COLLISION_BOXES_MAIN mc_movement_boxes_fixture_main
#include "../../world/runtime/collision/test_boxes.c"
#undef MC_COLLISION_BOXES_MAIN
#include "../../../src/world/runtime/collision/entities_internal.h"
#include "../../support/java_random.h"
#include "mc/entity/movement/movement.h"
#include <xmmintrin.h>

/* Native source-state inputs, not original replay or injected collision answers.
 * Actual setters prepare unit boxes; transient pointer/hold faults restore before CHECK. */
static bool movement_input(mc_entity *e, double x, double y, double z) {
    e->base.offset = e->base.y_size = e->base.bp = e->base.bk = 0;
    e->base.aX = e->base.aY = e->base.aZ = e->base.ba = false;
    e->base.bb = e->base.bc = e->base.be = false;
    e->base.bd = true;
    e->base.bq = e->base.bx = e->base.bC = false;
    e->base.bu = 5;
    e->base.bv = 0;
    e->base.aG = e->base.aH = NULL;
    if (mc_entity_is_kind(e, MC_ENTITY_CLASS_KIND_ITEM))
        e->item.f = 5;
    return mc_entity_set_size(e, 1, 1) == MC_ENTITY_OK &&
           mc_entity_set_position(e, x, y, z) == MC_ENTITY_OK &&
           mc_entity_set_velocity(e, 0, 2) == MC_ENTITY_OK &&
           mc_entity_set_velocity(e, 1, 3) == MC_ENTITY_OK &&
           mc_entity_set_velocity(e, 2, 4) == MC_ENTITY_OK;
}

static void movement_air(fixture *f, size_t side) {
    boxes_air(f, side);
    memset(f->states[side]->data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
}

static bool movement_cell(fixture *f, size_t side, int32_t x, int32_t y, int32_t z, uint8_t id,
                          int32_t metadata) {
    f->states[side]->data->blocks[boxes_index(x, y, z)] = id;
    bool changed;
    return mc_world_runtime_metadata_set_quiet(f->worlds[side], x, y, z, metadata, &changed,
                                               NULL) == MC_WORLD_STORAGE_OK;
}

static void movement_floor(fixture *f, size_t side) {
    for (int32_t x = 6; x <= 12; ++x)
        for (int32_t z = 6; z <= 12; ++z)
            f->states[side]->data->blocks[boxes_index(x, 7, z)] = 1;
}

static bool movement_quiet(fixture *f, size_t side, mc_entity *e, size_t leases) {
    mc_world_runtime *w = f->worlds[side];
    return e->holds == 1 && w->active_calls == 0 && w->local_leases == leases &&
           f->entity_classes[side]->bounds_holds == w->collision_boxes.count &&
           mc_world_collision_owned_valid(w) && mc_world_nearby_owned_valid(w) &&
           f->providers[side].cache->count == 1 && f->providers[side].cache->insertion_count == 1 &&
           mc_world_store_chunk_count(f->stores[side]) == 1;
}

static bool movement_make(fixture *f, size_t side, mc_world_runtime *w, bool falling,
                          mc_entity **out) {
    bool ok = false;
    mc_entity *e = NULL;
    CHECK(f->entity_count < COUNT(f->entities));
    CHECK((falling ? mc_falling_entity_allocate(f->entity_classes[side], &e, NULL)
                   : mc_item_entity_allocate(f->entity_classes[side], &e, NULL)) == MC_ENTITY_OK &&
          e != NULL);
    f->entities[f->entity_count++] = e;
    CHECK((falling ? mc_falling_entity_construct(e, w, 8.5, 8, 8.5, 12, NULL)
                   : mc_item_entity_construct(e, w, 8.5, 8, 8.5, NULL, NULL)) == MC_ENTITY_OK);
    *out = e;
    ok = true;
cleanup:
    return ok;
}

static bool movement_capture_region(fixture *f, size_t side) {
    bool ok = false;
    char *path = NULL;
    CHECK(region_path(f, side, &path) &&
          mc_ws_read_file(path, 1024u * 1024u, &f->saved[side], NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    free(path);
    return ok;
}

#include "test_effects.c"

static bool movement_axes(fixture *f, size_t side, mc_entity *e) {
    bool ok = false;
    mc_entity_move_error error;
    mc_entity_bounds *const permanent = e->bounds;
    const uint16_t base_pc = e->base_bytecode, item_pc = e->item_bytecode,
                   falling_pc = e->falling_bytecode;
    const mc_java_random random = *e->random, world_random = *f->worlds[side]->fields.r;
    movement_air(f, side);
    CHECK(movement_input(e, 8.5, 8, 8.5));
    const mc_entity_bounds free_box = {9, 8.25, 7.5, 10, 9.25, 8.5};
    CHECK(mc_entity_move(e, 1, .25, -.5, NULL, &error) == MC_ENTITY_OK && error.bytecode == 2028 &&
          boxes_coordinates(e->bounds, &free_box) && e->base.x == 9.5 && e->base.y == 8.25 &&
          e->base.z == 8 && !e->base.aX && !e->base.aY && !e->base.aZ && !e->base.ba &&
          e->base.velocity_x == 2 && e->base.velocity_y == 3 && e->base.velocity_z == 4 &&
          e->base.bv == -5);
    movement_air(f, side);
    movement_floor(f, side);
    CHECK(movement_input(e, 8.5, 8, 8.5));
    e->base.bk = 2.5f;
    const mc_entity_bounds ground = {9, 8, 8.5, 10, 9, 9.5};
    CHECK(mc_entity_move(e, 1, -.5, .5, NULL, &error) == MC_ENTITY_OK &&
          boxes_coordinates(e->bounds, &ground) && e->base.aX && !e->base.aY && e->base.aZ &&
          e->base.ba && e->base.bk == 0 && e->base.velocity_x == 2 && e->base.velocity_y == 0 &&
          e->base.velocity_z == 4);
    movement_air(f, side);
    CHECK(movement_input(e, 8.5, 8, 8.5) && movement_cell(f, side, 10, 8, 8, 1, 0));
    const mc_entity_bounds wall = {9, 8, 8, 10, 9, 9};
    CHECK(mc_entity_move(e, 2, 0, 0, NULL, &error) == MC_ENTITY_OK &&
          boxes_coordinates(e->bounds, &wall) && e->base.aY && !e->base.aZ && !e->base.aX &&
          e->base.velocity_x == 0);
    movement_air(f, side);
    CHECK(movement_input(e, 8.5, 8, 8.5) && movement_cell(f, side, 8, 10, 8, 1, 0));
    const mc_entity_bounds ceiling = {8, 9, 8, 9, 10, 9};
    CHECK(mc_entity_move(e, 0, 2, 0, NULL, &error) == MC_ENTITY_OK &&
          boxes_coordinates(e->bounds, &ceiling) && e->base.aZ && !e->base.aX &&
          e->base.velocity_y == 0);
    /* X clips only after Y has moved into this obstacle's vertical interval. */
    movement_air(f, side);
    CHECK(movement_input(e, 8.5, 8, 8.5) && movement_cell(f, side, 10, 9, 8, 1, 0));
    const mc_entity_bounds yx = {9, 9, 9, 10, 10, 10};
    CHECK(mc_entity_move(e, 2, 1, 1, NULL, &error) == MC_ENTITY_OK &&
          boxes_coordinates(e->bounds, &yx) && e->base.velocity_x == 0 && e->base.velocity_y == 3 &&
          e->base.velocity_z == 4);
    /* Z clips only after X has moved into this obstacle's horizontal interval. */
    movement_air(f, side);
    CHECK(movement_input(e, 8.5, 8, 8.5) && movement_cell(f, side, 10, 8, 9, 1, 0));
    const mc_entity_bounds xz = {10, 8, 8, 11, 9, 9};
    CHECK(mc_entity_move(e, 2, 0, 2, NULL, &error) == MC_ENTITY_OK &&
          boxes_coordinates(e->bounds, &xz) && e->base.velocity_x == 2 && e->base.velocity_z == 0);
    movement_air(f, side);
    movement_floor(f, side);
    CHECK(movement_input(e, 8.5, 8, 8.5));
    e->base.bd = false;
    const mc_entity_bounds cancelled = {8, 8, 8, 9, 9, 9};
    CHECK(mc_entity_move(e, 1, -.5, 1, NULL, &error) == MC_ENTITY_OK &&
          boxes_coordinates(e->bounds, &cancelled) && e->base.velocity_x == 0 &&
          e->base.velocity_y == 0 && e->base.velocity_z == 0);
    movement_air(f, side);
    CHECK(movement_input(e, 8.5, 8, 8.5) && movement_cell(f, side, 10, 8, 8, 1, 0));
    e->base.bd = false;
    const mc_entity_bounds cancelled_x = {9, 8.25, 8, 10, 9.25, 9};
    CHECK(mc_entity_move(e, 2, .25, 1, NULL, &error) == MC_ENTITY_OK &&
          boxes_coordinates(e->bounds, &cancelled_x) && e->base.aZ && e->base.velocity_x == 0 &&
          e->base.velocity_y == 0 && e->base.velocity_z == 0);
    CHECK(e->bounds == permanent && e->base_bytecode == base_pc && e->item_bytecode == item_pc &&
          e->falling_bytecode == falling_pc && mc_test_java_random_equal(e->random, &random) &&
          mc_test_java_random_equal(f->worlds[side]->fields.r, &world_random) &&
          movement_quiet(f, side, e, 0));
    ok = true;
cleanup:
    return ok;
}

static bool movement_steps(fixture *f, size_t side, mc_entity *e) {
    bool ok = false;
    mc_world_collision_list_view view = {0};
    mc_entity_move_error error;
    movement_air(f, side);
    movement_floor(f, side);
    CHECK(movement_input(e, 8.5, 8, 8.5) && movement_cell(f, side, 9, 8, 8, 1, 0));
    e->base.aX = true;
    e->base.bp = 1;
    const mc_entity_bounds winner = {9, 9, 8, 10, 10, 9};
    CHECK(mc_entity_move(e, 1, -.25, 0, NULL, &error) == MC_ENTITY_OK &&
          boxes_coordinates(e->bounds, &winner) && e->base.x == 9.5 && e->base.y == 9 &&
          e->base.y_size == 0);
    /* At a fractional height, accepting a zero-distance trial tie would add
     * .25+.01 to y_size. The correct >= branch restores the flat result. */
    movement_air(f, side);
    movement_floor(f, side);
    CHECK(movement_input(e, 8.5, 8.25, 8.5) && movement_cell(f, side, 9, 8, 8, 1, 0) &&
          movement_cell(f, side, 9, 9, 8, 1, 0));
    e->base.aX = true;
    e->base.bp = 1;
    const mc_entity_bounds tie = {8, 8.25, 8, 9, 9.25, 9};
    CHECK(mc_entity_move(e, 1, 0, 0, NULL, &error) == MC_ENTITY_OK &&
          boxes_coordinates(e->bounds, &tie) && e->base.y_size == 0 && e->base.x == 8.5 &&
          e->base.y == 8.25);
    movement_air(f, side);
    movement_floor(f, side);
    CHECK(movement_input(e, 8.5, 8, 8.5) && movement_cell(f, side, 9, 8, 8, 44, 0));
    e->base.aX = true;
    e->base.bp = 1;
    const mc_entity_bounds seed = {7.5, 7.5, 7.5, 10.5, 9.5, 9.5};
    mc_world_collision_error collision = mc_world_collision_empty_error();
    CHECK(mc_world_runtime_collision_boxes(f->worlds[side], e, &seed, &view, &collision) ==
          MC_WORLD_STORAGE_OK);
    size_t before = 0;
    CHECK(mc_world_collision_list_view_count(&view, &before) == MC_WORLD_STORAGE_OK && before > 1 &&
          f->worlds[side]->local_leases == 1);
    const mc_entity_bounds slab_step = {9, 8.5, 8, 10, 9.5, 9};
    CHECK(mc_entity_move(e, 1, -.25, 0, NULL, &error) == MC_ENTITY_OK &&
          boxes_coordinates(e->bounds, &slab_step) && e->base.y_size == .51f &&
          e->base.y == 8.5 - (double).51f);
    size_t after = SIZE_MAX;
    const mc_entity_bounds *actual = NULL;
    const mc_entity_bounds slab = {9, 8, 8, 10, 8.5, 9};
    CHECK(mc_world_collision_list_view_count(&view, &after) == MC_WORLD_STORAGE_OK && after == 1 &&
          mc_world_collision_list_view_get(&view, 0, &actual) == MC_WORLD_STORAGE_OK &&
          boxes_coordinates(actual, &slab) && movement_quiet(f, side, e, 1));
    CHECK(mc_world_runtime_retire_entity_containers(f->worlds[side], NULL) ==
              MC_WORLD_STORAGE_BUSY &&
          movement_quiet(f, side, e, 1));
    mc_world_collision_list_view_release(&view);
    CHECK(movement_quiet(f, side, e, 0));
    ok = true;
cleanup:
    mc_world_collision_list_view_release(&view);
    return ok;
}

static bool movement_web(fixture *f, size_t side, mc_entity *e) {
    bool ok = false;
    mc_entity_move_error error;
    movement_air(f, side);
    CHECK(movement_input(e, 8.5, 8, 8.5) && movement_cell(f, side, 9, 8, 8, 30, 0));
    const mc_entity_bounds first = {8.5, 8, 8, 9.5, 9, 9};
    CHECK(mc_entity_move(e, .5, 0, 0, NULL, &error) == MC_ENTITY_OK &&
          boxes_coordinates(e->bounds, &first) && e->base.bc && e->base.velocity_x == 2 &&
          e->base.velocity_y == 3 && e->base.velocity_z == 4);
    movement_air(f, side);
    const mc_entity_bounds next = {8.75, 8.05000000074505806, 8.25,
                                   9.75, 9.05000000074505806, 9.25};
    CHECK(mc_entity_move(e, 1, 1, 1, NULL, &error) == MC_ENTITY_OK &&
          boxes_coordinates(e->bounds, &next) && !e->base.bc && e->base.velocity_x == 0 &&
          e->base.velocity_y == 0 && e->base.velocity_z == 0 && movement_quiet(f, side, e, 0));
    ok = true;
cleanup:
    return ok;
}

static bool movement_ledge(fixture *f, size_t side, mc_entity *e) {
    bool ok = false;
    mc_entity_move_error error;
    movement_air(f, side);
    CHECK(movement_input(e, 8.5, 8, 8.5) && movement_cell(f, side, 8, 7, 8, 1, 0));
    e->base.aX = true;
    mc_entity_watch_record *const record = e->watcher->map->head->record;
    const int8_t saved = record->byte_value;
    record->byte_value = 2;
    const mc_entity_status status = mc_entity_move(e, 1, 0, 1, NULL, &error);
    record->byte_value = saved;
    const mc_entity_bounds trimmed = {8.95, 8, 8.95, 9.95, 9, 9.95};
    CHECK(status == MC_ENTITY_OK && error.bytecode == 2028 &&
          boxes_coordinates(e->bounds, &trimmed) && !e->base.aY && !e->base.aZ && !e->base.aX &&
          e->base.velocity_x == 2 && e->base.velocity_z == 4 && movement_quiet(f, side, e, 0));
    ok = true;
cleanup:
    return ok;
}

static bool movement_null_world(fixture *f, size_t side, mc_entity *e) {
    bool ok = false;
    mc_entity_move_error error;
    CHECK(e->base.world == NULL && movement_input(e, 8.5, 8, 8.5));
    e->base.bq = true;
    e->base.bc = true;
    e->base.bk = 2.5f;
    e->base.bv = 17;
    e->base.y_size = .5f;
    const mc_entity_bounds translated = {9, 7, 10, 10, 8, 11};
    const size_t pool_holds = f->entity_classes[side]->bounds_holds;
    const int32_t cursor = f->entity_classes[side]->bounds_cursor;
    CHECK(mc_entity_move(e, 1, -1, 2, NULL, &error) == MC_ENTITY_OK && error.bytecode == 88 &&
          error.loop_work == 0 && boxes_coordinates(e->bounds, &translated) && e->base.x == 9.5 &&
          e->base.y == 6.5 && e->base.z == 10.5 && e->base.bc && e->base.bk == 2.5f &&
          e->base.bv == 17 && e->base.y_size == .5f && e->base.velocity_x == 2 &&
          e->base.velocity_y == 3 && f->entity_classes[side]->bounds_cursor == cursor &&
          f->entity_classes[side]->bounds_holds == pool_holds);
    mc_entity_bounds *const permanent = e->bounds;
    e->bounds = NULL;
    const mc_entity_status missing_no_clip = mc_entity_move(e, 1, 2, 3, NULL, &error);
    e->bounds = permanent;
    CHECK(missing_no_clip == MC_ENTITY_STATE_ERROR && error.bytecode == 15 && e->holds == 1);
    CHECK(movement_input(e, 8.5, 8, 8.5));
    e->base.y_size = .5f;
    e->base.bc = true;
    const mc_entity_bounds original = *e->bounds;
    CHECK(mc_entity_move(e, 1, 2, 3, NULL, &error) == MC_ENTITY_WORLD_FAILED &&
          error.bytecode == 394 && e->base.y_size == .2f && !e->base.bc &&
          e->base.velocity_x == 0 && e->base.velocity_y == 0 && e->base.velocity_z == 0 &&
          boxes_coordinates(e->bounds, &original) && e->base.x == 8.5 && e->base.y == 8 &&
          e->base.z == 8.5 && f->entity_classes[side]->bounds_holds == pool_holds && e->holds == 1);
    e->base.y_size = .5f;
    e->base.bc = true;
    e->bounds = NULL;
    const mc_entity_status missing_normal = mc_entity_move(e, 1, 2, 3, NULL, &error);
    e->bounds = permanent;
    CHECK(missing_normal != MC_ENTITY_OK && error.bytecode == 173 && e->base.y_size == .2f &&
          !e->base.bc && e->holds == 1);
    const size_t holds = e->holds;
    e->holds = SIZE_MAX;
    const float before_size = e->base.y_size;
    const mc_entity_status held = mc_entity_move(e, 1, 2, 3, NULL, &error);
    e->holds = holds;
    CHECK(held == MC_ENTITY_LIMIT && error.bytecode == 0 && e->base.y_size == before_size &&
          boxes_coordinates(e->bounds, &original));
    ok = true;
cleanup:
    return ok;
}

static bool movement_prefixes(fixture *f, size_t side, mc_entity *e, mc_entity *other) {
    bool ok = false;
    mc_entity_move_error error;
    movement_air(f, side);
    CHECK(movement_input(e, 8.5, 8, 8.5));
    e->base.bv = 23;
    const mc_entity_move_limits zero = {0}, one = {1}, two = {2};
    const mc_entity_bounds moved = {9, 8, 8, 10, 9, 9};
    CHECK(mc_entity_move(e, 1, 0, 0, &zero, &error) == MC_ENTITY_LIMIT && error.bytecode == 1841 &&
          error.loop_work == 0 && error.max_loop_work == 0 &&
          boxes_coordinates(e->bounds, &moved) && e->base.x == 9.5 && e->base.bv == 23 &&
          movement_quiet(f, side, e, 0));
    movement_air(f, side);
    CHECK(movement_input(e, 8.5, 9, 8.5) && movement_cell(f, side, 8, 7, 8, 1, 0));
    e->base.y_size = .5f;
    const mc_entity_bounds before = *e->bounds;
    CHECK(mc_entity_move(e, 0, -2, 0, &zero, &error) == MC_ENTITY_LIMIT && error.bytecode == 418 &&
          error.loop_work == 0 && boxes_coordinates(e->bounds, &before) && e->base.y_size == .2f &&
          f->worlds[side]->collision_boxes.count == 1 && movement_quiet(f, side, e, 0));
    CHECK(movement_input(e, 8.5, 9, 8.5));
    const mc_entity_bounds moved_y = {8, 8, 8, 9, 9, 9};
    CHECK(mc_entity_move(e, 0, -2, 0, &one, &error) == MC_ENTITY_LIMIT && error.bytecode == 520 &&
          error.loop_work == 1 && boxes_coordinates(e->bounds, &moved_y) && e->base.y == 9 &&
          e->base.velocity_y == 3 && movement_quiet(f, side, e, 0));
    movement_air(f, side);
    CHECK(movement_input(e, 8.5, 8, 8.5));
    e->base.aX = true;
    mc_entity_watch_record *const record = e->watcher->map->head->record;
    const int8_t saved = record->byte_value;
    record->byte_value = 2;
    const mc_entity_status ledge = mc_entity_move(e, 1, 0, 0, &two, &error);
    record->byte_value = saved;
    const mc_entity_bounds unchanged = {8, 8, 8, 9, 9, 9};
    CHECK(ledge == MC_ENTITY_LIMIT && error.bytecode == 229 && error.loop_work == 2 &&
          boxes_coordinates(e->bounds, &unchanged) && e->base.x == 8.5 &&
          f->worlds[side]->collision_boxes.count == 0 && movement_quiet(f, side, e, 0));
    movement_air(f, side);
    movement_floor(f, side);
    CHECK(movement_input(e, 8.5, 8, 8.5));
    e->base.bk = 3;
    e->base.aG = other;
    const mc_entity_status fall = mc_entity_move(e, 0, -.25, 0, NULL, &error);
    e->base.aG = NULL;
    CHECK(fall == MC_ENTITY_STATE_ERROR && error.bytecode == 1319 &&
          error.effect.source_bytecode == 18 && error.effect.fall_callback_bytecode == 12 &&
          e->base.aX && e->base.aZ && e->base.bk == 3 && e->base.y == 8 &&
          e->base.velocity_y == 3 && movement_quiet(f, side, e, 0));
    ok = true;
cleanup:
    return ok;
}

static bool movement_contacts(fixture *f, size_t side, mc_entity *e, bool item) {
    bool ok = false;
    mc_entity_move_error error;
    movement_air(f, side);
    CHECK(movement_input(e, 8.5, 8, 8.5) && movement_cell(f, side, 8, 8, 8, 88, 0));
    CHECK(mc_entity_move(e, 0, 0, 0, NULL, &error) == MC_ENTITY_OK && e->base.velocity_x == .8 &&
          e->base.velocity_y == 3 && e->base.velocity_z == 1.6);
    movement_air(f, side);
    CHECK(movement_input(e, 8.5, 8, 8.5) && movement_cell(f, side, 8, 8, 8, 81, 0));
    if (item)
        e->item.f = 1;
    CHECK(mc_entity_move(e, 0, 0, 0, NULL, &error) == MC_ENTITY_OK && e->base.bb &&
          e->base.be == item && e->item.f == 0);
    movement_air(f, side);
    CHECK(movement_input(e, 8.5, 8, 8.5) && mc_entity_set_size(e, 1, 2) == MC_ENTITY_OK &&
          mc_entity_set_position(e, 8.5, 8, 8.5) == MC_ENTITY_OK &&
          movement_cell(f, side, 8, 8, 8, 81, 0) && movement_cell(f, side, 8, 9, 8, 30, 0));
    e->base.bv = 19;
    const mc_entity_move_limits first_contact = {4}; /* Three K clips, then y8 cactus. */
    CHECK(mc_entity_move(e, 0, 0, 0, &first_contact, &error) == MC_ENTITY_LIMIT &&
          error.bytecode == 1841 && error.loop_work == 4 && e->base.bb &&
          e->item.f == (item ? 4 : 0) && !e->base.bc && e->base.bv == 19 &&
          movement_quiet(f, side, e, 0));
    movement_air(f, side);
    CHECK(movement_input(e, 8.5, 8, 8.5) && movement_cell(f, side, 8, 8, 8, 90, 0));
    CHECK(mc_entity_move(e, 0, 0, 0, NULL, &error) == MC_ENTITY_OK && !e->base.bb &&
          e->base.aG == NULL && e->base.aH == NULL);
    const uint8_t ids[] = {70, 28};
    const int32_t metadata[] = {1, 8};
    for (size_t i = 0; i < COUNT(ids); ++i) {
        movement_air(f, side);
        CHECK(movement_input(e, 8.5, 8, 8.5) &&
              movement_cell(f, side, 8, 8, 8, ids[i], metadata[i]));
        CHECK(mc_entity_move(e, 0, 0, 0, NULL, &error) == MC_ENTITY_OK);
        CHECK(movement_cell(f, side, 8, 8, 8, ids[i], 0));
        e->base.bv = 19;
        /* The real Living or Minecart Class query has no published candidate.
         * Contact does not insert the mover into a Chunk or manufacture a match. */
        CHECK(mc_entity_move(e, 0, 0, 0, NULL, &error) == MC_ENTITY_OK && e->base.bv == 19 &&
              movement_quiet(f, side, e, 0) && error.contact.status == MC_WORLD_STORAGE_OK &&
              error.contact.block == NULL && error.contact.bytecode == 0 &&
              error.contact.mutation.rail_sites_selected == 0 &&
              error.contact.mutation.registry.initializer_pc == -1);
        int32_t contact_metadata = -1;
        CHECK(mc_world_runtime_metadata_get(f->worlds[side], 8, 8, 8, &contact_metadata, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              contact_metadata == 0 && f->worlds[side]->local_leases == 0 &&
              f->worlds[side]->active_calls == 0);
    }
    /* Contact alone is not query membership. The real wooden helper sees no
     * published occurrence of this mover and leaves the plate off. */
    movement_air(f, side);
    CHECK(movement_input(e, 8.5, 8, 8.5) && movement_cell(f, side, 8, 8, 8, 72, 0));
    CHECK(mc_entity_move(e, 0, 0, 0, NULL, &error) == MC_ENTITY_OK &&
          movement_quiet(f, side, e, 0));
    int32_t wood_metadata = -1;
    CHECK(mc_world_runtime_metadata_get(f->worlds[side], 8, 8, 8, &wood_metadata, NULL) ==
              MC_WORLD_STORAGE_OK &&
          wood_metadata == 0 && f->worlds[side]->fields.R->count == 0);
    mc_world_runtime *const w = f->worlds[side];
    mc_chunk_provider *const provider = w->fields.v;
    const bool remote = w->fields.B;
    w->fields.B = true;
    w->fields.v = NULL;
    mc_world_entity_contact_error contact;
    const mc_world_storage_status pressure = mc_world_runtime_entity_contact(
        w, f->registry[side]->blocks.m[70], 8, 8, 8, NULL, &contact);
    const mc_world_storage_status detector = mc_world_runtime_entity_contact(
        w, f->registry[side]->blocks.m[28], 8, 8, 8, NULL, &contact);
    w->fields.v = provider;
    w->fields.B = remote;
    CHECK(pressure == MC_WORLD_STORAGE_OK && detector == MC_WORLD_STORAGE_OK &&
          movement_quiet(f, side, e, 0));
    ok = true;
cleanup:
    return ok;
}

static bool movement_fire(fixture *f, size_t side, mc_entity *e, bool item) {
    bool ok = false;
    mc_entity_move_error error;
    movement_air(f, side);
    CHECK(movement_input(e, 8.5, 8, 8.5) && movement_cell(f, side, 8, 8, 8, 51, 0));
    e->base.bC = true;
    e->base.bv = -1;
    const mc_java_random first = *e->random;
    CHECK(mc_entity_move(e, 0, 0, 0, NULL, &error) == MC_ENTITY_OK && e->base.bv == 300 &&
          e->base.bb == item && e->item.f == (item ? 4 : 0) &&
          mc_test_java_random_equal(e->random, &first));
    CHECK(movement_input(e, 8.5, 8, 8.5));
    e->base.bv = INT32_MAX;
    CHECK(mc_entity_move(e, 0, 0, 0, NULL, &error) == MC_ENTITY_OK && e->base.bv == INT32_MIN &&
          e->base.bb);
    CHECK(movement_input(e, 8.5, 8, 8.5));
    e->base.bx = true;
    const mc_java_random wet_fire = *e->random;
    CHECK(mc_entity_move(e, 0, 0, 0, NULL, &error) == MC_ENTITY_OK && e->base.bv == 0 &&
          e->base.bb && e->item.f == (item ? 4 : 0) &&
          mc_test_java_random_equal(e->random, &wet_fire));

    movement_air(f, side);
    CHECK(movement_input(e, 8.5, 8, 8.5));
    e->base.bx = true;
    e->base.bv = 12;
    mc_java_random expected = *e->random;
    (void)mc_java_random_next_float(&expected);
    (void)mc_java_random_next_float(&expected);
    const mc_java_random world_random = *f->worlds[side]->fields.r;
    CHECK(mc_entity_move(e, 0, 0, 0, NULL, &error) == MC_ENTITY_OK && e->base.bv == -5 &&
          !e->base.bb && mc_test_java_random_equal(e->random, &expected) &&
          mc_test_java_random_equal(f->worlds[side]->fields.r, &world_random));

    CHECK(movement_input(e, 8.5, 8, 8.5));
    e->base.bx = true;
    e->base.bv = 12;
    expected = *e->random;
    (void)mc_java_random_next_float(&expected);
    (void)mc_java_random_next_float(&expected);
    /* Actual constructor-created WorldAccess descriptor, with a temporary native
     * one-null-entry projection. No WorldAccess callback or success is supplied. */
    mc_world_reference_list *const world_accesses = f->worlds[side]->fields.u;
    const mc_world_reference_list saved = *world_accesses;
    void *entries[] = {NULL};
    *world_accesses = (mc_world_reference_list){entries, 1, 1};
    const mc_entity_status fizz = mc_entity_move(e, 0, 0, 0, NULL, &error);
    *world_accesses = saved;
    CHECK(fizz == MC_ENTITY_WORLD_FAILED && error.bytecode == 2016 &&
          error.world.status != MC_WORLD_STORAGE_OK && e->base.bv == 12 &&
          mc_test_java_random_equal(e->random, &expected) && movement_quiet(f, side, e, 0));
    ok = true;
cleanup:
    return ok;
}

static bool movement_provider_binding(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *const w = f->worlds[side];
    mc_chunk_provider *const original = w->fields.v;
    mc_chunk_state *const state = f->states[side];
    const size_t count = f->providers[side].cache->count;
    const size_t insertions = f->providers[side].cache->insertion_count;
    mc_world_query_error sky_error, height_error;
    mc_world_movement_error dry_error, area_error, sound_error;
    bool sky = true, dry = true, loaded = true;
    int32_t height = 37;
    /* Foreign projection names another actual World provider, not a fake backend. */
    w->fields.v = f->worlds[1u - side]->fields.v;
    const mc_world_storage_status dry_status =
        mc_world_runtime_rain_at(w, 8, 8, 8, &dry, &dry_error);
    const mc_world_storage_status area_status =
        mc_world_runtime_area_loaded(w, 8, -2, 8, 8, -1, 8, &loaded, &area_error);
    const unsigned int mxcsr = _mm_getcsr();
    _mm_setcsr(mxcsr | 0x40u);
    const mc_world_storage_status sound_status =
        mc_world_runtime_entity_sound(w, NULL, NULL, 0, 0, &sound_error);
    _mm_setcsr(mxcsr);
    const mc_world_storage_status sky_status =
        mc_world_runtime_sky_visible(w, 8, 8, 8, &sky, &sky_error);
    const mc_world_storage_status height_status =
        mc_world_runtime_top_solid_liquid(w, 8, 8, &height, &height_error);
    w->fields.v = original;
    CHECK(dry_status == MC_WORLD_STORAGE_OK && !dry && area_status == MC_WORLD_STORAGE_OK &&
          !loaded && sound_status == MC_WORLD_STORAGE_OK &&
          dry_error.status == MC_WORLD_STORAGE_OK && area_error.status == MC_WORLD_STORAGE_OK &&
          sound_error.status == MC_WORLD_STORAGE_OK);
    CHECK(sky_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && sky &&
          sky_error.provider_bytecode == 6 && height_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          height == 37 && height_error.provider_bytecode == 6 && f->states[side] == state &&
          f->providers[side].cache->count == count &&
          f->providers[side].cache->insertion_count == insertions &&
          mc_world_store_chunk_count(f->stores[side]) == 1 && w->active_calls == 0 &&
          w->local_leases == 0);
    ok = true;
cleanup:
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    mc_entity *bound[2][2] = {{NULL}}, *unbound[2][2] = {{NULL}};
    char root[8192];
    const uint16_t title[] = {'M', 'o', 'v', 'e'};
    if (argc != 3 || strcmp(argv[1], "world-entity-movement") != 0 ||
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
        r->streams[1] = r->streams[2] = (stream){.bytes = language};
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
        CHECK(boxes_open_store(f, side, root) && boxes_create_world(f, side) &&
              movement_capture_region(f, side));
    }
    for (size_t side = 0; side < 2; ++side)
        for (size_t kind = 0; kind < 2; ++kind)
            CHECK(movement_make(f, side, f->worlds[side], kind != 0, &bound[side][kind]) &&
                  movement_make(f, side, NULL, kind != 0, &unbound[side][kind]));
    for (size_t side = 0; side < 2; ++side) {
        CHECK(movement_provider_binding(f, side));
        for (size_t kind = 0; kind < 2; ++kind) {
            mc_entity *e = bound[side][kind];
            const mc_entity_bounds *const permanent = e->bounds;
            CHECK(movement_axes(f, side, e) && movement_steps(f, side, e) &&
                  movement_ledge(f, side, e) && movement_web(f, side, e) &&
                  movement_prefixes(f, side, e, bound[side][1u - kind]) &&
                  movement_contacts(f, side, e, kind == 0) &&
                  movement_fire(f, side, e, kind == 0) &&
                  movement_null_world(f, side, unbound[side][kind]) &&
                  movement_effects(unbound[side][kind], unbound[side][1u - kind], kind == 0));
            CHECK(e->bounds == permanent && e->phase == MC_ENTITY_READY &&
                  e->base.world == f->worlds[side] && movement_quiet(f, side, e, 0));
        }
        CHECK(files_unchanged(f, side));
    }
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("Entity movement cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    if (result == 0 &&
        printf("PASS: %u Entity movement checks (world-entity-movement)\n", checks) < 0)
        result = 1;
    return result;
}
