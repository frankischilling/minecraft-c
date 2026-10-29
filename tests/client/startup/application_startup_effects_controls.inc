/* Same real whole-App/Session/imported resources/cold sound backend as the
 * normal session profile. Bounds stop actual callees; no synthetic child or
 * READY receiver replaces earlier startup. These window/home-writing profiles
 * require separate platform admission and are not default CPU tests. */
static int effects_failure_window(const char *resources, const char *assets,
                                  bool constructor_limit) {
    fixture f;
    startup_fixture startup;
    CHECK(fixture_open(&f, resources) == 0);
    CHECK(startup_services(&startup) == 0);
    CHECK(startup_icons(&startup, assets) == 0);
    client_application *app = NULL;
    CHECK(actual_app(&f, false, &app) == 0);
    CHECK(statistics_session(app) == 0);
    mc_client_application_startup *attempt = NULL;
    mc_client_startup_options options = mc_client_application_startup_default_options();
    options.hidden = true;
    if (constructor_limit)
        options.texture_effects.max_array_bytes = 1024;
    else
        options.maximum_effect_registrations = 3;
    CHECK(mc_client_application_startup_allocate(app, &startup.services, &options, &attempt,
                                                 startup.error) == MC_CLIENT_STARTUP_OK);
    CHECK(mc_client_application_run_prefix(attempt, startup.error) ==
          MC_CLIENT_STARTUP_EFFECTS_FAILED);
    mc_client_startup_effects_call *call = attempt->effects;
    CHECK(call != NULL && !call->active && call->S_stored && call->sound_returned &&
          call->sound_settings_borrow && call->sound_hook_attached &&
          call->last_pc == (constructor_limit ? 510u : 536u) &&
          call->registrations_completed == (constructor_limit ? 2u : 3u) &&
          app->S == call->cx_candidate && attempt->last_startup_store == 477 &&
          startup.error->effects == call && app->g == NULL);
    const size_t failed_index = constructor_limit ? 2u : 3u;
    const mc_client_startup_effect_call *failed = &call->effects[failed_index];
    CHECK(failed->engine == app->render_engine && failed->owned != NULL && failed->allocated &&
          failed->argument == failed->owned && failed->snapshot_valid);
    mc_texture_effect_state state;
    CHECK(mc_client_texture_effect_inspect(failed->owned, &state));
    if (constructor_limit) {
        CHECK(startup.error->caller.pc == 521 && startup.error->child.pc == 21 &&
              strcmp(startup.error->child.class_name, "hs") == 0 &&
              failed->construction.status == MC_TEXTURE_EFFECT_LIMIT && !failed->constructed &&
              !failed->registration_entered && state.phase == MC_TEXTURE_EFFECT_FAILED &&
              state.fields.a_length == 1024 && state.array_bytes == 1024 &&
              state.last_store_pc == 12 && state.frame_count == 0 && state.frames == NULL &&
              atomic_load(&app->effect_borrows) == 0);
    } else {
        CHECK(startup.error->caller.pc == 539 && startup.error->child.pc == 5 &&
              strcmp(startup.error->child.class_name, "ji") == 0 && failed->constructed &&
              failed->registration_entered && !failed->registration.appended &&
              !failed->registration.child_returned &&
              failed->registration.status == MC_TEXTURE_EFFECT_LIMIT &&
              state.phase == MC_TEXTURE_EFFECT_READY && state.app == app && state.fields.f == 1 &&
              state.icon != NULL && atomic_load(&app->effect_borrows) == 1);
    }
    for (size_t index = 0; index < call->registrations_completed; ++index) {
        const mc_client_texture_effect *registered = NULL;
        CHECK(call->effects[index].registration.appended &&
              call->effects[index].registration.source_returned &&
              mc_client_render_engine_effect_at(app->render_engine, index, &registered) &&
              registered == call->effects[index].argument);
    }
    for (size_t index = failed_index + 1; index < MC_CLIENT_STARTUP_EFFECT_CALLS; ++index)
        CHECK(!call->effects[index].receiver_captured && call->effects[index].engine == NULL &&
              call->effects[index].owned == NULL && !call->effects[index].registration_entered);
    const mc_client_render_manager_classes *manager_identity = attempt->manager_classes;
    mc_client_render_manager_classes *manager = attempt->manager_classes;
    CHECK(mc_client_application_startup_retire(&attempt, startup.error) == MC_CLIENT_STARTUP_BUSY);
    CHECK(attempt != NULL && call->retired && app->S == NULL && app->stats_writer == NULL &&
          app->settings != NULL && app->render_engine != NULL && app->display != NULL &&
          atomic_load(&app->effect_borrows) == 0 && call->cx_candidate == NULL &&
          manager == manager_identity);
    /* Native fixture authority owns global F/RA and actual graphics resources,
     * exactly as the pre-existing session profile. No source cleanup replay. */
    CHECK(mc_client_render_manager_retire_hands(manager, &startup.error->manager) ==
          MC_CLIENT_MANAGER_OK);
    CHECK(mc_client_render_manager_classes_retire(&manager, &startup.error->manager) ==
              MC_CLIENT_MANAGER_OK &&
          manager == NULL);
    CHECK(mc_client_application_startup_retire(&attempt, startup.error) == MC_CLIENT_STARTUP_BUSY);
    CHECK(attempt != NULL && app->font_renderer == NULL && app->display != NULL &&
          app->settings != NULL);
    CHECK(mc_client_render_engine_retire_resources(f.graphics, NULL) ==
          MC_CLIENT_RENDER_TEXTURE_OK);
    CHECK(mc_client_application_startup_retire(&attempt, startup.error) == MC_CLIENT_STARTUP_OK &&
          attempt == NULL && app->startup == NULL && app->display == NULL && app->settings == NULL);
    CHECK(statistics_end_app(&f, &startup, &app) == 0);
    return 0;
}
