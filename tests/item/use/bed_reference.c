/* Prepared genuine owners; the imported statistics entry point is unused. */
#define MC_PLAYER_STATISTICS_WORLD_SEED INT64_C(23)
#define MC_PLAYER_STATISTICS_MAX_PROVIDER_CHUNKS 4
#define main mc_bed_unused_statistics_fixture_main
#include "../../client/statistics/test_player_dispatch.c"
#undef main

#include "item/registry/registry_internal.h"
#include "item/stack/source_internal.h"
#include "item/use/bed_internal.h"
#include "mc/core/java/java.h"
#include "mc/world/runtime/lighting.h"
#include "mc/world/runtime/world_access.h"
#include "world/runtime/lighting_internal.h"
#include "world/runtime/runtime_internal.h"
#include "world/storage/generated_internal.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <xmmintrin.h>

enum { BED_ROWS = 72, BED_CHUNKS = 4, BED_PLANES = 5, BED_EVENTS = 4096 };

static const size_t bed_lengths[BED_PLANES] = {MC_CHUNK_BLOCKS, MC_CHUNK_NIBBLE_BYTES,
                                               MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_NIBBLE_BYTES,
                                               MC_CHUNK_HEIGHTMAP_BYTES};

typedef struct bed_event {
    bool range;
    int32_t xyz[6];
} bed_event;

typedef struct bed_observer {
    mc_chunk_state **chunks;
    mc_item_stack *stack;
    mc_block *bed;
    mc_item_use_call **call;
    mc_math_classes *math;
    bed_event events[BED_EVENTS];
    size_t count;
    int32_t x, y, z, hx, hz, direction, flags;
    bool foot, head, refused, overflow, busy_checked;
} bed_observer;

static uint8_t *bed_plane(mc_chunk_state *c, size_t n) {
    switch (n) {
        case 0:
            return c->data->blocks;
        case 1:
            return c->data->metadata;
        case 2:
            return c->data->skylight;
        case 3:
            return c->data->block_light;
        case 4:
            return c->data->heightmap;
        default:
            return NULL;
    }
}

static void bed_put(mc_chunk_state **chunks, int32_t x, int32_t y, int32_t z, int32_t id,
                    int32_t m) {
    if (x < 0 || x > 31 || z < 0 || z > 31 || y < 0 || y > 127)
        return;
    mc_chunk_state *c = chunks[(size_t)(x >> 4) * 2u + (size_t)(z >> 4)];
    size_t index = (((size_t)x & 15u) * 16u + ((size_t)z & 15u)) * 128u + (size_t)y;
    unsigned shift = (unsigned)(index & 1u) * 4u;
    c->data->blocks[index] = (uint8_t)id;
    c->data->metadata[index >> 1] = (uint8_t)((c->data->metadata[index >> 1] & ~(15u << shift)) |
                                              (((uint32_t)m & 15u) << shift));
}

static void bed_cell(mc_chunk_state **chunks, int32_t x, int32_t y, int32_t z, int32_t out[2]) {
    out[0] = out[1] = 0;
    if (x < 0 || x > 31 || z < 0 || z > 31 || y < 0 || y > 127)
        return;
    mc_chunk_state *c = chunks[(size_t)(x >> 4) * 2u + (size_t)(z >> 4)];
    size_t index = (((size_t)x & 15u) * 16u + ((size_t)z & 15u)) * 128u + (size_t)y;
    out[0] = c->data->blocks[index];
    out[1] = (int32_t)((c->data->metadata[index >> 1] >> ((unsigned)(index & 1u) * 4u)) & 15u);
}

static mc_world_storage_status bed_refuse(bed_observer *o, mc_world_storage_error *e) {
    o->refused = true;
    if (e != NULL)
        *e = (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT,
                                      "declared Bed observer native refusal", 0};
    return MC_WORLD_STORAGE_LIMIT;
}

