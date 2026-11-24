/* Genuine App/statistics/Player owners; the imported entry point is unused. */
#define MC_PLAYER_STATISTICS_WORLD_SEED INT64_C(23)
#define MC_PLAYER_STATISTICS_MAX_PROVIDER_CHUNKS 4
#define main mc_dye_unused_statistics_fixture_main
#include "../../client/statistics/test_player_dispatch.c"
#undef main

#include "item/registry/registry_internal.h"
#include "item/stack/source_internal.h"
#include "mc/core/java/java.h"
#include "mc/item/use/use.h"
#include "mc/world/runtime/world_access.h"
#include "world/runtime/runtime_internal.h"
#include "world/storage/generated_internal.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>

enum {
    DYE_CHUNKS = 4,
    DYE_PLANES = 5,
    DYE_EVENTS = 4096,
    DYE_ROWS = 39,
    DYE_ARRAY_BYTES = MC_CHUNK_BLOCKS + 3 * MC_CHUNK_NIBBLE_BYTES + MC_CHUNK_HEIGHTMAP_BYTES
};

static const size_t plane_lengths[DYE_PLANES] = {MC_CHUNK_BLOCKS, MC_CHUNK_NIBBLE_BYTES,
                                                 MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_NIBBLE_BYTES,
                                                 MC_CHUNK_HEIGHTMAP_BYTES};

typedef enum dye_event_kind { DYE_CELL, DYE_RANGE, DYE_ALIAS, DYE_COUNT } dye_event_kind;

typedef struct dye_event {
    dye_event_kind kind;
    int32_t coordinates[6];
    mc_item_stack_fields stack;
    mc_java_random item_random, world_random;
    size_t after_cell;
} dye_event;

typedef struct dye_observation {
    mc_registry_classes *registry;
    mc_world_runtime *world;
    mc_item_stack *stack;
    mc_java_random *item_random;
    dye_event events[DYE_EVENTS];
    size_t event_count, cells;
    int32_t mode;
    bool switched, refused, overflow, invalid;
} dye_observation;

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

static void snapshot(mc_chunk_state **chunks, uint8_t *out) {
    for (size_t n = 0; n < DYE_CHUNKS; ++n)
        for (size_t k = 0; k < DYE_PLANES; ++k) {
            memcpy(out, plane(chunks[n], k), plane_lengths[k]);
            out += plane_lengths[k];
        }
}

static bool unchanged(mc_chunk_state **chunks, const uint8_t *prior) {
    for (size_t n = 0; n < DYE_CHUNKS; ++n)
        for (size_t k = 0; k < DYE_PLANES; ++k) {
            if (memcmp(prior, plane(chunks[n], k), plane_lengths[k]) != 0)
                return false;
            prior += plane_lengths[k];
        }
    return true;
}

static bool same_random(const mc_java_random *a, const mc_java_random *b) {
    return a->state == b->state &&
           memcmp(&a->next_next_gaussian, &b->next_next_gaussian, sizeof(a->next_next_gaussian)) ==
               0 &&
           a->have_next_next_gaussian == b->have_next_next_gaussian;
}

static bool same_stack(const mc_item_stack_fields *a, const mc_item_stack_fields *b) {
    return a->count == b->count && a->animation == b->animation && a->id == b->id &&
           a->damage == b->damage;
}

static mc_world_storage_status refuse(dye_observation *o, mc_world_storage_error *error) {
    o->refused = true;
    if (error != NULL)
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT,
                                          "declared Dye observer native refusal", 0};
    return MC_WORLD_STORAGE_LIMIT;
}

static dye_event *event(dye_observation *o, dye_event_kind kind) {
    if (o->event_count == DYE_EVENTS) {
        o->overflow = true;
        return NULL;
    }
    dye_event *e = &o->events[o->event_count++];
    *e = (dye_event){.kind = kind};
    return e;
}

