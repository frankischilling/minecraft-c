#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/world/runtime/provider_internal.h"
#include "../../../src/world/runtime/runtime_internal.h"
#include "../../support/java_random.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/environment.h"
#include "mc/world/storage/chunk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Constructed native store/World cases, not an original World oracle. The
 * driver supplies a fresh owned UTF-8 root as hex; all files are retained. */
static unsigned int checks;
static mc_math_trig trig;
static uint8_t blocks[MC_CHUNK_BLOCKS];
static const uint8_t zero_blocks[MC_CHUNK_BLOCKS] = {0};

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static uint64_t uniquifier(const mc_java_random_profile *profile)
{
    return atomic_load_explicit(&profile->uniquifier, memory_order_relaxed);
}

static uint64_t advanced(uint64_t value, unsigned int constructors)
{
    while (constructors-- != 0)
        value *= UINT64_C(1181783497276652981);
    return value;
}

static bool cold(const mc_java_random_profile *profile)
{
    return profile->math == NULL &&
           atomic_load_explicit(&profile->holder_phase, memory_order_acquire) ==
               (unsigned int)MC_JAVA_RANDOM_HOLDER_COLD;
}

static uint64_t double_bits(double value)
{
    uint64_t bits;
    _Static_assert(sizeof(bits) == sizeof(value), "binary64 required");
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool same_info(const mc_level_info *left, const mc_level_info *right)
{
    return left->seed == right->seed && left->spawn_x == right->spawn_x &&
           left->spawn_y == right->spawn_y && left->spawn_z == right->spawn_z &&
           left->time == right->time && left->last_played == right->last_played &&
           left->size_on_disk == right->size_on_disk &&
           left->level_name.units == right->level_name.units &&
           left->level_name.length == right->level_name.length && left->version == right->version &&
           left->rain_time == right->rain_time && left->thunder_time == right->thunder_time &&
           left->raining == right->raining && left->thundering == right->thundering &&
           left->player == right->player && left->player_dimension == right->player_dimension;
}

static bool make_player(mc_world_info_player **out)
{
    const uint16_t key[] = {'D', 'i', 'm', 'e', 'n', 's', 'i', 'o', 'n'};
    const uint16_t name[] = {'B', 'e', 'f', 'o', 'r', 'e'};
    mc_nbt_tag dimension = {.type = MC_NBT_INT, .name = {key, 9}};
    const mc_nbt_tag *items[] = {&dimension};
    mc_nbt_tag player = {.type = MC_NBT_COMPOUND, .name = {name, 6}};
    player.value.compound.items = items;
    player.value.compound.length = 1;
    return mc_world_info_player_copy(&player, NULL, out, NULL) == MC_WORLD_INFO_OK;
}

static bool make_metadata(mc_world_info_name *name, mc_world_info_player *player,
                          mc_world_info **out)
{
    mc_world_info *info = NULL;
    bool ok = false;
    if (mc_world_info_allocate(MC_WORLD_INFO_CLIENT, &info) != MC_WORLD_INFO_OK ||
        mc_world_info_construct(info, 23, name) != MC_WORLD_INFO_OK ||
        mc_world_info_time_set(info, 5999) != MC_WORLD_INFO_OK ||
        mc_world_info_version_set(info, 17) != MC_WORLD_INFO_OK ||
        mc_world_info_raining_set(info, true) != MC_WORLD_INFO_OK ||
        mc_world_info_thundering_set(info, true) != MC_WORLD_INFO_OK ||
        mc_world_info_player_set(info, player) != MC_WORLD_INFO_OK)
        goto cleanup;
    *out = info;
    info = NULL;
    ok = true;
cleanup:
    mc_world_info_release(info);
    return ok;
}

static int check_provider(mc_world_runtime *world, mc_world_store *store,
                          mc_world_provider_fields *out)
{
    int result = 1;
    mc_world_runtime_fields f;
    mc_world_dimension_fields dimension;
    mc_world_dimension_biome_fields biome;
    mc_world_provider_fields p;
    mc_world_storage_error error;
    const mc_chunk_acquire_context *acquisition = mc_world_runtime_acquisition(world);
    CHECK(mc_world_runtime_fields_get(world, &f) == MC_WORLD_STORAGE_OK && f.v != NULL);
    CHECK(mc_world_provider_bind_fields(f.v, &p, &error) == MC_WORLD_STORAGE_OK);
    CHECK(p.world == world && p.store == store && p.max_chunks == 4 &&
          p.direct_allocation_bytes <= p.max_direct_allocation_bytes);
    CHECK(mc_world_dimension_fields_get(f.t, &dimension) == MC_WORLD_DIMENSION_OK &&
          dimension.a == world);
    CHECK(p.biome == dimension.b &&
          mc_world_dimension_biome_fields_get(p.biome, &biome) == MC_WORLD_DIMENSION_OK);
    CHECK(!biome.fixed && biome.normal == p.manager && p.manager->initialized == 1 &&
          biome.holds >= 2);
    CHECK(p.generator != NULL && p.generator->initialized &&
          p.generator->phase == MC_NORMAL_GENERATOR_PHASE_READY &&
          p.generator->origin == MC_NORMAL_GENERATOR_PROVIDER && p.generator->cave_range == 8);
    CHECK(p.generator->world_seed == 23 && p.generator->biome_manager == p.manager &&
          p.generator->trig == &trig);
    CHECK(p.generator->cave_random.state <= UINT64_C(0xffffffffffff) &&
          double_bits(p.generator->cave_random.next_next_gaussian) == 0 &&
          !p.generator->cave_random.have_next_next_gaussian && f.r != &p.generator->cave_random);
    CHECK(p.cache == world->options.cache && p.pending == world->options.pending &&
          p.generator == world->options.generator && world->options.generation == NULL);
    CHECK(mc_chunk_cache_validate(p.cache) && p.cache->capacity == 4 && p.cache->count == 0 &&
          p.cache->insertion_capacity == 4 && p.cache->insertion_count == 0);
    CHECK(mc_chunk_pending_validate(p.pending) && p.pending->capacity == 4 &&
          p.pending->count == 0 && p.pending->history != NULL &&
          mc_java_int_set_size(p.pending->history) == 0);
    CHECK(p.fallback == &world->fallback && acquisition != NULL &&
          acquisition->fallback == p.fallback && acquisition->cache == p.cache &&
          acquisition->pending == p.pending);
    CHECK(p.fallback->blank && p.fallback->never_save && !p.fallback->active &&
          p.fallback->data != NULL && p.fallback->data->blocks != NULL &&
          p.fallback->data->blocks_capacity == MC_CHUNK_BLOCKS);
    CHECK(memcmp(p.fallback->data->blocks, zero_blocks, MC_CHUNK_BLOCKS) == 0);
    CHECK(memcmp(p.fallback->data->metadata, zero_blocks, MC_CHUNK_NIBBLE_BYTES) == 0 &&
          memcmp(p.fallback->data->skylight, zero_blocks, MC_CHUNK_NIBBLE_BYTES) == 0 &&
          memcmp(p.fallback->data->block_light, zero_blocks, MC_CHUNK_NIBBLE_BYTES) == 0 &&
          memcmp(p.fallback->data->heightmap, zero_blocks, MC_CHUNK_HEIGHTMAP_BYTES) == 0 &&
          p.fallback->data->minimum_height == 0);
    for (size_t x = 0; x < MC_WORLD_PROVIDER_GRID_WIDTH; ++x)
        for (size_t z = 0; z < MC_WORLD_PROVIDER_GRID_WIDTH; ++z)
            CHECK(p.constructor_grid[x][z] == 0);
    *out = p;
    result = 0;
cleanup:
    return result;
}

static size_t index_of(size_t x, size_t y, size_t z)
{
    return (x * 16u + z) * 128u + y;
}

static bool same_chunk_except_dirty(const mc_chunk_state *left, const mc_chunk_state *right)
{
    return left->data == right->data && left->x == right->x && left->z == right->z &&
           left->blank == right->blank && left->active == right->active &&
           left->terrain_populated == right->terrain_populated &&
           left->never_save == right->never_save && left->has_entities == right->has_entities &&
           left->last_save_time == right->last_save_time;
}

static bool same_nonmetadata(const mc_chunk_data *data, const mc_chunk_data *before,
                             const uint8_t before_blocks[MC_CHUNK_BLOCKS])
{
    return data->blocks == before->blocks && data->blocks_capacity == before->blocks_capacity &&
           memcmp(data->blocks, before_blocks, MC_CHUNK_BLOCKS) == 0 &&
           memcmp(data->skylight, before->skylight, MC_CHUNK_NIBBLE_BYTES) == 0 &&
           memcmp(data->block_light, before->block_light, MC_CHUNK_NIBBLE_BYTES) == 0 &&
           memcmp(data->heightmap, before->heightmap, MC_CHUNK_HEIGHTMAP_BYTES) == 0 &&
           data->minimum_height == before->minimum_height;
}

/* Actual in-memory typed storage round trip of the edited chunk. No World save,
 * file output, session check or original-runtime observation is introduced. */
static int test_metadata_codec(const mc_chunk_state *chunk)
{
    int result = 1;
    mc_stored_chunk *copy = NULL, *decoded = NULL;
    mc_nbt_document *document = NULL;
    uint8_t *bytes = NULL;
    size_t length = 0, written = 0, consumed = 0;
    const uint16_t level_key[] = {'L', 'e', 'v', 'e', 'l'};
    const uint16_t data_key[] = {'D', 'a', 't', 'a'};
    CHECK(mc_stored_chunk_create(chunk, NULL, &copy, NULL) == MC_STORED_CHUNK_OK);
    CHECK(mc_stored_chunk_build(copy, 57, NULL, &document, NULL) == MC_STORED_CHUNK_OK);
    const mc_nbt_tag *level = mc_nbt_compound_get(mc_nbt_document_root(document), level_key, 5);
    const mc_nbt_tag *metadata = mc_nbt_compound_get(level, data_key, 4);
    CHECK(level != NULL && level->type == MC_NBT_COMPOUND && metadata != NULL &&
          metadata->type == MC_NBT_BYTE_ARRAY &&
          metadata->value.byte_array.length == MC_CHUNK_NIBBLE_BYTES &&
          metadata->value.byte_array.data != NULL);
    CHECK(memcmp(metadata->value.byte_array.data, chunk->data->metadata, MC_CHUNK_NIBBLE_BYTES) ==
          0);
    CHECK(mc_nbt_measure(mc_nbt_document_root(document), NULL, &length, NULL) == MC_NBT_OK &&
          length != 0);
    bytes = malloc(length);
    CHECK(bytes != NULL);
    CHECK(mc_nbt_write(mc_nbt_document_root(document), NULL, bytes, length, &written, NULL) ==
              MC_NBT_OK &&
          written == length);
    CHECK(mc_stored_chunk_decode(bytes, written, NULL, &decoded, &consumed, NULL) ==
              MC_STORED_CHUNK_OK &&
          consumed == written);
    const mc_chunk_state *restored = mc_stored_chunk_state(decoded);
    CHECK(restored != NULL && restored != chunk && restored->data != NULL &&
          restored->data != chunk->data && restored->x == chunk->x && restored->z == chunk->z &&
          restored->terrain_populated == chunk->terrain_populated);
    CHECK(memcmp(restored->data->metadata, chunk->data->metadata, MC_CHUNK_NIBBLE_BYTES) == 0 &&
          memcmp(restored->data->blocks, chunk->data->blocks, MC_CHUNK_BLOCKS) == 0 &&
          memcmp(restored->data->skylight, chunk->data->skylight, MC_CHUNK_NIBBLE_BYTES) == 0 &&
          memcmp(restored->data->block_light, chunk->data->block_light, MC_CHUNK_NIBBLE_BYTES) ==
              0 &&
          memcmp(restored->data->heightmap, chunk->data->heightmap, MC_CHUNK_HEIGHTMAP_BYTES) == 0);
    result = 0;
cleanup:
    mc_stored_chunk_free(decoded);
    free(bytes);
    mc_nbt_document_free(document);
    mc_stored_chunk_free(copy);
    return result;
}

static int test_metadata(mc_world_runtime *world, mc_world_store *store, mc_world_regions *regions,
                         const mc_world_provider_fields *provider, mc_chunk_state *chunks[4])
{
    int result = 1;
    int32_t value = -999;
    int32_t ordered_size = -1, membership_size = -1;
    bool changed = true;
    mc_world_storage_error error;
    mc_world_runtime_class_state class_before, class_after;
    const mc_java_random random_before = *world->fields.r;
    mc_chunk_state *selected = NULL;
    mc_chunk_state *const chunk = chunks[0];
    const mc_chunk_state state_before = *chunk;
    mc_chunk_data expected = *chunk->data;
    uint8_t before_blocks[MC_CHUNK_BLOCKS];
    const size_t pair = index_of(15, 10, 15) / 2u;
    const size_t bottom = index_of(0, 0, 0) / 2u;
    const size_t top = index_of(15, 127, 15) / 2u;
    const mc_world_regions_usage usage = mc_world_regions_get_usage(regions);
    const int32_t outside[][3] = {{32000000, 63, 0},  {-32000001, 63, 0}, {0, 63, 32000001},
                                  {0, 63, -32000001}, {128, -1, 128},     {128, 128, 128}};
    memcpy(before_blocks, chunk->data->blocks, sizeof(before_blocks));
    CHECK(mc_world_runtime_classes_get(world->classes, &class_before) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_owns_chunk(store, chunk) && !chunk->dirty);
    CHECK(mc_world_runtime_metadata_get(world, -1, 10, -17, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value == 5 && !chunk->dirty);
    CHECK(mc_world_runtime_metadata_get(world, -1, 11, -17, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value == 10 && !chunk->dirty);
    CHECK(mc_world_runtime_metadata_set_quiet(world, -1, 10, -17, INT32_MIN, &changed, &error) ==
              MC_WORLD_STORAGE_OK &&
          changed && chunk->dirty);
    expected.metadata[pair] = 0xa0;
    CHECK(chunk->data->metadata[pair] == 0xa0);
    chunk->dirty = false;
    changed = false;
    CHECK(mc_world_runtime_metadata_set_quiet(world, -1, 11, -17, INT32_MAX, &changed, &error) ==
              MC_WORLD_STORAGE_OK &&
          changed && chunk->dirty);
    expected.metadata[pair] = 0xf0;
    CHECK(chunk->data->metadata[pair] == 0xf0);
    changed = false;
    CHECK(mc_world_runtime_metadata_set_quiet(world, -1, 10, -17, -1, &changed, &error) ==
              MC_WORLD_STORAGE_OK &&
          changed && chunk->data->metadata[pair] == 0xff);
    expected.metadata[pair] = 0xff;
    chunk->dirty = false;
    changed = false;
    CHECK(mc_world_runtime_metadata_set_quiet(world, -1, 10, -17, 31, &changed, &error) ==
              MC_WORLD_STORAGE_OK &&
          changed && chunk->dirty && chunk->data->metadata[pair] == 0xff);
    CHECK(mc_world_runtime_metadata_get(world, -1, 10, -17, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value == 15);
    /* A different negative coordinate pair selects the SAME packed-key owner;
     * local coordinates come from the requested position, not stored x/z. */
    CHECK(mc_chunk_cache_key(-1, -2) == mc_chunk_cache_key(-32769, -32770));
    CHECK(mc_chunk_cache_lookup(provider->cache, -32769, -32770, &selected) && selected == chunk);
    changed = false;
    CHECK(mc_world_runtime_metadata_set_quiet(world, -524289, 11, -524305, 18, &changed, &error) ==
              MC_WORLD_STORAGE_OK &&
          changed && chunk->data->metadata[pair] == 0x2f);
    expected.metadata[pair] = 0x2f;
    CHECK(mc_world_runtime_metadata_get(world, -1, 11, -17, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value == 2 && chunk->x == -1 && chunk->z == -2);
    CHECK(mc_world_runtime_metadata_get(world, -16, 0, -32, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value == 12);
    changed = false;
    CHECK(mc_world_runtime_metadata_set_quiet(world, -16, 0, -32, INT32_MIN + 7, &changed,
                                              &error) == MC_WORLD_STORAGE_OK &&
          changed && chunk->data->metadata[bottom] == 0x37);
    expected.metadata[bottom] = 0x37;
    changed = false;
    CHECK(mc_world_runtime_metadata_set_quiet(world, -1, 127, -17, INT32_C(0x12345678), &changed,
                                              &error) == MC_WORLD_STORAGE_OK &&
          changed && chunk->data->metadata[top] == 0x8d);
    expected.metadata[top] = 0x8d;
    CHECK(mc_world_runtime_metadata_get(world, -1, 127, -17, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value == 8);
    const int32_t edge[][3] = {{0, 63, 32000000}, {-32000000, 63, 0}, {31999999, 63, 0}};
    for (size_t i = 0; i < 3; ++i) {
        CHECK(mc_world_runtime_metadata_get(world, edge[i][0], edge[i][1], edge[i][2], &value,
                                            &error) == MC_WORLD_STORAGE_OK &&
              value == 11);
        changed = false;
        CHECK(mc_world_runtime_metadata_set_quiet(world, edge[i][0], edge[i][1], edge[i][2], -17,
                                                  &changed, &error) == MC_WORLD_STORAGE_OK &&
              changed && chunks[i + 1u]->dirty);
        CHECK(chunks[i + 1u]->data->metadata[index_of(i == 2 ? 15u : 0u, 63, 0) / 2u] == 0xf4);
    }
    for (size_t i = 0; i < sizeof(outside) / sizeof(outside[0]); ++i) {
        value = -999;
        changed = true;
        CHECK(mc_world_runtime_metadata_get(world, outside[i][0], outside[i][1], outside[i][2],
                                            &value, &error) == MC_WORLD_STORAGE_OK &&
              value == 0);
        CHECK(mc_world_runtime_metadata_set_quiet(world, outside[i][0], outside[i][1],
                                                  outside[i][2], -1, &changed,
                                                  &error) == MC_WORLD_STORAGE_OK &&
              !changed);
    }
    value = -999;
    changed = false;
    CHECK(mc_world_runtime_metadata_get(world, 0, 63, -32000000, &value, &error) ==
              MC_WORLD_STORAGE_LIMIT &&
          value == -999);
    changed = true;
    CHECK(mc_world_runtime_metadata_set_quiet(world, 0, 63, -32000000, 3, &changed, &error) ==
              MC_WORLD_STORAGE_LIMIT &&
          changed);
    CHECK(mc_world_regions_get_usage(regions).count == usage.count &&
          mc_world_regions_get_usage(regions).bytes == usage.bytes && provider->cache->count == 4 &&
          provider->cache->insertion_count == 4 && provider->pending->count == 0 &&
          mc_world_store_chunk_count(store) == 4);
    changed = false;
    CHECK(mc_world_runtime_metadata_get(NULL, -1, 10, -17, &value, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          value == -999);
    CHECK(mc_world_runtime_metadata_get(world, -1, 10, -17, NULL, &error) ==
          MC_WORLD_STORAGE_INVALID_ARGUMENT);
    CHECK(mc_world_runtime_metadata_set_quiet(NULL, -1, 10, -17, 3, &changed, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          !changed);
    CHECK(mc_world_runtime_metadata_set_quiet(world, -1, 10, -17, 3, NULL, &error) ==
          MC_WORLD_STORAGE_INVALID_ARGUMENT);
    CHECK(same_chunk_except_dirty(chunk, &state_before) && chunk->dirty &&
          same_nonmetadata(chunk->data, &expected, before_blocks) &&
          memcmp(chunk->data->metadata, expected.metadata, MC_CHUNK_NIBBLE_BYTES) == 0);
    CHECK(mc_world_store_owns_chunk(store, chunk) &&
          mc_world_runtime_classes_get(world->classes, &class_after) == MC_WORLD_STORAGE_OK &&
          class_after.enqueue_depth == class_before.enqueue_depth &&
          mc_scheduled_ordered_size(world->fields.E, &ordered_size) == MC_SCHEDULED_ORDERED_OK &&
          ordered_size == 0 &&
          mc_scheduled_membership_size(world->fields.F, &membership_size) ==
              MC_SCHEDULED_MEMBERSHIP_OK &&
          membership_size == 0 && mc_test_java_random_equal(world->fields.r, &random_before));
    CHECK(test_metadata_codec(chunk) == 0);
    CHECK(same_chunk_except_dirty(chunk, &state_before) && chunk->dirty &&
          same_nonmetadata(chunk->data, &expected, before_blocks) &&
          memcmp(chunk->data->metadata, expected.metadata, MC_CHUNK_NIBBLE_BYTES) == 0);
    result = 0;
cleanup:
    return result;
}

static int test_blank_metadata(mc_world_runtime *world, const mc_world_provider_fields *provider)
{
    int result = 1;
    bool published = false, changed = false;
    int32_t value = -999;
    mc_world_storage_error error;
    const mc_chunk_state before = *provider->fallback;
    const mc_chunk_data data_before = *provider->fallback->data;
    uint8_t before_blocks[MC_CHUNK_BLOCKS];
    memcpy(before_blocks, data_before.blocks, sizeof(before_blocks));
    CHECK(provider->cache->count == 0 && provider->cache->insertion_count == 0);
    CHECK(mc_chunk_cache_publish(provider->cache, 9, 10, provider->fallback));
    published = true;
    CHECK(mc_world_runtime_metadata_get(world, 145, 127, 175, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value == 0);
    CHECK(mc_world_runtime_metadata_set_quiet(world, 145, 127, 175, INT32_MIN, &changed, &error) ==
              MC_WORLD_STORAGE_OK &&
          changed);
    changed = false;
    CHECK(mc_world_runtime_metadata_set_quiet(world, 145, 0, 175, INT32_MAX, &changed, &error) ==
              MC_WORLD_STORAGE_OK &&
          changed);
    CHECK(provider->cache->entries[0].chunk == provider->fallback &&
          provider->cache->insertions[0] == provider->fallback &&
          same_chunk_except_dirty(provider->fallback, &before) &&
          provider->fallback->dirty == before.dirty &&
          same_nonmetadata(provider->fallback->data, &data_before, before_blocks) &&
          memcmp(provider->fallback->data->metadata, data_before.metadata, MC_CHUNK_NIBBLE_BYTES) ==
              0);
    result = 0;
cleanup:
    if (published) {
        provider->cache->count = 0;
        provider->cache->insertion_count = 0;
    }
    return result;
}

static int test_blocks(mc_world_runtime *world, mc_world_store *store, mc_world_regions *regions,
                       mc_chunk_state *chunks[4])
{
    int result = 1;
    bool provider_bound = false;
    mc_world_storage_error error;
    mc_world_provider_fields provider;
    mc_chunk_data data;
    mc_chunk_state source = {.data = &data, .terrain_populated = true};
    const int32_t x[] = {-1, 0, -2000000, 1999999};
    const int32_t z[] = {-2, 2000000, 0, 0};
    int32_t value = -999;
    mc_java_random cave;
    CHECK(mc_world_provider_bind_fields(world->fields.v, &provider, &error) == MC_WORLD_STORAGE_OK);
    provider_bound = true;
    cave = provider.generator->cave_random;
    for (size_t i = 0; i < 4; ++i) {
        memset(blocks, 0, sizeof(blocks));
        CHECK(mc_chunk_data_init(&data, blocks, sizeof(blocks)));
        if (i == 0) {
            blocks[index_of(15, 63, 15)] = 201;
            blocks[index_of(15, 64, 15)] = 202;
            blocks[index_of(15, 65, 15)] = 203;
            blocks[index_of(14, 63, 15)] = 77;
            blocks[index_of(14, 10, 15)] = 88;
            blocks[index_of(0, 5, 0)] = 155;
            blocks[index_of(15, 10, 15)] = 255;
            data.metadata[index_of(15, 10, 15) / 2u] = 0xa5;
            data.metadata[index_of(0, 0, 0) / 2u] = 0x3c;
            data.metadata[index_of(15, 127, 15) / 2u] = 0x6d;
            data.skylight[7] = 0x65;
            data.block_light[8] = 0x43;
            data.heightmap[9] = 91;
            data.minimum_height = 37;
            blocks[index_of(13, 63, 15)] = 220;
            for (size_t y = 64; y < 128; ++y)
                blocks[index_of(13, y, 15)] = 219;
        } else {
            blocks[index_of(i == 3 ? 15u : 0u, 63, 0)] = (uint8_t)(211u + i);
            data.metadata[index_of(i == 3 ? 15u : 0u, 63, 0) / 2u] = 0xb4;
        }
        source.x = x[i];
        source.z = z[i];
        CHECK(mc_world_store_retain_chunk(store, 0, &source, &chunks[i], &error) ==
              MC_WORLD_STORAGE_OK);
        CHECK(mc_world_store_owns_chunk(store, chunks[i]));
        chunks[i]->terrain_populated = true;
        chunks[i]->active = true;
        CHECK(mc_chunk_cache_publish(provider.cache, x[i], z[i], chunks[i]));
    }
    /* A valid, explicitly restored cache baseline; no loader/activation or file
     * acquisition is impersonated. Every actual identity is retained by store. */
    CHECK(mc_chunk_cache_validate(provider.cache) && provider.cache->count == 4);
    CHECK(mc_world_runtime_block_get(world, -1, 63, -17, &value, &error) == MC_WORLD_STORAGE_OK &&
          value == 201);
    CHECK(mc_world_runtime_block_get(world, -16, 5, -32, &value, &error) == MC_WORLD_STORAGE_OK &&
          value == 155);
    CHECK(mc_world_runtime_top_block(world, -1, -17, &value, &error) == MC_WORLD_STORAGE_OK &&
          value == 203);
    CHECK(mc_world_runtime_top_block(world, -2, -17, &value, &error) == MC_WORLD_STORAGE_OK &&
          value == 77);
    CHECK(mc_world_runtime_top_block(world, -3, -17, &value, &error) == MC_WORLD_STORAGE_OK &&
          value == 219);
    CHECK(mc_world_runtime_block_get(world, 0, 63, 32000000, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value == 212);
    CHECK(mc_world_runtime_block_get(world, -32000000, 63, 0, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value == 213);
    CHECK(mc_world_runtime_block_get(world, 31999999, 63, 0, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value == 214);
    const mc_world_regions_usage before = mc_world_regions_get_usage(regions);
    CHECK(mc_world_runtime_block_get(world, 32000000, 63, 0, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value == 0);
    CHECK(mc_world_runtime_block_get(world, -32000001, 63, 0, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value == 0);
    CHECK(mc_world_runtime_block_get(world, 0, 63, 32000001, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value == 0);
    CHECK(mc_world_runtime_block_get(world, 0, 63, -32000001, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value == 0);
    CHECK(mc_world_runtime_block_get(world, -1, -1, -17, &value, &error) == MC_WORLD_STORAGE_OK &&
          value == 0);
    CHECK(mc_world_runtime_block_get(world, -1, 128, -17, &value, &error) == MC_WORLD_STORAGE_OK &&
          value == 0);
    CHECK(mc_world_regions_get_usage(regions).count == before.count &&
          mc_world_regions_get_usage(regions).bytes == before.bytes && provider.cache->count == 4);
    value = -999;
    CHECK(mc_world_runtime_block_get(world, 0, 63, -32000000, &value, &error) ==
              MC_WORLD_STORAGE_LIMIT &&
          value == -999);
    CHECK(mc_test_java_random_equal(&cave, &provider.generator->cave_random));
    CHECK(test_metadata(world, store, regions, &provider, chunks) == 0);
    CHECK(mc_world_runtime_retire_world(world, &error) == MC_WORLD_STORAGE_BUSY);
    /* Explicit retirement of this test's restored baseline only. */
    provider.cache->count = 0;
    provider.cache->insertion_count = 0;
    for (size_t i = 0; i < 4; ++i) {
        chunks[i]->active = false;
        CHECK(mc_world_store_retire_chunk(store, chunks[i], &error) == MC_WORLD_STORAGE_OK);
        chunks[i] = NULL;
    }
    CHECK(mc_world_store_chunk_count(store) == 0 && provider.pending->count == 0);
    CHECK(test_blank_metadata(world, &provider) == 0);
    value = -999;
    CHECK(mc_world_runtime_block_get(world, 128, 64, 128, &value, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          value == -999);
    CHECK(provider.cache->count == 0 && provider.cache->insertion_count == 0 &&
          mc_world_store_chunk_count(store) == 0 && mc_world_store_reservation_count(store) == 0);
    bool pending_added = false, changed = false;
    const int32_t missing_key = mc_chunk_cache_key(8, 8);
    CHECK(mc_chunk_pending_add(provider.pending, missing_key, &pending_added) ==
              MC_CHUNK_ACQUIRE_OK &&
          pending_added);
    value = -999;
    CHECK(mc_world_runtime_metadata_get(world, 128, 64, 128, &value, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          value == -999);
    CHECK(provider.pending->count == 0 &&
          !mc_java_int_set_contains(provider.pending->history, missing_key));
    pending_added = false;
    CHECK(mc_chunk_pending_add(provider.pending, missing_key, &pending_added) ==
              MC_CHUNK_ACQUIRE_OK &&
          pending_added);
    changed = true;
    CHECK(mc_world_runtime_metadata_set_quiet(world, 128, 64, 128, -1, &changed, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          changed);
    CHECK(provider.pending->count == 0 &&
          !mc_java_int_set_contains(provider.pending->history, missing_key) &&
          provider.cache->count == 0 && provider.cache->insertion_count == 0 &&
          mc_world_store_chunk_count(store) == 0 && mc_world_store_reservation_count(store) == 0);
    CHECK(mc_test_java_random_equal(&cave, &provider.generator->cave_random));
    result = 0;
cleanup:
    if (provider_bound)
        (void)mc_chunk_pending_clear(provider.pending);
    return result;
}

static void drop_world(mc_world_runtime **world)
{
    if (*world != NULL) {
        (void)mc_world_runtime_retire_world(*world, NULL);
        (void)mc_world_runtime_free(*world, NULL);
        *world = NULL;
    }
}

static int test_scheduled_retirement(mc_world_runtime *world)
{
    int result = 1;
    mc_scheduled_tick *tick = NULL;
    mc_scheduled_tick_sequence sequence = {0};
    bool in_e = false, in_f = false, removed = false;
    mc_world_storage_error error;
    int32_t count = 0;
    mc_world_runtime *const binding = mc_world_dimension_world(world->fields.t);
    CHECK(mc_scheduled_tick_create(&sequence, -1, 64, 7, 17, &tick) == MC_SCHEDULED_TICK_OK);
    CHECK(mc_scheduled_ordered_add(world->fields.E, tick, &in_e) == MC_SCHEDULED_ORDERED_OK &&
          in_e);
    CHECK(mc_world_runtime_retire_world(world, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_dimension_world(world->fields.t) == (binding == world ? NULL : binding) &&
          !world->construction.contexts_bound &&
          world->construction.phase == MC_WORLD_RUNTIME_RETIRED);
    CHECK(mc_scheduled_ordered_size(world->fields.E, &count) == MC_SCHEDULED_ORDERED_OK &&
          count == 1);
    CHECK(mc_scheduled_ordered_remove(world->fields.E, tick, &removed) == MC_SCHEDULED_ORDERED_OK &&
          removed);
    in_e = false;
    CHECK(mc_scheduled_membership_add(world->fields.F, tick, &in_f) == MC_SCHEDULED_MEMBERSHIP_OK &&
          in_f);
    CHECK(mc_world_runtime_retire_world(world, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_dimension_world(world->fields.t) == (binding == world ? NULL : binding) &&
          !world->construction.contexts_bound &&
          world->construction.phase == MC_WORLD_RUNTIME_RETIRED);
    CHECK(mc_scheduled_membership_size(world->fields.F, &count) == MC_SCHEDULED_MEMBERSHIP_OK &&
          count == 1);
    CHECK(mc_scheduled_membership_remove(world->fields.F, tick, &removed) ==
              MC_SCHEDULED_MEMBERSHIP_OK &&
          removed);
    in_f = false;
    result = 0;
cleanup:
    if (in_e)
        (void)mc_scheduled_ordered_remove(world->fields.E, tick, &removed);
    if (in_f)
        (void)mc_scheduled_membership_remove(world->fields.F, tick, &removed);
    mc_scheduled_tick_release(tick);
    return result;
}

static int test_game(const char *root)
{
    int result = 1;
    mc_world_regions *regions = NULL;
    mc_world_store *store = NULL;
    mc_java_random_profile *profile = NULL;
    mc_world_runtime_classes *classes = NULL;
    mc_world_runtime *world[7] = {NULL};
    mc_world_dimension *dimension[7] = {NULL};
    mc_chunk_state *chunks[4] = {NULL};
    mc_world_info_name *name = NULL;
    mc_world_info_player *player = NULL;
    mc_world_info *metadata = NULL, *held_info = NULL;
    mc_world_dimension_biome *held_biome = NULL;
    mc_world_provider_fields direct, copied, loaded, missing;
    mc_world_runtime_fields a, b, f;
    mc_world_runtime_construction state;
    mc_world_runtime_class_state class_state;
    mc_world_info_fields info_a, info_b;
    mc_world_dimension_fields df;
    mc_world_dimension_biome_fields bf;
    mc_world_storage_error error;
    mc_world_runtime_world_arguments arguments = {0};
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    uint8_t map[256];
    const uint16_t title[] = {'G', 0, 0xd800, 'x', 0xdc00};
    uint64_t before = 0;
    size_t owner_bytes = 0;
    unsigned int bindings = 0;
    int32_t metadata_value = -999;
    bool metadata_changed = true;
    mc_world_store_options options = {.session_token = INT64_C(0x123456789abcdef),
                                      .block_map = map,
                                      .block_map_length = sizeof(map),
                                      .limits = mc_world_store_default_limits()};
    for (size_t i = 0; i < sizeof(map); ++i)
        map[i] = (uint8_t)i;
    limits.max_provider_chunks = 4;
    CHECK(mc_world_regions_create(NULL, &regions, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_open(root, regions, &options, &store, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_info(store) == NULL);
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, profile, &trig, &classes, &error) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, sizeof(title) / sizeof(title[0])},
                                  SIZE_MAX, &name) == MC_WORLD_INFO_OK);
    CHECK(make_player(&player));
    arguments = (mc_world_runtime_world_arguments){.store = store, .name = name, .seed = 23};

    /* Fresh load enters the actual first top-block request. Missing real
     * generation capability preserves y=true/provider/context effects. */
    before = uniquifier(profile);
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE, classes, &limits,
                                          &world[0], &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_construct_world(world[0], &arguments, &error) ==
          MC_WORLD_STORAGE_UNSUPPORTED_STATE);
    CHECK(mc_world_runtime_construction_get(world[0], &state) == MC_WORLD_STORAGE_OK &&
          state.phase == MC_WORLD_RUNTIME_FAILED_CONSTRUCTION && state.failed_call_pc == 415 &&
          state.contexts_bound && state.cache_registered && state.last_store_pc == 406);
    CHECK(mc_world_runtime_fields_get(world[0], &f) == MC_WORLD_STORAGE_OK && f.y && f.s &&
          f.v != NULL && f.x != NULL);
    CHECK(check_provider(world[0], store, &missing) == 0);
    CHECK(uniquifier(profile) == advanced(before, 3) && cold(profile));
    CHECK(mc_world_regions_get_usage(regions).count != 0 && mc_world_store_info(store) == NULL &&
          mc_world_store_chunk_count(store) == 0);
    before = uniquifier(profile);
    CHECK(mc_world_runtime_construct_world(world[0], &arguments, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          uniquifier(profile) == before);
    CHECK(mc_world_runtime_metadata_get(world[0], 32000000, 0, 0, &metadata_value, &error) ==
              MC_WORLD_STORAGE_OK &&
          metadata_value == 0);
    CHECK(mc_world_runtime_metadata_set_quiet(world[0], 32000000, 0, 0, 15, &metadata_changed,
                                              &error) == MC_WORLD_STORAGE_OK &&
          !metadata_changed);
    CHECK(mc_world_runtime_retire_world(world[0], &error) == MC_WORLD_STORAGE_OK);
    metadata_value = -999;
    metadata_changed = false;
    CHECK(mc_world_runtime_metadata_get(world[0], 32000000, 0, 0, &metadata_value, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          metadata_value == -999);
    CHECK(mc_world_runtime_metadata_set_quiet(world[0], 32000000, 0, 0, 15, &metadata_changed,
                                              &error) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          !metadata_changed);
    CHECK(mc_world_runtime_free(world[0], &error) == MC_WORLD_STORAGE_OK);
    world[0] = NULL;

    CHECK(mc_world_dimension_create(0, NULL, &dimension[1], NULL) == MC_WORLD_DIMENSION_OK);
    arguments.dimension = dimension[1];
    before = uniquifier(profile);
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, classes, &limits,
                                          &world[1], &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_construct_world(world[1], &arguments, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_construction_get(world[1], &state) == MC_WORLD_STORAGE_OK &&
          state.phase == MC_WORLD_RUNTIME_READY && state.last_store_pc == 308 &&
          state.failed_call_pc == 0 && state.contexts_bound && state.cache_registered);
    CHECK(uniquifier(profile) == advanced(before, 3) && cold(profile));
    CHECK(check_provider(world[1], store, &direct) == 0);
    CHECK(mc_world_runtime_fields_get(world[1], &a) == MC_WORLD_STORAGE_OK && a.w == store &&
          a.t == dimension[1] && !a.s && !a.y);
    CHECK(mc_world_store_info(store) == NULL);
    CHECK(mc_world_info_player_set(a.x, player) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_time_set(a.x, 37) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_fields_get(a.x, &info_a) == MC_WORLD_INFO_OK && info_a.name == name &&
          info_a.player == player);
    CHECK(mc_world_dimension_biome_retain(direct.biome) == MC_WORLD_DIMENSION_OK);
    held_biome = direct.biome;
    /* Recover the opaque provider-owner charge from its exact published total
     * and documented concrete subordinate allocations, not a replica layout. */
    const size_t subordinate = direct.max_chunks * (sizeof(int32_t) + sizeof(mc_chunk_cache_entry) +
                                                    sizeof(mc_chunk_state *)) +
                               sizeof(mc_chunk_data) + MC_CHUNK_BLOCKS;
    CHECK(direct.direct_allocation_bytes > subordinate);
    owner_bytes = direct.direct_allocation_bytes - subordinate;
    before = uniquifier(profile);
    CHECK(test_blocks(world[1], store, regions, chunks) == 0);
    CHECK(uniquifier(profile) == before && cold(profile));

    CHECK(mc_world_dimension_create(0, NULL, &dimension[2], NULL) == MC_WORLD_DIMENSION_OK);
    arguments.parent = world[1];
    arguments.dimension = dimension[2];
    before = uniquifier(profile);
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_COPY, classes, &limits, &world[2],
                                          &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_construct_world(world[2], &arguments, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_construction_get(world[2], &state) == MC_WORLD_STORAGE_OK &&
          state.phase == MC_WORLD_RUNTIME_READY && state.last_store_pc == 323 &&
          state.failed_call_pc == 0);
    CHECK(uniquifier(profile) == advanced(before, 3) && cold(profile));
    CHECK(check_provider(world[2], store, &copied) == 0);
    CHECK(mc_world_runtime_fields_get(world[2], &b) == MC_WORLD_STORAGE_OK && b.I == a.I &&
          b.w == a.w && b.x != a.x && b.r != a.r && b.v != a.v);
    CHECK(mc_world_info_fields_get(b.x, &info_b) == MC_WORLD_INFO_OK &&
          info_b.name == info_a.name && info_b.player == info_a.player && info_b.value.time == 37);
    CHECK(copied.cache != direct.cache && copied.pending != direct.pending &&
          copied.generator != direct.generator && copied.fallback != direct.fallback &&
          copied.biome != direct.biome);
    CHECK(mc_world_info_time_set(b.x, 91) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_fields_get(a.x, &info_a) == MC_WORLD_INFO_OK && info_a.value.time == 37);

    CHECK(make_metadata(name, player, &metadata));
    CHECK(mc_world_store_save_world_info(store, metadata, &error) == MC_WORLD_STORAGE_OK &&
          mc_world_store_info(store) != NULL);
    CHECK(mc_world_store_info(store)->time == 5999 && mc_world_store_info(store)->version == 17);
    arguments = (mc_world_runtime_world_arguments){.store = store, .name = name, .seed = -99};
    before = uniquifier(profile);
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE, classes, &limits,
                                          &world[3], &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_construct_world(world[3], &arguments, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_construction_get(world[3], &state) == MC_WORLD_STORAGE_OK &&
          state.phase == MC_WORLD_RUNTIME_READY && state.last_store_pc == 406 &&
          state.failed_call_pc == 0);
    CHECK(uniquifier(profile) == advanced(before, 3) && cold(profile));
    CHECK(check_provider(world[3], store, &loaded) == 0);
    CHECK(mc_world_runtime_fields_get(world[3], &f) == MC_WORLD_STORAGE_OK && !f.s && !f.y &&
          f.f == 0 && f.i == 0.0f && f.j == 1.0f && f.k == 0.0f && f.l == 1.0f);
    CHECK(mc_world_info_fields_get(f.x, &info_b) == MC_WORLD_INFO_OK && info_b.value.seed == 23 &&
          info_b.value.time == 5999 && info_b.value.raining && info_b.value.thundering &&
          info_b.name == name);
    int32_t sky = -1;
    CHECK(mc_world_runtime_sky_subtraction(world[3], 1.0f, &sky, &error) == MC_WORLD_STORAGE_OK &&
          sky == 5);
    CHECK(world[3]->fields.f == 0);
    CHECK(mc_world_runtime_retire_world(world[3], &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_free(world[3], &error) == MC_WORLD_STORAGE_OK);
    world[3] = NULL;

    /* Failed GAME Info read must not enter the legacy metadata/live fallback. */
    mc_world_runtime_construction_limits failed_ids = limits;
    failed_ids.saved_data.max_allocation_bytes = 0;
    before = uniquifier(profile);
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE, classes,
                                          &failed_ids, &world[6], &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_construct_world(world[6], &arguments, &error) == MC_WORLD_STORAGE_LIMIT);
    CHECK(mc_world_runtime_construction_get(world[6], &state) == MC_WORLD_STORAGE_OK &&
          state.phase == MC_WORLD_RUNTIME_FAILED_CONSTRUCTION && state.last_store_pc == 264 &&
          state.failed_call_pc == 273);
    CHECK(world[6]->fields.w == store && world[6]->fields.x == NULL && world[6]->fields.z == NULL &&
          mc_world_store_info(store) != NULL);
    mc_level_info guard = {.seed = -777,
                           .spawn_x = -1,
                           .spawn_y = 2,
                           .spawn_z = -3,
                           .time = 99,
                           .last_played = -98,
                           .size_on_disk = 97,
                           .level_name = {title, 5},
                           .version = 96,
                           .rain_time = -95,
                           .thunder_time = 94,
                           .raining = true,
                           .thundering = true,
                           .player = mc_world_info_player_root(player),
                           .player_dimension = -93};
    const mc_level_info saved_guard = guard;
    CHECK(mc_world_runtime_read_info(world[6], &guard, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          same_info(&guard, &saved_guard));
    CHECK(uniquifier(profile) == advanced(before, 2) && cold(profile));
    CHECK(mc_world_runtime_retire_world(world[6], &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_free(world[6], &error) == MC_WORLD_STORAGE_OK);
    world[6] = NULL;

    for (size_t i = 4; i < 6; ++i) {
        mc_world_runtime_construction_limits limited = limits;
        limited.max_provider_allocation_bytes = i == 4 ? 0 : owner_bytes;
        CHECK(mc_world_dimension_create(0, NULL, &dimension[i], NULL) == MC_WORLD_DIMENSION_OK);
        arguments = (mc_world_runtime_world_arguments){
            .store = store, .name = name, .seed = 23, .dimension = dimension[i]};
        before = uniquifier(profile);
        CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, classes,
                                              &limited, &world[i], &error) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_construct_world(world[i], &arguments, &error) ==
              MC_WORLD_STORAGE_LIMIT);
        CHECK(mc_world_runtime_construction_get(world[i], &state) == MC_WORLD_STORAGE_OK &&
              state.phase == MC_WORLD_RUNTIME_FAILED_CONSTRUCTION && state.last_store_pc == 295 &&
              state.failed_call_pc == 305 && !state.contexts_bound && !state.cache_registered);
        CHECK(world[i]->fields.v == NULL && world[i]->fields.x != NULL &&
              world[i]->fallback.data == NULL && !world[i]->fallback.blank &&
              !world[i]->fallback.never_save);
        CHECK(uniquifier(profile) == advanced(before, i == 4 ? 2u : 3u) && cold(profile));
        CHECK(mc_world_dimension_fields_get(dimension[i], &df) == MC_WORLD_DIMENSION_OK &&
              df.a == world[i] && df.b != NULL);
        CHECK(mc_world_dimension_biome_fields_get(df.b, &bf) == MC_WORLD_DIMENSION_OK &&
              bf.holds == 1);
        before = uniquifier(profile);
        CHECK(mc_world_runtime_construct_world(world[i], &arguments, &error) ==
                  MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              uniquifier(profile) == before);
        CHECK(mc_world_runtime_retire_world(world[i], &error) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_free(world[i], &error) == MC_WORLD_STORAGE_OK);
        world[i] = NULL;
    }

    /* Rebind the real Dimension without invalidating the provider's supplying
     * biome. Extra Dimension bindings must not disappear at World retirement. */
    CHECK(mc_world_dimension_bind(dimension[1], world[2], NULL) == MC_WORLD_DIMENSION_OK);
    CHECK(mc_world_dimension_fields_get(dimension[1], &df) == MC_WORLD_DIMENSION_OK &&
          df.a == world[2] && df.b != held_biome);
    CHECK(mc_world_dimension_biome_fields_get(held_biome, &bf) == MC_WORLD_DIMENSION_OK &&
          bf.normal == direct.manager && bf.normal->initialized == 1 && bf.holds == 2);
    CHECK(mc_world_runtime_binding_retain(world[1]));
    ++bindings;
    /* Retirement is final for gameplay. Exercise retained queue owners only
     * after the last use of this World as a constructor/query receiver. */
    CHECK(test_scheduled_retirement(world[1]) == 0);
    CHECK(mc_world_runtime_retire_world(world[1], &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_dimension_world(dimension[1]) == world[2] &&
          mc_world_runtime_free(world[1], &error) == MC_WORLD_STORAGE_BUSY);
    mc_world_runtime_binding_release(world[1]);
    --bindings;
    CHECK(mc_world_runtime_free(world[1], &error) == MC_WORLD_STORAGE_OK);
    world[1] = NULL;
    CHECK(mc_world_dimension_biome_fields_get(held_biome, &bf) == MC_WORLD_DIMENSION_OK &&
          bf.normal == direct.manager && bf.normal->initialized == 1 && bf.holds == 1);
    CHECK(mc_world_info_fields_get(b.x, &info_b) == MC_WORLD_INFO_OK && info_b.name == name &&
          info_b.player == player && info_b.value.time == 91);
    CHECK(mc_world_info_retain(b.x) == MC_WORLD_INFO_OK);
    held_info = b.x;
    CHECK(mc_world_runtime_retire_world(world[2], &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_dimension_world(dimension[2]) == NULL &&
          mc_world_dimension_world(dimension[1]) == world[2]);
    CHECK(mc_world_runtime_free(world[2], &error) == MC_WORLD_STORAGE_BUSY);
    mc_world_dimension_unbind(dimension[1]);
    CHECK(mc_world_runtime_free(world[2], &error) == MC_WORLD_STORAGE_OK);
    world[2] = NULL;
    CHECK(mc_world_info_fields_get(held_info, &info_b) == MC_WORLD_INFO_OK && info_b.name == name &&
          info_b.player == player && info_b.value.time == 91);
    CHECK(mc_world_runtime_classes_get(classes, &class_state) == MC_WORLD_STORAGE_OK &&
          class_state.live_worlds == 0);
    CHECK(mc_world_store_check_session(store, &error) == MC_WORLD_STORAGE_OK && cold(profile));
    result = 0;
cleanup:
    if (world[1] != NULL && world[1]->fields.v != NULL) {
        mc_world_provider_fields fixture;
        if (mc_world_provider_bind_fields(world[1]->fields.v, &fixture, NULL) ==
            MC_WORLD_STORAGE_OK) {
            fixture.cache->count = 0;
            fixture.cache->insertion_count = 0;
        }
    }
    while (bindings != 0) {
        mc_world_runtime_binding_release(world[1]);
        --bindings;
    }
    for (size_t i = 0; i < 7; ++i)
        mc_world_dimension_unbind(dimension[i]);
    for (size_t i = 0; i < 7; ++i)
        drop_world(&world[i]);
    for (size_t i = 0; i < 4; ++i) {
        if (chunks[i] != NULL) {
            chunks[i]->active = false;
            (void)mc_world_store_retire_chunk(store, chunks[i], NULL);
        }
    }
    mc_world_info_release(held_info);
    mc_world_info_release(metadata);
    mc_world_dimension_biome_release(held_biome);
    for (size_t i = 0; i < 7; ++i)
        mc_world_dimension_release(dimension[i]);
    mc_world_info_player_release(player);
    mc_world_info_name_release(name);
    (void)mc_world_runtime_classes_free(classes, NULL);
    mc_java_random_profile_free(profile);
    mc_world_store_free(store);
    mc_world_regions_free(regions);
    return result;
}

static bool decode_path(const char *hex, char *out, size_t capacity)
{
    const size_t length = strlen(hex);
    if (length == 0 || length % 2u != 0 || length / 2u >= capacity)
        return false;
    for (size_t i = 0; i < length / 2u; ++i) {
        unsigned int value = 0;
        for (size_t j = 0; j < 2; ++j) {
            const unsigned char c = (unsigned char)hex[i * 2u + j];
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
    out[length / 2u] = '\0';
    return true;
}

int main(int argc, char **argv)
{
    char root[8192];
    if (argc != 3 || strcmp(argv[1], "game-world") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    if (test_game(root) != 0)
        return 1;
    printf("PASS: %u constructed GAME World checks (game-world)\n", checks);
    return 0;
}