static mc_world_storage_status bed_notice(void *context, mc_world_runtime *world, int32_t x,
                                          int32_t y, int32_t z, mc_world_storage_error *e) {
    (void)world;
    bed_observer *o = context;
    if (o->count == BED_EVENTS) {
        o->overflow = true;
        return bed_refuse(o, e);
    }
    o->events[o->count++] = (bed_event){false, {x, y, z, 0, 0, 0}};
    if (!o->foot && x == o->x && y == o->y && z == o->z) {
        o->foot = true;
        if ((o->flags & 1) != 0)
            bed_put(o->chunks, o->hx, o->y, o->hz, o->bed->fields.bn, o->direction + 8);
        if ((o->flags & 8) != 0 &&
            mc_item_stack_source_count_set(o->stack, 100) != MC_ITEM_STACK_OK)
            return bed_refuse(o, e);
        if ((o->flags & 16) != 0)
            o->bed->fields.bn = 1;
        if (o->call != NULL) {
            mc_item_use_error cleanup;
            mc_math_class_state state;
            o->busy_checked = mc_item_use_retire(o->call, &cleanup) == MC_ITEM_USE_BUSY &&
                              *o->call != NULL &&
                              mc_math_classes_state(o->math, &state, NULL) == MC_MATH_CLASS_OK &&
                              state.holds == 2;
            if (!o->busy_checked)
                return bed_refuse(o, e);
        }
        if ((o->flags & 2) != 0)
            return bed_refuse(o, e);
    }
    if (!o->head && x == o->hx && y == o->y && z == o->hz) {
        o->head = true;
        if ((o->flags & 4) != 0)
            return bed_refuse(o, e);
    }
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status bed_range(void *context, mc_world_runtime *world, int32_t x,
                                         int32_t y, int32_t z, int32_t a, int32_t b, int32_t c,
                                         mc_world_storage_error *e) {
    (void)world;
    bed_observer *o = context;
    if (o->count == BED_EVENTS) {
        o->overflow = true;
        return bed_refuse(o, e);
    }
    o->events[o->count++] = (bed_event){true, {x, y, z, a, b, c}};
    return MC_WORLD_STORAGE_OK;
}

static void bed_string(const char *s) {
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)s; *p != 0; ++p) {
        if (*p == '"' || *p == '\\')
            putchar('\\');
        if (*p < 32)
            printf("\\u%04x", (unsigned)*p);
        else
            putchar(*p);
    }
    putchar('"');
}

static void bed_pair(const int32_t v[2]) {
    printf("[%" PRId32 ",%" PRId32 "]", v[0], v[1]);
}

static void bed_reset(mc_chunk_state **chunks) {
    for (size_t n = 0; n < BED_CHUNKS; ++n) {
        for (size_t k = 0; k < BED_PLANES; ++k)
            memset(bed_plane(chunks[n], k), k == 2 ? 15 : 0, bed_lengths[k]);
        chunks[n]->data->minimum_height = 0;
        chunks[n]->terrain_populated = true;
    }
}

static size_t bed_snapshot(mc_chunk_state **chunks, uint8_t *p) {
    size_t total = 0;
    for (size_t n = 0; n < BED_CHUNKS; ++n)
        for (size_t k = 0; k < BED_PLANES; ++k) {
            if (p != NULL)
                memcpy(p + total, bed_plane(chunks[n], k), bed_lengths[k]);
            total += bed_lengths[k];
        }
    return total;
}