static mc_world_storage_status observe_cell(void *context, mc_world_runtime *world, int32_t x,
                                            int32_t y, int32_t z, mc_world_storage_error *error) {
    dye_observation *o = context;
    ++o->cells;
    dye_event *e = event(o, DYE_CELL);
    if (e == NULL)
        return refuse(o, error);
    e->coordinates[0] = x;
    e->coordinates[1] = y;
    e->coordinates[2] = z;
    if (world != o->world || world->fields.r == NULL ||
        mc_item_stack_fields_get(o->stack, &e->stack) != MC_ITEM_STACK_OK) {
        o->invalid = true;
        return refuse(o, error);
    }
    /* Always sample the original saved GM.b owner, including after aliasing. */
    e->item_random = *o->item_random;
    e->world_random = *world->fields.r;
    if (o->mode == 2 && !o->switched) {
        o->registry->items.b = world->fields.r;
        o->switched = true;
        e = event(o, DYE_ALIAS);
        if (e == NULL)
            return refuse(o, error);
        e->after_cell = o->cells;
    }
    if (o->mode == 4 && o->cells == 1) {
        if (mc_item_stack_source_count_set(o->stack, 99) != MC_ITEM_STACK_OK) {
            o->invalid = true;
            return refuse(o, error);
        }
        if (event(o, DYE_COUNT) == NULL)
            return refuse(o, error);
    }
    if ((o->mode == 1 && o->cells == 1) || (o->mode == 3 && o->cells == 3))
        return refuse(o, error);
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status observe_range(void *context, mc_world_runtime *world, int32_t x,
                                             int32_t y, int32_t z, int32_t max_x, int32_t max_y,
                                             int32_t max_z, mc_world_storage_error *error) {
    dye_observation *o = context;
    dye_event *e = event(o, DYE_RANGE);
    if (e == NULL)
        return refuse(o, error);
    if (world != o->world) {
        o->invalid = true;
        return refuse(o, error);
    }
    const int32_t coordinates[] = {x, y, z, max_x, max_y, max_z};
    memcpy(e->coordinates, coordinates, sizeof coordinates);
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

static void random_json(const mc_java_random *r) {
    uint64_t bits;
    memcpy(&bits, &r->next_next_gaussian, sizeof bits);
    printf("{\"seed_bits\":\"%012" PRIx64 "\",\"gaussian_bits\":\"%016" PRIx64
           "\",\"have_gaussian\":%s}",
           r->state, bits, r->have_next_next_gaussian ? "true" : "false");
}

static void notifications_json(const dye_observation *o) {
    putchar('[');
    for (size_t n = 0; n < o->event_count; ++n) {
        const dye_event *e = &o->events[n];
        if (n != 0)
            putchar(',');
        if (e->kind == DYE_ALIAS) {
            printf("{\"event\":\"gm_b_alias_world\",\"after_cell\":%zu}", e->after_cell);
        } else if (e->kind == DYE_COUNT) {
            fputs("{\"event\":\"authored_count_store\",\"value\":99}", stdout);
        } else if (e->kind == DYE_RANGE) {
            fputs("{\"event\":\"range\",\"coordinates\":[", stdout);
            for (size_t k = 0; k < 6; ++k)
                printf("%s%" PRId32, k == 0 ? "" : ",", e->coordinates[k]);
            fputs("]}", stdout);
        } else {
            printf("{\"event\":\"cell\",\"position\":[%" PRId32 ",%" PRId32 ",%" PRId32
                   "],\"stack\":",
                   e->coordinates[0], e->coordinates[1], e->coordinates[2]);
            stack_json(&e->stack);
            fputs(",\"item_random\":", stdout);
            random_json(&e->item_random);
            fputs(",\"world_random\":", stdout);
            random_json(&e->world_random);
            putchar('}');
        }
    }
    putchar(']');
}

static void put_block(mc_chunk_state **chunks, int32_t x, int32_t y, int32_t z, int32_t id,
                      int32_t metadata) {
    if (x < 0 || x > 31 || z < 0 || z > 31 || y < 0 || y > 127)
        return;
    mc_chunk_state *chunk = chunks[(size_t)(x >> 4) * 2u + (size_t)(z >> 4)];
    const size_t index = (((size_t)x & 15u) * 16u + ((size_t)z & 15u)) * 128u + (size_t)y;
    chunk->data->blocks[index] = (uint8_t)id;
    const unsigned int shift = (unsigned int)(index & 1u) * 4u;
    const uint32_t mask = UINT32_C(15) << shift;
    chunk->data->metadata[index / 2u] =
        (uint8_t)(((uint32_t)chunk->data->metadata[index / 2u] & ~mask) |
                  (((uint32_t)metadata & 15u) << shift));
}

static void changes_json(const uint8_t *prior, const uint8_t *post) {
    fputs("[", stdout);
    size_t offset = 0;
    bool first = true;
    for (size_t n = 0; n < DYE_CHUNKS; ++n)
        for (size_t k = 0; k < DYE_PLANES; ++k) {
            for (size_t j = 0; j < plane_lengths[k]; ++j)
                if (prior[offset + j] != post[offset + j]) {
                    printf("%s[%zu,%zu,%zu,%u,%u]", first ? "" : ",", n, k, j,
                           (unsigned int)prior[offset + j], (unsigned int)post[offset + j]);
                    first = false;
                }
            offset += plane_lengths[k];
        }
    putchar(']');
}

static void cells_json(mc_chunk_state **chunks) {
    putchar('[');
    bool first = true;
    for (size_t x = 0; x < 32; ++x)
        for (size_t z = 0; z < 32; ++z)
            for (size_t y = 0; y < 128; ++y) {
                mc_chunk_state *chunk = chunks[(x >> 4) * 2u + (z >> 4)];
                const size_t index = ((x & 15u) * 16u + (z & 15u)) * 128u + y;
                const unsigned int id = chunk->data->blocks[index];
                if (id != 0) {
                    const unsigned int metadata =
                        ((unsigned int)chunk->data->metadata[index / 2u] >>
                         ((unsigned int)(index & 1u) * 4u)) &
                        15u;
                    printf("%s[%zu,%zu,%zu,%u,%u]", first ? "" : ",", x, y, z, id, metadata);
                    first = false;
                }
            }
    putchar(']');
}

static bool project_row(fixture *f, mc_chunk_state **chunks, dye_observation *o,
                        mc_java_random *item_random, mc_block *crop, size_t index, const char *name,
                        const int64_t p[15], uint8_t *prior, uint8_t *post) {
    CHECK(p[0] == 1 && p[9] >= 0 && p[9] <= 1 && p[10] >= 0 && (p[10] & ~7) == 0 && p[13] >= 0 &&
          p[13] <= 4 && p[14] >= 0 && p[14] <= 4);
    for (size_t n = 0; n < DYE_CHUNKS; ++n) {
        for (size_t k = 0; k < DYE_PLANES; ++k)
            memset(plane(chunks[n], k), k == 2 ? 15 : k == 4 ? 64 : 0, plane_lengths[k]);
        chunks[n]->data->minimum_height = 64;
        chunks[n]->terrain_populated = true;
    }
    const int32_t x = (int32_t)p[4], y = (int32_t)p[5], z = (int32_t)p[6];
    for (int32_t floor_x = 0; floor_x < 32; ++floor_x)
        for (int32_t floor_z = 0; floor_z < 32; ++floor_z)
            put_block(chunks, floor_x, 63, floor_z, p[13] == 1 ? 3 : 2, 0);
    if (p[13] == 1)
        put_block(chunks, x, 63, z, 2, 0);
    if (p[7] == 59)
        put_block(chunks, x, mc_java_i32_sub(y, 1), z, 60, 0);
    put_block(chunks, x, y, z, (int32_t)p[7], (int32_t)p[8]);
    if (p[13] == 2 || p[13] == 3)
        put_block(chunks, x, mc_java_i32_add(y, 1), z, 1, 0);
    const int32_t crop_id = crop->fields.bn;
    if (p[13] == 4)
        crop->fields.bn = 0;
    f->world->fields.B = p[9] != 0;
    f->app.registry->items.b = item_random;
    mc_java_random_reseed(item_random, p[11]);
    mc_java_random_reseed(f->world->fields.r, p[12]);
    mc_item_stack *stack = NULL;
    CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(stack, 351, (int32_t)p[1], (int32_t)p[2]) == MC_ITEM_STACK_OK &&
          mc_item_stack_source_animation_set(stack, 5) == MC_ITEM_STACK_OK);
    /* No lighting or scheduled queue reset/drain occurs between source calls. */
    o->event_count = o->cells = 0;
    o->stack = stack;
    o->mode = (int32_t)p[14];
    o->switched = o->refused = o->overflow = o->invalid = false;
    const mc_java_random item_before = *item_random, world_before = *f->world->fields.r;
    snapshot(chunks, prior);
    mc_item_stack_fields before, after, retired;
    const mc_statistic *used = f->app.registry->statistics.E[351];
    int32_t used_before, used_after, used_retired;
    CHECK(used != NULL && mc_item_stack_fields_get(stack, &before) == MC_ITEM_STACK_OK &&
          mc_client_stats_writer_value(f->app.stats_writer, used, &used_before, NULL) ==
              MC_CLIENT_STATS_OK &&
          unchanged(chunks, prior) && same_random(item_random, &item_before) &&
          same_random(f->world->fields.r, &world_before) && o->event_count == 0);
    const size_t bindings = f->world->persistent_bindings, player_holds = f->player->holds;
    mc_item_use_call *call = NULL;
    mc_item_use_error error;
    mc_item_use_state state;
    const mc_item_use_status status = mc_item_use_on_block_begin(
        f->app.registry, (p[10] & 4) != 0 ? NULL : stack, (p[10] & 2) != 0 ? NULL : f->player,
        (p[10] & 1) != 0 ? NULL : f->world, x, y, z, (int32_t)p[3], &call, &error);
    CHECK(mc_item_use_inspect(call, &state) && !state.active && !o->overflow && !o->invalid);
    const mc_java_random item_after = *item_random, world_after = *f->world->fields.r;
    const bool alias_after = f->app.registry->items.b == f->world->fields.r;
    const bool remote_after = f->world->fields.B;
    CHECK(mc_item_stack_fields_get(stack, &after) == MC_ITEM_STACK_OK &&
          mc_client_stats_writer_value(f->app.stats_writer, used, &used_after, NULL) ==
              MC_CLIENT_STATS_OK);
    snapshot(chunks, post);
    const char *exception = "NONE";
    char callback_cause[128] = {0};
    if (o->refused) {
        CHECK(status == MC_ITEM_USE_WORLD && !state.returned && error.native_policy &&
              !error.source_exception && !error.source_error &&
              error.world.status == MC_WORLD_STORAGE_LIMIT && error.mutation != NULL &&
              error.world.operation != NULL &&
              strcmp(error.world.operation, "declared Dye observer native refusal") == 0 &&
              strlen(error.world.operation) < sizeof callback_cause);
        memcpy(callback_cause, error.world.operation, strlen(error.world.operation) + 1u);
        exception = "NATIVE_CALLBACK_REFUSAL";
    } else if (status != MC_ITEM_USE_OK) {
        CHECK(status == MC_ITEM_USE_SOURCE_NULL && !state.returned && !error.native_policy &&
              error.source_exception && !error.source_error && error.exception_class != NULL &&
              strcmp(error.exception_class, "java/lang/NullPointerException") == 0);
        exception = "java.lang.NullPointerException";
    } else {
        CHECK(state.returned && !error.source_exception && !error.source_error);
    }
    const size_t events_after = o->event_count, cells_after = o->cells;
    CHECK(mc_item_use_retire(&call, NULL) == MC_ITEM_USE_OK && call == NULL &&
          f->world->persistent_bindings == bindings && f->player->holds == player_holds &&
          mc_item_stack_fields_get(stack, &retired) == MC_ITEM_STACK_OK &&
          same_stack(&after, &retired) && unchanged(chunks, post) &&
          same_random(item_random, &item_after) && same_random(f->world->fields.r, &world_after) &&
          o->event_count == events_after && o->cells == cells_after &&
          (f->app.registry->items.b == f->world->fields.r) == alias_after &&
          f->world->fields.B == remote_after &&
          mc_client_stats_writer_value(f->app.stats_writer, used, &used_retired, NULL) ==
              MC_CLIENT_STATS_OK &&
          used_retired == used_after);
    /* Restore the same declared live fields after observation, as the helper does. */
    f->app.registry->items.b = item_random;
    crop->fields.bn = crop_id;
    printf("DYE\t{\"index\":%zu,\"name\":", index);
    json_string(name);
    fputs(",\"inputs\":[", stdout);
    for (size_t n = 0; n < 15; ++n)
        printf("%s%" PRId64, n == 0 ? "" : ",", p[n]);
    printf("],\"channel\":\"wrapper\",\"result\":\"%s\",\"exception\":",
           state.returned ? (state.result ? "true" : "false") : "UNRETURNED");
    json_string(exception);
    fputs(",\"stack_before\":", stdout);
    stack_json(&before);
    fputs(",\"stack_after\":", stdout);
    stack_json(&after);
    fputs(",\"item_random_before\":", stdout);
    random_json(&item_before);
    fputs(",\"item_random_after\":", stdout);
    random_json(&item_after);
    fputs(",\"world_random_before\":", stdout);
    random_json(&world_before);
    fputs(",\"world_random_after\":", stdout);
    random_json(&world_after);
    printf(",\"gm_b_is_world_after\":%s,\"used_delta\":%" PRId32
           ",\"remote_after\":%s,\"observer_cells\":%zu,\"notifications\":",
           alias_after ? "true" : "false", mc_java_i32_sub(used_after, used_before),
           remote_after ? "true" : "false", cells_after);
    notifications_json(o);
    fputs(",\"changes\":", stdout);
    changes_json(prior, post);
    fputs(",\"blocks_metadata_after\":", stdout);
    cells_json(chunks);
    fputs("}\n", stdout);
    printf("DYE_NATIVE\t{\"index\":%zu,\"status\":%d,\"native_policy\":%s,"
           "\"source_exception\":%s,\"source_error\":%s,\"observer_refused\":%s,"
           "\"world_status\":%d,\"retired_prefix_unchanged\":true,\"callback_cause\":",
           index, (int)status, error.native_policy ? "true" : "false",
           error.source_exception ? "true" : "false", error.source_error ? "true" : "false",
           o->refused ? "true" : "false", (int)error.world.status);
    if (o->refused)
        json_string(callback_cause);
    else
        fputs("null", stdout);
    fputs("}\n", stdout);
    mc_item_stack_release(stack);
    return true;
}

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
    FILE *input = NULL;
#ifdef _WIN32
    CHECK(fopen_s(&input, path, "rb") == 0);
#else
    input = fopen(path, "rb");
#endif
    CHECK(input != NULL);
    const mc_chunk_acquire_context *acquisition = mc_world_runtime_acquisition(f->world);
    CHECK(acquisition != NULL && acquisition->cache != NULL && acquisition->cache->count == 0);
    mc_chunk_state *chunks[DYE_CHUNKS] = {0};
    uint8_t *arrays = malloc(2u * DYE_CHUNKS * DYE_ARRAY_BYTES);
    dye_observation *observer = calloc(1, sizeof(*observer));
    CHECK(arrays != NULL && observer != NULL);
    for (size_t n = 0; n < DYE_CHUNKS; ++n) {
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
    const mc_item *dye = NULL;
    const mc_registry_item_array *items = NULL;
    const mc_block *crop_const = NULL;
    CHECK(mc_registry_item_c_get(f->app.registry, &items, NULL) == MC_REGISTRY_OK &&
          mc_registry_item_c_at(f->app.registry, items, 351, &dye, NULL) == MC_REGISTRY_OK &&
          dye != NULL && dye->kind == MC_ITEM_DYE && dye->classes == f->app.registry &&
          mc_registry_block_static_get(f->app.registry, "aA", &crop_const, NULL) ==
              MC_REGISTRY_OK &&
          crop_const != NULL);
    mc_java_random *item_random = f->app.registry->items.b;
    CHECK(item_random != NULL && f->world->fields.r != NULL && item_random != f->world->fields.r);
    observer->registry = f->app.registry;
    observer->world = f->world;
    observer->item_random = item_random;
    mc_world_access *access = NULL;
    const mc_world_access_callbacks callbacks = {.cell = observe_cell, .range = observe_range};
    CHECK(mc_world_access_create(&callbacks, observer, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->world, access, NULL) == MC_WORLD_STORAGE_OK);
    char line[2048];
    size_t rows = 0;
    while (fgets(line, sizeof line, input) != NULL) {
        CHECK(strchr(line, '\n') != NULL);
        char *cursor = line, *token = next_field(&cursor), *end = NULL;
        CHECK(token != NULL && token[0] != 0 && token[0] != '-');
        errno = 0;
        const unsigned long long raw_index = strtoull(token, &end, 10);
        CHECK(errno == 0 && *end == 0 && raw_index == rows && rows < DYE_ROWS);
        token = next_field(&cursor);
        CHECK(token != NULL && token[0] != 0 && strlen(token) < 96);
        char name[96];
        memcpy(name, token, strlen(token) + 1);
        int64_t p[15];
        for (size_t n = 0; n < 15; ++n) {
            token = next_field(&cursor);
            CHECK(token != NULL && token[0] != 0);
            errno = 0;
            const intmax_t value = strtoimax(token, &end, 10);
            CHECK(errno == 0 && *end == 0 && value >= INT64_MIN && value <= INT64_MAX &&
                  ((n == 11 || n == 12) || (value >= INT32_MIN && value <= INT32_MAX)));
            p[n] = (int64_t)value;
        }
        CHECK(next_field(&cursor) == NULL &&
              project_row(f, chunks, observer, item_random, (mc_block *)crop_const, rows, name, p,
                          arrays, arrays + DYE_CHUNKS * DYE_ARRAY_BYTES));
        ++rows;
    }
    CHECK(!ferror(input) && fclose(input) == 0 && rows == DYE_ROWS);
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->world, access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
    for (size_t n = 0; n < DYE_CHUNKS; ++n) {
        mc_chunk_state *actual = NULL;
        CHECK(mc_chunk_cache_lookup(acquisition->cache, (int32_t)(n / 2u), (int32_t)(n % 2u),
                                    &actual) &&
              actual == chunks[n]);
    }
    CHECK(mc_chunk_cache_init(acquisition->cache, acquisition->cache->entries,
                              acquisition->cache->capacity, acquisition->cache->insertions,
                              acquisition->cache->insertion_capacity));
    for (size_t n = 0; n < DYE_CHUNKS; ++n)
        CHECK(mc_world_store_retire_chunk(f->store, chunks[n], NULL) == MC_WORLD_STORAGE_OK);
    free(observer);
    free(arrays);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !setup(f, argv[1], "dye-reference", true) || !run_profiles(f, argv[3]) ||
        !teardown(f))
        return 1;
    free(f);
    printf("PASS: 39 Dye wrapper profiles, %zu actual-owner checks\n", checks);
    return 0;
}
