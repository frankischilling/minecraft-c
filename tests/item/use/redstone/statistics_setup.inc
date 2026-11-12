/* Private fixture-input projection of unchanged genuine stats owner setup.
 * Only App user, World path and World constructor-name inputs differ. */
static bool rd_use_setup(fixture *f, const char *resources, const char *directory,
                         bool construct_writer, mc_chunk_state **chunks) {
    memset(f, 0, sizeof(*f));
    CHECK(mc_resource_files_create(resources, &f->app.files) == MC_REGISTRY_OK);
    CHECK(mc_materials_create(&f->app.materials));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->app.profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->app.profile, NULL, &f->app.entities) ==
          MC_ENTITY_OK);
    CHECK(mc_client_render_classes_create(f->app.entities, f->app.profile,
                                          &f->app.render_classes) == MC_CLIENT_MODEL_OK);
    const mc_registry_options ro = {MC_REGISTRY_CLIENT, f->app.profile, f->app.materials, 0,
                                    mc_resource_files_source(f->app.files)};
    CHECK(mc_registry_classes_allocate(&ro, &f->app.registry, NULL) == MC_REGISTRY_OK);
    mc_client_achievement_notice *notice = NULL;
    CHECK(mc_client_achievement_notice_allocate(f->app.render_classes, f->app.registry,
                                                f->app.profile, &notice,
                                                NULL) == MC_CLIENT_NOTICE_OK);
    CHECK(mc_client_achievement_notice_construct(notice, &f->app, NULL) == MC_CLIENT_NOTICE_OK);
    f->app.achievement_notice = notice; /* Actual u publication after real constructor. */
    f->notice_retained = notice;
    mc_client_statistics_owner_error owner;
    CHECK(mc_client_application_create_session(&f->app, NULL, NULL, &owner) ==
          MC_CLIENT_STATISTICS_OWNER_INVALID_PHASE);
    CHECK(mc_client_application_construct_water_effect(&f->app, NULL, &owner) ==
          MC_CLIENT_STATISTICS_OWNER_OK);
    CHECK(f->app.water_effect != NULL && f->app.water_effect == f->app.water_effect_retained);
    CHECK(mc_client_application_statistics_active_use(&f->app, &owner) ==
          MC_CLIENT_STATISTICS_OWNER_OK);
    CHECK(f->app.statistics_active_use_completed);
    static const uint16_t user[] = {'R', 'e', 'd', 's', 't', 'o', 'n', 'e', 'U', 's',
                                    'e', 'R', 'e', 'f', 'e', 'r', 'e', 'n', 'c', 'e'},
                          token[] = {'-'};
    mc_player_string *username = NULL, *session_id = NULL;
    CHECK(mc_player_string_create(user, sizeof(user) / sizeof(*user), &username) == MC_PLAYER_OK);
    CHECK(mc_player_string_create(token, 1, &session_id) == MC_PLAYER_OK);
    CHECK(mc_client_application_create_session(&f->app, username, session_id, &owner) ==
          MC_CLIENT_STATISTICS_OWNER_OK);
    mc_player_string_release(username);
    mc_player_string_release(session_id);
    CHECK(mc_client_game_directory_create(directory, NULL, &f->app.diagnostic_game_directory, NULL,
                                          NULL) == MC_CLIENT_GAME_DIRECTORY_OK);
    if (construct_writer) {
        mc_client_stats_options options = mc_client_stats_default_options();
        options.charset = MC_JAVA_READER_UTF8;
        options.diagnostics.emit = quiet;
        CHECK(mc_client_application_construct_statistics(&f->app, &options, NULL, &owner) ==
              MC_CLIENT_STATISTICS_OWNER_OK);
        CHECK(f->app.stats_writer == f->app.stats_writer_retained && f->app.stats_writer != NULL);
        mc_client_stats_state state;
        CHECK(mc_client_stats_writer_read_state(f->app.stats_writer, &state));
        CHECK(state.workers_started == 1 && state.workers_joined == 0); /* No startup join. */
        CHECK(mc_client_stats_writer_join(f->app.stats_writer, NULL) ==
              MC_CLIENT_STATS_OK); /* Finite test observation. */
    }
    mc_registry_achievement_view av;
    mc_registry_statistic_view sv;
    CHECK(mc_registry_classes_inspect_achievements(f->app.registry, &av, NULL) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_inspect_statistics(f->app.registry, &sv, NULL) == MC_REGISTRY_OK);
    f->root = av.named[0];
    f->child = av.named[1];
    f->next = av.named[2];
    f->general = sv.named[0];
    CHECK(mc_statistic_achievement_fields_get(f->root)->c == NULL);
    CHECK(mc_statistic_achievement_fields_get(f->child)->c == f->root);
    CHECK(mc_statistic_achievement_fields_get(f->next)->c == f->child);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->app.profile, &trig,
                                          &f->app.world_classes, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(f->app.world_classes, f->app.registry, NULL) ==
          MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_bind_entities(f->app.world_classes, f->app.entities) ==
          MC_ENTITY_OK);
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    uint8_t blocks[256] = {0};
    mc_registry_block_view bv;
    CHECK(mc_registry_classes_inspect_blocks(f->app.registry, &bv, NULL) == MC_REGISTRY_OK);
    for (size_t i = 1; i < 256; ++i)
        blocks[i] = bv.m[i] == NULL ? 0 : (uint8_t)i;
    const mc_world_store_options so = {.session_token = 6381,
                                       .block_map = blocks,
                                       .block_map_length = 256,
                                       .limits = mc_world_store_default_limits()};
    CHECK(mc_world_store_open(directory, f->regions, &so, &f->store, NULL) == MC_WORLD_STORAGE_OK);
    static const uint16_t world_name[] = {0};
    CHECK(mc_world_info_name_copy((mc_nbt_string){world_name, 0}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_world_provider_create(0, NULL, &f->dimension, NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = MC_PLAYER_STATISTICS_MAX_PROVIDER_CHUNKS;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA,
                                          f->app.world_classes, &limits, &f->world,
                                          NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments args = {.store = f->store,
                                                   .name = f->name,
                                                   .seed = MC_PLAYER_STATISTICS_WORLD_SEED,
                                                   .dimension = f->dimension};
    CHECK(mc_world_runtime_construct_world(f->world, &args, NULL) == MC_WORLD_STORAGE_OK);
    /* Original cohort loads the four canonical Chunks BEFORE DC construction. */
    for (size_t i = 0; i < RD_CHUNKS; ++i) {
        CHECK(mc_world_runtime_get(f->world, (int32_t)(i / 2u), (int32_t)(i % 2u), &chunks[i]) ==
                  MC_CHUNK_ACQUIRE_OK &&
              chunks[i] != NULL);
    }
    CHECK(mc_local_player_allocate(f->app.entities, &f->player, NULL) == MC_ENTITY_OK);
    CHECK(mc_local_player_construct(f->player, f->world, &f->app, f->app.session, 0, NULL, NULL) ==
          MC_PLAYER_OK);
    CHECK(f->player->phase == MC_ENTITY_READY &&
          f->player->local_player->fields.minecraft == &f->app);
    return true;
}
