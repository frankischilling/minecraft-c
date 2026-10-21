/* Reuse the actual Registry/World/Store/Chunk fixture, not its old main.
 * Compile this translation unit alone. All queries use the real GAME provider;
 * no alternate backend, gameplay callback or allocator hook is installed. */
#define main mc_nearby_publication_fixture_main
#include "../test_entity_publication.c"
#undef main

#include "../../../../src/world/chunk/object_state_internal.h"
#include "../../../../src/world/runtime/collision/entities_internal.h"
#include "mc/world/runtime/chunks.h"

/* These are constructed native contracts, not original-JAR replay results.
 * Temporary nullable source-field controls and one explicitly labeled native
 * hold-overflow control are restored before CHECK may leave their scope. */
static mc_chunk_state *nearby_extra[2][3];
static mc_world_chunk *nearby_extra_chunks[2][3];

static bool nearby_open_store(fixture *f, size_t slot, const char *root) {
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

static bool nearby_seed_record(fixture *f, size_t slot, int32_t x, int32_t z) {
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
    r->fields[0].value.int_value = x;
    r->fields[1].value.int_value = z;
    r->fields[2].value.long_value = -123;
    r->fields[8].value.byte_value = 1;
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
    CHECK(mc_world_regions_write(f->regions, mc_world_store_root(f->stores[slot]), x, z, encoded,
                                 length, fixture_timestamp, &growth, NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    free(encoded);
    free(r);
    return ok;
}

static bool nearby_create_world(fixture *f, size_t slot) {
    bool ok = false;
    mc_world_runtime_construction state = {0};
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 4;
    limits.max_scheduled_entries = 0;
    if (slot == 4 || slot == 5)
        limits.max_direct_allocation_bytes =
            f->baseline_world_bytes[slot % 2u] + 4u * (sizeof(void *) + sizeof(mc_entity *));
    CHECK(mc_world_dimension_create(0, NULL, &f->dimensions[slot], NULL) == MC_WORLD_DIMENSION_OK);
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
    CHECK(mc_world_provider_bind_fields(f->worlds[slot]->fields.v, &f->providers[slot], NULL) ==
          MC_WORLD_STORAGE_OK);
    f->bound[slot] = true;
    CHECK(nearby_seed_record(f, slot, 0, 0));
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

static bool nearby_view_is(const mc_world_entity_list_view *view, mc_entity *const *expected,
                           size_t count) {
    size_t actual = SIZE_MAX;
    if (mc_world_entity_list_view_count(view, &actual) != MC_WORLD_STORAGE_OK || actual != count)
        return false;
    for (size_t i = 0; i < count; ++i) {
        mc_entity *entity = NULL;
        if (mc_world_entity_list_view_get(view, i, &entity) != MC_WORLD_STORAGE_OK ||
            entity != expected[i])
            return false;
    }
    return true;
}

static bool nearby_query_is(mc_world_runtime *world, mc_entity *excluded,
                            const mc_entity_bounds *bounds, mc_entity *const *expected,
                            size_t count) {
    mc_world_entity_list_view view = {0};
    mc_world_nearby_error error = mc_world_nearby_empty_error();
    const mc_world_storage_status status =
        mc_world_runtime_nearby_entities(world, excluded, bounds, &view, &error);
    const bool result = status == MC_WORLD_STORAGE_OK && error.query.bytecode == 0 &&
                        error.query.chunk_bytecode == 0 && error.slice == SIZE_MAX &&
                        error.index == SIZE_MAX && error.entity.status == MC_ENTITY_OK &&
                        nearby_view_is(&view, expected, count);
    mc_world_entity_list_view_release(&view);
    return result;
}

static mc_world_storage_status nearby_gather(mc_world_runtime *world, mc_chunk_state *state,
                                             mc_entity *excluded, const mc_entity_bounds *bounds,
                                             mc_world_reference_list *destination,
                                             mc_world_nearby_error *error) {
    *error = mc_world_nearby_empty_error();
    mc_world_storage_status status = mc_world_runtime_call_enter(world, NULL);
    if (status == MC_WORLD_STORAGE_OK) {
        status = mc_world_nearby_gather_held(world, state, excluded, bounds, destination, error);
        mc_world_runtime_call_leave(world);
    }
    return status;
}

static double nearby_double(uint64_t bits) {
    double result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

static bool nearby_falling(fixture *f, size_t side, mc_world_runtime *world, double x, double y,
                           double z, mc_entity **out) {
    bool ok = false;
    mc_entity *entity = NULL;
    CHECK(f->entity_count < COUNT(f->entities) &&
          mc_falling_entity_allocate(f->entity_classes[side], &entity, NULL) == MC_ENTITY_OK);
    f->entities[f->entity_count++] = entity;
    CHECK(mc_falling_entity_construct(entity, world, x, y, z, 12, NULL) == MC_ENTITY_OK);
    *out = entity;
    ok = true;
cleanup:
    return ok;
}

static bool nearby_neighbors(fixture *f, size_t side) {
    bool ok = false;
    const int32_t coordinates[3][2] = {{-1, 0}, {0, -1}, {1, 0}};
    for (size_t i = 0; i < COUNT(coordinates); ++i) {
        CHECK(nearby_seed_record(f, side, coordinates[i][0], coordinates[i][1]));
        mc_chunk_state *state = NULL;
        CHECK(mc_world_runtime_get(f->worlds[side], coordinates[i][0], coordinates[i][1], &state) ==
                  MC_CHUNK_ACQUIRE_OK &&
              state != NULL);
        nearby_extra[side][i] = state;
        nearby_extra_chunks[side][i] = mc_world_store_chunk_object(f->stores[side], state);
        CHECK(nearby_extra_chunks[side][i] != NULL && state->active && state->terrain_populated &&
              !state->dirty);
    }
    CHECK(f->providers[side].cache->count == 4 && f->providers[side].cache->insertion_count == 4 &&
          mc_world_store_chunk_count(f->stores[side]) == 4);
    ok = true;
cleanup:
    return ok;
}

static bool nearby_order_and_views(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *w = f->worlds[side];
    mc_world_entity_list_view first = {0}, second = {0};
    mc_entity *a = NULL, *b = NULL, *falling = NULL, *left = NULL, *back = NULL, *right = NULL;
    CHECK(nearby_neighbors(f, side));
    CHECK(make_entity(f, side, w, 8.0, 8.0, 8.0, &a) &&
          make_entity(f, side, f->worlds[side + 2u], 8.5, 8.0, 8.5, &b) &&
          nearby_falling(f, side, NULL, 8.0, 16.0, 8.0, &falling) &&
          make_entity(f, side, w, -8.0, 8.0, 8.0, &left) &&
          make_entity(f, side, w, 8.0, 8.0, -8.0, &back) &&
          make_entity(f, side, w, 24.0, 8.0, 8.0, &right));
    CHECK(mc_world_runtime_chunk_add_entity(f->chunks[side], a, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_chunk_add_entity(f->chunks[side], a, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_chunk_add_entity(f->chunks[side], b, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_chunk_add_entity(f->chunks[side], falling, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_chunk_add_entity(nearby_extra_chunks[side][0], left, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_chunk_add_entity(nearby_extra_chunks[side][1], back, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_chunk_add_entity(nearby_extra_chunks[side][2], right, NULL) ==
              MC_WORLD_STORAGE_OK);
    const mc_entity_bounds broad = {-16.0, 0.0, -16.0, 32.0, 32.0, 16.0};
    const mc_entity_bounds center = {2.0, 0.0, 2.0, 13.0, 15.0, 13.0};
    mc_entity *all[] = {left, back, a, a, b, falling, right};
    mc_entity *without_a[] = {left, back, b, falling, right};
    mc_entity *center_values[] = {a, a, b};
    mc_world_nearby_error error = mc_world_nearby_empty_error();
    const mc_world_reference_list *const identity = w->fields.R;
    const size_t bytes_before = w->construction.direct_allocation_bytes;
    const mc_java_random random_before = *a->random;
    const mc_entity_bounds bounds_before = *a->bounds;
    CHECK(
        mc_world_runtime_nearby_entities(w, NULL, &broad, &first, &error) == MC_WORLD_STORAGE_OK &&
        nearby_view_is(&first, all, COUNT(all)) && first.identity == identity && first.owner == w &&
        w->local_leases == 1 && identity->capacity == 8 && a->holds == 5 && b->holds == 3 &&
        falling->holds == 3 && left->holds == 3 && back->holds == 3 && right->holds == 3);
    const size_t bytes = w->construction.direct_allocation_bytes;
    CHECK(bytes == bytes_before + 8u * (sizeof(void *) + sizeof(mc_entity *)) &&
          mc_world_runtime_nearby_entities(w, a, &broad, &second, &error) == MC_WORLD_STORAGE_OK &&
          second.identity == identity && w->local_leases == 2 &&
          nearby_view_is(&first, without_a, COUNT(without_a)) &&
          nearby_view_is(&second, without_a, COUNT(without_a)) && a->holds == 3 && b->holds == 3 &&
          identity->capacity == 8 && w->construction.direct_allocation_bytes == bytes);
    CHECK(nearby_query_is(w, NULL, &center, center_values, COUNT(center_values)) &&
          nearby_view_is(&first, center_values, COUNT(center_values)) &&
          mc_world_runtime_retire_entity_containers(w, NULL) == MC_WORLD_STORAGE_BUSY &&
          mc_world_runtime_retire_world(w, NULL) == MC_WORLD_STORAGE_BUSY &&
          mc_world_runtime_free(w, NULL) == MC_WORLD_STORAGE_BUSY &&
          !w->entity_containers_retired && w->active_calls == 0 && w->local_leases == 2);

    mc_world_entity_list_view untouched = {NULL, identity};
    CHECK(mc_world_runtime_nearby_entities(w, NULL, NULL, &untouched, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.query.bytecode == 10 && untouched.owner == NULL && untouched.identity == identity &&
          nearby_view_is(&first, NULL, 0) && a->holds == 3 && b->holds == 2 &&
          identity->capacity == 8 && w->construction.direct_allocation_bytes == bytes);
    CHECK(nearby_query_is(w, NULL, &center, center_values, COUNT(center_values)));
    mc_world_reference_list *const actual_r = w->fields.R;
    w->fields.R = NULL;
    const mc_world_storage_status null_r =
        mc_world_runtime_nearby_entities(w, NULL, NULL, &untouched, &error);
    w->fields.R = actual_r;
    CHECK(null_r == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.query.bytecode == 4 &&
          nearby_view_is(&first, center_values, COUNT(center_values)) && a->holds == 5 &&
          b->holds == 3 && w->local_leases == 2);

    /* A real captured destination survives source R replacement. These calls
     * append actual Chunk occurrences without clearing, while held views keep
     * the same descriptor identity through backing-array growth. */
    mc_world_reference_list foreign = {0};
    w->fields.R = &foreign;
    const mc_world_storage_status foreign_clear =
        mc_world_runtime_nearby_entities(w, NULL, &center, &untouched, &error);
    const bool captured_view = nearby_view_is(&first, center_values, COUNT(center_values));
    w->fields.R = actual_r;
    CHECK(foreign_clear == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.query.bytecode == 4 &&
          captured_view && foreign.count == 0 && a->holds == 5 && b->holds == 3);
    w->fields.R = &foreign;
    const mc_world_storage_status captured =
        nearby_gather(w, f->states[side], NULL, &center, actual_r, &error);
    w->fields.R = actual_r;
    CHECK(captured == MC_WORLD_STORAGE_OK && foreign.count == 0 && identity->count == 6 &&
          identity->capacity == 8 && first.identity == identity && a->holds == 7 && b->holds == 4);
    CHECK(nearby_gather(w, f->states[side], NULL, &center, actual_r, &error) ==
              MC_WORLD_STORAGE_OK &&
          identity->count == 9 && identity->capacity == 16 && first.identity == identity &&
          a->holds == 9 && b->holds == 5 &&
          w->construction.direct_allocation_bytes ==
              bytes_before + 16u * (sizeof(void *) + sizeof(mc_entity *)));
    mc_entity *three_times[] = {a, a, b, a, a, b, a, a, b};
    CHECK(nearby_view_is(&first, three_times, COUNT(three_times)) &&
          mc_test_java_random_equal(a->random, &random_before) &&
          memcmp(a->bounds, &bounds_before, sizeof(bounds_before)) == 0 &&
          f->providers[side].cache->count == 4 && f->providers[side].cache->insertion_count == 4 &&
          w->fields.b->count == 0 && w->fields.D->count == 0 && f->states[side]->has_entities &&
          !f->states[side]->dirty);
    size_t sentinel_count = 777;
    mc_entity *sentinel_entity = falling;
    CHECK(mc_world_entity_list_view_get(&first, 9, &sentinel_entity) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          sentinel_entity == falling &&
          mc_world_entity_list_view_get(&first, 0, NULL) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          mc_world_entity_list_view_count(NULL, &sentinel_count) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          sentinel_count == 777);
    ok = true;
cleanup:
    mc_world_entity_list_view_release(&second);
    mc_world_entity_list_view_release(&first);
    return ok;
}

static bool nearby_numeric_and_aliases(fixture *f, size_t side) {
    bool ok = false;
    const size_t slot = side + 2u;
    mc_world_runtime *w = f->worlds[slot];
    mc_entity *a = NULL, *b = NULL, *high = NULL, *above = NULL;
    CHECK(make_entity(f, side, NULL, 8.0, 8.0, 8.0, &a) &&
          make_entity(f, side, w, 8.0, 16.0, 8.0, &b) &&
          nearby_falling(f, side, w, 8.0, 112.0, 8.0, &high) &&
          make_entity(f, side, w, 8.0, 128.0, 8.0, &above));
    CHECK(mc_world_runtime_chunk_add_entity(f->chunks[slot], a, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_chunk_add_entity(f->chunks[slot], b, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_chunk_add_entity(f->chunks[slot], high, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_chunk_add_entity(f->chunks[slot], above, NULL) == MC_WORLD_STORAGE_OK);
    mc_entity *pair[] = {a, b};
    mc_entity *one[] = {a};
    mc_entity *all[] = {a, b, high, above};
    mc_entity_bounds q = {2.0, -100.0, 2.0, 13.0, 200.0, 13.0};
    CHECK(nearby_query_is(w, NULL, &q, all, COUNT(all)));
    CHECK(mc_entity_set_position(a, 8.0, 20.0, 8.0) == MC_ENTITY_OK &&
          mc_entity_set_position(b, 8.0, 20.0, 8.0) == MC_ENTITY_OK);
    q.min_y = 18.0;
    q.max_y = 21.0;
    mc_entity *only_b[] = {b};
    CHECK(nearby_query_is(w, NULL, &q, only_b, COUNT(only_b)));
    q.min_y = nearby_double(UINT64_C(0x4031ffffffffffff)); /* predecessor of 18 */
    CHECK(nearby_query_is(w, NULL, &q, pair, COUNT(pair)));
    CHECK(mc_entity_set_position(a, 8.0, 8.0, 8.0) == MC_ENTITY_OK &&
          mc_entity_set_position(b, 8.0, 8.0, 8.0) == MC_ENTITY_OK);
    q.min_y = 7.0;
    q.max_y = nearby_double(UINT64_C(0x402bfffffffffffe)); /* two ulps below 14 */
    CHECK(nearby_query_is(w, NULL, &q, one, COUNT(one)));
    q.max_y = nearby_double(UINT64_C(0x402bffffffffffff)); /* +2 stays below 16 */
    CHECK(nearby_query_is(w, NULL, &q, one, COUNT(one)));
    q.max_y = 14.0;
    CHECK(nearby_query_is(w, NULL, &q, pair, COUNT(pair)));

    /* Strict touching faces exclude; no normalization or epsilon is applied. */
    q = (mc_entity_bounds){a->bounds->max_x, 7.0, 2.0, 13.0, 14.0, 13.0};
    CHECK(nearby_query_is(w, NULL, &q, NULL, 0));
    q.min_x = nearby_double(UINT64_C(0x40203fffffffffff)); /* predecessor of 8.125 */
    CHECK(nearby_query_is(w, NULL, &q, pair, COUNT(pair)));

    const double nan = nearby_double(UINT64_C(0x7ff8000000000042));
    const double infinity = nearby_double(UINT64_C(0x7ff0000000000000));
    q = (mc_entity_bounds){nan, nan, nan, nan, nan, nan};
    const mc_entity_bounds untouched = q;
    CHECK(nearby_query_is(w, NULL, &q, one, COUNT(one)) && memcmp(&q, &untouched, sizeof(q)) == 0);
    CHECK(nearby_query_is(w, a, &q, NULL, 0));
    q = (mc_entity_bounds){2.0, infinity, 2.0, 13.0, infinity, 13.0};
    CHECK(nearby_query_is(w, NULL, &q, NULL, 0));
    q.min_y = -100.0;
    CHECK(nearby_query_is(w, NULL, &q, all, COUNT(all)));
    CHECK(mc_entity_set_position(a, 0.0, 8.0, 8.0) == MC_ENTITY_OK);
    q = (mc_entity_bounds){-0.0, nan, nan, 0.0, nan, nan};
    CHECK(nearby_query_is(w, NULL, &q, one, COUNT(one)));
    CHECK(mc_entity_set_position(a, 8.0, 8.0, 8.0) == MC_ENTITY_OK);

    /* An inverted outer interval skips the provider, even with an otherwise
     * nonterminating INT_MAX inner endpoint. Never enter an unbounded loop. */
    q = (mc_entity_bounds){34.0, 0.0, 0.0, 0.0, 16.0, infinity};
    mc_world_provider *const provider = w->fields.v;
    w->fields.v = NULL;
    const bool inverted = nearby_query_is(w, NULL, &q, NULL, 0);
    w->fields.v = provider;
    CHECK(inverted);

    /* The private presence call may use another genuine current provider, but
     * a reached acquiring getter must not silently use this World's stale
     * coordinator. Missing coordinates need no acquisition at all. */
    q = (mc_entity_bounds){2.0, 7.0, 2.0, 13.0, 15.0, 13.0};
    mc_world_nearby_error replacement_error;
    mc_world_entity_list_view replacement_out = {0};
    w->fields.v = f->worlds[side]->fields.v;
    const mc_world_storage_status replacement =
        mc_world_runtime_nearby_entities(w, NULL, &q, &replacement_out, &replacement_error);
    w->fields.v = provider;
    CHECK(replacement == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          replacement_error.query.bytecode == 113 &&
          replacement_error.query.provider_bytecode == 6 && replacement_out.owner == NULL &&
          w->fields.R->count == 0);
    q.min_x = 66.0;
    q.max_x = 77.0;
    w->fields.v = f->worlds[side]->fields.v;
    const bool absent_replacement = nearby_query_is(w, NULL, &q, NULL, 0);
    w->fields.v = provider;
    CHECK(absent_replacement);

    /* All 32,767 absent coordinates are nonloading. The two real present
     * coordinates 0 and 32768 have the same source packed key and acquire the
     * same actual Chunk, so every matching occurrence is appended twice. */
    q = (mc_entity_bounds){2.0, 7.0, 2.0, 524298.0, 15.0, 13.0};
    mc_entity *aliased[] = {a, b, a, b};
    CHECK(mc_chunk_cache_key(0, 0) == mc_chunk_cache_key(32768, 0) &&
          nearby_query_is(w, NULL, &q, aliased, COUNT(aliased)) &&
          f->providers[slot].cache->count == 1 && f->providers[slot].cache->insertion_count == 1 &&
          mc_world_store_chunk_count(f->stores[slot]) == 1 && f->states[slot]->x == 0 &&
          f->states[slot]->z == 0 && w->active_calls == 0 && w->local_leases == 0);
    /* A distant, narrow alias requires chunk-coordinate acquisition; a
     * block-coordinate getter would shift 32768 again and miss the real key. */
    CHECK(mc_entity_set_position(a, 524296.0, 8.0, 8.0) == MC_ENTITY_OK);
    q = (mc_entity_bounds){524290.0, 7.0, 2.0, 524301.0, 15.0, 13.0};
    CHECK(nearby_query_is(w, NULL, &q, one, COUNT(one)) && f->providers[slot].cache->count == 1 &&
          f->providers[slot].cache->insertion_count == 1 &&
          mc_world_store_chunk_count(f->stores[slot]) == 1);
    ok = true;
cleanup:
    return ok;
}

static bool nearby_clear_final_entity_hold(fixture *f, size_t side) {
    bool ok = false;
    const size_t slot = side + 2u;
    mc_world_runtime *const w = f->worlds[slot];
    mc_chunk_state *fresh_state = NULL;
    mc_world_chunk *fresh_chunk = NULL;
    mc_entity *entity = NULL;
    uint8_t *blocks = NULL;
    const mc_entity_bounds empty = {34.0, 0.0, 2.0, 0.0, 15.0, 13.0};
    const mc_entity_bounds query = {114.0, 0.0, -142.0, 125.0, 15.0, -131.0};
    CHECK(nearby_query_is(w, NULL, &empty, NULL, 0));
    const size_t live_before = f->entity_classes[side]->live_entities;
    const size_t bindings_before = w->persistent_bindings;
    blocks = calloc(MC_CHUNK_BLOCKS, sizeof(*blocks));
    CHECK(blocks != NULL);
    const mc_world_storage_status constructed =
        mc_world_runtime_chunk_construct(w, 7, -9, &blocks, MC_CHUNK_BLOCKS, &fresh_chunk, NULL);
    if (fresh_chunk != NULL)
        fresh_state = mc_world_chunk_state(fresh_chunk);
    CHECK(constructed == MC_WORLD_STORAGE_OK && blocks == NULL && fresh_state != NULL &&
          !fresh_state->active && !fresh_state->blank &&
          !mc_chunk_cache_contains(f->providers[slot].cache, 7, -9) &&
          mc_world_store_chunk_count(f->stores[slot]) == 2);
    const size_t tracked = f->entity_count;
    CHECK(make_entity(f, side, w, 120.0, 8.0, -136.0, &entity) && f->entities[tracked] == entity &&
          entity->holds == 1 && f->entity_classes[side]->live_entities == live_before + 1u &&
          w->persistent_bindings == bindings_before + 1u);
    CHECK(mc_world_runtime_chunk_add_entity(fresh_chunk, entity, NULL) == MC_WORLD_STORAGE_OK &&
          entity->holds == 2);
    mc_world_nearby_error error;
    CHECK(nearby_gather(w, fresh_state, NULL, &query, w->fields.R, &error) == MC_WORLD_STORAGE_OK &&
          w->fields.R->count == 1 && w->fields.R->items[0] == entity &&
          w->nearby_entities.entries[0] == entity && entity->holds == 3);

    /* This genuine store-owned Chunk was never activated or published. Its
     * checked retirement drops the real slice hold without retiring the World
     * or its captured R. Releasing the caller then leaves R as sole owner. */
    const mc_world_storage_status retired =
        mc_world_store_retire_chunk(f->stores[slot], fresh_state, NULL);
    if (retired == MC_WORLD_STORAGE_OK) {
        fresh_state = NULL;
        fresh_chunk = NULL;
    }
    CHECK(retired == MC_WORLD_STORAGE_OK && entity->holds == 2 &&
          mc_world_store_chunk_count(f->stores[slot]) == 1);
    mc_entity_release(entity);
    f->entities[tracked] = NULL;
    entity = NULL;
    CHECK(w->nearby_entities.entries[0]->holds == 1 &&
          f->entity_classes[side]->live_entities == live_before + 1u &&
          w->persistent_bindings == bindings_before + 1u);
    const size_t capacity = w->fields.R->capacity;
    const size_t bytes = w->construction.direct_allocation_bytes;
    /* Do not touch the occurrence after this real source clear: its final
     * release destroys Entity and releases the Entity's construction-World
     * binding while the actual World call guard keeps the World live. */
    CHECK(nearby_query_is(w, NULL, &empty, NULL, 0) &&
          f->entity_classes[side]->live_entities == live_before &&
          w->persistent_bindings == bindings_before && w->nearby_entities.count == 0 &&
          w->fields.R->count == 0 && w->fields.R->capacity == capacity &&
          w->construction.direct_allocation_bytes == bytes && mc_world_nearby_owned_valid(w) &&
          w->active_calls == 0 && w->local_leases == 0);
    ok = true;
cleanup:
    free(blocks);
    if (fresh_state != NULL &&
        mc_world_store_retire_chunk(f->stores[slot], fresh_state, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    return ok;
}

static bool nearby_budget(fixture *f, size_t side) {
    bool ok = false;
    const size_t slot = side + 4u;
    mc_world_runtime *w = f->worlds[slot];
    mc_entity *entity = NULL;
    CHECK(make_entity(f, side, NULL, 8.0, 8.0, 8.0, &entity));
    for (size_t i = 0; i < 5; ++i)
        CHECK(mc_world_runtime_chunk_add_entity(f->chunks[slot], entity, NULL) ==
              MC_WORLD_STORAGE_OK);
    const mc_entity_bounds q = {2.0, 0.0, 2.0, 13.0, 15.0, 13.0};
    const size_t baseline = w->construction.direct_allocation_bytes;
    mc_world_nearby_error error;
    mc_world_entity_list_view out = {NULL, w->fields.R};
    CHECK(mc_world_runtime_nearby_entities(w, NULL, &q, &out, &error) == MC_WORLD_STORAGE_LIMIT &&
          error.query.bytecode == 122 && error.query.chunk_bytecode == 131 && error.slice == 0 &&
          error.index == 4 && error.entity.status == MC_ENTITY_OK && out.owner == NULL &&
          out.identity == w->fields.R && w->fields.R->count == 4 && w->fields.R->capacity == 4 &&
          entity->holds == 10 &&
          w->construction.direct_allocation_bytes ==
              baseline + 4u * (sizeof(void *) + sizeof(mc_entity *)) &&
          w->construction.direct_allocation_bytes ==
              w->construction_limits.max_direct_allocation_bytes);
    for (size_t i = 0; i < 4; ++i)
        CHECK(w->fields.R->items[i] == entity && w->nearby_entities.entries[i] == entity);
    CHECK(mc_world_runtime_nearby_entities(w, NULL, &q, &out, &error) == MC_WORLD_STORAGE_LIMIT &&
          error.index == 4 && entity->holds == 10 && w->fields.R->count == 4 &&
          w->fields.R->capacity == 4);
    CHECK(nearby_query_is(w, entity, &q, NULL, 0) && entity->holds == 6 &&
          w->fields.R->capacity == 4 && w->active_calls == 0 && w->local_leases == 0);
    ok = true;
cleanup:
    return ok;
}

static bool nearby_failures_and_blank(fixture *f, size_t side) {
    bool ok = false;
    const size_t slot = side + 6u;
    mc_world_runtime *w = f->worlds[slot];
    mc_entity *a = NULL, *b = NULL;
    CHECK(make_entity(f, side, NULL, 8.0, 8.0, 8.0, &a) &&
          make_entity(f, side, NULL, 8.0, 16.0, 8.0, &b) &&
          mc_world_runtime_chunk_add_entity(f->chunks[slot], a, NULL) == MC_WORLD_STORAGE_OK);
    const mc_entity_bounds q = {2.0, 0.0, 2.0, 13.0, 31.0, 13.0};
    const mc_entity_bounds miss = {40.0, 0.0, 40.0, 41.0, 31.0, 41.0};
    const mc_entity_bounds inverted = {2.0, 80.0, 2.0, 13.0, 16.0, 13.0};
    mc_world_nearby_error error;
    CHECK(w->fallback.blank && mc_world_store_chunk_object(f->stores[slot], &w->fallback) == NULL &&
          nearby_gather(w, &w->fallback, NULL, NULL, NULL, &error) == MC_WORLD_STORAGE_OK &&
          error.query.chunk_bytecode == 0 && w->fields.R->count == 0);
    CHECK(nearby_gather(w, NULL, NULL, NULL, NULL, &error) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.query.chunk_bytecode == 0);
    CHECK(nearby_gather(w, f->states[slot], NULL, NULL, NULL, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.query.chunk_bytecode == 1);
    CHECK(nearby_gather(w, f->states[slot], a, &q, NULL, &error) == MC_WORLD_STORAGE_OK &&
          nearby_gather(w, f->states[slot], NULL, &miss, NULL, &error) == MC_WORLD_STORAGE_OK &&
          nearby_gather(w, f->states[slot], NULL, &q, NULL, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.query.chunk_bytecode == 131 && error.slice == 0 && error.index == 0 &&
          a->holds == 2);
    CHECK(mc_world_runtime_chunk_add_entity(f->chunks[slot], b, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_chunk *const chunk = f->chunks[slot];
    mc_world_chunk_list_array *const actual_m = chunk->m;
    chunk->m = NULL;
    const mc_world_storage_status null_m =
        nearby_gather(w, f->states[slot], NULL, &inverted, NULL, &error);
    chunk->m = actual_m;
    CHECK(null_m == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.query.chunk_bytecode == 48 &&
          error.slice == SIZE_MAX && w->fields.R->count == 0);
    CHECK(nearby_gather(w, f->states[slot], NULL, &inverted, NULL, &error) == MC_WORLD_STORAGE_OK);

    /* Reached source-null List: slice0's append stays when slice1.size fails.
     * Actual owning slice storage and its Entity hold remain live throughout. */
    mc_world_chunk_entity_list *const slice_one = actual_m->items[1];
    actual_m->items[1] = NULL;
    const mc_world_storage_status null_slice =
        nearby_gather(w, f->states[slot], NULL, &q, w->fields.R, &error);
    actual_m->items[1] = slice_one;
    CHECK(null_slice == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.query.chunk_bytecode == 88 &&
          error.slice == 1 && error.index == 0 && w->fields.R->count == 1 &&
          w->fields.R->items[0] == a && a->holds == 3 && b->holds == 2);
    CHECK(nearby_query_is(w, NULL, &inverted, NULL, 0) && a->holds == 2);

    /* Native invariant fault control only: SIZE_MAX real holds cannot be
     * accumulated in a finite contract run. This exercises retain rejection,
     * not Java gameplay, source OOM timing or an original execution result. */
    const size_t b_holds = b->holds;
    b->holds = SIZE_MAX;
    const mc_world_storage_status hold_limit =
        nearby_gather(w, f->states[slot], NULL, &q, w->fields.R, &error);
    b->holds = b_holds;
    CHECK(hold_limit == MC_WORLD_STORAGE_LIMIT && error.query.chunk_bytecode == 131 &&
          error.entity.status == MC_ENTITY_LIMIT && error.slice == 1 && error.index == 0 &&
          w->fields.R->count == 1 && w->fields.R->items[0] == a && a->holds == 3 && b->holds == 2 &&
          w->fields.R->capacity == 4 && w->nearby_entities.count == 1);
    CHECK(nearby_query_is(w, NULL, &inverted, NULL, 0) && a->holds == 2);
    mc_entity_bounds *const b_bounds = b->bounds;
    b->bounds = NULL;
    const mc_world_storage_status null_bounds =
        nearby_gather(w, f->states[slot], NULL, &q, w->fields.R, &error);
    b->bounds = b_bounds;
    CHECK(null_bounds == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.query.chunk_bytecode == 118 &&
          error.slice == 1 && error.index == 0 && w->fields.R->count == 1 &&
          w->fields.R->items[0] == a && a->holds == 3 && b->holds == 2);

    /* Exclusion occurs before the excluded candidate's bounds dereference. */
    b->bounds = NULL;
    const mc_world_storage_status excluded_null =
        nearby_gather(w, f->states[slot], b, &q, w->fields.R, &error);
    b->bounds = b_bounds;
    CHECK(excluded_null == MC_WORLD_STORAGE_OK && w->fields.R->count == 2 && a->holds == 4 &&
          b->holds == 2);
    const mc_entity_bounds low_only = {2.0, 0.0, 2.0, 13.0, 15.0, 13.0};
    mc_world_chunk_entity_list *const slice_zero = actual_m->items[0];
    mc_entity *const actual_candidate = slice_zero->items[0];
    slice_zero->items[0] = NULL;
    const mc_world_storage_status null_equals_excluded =
        nearby_gather(w, f->states[slot], NULL, &low_only, NULL, &error);
    slice_zero->items[0] = actual_candidate;
    CHECK(null_equals_excluded == MC_WORLD_STORAGE_OK && a->holds == 4 && b->holds == 2);
    slice_zero->items[0] = NULL;
    const mc_world_storage_status null_candidate =
        nearby_gather(w, f->states[slot], b, &low_only, NULL, &error);
    slice_zero->items[0] = actual_candidate;
    CHECK(null_candidate == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.query.chunk_bytecode == 118 && error.slice == 0 && error.index == 0 &&
          a->holds == 4 && b->holds == 2 && w->fields.R->count == 2);
    const size_t count = w->fields.R->count, bytes = w->construction.direct_allocation_bytes;
    CHECK(nearby_gather(w, &w->fallback, b, NULL, w->fields.R, &error) == MC_WORLD_STORAGE_OK &&
          w->fields.R->count == count && a->holds == 4 && b->holds == 2 &&
          w->construction.direct_allocation_bytes == bytes && w->active_calls == 0 &&
          w->local_leases == 0 && mc_world_nearby_owned_valid(w));

    CHECK(nearby_gather(w, f->states[slot], b, &q, w->fields.R, &error) == MC_WORLD_STORAGE_OK &&
          nearby_gather(w, f->states[slot], b, &q, w->fields.R, &error) == MC_WORLD_STORAGE_OK &&
          w->fields.R->count == 4 && w->fields.R->capacity == 4 && a->holds == 6 && b->holds == 2);
    const size_t growing_bytes = w->construction.direct_allocation_bytes;
    void **const growing_items = w->fields.R->items;
    mc_entity **const growing_entries = w->nearby_entities.entries;
    /* Same explicitly native hold-overflow fault, now after growth allocated
     * both new arrays. Excluding a reaches b as the first attempted addition;
     * failed retain must discard both arrays and preserve every old owner. */
    b->holds = SIZE_MAX;
    const mc_world_storage_status growing_hold_limit =
        nearby_gather(w, f->states[slot], a, &q, w->fields.R, &error);
    b->holds = b_holds;
    CHECK(growing_hold_limit == MC_WORLD_STORAGE_LIMIT && error.query.chunk_bytecode == 131 &&
          error.entity.status == MC_ENTITY_LIMIT && error.slice == 1 && error.index == 0 &&
          w->fields.R->count == 4 && w->fields.R->capacity == 4 &&
          w->fields.R->items == growing_items && w->nearby_entities.entries == growing_entries &&
          w->nearby_entities.count == 4 && w->nearby_entities.capacity == 4 &&
          w->construction.direct_allocation_bytes == growing_bytes && a->holds == 6 &&
          b->holds == 2 && mc_world_nearby_owned_valid(w));
    for (size_t i = 0; i < 4; ++i)
        CHECK(w->fields.R->items[i] == a && w->nearby_entities.entries[i] == a);
    ok = true;
cleanup:
    return ok;
}

static int nearby_foreign_value = 73;
static void *nearby_foreign_items[] = {&nearby_foreign_value};
static mc_world_reference_list nearby_foreign = {nearby_foreign_items, 1, 1};

static bool nearby_terminal(fixture *f) {
    bool ok = false;
    for (size_t slot = 0; slot < WORLD_COUNT; ++slot) {
        mc_world_runtime *const w = f->worlds[slot];
        mc_world_reference_list *const original = w->nearby_entities.list;
        const size_t bytes = w->construction.direct_allocation_bytes;
        const size_t backing = original->capacity * (sizeof(void *) + sizeof(mc_entity *));
        /* Unknown source R remains borrowed. Native teardown must release its
         * captured descriptor exactly once and must never free this static one. */
        w->fields.R = &nearby_foreign;
        const mc_world_storage_status retired = mc_world_runtime_retire_entity_containers(w, NULL);
        if (retired == MC_WORLD_STORAGE_OK)
            f->terminal[slot] = true;
        CHECK(retired == MC_WORLD_STORAGE_OK && w->entity_containers_retired &&
              w->fields.R == &nearby_foreign && original == w->nearby_entities.list &&
              original->count == 0 && original->capacity == 0 && original->items == NULL &&
              w->nearby_entities.count == 0 && w->nearby_entities.capacity == 0 &&
              w->nearby_entities.entries == NULL &&
              w->construction.direct_allocation_bytes == bytes - backing &&
              mc_world_nearby_owned_valid(w));
        CHECK(mc_world_runtime_retire_entity_containers(w, NULL) == MC_WORLD_STORAGE_OK &&
              w->construction.direct_allocation_bytes == bytes - backing &&
              nearby_foreign.count == 1 && nearby_foreign.capacity == 1 &&
              nearby_foreign.items == nearby_foreign_items && nearby_foreign_value == 73);
        mc_world_entity_list_view output = {0};
        mc_world_nearby_error error;
        CHECK(mc_world_runtime_nearby_entities(w, NULL, NULL, &output, &error) ==
                  MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              error.query.bytecode == 0 && output.owner == NULL && output.identity == NULL &&
              w->active_calls == 0 && w->local_leases == 0);
    }
    for (size_t i = 0; i < f->entity_count; ++i)
        CHECK(f->entities[i] == NULL || f->entities[i]->holds == 1);
    ok = true;
cleanup:
    return ok;
}

static bool nearby_cleanup(fixture *f) {
    if (f == NULL)
        return true;
    /* The reused fixture tracks its original (0,0) owner. Retire our three
     * additional real loaded owners after the same terminal/cache protocol. */
    for (size_t side = 0; side < 2; ++side) {
        if (nearby_extra[side][0] == NULL && nearby_extra[side][1] == NULL &&
            nearby_extra[side][2] == NULL)
            continue;
        if (!f->terminal[side]) {
            if (mc_world_runtime_retire_entity_containers(f->worlds[side], NULL) !=
                MC_WORLD_STORAGE_OK)
                return false;
            f->terminal[side] = true;
        }
        if (!retire_descriptors(f, side))
            return false;
        if (!f->world_retired[side]) {
            if (mc_world_runtime_retire_world(f->worlds[side], NULL) != MC_WORLD_STORAGE_OK)
                return false;
            f->world_retired[side] = true;
        }
        for (size_t i = 0; i < 3; ++i)
            if (nearby_extra[side][i] != NULL) {
                if (mc_world_store_retire_chunk(f->stores[side], nearby_extra[side][i], NULL) !=
                    MC_WORLD_STORAGE_OK)
                    return false;
                nearby_extra[side][i] = NULL;
            }
    }
    return cleanup_fixture(f);
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    const uint16_t title[] = {'N', 'e', 'a', 'r', 'b', 'y'};
    if (argc != 3 || strcmp(argv[1], "world-nearby") != 0 ||
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
        CHECK(nearby_open_store(f, slot, root) && nearby_create_world(f, slot));
    for (size_t side = 0; side < 2; ++side)
        CHECK(nearby_order_and_views(f, side) && nearby_numeric_and_aliases(f, side) &&
              nearby_clear_final_entity_hold(f, side) && nearby_budget(f, side) &&
              nearby_failures_and_blank(f, side));
    CHECK(nearby_terminal(f));
    result = 0;
cleanup:
    if (!nearby_cleanup(f)) {
        fputs("nearby cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    if (result == 0 && (nearby_foreign.count != 1 || nearby_foreign.capacity != 1 ||
                        nearby_foreign.items != nearby_foreign_items || nearby_foreign_value != 73))
        return 1;
    if (result == 0 && printf("PASS: %u World nearby Entity checks (world-nearby)\n", checks) < 0)
        result = 1;
    return result;
}