static bool bed_project(fixture *f, mc_chunk_state **chunks, bed_observer *o, mc_math_classes *math,
                        size_t index, const char *name, const int32_t p[17], uint8_t *prior) {
    const mc_registry_item_array *items = NULL;
    const mc_item *item = NULL;
    const mc_block *block = NULL;
    CHECK(mc_registry_item_c_get(f->app.registry, &items, NULL) == MC_REGISTRY_OK &&
          mc_registry_item_c_at(f->app.registry, items, 355, &item, NULL) == MC_REGISTRY_OK &&
          item != NULL &&
          mc_registry_block_static_get(f->app.registry, "T", &block, NULL) == MC_REGISTRY_OK &&
          block != NULL);
    mc_block *bed = (mc_block *)block;
    int32_t y = mc_java_i32_add(p[2], 1), direction = p[16];
    int32_t hx = mc_java_i32_add(p[1], direction == 1 ? -1 : direction == 3 ? 1 : 0);
    int32_t hz = mc_java_i32_add(p[3], direction == 0 ? 1 : direction == 2 ? -1 : 0);
    CHECK(direction >= 0 && direction < 4);
    bed_reset(chunks);
    /* Fixture List.clear is supplied by the World owner and retires old C
     * records without propagating/draining them or changing the C identity. */
    CHECK(mc_world_runtime_light_list_clear(f->world, NULL) == MC_WORLD_STORAGE_OK);
    bed_put(chunks, p[1], mc_java_i32_sub(y, 1), p[3], p[8], 0);
    bed_put(chunks, hx, mc_java_i32_sub(y, 1), hz, p[9], 0);
    bed_put(chunks, p[1], y, p[3], p[6], p[13]);
    bed_put(chunks, hx, y, hz, p[7], p[14]);
    f->world->fields.B = p[10] != 0;
    bed->fields.bn = p[12];
    uint32_t yaw_word;
    memcpy(&yaw_word, &p[4], sizeof yaw_word);
    memcpy(&f->player->base.yaw, &yaw_word, sizeof yaw_word);
    mc_item_stack *stack = NULL;
    CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(stack, 355, p[5], 0) == MC_ITEM_STACK_OK &&
          mc_item_stack_source_animation_set(stack, 7) == MC_ITEM_STACK_OK);
    mc_item_use_call *call = NULL;
    *o = (bed_observer){.chunks = chunks,
                        .stack = stack,
                        .bed = bed,
                        .call = &call,
                        .math = math,
                        .x = p[1],
                        .y = y,
                        .z = p[3],
                        .hx = hx,
                        .hz = hz,
                        .direction = direction,
                        .flags = p[15]};
    int32_t foot_before[2], head_before[2], foot_after[2], head_after[2];
    bed_cell(chunks, p[1], y, p[3], foot_before);
    bed_cell(chunks, hx, y, hz, head_before);
    (void)bed_snapshot(chunks, prior);
    const mc_java_random world_random = *f->world->fields.r, player_random = *f->player->random;
    const size_t player_holds = f->player->holds;
    mc_item_use_context context = {math};
    mc_item_use_error error;
    mc_item_use_state state;
    mc_item_use_status status = mc_item_use_bed_begin(
        f->app.registry, item, (p[11] & 2) != 0 ? NULL : stack, (p[11] & 4) != 0 ? NULL : f->player,
        (p[11] & 1) != 0 ? NULL : f->world, p[1], p[2], p[3], p[0], &context, &call, &error);
    CHECK(mc_item_use_inspect(call, &state) && !state.active && !o->overflow);
    CHECK(state.math == math &&
          (p[0] != 1 || (p[11] & 4) != 0 || state.bed_direction == direction));
    CHECK(!o->foot || o->busy_checked);
    CHECK(f->world->fields.r->state == world_random.state &&
          f->player->random->state == player_random.state &&
          f->world->fields.r->have_next_next_gaussian == world_random.have_next_next_gaussian &&
          f->player->random->have_next_next_gaussian == player_random.have_next_next_gaussian &&
          memcmp(&f->world->fields.r->next_next_gaussian, &world_random.next_next_gaussian,
                 sizeof(double)) == 0 &&
          memcmp(&f->player->random->next_next_gaussian, &player_random.next_next_gaussian,
                 sizeof(double)) == 0);
    mc_item_stack_fields after;
    CHECK(mc_item_stack_fields_get(stack, &after) == MC_ITEM_STACK_OK);
    bed_cell(chunks, p[1], y, p[3], foot_after);
    bed_cell(chunks, hx, y, hz, head_after);
    const char *exception = "NONE", *message = "";
    if (o->refused) {
        CHECK(status == MC_ITEM_USE_WORLD && !state.returned && error.native_policy &&
              !error.source_exception);
        exception = "NATIVE_CALLBACK_REFUSAL";
        message = (p[15] & 2) != 0 ? "foot" : "head";
    } else if (status != MC_ITEM_USE_OK) {
        CHECK(status == MC_ITEM_USE_SOURCE_NULL && !error.native_policy && error.source_exception);
        exception = "java.lang.NullPointerException";
    } else
        CHECK(state.returned);
    const size_t expected_player_holds = player_holds + (p[0] == 1 && (p[11] & 4) == 0 ? 1u : 0u);
    CHECK(f->player->holds == expected_player_holds);
    mc_world_light_list_view lights;
    CHECK(mc_world_runtime_light_list_get(f->world, &lights, NULL) == MC_WORLD_STORAGE_OK);
    printf("BED_PLACEMENT\t{\"index\":%zu,\"name\":", index);
    bed_string(name);
    fputs(",\"values\":[", stdout);
    for (size_t n = 0; n < 16; ++n) {
        if (n != 0)
            putchar(',');
        if (n == 4)
            printf("\"%08" PRIx32 "\"", yaw_word);
        else
            printf("%" PRId32, p[n]);
    }
    printf("],\"direction\":%" PRId32 ",\"target\":[%" PRId32 ",%" PRId32 ",%" PRId32
           "],\"head\":[%" PRId32 ",%" PRId32 ",%" PRId32 "],\"result\":\"%s\",\"exception\":",
           direction, p[1], y, p[3], hx, y, hz,
           state.returned ? (state.result ? "true" : "false") : "UNRETURNED");
    bed_string(exception);
    fputs(",\"message\":", stdout);
    bed_string(message);
    printf(",\"count_before\":%" PRId32 ",\"count_after\":%" PRId32 ",\"stack_id\":%" PRId32
           ",\"animation\":%" PRId32 ",\"foot_before\":",
           p[5], after.count, after.id, after.animation);
    bed_pair(foot_before);
    fputs(",\"foot_after\":", stdout);
    bed_pair(foot_after);
    fputs(",\"head_before\":", stdout);
    bed_pair(head_before);
    fputs(",\"head_after\":", stdout);
    bed_pair(head_after);
    uint32_t yaw_after;
    memcpy(&yaw_after, &f->player->base.yaw, sizeof yaw_after);
    printf(",\"bed_id_after\":%" PRId32 ",\"yaw_after\":\"%08" PRIx32
           "\",\"remote_after\":%s,\"notifications\":[",
           bed->fields.bn, yaw_after, f->world->fields.B ? "true" : "false");
    for (size_t n = 0; n < o->count; ++n) {
        const bed_event *event = &o->events[n];
        printf("%s[\"%s\"", n == 0 ? "" : ",", event->range ? "range" : "cell");
        for (size_t k = 0; k < (event->range ? 6u : 3u); ++k)
            printf(",%" PRId32, event->xyz[k]);
        putchar(']');
    }
    fputs("],\"observer_actions\":[", stdout);
    bool comma = false;
    if (o->foot && (p[15] & 1) != 0) {
        printf("[\"prepare-head\",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 "]", hx,
               y, hz, p[12], direction + 8);
        comma = true;
    }
    if (o->foot && (p[15] & 8) != 0) {
        printf("%s[\"stack-count\",100]", comma ? "," : "");
        comma = true;
    }
    if (o->foot && (p[15] & 16) != 0)
        printf("%s[\"bed-id\",1]", comma ? "," : "");
    fputs("],\"changes\":[", stdout);
    size_t offset = 0;
    bool first = true;
    for (size_t n = 0; n < BED_CHUNKS; ++n)
        for (size_t k = 0; k < BED_PLANES; ++k) {
            const uint8_t *current = bed_plane(chunks[n], k);
            for (size_t j = 0; j < bed_lengths[k]; ++j)
                if (prior[offset + j] != current[j]) {
                    printf("%s[%zu,%zu,%zu,%u,%u]", first ? "" : ",", n, k, j,
                           (unsigned)prior[offset + j], (unsigned)current[j]);
                    first = false;
                }
            offset += bed_lengths[k];
        }
    printf("],\"light_queue\":%zu,\"random_unchanged\":true}\n", lights.count);
    printf("BED_PLACEMENT_NATIVE\t{\"index\":%zu,\"status\":%d,\"native_policy\":%s,\"source_"
           "exception\":%s,\"source_error\":%s,\"observer_refused\":%s,\"source_pc\":%u,\"foot_"
           "returned\":%s,\"foot_value\":%s,\"head_returned\":%s,\"head_value\":%s}\n",
           index, (int)status, error.native_policy ? "true" : "false",
           error.source_exception ? "true" : "false", error.source_error ? "true" : "false",
           o->refused ? "true" : "false", state.source_pc,
           state.bed_foot_setter_returned ? "true" : "false",
           state.bed_foot_setter_value ? "true" : "false",
           state.bed_head_setter_returned ? "true" : "false",
           state.bed_head_setter_value ? "true" : "false");
    CHECK(mc_item_use_retire(&call, NULL) == MC_ITEM_USE_OK && call == NULL &&
          f->player->holds == player_holds);
    mc_math_class_state ms;
    CHECK(mc_math_classes_state(math, &ms, NULL) == MC_MATH_CLASS_OK && ms.holds == 1);
    bed->fields.bn = 26;
    mc_item_stack_release(stack);
    return true;
}

