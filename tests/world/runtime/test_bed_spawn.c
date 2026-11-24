/* Reuse actual World/Store/Chunk construction; its observation main is not run. */
#define MC_WORLD_ENTITY_TICK_MAIN mc_bed_spawn_tick_fixture_main
#include "test_entity_tick.c"
#undef MC_WORLD_ENTITY_TICK_MAIN

#include "../../../src/block/registry/runtime_internal.h"
#include "mc/world/chunk/nibbles.h"
#include "mc/world/runtime/bed_spawn.h"

#include <ctype.h>
#include <errno.h>

#define BED_FOOTPRINT 16u
#define BED_PLANE_BYTES (MC_CHUNK_BLOCKS + MC_CHUNK_NIBBLE_BYTES)

typedef struct bed_fixture {
    fixture *source;
    mc_chunk_state *chunks[BED_FOOTPRINT];
    mc_world_bed_spawn_error *error;
    uint8_t *planes;
} bed_fixture;

static bool bed_prepare(bed_fixture *b) {
    bool ok = false;
    for (int32_t x = -2; x <= 1; ++x)
        for (int32_t z = -2; z <= 1; ++z)
            CHECK(tick_seed(b->source, 0, x, z));
    for (int32_t x = -2; x <= 1; ++x)
        for (int32_t z = -2; z <= 1; ++z) {
            const size_t index = (size_t)(x + 2) * 4u + (size_t)(z + 2);
            CHECK(mc_world_runtime_get(b->source->worlds[0], x, z, &b->chunks[index]) ==
                      MC_CHUNK_ACQUIRE_OK &&
                  !b->chunks[index]->blank && b->chunks[index]->data != NULL);
        }
    ok = true;
cleanup:
    return ok;
}

