#include "mc/block/registry/runtime.h"
#include "mc/resources/translation/translation.h"
#include "mc/world/runtime/scheduling.h"

#include "../../../src/block/registry/runtime_internal.h"
#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/world/runtime/chunk_provider_internal.h"
#include "../../../src/world/runtime/construction_internal.h"
#include "../../../src/world/runtime/runtime_internal.h"
#include "../../../src/world/storage/objects_internal.h"
#include "../../support/java_random.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #c);                        \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

/* Native source contracts derived from fd/dj.c(IIIII)V. These are not original
 * observations or complete scheduled-tick processing. All Worlds/queues and
 * loaded Chunk owners are genuine. Explicit native controls below edit only
 * actual flags/nullable references, Chunk bytes and one initialized sequence cell;
 * they never fabricate class latches, cache counts, World phases or owners. */
static unsigned int checks;
static mc_math_trig trig;
static const uint8_t language[] = "stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                                  "stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n";

typedef struct stream {
    const uint8_t *bytes;
    size_t length, offset;
    bool closed;
} stream;

typedef struct resources {
    stream streams[3];
    mc_registry_resources api;
    mc_registry_classes *registry;
    mc_world_runtime *world;
    mc_world_runtime_classes *classes;
    const mc_block *sentinel;
    size_t opens, closes, guarded;
    int64_t expected_next;
    bool fail_first, bad;
} resources;

typedef struct child {
    mc_world_store *store;
    mc_chunk_state *state;
} child;

typedef struct fixture {
    mc_world_regions *regions;
    mc_world_store *stores[7];
    mc_world_info_name *name;
    mc_materials *materials;
    mc_java_random_profile *profile;
    resources resources[2];
    mc_registry_classes *registry[2];
    mc_world_runtime_classes *classes[3];
    mc_world_runtime *worlds[7];
    mc_world_provider *dimensions[7];
    mc_chunk_provider_fields provider[7];
    bool bound[7];
    child children[32];
    mc_chunk_state *center[3];
    mc_scheduled_tick *survivor, *extra;
    mc_scheduled_tick_fields survivor_fields;
} fixture;

typedef struct fixture_record {
    mc_nbt_tag root, level, fields[11];
    const mc_nbt_tag *root_fields[1], *level_fields[11];
    uint16_t names[12][20];
    uint8_t arrays[5][MC_CHUNK_BLOCKS];
} fixture_record;

static size_t cell(int32_t x, int32_t y, int32_t z) {
    return ((size_t)x << 11u) | ((size_t)z << 7u) | (size_t)y;
}

static bool same_cause(const mc_world_storage_error *a, const mc_world_storage_error *b) {
    return a != NULL && b != NULL && a->status == b->status && a->operation == b->operation &&
           a->system_error == b->system_error;
}

static bool same_tick(const mc_scheduled_tick_fields *a, const mc_scheduled_tick_fields *b) {
    return a->x == b->x && a->y == b->y && a->z == b->z && a->block == b->block &&
           a->due_time == b->due_time && a->sequence == b->sequence;
}

static bool counts(mc_world_runtime *w, int32_t e, int32_t f) {
    int32_t actual_e = -1, actual_f = -1;
    return mc_scheduled_ordered_size(w->fields.E, &actual_e) == MC_SCHEDULED_ORDERED_OK &&
           mc_scheduled_membership_size(w->fields.F, &actual_f) == MC_SCHEDULED_MEMBERSHIP_OK &&
           actual_e == e && actual_f == f;
}

static bool remember(fixture *f, size_t world, mc_chunk_state *state) {
    if (state == NULL || !mc_world_store_owns_chunk(f->stores[world], state))
        return false;
    for (size_t i = 0; i < COUNT(f->children); ++i) {
        if (f->children[i].state == state)
            return f->children[i].store == f->stores[world];
        if (f->children[i].state == NULL) {
            f->children[i] = (child){f->stores[world], state};
            return true;
        }
    }
    return false;
}

static mc_java_properties_status read_bytes(void *context, uint8_t *out, size_t capacity,
                                            size_t *count) {
    stream *s = context;
    if (s == NULL || out == NULL || count == NULL || capacity == 0 || s->closed ||
        s->offset > s->length)
        return MC_JAVA_PROPERTIES_IO_ERROR;
    size_t take = s->length - s->offset;
    if (take > 17)
        take = 17;
    if (take > capacity)
        take = capacity;
    if (take != 0)
        memcpy(out, s->bytes + s->offset, take);
    s->offset += take;
    *count = take;
    return MC_JAVA_PROPERTIES_OK;
}