static char *bed_field(char **cursor) {
    if (*cursor == NULL)
        return NULL;
    char *p = *cursor;
    size_t length = strcspn(p, "\t\r\n");
    char delimiter = p[length];
    if (delimiter == 0)
        *cursor = NULL;
    else {
        p[length] = 0;
        *cursor = p + length + 1;
        if (delimiter != '\t') {
            *cursor += strspn(*cursor, "\r\n");
            if (**cursor == 0)
                *cursor = NULL;
        }
    }
    return p;
}

static bool bed_wrapper_controls(fixture *f, mc_chunk_state **chunks, mc_math_classes *math) {
    mc_item_stack *stack = NULL;
    mc_item_use_call *call = NULL;
    mc_item_use_error error;
    mc_item_use_state state;
    CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(stack, 355, 2, 0) == MC_ITEM_STACK_OK);
    CHECK(mc_item_use_on_block_begin(f->app.registry, stack, NULL, NULL, 8, 63, 8, 0, &call,
                                     &error) == MC_ITEM_USE_OK &&
          mc_item_use_inspect(call, &state) && state.returned && !state.result &&
          !state.statistic_entered && mc_item_use_retire(&call, NULL) == MC_ITEM_USE_OK);
    CHECK(mc_item_use_on_block_begin(f->app.registry, stack, f->player, NULL, 8, 63, 8, 1, &call,
                                     &error) == MC_ITEM_USE_MATH &&
          mc_item_use_inspect(call, &state) && state.source_pc == 37 && !state.returned &&
          mc_item_use_retire(&call, NULL) == MC_ITEM_USE_OK);
    bed_reset(chunks);
    bed_put(chunks, 8, 63, 8, 1, 0);
    bed_put(chunks, 8, 63, 9, 1, 0);
    f->world->fields.B = false;
    f->player->base.yaw = 0;
    const mc_statistic *used = f->app.registry->statistics.E[355];
    int32_t before, after;
    CHECK(used != NULL && mc_client_stats_writer_value(f->app.stats_writer, used, &before, NULL) ==
                              MC_CLIENT_STATS_OK);
    mc_item_use_context context = {math};
    CHECK(mc_item_use_on_block_with_context_begin(f->app.registry, stack, f->player, f->world, 8,
                                                  63, 8, 1, &context, &call,
                                                  &error) == MC_ITEM_USE_OK &&
          mc_item_use_inspect(call, &state) && state.returned && state.result &&
          state.count_stored && state.statistic_returned && state.bed_foot_setter_returned &&
          state.bed_head_setter_returned && mc_item_use_retire(&call, NULL) == MC_ITEM_USE_OK);
    CHECK(mc_client_stats_writer_value(f->app.stats_writer, used, &after, NULL) ==
              MC_CLIENT_STATS_OK &&
          after == mc_java_i32_add(before, 1));
    mc_item_stack_release(stack);
    return true;
}