static void bed_clear(bed_fixture *b) {
    for (size_t i = 0; i < BED_FOOTPRINT; ++i) {
        memset(b->chunks[i]->data->blocks, 0, MC_CHUNK_BLOCKS);
        memset(b->chunks[i]->data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
    }
}

static bool bed_cell(bed_fixture *b, int32_t x, int32_t y, int32_t z, int32_t id,
                     int32_t metadata) {
    const int32_t cx = mc_java_i32_shr(x, 4), cz = mc_java_i32_shr(z, 4);
    if (cx < -2 || cx > 1 || cz < -2 || cz > 1 || y < 0 || y >= 128 || id < 0 || id > 255 ||
        metadata < 0 || metadata > 15)
        return false;
    mc_chunk_data *data = b->chunks[(size_t)(cx + 2) * 4u + (size_t)(cz + 2)]->data;
    const int32_t lx = (int32_t)((uint32_t)x & 15u), lz = (int32_t)((uint32_t)z & 15u);
    data->blocks[boxes_index(lx, y, lz)] = (uint8_t)id;
    return mc_chunk_nibbles_set(data->metadata, MC_CHUNK_NIBBLE_BYTES, lx, y, lz,
                                (uint8_t)metadata);
}

static void bed_planes(bed_fixture *b) {
    for (size_t i = 0; i < BED_FOOTPRINT; ++i) {
        memcpy(b->planes + i * BED_PLANE_BYTES, b->chunks[i]->data->blocks, MC_CHUNK_BLOCKS);
        memcpy(b->planes + i * BED_PLANE_BYTES + MC_CHUNK_BLOCKS, b->chunks[i]->data->metadata,
               MC_CHUNK_NIBBLE_BYTES);
    }
}

static bool bed_planes_equal(const bed_fixture *b) {
    for (size_t i = 0; i < BED_FOOTPRINT; ++i)
        if (memcmp(b->planes + i * BED_PLANE_BYTES, b->chunks[i]->data->blocks, MC_CHUNK_BLOCKS) !=
                0 ||
            memcmp(b->planes + i * BED_PLANE_BYTES + MC_CHUNK_BLOCKS, b->chunks[i]->data->metadata,
                   MC_CHUNK_NIBBLE_BYTES) != 0)
            return false;
    return true;
}

static bool bed_record(int32_t profile, const mc_player_coordinates *position,
                       const mc_world_bed_spawn_error *e) {
    static const char *const names[] = {"CAPTURED_ACQUIRE", "BLOCK", "METADATA", "SUPPORT", "AIR"};
    if (printf("BED_SPAWN {\"case\":%" PRId32 ",\"result\":", profile) < 0)
        return false;
    if (position == NULL) {
        if (printf("null") < 0)
            return false;
    } else if (printf("[%" PRId32 ",%" PRId32 ",%" PRId32 "]", position->x, position->y,
                      position->z) < 0)
        return false;
    if (printf(",\"journal\":[") < 0)
        return false;
    for (size_t i = 0; i < e->retained_events; ++i) {
        const mc_world_bed_spawn_event *v = &e->events[i];
        if ((unsigned int)v->kind > (unsigned int)MC_WORLD_BED_SPAWN_AIR)
            return false;
        if (printf("%s{\"kind\":\"%s\",\"pc\":%u,\"x\":%" PRId32 ",\"y\":%" PRId32 ",\"z\":%" PRId32
                   ",\"completed\":%s,\"result\":%" PRId32 "}",
                   i == 0 ? "" : ",", names[v->kind], v->pc, v->x, v->y, v->z,
                   v->completed ? "true" : "false", v->result) < 0)
            return false;
    }
    return puts("]}") >= 0;
}

static bool bed_profiles(bed_fixture *b, const char *path) {
    bool ok = false;
    FILE *file = fopen(path, "rb");
    mc_player_coordinates *position = NULL;
    CHECK(file != NULL);
    char line[32768];
    int32_t profile = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        int32_t fields[2568];
        size_t count = 0;
        char *cursor = line;
        while (*cursor != '\0') {
            while (isspace((unsigned char)*cursor))
                ++cursor;
            if (*cursor == '\0')
                break;
            CHECK(count < COUNT(fields));
            errno = 0;
            char *end;
            const intmax_t value = strtoimax(cursor, &end, 10);
            CHECK(errno == 0 && end != cursor && value >= INT32_MIN && value <= INT32_MAX &&
                  (*end == '\0' || isspace((unsigned char)*end)));
            fields[count++] = (int32_t)value;
            cursor = end;
        }
        CHECK(count >= 8 && fields[0] == profile && (fields[1] == 0 || fields[1] == 1) &&
              fields[7] >= 0 && (size_t)fields[7] <= 512 && count == 8u + (size_t)fields[7] * 5u);
        bed_clear(b);
        for (size_t i = 8; i < count; i += 5)
            CHECK(
                bed_cell(b, fields[i], fields[i + 1], fields[i + 2], fields[i + 3], fields[i + 4]));
        bed_planes(b);
        mc_world_runtime *world = b->source->worlds[0];
        const mc_java_random random = *world->fields.r;
        mc_player_coordinates input = {fields[2], fields[3], fields[4]};
        const mc_world_storage_status status =
            fields[1] == 0
                ? mc_world_bed_spawn_exit(world, b->source->registry[0], input.x, input.y, input.z,
                                          fields[6], NULL, &position, b->error)
                : mc_world_bed_spawn_validate(world, b->source->registry[0], &input, NULL,
                                              &position, b->error);
        if (status != MC_WORLD_STORAGE_OK)
            fprintf(stderr, "Bed profile%" PRId32 " status%d at%s:%u %s\n", profile, (int)status,
                    b->error->source_class, b->error->pc,
                    b->error->operation == NULL ? "-" : b->error->operation);
        CHECK(status == MC_WORLD_STORAGE_OK && b->error->returned && !b->error->event_overflow &&
              b->error->event_count == b->error->retained_events &&
              b->error->retained_world_count == 0 && world->active_calls == 0 &&
              bed_planes_equal(b) && world->fields.r->state == random.state &&
              world->fields.r->have_next_next_gaussian == random.have_next_next_gaussian &&
              memcmp(&world->fields.r->next_next_gaussian, &random.next_next_gaussian,
                     sizeof(double)) == 0);
        CHECK(position == NULL || position != &input);
        CHECK(bed_record(profile, position, b->error));
        mc_world_bed_spawn_position_free(position);
        position = NULL;
        ++profile;
    }
    CHECK(!ferror(file) && profile == 30);
    ok = true;
cleanup:
    mc_world_bed_spawn_position_free(position);
    if (file != NULL && fclose(file) != 0)
        ok = false;
    return ok;
}

