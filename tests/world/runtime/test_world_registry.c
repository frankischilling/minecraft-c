#include "mc/block/registry/runtime.h"
#include "mc/item/registry/registry.h"
#include "mc/resources/translation/translation.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/storage/chunk.h"

#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/world/runtime/chunk_provider_internal.h"
#include "../../support/java_random.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
static unsigned int checks;
static mc_math_trig trig;
static uint8_t block_bytes[MC_CHUNK_BLOCKS];
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #c);                        \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static const uint8_t english[] = "stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                                 "stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n";

typedef struct byte_stream {
    const uint8_t *bytes;
    size_t length, offset, opens, reads, closes;
    bool closed;
} byte_stream;

typedef struct resources {
    mc_registry_resources source;
    byte_stream streams[3];
    mc_registry_classes *registry;
    mc_world_runtime *observed_world;
    size_t protected_calls;
    bool missing_first, invalid;
} resources;

/* Heap-owned so an unexpected refused retirement preserves even the resource
 * descriptors borrowed by a surviving registry, rather than stack addresses. */
typedef struct fixture {
    mc_materials *materials;
    mc_java_random_profile *profile, *other_profile;
    mc_world_regions *regions;
    mc_world_store *store;
    mc_world_info_name *name;
    mc_registry_classes *registries[4];
    mc_world_runtime_classes *classes[3];
    mc_world_runtime *worlds[3];
    mc_world_provider *dimensions[8];
    resources sources[4];
    mc_chunk_provider_fields providers[3];
    bool restored[3];
    mc_chunk_state *chunk;
} fixture;

static uint64_t uniquifier(const mc_java_random_profile *profile) {
    return atomic_load_explicit(&profile->uniquifier, memory_order_relaxed);
}

static uint64_t advanced(uint64_t value, unsigned int count) {
    while (count-- != 0)
        value *= UINT64_C(1181783497276652981);
    return value;
}

static bool same_error(const mc_registry_error *a, const mc_registry_error *b) {
    return a->status == b->status && a->message == b->message &&
           a->initializer_pc == b->initializer_pc && a->block_id == b->block_id &&
           a->constructor_class == b->constructor_class && a->constructor_pc == b->constructor_pc &&
           a->profile_status == b->profile_status &&
           a->active_initializer_class == b->active_initializer_class &&
           a->active_initializer_pc == b->active_initializer_pc &&
           a->item_argument == b->item_argument &&
           a->format_error.status == b->format_error.status &&
           a->format_error.offset == b->format_error.offset &&
           a->format_error.length == b->format_error.length &&
           a->format_error.value == b->format_error.value &&
           a->format_error.flags == b->format_error.flags &&
           a->format_error.conversion == b->format_error.conversion &&
           a->format_error.date_time == b->format_error.date_time;
}

static mc_java_properties_status read_bytes(void *context, uint8_t *out, size_t capacity,
                                            size_t *count) {
    byte_stream *s = context;
    ++s->reads;
    if (out == NULL || count == NULL || capacity == 0 || s->closed || s->offset > s->length)
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
    for (size_t i = 0; i < COUNT(names); ++i) {
        if (name == NULL || strcmp(name, names[i]) != 0)
            continue;
        if (out == NULL || ++r->streams[i].opens != 1) {
            r->invalid = true;
            return MC_REGISTRY_INVALID_ARGUMENT;
        }
        if (r->observed_world != NULL) {
            if (mc_world_runtime_registry(r->observed_world) != r->registry ||
                mc_world_runtime_retire_world(r->observed_world, NULL) != MC_WORLD_STORAGE_BUSY ||
                mc_world_runtime_free(r->observed_world, NULL) != MC_WORLD_STORAGE_BUSY ||
                mc_registry_classes_free(r->registry, NULL) != MC_REGISTRY_BUSY)
                r->invalid = true;
            ++r->protected_calls;
        }
        if (i == 0 && r->missing_first)
            return MC_REGISTRY_NULL_REFERENCE;
        *out = (mc_java_properties_input){&r->streams[i], read_bytes};
        return MC_REGISTRY_OK;
    }
    r->invalid = true;
    return MC_REGISTRY_INVALID_ARGUMENT;
}

