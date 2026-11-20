/* Authored direct-VC and IZ prefix controls on genuine constructed owners.
 * No GUI callback is substituted. A nonnull Sign deliberately stops at the
 * missing DC editor child, after proving its real World/Stack/Tile prefix. */
#define MC_PLAYER_STATISTICS_WORLD_SEED INT64_C(23)
#define MC_PLAYER_STATISTICS_MAX_PROVIDER_CHUNKS 4
#define main mc_sign_unused_statistics_fixture_main
#include "../../client/statistics/test_player_dispatch.c"
#undef main

#include "item/registry/registry_internal.h"
#include "item/stack/source_internal.h"
#include "item/use/sign_internal.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/lighting.h"
#include "mc/world/runtime/tile_entities/tile_entities.h"
#include "world/chunk/object_state_internal.h"
#include "world/runtime/lighting_internal.h"
#include "world/runtime/runtime_internal.h"
#include "world/storage/generated_internal.h"

typedef struct sign_case {
    int32_t face, clicked, target, count;
    float yaw;
    unsigned int pc;
    mc_item_use_status status;
    bool null_world, null_player, null_stack, wrapper;
    bool placed, counted, tile, result;
    int32_t metadata;
} sign_case;

static size_t sign_map_count(fixture *f, mc_chunk_state **chunks) {
    size_t count = 0;
    for (size_t n = 0; n < 4; ++n) {
        mc_world_chunk_fields fields;
        if (mc_world_chunk_fields_get(mc_world_store_generated_chunk(f->store, chunks[n]),
                                      &fields) != MC_WORLD_STORAGE_OK ||
            fields.l == NULL)
            return SIZE_MAX;
        count += fields.l->count;
    }
    return count;
}

static mc_tile_entity *sign_map_find(fixture *f, mc_chunk_state **chunks, int32_t x, int32_t y,
                                     int32_t z) {
    mc_world_chunk *chunk =
        mc_world_store_generated_chunk(f->store, chunks[(size_t)(x >> 4) * 2u + (size_t)(z >> 4)]);
    mc_world_chunk_fields fields;
    if (mc_world_chunk_fields_get(chunk, &fields) != MC_WORLD_STORAGE_OK || fields.l == NULL)
        return NULL;
    for (size_t n = 0; n < fields.l->count; ++n) {
        mc_live_tile_entity_map_entry entry;
        if (mc_live_tile_entities_map_entry_at(chunk, n, &entry) != MC_WORLD_STORAGE_OK)
            return NULL;
        if (entry.x == (x & 15) && entry.y == y && entry.z == (z & 15))
            return entry.tile_entity;
    }
    return NULL;
}

static size_t sign_plane_snapshot(mc_chunk_state **chunks, uint8_t *bytes, bool compare) {
    size_t offset = 0;
    for (size_t n = 0; n < 4; ++n) {
        const uint8_t *planes[] = {chunks[n]->data->blocks, chunks[n]->data->metadata,
                                   chunks[n]->data->skylight, chunks[n]->data->block_light,
                                   chunks[n]->data->heightmap};
        const size_t sizes[] = {MC_CHUNK_BLOCKS, MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_NIBBLE_BYTES,
                                MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_HEIGHTMAP_BYTES};
        for (size_t plane = 0; plane < 5; ++plane) {
            if (bytes != NULL) {
                if (compare && memcmp(bytes + offset, planes[plane], sizes[plane]) != 0)
                    return 0;
                if (!compare)
                    memcpy(bytes + offset, planes[plane], sizes[plane]);
            }
            offset += sizes[plane];
        }
    }
    return offset;
}

static void sign_put(mc_chunk_state **chunks, int32_t x, int32_t y, int32_t z, int32_t id) {
    if (y < 0 || y >= 128)
        return;
    mc_chunk_state *chunk = chunks[(size_t)(x >> 4) * 2u + (size_t)(z >> 4)];
    const size_t cell = (((size_t)x & 15u) * 16u + ((size_t)z & 15u)) * 128u + (size_t)y;
    chunk->data->blocks[cell] = (uint8_t)id;
    chunk->data->metadata[cell >> 1] &= (uint8_t) ~(15u << ((unsigned)(cell & 1u) * 4u));
}

