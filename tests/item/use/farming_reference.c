/* Actual App/Stats/Player owners; the imported fixture entry point is unused. */
#define MC_PLAYER_STATISTICS_WORLD_SEED INT64_C(23)
#define MC_PLAYER_STATISTICS_MAX_PROVIDER_CHUNKS 4
#define main mc_farming_unused_statistics_fixture_main
#include "../../client/statistics/test_player_dispatch.c"
#undef main

#include "item/registry/registry_internal.h"
#include "item/stack/source_internal.h"
#include "mc/core/java/java.h"
#include "mc/item/use/use.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/world_access.h"
#include "world/runtime/runtime_internal.h"
#include "world/storage/generated_internal.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>

enum {
    FARMING_PLANES = 5,
    FARMING_CHUNKS = 4,
    FARMING_EVENTS = 64,
    FARMING_ARRAY_BYTES = MC_CHUNK_BLOCKS + 3 * MC_CHUNK_NIBBLE_BYTES + MC_CHUNK_HEIGHTMAP_BYTES
};

static const size_t plane_lengths[FARMING_PLANES] = {MC_CHUNK_BLOCKS, MC_CHUNK_NIBBLE_BYTES,
                                                     MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_NIBBLE_BYTES,
                                                     MC_CHUNK_HEIGHTMAP_BYTES};

typedef struct farming_event {
    bool range;
    int32_t coordinates[6];
} farming_event;

typedef struct farming_observation {
    mc_block *farmland;
    int32_t sound_mode, cell_fail;
    farming_event events[FARMING_EVENTS];
    size_t event_count, sound_count;
    uint16_t sound_name[64];
    size_t sound_length;
    double x, y, z;
    float volume, pitch;
    bool refused, overflow;
} farming_observation;

static uint8_t *plane(mc_chunk_state *chunk, size_t kind) {
    switch (kind) {
        case 0:
            return chunk->data->blocks;
        case 1:
            return chunk->data->metadata;
        case 2:
            return chunk->data->skylight;
        case 3:
            return chunk->data->block_light;
        case 4:
            return chunk->data->heightmap;
        default:
            return NULL;
    }
}

static mc_world_storage_status refusal(farming_observation *o, mc_world_storage_error *error) {
    o->refused = true;
    if (error != NULL)
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT,
                                          "declared farming observer native refusal", 0};
    return MC_WORLD_STORAGE_LIMIT;
}

static mc_world_storage_status observe_cell(void *context, mc_world_runtime *world, int32_t x,
                                            int32_t y, int32_t z, mc_world_storage_error *error) {
    (void)world;
    farming_observation *o = context;
    if (o->event_count == FARMING_EVENTS) {
        o->overflow = true;
        return refusal(o, error);
    }
    o->events[o->event_count++] = (farming_event){false, {x, y, z, 0, 0, 0}};
    return o->cell_fail != 0 ? refusal(o, error) : MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status observe_range(void *context, mc_world_runtime *world, int32_t x,
                                             int32_t y, int32_t z, int32_t max_x, int32_t max_y,
                                             int32_t max_z, mc_world_storage_error *error) {
    (void)world;
    farming_observation *o = context;
    if (o->event_count == FARMING_EVENTS) {
        o->overflow = true;
        return refusal(o, error);
    }
    o->events[o->event_count++] = (farming_event){true, {x, y, z, max_x, max_y, max_z}};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status observe_sound(void *context, mc_world_runtime *world,
                                             const mc_registry_string *name, double x, double y,
                                             double z, float volume, float pitch,
                                             mc_world_storage_error *error) {
    farming_observation *o = context;
    if (++o->sound_count != 1 || name == NULL || name->length > 64) {
        o->overflow = true;
        return refusal(o, error);
    }
    o->sound_length = name->length;
    memcpy(o->sound_name, name->units, name->length * sizeof(*name->units));
    o->x = x;
    o->y = y;
    o->z = z;
    o->volume = volume;
    o->pitch = pitch;
    if (o->sound_mode == 1)
        return refusal(o, error);
    if (o->sound_mode == 2)
        world->fields.B = true;
    if (o->sound_mode == 3)
        world->fields.B = false;
    if (o->sound_mode == 4)
        o->farmland->fields.bn = 3;
    return MC_WORLD_STORAGE_OK;
}

static void json_string(const char *text) {
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)text; *p != 0; ++p) {
        if (*p == '"' || *p == '\\')
            putchar('\\');
        if (*p < 32)
            printf("\\u%04x", (unsigned int)*p);
        else
            putchar(*p);
    }
    putchar('"');
}