static bool bed_native_arrays_and_limits(bed_fixture *b) {
    bool ok = false;
    mc_world_runtime *w = b->source->worlds[0];
    mc_registry_classes *registry = b->source->registry[0];
    mc_player_coordinates *a = NULL, *other = NULL;
    mc_player_coordinates sentinel = {1, 2, 3}, *out = &sentinel;
    int32_t **table = registry->ve_a;
    int32_t *row = table[0];
    mc_block **blocks = registry->blocks.m;
    bed_clear(b);
    CHECK(bed_cell(b, 8, 64, 8, 26, 0) && bed_cell(b, 7, 63, 7, 1, 0));
    CHECK(mc_world_bed_spawn_exit(w, registry, 8, 64, 8, 0, NULL, &a, b->error) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_bed_spawn_exit(w, registry, 8, 64, 8, 0, NULL, &other, b->error) ==
              MC_WORLD_STORAGE_OK &&
          a != NULL && other != NULL && a != other && a->x == 7 && a->y == 64 && a->z == 7);
    registry->ve_a = NULL; /* Authored nullable actual source static field. */
    const mc_world_storage_status no_table =
        mc_world_bed_spawn_exit(w, registry, 8, 64, 8, 0, NULL, &out, b->error);
    registry->ve_a = table;
    CHECK(no_table == MC_WORLD_STORAGE_UNSUPPORTED_STATE && b->error->helper_pc == 31 &&
          b->error->source_exception && out == &sentinel && w->active_calls == 1 &&
          mc_world_runtime_retire_world(w, NULL) == MC_WORLD_STORAGE_BUSY);
    CHECK(mc_world_bed_spawn_error_retire(b->error) && w->active_calls == 0);
    table[0] = NULL;
    const mc_world_storage_status no_row =
        mc_world_bed_spawn_exit(w, registry, 8, 64, 8, 0, NULL, &out, b->error);
    table[0] = row;
    CHECK(no_row == MC_WORLD_STORAGE_UNSUPPORTED_STATE && b->error->helper_pc == 33 &&
          b->error->source_exception && out == &sentinel);
    CHECK(mc_world_bed_spawn_error_retire(b->error));
    int32_t *short_row = mc_registry_allocate(registry, sizeof(*short_row));
    CHECK(short_row != NULL);
    short_row[0] = 0;
    table[0] = short_row;
    const mc_world_storage_status short_status =
        mc_world_bed_spawn_exit(w, registry, 8, 64, 8, 0, NULL, &out, b->error);
    table[0] = row;
    CHECK(short_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && b->error->helper_pc == 50 &&
          b->error->child.registry.status == MC_REGISTRY_INDEX_OUT_OF_RANGE &&
          b->error->source_exception && out == &sentinel);
    CHECK(mc_world_bed_spawn_error_retire(b->error));
    registry->blocks.m = NULL;
    const mc_world_storage_status no_blocks =
        mc_world_bed_spawn_exit(w, registry, 8, 64, 8, 0, NULL, &out, b->error);
    CHECK(no_blocks == MC_WORLD_STORAGE_UNSUPPORTED_STATE && b->error->helper_pc == 101 &&
          b->error->child_kind == MC_WORLD_BED_SPAWN_CHILD_SUPPORT &&
          b->error->child.support.error.bytecode == 10 &&
          b->error->child.support.error.registry.status == MC_REGISTRY_NULL_REFERENCE &&
          !b->error->native_policy && b->error->source_exception && !b->error->source_error &&
          strcmp(b->error->exception_class, "java/lang/NullPointerException") == 0 &&
          b->error->retained_events == 2 && b->error->events[0].completed &&
          !b->error->events[1].completed && b->error->retained_world_count == 1 &&
          w->active_calls == 1 && out == &sentinel);
    CHECK(mc_world_bed_spawn_error_retire(b->error) && w->active_calls == 0);
    registry->blocks.m = blocks;
    const mc_world_bed_spawn_limits zero = {0}, eight = {8};
    CHECK(mc_world_bed_spawn_exit(w, registry, 8, 64, 8, 0, &zero, &out, b->error) ==
              MC_WORLD_STORAGE_LIMIT &&
          b->error->helper_pc == 75 && b->error->candidate_visits == 0 &&
          b->error->retained_events == 1 && out == &sentinel);
    CHECK(mc_world_bed_spawn_error_retire(b->error));
    CHECK(mc_world_bed_spawn_exit(w, registry, INT32_MAX - 1, 64, 8, 0, &eight, &out, b->error) ==
              MC_WORLD_STORAGE_LIMIT &&
          b->error->helper_pc == 75 && b->error->candidate_visits == 6 && out == &sentinel);
    CHECK(mc_world_bed_spawn_error_retire(b->error));
    CHECK(mc_world_bed_spawn_exit(w, registry, 8, 64, 8, 0, &zero, &out, NULL) ==
              MC_WORLD_STORAGE_LIMIT &&
          w->active_calls == 0);
    CHECK(mc_world_bed_spawn_validate(w, registry, NULL, NULL, &out, b->error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          b->error->wrapper_pc == 7 && b->error->provider_calls == 0 && b->error->source_exception);
    CHECK(mc_world_bed_spawn_error_retire(b->error));
    CHECK(mc_world_bed_spawn_validate(NULL, registry, &sentinel, NULL, &out, b->error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          b->error->wrapper_pc == 1 && b->error->source_exception && out == &sentinel);
    CHECK(mc_world_bed_spawn_error_retire(b->error));
    bool changed = false;
    const int32_t key = mc_chunk_cache_key(0, 0);
    CHECK(mc_chunk_pending_add(b->source->providers[0].pending, key, &changed) ==
              MC_CHUNK_ACQUIRE_OK &&
          changed);
    mc_player_coordinates input = {8, 64, 8};
    CHECK(mc_world_bed_spawn_validate(w, registry, &input, NULL, &out, b->error) ==
              MC_WORLD_STORAGE_OK &&
          b->error->provider_calls == 4 &&
          !mc_chunk_pending_contains(b->source->providers[0].pending, key));
    mc_world_bed_spawn_position_free(out);
    out = &sentinel;
    ok = true;
cleanup:
    registry->blocks.m = blocks;
    registry->ve_a = table;
    table[0] = row;
    (void)mc_world_bed_spawn_error_retire(b->error);
    mc_world_bed_spawn_position_free(a);
    mc_world_bed_spawn_position_free(other);
    return ok;
}

static bool bed_native_acquisition_limit(bed_fixture *b) {
    bool ok = false, changed = false;
    mc_world_runtime *w = b->source->worlds[0];
    mc_chunk_cache *cache = b->source->providers[0].cache;
    const size_t capacity = cache->capacity, insertion_capacity = cache->insertion_capacity;
    const size_t count = cache->count, insertions = cache->insertion_count;
    mc_player_coordinates input = {104, 64, 8}, sentinel = {1, 2, 3}, *out = &sentinel;
    const int32_t key = mc_chunk_cache_key(6, 0);
    CHECK(tick_seed(b->source, 0, 6, 0) &&
          mc_chunk_pending_add(b->source->providers[0].pending, key, &changed) ==
              MC_CHUNK_ACQUIRE_OK &&
          changed && !mc_chunk_cache_contains(cache, 6, 0));
    /* Authored native capacity ceiling. The genuine acquisition coordinator
     * refuses its known miss before pending removal or any loader callback. */
    cache->capacity = count;
    cache->insertion_capacity = insertions;
    const mc_world_storage_status status =
        mc_world_bed_spawn_validate(w, b->source->registry[0], &input, NULL, &out, b->error);
    CHECK(status == MC_WORLD_STORAGE_LIMIT && b->error->status == MC_WORLD_STORAGE_LIMIT &&
          b->error->cause.status == MC_WORLD_STORAGE_LIMIT && b->error->operation != NULL &&
          b->error->native_policy && !b->error->source_exception && !b->error->source_error &&
          b->error->wrapper_pc == 22 && b->error->provider_calls == 1 &&
          b->error->child_kind == MC_WORLD_BED_SPAWN_CHILD_ACQUISITION &&
          b->error->child.acquisition.status == MC_CHUNK_ACQUIRE_LIMIT &&
          b->error->child.acquisition.cause.status == MC_WORLD_STORAGE_OK &&
          b->error->retained_events == 1 && !b->error->events[0].completed &&
          b->error->retained_world_count == 2 && w->active_calls == 2 && out == &sentinel &&
          cache->count == count && cache->insertion_count == insertions &&
          mc_chunk_pending_contains(b->source->providers[0].pending, key) &&
          mc_world_runtime_retire_world(w, NULL) == MC_WORLD_STORAGE_BUSY);
    CHECK(mc_world_bed_spawn_error_retire(b->error) && w->active_calls == 0);
    CHECK(mc_chunk_pending_remove(b->source->providers[0].pending, key, &changed) ==
              MC_CHUNK_ACQUIRE_OK &&
          changed);
    ok = true;
cleanup:
    (void)mc_world_bed_spawn_error_retire(b->error);
    cache->capacity = capacity;
    cache->insertion_capacity = insertion_capacity;
    return ok;
}

typedef struct bed_load_observer {
    mc_world_runtime *world;
    mc_chunk_load_callback actual;
    void *actual_owner;
    mc_player_coordinates *input;
    int32_t *direction_row;
    unsigned int mode;
    size_t calls;
} bed_load_observer;

static mc_chunk_load_result bed_observe_load(void *context,
                                             const mc_chunk_acquire_context *acquisition, int32_t x,
                                             int32_t z, mc_chunk_state **out) {
    bed_load_observer *observer = context;
    ++observer->calls;
    if (observer->mode == 3) {
        /* An authored native callback refusal, distinct from a Java Throwable.
         * The actual coordinator has removed its pending key before this call. */
        observer->world->fields.v = NULL;
        observer->world->acquisition_error->cause = (mc_world_storage_error){
            MC_WORLD_STORAGE_IO_ERROR, "Authored captured loader refusal", 0};
        return MC_CHUNK_LOAD_FATAL;
    }
    const mc_chunk_load_result result =
        observer->actual(observer->actual_owner, acquisition, x, z, out);
    if (observer->calls == 1) {
        if (observer->mode == 1)
            observer->direction_row[1] = 3;
        else {
            observer->world->fields.v = NULL;
            observer->input->x = 72;
        }
    }
    return result;
}

static bool bed_native_callbacks(bed_fixture *b) {
    bool ok = false;
    mc_world_runtime *w = b->source->worlds[0];
    mc_registry_classes *registry = b->source->registry[0];
    mc_chunk_provider *provider = w->fields.v;
    const mc_chunk_load_callback actual = w->acquisition.load;
    void *actual_owner = w->acquisition.loader;
    int32_t *row = registry->ve_a[0];
    const int32_t old_dz = row[1];
    mc_player_coordinates *position = NULL;
    mc_player_coordinates sentinel = {1, 2, 3}, *out = &sentinel;
    bed_load_observer observer = {.world = w,
                                  .actual = actual,
                                  .actual_owner = actual_owner,
                                  .direction_row = row,
                                  .mode = 1};
    bed_clear(b);
    CHECK(tick_seed(b->source, 0, 2, 0) && bed_cell(b, 31, 64, 8, 26, 0) &&
          bed_cell(b, 30, 63, 4, 1, 0));
    w->acquisition.load = bed_observe_load;
    w->acquisition.loader = &observer;
    CHECK(mc_world_bed_spawn_exit(w, registry, 31, 64, 8, 0, NULL, &position, b->error) ==
              MC_WORLD_STORAGE_OK &&
          observer.calls == 1 && row[1] == 3 && b->error->pass == 1 && position != NULL &&
          position->x == 30 && position->y == 64 && position->z == 4);
    row[1] = old_dz;
    mc_world_bed_spawn_position_free(position);
    position = NULL;
    w->acquisition.load = actual;
    w->acquisition.loader = actual_owner;
    CHECK(tick_seed(b->source, 0, 3, 0) && tick_seed(b->source, 0, 4, 0));
    mc_player_coordinates input = {56, 64, 8};
    observer.mode = 2;
    observer.calls = 0;
    observer.input = &input;
    w->acquisition.load = bed_observe_load;
    w->acquisition.loader = &observer;
    const mc_world_storage_status rebound =
        mc_world_bed_spawn_validate(w, registry, &input, NULL, &out, b->error);
    CHECK(rebound == MC_WORLD_STORAGE_UNSUPPORTED_STATE && input.x == 72 && w->fields.v == NULL &&
          observer.calls == 2 && b->error->provider_identity == (uintptr_t)provider &&
          b->error->provider_calls == 4 && b->error->wrapper_pc == 110 && out == &sentinel &&
          b->error->retained_events == 5 && b->error->events[0].x == 3 &&
          b->error->events[1].x == 4 && b->error->events[2].x == 4 && b->error->events[3].x == 4 &&
          b->error->events[3].completed && !b->error->events[4].completed &&
          b->error->retained_world_count == 2 && w->active_calls == 2 &&
          mc_world_runtime_retire_world(w, NULL) == MC_WORLD_STORAGE_BUSY);
    CHECK(mc_world_bed_spawn_error_retire(b->error) && w->active_calls == 0);
    w->fields.v = provider;
    w->acquisition.load = actual;
    w->acquisition.loader = actual_owner;
    CHECK(tick_seed(b->source, 0, 5, 0));
    observer.mode = 3;
    observer.calls = 0;
    input.x = 88;
    const int32_t key = mc_chunk_cache_key(5, 0);
    bool changed = false;
    CHECK(mc_chunk_pending_add(b->source->providers[0].pending, key, &changed) ==
              MC_CHUNK_ACQUIRE_OK &&
          changed);
    w->acquisition.load = bed_observe_load;
    w->acquisition.loader = &observer;
    const mc_world_storage_status refused =
        mc_world_bed_spawn_validate(w, registry, &input, NULL, &out, b->error);
    CHECK(refused == MC_WORLD_STORAGE_IO_ERROR && observer.calls == 1 && w->fields.v == NULL &&
          b->error->wrapper_pc == 22 && b->error->provider_calls == 1 &&
          b->error->child_kind == MC_WORLD_BED_SPAWN_CHILD_ACQUISITION &&
          b->error->child.acquisition.cause.status == MC_WORLD_STORAGE_IO_ERROR &&
          b->error->retained_world_count == 2 && w->active_calls == 2 &&
          !mc_chunk_pending_contains(b->source->providers[0].pending, key) &&
          mc_world_runtime_retire_world(w, NULL) == MC_WORLD_STORAGE_BUSY && out == &sentinel);
    CHECK(mc_world_bed_spawn_error_retire(b->error) && w->active_calls == 0);
    ok = true;
cleanup:
    row[1] = old_dz;
    w->fields.v = provider;
    w->acquisition.load = actual;
    w->acquisition.loader = actual_owner;
    (void)mc_world_bed_spawn_error_retire(b->error);
    mc_world_bed_spawn_position_free(position);
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    client_application *app = NULL;
    bed_fixture bed = {0};
    char root[8192], resource_root[8192];
    if (argc != 5 || strcmp(argv[1], "bed-spawn") != 0 ||
        !decode_path(argv[2], root, sizeof root) ||
        !decode_path(argv[3], resource_root, sizeof resource_root) || !mc_math_trig_init(&trig))
        return 2;
    f = calloc(1, sizeof(*f));
    app = calloc(1, sizeof(*app));
    bed.error = malloc(sizeof(*bed.error));
    bed.planes = malloc(BED_FOOTPRINT * BED_PLANE_BYTES);
    CHECK(f != NULL && app != NULL && bed.error != NULL && bed.planes != NULL);
    mc_world_bed_spawn_error_init(bed.error);
    CHECK(mc_resource_files_create(resource_root, &app->files) == MC_REGISTRY_OK);
    tick_resources = mc_resource_files_source(app->files);
    CHECK(tick_setup(f, root));
    bed.source = f;
    CHECK(bed_prepare(&bed) && bed_profiles(&bed, argv[4]) && bed_native_arrays_and_limits(&bed) &&
          bed_native_acquisition_limit(&bed) && bed_native_callbacks(&bed));
    result = 0;
cleanup:
    if (bed.error != NULL)
        (void)mc_world_bed_spawn_error_retire(bed.error);
    free(bed.error);
    free(bed.planes);
    if (!tick_cleanup(f))
        return 1;
    if (app != NULL) {
        mc_resource_files_free(app->files);
        free(app);
    }
    if (result == 0 && printf("Bed spawn: %u checks\n", checks) < 0)
        return 1;
    return result;
}
