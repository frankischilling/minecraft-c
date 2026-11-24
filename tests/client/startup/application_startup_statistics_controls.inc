/* Real-owner startup tail controls. Prepared private-helper cases identify
 * their earlier endpoint and do not credit an omitted normal caller. */
#include "client/achievements/description_registry.h"
#include "client/startup/application_logo.h"
#include "client/startup/application_startup_graphics.h"
#include "client/startup/application_startup_gui_ingame.h"
#include "client/startup/application_startup_menu.h"
#include "client/startup/application_startup_resources.h"

/* Native worker cancellation never fabricates a source cz.b() call. The
 * original network endpoint may be unavailable; bounded native retirement
 * still joins before any App sound/File dependency is consumed. */
static int statistics_retire_resources(mc_client_application_startup *attempt) {
    for (unsigned int tries = 0; tries < 35000; ++tries) {
        const mc_client_startup_status status = application_startup_resources_retire(attempt);
        if (status == MC_CLIENT_STARTUP_OK)
            return 0;
        CHECK(status == MC_CLIENT_STARTUP_BUSY);
        const struct timespec delay = {0, 1000000};
        (void)thrd_sleep(&delay, NULL);
    }
    CHECK(false);
    return 1;
}

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
    mc_client_application_startup *before_rendering = NULL;
    CHECK(mc_client_application_startup_allocate(app, &startup.services, NULL, &before_rendering,
                                                 startup.error) == MC_CLIENT_STARTUP_OK);
    /* A real external RAW dn is a native retained owner, never a completed
     * source j. Retirement cannot consume it or its earlier dependencies. */
    CHECK(mc_client_effect_renderer_allocate(app->entities, NULL, &app->effect_renderer_retained,
                                             NULL) == MC_EFFECT_RENDERER_OK);
    mc_client_effect_renderer *raw_particles = app->effect_renderer_retained;
    CHECK(mc_client_application_startup_retire(&before_rendering, startup.error) ==
          MC_CLIENT_STARTUP_BUSY);
    CHECK(before_rendering != NULL && before_rendering->rendering == NULL &&
          app->effect_renderer_retained == raw_particles && app->effect_renderer == NULL &&
          app->startup == before_rendering && app->settings == NULL && app->display == NULL);
    CHECK(mc_client_effect_renderer_free(&app->effect_renderer_retained) == MC_EFFECT_RENDERER_OK &&
          app->effect_renderer_retained == NULL);
    raw_particles = NULL;
    CHECK(mc_client_application_startup_retire(&before_rendering, startup.error) ==
              MC_CLIENT_STARTUP_OK &&
          before_rendering == NULL && app->startup == NULL);
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
    CHECK(app->source_namespace->startup_cx_classes == NULL &&
          !app->source_namespace->startup_cx_attempted);
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
                    "normal_run32 not reached; no Source391 completion\n",
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
        if (result != MC_CLIENT_STARTUP_UNIMPLEMENTED || startup.error->caller.pc != 32 ||
            startup.error->child.pc != 32) {
            fprintf(stderr,
                    "Unexpected startup result=%d operation=%s caller=%s.%s:%u "
                    "child=%s.%s:%u last_store=%d manager_pc=%d\n",
                    (int)result,
                    startup.error->operation != NULL ? startup.error->operation : "<null>",
                    startup.error->caller.class_name != NULL ? startup.error->caller.class_name
                                                             : "<null>",
                    startup.error->caller.method != NULL ? startup.error->caller.method : "<null>",
                    startup.error->caller.pc,
                    startup.error->child.class_name != NULL ? startup.error->child.class_name
                                                            : "<null>",
                    startup.error->child.method != NULL ? startup.error->child.method : "<null>",
                    startup.error->child.pc, (int)attempt->last_startup_store,
                    (int)attempt->last_manager_pc);
            const mc_client_startup_effects_call *observed = attempt->effects;
            if (observed != NULL) {
                fprintf(stderr,
                        "Effects cx_status=%d cx_operation=%s sound_status=%d sound_operation=%s "
                        "sound_error_pc=%d sound_report_pc=%d setup_pc=%d native_terminal=%d "
                        "settings_status=%d settings_operation=%s backend_status=%d "
                        "backend_operation=%s backend_system_error=%d registrations=%u "
                        "last_pc=%u\n",
                        (int)observed->cx.status,
                        observed->cx.operation != NULL ? observed->cx.operation : "<null>",
                        (int)observed->sound_error.status,
                        observed->sound_error.operation != NULL ? observed->sound_error.operation
                                                                : "<null>",
                        observed->sound_error.source_pc, observed->sound_report.source_pc,
                        observed->sound_report.setup.source_pc,
                        (int)observed->sound_report.setup.native_terminal,
                        (int)observed->sound_report.setup.settings_status,
                        observed->sound_report.setup.settings_error.operation != NULL
                            ? observed->sound_report.setup.settings_error.operation
                            : "<null>",
                        (int)observed->sound_report.setup.backend_error.status,
                        observed->sound_report.setup.backend_error.operation != NULL
                            ? observed->sound_report.setup.backend_error.operation
                            : "<null>",
                        observed->sound_report.setup.backend_error.system_error,
                        observed->registrations_completed, observed->last_pc);
                if (observed->registrations_completed < MC_CLIENT_STARTUP_EFFECT_CALLS) {
                    const unsigned int index = observed->registrations_completed;
                    const mc_client_startup_effect_call *entry = &observed->effects[index];
                    fprintf(stderr,
                            "Pending/failed effect index=%u receiver=%d allocated=%d "
                            "constructed=%d registration_entered=%d construction_status=%d "
                            "construction_operation=%s construction_pc=%d "
                            "registration_status=%d registration_operation=%s ji_pc=%d "
                            "child_status=%d child_operation=%s child_pc=%d\n",
                            index, (int)entry->receiver_captured, (int)entry->allocated,
                            (int)entry->constructed, (int)entry->registration_entered,
                            (int)entry->construction.status,
                            entry->construction.operation != NULL ? entry->construction.operation
                                                                  : "<null>",
                            entry->construction.source_pc, (int)entry->registration.status,
                            entry->registration.operation != NULL ? entry->registration.operation
                                                                  : "<null>",
                            entry->registration.ji_pc, (int)entry->registration.child.status,
                            entry->registration.child.operation != NULL
                                ? entry->registration.child.operation
                                : "<null>",
                            entry->registration.child.source_pc);
                }
            }
        }
        CHECK(result == MC_CLIENT_STARTUP_UNIMPLEMENTED && startup.error->caller.pc == 32 &&
              startup.error->child.pc == 32 && attempt->last_startup_store == 705 &&
              attempt->last_manager_pc == 321);
        const mc_client_startup_rendering_call *rendering = attempt->rendering;
        CHECK(rendering != NULL && rendering->entered && !rendering->active &&
              !rendering->retired && rendering->calls_completed && rendering->g_stored &&
              rendering->j_stored && rendering->global_allocated &&
              rendering->global_constructor_returned && rendering->global_constructor_pc == 651 &&
              rendering->global_last_store == 489 && rendering->viewport_returned &&
              rendering->viewport_width == app->d && rendering->viewport_height == app->e &&
              rendering->particle_constructor_returned &&
              rendering->particle_constructor_pc == 63 && rendering->last_pc == 660 &&
              rendering->failed_pc == 0 && app->g == rendering->global &&
              app->effect_renderer == rendering->particle &&
              app->effect_renderer_retained == rendering->particle &&
              rendering->global_engine == app->render_engine &&
              rendering->particle_engine == app->render_engine &&
              rendering->particle_world == NULL && !rendering->world_held);
        mc_effect_renderer_snapshot particles;
        CHECK(mc_client_effect_renderer_read(app->effect_renderer, &particles) &&
              particles.phase == MC_EFFECT_RENDERER_READY && particles.world == NULL &&
              particles.render_engine == app->render_engine && particles.random != NULL);
        for (size_t layer = 0; layer < 4; ++layer) {
            CHECK(particles.lists[layer] != NULL && particles.sizes[layer] == 0);
            for (size_t prior = 0; prior < layer; ++prior)
                CHECK(particles.lists[layer] != particles.lists[prior]);
        }
        const mc_client_startup_effects_call *effects = attempt->effects;
        CHECK(effects != NULL && effects->entered && !effects->active && !effects->retired &&
              effects->S_stored && effects->cx_candidate == app->S && app->S != NULL &&
              effects->cx_classes == app->source_namespace->startup_cx_classes &&
              effects->sound_entered && effects->sound_returned && effects->sound_settings_borrow &&
              effects->sound_hook_attached && effects->sound == app->B &&
              effects->sound_settings == app->settings && effects->sound_report.source_pc == 45 &&
              effects->registrations_completed == 9 && effects->last_pc == 612 &&
              effects->lava_trig_bound);
        mc_client_cx_class_state cx_classes = {0};
        mc_client_cx_state cx = {0};
        CHECK(mc_client_cx_classes_inspect(effects->cx_classes, &cx_classes, NULL) ==
                  MC_CLIENT_CX_OK &&
              cx_classes.phase == MC_CLIENT_CX_CLASS_READY && cx_classes.enabled &&
              cx_classes.last_store == 1 && cx_classes.holds == 2 &&
              mc_client_cx_inspect(app->S, &cx, NULL) == MC_CLIENT_CX_OK &&
              cx.phase == MC_CLIENT_CX_READY && cx.constructor_pc == 4);
        mc_sound_manager_state sound;
        CHECK(mc_sound_manager_read_state(app->B, &sound) && sound.settings == app->settings &&
              sound.shared_initialized && sound.backend_present);
        static const unsigned int effect_calls[] = {499, 510, 524, 539, 554, 568, 582, 597, 612};
        for (size_t index = 0; index < MC_CLIENT_STARTUP_EFFECT_CALLS; ++index) {
            const mc_client_startup_effect_call *entry = &effects->effects[index];
            const mc_client_texture_effect *registered = NULL;
            CHECK(entry->engine == app->render_engine && entry->receiver_captured &&
                  entry->registration_entered && entry->invocation_pc == effect_calls[index] &&
                  entry->registration.appended && entry->registration.child_returned &&
                  entry->registration.source_returned && entry->registration.ji_pc == 15 &&
                  entry->snapshot_valid && entry->observed_phase == MC_TEXTURE_EFFECT_READY &&
                  mc_client_render_engine_effect_at(app->render_engine, index, &registered) &&
                  registered == entry->argument);
            CHECK((entry->owned != NULL) == (index >= 2));
            if (index >= 2)
                CHECK(entry->allocated && entry->constructed && entry->owned == entry->argument);
        }
        CHECK(effects->effects[0].argument == mc_client_lava_texture_as_effect(app->ae) &&
              effects->effects[1].argument == mc_client_water_texture_as_effect(app->water_effect));
        mc_client_texture_effect *external_compass = effects->effects[3].owned;
        mc_texture_effect_error *effect_error = calloc(1, sizeof(*effect_error));
        CHECK(effect_error != NULL && mc_client_texture_effect_retain(
                                          external_compass, effect_error) == MC_TEXTURE_EFFECT_OK);
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
        const mc_client_startup_resources_call *resource_call = attempt->resources;
        CHECK(resource_call != NULL && resource_call->constructed && resource_call->started &&
              resource_call->U_stored && resource_call->post_returned && !resource_call->active &&
              app->U == resource_call->candidate &&
              atomic_load(&app->resource_thread_borrows) == 1);
        mc_client_gl_output_state post;
        CHECK(mc_client_gl_output_inspect(resource_call->diagnostic, &post) &&
              post.error->source_returned && post.error->prefix.query_completed);
        const mc_client_startup_gui_ingame_call *ingame = attempt->gui_ingame;
        const mc_client_startup_menu_call *menu_call = attempt->menu;
        CHECK(ingame != NULL && ingame->allocated && ingame->constructor_returned &&
              ingame->v_stored && ingame->last_pc == 705 && app->v == ingame->candidate);
        mc_client_gui_ingame_state ingame_state;
        CHECK(mc_client_gui_ingame_inspect(app->v, &ingame_state, NULL) ==
                  MC_CLIENT_GUI_INGAME_OK &&
              ingame_state.phase == MC_CLIENT_GUI_INGAME_READY && ingame_state.fields.g == app &&
              ingame_state.fields.e != NULL && ingame_state.fields.f != NULL);
        CHECK(menu_call != NULL && menu_call->constructed && menu_call->source_startup_returned &&
              menu_call->last_pc == 749 && menu_call->display != NULL &&
              menu_call->display->source_returned && menu_call->display->site.pc == 168 &&
              menu_call->display->stats_b_returned && menu_call->display->stats_c_returned &&
              menu_call->display->messages_cleared && menu_call->display->r_stored &&
              menu_call->display->focus_released && menu_call->display->resolution_returned &&
              menu_call->display->initialized && menu_call->display->w_stored && !app->w &&
              !app->N);
        mc_client_source_da *current_screen = mc_client_screen_menu_base(menu_call->candidate);
        CHECK(app->r == current_screen && atomic_load(&app->screen_borrows) == 2);
        mc_client_screen_init_state initialized;
        CHECK(mc_client_screen_initialization_inspect(current_screen, 0, &initialized) &&
              initialized.phase == MC_CLIENT_SCREEN_INIT_RETURNED &&
              initialized.button_count == 5 && initialized.app_argument == app &&
              initialized.held_font == app->font_renderer);
        /* The actual bound MC class owns H; empty notice returns before clock/GL. */
        mc_client_notice_error notice_gate = {0};
        CHECK(mc_client_achievement_notice_draw(app->achievement_notice, &notice_gate) ==
                  MC_CLIENT_NOTICE_OK &&
              notice_gate.notice_pc == 107);
        CHECK(mc_client_achievement_notice_fields_get(app->achievement_notice)->f == NULL &&
              mc_client_achievement_notice_fields_get(app->achievement_notice)->g == 0);
        /* A new real displayGuiScreen invocation preserves r106 and h24 when
         * the standalone profile reaches the actual null Component fault. */
        app->N = true;
        app->w = true;
        mc_client_display_screen_call *focus_failure = NULL;
        CHECK(
            mc_client_display_screen_run(app, menu_call->classes, current_screen, &focus_failure) ==
            MC_CLIENT_DISPLAY_SCREEN_NULL_REFERENCE);
        CHECK(focus_failure != NULL && focus_failure->old_closed &&
              focus_failure->stats_b_returned && focus_failure->stats_c_returned &&
              focus_failure->messages_cleared && focus_failure->r_stored &&
              !focus_failure->initialized && !focus_failure->w_stored &&
              focus_failure->site.pc == 114 && focus_failure->mouse.site.pc == 4 &&
              focus_failure->mouse.status == MC_CLIENT_MOUSE_HELPER_NULL_REFERENCE &&
              app->r == current_screen && !app->N && app->w);
        CHECK(mc_client_display_screen_retire(&focus_failure) == MC_CLIENT_DISPLAY_SCREEN_OK &&
              focus_failure == NULL);
        app->w = false; /* End the prepared nondefault w control. */
        CHECK(mc_client_screen_retain(current_screen, NULL) == MC_CLIENT_SCREEN_OK);
        CHECK(mc_client_application_startup_retire(&attempt, startup.error) ==
                  MC_CLIENT_STARTUP_BUSY &&
              app->r == current_screen && app->v == ingame->candidate &&
              app->U == resource_call->candidate && app->display != NULL && !menu_call->retired);
        CHECK(mc_client_screen_release(&current_screen, NULL) == MC_CLIENT_SCREEN_OK &&
              current_screen == NULL);
        mc_client_source_uq *external_gui = app->v;
        CHECK(mc_client_gui_ingame_retain(external_gui, NULL) == MC_CLIENT_GUI_INGAME_OK);
        mc_client_source_du *external_particles_gui = initialized.held_particles;
        CHECK(mc_client_screen_particles_retain(external_particles_gui, NULL) ==
              MC_CLIENT_SCREEN_OK);
        CHECK(mc_client_application_startup_retire(&attempt, startup.error) ==
                  MC_CLIENT_STARTUP_BUSY &&
              menu_call->retired && !menu_call->r_stored && menu_call->display == NULL &&
              app->r == NULL && atomic_load(&app->screen_borrows) == 1 && app->v == external_gui &&
              app->U == resource_call->candidate && app->settings != NULL);
        CHECK(mc_client_screen_particles_release(&external_particles_gui, NULL) ==
                  MC_CLIENT_SCREEN_OK &&
              external_particles_gui == NULL);
        CHECK(mc_client_application_startup_retire(&attempt, startup.error) ==
                  MC_CLIENT_STARTUP_BUSY &&
              app->r == NULL && menu_call->retired && app->v == external_gui &&
              app->U == resource_call->candidate && !ingame->retired &&
              atomic_load(&app->screen_borrows) == 0);
        CHECK(mc_client_gui_ingame_release(&external_gui, NULL) == MC_CLIENT_GUI_INGAME_OK &&
              external_gui == NULL);
        CHECK(statistics_retire_resources(attempt) == 0);
        CHECK(app->U == NULL && resource_call->retired &&
              atomic_load(&app->resource_thread_borrows) == 0);
        /* Both actual later owners keep the complete App dependency graph
         * live while an external occurrence exists. No counted App cycle. */
        mc_client_render_global *external_global = app->g;
        CHECK(mc_client_render_global_retain(external_global, NULL) == MC_CLIENT_RENDER_GLOBAL_OK);
        CHECK(mc_client_application_startup_retire(&attempt, startup.error) ==
              MC_CLIENT_STARTUP_BUSY);
        CHECK(attempt != NULL && app->g == external_global &&
              app->effect_renderer == rendering->particle && rendering->g_stored &&
              rendering->j_stored && !rendering->retired && !effects->retired &&
              atomic_load(&app->effect_borrows) == 2 && app->stats_writer != NULL &&
              app->C != NULL && app->settings != NULL && app->display != NULL);
        CHECK(mc_client_render_global_release(&external_global, NULL) ==
                  MC_CLIENT_RENDER_GLOBAL_OK &&
              external_global == NULL);
        mc_client_effect_renderer *external_particles = app->effect_renderer;
        CHECK(mc_client_effect_renderer_retain(external_particles) == MC_EFFECT_RENDERER_OK);
        CHECK(mc_client_application_startup_retire(&attempt, startup.error) ==
              MC_CLIENT_STARTUP_BUSY);
        CHECK(app->g == rendering->global && app->effect_renderer == external_particles &&
              rendering->j_stored && rendering->g_stored && !effects->retired &&
              atomic_load(&app->effect_borrows) == 2);
        CHECK(mc_client_effect_renderer_release(external_particles) == MC_EFFECT_RENDERER_OK);
        external_particles = NULL;
        /* Real external occurrence keeps its counted actual-App borrow. The
         * owner first drops all g and caller occurrences, then stops BEFORE
         * any earlier resource, worker, cursor, Settings or S teardown. */
        CHECK(atomic_load(&app->effect_borrows) == 2);
        CHECK(mc_client_application_startup_retire(&attempt, startup.error) ==
              MC_CLIENT_STARTUP_BUSY);
        CHECK(attempt != NULL && !effects->retired && app->S == effects->cx_candidate &&
              app->settings == effects->sound_settings && app->stats_writer != NULL &&
              app->display != NULL && app->C != NULL && !attempt->graphics->retired &&
              !attempt->input->retired && atomic_load(&app->effect_borrows) == 1);
        CHECK(rendering->retired && rendering->global == NULL && rendering->particle == NULL &&
              !rendering->g_stored && !rendering->j_stored && app->g == NULL &&
              app->effect_renderer == NULL && app->effect_renderer_retained == NULL &&
              rendering->global_engine == NULL && rendering->particle_engine == NULL &&
              rendering->global_constructor_pc == 651 && rendering->last_pc == 660 &&
              rendering->calls_completed && attempt->last_startup_store == 705);
        mc_texture_effect_state compass;
        CHECK(mc_client_texture_effect_inspect(external_compass, &compass) && compass.app == app &&
              compass.phase == MC_TEXTURE_EFFECT_READY && compass.native_holds == 1);
        for (size_t index = 0; index < MC_CLIENT_STARTUP_EFFECT_CALLS; ++index)
            CHECK(effects->effects[index].owned == NULL &&
                  effects->effects[index].argument == NULL &&
                  effects->effects[index].container_retired);
        CHECK(mc_client_texture_effect_release(&external_compass, effect_error) ==
                  MC_TEXTURE_EFFECT_OK &&
              external_compass == NULL && atomic_load(&app->effect_borrows) == 0);
        free(effect_error);
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
        CHECK(effects->retired && app->S == NULL && !effects->S_stored &&
              effects->cx_candidate == NULL && effects->cx_classes == NULL &&
              !effects->sound_settings_borrow && !effects->sound_hook_attached &&
              effects->sound == NULL && effects->sound_settings == NULL &&
              effects->registrations_completed == 9 && effects->last_pc == 612 &&
              mc_sound_manager_read_state(app->B, &sound) && sound.settings == NULL);
        for (size_t index = 0; index < MC_CLIENT_STARTUP_EFFECT_CALLS; ++index)
            CHECK(effects->effects[index].engine == NULL &&
                  effects->effects[index].registration.source_returned);
        CHECK(mc_client_cx_classes_inspect(app->source_namespace->startup_cx_classes, &cx_classes,
                                           NULL) == MC_CLIENT_CX_OK &&
              cx_classes.phase == MC_CLIENT_CX_CLASS_READY && cx_classes.enabled &&
              cx_classes.holds == 1); /* No source static reset during native App cleanup. */
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
