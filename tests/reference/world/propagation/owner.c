#include "owner.h"

#include "chunk_provider_internal.h"
#include "millis.h"
#include "objects_internal.h"

#include <stdio.h>
#include <stdlib.h>

bool propagation_reference_owner_create(propagation_reference_owner *o, const char *resources,
                                        const char *directory) {
#define REQUIRE(value)                                                                             \
    do {                                                                                           \
        if (!(value)) {                                                                            \
            fprintf(stderr, "World owner line %d: %s\n", __LINE__, #value);                        \
            return false;                                                                          \
        }                                                                                          \
    } while (0)
    o->trig = malloc(sizeof(*o->trig));
    REQUIRE(o->trig != NULL && mc_math_trig_init(o->trig));
    REQUIRE(mc_resource_files_create(resources, &o->files) == MC_REGISTRY_OK);
    REQUIRE(mc_materials_create(&o->materials));
    REQUIRE(mc_java_random_profile_create(SIZE_MAX, &o->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    const mc_registry_options options = {.side = MC_REGISTRY_CLIENT,
                                         .random_profile = o->profile,
                                         .materials = o->materials,
                                         .resources = mc_resource_files_source(o->files)};
    REQUIRE(mc_registry_classes_allocate(&options, &o->registry, NULL) == MC_REGISTRY_OK);
    REQUIRE(mc_entity_classes_create(MC_ENTITY_CLIENT, o->profile, NULL, &o->entities) ==
            MC_ENTITY_OK);
    REQUIRE(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, o->profile, o->trig, &o->classes,
                                            NULL) == MC_WORLD_STORAGE_OK);
    REQUIRE(mc_world_runtime_classes_bind_registry(o->classes, o->registry, NULL) ==
            MC_REGISTRY_OK);
    REQUIRE(mc_world_runtime_classes_bind_entities(o->classes, o->entities) == MC_ENTITY_OK);
    REQUIRE(mc_registry_classes_initialize_blocks(o->registry, NULL) == MC_REGISTRY_OK);
    REQUIRE(mc_registry_classes_initialize_biomes(o->registry, NULL) == MC_REGISTRY_OK);
    mc_registry_block_view blocks;
    REQUIRE(mc_registry_classes_inspect_blocks(o->registry, &blocks, NULL) == MC_REGISTRY_OK);
    REQUIRE(blocks.phase == MC_REGISTRY_READY && blocks.m != NULL);
    uint8_t block_map[256] = {0};
    for (size_t i = 1; i < sizeof(block_map); ++i)
        block_map[i] = blocks.m[i] == NULL ? 0 : (uint8_t)i;
    REQUIRE(mc_world_regions_create(NULL, &o->regions, NULL) == MC_WORLD_STORAGE_OK);
    int64_t token;
    REQUIRE(mc_platform_current_time_millis(&token) == MC_PLATFORM_MILLIS_OK);
    mc_world_store_options store = {.session_token = token,
                                    .block_map = block_map,
                                    .block_map_length = sizeof(block_map),
                                    .limits = mc_world_store_default_limits()};
    store.limits.max_loaded_chunks = 512;
    REQUIRE(mc_world_store_open(directory, o->regions, &store, &o->store, NULL) ==
            MC_WORLD_STORAGE_OK);
    REQUIRE(mc_world_info_name_copy((mc_nbt_string){0}, SIZE_MAX, &o->name) == MC_WORLD_INFO_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    /* Explicit fixture policy supplied before allocation; no default mutation. */
    limits.max_scheduled_entries = 262144;
    limits.max_provider_chunks = 512;
    REQUIRE(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE, o->classes,
                                            &limits, &o->world, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments arguments = {
        .store = o->store, .name = o->name, .seed = 23};
    REQUIRE(mc_world_runtime_construct_world(o->world, &arguments, NULL) == MC_WORLD_STORAGE_OK);
    return true;
#undef REQUIRE
}

bool propagation_reference_owner_retire(propagation_reference_owner *o) {
    if (o->world != NULL) {
        if (mc_world_runtime_retire_entity_containers(o->world, NULL) != MC_WORLD_STORAGE_OK)
            return false;
        mc_world_runtime_fields fields;
        if (mc_world_runtime_fields_get(o->world, &fields) != MC_WORLD_STORAGE_OK)
            return false;
        if (fields.v != NULL) {
            mc_chunk_provider_fields provider;
            if (mc_chunk_provider_bind_fields(fields.v, &provider, NULL) != MC_WORLD_STORAGE_OK ||
                mc_chunk_pending_clear(provider.pending) != MC_CHUNK_ACQUIRE_OK ||
                !mc_chunk_cache_init(provider.cache, provider.cache->entries,
                                     provider.cache->capacity, provider.cache->insertions,
                                     provider.cache->insertion_capacity))
                return false;
        }
        if (mc_world_runtime_retire_world(o->world, NULL) != MC_WORLD_STORAGE_OK)
            return false;
    }
    while (mc_world_store_chunk_count(o->store) != 0) {
        mc_chunk_state *chunk = NULL;
        if (mc_world_store_chunk_at(o->store, 0, &chunk, NULL) != MC_WORLD_STORAGE_OK ||
            mc_world_store_retire_chunk(o->store, chunk, NULL) != MC_WORLD_STORAGE_OK)
            return false;
    }
    if (mc_world_runtime_free(o->world, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    o->world = NULL;
    if (mc_world_store_free_checked(&o->store, NULL) != MC_WORLD_STORAGE_OK ||
        mc_world_runtime_classes_free(o->classes, NULL) != MC_WORLD_STORAGE_OK ||
        mc_registry_classes_free(o->registry, NULL) != MC_REGISTRY_OK ||
        mc_entity_classes_free(o->entities) != MC_ENTITY_OK)
        return false;
    mc_world_info_name_release(o->name);
    mc_world_regions_free(o->regions);
    mc_materials_destroy(o->materials);
    mc_java_random_profile_free(o->profile);
    free(o->trig);
    return mc_resource_files_free(o->files) == MC_REGISTRY_OK;
}
