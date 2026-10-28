/* Real-owner startup tail controls. Prepared private-helper cases identify
 * their earlier endpoint and do not credit an omitted normal caller. */
#include "client/achievements/description_registry.h"
#include "client/startup/application_logo.h"
#include "client/startup/application_startup_graphics.h"

static int statistics_session(client_application *app) {
    static const uint16_t username[] = {'S', 't', 'a', 'r', 't', 'u', 'p',
                                        'P', 'l', 'a', 'y', 'e', 'r'};
    static const uint16_t local_id[] = {'0'};
    mc_player_string *name = NULL, *id = NULL;
    CHECK(mc_player_string_create(username, sizeof(username) / sizeof(*username), &name) ==
          MC_PLAYER_OK);
    CHECK(mc_player_string_create(local_id, 1, &id) == MC_PLAYER_OK);
    mc_client_statistics_owner_error error = {0};
    CHECK(mc_client_application_create_session(app, name, id, &error) ==
          MC_CLIENT_STATISTICS_OWNER_OK);
    mc_player_string_release(id);
    mc_player_string_release(name);
    CHECK(app->session != NULL);
    return 0;
}

static int statistics_end_app(fixture *f, startup_fixture *startup, client_application **app) {
    CHECK((*app)->startup == NULL);
    const bool source_j = atomic_load(&(*app)->J);
    const size_t callbacks = atomic_load(&(*app)->callback_holds);
    CHECK(mc_client_application_constructor_retire(app, f->error) == MC_CLIENT_APP_BUSY);
    CHECK(*app != NULL && (*app)->constructor_phase == MC_CLIENT_APP_RETIRING &&
          (*app)->registry == NULL && !(*app)->source_namespace_borrowed &&
          atomic_load(&(*app)->J) == source_j);
    CHECK(mc_client_application_classes_release(&f->services.application_classes, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    if (callbacks != 0) {
        CHECK(mc_client_application_constructor_retire(app, f->error) == MC_CLIENT_APP_BUSY);
        CHECK(*app != NULL && atomic_load(&(*app)->callback_holds) == callbacks &&
              atomic_load(&(*app)->source_static_holds) == 0);
    }
    /* The real Registry finalizer drops every owned kh, including replaced
     * nodes. The still-allocated App must survive those concrete callbacks. */
    CHECK(mc_registry_classes_free(f->services.registry, NULL) == MC_REGISTRY_OK);
    f->services.registry = NULL;
    mc_client_application_volatile_state storage;
    CHECK(mc_client_application_volatile_inspect(*app, &storage) &&
          storage.native_callback_holds == 0 && storage.native_static_holds == 0);
    CHECK(mc_client_application_constructor_retire(app, f->error) == MC_CLIENT_APP_OK &&
          *app == NULL);
    CHECK(startup_services_close(startup, f) == 0);
    return 0;
}

static int statistics_admission_profiles(const char *resources) {
    for (unsigned retired = 0; retired < 2; ++retired) {
        fixture f;
        startup_fixture startup;
        CHECK(fixture_open(&f, resources) == 0);
        CHECK(startup_services(&startup) == 0);
        client_application *app = NULL;
        CHECK(actual_app(&f, false, &app) == 0);
        mc_client_application_startup *attempt = NULL;
        mc_client_stats_writer *unrelated = NULL;
        if (!retired) {
            /* A genuine allocated RAW owner, never a fabricated READY child.
             * This native retained slot belongs to the external control. */
            CHECK(mc_client_stats_writer_allocate(app->registry, NULL, &app->stats_writer_retained,
                                                  NULL) == MC_CLIENT_STATS_OK);
            unrelated = app->stats_writer_retained;
            CHECK(mc_client_application_startup_allocate(app, &startup.services, NULL, &attempt,
                                                         startup.error) ==
                  MC_CLIENT_STARTUP_INVALID_PHASE);
            CHECK(attempt == NULL && app->startup == NULL &&
                  app->stats_writer_retained == unrelated && app->stats_writer == NULL);
            CHECK(mc_client_stats_writer_retire(&app->stats_writer_retained, NULL) ==
                  MC_CLIENT_STATS_OK);
        }
        CHECK(mc_client_application_startup_allocate(app, &startup.services, NULL, &attempt,
                                                     startup.error) == MC_CLIENT_STARTUP_OK);
        if (retired)
            CHECK(mc_client_application_retire_statistics(app, NULL) ==
                      MC_CLIENT_STATISTICS_OWNER_OK &&
                  app->statistics_retired);
        else {
            CHECK(mc_client_stats_writer_allocate(app->registry, NULL, &app->stats_writer_retained,
                                                  NULL) == MC_CLIENT_STATS_OK);
            unrelated = app->stats_writer_retained;
        }
        CHECK(app->source_namespace->startup_stats_classes == NULL);
        CHECK(application_startup_statistics(attempt) == MC_CLIENT_STARTUP_INVALID_PHASE);
        CHECK(attempt->statistics == NULL && attempt->failure.statistics == NULL &&
              attempt->last_startup_store == 0 && app->stats_writer == NULL &&
              app->source_namespace->startup_stats_classes == NULL);
        if (!retired) {
            CHECK(app->stats_writer_retained == unrelated);
            CHECK(mc_client_application_startup_retire(&attempt, startup.error) ==
                  MC_CLIENT_STARTUP_BUSY);
            CHECK(attempt != NULL && app->stats_writer_retained == unrelated);
            CHECK(mc_client_stats_writer_retire(&app->stats_writer_retained, NULL) ==
                  MC_CLIENT_STATS_OK);
        }
        CHECK(mc_client_application_startup_retire(&attempt, startup.error) ==
                  MC_CLIENT_STARTUP_OK &&
              attempt == NULL);
        if (retired) {
            CHECK(mc_client_application_startup_allocate(app, &startup.services, NULL, &attempt,
                                                         startup.error) ==
                  MC_CLIENT_STARTUP_INVALID_PHASE);
            CHECK(attempt == NULL && app->startup == NULL && app->statistics_retired);
        }
        CHECK(statistics_end_app(&f, &startup, &app) == 0);
    }
    fixture f;
    startup_fixture startup;
    CHECK(fixture_open(&f, resources) == 0);
    CHECK(startup_services(&startup) == 0);
    client_application *app = NULL;
    CHECK(actual_app(&f, false, &app) == 0);
    mc_client_mouse_helper_error mouse = {0};
    /* Explicit foreign RAW native slot for admission only, not C380 or a
     * successfully constructed vy. No Display/native cursor is fabricated. */
    CHECK(mc_client_mouse_helper_allocate(f.graphics, NULL, &app->C, &mouse) ==
          MC_CLIENT_MOUSE_HELPER_OK);
    mc_client_source_vy *foreign = app->C;
    mc_client_application_startup *rejected = NULL;
    CHECK(mc_client_application_startup_allocate(app, &startup.services, NULL, &rejected,
                                                 startup.error) == MC_CLIENT_STARTUP_INVALID_PHASE);
    CHECK(rejected == NULL && app->startup == NULL && app->C == foreign);
    CHECK(mc_client_mouse_helper_release(&app->C, &mouse) == MC_CLIENT_MOUSE_HELPER_OK &&
          app->C == NULL);
    CHECK(statistics_end_app(&f, &startup, &app) == 0);
    return 0;
}

static int statistics_null_z(const char *resources) {
    fixture f;
    startup_fixture startup;
    CHECK(fixture_open(&f, resources) == 0);
    CHECK(startup_services(&startup) == 0);
    client_application *app = NULL;
    CHECK(actual_app(&f, false, &app) == 0);
    CHECK(statistics_session(app) == 0);
    mc_client_application_startup *attempt = NULL;
    CHECK(mc_client_application_startup_allocate(app, &startup.services, NULL, &attempt,
                                                 startup.error) == MC_CLIENT_STARTUP_OK);
    /* Prepared actual MC324 helper: no earlier Display/directory caller is
     * credited. Its real null Z reaches xi.listFiles55 after stats mkdir. */
    CHECK(application_startup_statistics(attempt) == MC_CLIENT_STARTUP_STATISTICS_FAILED);
    const mc_client_startup_stats_call *call = attempt->statistics;
    CHECK(call != NULL && call->entered && !call->I_stored && !call->source_calls_completed &&
          !call->callback_installed && call->published == NULL && call->candidate != NULL &&
          app->stats_writer == NULL && attempt->failure.caller.pc == 337 &&
          attempt->failure.child.pc == 55 && strcmp(attempt->failure.child.class_name, "xi") == 0 &&
          attempt->last_startup_store == 0 && app->game_directory == NULL &&
          app->source_namespace->startup_stats_classes == call->classes);
    mc_client_stats_state state;
    mc_client_stats_file_state files;
    mc_client_stats_classes_state classes;
    CHECK(mc_client_stats_writer_read_state(call->candidate, &state) &&
          state.phase == MC_CLIENT_STATS_FAILED_CONSTRUCTION && state.workers_started == 0 &&
          state.workers_joined == 0);
    CHECK(mc_client_stats_writer_read_file_state(call->candidate, &files) && files.parent == NULL &&
          files.directory != NULL && files.listing == NULL);
    CHECK(mc_client_stats_classes_read_state(call->classes, &classes) && classes.resolved);
    mc_java_file_state directory;
    CHECK(mc_java_file_read_state((mc_java_file *)files.directory, &directory, NULL) ==
              MC_JAVA_FILE_OK &&
          directory.path == classes.stats_literal);
    const mc_client_stats_file_error snapshot = call->failure.source_writer;
    CHECK(snapshot.base.bytecode == 55 &&
          snapshot.base.exception == MC_CLIENT_STATS_NULL_REFERENCE);
    CHECK(mc_client_application_startup_retire(&attempt, startup.error) == MC_CLIENT_STARTUP_OK &&
          attempt == NULL && app->stats_writer_retained == NULL && app->session == NULL);
    CHECK(statistics_end_app(&f, &startup, &app) == 0);
    CHECK(snapshot.base.bytecode == 55 && strcmp(snapshot.base.class_name, "xi") == 0 &&
          snapshot.base.operation != NULL && strlen(snapshot.base.operation) != 0);
    return 0;
}

static int statistics_session_window(const char *resources, const char *assets, bool foreign,
                                     bool gl_error) {
    fixture f;
    startup_fixture startup;
    CHECK(fixture_open(&f, resources) == 0);
    CHECK(startup_services(&startup) == 0);
    CHECK(startup_icons(&startup, assets) == 0);
    client_application *app = NULL;
    CHECK(actual_app(&f, false, &app) == 0);
    CHECK(statistics_session(app) == 0);
    mc_player_session *session = app->session;
    mc_client_application_startup *attempt = NULL;
    mc_client_startup_options options = mc_client_application_startup_default_options();
    options.hidden = true;
    if (foreign)
        options.render_engine.max_constructor_allocations = 0;
    CHECK(mc_client_application_startup_allocate(app, &startup.services, &options, &attempt,
                                                 startup.error) == MC_CLIENT_STARTUP_OK);
    CHECK(app->source_namespace->startup_stats_classes == NULL);
#ifdef MC_STARTUP_CONTROLLER_BOUNDARY
    if (gl_error)
        CHECK(mc_test_controller_boundary_arm());
#else
    CHECK(!gl_error); /* This mode requires the separately mapped real-body observer target. */
#endif
    const mc_client_startup_status result =
        mc_client_application_run_prefix(attempt, startup.error);
    if (foreign) {
        CHECK(result == MC_CLIENT_STARTUP_RENDER_ENGINE_FAILED && startup.error->caller.pc == 235 &&
              attempt->statistics == NULL && app->game_directory != NULL);
        mc_client_stats_classes *external_classes = NULL;
        CHECK(mc_client_stats_classes_create(0, &external_classes, NULL) == MC_CLIENT_STATS_OK);
        /* The canonical App API completes a genuine writer and KH using its
         * real Z/Session. This is a foreign prepared owner, not source ordering
         * credit across the failed Engine constructor. */
        CHECK(mc_client_application_construct_statistics_file(
                  app, external_classes, startup.services.files, NULL, NULL, NULL) ==
              MC_CLIENT_STATISTICS_OWNER_OK);
        mc_client_stats_writer *writer = app->stats_writer;
        CHECK(writer != NULL && app->stats_writer_retained == writer);
        CHECK(application_startup_statistics(attempt) == MC_CLIENT_STARTUP_INVALID_PHASE &&
              attempt->statistics == NULL && app->source_namespace->startup_stats_classes == NULL);
        CHECK(mc_client_application_startup_retire(&attempt, startup.error) ==
              MC_CLIENT_STARTUP_BUSY);
        CHECK(app->stats_writer == writer && app->stats_writer_retained == writer &&
              app->settings != NULL && app->display != NULL);
        CHECK(mc_client_application_retire_statistics(app, NULL) == MC_CLIENT_STATISTICS_OWNER_OK);
        CHECK(mc_client_stats_classes_retire(&external_classes, NULL) == MC_CLIENT_STATS_OK &&
              external_classes == NULL);
        CHECK(mc_client_application_startup_retire(&attempt, startup.error) ==
                  MC_CLIENT_STARTUP_OK &&
              attempt == NULL);
    } else {
        mc_client_sdk_controllers_state controllers = {0};
        CHECK(mc_client_sdk_controllers_inspect(mc_client_display_inputs(app->display),
                                                &controllers));
        if (controllers.native_count != 0) {
            CHECK(result == MC_CLIENT_STARTUP_INPUT_FAILED && startup.error->caller.pc == 383 &&
                  attempt->input != NULL && !attempt->input->controllers_call_completed &&
                  attempt->input->input.status == MC_CLIENT_SDK_INPUT_UNSUPPORTED_PROFILE &&
                  attempt->input->input.site.pc == 12 && !controllers.created &&
                  attempt->graphics == NULL && app->source_namespace->system_output == NULL);
            fprintf(stderr,
                    "Genuine nonempty Controllers native count%d: SDK Native12; "
                    "normal470 not reached; no Source391 completion\n",
                    controllers.native_count);
        }
        CHECK(controllers.native_count == 0); /* Never replace physical discovery with empty. */
#ifdef MC_STARTUP_CONTROLLER_BOUNDARY
        if (gl_error) {
            mc_test_controller_boundary_state observed = {0};
            CHECK(mc_test_controller_boundary_inspect(&observed));
            mc_test_controller_boundary_disarm();
            if (!observed.injected)
                fprintf(stderr, "Controller boundary injection failure: %s\n",
                        observed.lookup_error);
            CHECK(observed.create_calls == 1 && observed.invalid_enum_calls == 1 &&
                  observed.create_status == MC_CLIENT_SDK_INPUT_OK && observed.injected &&
                  observed.injection_attempted && !observed.missing_gl_function &&
                  observed.input == mc_client_display_inputs(app->display) &&
                  observed.display == app->display);
        }
#endif
        CHECK(result == MC_CLIENT_STARTUP_UNIMPLEMENTED && startup.error->caller.pc == 470 &&
              startup.error->child.pc == 470 && attempt->last_startup_store == 380 &&
              attempt->last_manager_pc == 321);
        const mc_client_startup_stats_call *call = attempt->statistics;
        CHECK(call != NULL && call->source_calls_completed && call->callback_installed &&
              call->I_stored && call->last_pc == 354 && call->candidate == app->stats_writer &&
              call->published == app->stats_writer && app->stats_writer != NULL);
        mc_client_stats_state writer;
        CHECK(mc_client_stats_writer_read_state(app->stats_writer, &writer) &&
              writer.phase == MC_CLIENT_STATS_READY && writer.writer_pc == 185 &&
              writer.synchronizer_pc == 558 && writer.workers_started == 1 &&
              writer.workers_joined == 0 && writer.session == session);
        mc_client_stats_file_state unavailable;
        CHECK(!mc_client_stats_writer_read_file_state(app->stats_writer, &unavailable));
        mc_client_stats_classes_state classes;
        CHECK(mc_client_stats_classes_read_state(call->classes, &classes) && classes.resolved &&
              call->classes == app->source_namespace->startup_stats_classes);
        CHECK(attempt->logo != NULL && attempt->logo->source_returned &&
              attempt->logo->background_drawn && attempt->logo->logo_drawn &&
              attempt->logo->swapped && attempt->logo->pc == 270 &&
              attempt->logo->literal == app->source_namespace->startup_logo_resource);
        CHECK(attempt->input != NULL && attempt->input->keyboard_call_completed &&
              attempt->input->mouse_call_completed && attempt->input->C_stored &&
              attempt->input->controllers_call_completed && attempt->input->last_pc == 383 &&
              app->C == attempt->input->candidate && app->C != NULL);
        mc_client_mouse_helper_state mouse;
        CHECK(mc_client_mouse_helper_inspect(app->C, &mouse) &&
              mouse.phase == MC_CLIENT_MOUSE_HELPER_READY && mouse.last_store_pc == 57 &&
              mouse.source_c == app->m && mouse.source_d != NULL);
        mc_client_sdk_input_state input;
        CHECK(mc_client_sdk_input_inspect(mc_client_display_inputs(app->display), &input) &&
              input.keyboard_created && input.mouse_created && input.controllers_created);
        CHECK(controllers.created && controllers.last_create_pc == 143 &&
              controllers.init_calls == 1 && controllers.enumeration_calls == 1 &&
              controllers.controllers_count == 0 && controllers.events_count == 0 &&
              controllers.controller_count == 0 && controllers.event_identity == NULL &&
              controllers.controllers_identity != controllers.events_identity &&
              controllers.environment_identity != NULL && !controllers.create_attempt_retained);
        const mc_client_startup_graphics_call *graphics = attempt->graphics;
        CHECK(graphics != NULL && graphics->entered && graphics->setup_completed &&
              !graphics->active && !graphics->retired && graphics->setup_calls_completed == 11 &&
              graphics->diagnostic_calls_completed == 2 && graphics->last_pc == 466 &&
              graphics->namespace_identity == app->source_namespace->system_output &&
              app->source_namespace->system_output_attempted &&
              graphics->labels[0] == app->source_namespace->startup_gl_labels[0] &&
              graphics->labels[1] == app->source_namespace->startup_gl_labels[1] &&
              graphics->labels[0] != NULL && graphics->labels[1] != NULL &&
              graphics->labels[0] != graphics->labels[1]);
        mc_java_system_output_state system = {0};
        CHECK(mc_java_system_output_inspect(graphics->namespace_identity, &system) &&
              system.phase == MC_JAVA_PRINT_READY && system.out != NULL);
        for (size_t index = 0; index < 2; ++index) {
            mc_client_gl_output_state diagnostic = {0};
            CHECK(mc_client_gl_output_inspect(graphics->diagnostics[index], &diagnostic) &&
                  diagnostic.started && !diagnostic.active && !diagnostic.retiring &&
                  diagnostic.error->status == MC_CLIENT_GL_OUTPUT_OK &&
                  diagnostic.error->source_returned && diagnostic.error->site.pc == 75 &&
                  diagnostic.error->prefix.query_completed && graphics->snapshots[index].valid);
            const bool printed = gl_error && index == 0;
            CHECK(diagnostic.error->prefix.gl_error == (printed ? GL_INVALID_ENUM : GL_NO_ERROR) &&
                  diagnostic.captured_receivers == (printed ? 3u : 0u) &&
                  diagnostic.normal_println_returns == (printed ? 3u : 0u));
            if (printed) {
#ifdef _WIN32
                static const char printed_bytes[] = "########## GL ERROR ##########\r\n"
                                                    "@ Pre startup\r\n"
                                                    "1280: Invalid enum\r\n";
#else
                static const char printed_bytes[] = "########## GL ERROR ##########\n"
                                                    "@ Pre startup\n"
                                                    "1280: Invalid enum\n";
#endif
                for (size_t receiver = 0; receiver < 3; ++receiver)
                    CHECK(diagnostic.receivers[receiver] == system.out &&
                          graphics->snapshots[index].receiver_valid[receiver] &&
                          graphics->snapshots[index].receivers[receiver].println_returns == 3 &&
                          graphics->snapshots[index].receivers[receiver].physical_bytes_known &&
                          graphics->snapshots[index].receivers[receiver].physical_bytes ==
                              sizeof(printed_bytes) - 1 &&
                          !graphics->snapshots[index].receivers[receiver].trouble);
                CHECK(diagnostic.results[0] != NULL && diagnostic.results[1] != NULL);
            }
        }
        uint8_t diagnostic_snapshots[sizeof(graphics->snapshots)];
        memcpy(diagnostic_snapshots, graphics->snapshots, sizeof(diagnostic_snapshots));
        mc_client_events_state events;
        CHECK(mc_client_events_inspect(mc_client_display_event_port(app->display), &events) &&
              events.pump_generation == 0 && atomic_load(&app->J) && !atomic_load(&app->o));
        mc_registry_achievement_view achievements;
        CHECK(mc_registry_classes_inspect_achievements(app->registry, &achievements, NULL) ==
              MC_REGISTRY_OK);
        const mc_achievement_description_callback *callback =
            mc_statistic_achievement_fields_get(achievements.named[0])->client_callback;
        client_application *captured = NULL;
        CHECK(mc_client_inventory_callback_client(app->registry, callback, &captured, NULL) ==
                  MC_REGISTRY_OK &&
              captured == app && atomic_load(&app->callback_holds) == 1);
        CHECK(mc_client_application_run_prefix(attempt, startup.error) ==
              MC_CLIENT_STARTUP_INVALID_PHASE);
        mc_client_bitmap_font *external_font = app->font_renderer;
        CHECK(mc_client_bitmap_font_retain(external_font) == MC_CLIENT_BITMAP_FONT_OK);
        mc_client_render_manager_classes *manager = attempt->manager_classes;
        CHECK(mc_client_application_startup_retire(&attempt, startup.error) ==
              MC_CLIENT_STARTUP_BUSY);
        CHECK(attempt != NULL && attempt->statistics == call && call->retired &&
              call->candidate == NULL && call->published == NULL && app->stats_writer == NULL &&
              app->stats_writer_retained == NULL && app->session == NULL &&
              app->statistics_retired && app->font_renderer == external_font &&
              app->display != NULL && app->C == NULL && attempt->input->retired && call->I_stored &&
              call->callback_installed);
        CHECK(attempt->graphics == graphics && graphics->retired &&
              graphics->diagnostics[0] == NULL && graphics->diagnostics[1] == NULL &&
              graphics->namespace_identity == NULL && graphics->labels[0] == NULL &&
              graphics->labels[1] == NULL &&
              memcmp(graphics->snapshots, diagnostic_snapshots, sizeof(diagnostic_snapshots)) ==
                  0 &&
              app->source_namespace->system_output != NULL &&
              app->source_namespace->startup_gl_labels[0] != NULL &&
              app->source_namespace->startup_gl_labels[1] != NULL);
        CHECK(mc_client_render_manager_retire_hands(manager, &startup.error->manager) ==
              MC_CLIENT_MANAGER_OK);
        CHECK(mc_client_render_manager_classes_retire(&manager, &startup.error->manager) ==
                  MC_CLIENT_MANAGER_OK &&
              manager == NULL);
        CHECK(mc_client_bitmap_font_release(&external_font, NULL) == MC_CLIENT_BITMAP_FONT_OK &&
              external_font == NULL);
        CHECK(mc_client_application_startup_retire(&attempt, startup.error) ==
              MC_CLIENT_STARTUP_BUSY);
        CHECK(attempt != NULL && attempt->statistics == call && call->retired &&
              call->failure.status == MC_CLIENT_STATISTICS_OWNER_OK && app->display != NULL &&
              app->font_renderer == NULL && app->settings != NULL);
        CHECK(mc_client_render_engine_retire_resources(f.graphics, NULL) ==
              MC_CLIENT_RENDER_TEXTURE_OK);
        CHECK(mc_client_application_startup_retire(&attempt, startup.error) ==
                  MC_CLIENT_STARTUP_OK &&
              attempt == NULL);
        CHECK(app->C == NULL && app->stats_writer == NULL && app->session == NULL &&
              atomic_load(&app->callback_holds) == 1 && atomic_load(&app->J));
    }
    CHECK(statistics_end_app(&f, &startup, &app) == 0);
    return 0;
}