static bool bed_context_controls(fixture *f, mc_math_classes *math) {
    const mc_registry_item_array *array = NULL;
    const mc_item *item = NULL;
    CHECK(mc_registry_item_c_get(f->app.registry, &array, NULL) == MC_REGISTRY_OK &&
          mc_registry_item_c_at(f->app.registry, array, 355, &item, NULL) == MC_REGISTRY_OK);
    mc_math_class_state before, after;
    CHECK(mc_math_classes_state(math, &before, NULL) == MC_MATH_CLASS_OK &&
          before.phase == MC_MATH_CLASS_COLD && before.holds == 1);
    mc_item_use_context context = {math};
    mc_item_use_call *call = NULL;
    mc_item_use_error error;
    mc_item_use_state state;
    CHECK(mc_item_use_bed_begin(f->app.registry, item, NULL, NULL, NULL, 8, 63, 8, 0, &context,
                                &call, &error) == MC_ITEM_USE_OK &&
          mc_item_use_inspect(call, &state) && state.returned && !state.result &&
          state.source_pc == 7 && mc_item_use_retire(&call, NULL) == MC_ITEM_USE_OK);
    CHECK(mc_math_classes_state(math, &after, NULL) == MC_MATH_CLASS_OK &&
          after.phase == MC_MATH_CLASS_COLD && after.holds == 1 && after.source_a == NULL);
    CHECK(mc_item_use_bed_begin(f->app.registry, item, NULL, NULL, NULL, 8, 63, 8, 1, &context,
                                &call, &error) == MC_ITEM_USE_SOURCE_NULL &&
          mc_item_use_inspect(call, &state) && state.source_pc == 23 &&
          mc_item_use_retire(&call, NULL) == MC_ITEM_USE_OK);
    const unsigned csr = _mm_getcsr();
    _mm_setcsr(csr | 0x8000u);
    mc_item_use_status status = mc_item_use_bed_begin(f->app.registry, item, NULL, f->player, NULL,
                                                      8, 63, 8, 1, &context, &call, &error);
    _mm_setcsr(csr);
    CHECK(status == MC_ITEM_USE_FLOATING && mc_item_use_inspect(call, &state) &&
          state.source_pc == 28 && mc_math_classes_state(math, &after, NULL) == MC_MATH_CLASS_OK &&
          after.phase == MC_MATH_CLASS_COLD && after.holds == 1 &&
          mc_item_use_retire(&call, NULL) == MC_ITEM_USE_OK);
    mc_math_classes *limited = NULL;
    CHECK(mc_math_classes_create(1024, &limited, NULL) == MC_MATH_CLASS_OK);
    context.math = limited;
    CHECK(mc_item_use_bed_begin(f->app.registry, item, NULL, f->player, NULL, 8, 63, 8, 1, &context,
                                &call, &error) == MC_ITEM_USE_MATH &&
          mc_item_use_inspect(call, &state) && state.source_pc == 37 && !state.returned &&
          mc_math_classes_state(limited, &after, NULL) == MC_MATH_CLASS_OK &&
          after.phase == MC_MATH_CLASS_FAILED && after.holds == 2 &&
          mc_item_use_retire(&call, NULL) == MC_ITEM_USE_OK &&
          mc_math_classes_release(&limited, NULL) == MC_MATH_CLASS_OK);
    return true;
}

