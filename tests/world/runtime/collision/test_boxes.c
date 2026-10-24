/* Reuse real GAME Registry/World/Store/Chunk constructors and cleanup.
 * Compile this translation unit alone; the imported main is never executed. */
#define main mc_boxes_publication_fixture_main
#include "../test_entity_publication.c"
#undef main

#include "../../../../src/entity/bounds/bounds_internal.h"
#include "../../../../src/world/chunk/object_state_internal.h"
#include "../../../../src/world/runtime/collision/blocks_internal.h"
#include "../../../../src/world/runtime/collision/boxes_internal.h"
#include "mc/world/runtime/chunks.h"

/* Constructed native contracts, not original-JAR replay. Actual loaded Chunk
 * bytes supply fixture cells. Nullable source projections and native ownership
 * fault controls are temporary and restored before assertions can branch. */
static size_t boxes_first_capacity_bytes[2];

static bool boxes_open_store(fixture *f, size_t slot, const char *root) {
    bool ok = false;
    char path[8192];
    uint8_t map[256];
    const uint16_t title[] = {'P', 'u', 'b', 'l', 'i', 's', 'h'};
    const int n = snprintf(path, sizeof(path), "%s/world%zu", root, slot);
    CHECK(n > 0 && (size_t)n < sizeof(path));
    for (size_t i = 0; i < COUNT(map); ++i)
        map[i] = (uint8_t)i;
    mc_world_store_options options = {.session_token = 137,
                                      .block_map = map,
                                      .block_map_length = sizeof(map),
                                      .limits = mc_world_store_default_limits()};
    options.limits.max_loaded_chunks = 4;
    if (slot >= 6)
        options.limits.nbt.max_allocation_bytes = 256u * 1024u;
    CHECK(mc_world_store_open(path, f->regions, &options, &f->stores[slot], NULL) ==
          MC_WORLD_STORAGE_OK);
    const mc_level_info info = {.seed = 23,
                                .spawn_y = 64,
                                .time = 0,
                                .level_name = {title, COUNT(title)},
                                .version = 19132};
    CHECK(mc_world_store_save_info(f->stores[slot], &info, 123, NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    return ok;
}

static bool boxes_create_world(fixture *f, size_t slot) {
    bool ok = false;
    mc_world_runtime_construction state = {0};
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 4;
    limits.max_scheduled_entries = 0;
    if (slot == 4 || slot == 5)
        limits.max_direct_allocation_bytes =
            f->baseline_world_bytes[slot % 2u] + boxes_first_capacity_bytes[slot % 2u];
    CHECK(mc_world_provider_create(0, NULL, &f->dimensions[slot], NULL) == MC_WORLD_PROVIDER_OK);
    CHECK(mc_world_runtime_allocate_world(slot % 2u == 0 ? MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE
                                                         : MC_WORLD_RUNTIME_SERVER_LOAD_OR_CREATE,
                                          f->classes[slot % 2u], &limits, &f->worlds[slot],
                                          NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments arguments = {
        .store = f->stores[slot], .name = f->name, .seed = -99, .dimension = f->dimensions[slot]};
    CHECK(mc_world_runtime_construct_world(f->worlds[slot], &arguments, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_construction_get(f->worlds[slot], &state) == MC_WORLD_STORAGE_OK &&
          state.phase == MC_WORLD_RUNTIME_READY && state.contexts_bound && state.cache_registered);
    if (slot < 2)
        f->baseline_world_bytes[slot] = state.direct_allocation_bytes;
    if (slot == 4 || slot == 5)
        CHECK(state.direct_allocation_bytes == f->baseline_world_bytes[slot % 2u]);
    CHECK(mc_chunk_provider_bind_fields(f->worlds[slot]->fields.v, &f->providers[slot], NULL) ==
          MC_WORLD_STORAGE_OK);
    f->bound[slot] = true;
    CHECK(seed_record(f, slot));
    mc_chunk_state *loaded = NULL;
    const mc_chunk_acquire_status acquired = mc_world_runtime_get(f->worlds[slot], 0, 0, &loaded);
    /* Record a genuine published owner even if a later activation failed. */
    CHECK(mc_chunk_cache_validate(f->providers[slot].cache));
    if (f->providers[slot].cache->insertion_count == 1)
        f->states[slot] = f->providers[slot].cache->insertions[0];
    CHECK(acquired == MC_CHUNK_ACQUIRE_OK && loaded != NULL && loaded == f->states[slot] &&
          loaded->active && loaded->terrain_populated && !loaded->dirty && loaded->x == 0 &&
          loaded->z == 0);
    f->chunks[slot] = mc_world_store_chunk_object(f->stores[slot], loaded);
    mc_world_chunk_fields fields = {0};
    CHECK(f->chunks[slot] != NULL &&
          mc_world_chunk_fields_get(f->chunks[slot], &fields) == MC_WORLD_STORAGE_OK &&
          fields.d == f->worlds[slot] && !fields.q && f->providers[slot].cache->count == 1 &&
          f->providers[slot].cache->insertion_count == 1 &&
          mc_world_store_chunk_count(f->stores[slot]) == 1);
    bool distinct = true;
    for (size_t i = 0; i < 8; ++i) {
        CHECK(mc_world_chunk_slice_at(fields.m, i, &f->slices[slot][i]) == MC_WORLD_STORAGE_OK &&
              f->slices[slot][i] != NULL);
        for (size_t j = 0; j < i; ++j)
            if (f->slices[slot][i] == f->slices[slot][j])
                distinct = false;
    }
    CHECK(distinct);
    f->world_lists[slot] = f->worlds[slot]->fields.b;
    CHECK(f->world_lists[slot] != NULL && f->world_lists[slot]->count == 0);
    f->initial_chunk_bytes[slot] = mc_world_chunk_allocation_bytes(f->chunks[slot]);
    ok = true;
cleanup:
    return ok;
}

static size_t boxes_index(int32_t x, int32_t y, int32_t z) {
    return ((size_t)((uint32_t)x & 15u) << 11u) | ((size_t)((uint32_t)z & 15u) << 7u) |
           ((size_t)(uint32_t)y & 127u);
}

static bool boxes_cell(fixture *f, size_t slot, int32_t x, int32_t y, int32_t z, uint8_t id,
                       int32_t metadata) {
    f->states[slot]->data->blocks[boxes_index(x, y, z)] = id;
    bool changed = false;
    return mc_world_runtime_metadata_set_quiet(f->worlds[slot], x, y, z, metadata, &changed,
                                               NULL) == MC_WORLD_STORAGE_OK &&
           changed;
}

static void boxes_air(fixture *f, size_t slot) {
    memset(f->states[slot]->data->blocks, 0, MC_CHUNK_BLOCKS);
}

static bool boxes_coordinates(const mc_entity_bounds *actual, const mc_entity_bounds *expected) {
    return actual != NULL && memcmp(actual, expected, sizeof(*actual)) == 0;
}

static bool boxes_view_is(const mc_world_collision_list_view *view,
                          const mc_entity_bounds *expected, size_t count) {
    size_t actual_count = SIZE_MAX;
    if (mc_world_collision_list_view_count(view, &actual_count) != MC_WORLD_STORAGE_OK ||
        actual_count != count)
        return false;
    for (size_t i = 0; i < count; ++i) {
        const mc_entity_bounds *actual = NULL;
        if (mc_world_collision_list_view_get(view, i, &actual) != MC_WORLD_STORAGE_OK ||
            !boxes_coordinates(actual, &expected[i]))
            return false;
    }
    return true;
}

static bool boxes_query_is(mc_world_runtime *world, mc_entity *moving,
                           const mc_entity_bounds *query, const mc_entity_bounds *expected,
                           size_t count) {
    mc_world_collision_list_view view = {0};
    mc_world_collision_error error = mc_world_collision_empty_error();
    const mc_world_storage_status status =
        mc_world_runtime_collision_boxes(world, moving, query, &view, &error);
    const bool result = status == MC_WORLD_STORAGE_OK && error.world_bytecode == 0 &&
                        error.block_bytecode == 0 && boxes_view_is(&view, expected, count);
    mc_world_collision_list_view_release(&view);
    return result;
}

static mc_world_storage_status boxes_append_pool(mc_world_runtime *world,
                                                 mc_world_reference_list *destination,
                                                 mc_entity_bounds *bounds,
                                                 mc_world_collision_error *error) {
    *error = mc_world_collision_empty_error();
    mc_world_storage_status status = mc_world_runtime_call_enter(world, NULL);
    if (status == MC_WORLD_STORAGE_OK) {
        status = mc_world_collision_append_pool_held(world, destination, bounds, error);
        mc_world_runtime_call_leave(world);
    }
    return status;
}

static mc_world_storage_status boxes_append_entity(mc_world_runtime *world,
                                                   mc_world_reference_list *destination,
                                                   mc_entity *entity, mc_entity_bounds *bounds,
                                                   mc_world_collision_error *error) {
    *error = mc_world_collision_empty_error();
    mc_world_storage_status status = mc_world_runtime_call_enter(world, NULL);
    if (status == MC_WORLD_STORAGE_OK) {
        status = mc_world_collision_append_entity_held(world, destination, entity, bounds, error);
        mc_world_runtime_call_leave(world);
    }
    return status;
}

static size_t boxes_detached_count(const mc_entity_classes *classes) {
    size_t count = 0;
    for (const mc_entity_bounds_object *object = classes->bounds_objects; object != NULL;
         object = object->next)
        if (!object->pooled)
            ++count;
    return count;
}

static mc_world_storage_status boxes_contribute(mc_world_runtime *world, const mc_block *block,
                                                int32_t x, int32_t y, int32_t z,
                                                const mc_entity_bounds *query,
                                                mc_world_reference_list *destination,
                                                mc_world_collision_error *error) {
    *error = mc_world_collision_empty_error();
    mc_world_storage_status status = mc_world_runtime_call_enter(world, NULL);
    if (status == MC_WORLD_STORAGE_OK) {
        status = mc_world_collision_block_contribute_held(world, block, x, y, z, query, destination,
                                                          error);
        mc_world_runtime_call_leave(world);
    }
    return status;
}

static bool boxes_direct_is(mc_world_runtime *world, const mc_block *block, int32_t x, int32_t y,
                            int32_t z, const mc_entity_bounds *expected) {
    mc_world_collision_error error = mc_world_collision_empty_error();
    mc_entity_bounds *actual = NULL;
    if (mc_world_runtime_call_enter(world, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    const mc_world_storage_status status =
        mc_world_collision_block_box_held(world, block, x, y, z, &actual, &error);
    const bool result = status == MC_WORLD_STORAGE_OK &&
                        (expected == NULL ? actual == NULL : boxes_coordinates(actual, expected));
    const mc_entity_status released =
        actual == NULL ? MC_ENTITY_OK
                       : mc_entity_bounds_release(mc_world_runtime_entities(world), actual);
    mc_world_runtime_call_leave(world);
    return result && released == MC_ENTITY_OK;
}

static bool boxes_basic(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *const w = f->worlds[side];
    mc_entity_classes *const classes = f->entity_classes[side];
    mc_world_collision_list_view held = {0};
    mc_world_collision_error error;
    const mc_entity_bounds stone = {8.0, 8.0, 8.0, 9.0, 9.0, 9.0};
    const mc_entity_bounds inside = {8.125, 8.125, 8.125, 8.875, 8.875, 8.875};
    CHECK(boxes_cell(f, side, 8, 8, 8, 1, 0) &&
          mc_entity_bounds_pool_reset(classes, NULL) == MC_ENTITY_OK);
    const size_t bytes_before = w->construction.direct_allocation_bytes;
    const size_t holds_before = classes->bounds_holds;
    const mc_java_random random_before = *w->fields.r;
    const mc_entity_class_phase entity_phase = classes->entity_phase;
    const mc_entity_class_phase watcher_phase = classes->watcher_phase;
    const int32_t next_id = classes->next_id;
    mc_world_reference_list *const identity = w->fields.K;
    CHECK(mc_world_runtime_collision_boxes(w, NULL, &inside, &held, &error) ==
              MC_WORLD_STORAGE_OK &&
          boxes_view_is(&held, &stone, 1) && held.identity == identity && held.owner == w &&
          w->local_leases == 1 && classes->bounds_cursor == 2 &&
          classes->bounds_holds == holds_before + 1u && identity->capacity == 4 &&
          w->collision_boxes.count == 1 && w->collision_boxes.entries[0].pool == classes &&
          w->collision_boxes.entries[0].entity == NULL &&
          w->collision_boxes.entries[0].bounds == identity->items[0]);
    boxes_first_capacity_bytes[side] = w->construction.direct_allocation_bytes - bytes_before;
    CHECK(boxes_first_capacity_bytes[side] ==
          4u * (sizeof(void *) + sizeof(mc_world_collision_reference)));
    const size_t bytes = w->construction.direct_allocation_bytes;
    const mc_entity_bounds faces[] = {{7.75, 8.125, 8.125, 8.0, 8.875, 8.875},
                                      {8.125, 7.75, 8.125, 8.875, 8.0, 8.875},
                                      {8.125, 8.125, 7.75, 8.875, 8.875, 8.0}};
    for (size_t i = 0; i < COUNT(faces); ++i) {
        const int32_t cursor = classes->bounds_cursor;
        CHECK(boxes_query_is(w, NULL, &faces[i], NULL, 0) && boxes_view_is(&held, NULL, 0) &&
              classes->bounds_cursor == cursor + 2 && classes->bounds_holds == holds_before &&
              identity->capacity == 4 && w->construction.direct_allocation_bytes == bytes);
    }
    CHECK(boxes_query_is(w, NULL, &inside, &stone, 1));
    mc_world_collision_list_view out = {NULL, identity};
    CHECK(mc_world_runtime_collision_boxes(w, NULL, NULL, &out, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.world_bytecode == 8 && out.owner == NULL && out.identity == identity &&
          boxes_view_is(&held, NULL, 0) && identity->capacity == 4 &&
          classes->bounds_holds == holds_before);
    CHECK(boxes_query_is(w, NULL, &inside, &stone, 1));
    w->fields.K = NULL;
    const mc_world_storage_status null_k =
        mc_world_runtime_collision_boxes(w, NULL, NULL, &out, &error);
    w->fields.K = identity;
    CHECK(null_k == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.world_bytecode == 4 &&
          boxes_view_is(&held, &stone, 1) && classes->bounds_holds == holds_before + 1u);
    mc_world_reference_list foreign = {0};
    w->fields.K = &foreign;
    const mc_world_storage_status foreign_k =
        mc_world_runtime_collision_boxes(w, NULL, &inside, &out, &error);
    const bool captured = boxes_view_is(&held, &stone, 1);
    w->fields.K = identity;
    CHECK(foreign_k == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.world_bytecode == 4 &&
          captured && foreign.count == 0 && foreign.capacity == 0 && foreign.items == NULL);
    CHECK(mc_world_runtime_retire_entity_containers(w, NULL) == MC_WORLD_STORAGE_BUSY &&
          mc_world_runtime_retire_world(w, NULL) == MC_WORLD_STORAGE_BUSY &&
          mc_world_runtime_free(w, NULL) == MC_WORLD_STORAGE_BUSY && !w->entity_containers_retired);
    const mc_entity_bounds *sentinel = &stone;
    size_t sentinel_count = 73;
    CHECK(mc_world_collision_list_view_get(&held, 1, &sentinel) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          sentinel == &stone &&
          mc_world_collision_list_view_count(NULL, &sentinel_count) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          sentinel_count == 73);
    boxes_air(f, side);
    CHECK(boxes_query_is(w, NULL, &inside, NULL, 0) && boxes_view_is(&held, NULL, 0) &&
          classes->bounds_holds == holds_before && w->fields.R->count == 0 &&
          classes->entity_phase == entity_phase && classes->watcher_phase == watcher_phase &&
          classes->next_id == next_id && mc_test_java_random_equal(w->fields.r, &random_before) &&
          mc_world_collision_owned_valid(w));
    ok = true;
cleanup:
    mc_world_collision_list_view_release(&held);
    return ok;
}

static bool boxes_shape(fixture *f, size_t side, uint8_t id, int32_t metadata,
                        const mc_entity_bounds *expected, size_t count) {
    boxes_air(f, side);
    const mc_entity_bounds query = {7.75, 7.75, 7.75, 10.125, 10.125, 10.125};
    return boxes_cell(f, side, 8, 8, 8, id, metadata) &&
           boxes_query_is(f->worlds[side], NULL, &query, expected, count);
}

static bool boxes_registered_shapes(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *const w = f->worlds[side];
    mc_entity_classes *const classes = f->entity_classes[side];
    /* ID36 reaches the real completed absent-TileEntity route; the other entries
     * reach their actual literal-null getter declarations. */
    const uint8_t null_shapes[] = {6,  8,  9,  10, 11, 27, 28, 30, 31, 32, 36, 37, 38, 39, 40,
                                   50, 51, 55, 59, 63, 66, 68, 69, 70, 72, 75, 76, 77, 83, 90};
    for (size_t i = 0; i < COUNT(null_shapes); ++i) {
        CHECK(mc_entity_bounds_pool_reset(classes, NULL) == MC_ENTITY_OK &&
              boxes_shape(f, side, null_shapes[i], 0, NULL, 0) && classes->bounds_cursor == 1);
    }

    const struct {
        uint8_t id;
        mc_entity_bounds bounds;
    } fixed[] = {
        {1, {8.0, 8.0, 8.0, 9.0, 9.0, 9.0}},  {20, {8.0, 8.0, 8.0, 9.0, 9.0, 9.0}},
        {43, {8.0, 8.0, 8.0, 9.0, 9.0, 9.0}}, {44, {8.0, 8.0, 8.0, 9.0, 8.5, 9.0}},
        {60, {8.0, 8.0, 8.0, 9.0, 9.0, 9.0}}, {81, {8.0625, 8.0, 8.0625, 8.9375, 8.9375, 8.9375}},
        {85, {8.0, 8.0, 8.0, 9.0, 9.5, 9.0}}, {88, {8.0, 8.0, 8.0, 9.0, 8.875, 9.0}}};

    for (size_t i = 0; i < COUNT(fixed); ++i) {
        CHECK(mc_entity_bounds_pool_reset(classes, NULL) == MC_ENTITY_OK &&
              boxes_shape(f, side, fixed[i].id, 0, &fixed[i].bounds, 1) &&
              classes->bounds_cursor == 2);
    }
    /* Only the actual floor(minY)-1 start can reach this fence cell below
     * the query. Its real 1.5-high box overlaps the original query above y9. */
    boxes_air(f, side);
    const mc_entity_bounds above_fence = {8.25, 9.25, 8.25, 8.75, 9.375, 8.75};
    const mc_entity_bounds low_fence = {8.0, 8.0, 8.0, 9.0, 9.5, 9.0};
    CHECK(boxes_cell(f, side, 8, 8, 8, 85, 0) &&
          mc_entity_bounds_pool_reset(classes, NULL) == MC_ENTITY_OK &&
          boxes_query_is(w, NULL, &above_fence, &low_fence, 1) && classes->bounds_cursor == 2);
    const mc_entity_bounds slab = {8.0, 8.0, 8.0, 9.0, 8.5, 9.0};
    CHECK(boxes_shape(f, side, 44, 1, &slab, 1));
    for (int32_t metadata = 0; metadata < 8; ++metadata) {
        const mc_entity_bounds snow = {8.0, 8.0, 8.0, 9.0, 8.5, 9.0};
        CHECK(mc_entity_bounds_pool_reset(classes, NULL) == MC_ENTITY_OK &&
              boxes_shape(f, side, 78, metadata, metadata < 3 ? NULL : &snow,
                          metadata < 3 ? 0u : 1u) &&
              classes->bounds_cursor == (metadata < 3 ? 1 : 2));
    }
    for (int32_t metadata = 0; metadata < 16; ++metadata) {
        const mc_entity_bounds cake = {
            8.0 + (double)(1 + 2 * metadata) / 16.0, 8.0, 8.0625, 8.9375, 8.4375, 8.9375};
        CHECK(boxes_shape(f, side, 92, metadata, &cake, 1));
    }
    const mc_entity_bounds ladder[] = {{8.0, 8.0, 8.875, 9.0, 9.0, 9.0},
                                       {8.0, 8.0, 8.0, 9.0, 9.0, 8.125},
                                       {8.875, 8.0, 8.0, 9.0, 9.0, 9.0},
                                       {8.0, 8.0, 8.0, 8.125, 9.0, 9.0}};
    for (int32_t metadata = 2; metadata <= 5; ++metadata)
        CHECK(boxes_shape(f, side, 65, metadata, &ladder[metadata - 2], 1));
    CHECK(boxes_shape(f, side, 65, 7, &ladder[3], 1));
    const mc_entity_bounds door[] = {{8.0, 8.0, 8.0, 9.0, 9.0, 8.1875},
                                     {8.8125, 8.0, 8.0, 9.0, 9.0, 9.0},
                                     {8.0, 8.0, 8.8125, 9.0, 9.0, 9.0},
                                     {8.0, 8.0, 8.0, 8.1875, 9.0, 9.0}};
    const mc_entity_bounds trapdoor[] = {{8.0, 8.0, 8.8125, 9.0, 9.0, 9.0},
                                         {8.0, 8.0, 8.0, 9.0, 9.0, 8.1875},
                                         {8.8125, 8.0, 8.0, 9.0, 9.0, 9.0},
                                         {8.0, 8.0, 8.0, 8.1875, 9.0, 9.0}};
    const mc_entity_bounds closed = {8.0, 8.0, 8.0, 9.0, 8.1875, 9.0};
    for (int32_t metadata = 0; metadata < 16; ++metadata) {
        const size_t orientation = (size_t)(((metadata & 4) == 0 ? metadata - 1 : metadata) & 3);
        CHECK(boxes_shape(f, side, 64, metadata, &door[orientation], 1) &&
              boxes_shape(f, side, 71, metadata, &door[orientation], 1) &&
              boxes_shape(f, side, 96, metadata,
                          (metadata & 4) == 0 ? &closed : &trapdoor[metadata & 3], 1));
    }
    CHECK(f->registry[side]->blocks.m[60]->fields.bw == 0.9375 &&
          f->registry[side]->blocks.m[44]->fields.bw == 0.5 && w->fields.b->count == 0 &&
          w->fields.D->count == 0 && w->fields.R->count == 0 &&
          f->providers[side].cache->count == 1 && f->providers[side].cache->insertion_count == 1);
    boxes_air(f, side);
    ok = true;
cleanup:
    return ok;
}

static bool boxes_full_block(const mc_block *block) {
    const mc_block_fields *const fields = mc_block_fields_get(block);
    return fields != NULL && fields->bs == 0.0 && fields->bt == 0.0 && fields->bu == 0.0 &&
           fields->bv == 1.0 && fields->bw == 1.0 && fields->bx == 1.0;
}

static bool boxes_multipart_and_order(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *const w = f->worlds[side];
    mc_entity_classes *const classes = f->entity_classes[side];
    const mc_entity_bounds stairs[4][2] = {
        {{8.0, 8.0, 8.0, 8.5, 8.5, 9.0}, {8.5, 8.0, 8.0, 9.0, 9.0, 9.0}},
        {{8.0, 8.0, 8.0, 8.5, 9.0, 9.0}, {8.5, 8.0, 8.0, 9.0, 8.5, 9.0}},
        {{8.0, 8.0, 8.0, 9.0, 8.5, 8.5}, {8.0, 8.0, 8.5, 9.0, 9.0, 9.0}},
        {{8.0, 8.0, 8.0, 9.0, 9.0, 8.5}, {8.0, 8.0, 8.5, 9.0, 8.5, 9.0}}};
    for (int32_t metadata = 0; metadata < 6; ++metadata)
        for (size_t type = 0; type < 2; ++type) {
            const uint8_t id = type == 0 ? 53 : 67;
            CHECK(mc_entity_bounds_pool_reset(classes, NULL) == MC_ENTITY_OK &&
                  boxes_shape(f, side, id, metadata, metadata < 4 ? stairs[metadata] : NULL,
                              metadata < 4 ? 2u : 0u) &&
                  classes->bounds_cursor == (metadata < 4 ? 3 : 1) &&
                  boxes_full_block(f->registry[side]->blocks.m[id]));
        }
    const mc_entity_bounds head[6][2] = {
        {{8.0, 8.0, 8.0, 9.0, 8.25, 9.0}, {8.375, 8.25, 8.375, 8.625, 9.0, 8.625}},
        {{8.0, 8.75, 8.0, 9.0, 9.0, 9.0}, {8.375, 8.0, 8.375, 8.625, 8.75, 8.625}},
        {{8.0, 8.0, 8.0, 9.0, 9.0, 8.25}, {8.25, 8.375, 8.25, 8.75, 8.625, 9.0}},
        {{8.0, 8.0, 8.75, 9.0, 9.0, 9.0}, {8.25, 8.375, 8.0, 8.75, 8.625, 8.75}},
        {{8.0, 8.0, 8.0, 8.25, 9.0, 9.0}, {8.375, 8.25, 8.25, 8.625, 8.75, 9.0}},
        {{8.75, 8.0, 8.0, 9.0, 9.0, 9.0}, {8.0, 8.375, 8.25, 8.75, 8.625, 8.75}}};
    for (int32_t metadata = 0; metadata < 16; ++metadata) {
        const int32_t direction = metadata & 7;
        CHECK(mc_entity_bounds_pool_reset(classes, NULL) == MC_ENTITY_OK &&
              boxes_shape(f, side, 34, metadata, direction < 6 ? head[direction] : NULL,
                          direction < 6 ? 2u : 0u) &&
              classes->bounds_cursor == (direction < 6 ? 3 : 1) &&
              boxes_full_block(f->registry[side]->blocks.m[34]));
    }
    const mc_entity_bounds full = {8.0, 8.0, 8.0, 9.0, 9.0, 9.0};
    CHECK(boxes_shape(f, side, 29, 13, &full, 1) && boxes_shape(f, side, 33, 13, &full, 1) &&
          boxes_full_block(f->registry[side]->blocks.m[29]) &&
          boxes_full_block(f->registry[side]->blocks.m[33]));

    boxes_air(f, side);
    const int32_t cells[5][3] = {{7, 8, 8}, {7, 9, 7}, {8, 7, 8}, {8, 8, 7}, {8, 8, 8}};
    for (size_t i = 0; i < COUNT(cells); ++i)
        CHECK(boxes_cell(f, side, cells[i][0], cells[i][1], cells[i][2], 1, 0));
    const mc_entity_bounds expected[] = {{7.0, 9.0, 7.0, 8.0, 10.0, 8.0},
                                         {7.0, 8.0, 8.0, 8.0, 9.0, 9.0},
                                         {8.0, 8.0, 7.0, 9.0, 9.0, 8.0},
                                         {8.0, 7.0, 8.0, 9.0, 8.0, 9.0},
                                         {8.0, 8.0, 8.0, 9.0, 9.0, 9.0}};
    const mc_entity_bounds query = {6.75, 6.75, 6.75, 9.25, 10.25, 9.25};
    CHECK(mc_entity_bounds_pool_reset(classes, NULL) == MC_ENTITY_OK &&
          boxes_query_is(w, NULL, &query, expected, COUNT(expected)) &&
          classes->bounds_cursor == 6 && w->fields.K->count == 5 && w->fields.K->capacity == 8 &&
          mc_world_collision_owned_valid(w));
    for (size_t i = 0; i < COUNT(expected); ++i)
        for (size_t j = 0; j < i; ++j)
            CHECK(w->fields.K->items[i] != w->fields.K->items[j]);
    /* Clear before another World's explicit reset can reuse these slots. */
    mc_world_collision_list_view out = {0};
    mc_world_collision_error error;
    CHECK(mc_world_runtime_collision_boxes(w, NULL, NULL, &out, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.world_bytecode == 8 && w->fields.K->count == 0 && classes->bounds_holds == 0);
    boxes_air(f, side);
    ok = true;
cleanup:
    return ok;
}

static bool boxes_pool_lifetime(fixture *f, size_t side) {
    bool ok = false;
    const size_t slot = side + 2u;
    mc_world_runtime *const w = f->worlds[slot];
    mc_entity_classes *const classes = f->entity_classes[side];
    mc_world_collision_list_view view = {0};
    mc_entity_bounds *reuse = NULL;
    const mc_entity_bounds query = {8.125, 8.125, 8.125, 8.875, 8.875, 8.875};
    const mc_entity_bounds stone = {8.0, 8.0, 8.0, 9.0, 9.0, 9.0};
    mc_world_collision_error error;
    CHECK(boxes_cell(f, slot, 8, 8, 8, 1, 0) &&
          mc_entity_bounds_pool_reset(classes, NULL) == MC_ENTITY_OK &&
          mc_world_runtime_collision_boxes(w, NULL, &query, &view, &error) == MC_WORLD_STORAGE_OK &&
          boxes_view_is(&view, &stone, 1) && classes->bounds_holds == 1);
    const mc_entity_bounds *actual = NULL;
    CHECK(mc_world_collision_list_view_get(&view, 0, &actual) == MC_WORLD_STORAGE_OK &&
          actual == w->collision_boxes.entries[0].bounds &&
          mc_entity_bounds_pool_reset(classes, NULL) == MC_ENTITY_OK &&
          classes->bounds_cursor == 0 &&
          mc_entity_bounds_pool_get(classes, 100.0, 101.0, 102.0, 103.0, 104.0, 105.0, &reuse,
                                    NULL) == MC_ENTITY_OK &&
          reuse == actual && classes->bounds_holds == 2 && classes->bounds_cursor == 1);
    const mc_entity_bounds overwritten = {100.0, 101.0, 102.0, 103.0, 104.0, 105.0};
    CHECK(boxes_view_is(&view, &overwritten, 1) &&
          mc_entity_bounds_release(classes, reuse) == MC_ENTITY_OK);
    reuse = NULL;
    CHECK(classes->bounds_holds == 1);
    if (side == 0) {
        /* The genuine client recovery clear detaches the box. K now owns its
         * sole native hold; the subsequent source K.clear must free it once. */
        CHECK(mc_entity_bounds_pool_clear_client(classes, NULL) == MC_ENTITY_OK &&
              classes->bounds_pool->length == 0 && classes->bounds_cursor == 0 &&
              boxes_detached_count(classes) == 1 && classes->bounds_holds == 1 &&
              boxes_view_is(&view, &overwritten, 1));
    }
    mc_world_collision_list_view out = {0};
    CHECK(mc_world_runtime_collision_boxes(w, NULL, NULL, &out, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.world_bytecode == 8 && boxes_view_is(&view, NULL, 0) &&
          classes->bounds_holds == 0 && boxes_detached_count(classes) == 0 &&
          w->fields.K->capacity == 4 && mc_world_collision_owned_valid(w));
    if (side == 0)
        CHECK(classes->bounds_objects == NULL);
    boxes_air(f, slot);
    ok = true;
cleanup:
    if (reuse != NULL)
        (void)mc_entity_bounds_release(classes, reuse);
    mc_world_collision_list_view_release(&view);
    return ok;
}

static bool boxes_permanent_ownership(fixture *f, size_t side) {
    bool ok = false;
    const size_t slot = side + 2u;
    mc_world_runtime *const w = f->worlds[slot];
    mc_entity_classes *const classes = f->entity_classes[side];
    const size_t live_before = classes->live_entities;
    const size_t bindings_before = w->persistent_bindings;
    const size_t tracked = f->entity_count;
    mc_entity *entity = NULL;
    CHECK(make_entity(f, side, w, 8.0, 8.0, 8.0, &entity) && f->entities[tracked] == entity &&
          entity->holds == 1);
    mc_world_collision_error error;
    /* Native provenance contract only. The implemented Item/Falling virtuals
     * genuinely return NULL; this test does not invent an Entity override. */
    CHECK(boxes_append_entity(w, w->fields.K, entity, entity->bounds, &error) ==
              MC_WORLD_STORAGE_OK &&
          boxes_append_entity(w, w->fields.K, entity, entity->bounds, &error) ==
              MC_WORLD_STORAGE_OK &&
          entity->holds == 3 && w->fields.K->count == 2 &&
          w->fields.K->items[0] == entity->bounds && w->fields.K->items[1] == entity->bounds &&
          w->collision_boxes.entries[0].entity == entity &&
          w->collision_boxes.entries[1].entity == entity &&
          w->collision_boxes.entries[0].pool == NULL &&
          w->collision_boxes.entries[1].pool == NULL && classes->bounds_holds == 0);
    mc_entity_release(entity);
    f->entities[tracked] = NULL;
    entity = NULL;
    CHECK(w->collision_boxes.entries[0].entity->holds == 2 &&
          classes->live_entities == live_before + 1u &&
          w->persistent_bindings == bindings_before + 1u);
    const size_t bytes = w->construction.direct_allocation_bytes;
    mc_world_collision_list_view out = {0};
    /* Releasing the second duplicate destroys the Entity and its real World
     * binding. No dereference of that expired Entity follows this clear. */
    CHECK(mc_world_runtime_collision_boxes(w, NULL, NULL, &out, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.world_bytecode == 8 && w->fields.K->count == 0 &&
          classes->live_entities == live_before && w->persistent_bindings == bindings_before &&
          w->construction.direct_allocation_bytes == bytes && mc_world_collision_owned_valid(w) &&
          w->active_calls == 0 && w->local_leases == 0);
    ok = true;
cleanup:
    return ok;
}

static bool boxes_budget(fixture *f, size_t side) {
    bool ok = false;
    const size_t slot = side + 4u;
    mc_world_runtime *const w = f->worlds[slot];
    mc_entity_classes *const classes = f->entity_classes[side];
    for (int32_t x = 4; x <= 8; ++x)
        CHECK(boxes_cell(f, slot, x, 8, 8, 1, 0));
    const mc_entity_bounds query = {4.125, 8.125, 8.125, 8.875, 8.875, 8.875};
    CHECK(mc_entity_bounds_pool_reset(classes, NULL) == MC_ENTITY_OK);
    const size_t bytes = w->construction.direct_allocation_bytes;
    const size_t holds = classes->bounds_holds;
    mc_world_collision_list_view out = {NULL, w->fields.K};
    mc_world_collision_error error;
    CHECK(mc_world_runtime_collision_boxes(w, NULL, &query, &out, &error) ==
              MC_WORLD_STORAGE_LIMIT &&
          error.world_bytecode == 148 && error.x == 8 && error.y == 8 && error.z == 8 &&
          out.owner == NULL && out.identity == w->fields.K && w->fields.K->count == 4 &&
          w->fields.K->capacity == 4 && w->collision_boxes.count == 4 &&
          classes->bounds_cursor == 5 && classes->bounds_holds == holds + 4u &&
          w->construction.direct_allocation_bytes == bytes + boxes_first_capacity_bytes[side] &&
          w->construction.direct_allocation_bytes ==
              w->construction_limits.max_direct_allocation_bytes);
    for (size_t i = 0; i < 4; ++i) {
        const mc_entity_bounds expected = {(double)(4u + i), 8.0, 8.0, (double)(5u + i), 9.0, 9.0};
        CHECK(boxes_coordinates(w->fields.K->items[i], &expected) &&
              w->collision_boxes.entries[i].pool == classes &&
              w->collision_boxes.entries[i].entity == NULL);
    }
    CHECK(mc_world_runtime_collision_boxes(w, NULL, NULL, &out, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.world_bytecode == 8 && w->fields.K->count == 0 && w->fields.K->capacity == 4 &&
          classes->bounds_holds == holds &&
          w->construction.direct_allocation_bytes ==
              w->construction_limits.max_direct_allocation_bytes);
    boxes_air(f, slot);
    ok = true;
cleanup:
    return ok;
}

static bool boxes_multipart_failure(fixture *f, size_t side) {
    bool ok = false;
    const size_t slot = side + 4u;
    mc_world_runtime *const w = f->worlds[slot];
    mc_entity_classes *const classes = f->entity_classes[side];
    mc_entity_bounds *factory = NULL;
    mc_world_collision_error error;
    const mc_entity_bounds query = {7.75, 7.75, 7.75, 9.25, 9.25, 9.25};
    CHECK(boxes_cell(f, slot, 8, 8, 8, 53, 0) &&
          mc_entity_bounds_pool_reset(classes, NULL) == MC_ENTITY_OK);
    mc_entity_bounds *prefix[3] = {0};
    for (size_t i = 0; i < 3; ++i) {
        CHECK(mc_entity_bounds_pool_get(classes, -(double)(i + 2u), -2.0, -2.0, -(double)(i + 1u),
                                        -1.0, -1.0, &factory, NULL) == MC_ENTITY_OK &&
              boxes_append_pool(w, w->fields.K, factory, &error) == MC_WORLD_STORAGE_OK);
        prefix[i] = factory;
        CHECK(mc_entity_bounds_release(classes, factory) == MC_ENTITY_OK);
        factory = NULL;
    }
    CHECK(w->fields.K->count == 3 && w->fields.K->capacity == 4);
    const size_t bytes = w->construction.direct_allocation_bytes;
    const mc_block *const stairs = f->registry[side]->blocks.m[53];
    CHECK(boxes_contribute(w, stairs, 8, 8, 8, &query, w->fields.K, &error) ==
              MC_WORLD_STORAGE_LIMIT &&
          error.block_bytecode == 30 && error.block_class != NULL &&
          strcmp(error.block_class, side == 0 ? "uu" : "na") == 0 && w->fields.K->count == 4 &&
          w->fields.K->capacity == 4 && w->construction.direct_allocation_bytes == bytes &&
          classes->bounds_cursor == 5 && stairs->fields.bs == 0.5 && stairs->fields.bt == 0.0 &&
          stairs->fields.bu == 0.0 && stairs->fields.bv == 1.0 && stairs->fields.bw == 1.0 &&
          stairs->fields.bx == 1.0 && mc_world_collision_owned_valid(w));
    const mc_entity_bounds first = {8.0, 8.0, 8.0, 8.5, 8.5, 9.0};
    const mc_entity_bounds second = {8.5, 8.0, 8.0, 9.0, 9.0, 9.0};
    CHECK(boxes_coordinates(w->fields.K->items[3], &first) &&
          boxes_direct_is(w, stairs, 8, 8, 8, &second));
    for (size_t i = 0; i < 3; ++i)
        CHECK(w->fields.K->items[i] == prefix[i]);
    mc_world_collision_list_view out = {0};
    CHECK(mc_world_runtime_collision_boxes(w, NULL, NULL, &out, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.world_bytecode == 8 && w->fields.K->count == 0 &&
          boxes_contribute(w, stairs, 8, 8, 8, &query, w->fields.K, &error) ==
              MC_WORLD_STORAGE_OK &&
          w->fields.K->count == 2 && boxes_full_block(stairs));
    CHECK(mc_world_runtime_collision_boxes(w, NULL, NULL, &out, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.world_bytecode == 8 && w->fields.K->count == 0 &&
          w->construction.direct_allocation_bytes == bytes);
    boxes_air(f, slot);
    ok = true;
cleanup:
    if (factory != NULL)
        (void)mc_entity_bounds_release(classes, factory);
    return ok;
}

static bool boxes_direct_numeric_and_null_order(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *const w = f->worlds[side];
    mc_entity_classes *const classes = f->entity_classes[side];
    const mc_block *const stone = f->registry[side]->blocks.m[1];
    const mc_entity_bounds base = {2147483647.0, 8.0, 2147483647.0,
                                   2147483648.0, 9.0, 2147483648.0};
    const mc_entity_bounds fence = {2147483647.0,  16777217.0, 2147483647.0,
                                    -2147483648.0, 16777218.0, -2147483648.0};
    const mc_entity_bounds cactus = {16777216.0, 2147483647.0,  2147483648.0,
                                     16777218.0, -2147483648.0, -2147483648.0};
    const mc_entity_bounds soul = {1.0, 2147483647.0, 1.0, 2.0, -2147483648.0, 2.0};
    const mc_entity_bounds farm = {2147483647.0,  8.0, 2147483647.0,
                                   -2147483648.0, 9.0, -2147483648.0};
    CHECK(boxes_direct_is(w, stone, INT32_MAX, 8, INT32_MAX, &base) &&
          boxes_direct_is(w, f->registry[side]->blocks.m[85], INT32_MAX, 16777217, INT32_MAX,
                          &fence) &&
          boxes_direct_is(w, f->registry[side]->blocks.m[81], 16777217, INT32_MAX, INT32_MAX,
                          &cactus) &&
          boxes_direct_is(w, f->registry[side]->blocks.m[88], 1, INT32_MAX, 1, &soul) &&
          boxes_direct_is(w, f->registry[side]->blocks.m[60], INT32_MAX, 8, INT32_MAX, &farm));
    const mc_entity_bounds full = {8.0, 8.0, 8.0, 9.0, 9.0, 9.0};
    CHECK(boxes_direct_is(w, f->registry[side]->blocks.m[53], 8, 8, 8, &full) &&
          boxes_direct_is(w, f->registry[side]->blocks.m[34], 8, 8, 8, &full));
    mc_world_collision_error error;
    const int32_t cursor = classes->bounds_cursor;
    CHECK(boxes_contribute(w, f->registry[side]->blocks.m[37], 8, 8, 8, NULL, NULL, &error) ==
              MC_WORLD_STORAGE_OK &&
          classes->bounds_cursor == cursor);
    CHECK(boxes_contribute(w, stone, 8, 8, 8, NULL, NULL, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.block_bytecode == 20 && classes->bounds_cursor == cursor + 1 &&
          w->fields.K->count == 0);
    const mc_entity_bounds miss = {20.0, 20.0, 20.0, 21.0, 21.0, 21.0};
    CHECK(boxes_contribute(w, stone, 8, 8, 8, &miss, NULL, &error) == MC_WORLD_STORAGE_OK &&
          classes->bounds_cursor == cursor + 2 && w->fields.K->count == 0);
    CHECK(boxes_contribute(w, stone, 8, 8, 8, &full, NULL, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.block_bytecode == 30 && classes->bounds_cursor == cursor + 3 &&
          w->fields.K->count == 0 && classes->bounds_holds == 0 &&
          mc_world_collision_owned_valid(w));
    ok = true;
cleanup:
    return ok;
}

static bool boxes_native_append_failure(fixture *f, size_t side) {
    bool ok = false;
    const size_t slot = side + 6u;
    mc_world_runtime *const w = f->worlds[slot];
    mc_entity_classes *const classes = f->entity_classes[side];
    mc_entity_bounds *box = NULL;
    mc_world_collision_error error;
    CHECK(mc_entity_bounds_pool_get(classes, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, &box, NULL) ==
          MC_ENTITY_OK);
    for (size_t i = 0; i < 4; ++i)
        CHECK(boxes_append_pool(w, w->fields.K, box, &error) == MC_WORLD_STORAGE_OK);
    CHECK(w->fields.K->count == 4 && w->fields.K->capacity == 4 && classes->bounds_holds == 5);
    mc_entity_bounds_object *const object = (mc_entity_bounds_object *)box;
    const size_t object_holds = object->holds;
    const size_t class_holds = classes->bounds_holds;
    const size_t class_bytes = classes->allocated_bytes;
    const size_t world_bytes = w->construction.direct_allocation_bytes;
    void **const items = w->fields.K->items;
    mc_world_collision_reference *const entries = w->collision_boxes.entries;
    const size_t growth_bytes = 8u * (sizeof(void *) + sizeof(mc_world_collision_reference));
    CHECK(world_bytes <= w->construction_limits.max_direct_allocation_bytes &&
          growth_bytes <= w->construction_limits.max_direct_allocation_bytes - world_bytes);
    /* Impossible native ownership fault, not a gameplay route or original
     * OOM observation. The failed retain follows real overlapping growth;
     * restore the actual wrapper before any assertion or owner release. */
    object->holds = SIZE_MAX;
    const mc_world_storage_status failed = boxes_append_pool(w, w->fields.K, box, &error);
    object->holds = object_holds;
    CHECK(failed == MC_WORLD_STORAGE_LIMIT && error.nearby.entity.status == MC_ENTITY_LIMIT &&
          w->fields.K->count == 4 && w->fields.K->capacity == 4 && w->fields.K->items == items &&
          w->collision_boxes.entries == entries && w->collision_boxes.count == 4 &&
          w->collision_boxes.capacity == 4 &&
          w->construction.direct_allocation_bytes == world_bytes &&
          classes->bounds_holds == class_holds && classes->allocated_bytes == class_bytes &&
          object->holds == object_holds && mc_world_collision_owned_valid(w));
    for (size_t i = 0; i < 4; ++i)
        CHECK(w->fields.K->items[i] == box && entries[i].bounds == box &&
              entries[i].pool == classes && entries[i].entity == NULL);
    mc_world_reference_list foreign = {0};
    CHECK(boxes_append_pool(w, &foreign, box, &error) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          foreign.count == 0 && classes->bounds_holds == class_holds);
    CHECK(mc_entity_bounds_release(classes, box) == MC_ENTITY_OK);
    box = NULL;
    mc_world_collision_list_view out = {0};
    CHECK(mc_world_runtime_collision_boxes(w, NULL, NULL, &out, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.world_bytecode == 8 && w->fields.K->count == 0 && classes->bounds_holds == 0 &&
          w->construction.direct_allocation_bytes == world_bytes &&
          mc_world_collision_owned_valid(w));
    ok = true;
cleanup:
    if (box != NULL)
        (void)mc_entity_bounds_release(classes, box);
    return ok;
}

static bool boxes_real_entity_descendants(fixture *f, size_t side) {
    bool ok = false;
    const size_t slot = side + 6u;
    mc_world_runtime *const w = f->worlds[slot];
    mc_entity *candidate = NULL, *moving = NULL;
    mc_world_collision_list_view held = {0};
    CHECK(make_entity(f, side, w, 8.0, 8.0, 8.0, &candidate) &&
          make_entity(f, side, w, 8.5, 8.0, 8.5, &moving) &&
          mc_world_runtime_chunk_add_entity(f->chunks[slot], candidate, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_chunk_add_entity(f->chunks[slot], candidate, NULL) ==
              MC_WORLD_STORAGE_OK &&
          boxes_cell(f, slot, 8, 8, 8, 1, 0));
    const mc_entity_bounds query = {7.75, 7.75, 7.75, 9.25, 9.25, 9.25};
    const mc_entity_bounds stone = {8.0, 8.0, 8.0, 9.0, 9.0, 9.0};
    mc_world_collision_error error;
    CHECK(mc_world_runtime_collision_boxes(w, moving, &query, &held, &error) ==
              MC_WORLD_STORAGE_OK &&
          boxes_view_is(&held, &stone, 1) && w->fields.R->count == 2 &&
          w->fields.R->items[0] == candidate && w->fields.R->items[1] == candidate &&
          candidate->holds == 5 && moving->holds == 1 &&
          w->collision_boxes.entries[0].pool == f->entity_classes[side] &&
          w->collision_boxes.entries[0].entity == NULL);
    /* Actual candidate virtual returns NULL, but the separate pair call still
     * reaches its NULL moving receiver after the Block prefix and R gather. */
    mc_world_collision_list_view out = {NULL, held.identity};
    CHECK(mc_world_runtime_collision_boxes(w, NULL, &query, &out, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.world_bytecode == 260 && out.owner == NULL && out.identity == held.identity &&
          boxes_view_is(&held, &stone, 1) && w->fields.R->count == 2 && candidate->holds == 5 &&
          moving->holds == 1);
    CHECK(boxes_query_is(w, candidate, &query, &stone, 1) && w->fields.R->count == 0 &&
          candidate->holds == 3 && moving->holds == 1 && boxes_view_is(&held, &stone, 1));
    boxes_air(f, slot);
    CHECK(boxes_query_is(w, candidate, &query, NULL, 0) && w->fields.R->count == 0 &&
          boxes_view_is(&held, NULL, 0) && w->active_calls == 0 && w->local_leases == 1);
    ok = true;
cleanup:
    mc_world_collision_list_view_release(&held);
    return ok;
}

static int boxes_foreign_value = 97;
static void *boxes_foreign_items[] = {&boxes_foreign_value};
static mc_world_reference_list boxes_foreign = {boxes_foreign_items, 1, 1};

static bool boxes_terminal_seed(fixture *f, size_t side) {
    bool ok = false;
    const size_t slot = side + 2u;
    mc_world_runtime *const w = f->worlds[slot];
    mc_entity_classes *const classes = f->entity_classes[side];
    mc_entity_bounds *factory = NULL;
    mc_entity *entity = NULL;
    mc_world_collision_error error;
    const size_t tracked = f->entity_count;
    CHECK(w->fields.K->count == 0 && classes->bounds_holds == 0 &&
          mc_entity_bounds_pool_get(classes, -7.0, -6.0, -5.0, -4.0, -3.0, -2.0, &factory, NULL) ==
              MC_ENTITY_OK &&
          boxes_append_pool(w, w->fields.K, factory, &error) == MC_WORLD_STORAGE_OK &&
          boxes_append_pool(w, w->fields.K, factory, &error) == MC_WORLD_STORAGE_OK &&
          make_entity(f, side, w, 8.0, 8.0, 8.0, &entity) && f->entities[tracked] == entity &&
          boxes_append_entity(w, w->fields.K, entity, entity->bounds, &error) ==
              MC_WORLD_STORAGE_OK &&
          boxes_append_entity(w, w->fields.K, entity, entity->bounds, &error) ==
              MC_WORLD_STORAGE_OK);
    CHECK(mc_entity_bounds_release(classes, factory) == MC_ENTITY_OK);
    factory = NULL;
    mc_entity_release(entity);
    f->entities[tracked] = NULL;
    entity = NULL;
    /* Both real owners now survive solely through two duplicate K occurrence
     * holds apiece. No concrete Entity virtual is invented by this native test. */
    CHECK(w->fields.K->count == 4 && w->fields.K->capacity == 4 &&
          w->collision_boxes.entries[0].pool == classes &&
          w->collision_boxes.entries[1].pool == classes &&
          w->collision_boxes.entries[0].bounds == w->collision_boxes.entries[1].bounds &&
          w->collision_boxes.entries[0].entity == NULL &&
          w->collision_boxes.entries[1].entity == NULL &&
          w->collision_boxes.entries[2].entity == w->collision_boxes.entries[3].entity &&
          w->collision_boxes.entries[2].entity != NULL &&
          w->collision_boxes.entries[2].entity->holds == 2 &&
          w->collision_boxes.entries[2].entity->base.world == w &&
          w->collision_boxes.entries[2].pool == NULL &&
          w->collision_boxes.entries[3].pool == NULL && classes->bounds_holds == 2 &&
          mc_world_collision_owned_valid(w));
    if (side == 0)
        CHECK(mc_entity_bounds_pool_clear_client(classes, NULL) == MC_ENTITY_OK &&
              boxes_detached_count(classes) == 1 && classes->bounds_holds == 2 &&
              mc_world_collision_owned_valid(w));
    ok = true;
cleanup:
    if (factory != NULL)
        (void)mc_entity_bounds_release(classes, factory);
    return ok;
}

static bool boxes_terminal(fixture *f) {
    bool ok = false;
    for (size_t slot = 0; slot < WORLD_COUNT; ++slot) {
        mc_world_runtime *const w = f->worlds[slot];
        mc_world_reference_list *const original = w->collision_boxes.list;
        const size_t bytes = w->construction.direct_allocation_bytes;
        const size_t collision_backing =
            original->capacity * (sizeof(void *) + sizeof(mc_world_collision_reference));
        const size_t nearby_backing =
            w->nearby_entities.capacity * (sizeof(void *) + sizeof(mc_entity *));
        const bool live_owners = slot == 2 || slot == 3;
        mc_entity_classes *const classes = f->entity_classes[slot % 2u];
        const size_t pool_holds = classes->bounds_holds;
        const size_t live_entities = classes->live_entities;
        const size_t world_bindings = w->persistent_bindings;
        CHECK(original->count == (live_owners ? 4u : 0u) &&
              (!live_owners || (pool_holds >= 2 && live_entities != 0 && world_bindings != 0)));
        w->fields.K = &boxes_foreign;
        const mc_world_storage_status retired = mc_world_runtime_retire_entity_containers(w, NULL);
        if (retired == MC_WORLD_STORAGE_OK)
            f->terminal[slot] = true;
        CHECK(retired == MC_WORLD_STORAGE_OK && w->entity_containers_retired &&
              w->fields.K == &boxes_foreign && w->collision_boxes.list == original &&
              original->count == 0 && original->capacity == 0 && original->items == NULL &&
              w->collision_boxes.entries == NULL && w->collision_boxes.count == 0 &&
              w->collision_boxes.capacity == 0 &&
              w->construction.direct_allocation_bytes ==
                  bytes - collision_backing - nearby_backing &&
              classes->bounds_holds == pool_holds - (live_owners ? 2u : 0u) &&
              classes->live_entities == live_entities - (live_owners ? 1u : 0u) &&
              w->persistent_bindings == world_bindings - (live_owners ? 1u : 0u) &&
              (!live_owners || slot % 2u == 1 || boxes_detached_count(classes) == 0) &&
              mc_world_collision_owned_valid(w));
        CHECK(mc_world_runtime_retire_entity_containers(w, NULL) == MC_WORLD_STORAGE_OK &&
              w->construction.direct_allocation_bytes ==
                  bytes - collision_backing - nearby_backing &&
              boxes_foreign.count == 1 && boxes_foreign.capacity == 1 &&
              boxes_foreign.items == boxes_foreign_items && boxes_foreign_value == 97);
    }
    for (size_t i = 0; i < f->entity_count; ++i)
        CHECK(f->entities[i] == NULL || f->entities[i]->holds == 1);
    CHECK(f->entity_classes[0]->bounds_holds == 0 && f->entity_classes[1]->bounds_holds == 0);
    ok = true;
cleanup:
    return ok;
}

#ifndef MC_COLLISION_BOXES_MAIN
#define MC_COLLISION_BOXES_MAIN main
#endif
int MC_COLLISION_BOXES_MAIN(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    const uint16_t title[] = {'C', 'o', 'l', 'l', 'i', 's', 'i', 'o', 'n'};
    if (argc != 3 || strcmp(argv[1], "world-collision-boxes") != 0 ||
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
    }
    for (size_t slot = 0; slot < WORLD_COUNT; ++slot)
        if (slot != 4 && slot != 5)
            CHECK(boxes_open_store(f, slot, root) && boxes_create_world(f, slot));
    for (size_t side = 0; side < 2; ++side)
        CHECK(boxes_basic(f, side) && boxes_registered_shapes(f, side) &&
              boxes_multipart_and_order(f, side) && boxes_direct_numeric_and_null_order(f, side) &&
              boxes_pool_lifetime(f, side) && boxes_permanent_ownership(f, side));
    for (size_t slot = 4; slot <= 5; ++slot)
        CHECK(boxes_open_store(f, slot, root) && boxes_create_world(f, slot));
    for (size_t side = 0; side < 2; ++side)
        CHECK(boxes_budget(f, side) && boxes_multipart_failure(f, side) &&
              boxes_native_append_failure(f, side) && boxes_real_entity_descendants(f, side));
    for (size_t side = 0; side < 2; ++side)
        CHECK(boxes_terminal_seed(f, side));
    CHECK(boxes_terminal(f));
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("collision box cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    if (result == 0 && (boxes_foreign.count != 1 || boxes_foreign.capacity != 1 ||
                        boxes_foreign.items != boxes_foreign_items || boxes_foreign_value != 97))
        return 1;
    if (result == 0 &&
        printf("PASS: %u World collision box checks (world-collision-boxes)\n", checks) < 0)
        result = 1;
    return result;
}