static bool sign_profile(fixture *f, mc_chunk_state **chunks, mc_math_classes *math,
                         const mc_item *item, size_t index, const sign_case *profile) {
    const int32_t x = 3 + (int32_t)(index % 7u) * 4;
    const int32_t z = 3 + (int32_t)(index / 7u) * 4;
    const int32_t y = profile->count == -100 ? 127 : 63;
    int32_t tx = x, ty = y, tz = z;
    if (profile->face == 1)
        ++ty;
    if (profile->face == 2)
        --tz;
    if (profile->face == 3)
        ++tz;
    if (profile->face == 4)
        --tx;
    if (profile->face == 5)
        ++tx;
    sign_put(chunks, x, y, z, profile->clicked);
    if (tx != x || ty != y || tz != z)
        sign_put(chunks, tx, ty, tz, profile->target);
    f->player->base.yaw = profile->yaw; /* Declared public input prestate. */
    const size_t player_holds = f->player->holds;
    const size_t map_before = sign_map_count(f, chunks);
    const size_t plane_bytes = sign_plane_snapshot(chunks, NULL, false);
    uint8_t *planes = malloc(plane_bytes);
    CHECK(planes != NULL && sign_plane_snapshot(chunks, planes, false) == plane_bytes &&
          sign_map_find(f, chunks, tx, ty, tz) == NULL && map_before != SIZE_MAX);
    const mc_statistic *used = f->app.registry->statistics.E[323];
    int32_t total_before = 0, unsent_before = 0;
    bool total_present = false, unsent_present = false;
    CHECK(used != NULL &&
          mc_client_stats_map_get(f->app.stats_writer->total, used, &total_present, &total_before,
                                  NULL) == MC_CLIENT_STATS_OK &&
          mc_client_stats_map_get(f->app.stats_writer->unsent, used, &unsent_present,
                                  &unsent_before, NULL) == MC_CLIENT_STATS_OK);
    mc_item_stack *stack = NULL;
    CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(stack, 323, profile->count == -100 ? 2 : profile->count, 0) ==
              MC_ITEM_STACK_OK);
    const mc_item_use_context context = {.math = math, .world_code_classes = f->app.world_classes};
    mc_item_use_call *call = NULL;
    mc_item_use_error error;
    mc_item_use_state state;
    const mc_item_use_status status =
        profile->wrapper
            ? mc_item_use_on_block_with_context_begin(
                  f->app.registry, profile->null_stack ? NULL : stack,
                  profile->null_player ? NULL : f->player, profile->null_world ? NULL : f->world, x,
                  y, z, profile->face, &context, &call, &error)
            : mc_item_use_sign_begin(f->app.registry, item, profile->null_stack ? NULL : stack,
                                     profile->null_player ? NULL : f->player,
                                     profile->null_world ? NULL : f->world, x, y, z, profile->face,
                                     &context, &call, &error);
    CHECK(status == profile->status && call != NULL && mc_item_use_inspect(call, &state) &&
          !state.active && state.returned == (status == MC_ITEM_USE_OK) &&
          state.source_pc == profile->pc && state.setter_returned == profile->placed &&
          state.count_stored == profile->counted && (state.sign_tile != NULL) == profile->tile &&
          (!state.returned || state.result == profile->result));
    const bool material_returned = profile->face != 0 && !profile->null_world;
    const bool predicate = material_returned && profile->clicked != 0;
    CHECK(state.sign_material_returned == material_returned && state.sign_solid == predicate &&
          state.predicate_entered == predicate && state.predicate_returned == predicate);
    if (predicate)
        CHECK(state.can_place ==
              (profile->target == 0 && profile->face >= 1 && profile->face <= 5));
    if (!profile->placed)
        CHECK(sign_plane_snapshot(chunks, planes, true) == plane_bytes);
    free(planes);
    mc_tile_entity *actual_tile = sign_map_find(f, chunks, tx, ty, tz);
    const bool tile_created = profile->placed && ty < 128;
    CHECK(sign_map_count(f, chunks) == map_before + (tile_created ? 1u : 0u) &&
          (actual_tile != NULL) == tile_created &&
          (!profile->tile || actual_tile == state.sign_tile));
    if (profile->status == MC_ITEM_USE_SOURCE_NULL)
        CHECK(error.source_exception && !error.native_policy && !error.source_error &&
              error.exception_class != NULL &&
              strcmp(error.exception_class, "java/lang/NullPointerException") == 0);
    if (profile->status == MC_ITEM_USE_UNSUPPORTED)
        CHECK(error.native_policy && !error.source_exception && state.sign_cast_returned &&
              !state.sign_callback_entered && !state.statistic_entered);
    if (profile->placed) {
        CHECK(state.metadata_returned && state.metadata == profile->metadata &&
              state.setter_value == (ty < 128));
        int32_t id = -1, metadata = -1;
        CHECK(mc_world_runtime_block_get(f->world, tx, ty, tz, &id, NULL) == MC_WORLD_STORAGE_OK &&
              mc_world_runtime_metadata_get(f->world, tx, ty, tz, &metadata, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              id == (ty < 128 ? (profile->face == 1 ? 63 : 68) : 0) &&
              metadata == (ty < 128 ? profile->metadata : 0));
    }
    int32_t count = 123;
    CHECK(mc_item_stack_source_count_get(stack, &count) == MC_ITEM_STACK_OK &&
          count == (profile->count == -100 ? 2 : profile->count) - (profile->counted ? 1 : 0));
    if (tile_created) {
        mc_tile_entity_snapshot sign;
        CHECK(mc_tile_entity_get_snapshot(actual_tile, &sign) == MC_TILE_ENTITY_OK &&
              sign.sign_read_flag && sign.sign_edit_row == -1 && sign.sign_line_count == 4);
        mc_tile_entity_name *first = NULL;
        CHECK(mc_tile_entity_sign_get_line(actual_tile, 0, &first) == MC_TILE_ENTITY_OK);
        for (int32_t row = 0; row < 4; ++row) {
            mc_tile_entity_name *line = NULL;
            mc_tile_entity_name_view view;
            CHECK(mc_tile_entity_sign_get_line(actual_tile, row, &line) == MC_TILE_ENTITY_OK &&
                  line == first && mc_tile_entity_name_get_view(line, &view) == MC_TILE_ENTITY_OK &&
                  !view.is_null && view.text.length == 0);
            mc_tile_entity_name_release(line);
        }
        mc_tile_entity_name_release(first);
    }
    if (profile->wrapper && profile->result)
        CHECK(state.statistic_entered && state.statistic_returned);
    int32_t total_after = 0, unsent_after = 0;
    bool total_after_present = false, unsent_after_present = false;
    const bool incremented = profile->wrapper && profile->result;
    CHECK(mc_client_stats_map_get(f->app.stats_writer->total, used, &total_after_present,
                                  &total_after, NULL) == MC_CLIENT_STATS_OK &&
          mc_client_stats_map_get(f->app.stats_writer->unsent, used, &unsent_after_present,
                                  &unsent_after, NULL) == MC_CLIENT_STATS_OK &&
          total_after_present == (total_present || incremented) &&
          unsent_after_present == (unsent_present || incremented) &&
          total_after == total_before + (incremented ? 1 : 0) &&
          unsent_after == unsent_before + (incremented ? 1 : 0));
    CHECK(mc_item_use_retire(&call, NULL) == MC_ITEM_USE_OK && call == NULL &&
          f->player->holds == player_holds);
    mc_item_stack_release(stack);
    return true;
}