static mc_registry_status close_resource(void *context, const mc_java_properties_input *input) {
    resources *r = context;
    if (input == NULL || input->context != &r->streams[2] || input->read != read_bytes) {
        r->invalid = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    ++r->streams[2].closes;
    r->streams[2].closed = true;
    return MC_REGISTRY_OK;
}

static bool ready_resource(void *context, const mc_java_properties_input *input) {
    resources *r = context;
    if (input == NULL || input->read != read_bytes)
        r->invalid = true;
    return false;
}

static mc_registry_status registry_create(fixture *f, size_t index, mc_registry_side side,
                                          mc_java_random_profile *profile, bool missing) {
    resources *r = &f->sources[index];
    *r = (resources){.missing_first = missing};
    r->streams[0] = (byte_stream){.bytes = english, .length = sizeof(english) - 1};
    r->streams[1] = (byte_stream){.bytes = english};
    r->streams[2] = (byte_stream){.bytes = english};
    r->source = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {side, profile, f->materials, 0, &r->source};
    mc_registry_status status = mc_registry_classes_allocate(&options, &f->registries[index], NULL);
    if (status == MC_REGISTRY_OK)
        r->registry = f->registries[index];
    return status;
}

static bool resources_unread(const resources *r) {
    for (size_t i = 0; i < COUNT(r->streams); ++i)
        if (r->streams[i].opens != 0 || r->streams[i].reads != 0 || r->streams[i].closes != 0)
            return false;
    return !r->invalid && r->protected_calls == 0;
}

static bool registry_cold(mc_registry_classes *registry) {
    mc_registry_block_view block;
    mc_registry_item_view item;
    return mc_registry_classes_inspect_blocks(registry, &block, NULL) == MC_REGISTRY_OK &&
           mc_registry_classes_inspect_items(registry, &item, NULL) == MC_REGISTRY_OK &&
           block.phase == MC_REGISTRY_COLD && block.m == NULL && block.named_blocks == 0 &&
           item.phase == MC_REGISTRY_COLD && item.b == NULL && item.c == NULL &&
           item.named_items == 0;
}

static bool class_drop(mc_world_runtime_classes **classes) {
    if (mc_world_runtime_classes_free(*classes, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    *classes = NULL;
    return true;
}

static bool world_drop(mc_world_runtime **world) {
    if (*world == NULL)
        return true;
    if (mc_world_runtime_retire_world(*world, NULL) != MC_WORLD_STORAGE_OK ||
        mc_world_runtime_free(*world, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    *world = NULL;
    return true;
}

static int test_bindings(fixture *f) {
    int result = 1;
    mc_world_storage_error error;
    mc_registry_error registry_error;
    mc_world_runtime_construction state;
    mc_world_runtime_class_state class_state;
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    const uint64_t u = uniquifier(f->profile);
    const size_t profile_bytes = mc_java_random_profile_allocation_bytes(f->profile);
    CHECK(registry_create(f, 0, MC_REGISTRY_CLIENT, f->profile, false) == MC_REGISTRY_OK);
    CHECK(registry_create(f, 1, MC_REGISTRY_SERVER, f->profile, false) == MC_REGISTRY_OK);
    CHECK(registry_create(f, 2, MC_REGISTRY_CLIENT, f->other_profile, false) == MC_REGISTRY_OK);
    for (size_t i = 0; i < COUNT(f->classes); ++i)
        CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig,
                                              &f->classes[i], &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(NULL, f->registries[0], &registry_error) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(mc_world_runtime_classes_bind_registry(f->classes[0], NULL, &registry_error) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(mc_world_runtime_classes_bind_registry(f->classes[0], f->registries[1],
                                                 &registry_error) == MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(mc_registry_classes_free(f->registries[1], NULL) == MC_REGISTRY_OK);
    f->registries[1] = NULL;
    CHECK(mc_world_runtime_classes_bind_registry(f->classes[0], f->registries[2],
                                                 &registry_error) == MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(mc_registry_classes_free(f->registries[2], NULL) == MC_REGISTRY_OK);
    f->registries[2] = NULL;
    CHECK(mc_world_runtime_classes_get(f->classes[0], &class_state) == MC_WORLD_STORAGE_OK &&
          !class_state.initialized && class_state.live_worlds == 0);
    mc_registry_block_view before, after;
    CHECK(mc_registry_classes_inspect_blocks(f->registries[0], &before, NULL) == MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_bind_registry(f->classes[0], f->registries[0],
                                                 &registry_error) == MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_bind_registry(f->classes[1], f->registries[0],
                                                 &registry_error) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_inspect_blocks(f->registries[0], &after, NULL) == MC_REGISTRY_OK &&
          before.phase == after.phase && before.initializer_pc == after.initializer_pc &&
          before.allocation_bytes == after.allocation_bytes);
    CHECK(registry_cold(f->registries[0]) && resources_unread(&f->sources[0]) &&
          uniquifier(f->profile) == u &&
          mc_java_random_profile_allocation_bytes(f->profile) == profile_bytes &&
          f->profile->math == NULL);
    CHECK(mc_world_runtime_classes_bind_registry(f->classes[0], f->registries[0],
                                                 &registry_error) == MC_REGISTRY_BUSY);
    CHECK(registry_create(f, 3, MC_REGISTRY_CLIENT, f->profile, false) == MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_bind_registry(f->classes[0], f->registries[3],
                                                 &registry_error) == MC_REGISTRY_BUSY);
    CHECK(mc_registry_classes_free(f->registries[3], NULL) == MC_REGISTRY_OK);
    f->registries[3] = NULL;
    CHECK(mc_registry_classes_free(f->registries[0], NULL) == MC_REGISTRY_BUSY);
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, f->classes[0],
                                          NULL, &f->worlds[0], &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_registry(f->worlds[0]) == f->registries[0] &&
          mc_world_runtime_registry(NULL) == NULL);
    CHECK(mc_world_runtime_construction_get(f->worlds[0], &state) == MC_WORLD_STORAGE_OK &&
          state.phase == MC_WORLD_RUNTIME_RAW);
    CHECK(mc_world_runtime_last_registry_error(f->worlds[0]) != NULL &&
          mc_world_runtime_last_registry_error(f->worlds[0])->status == MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_free(f->classes[0], &error) == MC_WORLD_STORAGE_BUSY);
    CHECK(mc_world_runtime_retire_world(f->worlds[0], &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_construction_get(f->worlds[0], &state) == MC_WORLD_STORAGE_OK &&
          state.phase == MC_WORLD_RUNTIME_RETIRED &&
          mc_world_runtime_registry(f->worlds[0]) == f->registries[0]);
    CHECK(world_drop(&f->worlds[0]));
    CHECK(mc_registry_classes_free(f->registries[0], NULL) == MC_REGISTRY_BUSY);

    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, f->classes[2],
                                          NULL, &f->worlds[1], &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_registry(f->worlds[1]) == NULL &&
          mc_world_runtime_classes_bind_registry(f->classes[2], f->registries[0], NULL) ==
              MC_REGISTRY_BUSY);
    CHECK(world_drop(&f->worlds[1]));
    CHECK(mc_world_runtime_classes_get(f->classes[2], &class_state) == MC_WORLD_STORAGE_OK &&
          class_state.initialized && class_state.live_worlds == 0);
    CHECK(mc_world_runtime_classes_bind_registry(f->classes[2], f->registries[0], NULL) ==
          MC_REGISTRY_BUSY);

    CHECK(mc_world_provider_create(0, NULL, &f->dimensions[0], NULL) == MC_WORLD_PROVIDER_OK);
    limits.max_provider_allocation_bytes = 0;
    const mc_world_runtime_world_arguments arguments = {
        .store = f->store, .name = f->name, .seed = 23, .dimension = f->dimensions[0]};
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, f->classes[0],
                                          &limits, &f->worlds[2], &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_construct_world(f->worlds[2], &arguments, &error) ==
          MC_WORLD_STORAGE_LIMIT);
    CHECK(mc_world_runtime_construction_get(f->worlds[2], &state) == MC_WORLD_STORAGE_OK &&
          state.phase == MC_WORLD_RUNTIME_FAILED_CONSTRUCTION && state.failed_call_pc == 305);
    CHECK(mc_world_runtime_registry(f->worlds[2]) == f->registries[0] &&
          registry_cold(f->registries[0]) && resources_unread(&f->sources[0]));
    CHECK(uniquifier(f->profile) == advanced(u, 2));
    CHECK(mc_world_runtime_retire_world(f->worlds[2], NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_registry(f->worlds[2]) == f->registries[0]);
    CHECK(world_drop(&f->worlds[2]));
    mc_world_provider_release(f->dimensions[0]);
    f->dimensions[0] = NULL;
    CHECK(class_drop(&f->classes[0]) &&
          mc_registry_classes_free(f->registries[0], NULL) == MC_REGISTRY_BUSY);
    CHECK(class_drop(&f->classes[1]) && class_drop(&f->classes[2]));
    CHECK(mc_registry_classes_free(f->registries[0], NULL) == MC_REGISTRY_OK);
    f->registries[0] = NULL;
    result = 0;
cleanup:
    return result;
}

static size_t block_index(size_t x, size_t y, size_t z) {
    return (x * 16u + z) * 128u + y;
}

static int create_query_worlds(fixture *f) {
    int result = 1;
    mc_world_storage_error error;
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 2;
    CHECK(registry_create(f, 0, MC_REGISTRY_CLIENT, f->profile, false) == MC_REGISTRY_OK);
    CHECK(registry_create(f, 2, MC_REGISTRY_CLIENT, f->profile, true) == MC_REGISTRY_OK);
    for (size_t i = 0; i < COUNT(f->classes); ++i) {
        CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig,
                                              &f->classes[i], &error) == MC_WORLD_STORAGE_OK);
        if (i != 1)
            CHECK(mc_world_runtime_classes_bind_registry(f->classes[i], f->registries[i], NULL) ==
                  MC_REGISTRY_OK);
        CHECK(mc_world_provider_create(0, NULL, &f->dimensions[i], NULL) == MC_WORLD_PROVIDER_OK);
        const mc_world_runtime_world_arguments arguments = {
            .store = f->store, .name = f->name, .seed = 23, .dimension = f->dimensions[i]};
        uint64_t before = uniquifier(f->profile);
        CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA,
                                              f->classes[i], &limits, &f->worlds[i],
                                              &error) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_construct_world(f->worlds[i], &arguments, &error) ==
              MC_WORLD_STORAGE_OK);
        mc_world_runtime_construction state;
        mc_world_runtime_fields fields;
        CHECK(mc_world_runtime_construction_get(f->worlds[i], &state) == MC_WORLD_STORAGE_OK &&
              state.phase == MC_WORLD_RUNTIME_READY);
        CHECK(mc_world_runtime_fields_get(f->worlds[i], &fields) == MC_WORLD_STORAGE_OK &&
              fields.v != NULL);
        CHECK(mc_chunk_provider_bind_fields(fields.v, &f->providers[i], &error) ==
              MC_WORLD_STORAGE_OK);
        CHECK(uniquifier(f->profile) == advanced(before, 3));
    }
    CHECK(registry_cold(f->registries[0]) && registry_cold(f->registries[2]) &&
          resources_unread(&f->sources[0]) && resources_unread(&f->sources[2]));
    mc_chunk_data data;
    memset(block_bytes, 0, sizeof(block_bytes));
    CHECK(mc_chunk_data_init(&data, block_bytes, sizeof(block_bytes)));
    /* Literal original Block IDs in the paired registry/spawn primary:
     * F sand12, u stone1, A bedrock7; water8 and unused255 probe other branches. */
    const uint8_t tops[] = {0, 12, 1, 7, 8, 255};
    for (size_t x = 0; x < COUNT(tops); ++x)
        block_bytes[block_index(x, 63, 0)] = tops[x];
    const mc_chunk_state source = {.data = &data, .terrain_populated = true};
    CHECK(mc_world_store_retain_chunk(f->store, 0, &source, &f->chunk, &error) ==
          MC_WORLD_STORAGE_OK);
    f->chunk->active = true;
    for (size_t i = 0; i < COUNT(f->providers); ++i) {
        /* Explicitly restored, validated real cache baseline. This does NOT
         * impersonate generation, activation, population or loader success. */
        CHECK(mc_chunk_cache_publish(f->providers[i].cache, 0, 0, f->chunk));
        f->restored[i] = true;
        CHECK(mc_chunk_cache_validate(f->providers[i].cache));
    }
    const int32_t dimension_ids[] = {1, -1, 1, -1};
    for (size_t i = 0; i < COUNT(dimension_ids); ++i) {
        CHECK(mc_world_provider_create(dimension_ids[i], NULL, &f->dimensions[i + 3], NULL) ==
              MC_WORLD_PROVIDER_OK);
        CHECK(mc_world_provider_bind(f->dimensions[i + 3], f->worlds[i < 2 ? 0 : 1], NULL) ==
              MC_WORLD_PROVIDER_OK);
    }
    result = 0;
cleanup:
    return result;
}

static int test_queries(fixture *f) {
    int result = 1;
    mc_material *air = mc_materials_get(f->materials, MC_MATERIAL_a), *material = air;
    mc_world_storage_error error;
    mc_world_provider_error dimension_error;
    bool allowed = true;
    CHECK(create_query_worlds(f) == 0);
    uint64_t u = uniquifier(f->profile);
    const mc_world_regions_usage usage = mc_world_regions_get_usage(f->regions);
    CHECK(mc_world_runtime_material_get(f->worlds[0], 32000000, 63, 0, &material, &error) ==
              MC_WORLD_STORAGE_OK &&
          material == air);
    CHECK(mc_world_runtime_material_get(f->worlds[0], 0, 63, 0, &material, &error) ==
              MC_WORLD_STORAGE_OK &&
          material == air);
    CHECK(mc_world_regions_get_usage(f->regions).count == usage.count &&
          mc_world_regions_get_usage(f->regions).bytes == usage.bytes);
    CHECK(mc_world_provider_can_spawn(f->dimensions[3], 0, 0, &allowed, &dimension_error) ==
              MC_WORLD_PROVIDER_OK &&
          !allowed);
    allowed = true;
    CHECK(mc_world_provider_can_spawn(f->dimensions[5], 0, 0, &allowed, &dimension_error) ==
              MC_WORLD_PROVIDER_OK &&
          !allowed);
    CHECK(registry_cold(f->registries[0]) && resources_unread(&f->sources[0]) &&
          uniquifier(f->profile) == u);
    material = air;
    CHECK(mc_world_runtime_material_get(f->worlds[1], 128, 63, 128, &material, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          material == air);
    /* This genuine cache miss now enters concrete generation. Its numerical
     * manager has no actual reference capability without a Registry binding;
     * failure precedes the later material lookup and retires its new child. */
    const mc_registry_error *generation_cause = mc_world_runtime_last_registry_error(f->worlds[1]);
    CHECK(generation_cause->status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY &&
          generation_cause->message != NULL &&
          strcmp(generation_cause->message, "numerical biome has no actual reference capability") ==
              0 &&
          mc_world_store_chunk_count(f->store) == 1 && f->providers[1].cache->count == 1);
    allowed = true;
    CHECK(mc_world_provider_can_spawn(f->dimensions[1], 128, 128, &allowed, &dimension_error) ==
              MC_WORLD_PROVIDER_WORLD_FAILED &&
          allowed && dimension_error.bytecode == 6 &&
          dimension_error.world.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          dimension_error.registry.status == MC_REGISTRY_OK);
    CHECK(mc_world_runtime_material_get(f->worlds[1], 1, 63, 0, &material, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          material == air &&
          mc_world_runtime_last_registry_error(f->worlds[1])->status ==
              MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY);
    allowed = true;
    CHECK(mc_world_provider_can_spawn(f->dimensions[1], 1, 0, &allowed, &dimension_error) ==
              MC_WORLD_PROVIDER_REGISTRY_FAILED &&
          allowed && dimension_error.bytecode == 11 &&
          dimension_error.registry.status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY);
    CHECK(mc_world_provider_can_spawn(f->dimensions[6], 0, 0, &allowed, &dimension_error) ==
              MC_WORLD_PROVIDER_REGISTRY_FAILED &&
          allowed && dimension_error.bytecode == 11 &&
          dimension_error.registry.status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY);
    CHECK(mc_world_provider_can_spawn(f->dimensions[5], 2, 0, &allowed, &dimension_error) ==
              MC_WORLD_PROVIDER_REGISTRY_FAILED &&
          allowed && dimension_error.bytecode == 16 &&
          dimension_error.registry.status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY);
    CHECK(registry_cold(f->registries[0]) && resources_unread(&f->sources[0]) &&
          uniquifier(f->profile) == u);

    f->sources[0].observed_world = f->worlds[0];
    mc_world_runtime_fields world_fields;
    CHECK(mc_world_runtime_fields_get(f->worlds[0], &world_fields) == MC_WORLD_STORAGE_OK &&
          world_fields.r != NULL);
    const mc_java_random before = *world_fields.r;
    CHECK(mc_world_runtime_material_get(f->worlds[0], 2, 63, 0, &material, &error) ==
              MC_WORLD_STORAGE_OK &&
          material == mc_materials_get(f->materials, MC_MATERIAL_e));
    CHECK(uniquifier(f->profile) == advanced(u, 5) && f->profile->math == NULL &&
          f->sources[0].protected_calls == 3 && !f->sources[0].invalid &&
          mc_test_java_random_equal(&before, world_fields.r));
    mc_registry_block_view blocks;
    CHECK(mc_registry_classes_inspect_blocks(f->registries[0], &blocks, NULL) == MC_REGISTRY_OK &&
          blocks.phase == MC_REGISTRY_READY && blocks.initializer_pc == 3677);
    const mc_block *block = NULL;
    CHECK(mc_registry_block_get(f->registries[0], 1, &block, NULL) == MC_REGISTRY_OK &&
          block != NULL && mc_block_fields_get(block)->bA == material);
    material = air;
    CHECK(mc_world_runtime_material_get(f->worlds[0], 5, 63, 0, &material, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          material == air &&
          mc_world_runtime_last_registry_error(f->worlds[0])->status == MC_REGISTRY_NULL_REFERENCE);
    CHECK(mc_world_runtime_material_get(f->worlds[0], 0, 63, 0, NULL, &error) ==
          MC_WORLD_STORAGE_INVALID_ARGUMENT);
    static const bool base_expected[] = {false, true, false, false, false};
    static const bool sky_expected[] = {false, true, true, true, false};
    for (size_t x = 0; x < COUNT(base_expected); ++x) {
        allowed = !base_expected[x];
        CHECK(mc_world_provider_can_spawn(f->dimensions[0], (int32_t)x, 0, &allowed,
                                          &dimension_error) == MC_WORLD_PROVIDER_OK &&
              allowed == base_expected[x]);
        allowed = !sky_expected[x];
        CHECK(mc_world_provider_can_spawn(f->dimensions[3], (int32_t)x, 0, &allowed,
                                          &dimension_error) == MC_WORLD_PROVIDER_OK &&
              allowed == sky_expected[x]);
        bool opaque = false;
        const int32_t id = x == 0 ? 0 : x == 1 ? 12 : x == 2 ? 1 : x == 3 ? 7 : 8;
        if (id != 0 && id != 7)
            CHECK(mc_registry_block_opaque_get(f->registries[0], id, &opaque, NULL) ==
                  MC_REGISTRY_OK);
        allowed = true;
        CHECK(mc_world_provider_can_spawn(f->dimensions[4], (int32_t)x, 0, &allowed,
                                          &dimension_error) == MC_WORLD_PROVIDER_OK &&
              allowed == opaque);
    }
    allowed = true;
    CHECK(mc_world_provider_can_spawn(f->dimensions[3], 5, 0, &allowed, &dimension_error) ==
              MC_WORLD_PROVIDER_REGISTRY_FAILED &&
          allowed && dimension_error.bytecode == 21 &&
          dimension_error.registry.status == MC_REGISTRY_NULL_REFERENCE);
    CHECK(mc_test_java_random_equal(&before, world_fields.r) && f->sources[0].protected_calls == 3);

    /* A real missing resource during Dimension's Block active use, not injected
     * class phases or a fabricated registry failure. */
    f->sources[2].observed_world = f->worlds[2];
    u = uniquifier(f->profile);
    allowed = true;
    CHECK(mc_world_provider_can_spawn(f->dimensions[2], 1, 0, &allowed, &dimension_error) ==
              MC_WORLD_PROVIDER_REGISTRY_FAILED &&
          allowed && dimension_error.bytecode == 11 &&
          dimension_error.registry.status == MC_REGISTRY_NULL_REFERENCE);
    const mc_registry_error first = dimension_error.registry;
    CHECK(first.active_initializer_class != NULL &&
          strcmp(first.active_initializer_class, "nh") == 0 && first.active_initializer_pc == 4 &&
          first.constructor_class != NULL && strcmp(first.constructor_class, "nh") == 0 &&
          first.constructor_pc == 26);
    CHECK(f->sources[2].protected_calls == 1 && !f->sources[2].invalid &&
          uniquifier(f->profile) == advanced(u, 5));
    CHECK(mc_registry_classes_inspect_blocks(f->registries[2], &blocks, NULL) == MC_REGISTRY_OK &&
          blocks.phase == MC_REGISTRY_FAILED);
    const size_t bytes = blocks.allocation_bytes;
    const uint64_t failed_u = uniquifier(f->profile);
    mc_registry_error expected = first;
    expected.status = MC_REGISTRY_FAILED_INITIALIZATION;
    material = air;
    CHECK(mc_world_runtime_material_get(f->worlds[2], 2, 63, 0, &material, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          material == air &&
          same_error(&expected, mc_world_runtime_last_registry_error(f->worlds[2])));
    CHECK(mc_registry_classes_inspect_blocks(f->registries[2], &blocks, NULL) == MC_REGISTRY_OK &&
          blocks.allocation_bytes == bytes && f->sources[2].streams[0].opens == 1 &&
          f->sources[2].streams[0].reads == 0 && f->sources[2].protected_calls == 1 &&
          uniquifier(f->profile) == failed_u && f->sources[2].streams[1].opens == 0 &&
          f->sources[2].streams[2].opens == 0);
    CHECK(mc_world_runtime_retire_world(f->worlds[0], &error) == MC_WORLD_STORAGE_BUSY);
    result = 0;
cleanup:
    return result;
}

static bool cleanup_fixture(fixture *f) {
    for (size_t i = 0; i < COUNT(f->restored); ++i) {
        if (!f->restored[i])
            continue;
        mc_chunk_cache *cache = f->providers[i].cache;
        if (!mc_chunk_cache_validate(cache) || cache->count != 1 || cache->insertion_count != 1 ||
            cache->entries[0].chunk != f->chunk || cache->insertions[0] != f->chunk)
            return false;
        cache->count = 0;
        cache->insertion_count = 0;
        f->restored[i] = false;
    }
    for (size_t i = 0; i < COUNT(f->dimensions); ++i)
        mc_world_provider_unbind(f->dimensions[i]);
    for (size_t i = 0; i < COUNT(f->worlds); ++i)
        if (!world_drop(&f->worlds[i]))
            return false;
    if (f->chunk != NULL) {
        f->chunk->active = false;
        if (mc_world_store_retire_chunk(f->store, f->chunk, NULL) != MC_WORLD_STORAGE_OK)
            return false;
        f->chunk = NULL;
    }
    for (size_t i = 0; i < COUNT(f->dimensions); ++i) {
        mc_world_provider_release(f->dimensions[i]);
        f->dimensions[i] = NULL;
    }
    for (size_t i = 0; i < COUNT(f->classes); ++i)
        if (!class_drop(&f->classes[i]))
            return false;
    for (size_t i = 0; i < COUNT(f->registries); ++i) {
        if (mc_registry_classes_free(f->registries[i], NULL) != MC_REGISTRY_OK)
            return false;
        f->registries[i] = NULL;
    }
    mc_world_info_name_release(f->name);
    mc_world_store_free(f->store);
    mc_world_regions_free(f->regions);
    mc_java_random_profile_free(f->other_profile);
    mc_java_random_profile_free(f->profile);
    mc_materials_destroy(f->materials);
    free(f);
    return true;
}

static bool decode_path(const char *hex, char *out, size_t capacity) {
    size_t n = strlen(hex);
    if (n == 0 || n % 2u != 0 || n / 2u >= capacity)
        return false;
    for (size_t i = 0; i < n / 2u; ++i) {
        unsigned int value = 0;
        for (size_t j = 0; j < 2; ++j) {
            unsigned char c = (unsigned char)hex[2 * i + j];
            unsigned int digit = c >= '0' && c <= '9'   ? (unsigned int)(c - '0')
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
    char root[8192];
    fixture *f = NULL;
    uint8_t map[256];
    const uint16_t title[] = {'R', 'e', 'g', 'i', 's', 't', 'r', 'y'};
    if (argc != 3 || strcmp(argv[1], "world-registry") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    if (f == NULL)
        return 1;
    for (size_t i = 0; i < COUNT(map); ++i)
        map[i] = (uint8_t)i;
    const mc_world_store_options options = {.session_token = 123,
                                            .block_map = map,
                                            .block_map_length = sizeof(map),
                                            .limits = mc_world_store_default_limits()};
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_open(root, f->regions, &options, &f->store, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->other_profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    CHECK(test_bindings(f) == 0);
    CHECK(test_queries(f) == 0);
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs(
            "World registry fixture retirement refused; live borrowers and dependencies retained\n",
            stderr);
        return 1;
    }
    if (result == 0 &&
        printf("PASS: %u constructed World registry checks (world-registry)\n", checks) < 0)
        result = 1;
    return result;
}
