/* Same genuine Registry/World construction helpers as the Item-use controls.
 * The imported Player control executable is never run. */
#define main mc_plant_placement_player_fixture_main
#include "../../entity/player/test_construction.c"
#undef main

#include "../../../src/block/registry/registry_internal.h"
#include "../../../src/block/registry/runtime_internal.h"
#include "../../../src/world/chunk/object_state_internal.h"
#include "../../../src/world/runtime/plant_substrate_internal.h"
#include "../../../src/world/runtime/runtime_internal.h"
#include "../../../src/world/storage/generated_internal.h"
#include "mc/core/java/java.h"
#include "mc/world/runtime/placement.h"

#include <errno.h>
#include <inttypes.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

enum {
    PLANT_PROFILES = 288,
    PLANT_CHUNKS = 4,
    PLANT_PLANES = 5,
    PLANT_ARRAY_BYTES = MC_CHUNK_BLOCKS + 3 * MC_CHUNK_NIBBLE_BYTES + MC_CHUNK_HEIGHTMAP_BYTES
};

typedef struct plant_profile {
    char name[96];
    int32_t id, x, y, z, face, target, below, null_world;
} plant_profile;

static const int32_t plant_receivers[] = {6, 31, 32, 37, 38, 39, 40, 59};
static const size_t plant_lengths[PLANT_PLANES] = {MC_CHUNK_BLOCKS, MC_CHUNK_NIBBLE_BYTES,
                                                   MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_NIBBLE_BYTES,
                                                   MC_CHUNK_HEIGHTMAP_BYTES};

/* Preserve the existing constructors and bindings; this declared fixture uses
 * seed23 and four cache slots for the original observer's four concrete Chunks. */