static void stack_json(const mc_item_stack_fields *s) {
    printf("[%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 "]", s->count, s->animation, s->id,
           s->damage);
}

static void double_bits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof bits);
    printf("\"%016" PRIx64 "\"", bits);
}

static void float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof bits);
    printf("\"%08" PRIx32 "\"", bits);
}

static bool cell(mc_world_runtime *world, const int32_t *p, bool above, int32_t out[2]) {
    const int32_t y = above ? mc_java_i32_add(p[8], 1) : p[8];
    CHECK(mc_world_runtime_block_get(world, p[7], y, p[9], &out[0], NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_metadata_get(world, p[7], y, p[9], &out[1], NULL) ==
          MC_WORLD_STORAGE_OK);
    return true;
}

static void cell_json(const int32_t value[2]) {
    printf("[%" PRId32 ",%" PRId32 "]", value[0], value[1]);
}

static void put_block(mc_chunk_state **chunks, int32_t x, int32_t y, int32_t z, int32_t id) {
    if (x < 0 || x > 31 || z < 0 || z > 31 || y < 0 || y > 127)
        return;
    mc_chunk_state *c = chunks[(size_t)(x >> 4) * 2u + (size_t)(z >> 4)];
    const size_t index = (((size_t)x & 15u) * 16u + ((size_t)z & 15u)) * 128u + (size_t)y;
    c->data->blocks[index] = (uint8_t)id;
}

static bool project_row(fixture *f, mc_chunk_state **chunks, farming_observation *observer,
                        size_t index, const char *name, const int32_t p[19], uint8_t *prior) {
    const mc_registry_item_array *array = NULL;
    const mc_item *item_const = NULL;
    CHECK(p[1] == 1 && p[14] == 0 && (p[13] & ~69) == 0 && p[15] >= 0 && p[15] <= 4);
    CHECK(mc_registry_item_c_get(f->app.registry, &array, NULL) == MC_REGISTRY_OK &&
          mc_registry_item_c_at(f->app.registry, array, p[2], &item_const, NULL) == MC_REGISTRY_OK);
    CHECK(item_const != NULL && item_const->classes == f->app.registry &&
          ((p[0] == 0 && item_const->kind == MC_ITEM_HOE) ||
           (p[0] == 1 && item_const->kind == MC_ITEM_SEEDS)));
    mc_item *item = (mc_item *)item_const;
    const mc_block *dirt_const = NULL, *farmland_const = NULL;
    CHECK(mc_registry_block_static_get(f->app.registry, "w", &dirt_const, NULL) == MC_REGISTRY_OK &&
          mc_registry_block_static_get(f->app.registry, "aB", &farmland_const, NULL) ==
              MC_REGISTRY_OK &&
          dirt_const != NULL && farmland_const != NULL);
    mc_block *dirt = (mc_block *)dirt_const, *farmland = (mc_block *)farmland_const;
    const int32_t dirt_id = dirt->fields.bn, farmland_id = farmland->fields.bn;
    const mc_block_sound *step = farmland->fields.by;
    const int32_t crop_id = item->kind == MC_ITEM_SEEDS ? item->subtype.rk.a : 0;
    const int32_t maximum = item->fields.a; /* Actual inherited GM.f body. */
    const int32_t damage = p[5] == 1   ? maximum
                           : p[5] == 2 ? mc_java_i32_sub(maximum, 1)
                           : p[5] == 3 ? mc_java_i32_add(maximum, 1)
                                       : p[4];
    mc_item_stack *stack = NULL;
    CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(stack, p[2], p[3], damage) == MC_ITEM_STACK_OK &&
          mc_item_stack_source_animation_set(stack, 5) == MC_ITEM_STACK_OK);
    for (size_t n = 0; n < FARMING_CHUNKS; ++n) {
        for (size_t k = 0; k < FARMING_PLANES; ++k)
            memset(plane(chunks[n], k), k == 2 ? 15 : 0, plane_lengths[k]);
        chunks[n]->data->minimum_height = 0;
        chunks[n]->terrain_populated = true;
    }
    put_block(chunks, p[7], p[8], p[9], p[10]);
    put_block(chunks, p[7], mc_java_i32_add(p[8], 1), p[9], p[11]);
    f->world->fields.B = p[12] != 0;
    *observer =
        (farming_observation){.farmland = farmland, .sound_mode = p[15], .cell_fail = p[16]};
    if (item->kind == MC_ITEM_SEEDS)
        item->subtype.rk.a = p[17];
    dirt->fields.bn = p[18];
    if ((p[13] & 64) != 0)
        farmland->fields.by = NULL;
    mc_item_stack_fields before, after;
    int32_t center_before[2], above_before[2], center_after[2], above_after[2];
    CHECK(mc_item_stack_fields_get(stack, &before) == MC_ITEM_STACK_OK &&
          cell(f->world, p, false, center_before) && cell(f->world, p, true, above_before));
    size_t offset = 0;
    for (size_t n = 0; n < FARMING_CHUNKS; ++n)
        for (size_t k = 0; k < FARMING_PLANES; ++k) {
            memcpy(prior + offset, plane(chunks[n], k), plane_lengths[k]);
            offset += plane_lengths[k];
        }
    const mc_statistic *used = f->app.registry->statistics.E[p[2]];
    const mc_statistic *broken = f->app.registry->statistics.F[p[2]];
    int32_t used_before, used_after, broken_before = 0, broken_after = 0;
    CHECK(used != NULL && mc_client_stats_writer_value(f->app.stats_writer, used, &used_before,
                                                       NULL) == MC_CLIENT_STATS_OK);
    if (broken != NULL)
        CHECK(mc_client_stats_writer_value(f->app.stats_writer, broken, &broken_before, NULL) ==
              MC_CLIENT_STATS_OK);
    mc_item_use_call *call = NULL;
    mc_item_use_error error;
    mc_item_use_state state;
    const mc_item_use_status status = mc_item_use_on_block_begin(
        f->app.registry, stack, (p[13] & 4) != 0 ? NULL : f->player,
        (p[13] & 1) != 0 ? NULL : f->world, p[7], p[8], p[9], p[6], &call, &error);
    CHECK(mc_item_use_inspect(call, &state) && !state.active && !observer->overflow);
    uint8_t *post = prior + FARMING_CHUNKS * FARMING_ARRAY_BYTES;
    offset = 0;
    for (size_t n = 0; n < FARMING_CHUNKS; ++n)
        for (size_t k = 0; k < FARMING_PLANES; ++k) {
            memcpy(post + offset, plane(chunks[n], k), plane_lengths[k]);
            offset += plane_lengths[k];
        }
    CHECK(mc_item_stack_fields_get(stack, &after) == MC_ITEM_STACK_OK &&
          mc_client_stats_writer_value(f->app.stats_writer, used, &used_after, NULL) ==
              MC_CLIENT_STATS_OK);
    if (broken != NULL)
        CHECK(mc_client_stats_writer_value(f->app.stats_writer, broken, &broken_after, NULL) ==
              MC_CLIENT_STATS_OK);
    /* These are the declared nullable/data inputs, restored after the call. */
    farmland->fields.by = step;
    dirt->fields.bn = dirt_id;
    farmland->fields.bn = farmland_id;
    if (item->kind == MC_ITEM_SEEDS)
        item->subtype.rk.a = crop_id;
    CHECK(cell(f->world, p, false, center_after) && cell(f->world, p, true, above_after));
    const char *exception = "NONE";
    if (observer->refused) {
        CHECK(status == MC_ITEM_USE_WORLD && !state.returned && error.native_policy &&
              !error.source_exception && !error.source_error &&
              error.world.status == MC_WORLD_STORAGE_LIMIT);
        exception = "NATIVE_CALLBACK_REFUSAL";
    } else if (status != MC_ITEM_USE_OK) {
        CHECK(!state.returned && !error.native_policy && error.source_exception &&
              !error.source_error && error.exception_class != NULL &&
              strcmp(error.exception_class, "java/lang/NullPointerException") == 0);
        exception = "java.lang.NullPointerException";
    } else {
        CHECK(state.returned);
    }
    printf("FARMING\t{\"index\":%zu,\"name\":", index);
    json_string(name);
    fputs(",\"values\":[", stdout);
    for (size_t n = 0; n < 19; ++n)
        printf("%s%" PRId32, n == 0 ? "" : ",", p[n]);
    printf("],\"maximum\":%" PRId32 ",\"result\":\"%s\",\"exception\":", maximum,
           state.returned ? (state.result ? "true" : "false") : "UNRETURNED");
    json_string(exception);
    fputs(",\"stack_before\":", stdout);
    stack_json(&before);
    fputs(",\"stack_after\":", stdout);
    stack_json(&after);
    fputs(",\"center_before\":", stdout);
    cell_json(center_before);
    fputs(",\"center_after\":", stdout);
    cell_json(center_after);
    fputs(",\"above_before\":", stdout);
    cell_json(above_before);
    fputs(",\"above_after\":", stdout);
    cell_json(above_after);
    printf(",\"remote_after\":%s,\"sounds\":[", f->world->fields.B ? "true" : "false");
    if (observer->sound_count != 0) {
        fputs("[\"", stdout);
        for (size_t n = 0; n < observer->sound_length; ++n)
            printf("\\u%04x", (unsigned int)observer->sound_name[n]);
        fputs("\",", stdout);
        double_bits(observer->x);
        putchar(',');
        double_bits(observer->y);
        putchar(',');
        double_bits(observer->z);
        putchar(',');
        float_bits(observer->volume);
        putchar(',');
        float_bits(observer->pitch);
        putchar(']');
    }
    fputs("],\"notifications\":[", stdout);
    for (size_t n = 0; n < observer->event_count; ++n) {
        const farming_event *event = &observer->events[n];
        printf("%s[\"%s\"", n == 0 ? "" : ",", event->range ? "range" : "cell");
        for (size_t k = 0; k < (event->range ? 6u : 3u); ++k)
            printf(",%" PRId32, event->coordinates[k]);
        putchar(']');
    }
    fputs("],\"changes\":[", stdout);
    offset = 0;
    bool first = true;
    for (size_t n = 0; n < FARMING_CHUNKS; ++n)
        for (size_t k = 0; k < FARMING_PLANES; ++k) {
            const uint8_t *current = post + offset;
            for (size_t j = 0; j < plane_lengths[k]; ++j)
                if (prior[offset + j] != current[j]) {
                    printf("%s[%zu,%zu,%zu,%u,%u]", first ? "" : ",", n, k, j,
                           (unsigned int)prior[offset + j], (unsigned int)current[j]);
                    first = false;
                }
            offset += plane_lengths[k];
        }
    printf("],\"used_delta\":%" PRId32 ",\"broken_delta\":%" PRId32 "}\n",
           mc_java_i32_sub(used_after, used_before), mc_java_i32_sub(broken_after, broken_before));
    printf("FARMING_NATIVE\t{\"index\":%zu,\"status\":%d,\"native_policy\":%s,"
           "\"source_exception\":%s,\"source_error\":%s,\"observer_refused\":%s}\n",
           index, (int)status, error.native_policy ? "true" : "false",
           error.source_exception ? "true" : "false", error.source_error ? "true" : "false",
           observer->refused ? "true" : "false");
    CHECK(mc_item_use_retire(&call, NULL) == MC_ITEM_USE_OK && call == NULL);
    mc_item_stack_release(stack);
    return true;
}

