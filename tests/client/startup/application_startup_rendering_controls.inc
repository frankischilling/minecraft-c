/* Genuine whole-App startup through all nine effect registrations, followed
 * by a declared real constructor allocation bound. These window/home-writing
 * profiles require the same explicit platform admission as window-session. */
static int rendering_failure_window(const char *resources, const char *assets, bool global_limit) {
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
    if (global_limit)
        options.render_global.max_constructor_allocations = 0;
    else
        options.particles.max_allocation_bytes =
            sizeof(mc_client_effect_renderer) + 4 * sizeof(mc_effect_particle_list *);
    CHECK(mc_client_application_startup_allocate(app, &startup.services, &options, &attempt,
                                                 startup.error) == MC_CLIENT_STARTUP_OK);
    CHECK(mc_client_application_run_prefix(attempt, startup.error) ==
          (global_limit ? MC_CLIENT_STARTUP_RENDER_GLOBAL_FAILED
                        : MC_CLIENT_STARTUP_PARTICLES_FAILED));
    mc_client_startup_rendering_call *call = attempt->rendering;
    CHECK(call != NULL && !call->active && call->entered && !call->retired &&
          call->global_allocated && call->global != NULL &&
          call->global_engine == app->render_engine && !call->calls_completed && !call->j_stored &&
          app->effect_renderer == NULL && startup.error->rendering == call &&
          attempt->effects->registrations_completed == 9 && app->S != NULL &&
          app->settings != NULL && app->display != NULL && atomic_load(&app->effect_borrows) == 2 &&
          !app->effect_renderer_constructing);
    if (global_limit) {
        mc_client_render_global_state state;
        CHECK(mc_client_render_global_inspect(call->global, &state));
        CHECK(startup.error->caller.pc == 625 && startup.error->child.pc == 5 &&
              strcmp(startup.error->child.class_name, "n") == 0 && call->failed_pc == 625 &&
              call->global_error.status == MC_CLIENT_RENDER_GLOBAL_LIMIT &&
              state.phase == MC_CLIENT_RENDER_GLOBAL_FAILED && !call->global_constructor_returned &&
              !call->g_stored && !call->viewport_returned && !call->particle_attempted &&
              !call->particle_allocated && call->global_last_store == 0 &&
              call->global_constructor_pc == 5 && app->g == NULL &&
              app->effect_renderer_retained == NULL && attempt->last_startup_store == 477);
    } else {
        mc_effect_renderer_snapshot state;
        CHECK(mc_client_effect_renderer_read(call->particle, &state));
        CHECK(startup.error->caller.pc == 657 && startup.error->child.pc == 13 &&
              strcmp(startup.error->child.class_name, "dn") == 0 && call->failed_pc == 657 &&
              call->particle_error.status == MC_EFFECT_RENDERER_LIMIT &&
              call->global_constructor_returned && call->g_stored && call->viewport_returned &&
              app->g == call->global && call->global_constructor_pc == 651 &&
              call->particle_allocated && call->particle == app->effect_renderer_retained &&
              !call->particle_constructor_returned && call->particle_constructor_pc == 9 &&
              call->particle_engine == app->render_engine && !call->world_held &&
              call->particle_world == NULL && attempt->last_startup_store == 628 &&
              state.phase == MC_EFFECT_RENDERER_FAILED_CONSTRUCTION && state.layers != NULL &&
              state.random == NULL && state.world == NULL && state.render_engine == NULL);
        for (size_t index = 0; index < 4; ++index)
            CHECK(state.lists[index] == NULL);
    }
    mc_client_render_manager_classes *manager = attempt->manager_classes;
    CHECK(mc_client_application_startup_retire(&attempt, startup.error) == MC_CLIENT_STARTUP_BUSY);
    CHECK(attempt != NULL && call->retired && call->global == NULL && call->particle == NULL &&
          call->global_engine == NULL && call->particle_engine == NULL &&
          call->particle_world == NULL && !call->world_held && app->g == NULL &&
          app->effect_renderer == NULL && app->effect_renderer_retained == NULL &&
          atomic_load(&app->effect_borrows) == 0 && app->S == NULL && app->settings != NULL &&
          app->render_engine != NULL && app->display != NULL);
    CHECK(mc_client_render_manager_retire_hands(manager, &startup.error->manager) ==
          MC_CLIENT_MANAGER_OK);
    CHECK(mc_client_render_manager_classes_retire(&manager, &startup.error->manager) ==
              MC_CLIENT_MANAGER_OK &&
          manager == NULL);
    CHECK(mc_client_application_startup_retire(&attempt, startup.error) == MC_CLIENT_STARTUP_BUSY);
    CHECK(mc_client_render_engine_retire_resources(f.graphics, NULL) ==
          MC_CLIENT_RENDER_TEXTURE_OK);
    CHECK(mc_client_application_startup_retire(&attempt, startup.error) == MC_CLIENT_STARTUP_OK &&
          attempt == NULL && app->startup == NULL);
    CHECK(statistics_end_app(&f, &startup, &app) == 0);
    return 0;
}