static mc_registry_status open_resource(void *context, const char *name,
                                        mc_java_properties_input *out) {
    resources *r = context;
    const char *const names[] = {"/lang/en_US.lang", "/lang/stats_US.lang", "/achievement/map.txt"};
    if (name == NULL || out == NULL || r->opens >= COUNT(names) ||
        strcmp(name, names[r->opens]) != 0) {
        r->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    const size_t index = r->opens++;
    if (r->fail_first) {
        mc_world_schedule_class_state sequence;
        const mc_block *block = r->sentinel;
        mc_registry_error error;
        if (index != 0 || r->world == NULL || r->world->active_calls == 0 ||
            mc_world_runtime_schedule_class_get(r->classes, &sequence) != MC_WORLD_STORAGE_OK ||
            !sequence.initialized || sequence.next != r->expected_next ||
            mc_world_runtime_retire_world(r->world, NULL) != MC_WORLD_STORAGE_BUSY ||
            mc_world_runtime_free(r->world, NULL) != MC_WORLD_STORAGE_BUSY ||
            mc_registry_block_get(r->registry, 1, &block, &error) != MC_REGISTRY_BUSY ||
            block != r->sentinel || mc_registry_classes_free(r->registry, NULL) != MC_REGISTRY_BUSY)
            r->bad = true;
        ++r->guarded;
        return MC_REGISTRY_IO_FAILED;
    }
    *out = (mc_java_properties_input){&r->streams[index], read_bytes};
    return MC_REGISTRY_OK;
}

static mc_registry_status close_resource(void *context, const mc_java_properties_input *input) {
    resources *r = context;
    if (input == NULL || input->context != &r->streams[2] || input->read != read_bytes) {
        r->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    r->streams[2].closed = true;
    ++r->closes;
    return MC_REGISTRY_OK;
}

static bool resource_ready(void *context, const mc_java_properties_input *input) {
    resources *r = context;
    if (input == NULL || input->read != read_bytes)
        r->bad = true;
    return false;
}

static bool open_store(fixture *f, size_t slot, const char *root) {
    bool ok = false;
    char path[8192];
    uint8_t map[256];
    const int n = snprintf(path, sizeof(path), "%s/world%zu", root, slot);
    CHECK(n > 0 && (size_t)n < sizeof(path));
    for (size_t i = 0; i < COUNT(map); ++i)
        map[i] = (uint8_t)i;
    mc_world_store_options options = {.session_token = 127,
                                      .block_map = map,
                                      .block_map_length = sizeof(map),
                                      .limits = mc_world_store_default_limits()};
    options.limits.max_loaded_chunks = 8;
    CHECK(mc_world_store_open(path, f->regions, &options, &f->stores[slot], NULL) ==
          MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    return ok;
}

static bool create_world(fixture *f, size_t slot, size_t class_index, size_t entries) {
    bool ok = false;
    mc_world_runtime_construction state;
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 8;
    limits.max_scheduled_entries = entries;
    CHECK(mc_world_provider_create(0, NULL, &f->dimensions[slot], NULL) == MC_WORLD_PROVIDER_OK);
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA,
                                          f->classes[class_index], &limits, &f->worlds[slot],
                                          NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments args = {
        .store = f->stores[slot], .name = f->name, .seed = 23, .dimension = f->dimensions[slot]};
    CHECK(mc_world_runtime_construct_world(f->worlds[slot], &args, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_construction_get(f->worlds[slot], &state) == MC_WORLD_STORAGE_OK &&
          state.phase == MC_WORLD_RUNTIME_READY && state.contexts_bound && state.cache_registered);
    CHECK(mc_chunk_provider_bind_fields(f->worlds[slot]->fields.v, &f->provider[slot], NULL) ==
          MC_WORLD_STORAGE_OK);
    f->bound[slot] = true;
    CHECK(counts(f->worlds[slot], 0, 0) && f->provider[slot].cache->count == 0);
    ok = true;
cleanup:
    return ok;
}

static mc_nbt_string ascii(const char *text, uint16_t *units) {
    const size_t n = strlen(text);
    for (size_t i = 0; i < n; ++i)
        units[i] = (uint16_t)(unsigned char)text[i];
    return (mc_nbt_string){units, n};
}

static bool seed_record(fixture *f, size_t world, int32_t x, int32_t z) {
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
    if (x == 0 && z == 0)
        r->arrays[0][cell(8, 64, 8)] = 1;
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
    r->root.value.compound.length = COUNT(r->root_fields);
    CHECK(mc_nbt_measure(&r->root, NULL, &length, NULL) == MC_NBT_OK && length != 0);
    encoded = malloc(length);
    CHECK(encoded != NULL &&
          mc_nbt_write(&r->root, NULL, encoded, length, &written, NULL) == MC_NBT_OK &&
          written == length);
    uint32_t growth = 0;
    /* Native stored-input fixture timestamp, never a substituted GAME clock. */
    CHECK(mc_world_regions_write(f->regions, mc_world_store_root(f->stores[world]), x, z, encoded,
                                 length, UINT32_C(0xfedcba98), &growth,
                                 NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    free(encoded);
    free(r);
    return ok;
}

static bool load_square(fixture *f, size_t world) {
    bool ok = false;
    for (int32_t x = 0; x < 2; ++x)
        for (int32_t z = 0; z < 2; ++z)
            CHECK(seed_record(f, world, x, z));
    for (int32_t x = 0; x < 2; ++x) {
        for (int32_t z = 0; z < 2; ++z) {
            mc_chunk_state *state = NULL;
            const mc_chunk_acquire_status status =
                mc_world_runtime_get(f->worlds[world], x, z, &state);
            /* Track actual published owners even if a later acquisition stage
             * failed, so teardown never manually erases cache membership. */
            mc_chunk_cache *cache = f->provider[world].cache;
            for (size_t i = 0; i < cache->insertion_count; ++i)
                CHECK(remember(f, world, cache->insertions[i]));
            CHECK(status == MC_CHUNK_ACQUIRE_OK && state != NULL && state->active &&
                  !state->dirty && state->terrain_populated && state->x == x && state->z == z &&
                  mc_world_store_chunk_object(f->stores[world], state) != NULL);
            if (x == 0 && z == 0)
                f->center[world] = state;
        }
    }
    CHECK(f->provider[world].cache->count == 4 && f->provider[world].cache->insertion_count == 4 &&
          f->center[world]->data->blocks[cell(8, 64, 8)] == 1);
    ok = true;
cleanup:
    return ok;
}

static bool invoke(mc_world_runtime *w, int32_t x, int32_t y, int32_t z, int32_t block,
                   int32_t delay, mc_world_storage_status expected,
                   mc_world_schedule_error *error) {
    bool ok = false;
    mc_world_schedule_class_state before, after;
    CHECK(mc_world_runtime_schedule_class_get(w->classes, &before) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_schedule(w, x, y, z, block, delay, error) == expected);
    CHECK(mc_world_runtime_schedule_class_get(w->classes, &after) == MC_WORLD_STORAGE_OK &&
          after.initialized &&
          after.next == mc_java_i64_add(before.initialized ? before.next : 0, 1));
    ok = true;
cleanup:
    return ok;
}

static bool lazy_and_deferred(fixture *f) {
    bool ok = false, controlled = false;
    mc_world_runtime *w = f->worlds[0];
    mc_world_info *saved_info = w->fields.x;
    mc_java_random *saved_random = w->fields.r;
    const mc_java_random random = *saved_random;
    const bool saved_b = w->fields.B, saved_o = w->fields.o;
    mc_world_schedule_class_state sequence;
    mc_world_schedule_error error;
    mc_registry_block_view view;
    CHECK(mc_world_runtime_schedule_class_get(f->classes[0], &sequence) == MC_WORLD_STORAGE_OK &&
          !sequence.initialized && sequence.next == 0);
    CHECK(invoke(w, 8, 64, 8, 1, 3, MC_WORLD_STORAGE_OK, &error) && counts(w, 0, 0));
    const int32_t y_values[] = {-9, 136, INT32_MIN, INT32_MAX};
    for (size_t i = 0; i < COUNT(y_values); ++i)
        CHECK(invoke(w, 8, y_values[i], 8, INT32_MAX, INT32_MIN, MC_WORLD_STORAGE_OK, &error) &&
              counts(w, 0, 0));
    CHECK(f->resources[0].opens == 0 && f->provider[0].cache->count == 0 &&
          mc_world_store_chunk_count(f->stores[0]) == 0);
    CHECK(load_square(f, 0));
    CHECK(f->resources[0].opens == 0);
    mc_scheduled_ordered *const e = w->fields.E;
    mc_scheduled_membership *const membership = w->fields.F;
    const uint64_t u = atomic_load_explicit(&f->profile->uniquifier, memory_order_relaxed);
    /* Explicit native reached-field controls. Keep real borrowed owners live;
     * restore pointers/flags on EVERY cleanup path, without releasing them. */
    w->fields.x = NULL;
    w->fields.r = NULL;
    w->fields.B = true;
    w->fields.o = true;
    controlled = true;
    CHECK(invoke(w, 8, 64, 8, 0, INT32_MAX, MC_WORLD_STORAGE_OK, &error) && counts(w, 1, 1));
    CHECK(invoke(w, 8, 64, 8, INT32_MIN, INT32_MIN, MC_WORLD_STORAGE_OK, &error) &&
          counts(w, 2, 2));
    CHECK(invoke(w, INT32_MAX, 64, INT32_MAX, 1, 1, MC_WORLD_STORAGE_UNSUPPORTED_STATE, &error) &&
          error.bytecode == 191 && error.registry.status == MC_REGISTRY_OK &&
          error.membership == MC_SCHEDULED_MEMBERSHIP_OK &&
          error.ordered == MC_SCHEDULED_ORDERED_OK && counts(w, 2, 2) &&
          same_cause(&error.cause, mc_world_runtime_last_error(w)));
    CHECK(w->fields.E == e && w->fields.F == membership && f->resources[0].opens == 0 &&
          mc_test_java_random_equal(saved_random, &random) &&
          atomic_load_explicit(&f->profile->uniquifier, memory_order_relaxed) == u);
    w->fields.x = saved_info;
    w->fields.r = saved_random;
    w->fields.B = saved_b;
    w->fields.o = saved_o;
    controlled = false;
    CHECK(mc_registry_classes_inspect_blocks(f->registry[0], &view, NULL) == MC_REGISTRY_OK &&
          view.phase == MC_REGISTRY_COLD);
    mc_scheduled_tick *first = NULL;
    CHECK(mc_scheduled_ordered_first(e, &first) == MC_SCHEDULED_ORDERED_OK);
    mc_scheduled_tick_fields fields;
    const bool read = mc_scheduled_tick_fields_get(first, &fields) == MC_SCHEDULED_TICK_OK;
    mc_scheduled_tick_release(first);
    CHECK(read && fields.x == 8 && fields.y == 64 && fields.z == 8 && fields.block == 0 &&
          fields.due_time == 0 && fields.sequence == 5);
    CHECK(invoke(w, 40, 64, 8, 1, 3, MC_WORLD_STORAGE_OK, NULL) && counts(w, 2, 2));
    ok = true;
cleanup:
    if (controlled) {
        w->fields.x = saved_info;
        w->fields.r = saved_random;
        w->fields.B = saved_b;
        w->fields.o = saved_o;
    }
    return ok;
}

static bool time_identity_and_classes(fixture *f) {
    bool ok = false;
    mc_world_schedule_error error;
    mc_world_schedule_class_state before, after, other;
    mc_scheduled_tick_fields fields;
    mc_world_runtime *w = f->worlds[0];
    CHECK(mc_world_info_time_set(w->fields.x, INT64_MAX) == MC_WORLD_INFO_OK);
    CHECK(mc_world_runtime_schedule_class_get(f->classes[0], &before) == MC_WORLD_STORAGE_OK);
    const mc_world_storage_error old_cause = *mc_world_runtime_last_error(w);
    CHECK(invoke(w, INT32_MAX, -8, INT32_MIN, INT32_MAX, 1, MC_WORLD_STORAGE_OK, &error));
    CHECK(mc_scheduled_ordered_first(w->fields.E, &f->survivor) == MC_SCHEDULED_ORDERED_OK &&
          mc_scheduled_tick_fields_get(f->survivor, &f->survivor_fields) == MC_SCHEDULED_TICK_OK);
    fields = f->survivor_fields;
    CHECK(fields.x == INT32_MAX && fields.y == -8 && fields.z == INT32_MIN &&
          fields.block == INT32_MAX && fields.due_time == INT64_MIN &&
          fields.sequence == before.next && counts(w, 3, 3) &&
          same_cause(&old_cause, mc_world_runtime_last_error(w)));
    /* Explicit native lower-owner alias probe: shifting the real F must change
     * the SAME retained E/externally-held record, not an equal detached copy.
     * Both actual API shifts complete before ordinary scheduler tests resume. */
    CHECK(mc_scheduled_membership_shift_time(w->fields.F, 7) == MC_SCHEDULED_MEMBERSHIP_OK);
    CHECK(mc_scheduled_tick_fields_get(f->survivor, &fields) == MC_SCHEDULED_TICK_OK &&
          fields.due_time == INT64_MIN + 7 && fields.sequence == f->survivor_fields.sequence);
    CHECK(mc_scheduled_membership_shift_time(w->fields.F, -7) == MC_SCHEDULED_MEMBERSHIP_OK);
    CHECK(mc_scheduled_tick_fields_get(f->survivor, &fields) == MC_SCHEDULED_TICK_OK &&
          same_tick(&fields, &f->survivor_fields));
    CHECK(mc_world_info_time_set(w->fields.x, 55) == MC_WORLD_INFO_OK);
    CHECK(invoke(w, INT32_MAX, -8, INT32_MIN, INT32_MAX, -7, MC_WORLD_STORAGE_OK, &error) &&
          counts(w, 3, 3));
    mc_scheduled_tick *same = NULL;
    CHECK(mc_scheduled_ordered_first(w->fields.E, &same) == MC_SCHEDULED_ORDERED_OK);
    const bool unchanged = same == f->survivor &&
                           mc_scheduled_tick_fields_get(same, &fields) == MC_SCHEDULED_TICK_OK &&
                           same_tick(&fields, &f->survivor_fields);
    mc_scheduled_tick_release(same);
    CHECK(unchanged);
    /* The source reads Info BEFORE duplicate contains205. A duplicate with a
     * missing reached Info must still fail191, consume sequence, and preserve
     * its old queues/identity. Restore the explicit reference control first. */
    mc_world_info *const saved_info = w->fields.x;
    w->fields.x = NULL;
    const bool duplicate_time_failure = invoke(w, INT32_MAX, -8, INT32_MIN, INT32_MAX, -7,
                                               MC_WORLD_STORAGE_UNSUPPORTED_STATE, &error);
    w->fields.x = saved_info;
    CHECK(duplicate_time_failure && error.bytecode == 191 && counts(w, 3, 3));
    CHECK(mc_scheduled_tick_fields_get(f->survivor, &fields) == MC_SCHEDULED_TICK_OK &&
          same_tick(&fields, &f->survivor_fields));
    CHECK(mc_world_runtime_schedule_class_get(f->classes[0], &before) == MC_WORLD_STORAGE_OK);
    CHECK(create_world(f, 3, 0, 16));
    CHECK(mc_world_runtime_schedule_class_get(f->classes[0], &after) == MC_WORLD_STORAGE_OK &&
          after.initialized && after.next == before.next && f->worlds[3]->fields.E != w->fields.E &&
          f->worlds[3]->fields.F != w->fields.F);
    CHECK(mc_world_info_time_set(f->worlds[3]->fields.x, INT64_MIN) == MC_WORLD_INFO_OK);
    CHECK(invoke(f->worlds[3], INT32_MAX, 135, INT32_MAX, 257, -1, MC_WORLD_STORAGE_OK, &error));
    mc_scheduled_tick *tick = NULL;
    CHECK(mc_scheduled_ordered_first(f->worlds[3]->fields.E, &tick) == MC_SCHEDULED_ORDERED_OK);
    const bool underflow = mc_scheduled_tick_fields_get(tick, &fields) == MC_SCHEDULED_TICK_OK &&
                           fields.x == INT32_MAX && fields.y == 135 && fields.z == INT32_MAX &&
                           fields.block == 257 && fields.due_time == INT64_MAX &&
                           fields.sequence == before.next;
    mc_scheduled_tick_release(tick);
    CHECK(underflow);
    CHECK(mc_world_runtime_schedule_class_get(f->classes[0], &before) == MC_WORLD_STORAGE_OK);
    CHECK(create_world(f, 6, 0, 16));
    CHECK(mc_world_runtime_schedule_class_get(f->classes[0], &after) == MC_WORLD_STORAGE_OK &&
          after.next == before.next);
    CHECK(mc_world_info_time_set(f->worlds[6]->fields.x, 0) == MC_WORLD_INFO_OK);
    CHECK(
        invoke(f->worlds[6], INT32_MAX, 64, INT32_MAX, 1, INT32_MIN, MC_WORLD_STORAGE_OK, &error));
    tick = NULL;
    CHECK(mc_scheduled_ordered_first(f->worlds[6]->fields.E, &tick) == MC_SCHEDULED_ORDERED_OK);
    const bool signed_delay = mc_scheduled_tick_fields_get(tick, &fields) == MC_SCHEDULED_TICK_OK &&
                              fields.due_time == INT64_C(-2147483648) &&
                              fields.sequence == before.next;
    mc_scheduled_tick_release(tick);
    CHECK(signed_delay);
    CHECK(mc_world_runtime_schedule_class_get(f->classes[2], &other) == MC_WORLD_STORAGE_OK &&
          !other.initialized && other.next == 0);
    CHECK(mc_world_runtime_schedule_class_get(f->classes[0], &before) == MC_WORLD_STORAGE_OK);
    CHECK(invoke(f->worlds[2], 8, 64, 8, 1, 1, MC_WORLD_STORAGE_OK, &error));
    CHECK(mc_world_runtime_schedule_class_get(f->classes[2], &other) == MC_WORLD_STORAGE_OK &&
          other.initialized && other.next == 1 && counts(f->worlds[2], 0, 0));
    CHECK(mc_world_runtime_schedule_class_get(f->classes[0], &after) == MC_WORLD_STORAGE_OK &&
          after.next == before.next && f->resources[0].opens == 0);
    ok = true;
cleanup:
    return ok;
}

static bool immediate(fixture *f) {
    bool ok = false, controlled = false;
    mc_world_runtime *w = f->worlds[0];
    mc_world_info *saved_info = w->fields.x;
    mc_java_random *saved_random = w->fields.r;
    const mc_java_random random = *saved_random;
    const bool saved_a = w->fields.a;
    mc_scheduled_ordered *const ordered = w->fields.E;
    mc_scheduled_membership *const membership = w->fields.F;
    mc_world_schedule_error error;
    const uint64_t u = atomic_load_explicit(&f->profile->uniquifier, memory_order_relaxed);
    w->fields.a = true;
    w->fields.x = NULL;
    w->fields.r = NULL;
    controlled = true;
    CHECK(invoke(w, 40, 64, 8, 1, INT32_MIN, MC_WORLD_STORAGE_OK, &error));
    CHECK(invoke(w, 8, 64, 8, 2, INT32_MAX, MC_WORLD_STORAGE_OK, &error));
    CHECK(invoke(w, 9, 64, 8, 0, INT32_MAX, MC_WORLD_STORAGE_OK, &error));
    CHECK(invoke(w, 8, -1, 8, 1, INT32_MAX, MC_WORLD_STORAGE_OK, &error));
    CHECK(counts(w, 3, 3) && f->resources[0].opens == 0 &&
          atomic_load_explicit(&f->profile->uniquifier, memory_order_relaxed) == u);
    CHECK(mc_registry_classes_initialize_blocks(f->registry[0], NULL) == MC_REGISTRY_OK &&
          f->resources[0].opens == 3 && f->resources[0].closes == 1);
    const mc_world_storage_error previous = *mc_world_runtime_last_error(w);
    CHECK(invoke(w, 8, 64, 8, 1, INT32_MIN, MC_WORLD_STORAGE_OK, &error) && counts(w, 3, 3) &&
          mc_test_java_random_equal(saved_random, &random) &&
          same_cause(&previous, mc_world_runtime_last_error(w)));
    /* Actual cached array-content controls choose real existing registry
     * receivers. No registry element/pointer/phase or owner count is changed. */
    f->center[0]->data->blocks[cell(8, 64, 8)] = 255;
    CHECK(invoke(w, 8, 64, 8, 255, 0, MC_WORLD_STORAGE_UNSUPPORTED_STATE, &error) &&
          error.bytecode == 142 && error.registry.status == MC_REGISTRY_OK &&
          error.tick.hook_class == NULL && error.tick.hook_bytecode == 0 && counts(w, 3, 3) &&
          error.tick.operation != NULL &&
          strcmp(error.tick.operation, "Block tick receiver is null") == 0 &&
          f->center[0]->data->blocks[cell(8, 64, 8)] == 255 && !f->center[0]->dirty &&
          same_cause(&error.cause, mc_world_runtime_last_error(w)));
    f->center[0]->data->blocks[cell(8, 64, 8)] = 12;
    /* A real solid support makes the now-implemented falling body return
     * normally before a flag/presence/mutation call, even with Random NULL. */
    f->center[0]->data->blocks[cell(8, 63, 8)] = 1;
    CHECK(invoke(w, 8, 64, 8, 12, 0, MC_WORLD_STORAGE_OK, &error) && error.bytecode == 0 &&
          error.registry.status == MC_REGISTRY_OK && error.tick.status == MC_WORLD_STORAGE_OK &&
          error.tick.hook_class == NULL && counts(w, 3, 3) &&
          f->center[0]->data->blocks[cell(8, 64, 8)] == 12 && !f->center[0]->dirty);
    CHECK(mc_test_java_random_equal(saved_random, &random) && w->fields.a &&
          w->fields.E == ordered && w->fields.F == membership && f->resources[0].opens == 3);
    ok = true;
cleanup:
    if (controlled) {
        f->center[0]->data->blocks[cell(8, 64, 8)] = 1;
        f->center[0]->data->blocks[cell(8, 63, 8)] = 0;
        w->fields.a = saved_a;
        w->fields.x = saved_info;
        w->fields.r = saved_random;
    }
    return ok;
}

static bool missing_and_cold_registry(fixture *f) {
    bool ok = false, controlled = false;
    mc_world_runtime *w = f->worlds[2];
    mc_world_info *info = w->fields.x;
    mc_java_random *random = w->fields.r;
    mc_world_schedule_error error;
    CHECK(load_square(f, 2));
    w->fields.a = true;
    w->fields.x = NULL;
    w->fields.r = NULL;
    controlled = true;
    CHECK(invoke(w, 8, 64, 8, 1, 0, MC_WORLD_STORAGE_UNSUPPORTED_STATE, &error) &&
          error.bytecode == 116 && error.registry.status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY &&
          error.registry.constructor_class == NULL && error.registry.constructor_pc == -1 &&
          counts(w, 0, 0) && same_cause(&error.cause, mc_world_runtime_last_error(w)));
    w->fields.a = false;
    w->fields.x = info;
    w->fields.r = random;
    controlled = false;
    CHECK(load_square(f, 1));
    w = f->worlds[1];
    info = w->fields.x;
    random = w->fields.r;
    w->fields.a = true;
    w->fields.x = NULL;
    w->fields.r = NULL;
    controlled = true;
    resources *r = &f->resources[1];
    r->world = w;
    r->classes = f->classes[1];
    CHECK(mc_registry_block_get(f->registry[0], 1, &r->sentinel, NULL) == MC_REGISTRY_OK &&
          r->sentinel != NULL);
    r->expected_next = 1;
    CHECK(invoke(w, 8, 64, 8, 1, 0, MC_WORLD_STORAGE_UNSUPPORTED_STATE, &error) &&
          error.bytecode == 116 && error.registry.status == MC_REGISTRY_IO_FAILED &&
          error.registry.constructor_pc == 23 && error.registry.active_initializer_pc == 4 &&
          error.registry.active_initializer_class != NULL &&
          strcmp(error.registry.active_initializer_class, "nh") == 0 && r->opens == 1 &&
          r->guarded == 1 && !r->bad && counts(w, 0, 0));
    CHECK(invoke(w, 8, 64, 8, 1, 0, MC_WORLD_STORAGE_UNSUPPORTED_STATE, &error) &&
          error.bytecode == 116 && error.registry.status == MC_REGISTRY_FAILED_INITIALIZATION &&
          error.registry.constructor_pc == 23 && r->opens == 1 && counts(w, 0, 0));
    const mc_world_storage_error previous = *mc_world_runtime_last_error(w);
    CHECK(invoke(w, 8, 64, 8, 2, INT32_MIN, MC_WORLD_STORAGE_OK, &error) && r->opens == 1 &&
          counts(w, 0, 0) && same_cause(&previous, mc_world_runtime_last_error(w)));
    ok = true;
cleanup:
    if (controlled) {
        w->fields.a = false;
        w->fields.x = info;
        w->fields.r = random;
    }
    return ok;
}

static bool collection_failures(fixture *f) {
    bool ok = false;
    mc_world_schedule_error error;
    size_t f_before = 0, f_after = 0, e_before = 0, e_after = 0;
    CHECK(create_world(f, 4, 0, 0));
    mc_world_runtime *w = f->worlds[4];
    CHECK(mc_scheduled_membership_native_bytes(w->fields.F, &f_before) ==
          MC_SCHEDULED_MEMBERSHIP_OK);
    CHECK(invoke(w, INT32_MAX, 64, INT32_MAX, 0, 0, MC_WORLD_STORAGE_LIMIT, &error) &&
          error.bytecode == 219 && error.membership == MC_SCHEDULED_MEMBERSHIP_LIMIT &&
          error.ordered == MC_SCHEDULED_ORDERED_OK && counts(w, 0, 0));
    CHECK(mc_scheduled_membership_native_bytes(w->fields.F, &f_after) ==
              MC_SCHEDULED_MEMBERSHIP_OK &&
          f_after == f_before && same_cause(&error.cause, mc_world_runtime_last_error(w)));
    const mc_world_storage_error saved = *mc_world_runtime_last_error(w);
    mc_world_schedule_class_state before, after;
    CHECK(mc_world_runtime_schedule_class_get(f->classes[0], &before) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_retire_world(w, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_schedule(w, 8, 64, 8, 1, 0, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.bytecode == 0 && same_cause(&saved, mc_world_runtime_last_error(w)));
    CHECK(mc_world_runtime_schedule(NULL, 8, 64, 8, 1, 0, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          error.bytecode == 0);
    CHECK(mc_world_runtime_schedule_class_get(f->classes[0], &after) == MC_WORLD_STORAGE_OK &&
          after.next == before.next);
    CHECK(create_world(f, 5, 0, 1));
    w = f->worlds[5];
    mc_scheduled_ordered *const ordered = w->fields.E;
    mc_scheduled_membership *const membership = w->fields.F;
    mc_scheduled_tick_sequence independent = {9000};
    CHECK(mc_scheduled_tick_create(&independent, 17, 2, 19, 0, &f->extra) == MC_SCHEDULED_TICK_OK);
    bool added = false;
    /* Explicit native lower-owner preparation: one genuine E node, F empty.
     * No count/opaque-layout write, owner rebind or fictional Java failure. */
    CHECK(mc_scheduled_ordered_add(ordered, f->extra, &added) == MC_SCHEDULED_ORDERED_OK && added &&
          counts(w, 1, 0));
    CHECK(mc_scheduled_membership_native_bytes(membership, &f_before) ==
              MC_SCHEDULED_MEMBERSHIP_OK &&
          mc_scheduled_ordered_native_bytes(ordered, &e_before) == MC_SCHEDULED_ORDERED_OK);
    CHECK(invoke(w, INT32_MAX, 64, INT32_MAX, 0, INT32_MIN, MC_WORLD_STORAGE_LIMIT, &error) &&
          error.bytecode == 231 && error.membership == MC_SCHEDULED_MEMBERSHIP_OK &&
          error.ordered == MC_SCHEDULED_ORDERED_LIMIT && counts(w, 1, 1));
    const mc_world_storage_error limited = *mc_world_runtime_last_error(w);
    CHECK(mc_scheduled_membership_native_bytes(membership, &f_after) ==
              MC_SCHEDULED_MEMBERSHIP_OK &&
          f_after > f_before &&
          mc_scheduled_ordered_native_bytes(ordered, &e_after) == MC_SCHEDULED_ORDERED_OK &&
          e_after == e_before && w->fields.E == ordered && w->fields.F == membership);
    mc_scheduled_tick *query = NULL;
    CHECK(mc_scheduled_tick_create(&independent, INT32_MAX, 64, INT32_MAX, 0, &query) ==
          MC_SCHEDULED_TICK_OK);
    bool present = false;
    const bool contains = mc_scheduled_membership_contains(membership, query, &present) ==
                              MC_SCHEDULED_MEMBERSHIP_OK &&
                          present;
    mc_scheduled_tick_release(query);
    CHECK(contains);
    CHECK(invoke(w, INT32_MAX, 64, INT32_MAX, 0, INT32_MAX, MC_WORLD_STORAGE_OK, &error) &&
          counts(w, 1, 1) && same_cause(&limited, mc_world_runtime_last_error(w)));
    CHECK(mc_world_runtime_retire_world(w, NULL) == MC_WORLD_STORAGE_OK && counts(w, 1, 1));
    CHECK(mc_world_runtime_free(w, NULL) == MC_WORLD_STORAGE_OK);
    f->worlds[5] = NULL;
    f->bound[5] = false;
    mc_scheduled_tick_fields fields;
    CHECK(mc_scheduled_tick_fields_get(f->extra, &fields) == MC_SCHEDULED_TICK_OK &&
          fields.x == 17 && fields.y == 2 && fields.z == 19 && fields.block == 0 &&
          fields.due_time == 0 && fields.sequence == 9000);
    mc_scheduled_tick_release(f->extra);
    f->extra = NULL;
    ok = true;
cleanup:
    return ok;
}

static bool sequence_wrap(fixture *f) {
    bool ok = false;
    mc_world_runtime *w = f->worlds[3];
    mc_world_schedule_error error;
    mc_world_schedule_class_state state;
    CHECK(mc_world_runtime_schedule_class_get(f->classes[0], &state) == MC_WORLD_STORAGE_OK &&
          state.initialized);
    /* SYNTHETIC native edge input: change only the actual already-initialized
     * sequence cell. No initialized latch is forged, and later calls perform
     * real record increments. Later calls keep using that wrapped sequence. */
    f->classes[0]->scheduled_sequence.next = INT64_MAX;
    CHECK(invoke(w, INT32_MAX, 64, INT32_MAX, 0, 0, MC_WORLD_STORAGE_OK, &error));
    CHECK(mc_world_runtime_schedule_class_get(f->classes[0], &state) == MC_WORLD_STORAGE_OK &&
          state.next == INT64_MIN);
    CHECK(invoke(w, INT32_MAX, 65, INT32_MAX, 0, 0, MC_WORLD_STORAGE_OK, &error));
    CHECK(mc_world_runtime_schedule_class_get(f->classes[0], &state) == MC_WORLD_STORAGE_OK &&
          state.next == INT64_MIN + 1);
    mc_scheduled_tick *first = NULL;
    CHECK(mc_scheduled_ordered_first(w->fields.E, &first) == MC_SCHEDULED_ORDERED_OK);
    mc_scheduled_tick_fields fields;
    const bool wrapped_order =
        mc_scheduled_tick_fields_get(first, &fields) == MC_SCHEDULED_TICK_OK && fields.y == 65 &&
        fields.block == 0 && fields.due_time == 0 && fields.sequence == INT64_MIN;
    mc_scheduled_tick_release(first);
    CHECK(wrapped_order && counts(w, 3, 3));
    ok = true;
cleanup:
    return ok;
}

static bool drain(fixture *f, size_t world) {
    if (!f->bound[world])
        return true;
    mc_chunk_cache *cache = f->provider[world].cache;
    mc_chunk_pending *pending = f->provider[world].pending;
    if (!mc_chunk_cache_validate(cache) || !mc_chunk_pending_validate(pending))
        return false;
    for (size_t i = 0; i < cache->insertion_count; ++i)
        if (!remember(f, world, cache->insertions[i]))
            return false;
    for (size_t i = 0; i < cache->count; ++i) {
        bool changed = false;
        if (mc_chunk_pending_add(pending, cache->entries[i].key, &changed) != MC_CHUNK_ACQUIRE_OK)
            return false;
    }
    while (pending->count != 0) {
        const size_t before = pending->count;
        bool result = true;
        if (mc_world_runtime_maintain(f->worlds[world], &result) != MC_CHUNK_LIFECYCLE_OK ||
            result || pending->count >= before)
            return false;
    }
    return cache->count == 0 && cache->insertion_count == 0;
}

static bool raw_info_time(fixture *f) {
    bool ok = false;
    mc_world_runtime *world = f->worlds[6];
    mc_world_info *const saved_info = world->fields.x;
    mc_world_info *raw = NULL;
    mc_scheduled_tick *old = NULL, *current = NULL;
    mc_world_info_state info_state;
    mc_world_schedule_class_state before;
    mc_world_schedule_error error;
    mc_scheduled_tick_fields fields;
    bool removed = false;
    CHECK(mc_world_info_allocate(MC_WORLD_INFO_CLIENT, &raw) == MC_WORLD_INFO_OK);
    CHECK(mc_scheduled_ordered_first(world->fields.E, &old) == MC_SCHEDULED_ORDERED_OK);
    CHECK(mc_scheduled_ordered_remove(world->fields.E, old, &removed) == MC_SCHEDULED_ORDERED_OK &&
          removed);
    CHECK(mc_scheduled_membership_remove(world->fields.F, old, &removed) ==
              MC_SCHEDULED_MEMBERSHIP_OK &&
          removed && counts(world, 0, 0));
    CHECK(mc_world_runtime_schedule_class_get(world->classes, &before) == MC_WORLD_STORAGE_OK);
    /* Explicit native reference preparation. The actual allocated RAW Info
     * still has default time zero; no phase/latch or borrowed owner is forged.
     * The source getter at191 must read that field without a READY predicate. */
    world->fields.x = raw;
    const bool scheduled =
        invoke(world, INT32_MAX, 64, INT32_MAX, 1, 17, MC_WORLD_STORAGE_OK, &error);
    world->fields.x = saved_info;
    CHECK(scheduled && counts(world, 1, 1));
    CHECK(mc_world_info_state_get(raw, &info_state) == MC_WORLD_INFO_OK &&
          info_state.phase == MC_WORLD_INFO_RAW && info_state.last_store_pc == 0);
    CHECK(mc_scheduled_ordered_first(world->fields.E, &current) == MC_SCHEDULED_ORDERED_OK);
    CHECK(mc_scheduled_tick_fields_get(current, &fields) == MC_SCHEDULED_TICK_OK &&
          fields.x == INT32_MAX && fields.y == 64 && fields.z == INT32_MAX && fields.block == 1 &&
          fields.due_time == 17 && fields.sequence == before.next);
    ok = true;
cleanup:
    world->fields.x = saved_info;
    mc_scheduled_tick_release(old);
    mc_scheduled_tick_release(current);
    mc_world_info_release(raw);
    return ok;
}

static bool maintained_and_retired(fixture *f) {
    bool ok = false;
    for (size_t i = 0; i < 3; ++i) {
        CHECK(mc_world_info_time_set(f->worlds[i]->fields.x, 800 + (int64_t)i) == MC_WORLD_INFO_OK);
        CHECK(drain(f, i));
        for (size_t j = 0; j < COUNT(f->children); ++j)
            if (f->children[j].store == f->stores[i])
                CHECK(!f->children[j].state->active &&
                      f->children[j].state->last_save_time == 800 + (int64_t)i);
    }
    for (size_t i = 0; i < COUNT(f->children); ++i) {
        if (f->children[i].state != NULL) {
            for (size_t j = 0; j < COUNT(f->center); ++j)
                if (f->center[j] == f->children[i].state)
                    f->center[j] = NULL;
            CHECK(mc_world_store_retire_chunk(f->children[i].store, f->children[i].state, NULL) ==
                  MC_WORLD_STORAGE_OK);
            f->children[i] = (child){0};
        }
    }
    CHECK(counts(f->worlds[0], 3, 3));
    CHECK(mc_world_runtime_retire_world(f->worlds[0], NULL) == MC_WORLD_STORAGE_OK &&
          counts(f->worlds[0], 3, 3));
    CHECK(mc_world_runtime_free(f->worlds[0], NULL) == MC_WORLD_STORAGE_OK);
    f->worlds[0] = NULL;
    f->bound[0] = false;
    mc_scheduled_tick_fields fields;
    CHECK(mc_scheduled_tick_fields_get(f->survivor, &fields) == MC_SCHEDULED_TICK_OK &&
          same_tick(&fields, &f->survivor_fields));
    mc_scheduled_tick_release(f->survivor);
    f->survivor = NULL;
    ok = true;
cleanup:
    return ok;
}

static bool cleanup_fixture(fixture *f) {
    if (f == NULL)
        return true;
    for (size_t i = 0; i < COUNT(f->worlds); ++i)
        if (f->worlds[i] != NULL && !drain(f, i))
            return false;
    for (size_t i = 0; i < COUNT(f->children); ++i) {
        if (f->children[i].state == NULL)
            continue;
        for (size_t j = 0; j < COUNT(f->center); ++j)
            if (f->center[j] == f->children[i].state)
                f->center[j] = NULL;
        if (mc_world_store_retire_chunk(f->children[i].store, f->children[i].state, NULL) !=
            MC_WORLD_STORAGE_OK)
            return false;
        f->children[i] = (child){0};
    }
    for (size_t i = 0; i < COUNT(f->worlds); ++i)
        if (f->worlds[i] != NULL &&
            (mc_world_runtime_retire_world(f->worlds[i], NULL) != MC_WORLD_STORAGE_OK ||
             mc_world_runtime_free(f->worlds[i], NULL) != MC_WORLD_STORAGE_OK))
            return false;
    mc_scheduled_tick_release(f->survivor);
    mc_scheduled_tick_release(f->extra);
    for (size_t i = 0; i < COUNT(f->dimensions); ++i)
        mc_world_provider_release(f->dimensions[i]);
    for (size_t i = 0; i < COUNT(f->classes); ++i)
        if (mc_world_runtime_classes_free(f->classes[i], NULL) != MC_WORLD_STORAGE_OK)
            return false;
    for (size_t i = 0; i < COUNT(f->registry); ++i)
        if (mc_registry_classes_free(f->registry[i], NULL) != MC_REGISTRY_OK)
            return false;
    mc_world_info_name_release(f->name);
    for (size_t i = 0; i < COUNT(f->stores); ++i)
        mc_world_store_free(f->stores[i]);
    mc_world_regions_free(f->regions);
    mc_java_random_profile_free(f->profile);
    mc_materials_destroy(f->materials);
    free(f);
    return true;
}

static bool decode_path(const char *hex, char *out, size_t capacity) {
    const size_t n = strlen(hex);
    if (n == 0 || n % 2u != 0 || n / 2u >= capacity)
        return false;
    for (size_t i = 0; i < n / 2u; ++i) {
        unsigned int value = 0;
        for (size_t j = 0; j < 2; ++j) {
            const unsigned char c = (unsigned char)hex[2u * i + j];
            const unsigned int digit = c >= '0' && c <= '9'   ? (unsigned int)(c - '0')
                                       : c >= 'a' && c <= 'f' ? (unsigned int)(c - 'a') + 10u
                                                              : 16u;
            if (digit > 15u)
                return false;
            value = value * 16u + digit;
        }
        if (value == 0)
            return false;
        out[i] = (char)(unsigned char)value;
    }
    out[n / 2u] = '\0';
    return true;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    const uint16_t title[] = {'S', 'c', 'h'};
    if (argc != 3 || strcmp(argv[1], "world-scheduling") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    for (size_t i = 0; i < COUNT(f->stores); ++i)
        CHECK(open_store(f, i, root));
    for (size_t i = 0; i < COUNT(f->classes); ++i) {
        CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig,
                                              &f->classes[i], NULL) == MC_WORLD_STORAGE_OK);
        if (i < COUNT(f->registry)) {
            resources *r = &f->resources[i];
            r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
            r->streams[1] = (stream){.bytes = language};
            r->streams[2] = (stream){.bytes = language};
            r->fail_first = i == 1;
            r->api = (mc_registry_resources){r, open_resource, close_resource, resource_ready};
            const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                                 &r->api};
            CHECK(mc_registry_classes_allocate(&options, &f->registry[i], NULL) == MC_REGISTRY_OK);
            r->registry = f->registry[i];
            CHECK(mc_world_runtime_classes_bind_registry(f->classes[i], f->registry[i], NULL) ==
                  MC_REGISTRY_OK);
        }
    }
    CHECK(create_world(f, 0, 0, 16) && create_world(f, 1, 1, 16) && create_world(f, 2, 2, 16));
    CHECK(lazy_and_deferred(f));
    CHECK(time_identity_and_classes(f));
    CHECK(immediate(f));
    CHECK(missing_and_cold_registry(f));
    CHECK(collection_failures(f));
    CHECK(sequence_wrap(f));
    CHECK(raw_info_time(f));
    CHECK(maintained_and_retired(f));
    CHECK(!f->resources[0].bad && !f->resources[1].bad);
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("scheduling cleanup refused; live dependencies retained\n", stderr);
        return 1;
    }
    if (result == 0 && printf("PASS: %u World scheduling checks (world-scheduling)\n", checks) < 0)
        result = 1;
    return result;
}