/* Split exactly one field without shared tokenizer state or platform-specific
 * secure-CRT extensions. Empty fields remain visible to the checked parser. */
static char *next_field(char **cursor) {
    if (*cursor == NULL)
        return NULL;
    char *value = *cursor;
    const size_t length = strcspn(value, "\t\r\n");
    const char delimiter = value[length];
    if (delimiter == 0) {
        *cursor = NULL;
    } else {
        value[length] = 0;
        *cursor = value + length + 1;
        if (delimiter != '\t') {
            *cursor += strspn(*cursor, "\r\n");
            if (**cursor == 0)
                *cursor = NULL;
        }
    }
    return value;
}

static bool run_profiles(fixture *f, const char *path) {
    FILE *input = fopen(path, "rb");
    CHECK(input != NULL);
    const mc_chunk_acquire_context *acquisition = mc_world_runtime_acquisition(f->world);
    CHECK(acquisition != NULL && acquisition->cache != NULL && acquisition->cache->count == 0);
    mc_chunk_state *chunks[FARMING_CHUNKS] = {0};
    size_t snapshot_bytes = 0;
    for (size_t k = 0; k < FARMING_PLANES; ++k)
        snapshot_bytes += plane_lengths[k] * FARMING_CHUNKS;
    uint8_t *prior = malloc(snapshot_bytes * 2u);
    CHECK(prior != NULL);
    for (size_t n = 0; n < FARMING_CHUNKS; ++n) {
        uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
        mc_world_chunk *chunk = NULL;
        CHECK(blocks != NULL &&
              mc_world_store_create_generated(f->store, f->world, 0, (int32_t)(n / 2u),
                                              (int32_t)(n % 2u), &blocks, MC_CHUNK_BLOCKS, &chunk,
                                              NULL) == MC_WORLD_STORAGE_OK &&
              blocks == NULL);
        chunks[n] = mc_world_chunk_state(chunk);
        CHECK(chunks[n] != NULL && mc_chunk_cache_publish(acquisition->cache, (int32_t)(n / 2u),
                                                          (int32_t)(n % 2u), chunks[n]));
        for (size_t j = 0; j < n; ++j)
            CHECK(chunks[n] != chunks[j] && chunks[n]->data != chunks[j]->data &&
                  chunks[n]->data->blocks != chunks[j]->data->blocks);
    }
    farming_observation *observer = calloc(1, sizeof(*observer));
    mc_world_access *access = NULL;
    const mc_world_access_callbacks callbacks = {
        .cell = observe_cell, .range = observe_range, .sound = observe_sound};
    CHECK(observer != NULL &&
          mc_world_access_create(&callbacks, observer, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->world, access, NULL) == MC_WORLD_STORAGE_OK);
    char line[2048];
    size_t rows = 0, previous = 0;
    while (fgets(line, sizeof line, input) != NULL) {
        CHECK(strchr(line, '\n') != NULL);
        char *cursor = line, *token = next_field(&cursor), *end = NULL;
        CHECK(token != NULL && token[0] != 0 && token[0] != '-');
        errno = 0;
        unsigned long long raw_index = strtoull(token, &end, 10);
        CHECK(errno == 0 && *end == 0 && raw_index < 118u);
        size_t index = (size_t)raw_index;
        CHECK(rows == 0 || index > previous);
        previous = index;
        token = next_field(&cursor);
        CHECK(token != NULL && token[0] != 0 && strlen(token) < 96);
        char name[96];
        memcpy(name, token, strlen(token) + 1);
        int32_t p[19];
        for (size_t n = 0; n < 19; ++n) {
            token = next_field(&cursor);
            CHECK(token != NULL && token[0] != 0);
            errno = 0;
            long long value = strtoll(token, &end, 10);
            CHECK(errno == 0 && *end == 0 && value >= INT32_MIN && value <= INT32_MAX);
            p[n] = (int32_t)value;
        }
        CHECK(next_field(&cursor) == NULL &&
              project_row(f, chunks, observer, index, name, p, prior));
        ++rows;
    }
    CHECK(!ferror(input) && fclose(input) == 0 && rows == 57);
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->world, access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
    for (size_t n = 0; n < FARMING_CHUNKS; ++n) {
        mc_chunk_state *actual = NULL;
        CHECK(mc_chunk_cache_lookup(acquisition->cache, (int32_t)(n / 2u), (int32_t)(n % 2u),
                                    &actual) &&
              actual == chunks[n]);
    }
    CHECK(mc_chunk_cache_init(acquisition->cache, acquisition->cache->entries,
                              acquisition->cache->capacity, acquisition->cache->insertions,
                              acquisition->cache->insertion_capacity));
    for (size_t n = 0; n < FARMING_CHUNKS; ++n)
        CHECK(mc_world_store_retire_chunk(f->store, chunks[n], NULL) == MC_WORLD_STORAGE_OK);
    free(prior);
    free(observer);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !setup(f, argv[1], "farming-reference", true) || !run_profiles(f, argv[3]) ||
        !teardown(f))
        return 1;
    free(f);
    printf("PASS: 57 farming wrapper profiles, %zu actual-owner checks\n", checks);
    return 0;
}
