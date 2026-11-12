/* Real World-only constructor/retirement APIs; no App/Player fabricated. */
typedef struct rd_support_fixture {
    mc_resource_files *files;
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_entity_classes *entities;
    mc_registry_classes *registry;
    mc_world_runtime_classes *world_classes;
    mc_world_regions *regions;
    mc_world_store *store;
    mc_world_provider *dimension;
    mc_world_runtime *world;
    mc_world_info_name *name;
} rd_support_fixture;

static bool rd_support_setup(rd_support_fixture *f, const char *resources, const char *directory) {
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
    static const uint16_t name[] = {0};
    CHECK(mc_world_info_name_copy((mc_nbt_string){name, 0}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_world_provider_create(0, NULL, &f->dimension, NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = RD_CHUNKS;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, f->world_classes,
                                          &limits, &f->world, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments arguments = {
        .store = f->store, .name = f->name, .seed = 23, .dimension = f->dimension};
    CHECK(mc_world_runtime_construct_world(f->world, &arguments, NULL) == MC_WORLD_STORAGE_OK);
    return true;
}

static bool rd_support_teardown(rd_support_fixture *f, mc_chunk_state **chunks) {
    CHECK(rd_terminal_chunks(f->world, f->store, f->dimension, chunks));
    CHECK(rd_terminal_world_free(f->world));
    mc_world_provider_release(f->dimension);
    CHECK(mc_world_store_free_checked(&f->store, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_regions_free(f->regions);
    mc_world_info_name_release(f->name);
    CHECK(mc_world_runtime_classes_free(f->world_classes, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_registry_classes_free(f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_entity_classes_free(f->entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(f->profile);
    mc_materials_destroy(f->materials);
    mc_resource_files_free(f->files);
    return true;
}
