#ifndef MC_TEST_MAINTENANCE_FIXTURE_H
#define MC_TEST_MAINTENANCE_FIXTURE_H
#include "../../../src/world/runtime/chunk_provider_internal.h"
#include "../../../src/world/runtime/construction_internal.h"
#include "../../../src/world/storage/objects_internal.h"
#include "mc/resources/files/files.h"
#include "mc/statistics/registry/registry.h"
#include "mc/world/dimension/world_provider.h"
#include "mc/world/runtime/entities.h"
#include "mc/world/runtime/maintenance.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
static mc_math_trig trig;
#define REQUIRE(c)                                                                                 \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c);                                \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct maintenance_fixture {
    mc_resource_files *resources;
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_registry_classes *registry;
    mc_entity_classes *entities;
    mc_world_runtime_classes *classes;
    mc_world_regions *regions;
    mc_world_store *store;
    mc_world_info_name *name;
    mc_world_provider *dimension;
    mc_world_runtime *world;
    mc_chunk_provider_fields provider;
} maintenance_fixture;

static bool fixture_open(maintenance_fixture *f, const char *resources, const char *directory) {
    REQUIRE(mc_resource_files_create(resources, &f->resources) == MC_REGISTRY_OK &&
            mc_materials_create(&f->materials));
    REQUIRE(mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    REQUIRE(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entities) ==
            MC_ENTITY_OK);
    const mc_registry_options registry = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                          mc_resource_files_source(f->resources)};
    REQUIRE(mc_registry_classes_allocate(&registry, &f->registry, NULL) == MC_REGISTRY_OK);
    REQUIRE(mc_registry_classes_initialize_statistics(f->registry, NULL) == MC_REGISTRY_OK);
    REQUIRE(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes,
                                            NULL) == MC_WORLD_STORAGE_OK);
    REQUIRE(mc_world_runtime_classes_bind_registry(f->classes, f->registry, NULL) ==
            MC_REGISTRY_OK);
    REQUIRE(mc_world_runtime_classes_bind_entities(f->classes, f->entities) == MC_ENTITY_OK);
    REQUIRE(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    uint8_t map[256] = {0};
    mc_registry_block_view blocks;
    REQUIRE(mc_registry_classes_inspect_blocks(f->registry, &blocks, NULL) == MC_REGISTRY_OK);
    for (size_t i = 1; i < 256; ++i)
        map[i] = blocks.m[i] == NULL ? 0 : (uint8_t)i;
    const mc_world_store_options storage = {.session_token = 61937,
                                            .block_map = map,
                                            .block_map_length = 256,
                                            .limits = mc_world_store_default_limits()};
    REQUIRE(mc_world_store_open(directory, f->regions, &storage, &f->store, NULL) ==
            MC_WORLD_STORAGE_OK);
    static const uint16_t name[] = {'M', 'a', 'i', 'n', 't', 'e', 'n', 'a', 'n', 'c', 'e'};
    REQUIRE(mc_world_info_name_copy((mc_nbt_string){name, 11}, SIZE_MAX, &f->name) ==
            MC_WORLD_INFO_OK);
    REQUIRE(mc_world_provider_create(0, NULL, &f->dimension, NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 256;
    REQUIRE(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, f->classes,
                                            &limits, &f->world, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments arguments = {
        .store = f->store, .name = f->name, .seed = 23, .dimension = f->dimension};
    REQUIRE(mc_world_runtime_construct_world(f->world, &arguments, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_runtime_fields fields;
    REQUIRE(mc_world_runtime_fields_get(f->world, &fields) == MC_WORLD_STORAGE_OK);
    REQUIRE(mc_chunk_provider_bind_fields(fields.v, &f->provider, NULL) == MC_WORLD_STORAGE_OK);
    REQUIRE(f->provider.policy == MC_CHUNK_ACQUIRE_BASE && f->provider.cache->count == 0 &&
            f->provider.pending->count == 0);
    REQUIRE(mc_world_info_time_set(fields.x, 123) == MC_WORLD_INFO_OK);
    return true;
}

static bool fixture_close(maintenance_fixture *f) {
    REQUIRE(mc_world_runtime_retire_entity_containers(f->world, NULL) == MC_WORLD_STORAGE_OK);
    REQUIRE(mc_chunk_pending_clear(f->provider.pending) == MC_CHUNK_ACQUIRE_OK);
    REQUIRE(mc_chunk_cache_init(f->provider.cache, f->provider.cache->entries,
                                f->provider.cache->capacity, f->provider.cache->insertions,
                                f->provider.cache->insertion_capacity));
    mc_world_provider_unbind(f->dimension);
    REQUIRE(mc_world_runtime_retire_world(f->world, NULL) == MC_WORLD_STORAGE_OK);
    while (mc_world_store_chunk_count(f->store) != 0) {
        mc_chunk_state *chunk = NULL;
        REQUIRE(mc_world_store_chunk_at(f->store, 0, &chunk, NULL) == MC_WORLD_STORAGE_OK);
        REQUIRE(mc_world_store_retire_chunk(f->store, chunk, NULL) == MC_WORLD_STORAGE_OK);
    }
    REQUIRE(mc_world_runtime_free(f->world, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_provider_release(f->dimension);
    mc_world_info_name_release(f->name);
    REQUIRE(mc_world_store_free_checked(&f->store, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_regions_free(f->regions);
    REQUIRE(mc_world_runtime_classes_free(f->classes, NULL) == MC_WORLD_STORAGE_OK);
    REQUIRE(mc_registry_classes_free(f->registry, NULL) == MC_REGISTRY_OK);
    REQUIRE(mc_entity_classes_free(f->entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(f->profile);
    mc_materials_destroy(f->materials);
    REQUIRE(mc_resource_files_free(f->resources) == MC_REGISTRY_OK);
    return true;
}

static bool pending_input(maintenance_fixture *f, int32_t x, int32_t z) {
    /* Declared controlled input, not a normal-provider enqueue method. This
     * invokes the actual owned ordered HashSet, paired with original Set.add. */
    bool added = false;
    REQUIRE(mc_chunk_pending_add(f->provider.pending, mc_chunk_cache_key(x, z), &added) ==
                MC_CHUNK_ACQUIRE_OK &&
            added);
    return true;
}
#endif