static bool bed_run(fixture *f, const char *path) {
    const mc_chunk_acquire_context *acquisition = mc_world_runtime_acquisition(f->world);
    CHECK(acquisition != NULL && acquisition->cache != NULL && acquisition->cache->count == 0);
    mc_chunk_state *chunks[BED_CHUNKS] = {0};
    for (size_t n = 0; n < BED_CHUNKS; ++n) {
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
    }
    mc_math_classes *math = NULL;
    CHECK(mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK);
    CHECK(bed_context_controls(f, math));
    bed_observer *observer = calloc(1, sizeof(*observer));
    mc_world_access *access = NULL;
    const mc_world_access_callbacks callbacks = {.cell = bed_notice, .range = bed_range};
    CHECK(observer != NULL &&
          mc_world_access_create(&callbacks, observer, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->world, access, NULL) == MC_WORLD_STORAGE_OK);
    size_t bytes = bed_snapshot(chunks, NULL);
    uint8_t *prior = malloc(bytes);
    CHECK(prior != NULL);
    FILE *input = fopen(path, "rb");
    CHECK(input != NULL);
    char line[2048];
    size_t rows = 0;
    while (fgets(line, sizeof line, input) != NULL) {
        CHECK(strchr(line, '\n') != NULL);
        char *cursor = line, *name = bed_field(&cursor), *end = NULL;
        CHECK(name != NULL && strlen(name) < 96);
        int32_t p[17];
        for (size_t n = 0; n < 17; ++n) {
            char *token = bed_field(&cursor);
            CHECK(token != NULL);
            errno = 0;
            if (n == 4) {
                unsigned long long word = strtoull(token, &end, 16);
                CHECK(errno == 0 && *end == 0 && word <= UINT32_MAX);
                uint32_t v = (uint32_t)word;
                memcpy(&p[n], &v, sizeof v);
            } else {
                long long v = strtoll(token, &end, 10);
                CHECK(errno == 0 && *end == 0 && v >= INT32_MIN && v <= INT32_MAX);
                p[n] = (int32_t)v;
            }
        }
        CHECK(bed_field(&cursor) == NULL && rows < BED_ROWS &&
              bed_project(f, chunks, observer, math, rows, name, p, prior));
        ++rows;
    }
    CHECK(!ferror(input) && fclose(input) == 0 && rows == BED_ROWS);
    /* These separate authored controls enter the ordinary IZ/statistic path. */
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->world, access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(bed_wrapper_controls(f, chunks, math));
    CHECK(mc_math_classes_release(&math, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_chunk_cache_init(acquisition->cache, acquisition->cache->entries,
                              acquisition->cache->capacity, acquisition->cache->insertions,
                              acquisition->cache->insertion_capacity));
    for (size_t n = 0; n < BED_CHUNKS; ++n)
        CHECK(mc_world_store_retire_chunk(f->store, chunks[n], NULL) == MC_WORLD_STORAGE_OK);
    free(prior);
    free(observer);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !setup(f, argv[1], "bed-reference", true) || !bed_run(f, argv[3]) ||
        !teardown(f))
        return 1;
    free(f);
    printf("PASS: 72 Bed direct profiles and wrapper controls, %zu actual-owner checks\n", checks);
    return 0;
}
