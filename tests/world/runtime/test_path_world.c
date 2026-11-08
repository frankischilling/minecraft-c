/* Direct FW(World) over the existing real stored-Chunk path fixture. */
#define MC_PATH_ENTRY mc_direct_path_unused_fixture_main
#include "test_path.c"
#undef MC_PATH_ENTRY

/* Authored borrowed-loader failure envelope on a real admitted direct finder.
 * This tests native retention policy, not an original loader execution. */
static bool direct_loader_capture(path_fixture *f) {
    mc_path_finder *finder = NULL;
    mc_path_budget budget = {.max_bytes = SIZE_MAX, .max_allocations = SIZE_MAX};
    mc_path_error error;
    CHECK(mc_path_finder_create_for_world(f->base.entities, f->base.registry, f->base.math,
                                          f->base.world, NULL, &budget, &finder,
                                          &error) == MC_PATH_OK &&
          finder != NULL);
    mc_world_source_chunk_error source = {0};
    source.source.status = MC_WORLD_SOURCE_SOURCE_EXCEPTION;
    source.source.source_exception = true;
    source.source.exception_class = "java/lang/NullPointerException";
    source.source.operation = "Authored path loader failure envelope";
    source.source.frames[0] = (mc_world_source_site){"ld", "a", "(Lfd;II)Llm;", 11};
    source.source.frame_count = 1;
    finder->world_access->acquisition.source_chunk = &source;
    finder->world_access->query_failed = true;
    finder->error.source_chunk = &source;
    CHECK(!pf_source(finder, source.source.exception_class, source.source.operation));
    const mc_path_error original = finder->error.path;
    const size_t bytes = budget.bytes, allocations = budget.allocations;
    budget.max_bytes = bytes;
    budget.max_allocations = allocations;
    CHECK(pf_capture_world_query(finder) && finder->world_access->source_copied &&
          finder->error.source_chunk == &finder->world_access->source &&
          finder->error.source_chunk != &source && budget.bytes == bytes &&
          budget.allocations == allocations);
    memset(&source, 0, sizeof(source));
    CHECK(finder->error.source_chunk->source.source_exception &&
          finder->error.source_chunk->source.frames[0].pc == 11);
    const size_t held = f->base.world->persistent_bindings;
    f->base.world->persistent_bindings = 0;
    const mc_path_status refused = mc_path_finder_retire(&finder, &error);
    f->base.world->persistent_bindings = held;
    CHECK(refused == MC_PATH_STATE && finder != NULL &&
          memcmp(&finder->error.path, &original, sizeof(original)) == 0 &&
          finder->error.source_chunk->source.frames[0].pc == 11 && budget.bytes == bytes &&
          budget.allocations == allocations);
    CHECK(mc_path_finder_retire(&finder, &error) == MC_PATH_OK && finder == NULL &&
          budget.bytes == 0);
    return true;
}

static bool direct_metadata(path_fixture *f) {
    mc_chunk_state *state = NULL;
    CHECK(mc_chunk_cache_lookup(f->base.provider.cache, 0, 0, &state) && state != NULL);
    mc_world_chunk *object = mc_world_store_chunk_object(f->base.store, state);
    CHECK(object != NULL && object->e != NULL && object->e->a != NULL);
    mc_world_chunk_nibble *const original = object->e;
    uint8_t *const backing = original->a;
    const size_t length = original->length;
    mc_world_runtime_acquisition_error *acquisition = calloc(1, sizeof(*acquisition));
    uint8_t *replacement = calloc(MC_CHUNK_NIBBLE_BYTES, 1);
    CHECK(acquisition != NULL && replacement != NULL);
    mc_world_chunk_nibble alternate = {replacement, MC_CHUNK_NIBBLE_BYTES};
    const size_t index = ((size_t)8 << 11 | (size_t)8 << 7 | 64u) >> 1;
    replacement[index] = 0xa3;
    object->e = &alternate;
    int32_t value = -1;
    mc_world_storage_error error;
    mc_world_metadata_source_error source;
    CHECK(mc_world_runtime_metadata_get_source_failure(f->base.world, 8, 64, 8, &value, &error,
                                                       acquisition,
                                                       &source) == MC_WORLD_STORAGE_OK &&
          value == 3 && !source.source_exception &&
          mc_world_runtime_metadata_get(f->base.world, 8, 65, 8, &value, NULL) ==
              MC_WORLD_STORAGE_OK &&
          value == 10);
    object->e = NULL;
    value = -71;
    CHECK(mc_world_runtime_metadata_get_source_failure(f->base.world, 8, 64, 8, &value, &error,
                                                       acquisition, &source) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          value == -71 && source.source_exception && source.world_pc == 68 &&
          source.chunk_pc == 7 && source.nibble_pc == 0 &&
          strcmp(source.exception_class, "java/lang/NullPointerException") == 0);
    CHECK(mc_world_runtime_metadata_get(f->base.world, 8, 128, 8, &value, NULL) ==
              MC_WORLD_STORAGE_OK &&
          value == 0);
    object->e = original;
    original->a = NULL;
    for (int32_t y = 64; y <= 65; ++y) {
        value = -71;
        CHECK(mc_world_runtime_metadata_get_source_failure(f->base.world, 8, y, 8, &value, &error,
                                                           acquisition, &source) ==
                  MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              value == -71 && source.source_exception &&
              source.nibble_pc == (y == 64 ? 36u : 47u) &&
              strcmp(source.exception_class, "java/lang/NullPointerException") == 0);
        mc_world_storage_error other;
        CHECK(mc_world_runtime_metadata_get_detailed(f->base.world, 8, y, 8, &value, &other,
                                                     acquisition) == error.status &&
              other.operation == error.operation && value == -71 &&
              mc_world_runtime_metadata_get_failure(f->base.world, 8, y, 8, &value, &other,
                                                    acquisition) == error.status &&
              other.operation == error.operation && value == -71);
    }
    original->a = backing;
    original->length = 0;
    value = -71;
    CHECK(mc_world_runtime_metadata_get_source_failure(f->base.world, 8, 64, 8, &value, &error,
                                                       acquisition, &source) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          value == -71 && source.source_exception && source.nibble_pc == 36 &&
          strcmp(source.exception_class, "java/lang/ArrayIndexOutOfBoundsException") == 0);
    original->length = length;
    free(replacement);
    free(acquisition);
    return true;
}