static bool plant_setup(fixture *f, const char *resources, const char *directory) {
    CHECK(mc_resource_files_create(resources, &f->files) == MC_REGISTRY_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entities) ==
          MC_ENTITY_OK);
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                         mc_resource_files_source(f->files)};
    CHECK(mc_registry_classes_allocate(&options, &f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_initialize_blocks(f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig,
                                          &f->world_classes, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(f->world_classes, f->registry, NULL) ==
          MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_bind_entities(f->world_classes, f->entities) == MC_ENTITY_OK);
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    uint8_t map[256] = {0};
    mc_registry_block_view blocks;
    CHECK(mc_registry_classes_inspect_blocks(f->registry, &blocks, NULL) == MC_REGISTRY_OK);
    for (size_t i = 1; i < 256; ++i)
        map[i] = blocks.m[i] == NULL ? 0 : (uint8_t)i;
    const mc_world_store_options store = {.session_token = 6381,
                                          .block_map = map,
                                          .block_map_length = 256,
                                          .limits = mc_world_store_default_limits()};
    CHECK(mc_world_store_open(directory, f->regions, &store, &f->store, NULL) ==
          MC_WORLD_STORAGE_OK);
    static const uint16_t name[] = {'P', 'l', 'a', 'n', 't'};
    CHECK(mc_world_info_name_copy((mc_nbt_string){name, 5}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_world_provider_create(0, NULL, &f->dimension, NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = PLANT_CHUNKS;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, f->world_classes,
                                          &limits, &f->world, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments arguments = {
        .store = f->store, .name = f->name, .seed = 23, .dimension = f->dimension};
    CHECK(mc_world_runtime_construct_world(f->world, &arguments, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_player_session_classes_create(f->entities, f->registry, &f->sessions, NULL) ==
          MC_PLAYER_OK);
    return true;
}

static uint8_t *plant_plane(mc_chunk_state *chunk, size_t plane) {
    switch (plane) {
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

static void plant_cell(mc_chunk_state **chunks, int32_t x, int32_t y, int32_t z, int32_t id) {
    if (x < 0 || x >= 32 || z < 0 || z >= 32 || y < 0 || y >= 128)
        return;
    mc_chunk_state *chunk = chunks[(size_t)(x >> 4) * 2u + (size_t)(z >> 4)];
    const size_t index = (((size_t)x & 15u) * 16u + ((size_t)z & 15u)) * 128u + (size_t)y;
    chunk->data->blocks[index] = (uint8_t)id;
}

static void plant_reset(mc_chunk_state **chunks, const plant_profile *p, uint8_t *prior) {
    size_t offset = 0;
    for (size_t chunk = 0; chunk < PLANT_CHUNKS; ++chunk) {
        for (size_t plane = 0; plane < PLANT_PLANES; ++plane)
            memset(plant_plane(chunks[chunk], plane), plane == 2 ? 15 : 0, plant_lengths[plane]);
        chunks[chunk]->data->minimum_height = 0;
        chunks[chunk]->terrain_populated = true;
    }
    plant_cell(chunks, p->x, mc_java_i32_sub(p->y, 1), p->z, p->below);
    plant_cell(chunks, p->x, p->y, p->z, p->target);
    if (prior == NULL)
        return;
    for (size_t chunk = 0; chunk < PLANT_CHUNKS; ++chunk)
        for (size_t plane = 0; plane < PLANT_PLANES; ++plane) {
            memcpy(prior + offset, plant_plane(chunks[chunk], plane), plant_lengths[plane]);
            offset += plant_lengths[plane];
        }
}

static size_t plant_changes(mc_chunk_state **chunks, const uint8_t *prior) {
    size_t offset = 0, changed = 0;
    for (size_t chunk = 0; chunk < PLANT_CHUNKS; ++chunk)
        for (size_t plane = 0; plane < PLANT_PLANES; ++plane) {
            const uint8_t *actual = plant_plane(chunks[chunk], plane);
            for (size_t i = 0; i < plant_lengths[plane]; ++i)
                changed += actual[i] != prior[offset + i] ? 1u : 0u;
            offset += plant_lengths[plane];
        }
    return changed;
}

static const char *plant_class(const mc_block *block) {
    switch (block->kind) {
        case MC_BLOCK_SAPLING:
            return "he";
        case MC_BLOCK_TALL_GRASS:
            return "ru";
        case MC_BLOCK_DEAD_BUSH:
            return "jb";
        case MC_BLOCK_FLOWER:
            return "wb";
        case MC_BLOCK_MUSHROOM:
            return "tl";
        case MC_BLOCK_CROPS:
            return "ni";
        default:
            return NULL;
    }
}

static const char *plant_substrate_class(const mc_block *block) {
    return block->kind == MC_BLOCK_DEAD_BUSH  ? "jb"
           : block->kind == MC_BLOCK_MUSHROOM ? "tl"
           : block->kind == MC_BLOCK_CROPS    ? "ni"
                                              : "wb";
}

static mc_world_storage_status plant_call(fixture *f, const plant_profile *p, unsigned int channel,
                                          const mc_block *block, bool *out,
                                          mc_world_placement_error *error) {
    mc_world_runtime *world = p->null_world != 0 ? NULL : f->world;
    if (channel == 0)
        return mc_world_block_can_place_base(f->registry, block, world, p->x, p->y, p->z, out,
                                             error);
    if (channel == 1)
        return mc_world_block_can_place(f->registry, block, world, p->x, p->y, p->z, p->face, out,
                                        error);
    return mc_world_runtime_can_place(world, p->id, p->x, p->y, p->z, false, p->face, out, error);
}

static bool plant_project(fixture *f, mc_chunk_state **chunks, size_t index, const plant_profile *p,
                          uint8_t *prior) {
    const mc_block *block = NULL;
    CHECK(mc_registry_block_get(f->registry, p->id, &block, NULL) == MC_REGISTRY_OK &&
          block != NULL && plant_class(block) != NULL);
    for (unsigned int channel = 0; channel < 3; ++channel) {
        plant_reset(chunks, p, prior);
        bool value = true;
        mc_world_placement_error error = {0};
        const mc_world_storage_status status = plant_call(f, p, channel, block, &value, &error);
        const char *result = error.returned ? (value ? "true" : "false") : "UNRETURNED";
        char exception[128] = "NONE";
        bool qualified = status == MC_WORLD_STORAGE_OK && error.returned;
        if (!qualified && !error.native_policy && (error.source_exception || error.source_error) &&
            error.exception_class != NULL && strlen(error.exception_class) < sizeof(exception)) {
            size_t i = 0;
            for (; error.exception_class[i] != 0; ++i)
                exception[i] = error.exception_class[i] == '/' ? '.' : error.exception_class[i];
            exception[i] = 0;
            qualified = !error.returned && value;
        }
        if (!qualified)
            fprintf(stderr,
                    "plant profile=%zu name=%s channel=%u status=%d native=%d site=%s.%s pc=%u "
                    "queries=%zu operation=%s\n",
                    index, p->name, channel, (int)status, (int)error.native_policy,
                    error.site.class_name == NULL ? "-" : error.site.class_name,
                    error.site.method == NULL ? "-" : error.site.method, error.site.pc,
                    error.query_count, error.operation == NULL ? "-" : error.operation);
        const size_t changed = plant_changes(chunks, prior);
        const bool retired = mc_world_placement_error_retire(&error);
        CHECK(qualified && retired);
        CHECK(printf("PLANT\t%zu\t%s\t%u\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\t%" PRId32
                     "\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\t%s\t%s\t%zu\n",
                     index, p->name, channel, p->id, p->x, p->y, p->z, p->face, p->target, p->below,
                     result, exception, changed) > 0);
    }
    return true;
}

static char *plant_token(char **cursor) {
    if (*cursor == NULL)
        return NULL;
    char *result = *cursor;
    char *end = strpbrk(result, "\t\r\n");
    if (end == NULL) {
        *cursor = NULL;
    } else {
        const char delimiter = *end;
        *end = 0;
        *cursor = delimiter == '\t' ? end + 1 : NULL;
    }
    return result;
}

static bool plant_profiles(fixture *f, mc_chunk_state **chunks, const char *path, uint8_t *prior) {
    FILE *input = fopen(path, "rb");
    CHECK(input != NULL);
    char line[1024];
    size_t count = 0;
    while (fgets(line, sizeof(line), input) != NULL) {
        CHECK(strchr(line, '\n') != NULL && count < PLANT_PROFILES);
        plant_profile p = {0};
        char *cursor = line, *token = plant_token(&cursor);
        CHECK(token != NULL && token[0] != 0 && strlen(token) < sizeof(p.name));
        memcpy(p.name, token, strlen(token) + 1);
        int32_t values[8];
        for (size_t i = 0; i < 8; ++i) {
            token = plant_token(&cursor);
            CHECK(token != NULL && token[0] != 0);
            char *end = NULL;
            errno = 0;
            const intmax_t value = strtoimax(token, &end, 10);
            CHECK(errno == 0 && *end == 0 && value >= INT32_MIN && value <= INT32_MAX);
            values[i] = (int32_t)value;
        }
        CHECK(plant_token(&cursor) == NULL);
        p.id = values[0];
        p.x = values[1];
        p.y = values[2];
        p.z = values[3];
        p.face = values[4];
        p.target = values[5];
        p.below = values[6];
        p.null_world = values[7];
        CHECK(p.x == 8 && p.z == 8 && p.target >= 0 && p.target <= 255 && p.below >= 0 &&
              p.below <= 255 && (p.null_world == 0 || p.null_world == 1));
        CHECK(plant_project(f, chunks, count++, &p, prior));
    }
    CHECK(!ferror(input) && fclose(input) == 0 && count == PLANT_PROFILES);
    return true;
}

static size_t plant_named(const char *name) {
    for (size_t i = 0; i < 96; ++i)
        if (strcmp(mc_registry_block_static_name(i), name) == 0)
            return i;
    return SIZE_MAX;
}

static bool plant_owner_controls(fixture *f, mc_chunk_state **chunks,
                                 mc_registry_classes *foreign) {
    const mc_block *sapling = NULL;
    CHECK(mc_registry_block_get(f->registry, 6, &sapling, NULL) == MC_REGISTRY_OK &&
          sapling != NULL);
    const size_t grass = plant_named("v");
    CHECK(grass != SIZE_MAX);
    mc_block *saved_grass = f->registry->blocks.named[grass];
    const plant_profile supported = {.id = 6, .x = 8, .y = 64, .z = 8, .below = 2};
    plant_reset(chunks, &supported, NULL);
    mc_world_placement_error error = {0};
    bool value = false;
    const size_t bindings = f->world->persistent_bindings;
    const size_t borrows = f->registry->runtime_borrows;
    CHECK(mc_world_block_can_place_base(f->registry, sapling, f->world, 8, 64, 8, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value && error.returned && error.base_returned && error.below_read &&
          error.below_id == 2 && error.substrate_returned && error.query_count == 2 &&
          error.queries[0] != NULL && error.queries[1] != NULL && error.queries[2] == NULL &&
          f->world->persistent_bindings == bindings + 1 &&
          f->registry->runtime_borrows == borrows + 1);
    const mc_world_placement_work *work = error.work;
    const mc_world_runtime_acquisition_error *query = error.queries[1];
    value = false;
    CHECK(mc_world_block_can_place_base(f->registry, sapling, f->world, 8, 64, 8, &value, &error) ==
              MC_WORLD_STORAGE_BUSY &&
          !value && error.work == work && error.queries[1] == query && error.returned);
    CHECK(mc_world_placement_error_retire(&error) && error.work == NULL && error.query_count == 0 &&
          error.queries[0] == NULL && f->world->persistent_bindings == bindings &&
          f->registry->runtime_borrows == borrows);
    value = false;
    CHECK(mc_world_runtime_can_place(f->world, 6, 8, 64, 8, false, INT32_MIN, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value && error.returned && error.query_count == 3 && error.queries[0] != NULL &&
          error.queries[1] != NULL && error.queries[2] != NULL &&
          error.queries[0] != error.queries[1] && error.queries[1] != error.queries[2] &&
          error.acquisition == error.queries[2] && error.below_read && error.below_id == 2 &&
          error.caller.pc == 130 && error.wrapper.pc == 6 && error.base.pc == 34 &&
          error.substrate.pc == 35 && mc_world_placement_error_retire(&error));

    /* Reaching the matching grass or dirt static skips all later statics. */
    const size_t dirt = plant_named("w"), farmland = plant_named("aB");
    CHECK(dirt != SIZE_MAX && farmland != SIZE_MAX);
    mc_block *saved_dirt = f->registry->blocks.named[dirt];
    mc_block *saved_farmland = f->registry->blocks.named[farmland];
    f->registry->blocks.named[dirt] = NULL;
    f->registry->blocks.named[farmland] = NULL;
    value = false;
    const mc_world_storage_status grass_shortcut =
        mc_world_block_can_place_base(f->registry, sapling, f->world, 8, 64, 8, &value, &error);
    f->registry->blocks.named[dirt] = saved_dirt;
    f->registry->blocks.named[farmland] = saved_farmland;
    CHECK(grass_shortcut == MC_WORLD_STORAGE_OK && value && error.substrate_returned &&
          mc_world_placement_error_retire(&error));
    plant_profile dirt_supported = supported;
    dirt_supported.below = 3;
    plant_reset(chunks, &dirt_supported, NULL);
    f->registry->blocks.named[farmland] = NULL;
    value = false;
    const mc_world_storage_status dirt_shortcut =
        mc_world_block_can_place_base(f->registry, sapling, f->world, 8, 64, 8, &value, &error);
    f->registry->blocks.named[farmland] = saved_farmland;
    CHECK(dirt_shortcut == MC_WORLD_STORAGE_OK && value && error.substrate_returned &&
          mc_world_placement_error_retire(&error));

    /* Base false must not read a below Block or even the first substrate static. */
    plant_profile occupied = supported;
    occupied.target = 1;
    plant_reset(chunks, &occupied, NULL);
    f->registry->blocks.named[grass] = NULL;
    value = true;
    const mc_world_storage_status short_status =
        mc_world_block_can_place_base(f->registry, sapling, f->world, 8, 64, 8, &value, &error);
    f->registry->blocks.named[grass] = saved_grass;
    CHECK(short_status == MC_WORLD_STORAGE_OK && !value && error.returned && !error.below_read &&
          !error.substrate_entered && error.query_count == 1 &&
          mc_world_placement_error_retire(&error));

    plant_reset(chunks, &supported, NULL);
    f->registry->blocks.named[grass] = f->registry->blocks.m[1];
    value = true;
    const mc_world_storage_status rebound =
        mc_world_block_can_place_base(f->registry, sapling, f->world, 8, 64, 8, &value, &error);
    f->registry->blocks.named[grass] = saved_grass;
    CHECK(rebound == MC_WORLD_STORAGE_OK && !value && error.substrate_returned &&
          mc_world_placement_error_retire(&error));
    f->registry->blocks.named[grass] = NULL;
    value = true;
    const mc_world_storage_status null_static =
        mc_world_block_can_place_base(f->registry, sapling, f->world, 8, 64, 8, &value, &error);
    f->registry->blocks.named[grass] = saved_grass;
    CHECK(null_static != MC_WORLD_STORAGE_OK && value && !error.returned &&
          error.source_exception && !error.native_policy && error.substrate_entered &&
          !error.substrate_returned && error.site.pc == 4 &&
          strcmp(error.site.class_name, "wb") == 0 && strcmp(error.site.method, "d") == 0 &&
          error.query_count == 2 && mc_world_placement_error_retire(&error));

    /* A bounded target read at128 returns air before provider access; its
     * below127 read reaches the genuine nullable provider and fails later. */
    mc_chunk_provider *provider = f->world->fields.v;
    for (size_t below = 0; below < 2; ++below) {
        const int32_t y = below == 0 ? 64 : 128;
        f->world->fields.v = NULL;
        value = true;
        const mc_world_storage_status status =
            mc_world_block_can_place_base(f->registry, sapling, f->world, 8, y, 8, &value, &error);
        f->world->fields.v = provider;
        CHECK(status != MC_WORLD_STORAGE_OK && value && !error.returned &&
              error.acquisition != NULL && error.query_count == below + 1 &&
              error.site.pc == (below == 0 ? 5u : 20u) &&
              strcmp(error.site.class_name, below == 0 ? "uu" : "wb") == 0 &&
              !error.substrate_entered && mc_world_placement_error_retire(&error));
    }

    const mc_block *foreign_sapling = NULL;
    CHECK(mc_registry_block_get(foreign, 6, &foreign_sapling, NULL) == MC_REGISTRY_OK &&
          foreign_sapling != NULL && foreign_sapling != sapling);
    plant_reset(chunks, &supported, NULL);
    value = false;
    const size_t foreign_borrows = foreign->runtime_borrows;
    CHECK(mc_world_block_can_place_base(foreign, foreign_sapling, f->world, 8, 64, 8, &value,
                                        &error) == MC_WORLD_STORAGE_OK &&
          value && error.returned && foreign->runtime_borrows == foreign_borrows + 1 &&
          mc_world_placement_error_retire(&error));
    mc_block *foreign_grass = foreign->blocks.named[grass];
    foreign->blocks.named[grass] = foreign->blocks.m[1];
    value = true;
    const mc_world_storage_status foreign_rebound =
        mc_world_block_can_place_base(foreign, foreign_sapling, f->world, 8, 64, 8, &value, &error);
    foreign->blocks.named[grass] = foreign_grass;
    CHECK(foreign_rebound == MC_WORLD_STORAGE_OK && !value &&
          f->registry->blocks.named[grass] == saved_grass &&
          mc_world_placement_error_retire(&error));
    value = true;
    CHECK(mc_world_block_can_place_base(f->registry, foreign_sapling, f->world, 8, 64, 8, &value,
                                        &error) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          value && error.query_count == 0 && mc_world_placement_error_retire(&error));
    return true;
}

static bool plant_mushroom_controls(fixture *f) {
    const mc_block *mushroom = NULL;
    CHECK(mc_registry_block_get(f->registry, 39, &mushroom, NULL) == MC_REGISTRY_OK &&
          mushroom != NULL);
    static const int32_t invalid[] = {INT32_MIN, -1, 256, INT32_MAX};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) {
        bool value = true;
        mc_world_plant_substrate_error error = {0};
        CHECK(mc_world_plant_substrate(mushroom, invalid[i], &value, &error) ==
                  MC_REGISTRY_INDEX_OUT_OF_RANGE &&
              value && error.registry.status == MC_REGISTRY_INDEX_OUT_OF_RANGE && error.pc == 4 &&
              strcmp(error.class_name, "tl") == 0 && strcmp(error.method, "d") == 0);
    }
    bool *const opacity = f->registry->blocks.o;
    CHECK(opacity != NULL);
    for (size_t state = 0; state < 2; ++state) {
        const bool saved = opacity[255];
        opacity[255] = state != 0;
        bool value = state == 0;
        mc_world_plant_substrate_error error = {0};
        const mc_registry_status status = mc_world_plant_substrate(mushroom, 255, &value, &error);
        opacity[255] = saved;
        CHECK(status == MC_REGISTRY_OK && value == (state != 0) && error.pc == 5 &&
              strcmp(error.class_name, "tl") == 0);
    }
    f->registry->blocks.o = NULL;
    bool value = true;
    mc_world_plant_substrate_error error = {0};
    const mc_registry_status status = mc_world_plant_substrate(mushroom, INT32_MIN, &value, &error);
    f->registry->blocks.o = opacity;
    CHECK(status == MC_REGISTRY_NULL_REFERENCE && value &&
          error.registry.status == MC_REGISTRY_NULL_REFERENCE && error.pc == 4 &&
          strcmp(error.class_name, "tl") == 0);
    return true;
}

static bool plant_run(fixture *f, const char *profiles) {
    mc_chunk_state *chunks[PLANT_CHUNKS] = {0};
    const mc_chunk_acquire_context *acquisition = mc_world_runtime_acquisition(f->world);
    CHECK(acquisition != NULL);
    for (size_t i = 0; i < PLANT_CHUNKS; ++i) {
        uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
        mc_world_chunk *chunk = NULL;
        CHECK(blocks != NULL &&
              mc_world_store_create_generated(f->store, f->world, 0, (int32_t)(i / 2u),
                                              (int32_t)(i % 2u), &blocks, MC_CHUNK_BLOCKS, &chunk,
                                              NULL) == MC_WORLD_STORAGE_OK &&
              blocks == NULL);
        chunks[i] = mc_world_chunk_state(chunk);
        CHECK(chunks[i] != NULL && mc_chunk_cache_publish(acquisition->cache, (int32_t)(i / 2u),
                                                          (int32_t)(i % 2u), chunks[i]));
    }
    mc_registry_classes *foreign = NULL;
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                         mc_resource_files_source(f->files)};
    CHECK(mc_registry_classes_allocate(&options, &foreign, NULL) == MC_REGISTRY_OK &&
          mc_registry_classes_initialize_blocks(foreign, NULL) == MC_REGISTRY_OK);
    for (size_t i = 0; i < sizeof(plant_receivers) / sizeof(*plant_receivers); ++i) {
        const mc_block *block = NULL;
        CHECK(mc_registry_block_get(f->registry, plant_receivers[i], &block, NULL) ==
                  MC_REGISTRY_OK &&
              block != NULL && plant_class(block) != NULL);
        CHECK(printf("CLASS\t%" PRId32 "\t%s\tuu\twb\t%s\n", plant_receivers[i], plant_class(block),
                     plant_substrate_class(block)) > 0);
    }
    uint8_t *prior = malloc(PLANT_CHUNKS * PLANT_ARRAY_BYTES);
    CHECK(prior != NULL);
    const bool profiles_ok = plant_profiles(f, chunks, profiles, prior);
    free(prior);
    CHECK(profiles_ok && plant_owner_controls(f, chunks, foreign) && plant_mushroom_controls(f));
    CHECK(mc_registry_classes_free(foreign, NULL) == MC_REGISTRY_OK);
    CHECK(mc_chunk_cache_init(acquisition->cache, acquisition->cache->entries,
                              acquisition->cache->capacity, acquisition->cache->insertions,
                              acquisition->cache->insertion_capacity));
    for (size_t i = 0; i < PLANT_CHUNKS; ++i)
        CHECK(mc_world_store_retire_chunk(f->store, chunks[i], NULL) == MC_WORLD_STORAGE_OK);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4 || !mc_math_trig_init(&trig))
        return 2;
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1)
        return 2;
#endif
    fixture f = {0};
    if (!plant_setup(&f, argv[1], argv[2]) || !plant_run(&f, argv[3]) || !teardown(&f))
        return 1;
    if (printf("COMPLETE\t8\t288\t864\n") <= 0 || fflush(stdout) != 0 || ferror(stdout))
        return 1;
    return 0;
}
