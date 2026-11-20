/* Narrow copy of the existing statistics teardown; terminal Chunk ownership only. */
static bool rd_use_teardown(fixture *f, mc_chunk_state **chunks) {
    mc_entity_release(f->player);
    f->player = NULL;
    CHECK(mc_client_application_retire_statistics(&f->app, NULL) == MC_CLIENT_STATISTICS_OWNER_OK);
    CHECK(mc_client_application_retire_water_effect(&f->app, NULL) ==
          MC_CLIENT_STATISTICS_OWNER_OK);
    CHECK(f->app.water_effect == NULL && f->app.water_effect_retained == NULL);
    CHECK(f->app.stats_writer == NULL && f->app.stats_writer_retained == NULL &&
          f->app.session == NULL && f->app.session_classes == NULL);
    CHECK(rd_terminal_chunks(f->world, f->store, f->dimension, chunks));
    CHECK(rd_terminal_world_free(f->world));
    mc_world_provider_release(f->dimension);
    CHECK(mc_world_store_free_checked(&f->store, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_regions_free(f->regions);
    mc_world_info_name_release(f->name);
    CHECK(mc_world_runtime_classes_free(f->app.world_classes, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_client_achievement_notice_free(&f->app.achievement_notice, NULL) ==
          MC_CLIENT_NOTICE_OK);
    CHECK(mc_client_render_classes_free(f->app.render_classes) == MC_CLIENT_MODEL_OK);
    CHECK(mc_registry_classes_free(f->app.registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_entity_classes_free(f->app.entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(f->app.profile);
    mc_materials_destroy(f->app.materials);
    mc_resource_files_free(f->app.files);
    mc_client_game_directory_free(f->app.diagnostic_game_directory);
    return true;
}