static bool direct_case(const char *resources, const char *directory, unsigned int mode) {
    bool ok = false;
    path_fixture *f = calloc(1, sizeof(*f));
    mc_path_finder *finder = NULL;
    mc_path *prior = NULL;
    mc_path_budget budget = {.max_bytes = SIZE_MAX, .max_allocations = SIZE_MAX};
    mc_path_error error = {0};
    CHECK(f != NULL);
    f->input = (path_profile){.operation = "FD_entity",
                              .source = {8.5, 64, 8.5},
                              .target = {12.5, 64, 8.5},
                              .width = UINT32_C(0x3f666666),
                              .height = UINT32_C(0x3fa66666)};
    CHECK(prepare_path_fixture(f, resources, directory));
    if (mode == 0)
        CHECK(direct_metadata(f) && direct_loader_capture(f));
    mc_world_runtime *world = f->base.world;
    const size_t bindings = world->persistent_bindings;
    mc_chunk_provider_cache_observation before, after;
    CHECK(mc_chunk_provider_cache_observe(world->fields.v, &before));
    CHECK(mc_path_finder_create_for_world(f->base.entities, f->base.registry, f->base.math,
                                          mode == 1 ? NULL : world, NULL, &budget, &finder,
                                          &error) == MC_PATH_OK &&
          finder != NULL);
    mc_path_finder_state first, state;
    CHECK(mc_path_finder_inspect(finder, &first) && first.constructed && first.direct_world &&
          first.a == NULL && first.world == (mode == 1 ? NULL : world) && first.searches == 0 &&
          world->persistent_bindings == bindings + (mode == 1 ? 0u : 1u) &&
          mc_chunk_provider_cache_observe(world->fields.v, &after) &&
          after.generic_gets == before.generic_gets);
    mc_chunk_provider *const provider = world->fields.v;
    uint8_t *metadata = NULL;
    mc_world_chunk_nibble *nibble = NULL;
    if (mode == 2)
        world->fields.v = NULL;
    if (mode == 3) {
        CHECK(terrain_cell(f, 8, 64, 9, 64, 0));
        mc_chunk_state *chunk = NULL;
        CHECK(mc_chunk_cache_lookup(f->base.provider.cache, 0, 0, &chunk) && chunk != NULL);
        mc_world_chunk *object = mc_world_store_chunk_object(f->base.store, chunk);
        CHECK(object != NULL && object->e != NULL);
        nibble = object->e;
        metadata = nibble->a;
        nibble->a = NULL;
    }
    const mc_path_status status = mc_path_finder_to_entity(finder, f->source, f->target, 32.0f);
    CHECK(mc_path_finder_inspect(finder, &state) && state.searches == 1 && !state.active &&
          state.heap_identity == first.heap_identity && state.map_identity == first.map_identity);
    if (mode == 0) {
        CHECK(status == MC_PATH_OK && state.returned && state.result != NULL &&
              state.query_count > 0 && state.cell_queries == state.query_count &&
              mc_chunk_provider_cache_observe(world->fields.v, &after) &&
              after.generic_gets == before.generic_gets + state.cell_queries);
        for (size_t i = 0; i < state.query_count; ++i)
            CHECK(state.queries[i].direct_world && state.queries[i].completed &&
                  state.queries[i].world_status == MC_WORLD_STORAGE_OK);
        prior = state.result;
        CHECK(mc_path_retain(prior, NULL) == MC_PATH_OK);
        mc_path_state old;
        CHECK(mc_path_inspect(prior, &old) && old.a > 1 && old.a <= 64);
        mc_path_point *points[64];
        mc_path_point old_values[64];
        for (int32_t i = 0; i < old.a; ++i) {
            points[(size_t)i] = old.b[(size_t)i];
            memcpy(&old_values[(size_t)i], points[(size_t)i], sizeof(mc_path_point));
        }
        const void *const heap = state.heap_identity, *const map = state.map_identity;
        const mc_path_query *const journal = state.queries;
        const size_t previous_queries = state.cell_queries;
        const size_t journal_capacity = finder->query_capacity;
        CHECK(mc_entity_set_position(f->target, 8.5, 64, 12.5) == MC_ENTITY_OK &&
              mc_path_finder_to_entity(finder, f->source, f->target, 32.0f) == MC_PATH_OK &&
              mc_path_finder_inspect(finder, &state) && state.searches == 2 && state.returned &&
              state.result != NULL && state.result != prior && state.heap_identity == heap &&
              state.map_identity == map && state.cell_queries > previous_queries &&
              state.query_count == state.cell_queries - previous_queries);
        if (state.query_count <= journal_capacity)
            CHECK(state.queries == journal && finder->query_capacity == journal_capacity);
        mc_path_state next, kept;
        CHECK(mc_path_inspect(state.result, &next) && mc_path_inspect(prior, &kept) &&
              next.arena_identity == old.arena_identity && kept.a == old.a && kept.c == old.c &&
              kept.b == old.b && kept.holds == 1);
        for (int32_t i = 0; i < old.a; ++i)
            CHECK(kept.b[(size_t)i] == points[(size_t)i] &&
                  memcmp(points[(size_t)i], &old_values[(size_t)i], sizeof(mc_path_point)) == 0);
        const mc_path_point *last = NULL;
        mc_path_point_state point_state;
        CHECK(mc_path_last(prior, &last, NULL) == MC_PATH_OK &&
              mc_path_point_inspect(prior, last, &point_state) && point_state.a == 12 &&
              point_state.b == 64 && point_state.c == 8 &&
              mc_path_last(state.result, &last, NULL) == MC_PATH_OK &&
              mc_path_point_inspect(state.result, last, &point_state) && point_state.a == 8 &&
              point_state.b == 64 && point_state.c == 12);
    } else {
        CHECK(status != MC_PATH_OK && state.failed && !state.returned && state.error != NULL);
        const mc_path_error original = state.error->path;
        CHECK(mc_path_finder_to_entity(finder, f->source, f->target, 32.0f) == MC_PATH_STATE);
        if (mode == 1)
            CHECK(status == MC_PATH_SOURCE_EXCEPTION && original.source_exception &&
                  strcmp(original.exception_class, "java/lang/NullPointerException") == 0 &&
                  original.site.pc == 57);
        else {
            CHECK(state.error->world != NULL && state.error->acquisition != NULL &&
                  state.error->world->status != MC_WORLD_STORAGE_OK);
            if (mode == 2)
                CHECK(status == MC_PATH_SOURCE_EXCEPTION && original.source_exception &&
                      !original.native_policy && original.exception_class != NULL &&
                      strcmp(original.exception_class, "java/lang/NullPointerException") == 0 &&
                      original.child_site.class_name != NULL &&
                      strcmp(original.child_site.class_name, "fd") == 0 &&
                      strcmp(original.child_site.method, "c") == 0 && original.child_site.pc == 6);
            if (mode == 3)
                CHECK(state.site.pc == 101 && state.queries[state.query_count - 1].metadata &&
                      state.error->metadata != NULL && state.error->metadata->source_exception &&
                      state.error->metadata->nibble_pc == 36 && original.source_exception &&
                      original.child_site.pc == 36);
            const size_t held = world->persistent_bindings;
            world->persistent_bindings = 0;
            const mc_path_status refused = mc_path_finder_retire(&finder, &error);
            world->persistent_bindings = held;
            CHECK(refused == MC_PATH_STATE && finder != NULL &&
                  mc_path_finder_inspect(finder, &state) && state.retiring &&
                  memcmp(&state.error->path, &original, sizeof(original)) == 0);
        }
    }
    world->fields.v = provider;
    if (metadata != NULL) {
        CHECK(nibble != NULL);
        nibble->a = metadata;
    }
    CHECK(mc_path_finder_retire(&finder, &error) == MC_PATH_OK && finder == NULL &&
          budget.bytes == 0 && world->persistent_bindings == bindings);
    if (prior != NULL) {
        mc_path_state kept;
        CHECK(mc_path_inspect(prior, &kept) && kept.holds == 1 && kept.arena_holds == 1);
        mc_path_release(prior);
        prior = NULL;
    }
    CHECK(retire_path_fixture(f));
    ok = true;
    free(f);
    return ok;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    for (unsigned int mode = 0; mode < 4; ++mode) {
        char directory[8192];
        const int length = snprintf(directory, sizeof(directory), "%s/case%u", argv[2], mode);
        if (length <= 0 || (size_t)length >= sizeof(directory) ||
            !direct_case(argv[1], directory, mode))
            return 1;
    }
    printf("Direct World paths: %u checks\n", checks);
    return 0;
}