static bool sign_run(fixture *f) {
    const mc_chunk_acquire_context *acquisition = mc_world_runtime_acquisition(f->world);
    CHECK(acquisition != NULL && acquisition->cache != NULL && acquisition->cache->count == 0);
    mc_chunk_state *chunks[4] = {0};
    for (size_t n = 0; n < 4; ++n) {
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
    const mc_registry_item_array *items = NULL;
    const mc_item *item = NULL;
    CHECK(mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK &&
          mc_registry_item_c_get(f->app.registry, &items, NULL) == MC_REGISTRY_OK &&
          mc_registry_item_c_at(f->app.registry, items, 323, &item, NULL) == MC_REGISTRY_OK &&
          item != NULL && item->kind == MC_ITEM_SIGN && item->fields.bg == 1);
    size_t index = 0;
    for (int32_t bin = 0; bin < 16; ++bin) {
        const sign_case c = {.face = 1,
                             .clicked = 1,
                             .count = 2,
                             .yaw = (float)bin * 22.5f - 180.0f,
                             .pc = 190,
                             .status = MC_ITEM_USE_UNSUPPORTED,
                             .placed = true,
                             .counted = true,
                             .tile = true,
                             .metadata = bin};
        CHECK(sign_profile(f, chunks, math, item, index++, &c));
    }
    for (int32_t face = 2; face <= 5; ++face) {
        const sign_case c = {.face = face,
                             .clicked = 1,
                             .count = 2,
                             .pc = 190,
                             .status = MC_ITEM_USE_UNSUPPORTED,
                             .placed = true,
                             .counted = true,
                             .tile = true,
                             .metadata = face};
        CHECK(sign_profile(f, chunks, math, item, index++, &c));
    }
    const sign_case cases[] = {
        {.face = 0,
         .count = 2,
         .pc = 6,
         .null_stack = true,
         .null_world = true,
         .null_player = true},
        {.face = 1, .count = 2, .pc = 24, .null_player = true},
        {.face = 1, .clicked = 1, .target = 1, .count = 2, .pc = 87, .null_player = true},
        {.face = 6, .clicked = 1, .count = 2, .pc = 87, .null_player = true},
        {.face = -1, .clicked = 1, .count = 2, .pc = 87, .null_player = true},
        {.face = 1,
         .clicked = 1,
         .count = 2,
         .pc = 14,
         .status = MC_ITEM_USE_SOURCE_NULL,
         .null_world = true},
        {.face = 1,
         .clicked = 1,
         .count = 2,
         .pc = 108,
         .status = MC_ITEM_USE_SOURCE_NULL,
         .null_player = true},
        {.face = 2,
         .clicked = 1,
         .count = 2,
         .pc = 190,
         .status = MC_ITEM_USE_SOURCE_NULL,
         .null_player = true,
         .placed = true,
         .counted = true,
         .tile = true,
         .metadata = 2},
        {.face = 2,
         .clicked = 1,
         .count = 2,
         .pc = 159,
         .status = MC_ITEM_USE_SOURCE_NULL,
         .null_stack = true,
         .placed = true,
         .metadata = 2},
        {.face = 1,
         .clicked = 1,
         .count = 0,
         .pc = 190,
         .status = MC_ITEM_USE_UNSUPPORTED,
         .placed = true,
         .counted = true,
         .tile = true,
         .metadata = 8},
        {.face = 1,
         .clicked = 1,
         .count = -100,
         .pc = 194,
         .placed = true,
         .counted = true,
         .result = true,
         .metadata = 8},
        {.face = 1,
         .clicked = 1,
         .count = -100,
         .pc = 39,
         .wrapper = true,
         .placed = true,
         .counted = true,
         .result = true,
         .metadata = 8},
    };
    for (size_t n = 0; n < sizeof(cases) / sizeof(*cases); ++n)
        CHECK(sign_profile(f, chunks, math, item, index++, &cases[n]));
    CHECK(index == 32 && mc_math_classes_release(&math, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_chunk_cache_init(acquisition->cache, acquisition->cache->entries,
                              acquisition->cache->capacity, acquisition->cache->insertions,
                              acquisition->cache->insertion_capacity));
    for (size_t n = 0; n < 4; ++n)
        CHECK(mc_world_store_retire_chunk(f->store, chunks[n], NULL) == MC_WORLD_STORAGE_OK);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !setup(f, argv[1], "sign-prefix", true) || !sign_run(f) || !teardown(f))
        return 1;
    free(f);
    printf("PASS: 32 authored Sign placement prefixes, %zu actual-owner checks\n", checks);
    return 0;
}
