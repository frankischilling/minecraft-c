#include "application_owner_internal.h"
#include "application_startup_internal.h"

#include <stdlib.h>

#define MC_CLASS "net/minecraft/client/Minecraft"

mc_client_startup_options mc_client_application_startup_default_options(void) {
    return (mc_client_startup_options){.hidden = false,
                                       .maximum_builder_units = 1048576u,
                                       .maximum_string_bytes = 4u * 1048576u,
                                       .maximum_default_font_bytes = 32u * 1048576u,
                                       .render_engine = mc_client_render_engine_default_options(),
                                       .bitmap_font = mc_client_render_engine_default_options(),
                                       .color_lookup = mc_client_color_lookup_default_options(),
                                       .colorizers = mc_client_colorizer_default_options(),
                                       .entity_renderer =
                                           mc_client_entity_renderer_default_options(),
                                       .manager = mc_client_render_manager_default_options(),
                                       .render_global = {8u * 1024u * 1024u, SIZE_MAX},
                                       .particles = {16u * 1024u * 1024u}};
}

static bool app_ready(const client_application *app) {
    return app != NULL && app->construction != NULL &&
           app->native_lifetime_phase == MC_CLIENT_APPLICATION_LIFETIME_REGISTERED &&
           app->constructor_phase == MC_CLIENT_APP_READY && app->source_namespace_borrowed &&
           app->source_namespace != NULL;
}

static mc_client_startup_status stop_attempt(mc_client_application_startup *startup,
                                             mc_client_startup_error *out) {
    mc_client_startup_error retention = {0};
    if (application_startup_retain_settings_error(startup, &retention) != MC_CLIENT_STARTUP_OK) {
        /* The original complete Settings cause remains nested and its owner
         * stays live. Native metadata allocation is a separate current stop. */
        startup->failure.status = retention.status;
        startup->failure.operation = retention.operation;
        startup->failure.string = retention.string;
    }
    startup->active = false;
    startup->phase = MC_CLIENT_STARTUP_STOPPED;
    if (out != NULL)
        *out = startup->failure;
    return startup->failure.status;
}

mc_client_startup_status
application_startup_retain_settings_error(mc_client_application_startup *startup,
                                          mc_client_startup_error *error) {
    mc_game_settings_text *line = &startup->failure.settings.line;
    if (startup->settings_error_line == NULL && line->units != NULL) {
        /* This is metadata, not a source String identity/store. The parser's
         * existing bound limits the retained native copy to at most1Mi UTF16
         * units; no legacy constructor/reader budget is changed. */
        if (line->is_null || line->length > MC_GAME_SETTINGS_MAX_TEXT_UNITS)
            return application_startup_fail(error, MC_CLIENT_STARTUP_INVALID_PHASE,
                                            "startup Settings diagnostic view contract", NULL, NULL,
                                            NULL, 0);
        error->string = mc_world_info_name_copy((mc_nbt_string){line->units, line->length},
                                                SIZE_MAX, &startup->settings_error_line);
        if (error->string != MC_WORLD_INFO_OK)
            return application_startup_fail(error, MC_CLIENT_STARTUP_STRING_FAILED,
                                            "startup native Settings diagnostic retention", NULL,
                                            NULL, NULL, 0);
    }
    if (startup->settings_error_line != NULL) {
        const mc_nbt_string retained = mc_world_info_name_text(startup->settings_error_line);
        *line = (mc_game_settings_text){retained.units, retained.length, false};
    }
    return MC_CLIENT_STARTUP_OK;
}

mc_client_startup_status mc_client_application_startup_allocate(
    client_application *app, const mc_client_startup_services *services,
    const mc_client_startup_options *options, mc_client_application_startup **out,
    mc_client_startup_error *error) {
    mc_client_startup_error failure = {0};
    if (error != NULL)
        *error = failure;
    if (out == NULL || services == NULL || services->files == NULL ||
        services->save_formats == NULL || services->directory_inputs == NULL ||
        services->settings_classes == NULL)
        return application_startup_fail(
            error == NULL ? &failure : error, MC_CLIENT_STARTUP_INVALID_ARGUMENT,
            "startup concrete native services/output", NULL, NULL, NULL, 0);
    if (!app_ready(app) || app->startup != NULL || app->display != NULL ||
        app->game_directory != NULL || app->aa != NULL || app->settings != NULL || app->D != NULL ||
        app->render_engine != NULL || app->font_renderer != NULL ||
        app->font_renderer_retained != NULL || app->font_renderer_constructing ||
        app->stats_writer != NULL || app->stats_writer_retained != NULL ||
        app->stats_writer_constructing || app->statistics_retiring || app->statistics_retired ||
        app->C != NULL || app->diagnostic_game_directory != NULL)
        return application_startup_fail(
            error == NULL ? &failure : error, MC_CLIENT_STARTUP_INVALID_PHASE,
            "startup genuine constructor READY App admission", NULL, NULL, NULL, 0);
    const mc_client_startup_options selected =
        options == NULL ? mc_client_application_startup_default_options() : *options;
    if (selected.maximum_builder_units < 16 || selected.maximum_string_bytes == 0)
        return application_startup_fail(error == NULL ? &failure : error, MC_CLIENT_STARTUP_LIMIT,
                                        "startup native string budget", NULL, NULL, NULL, 0);
    mc_client_application_startup *startup = calloc(1, sizeof(*startup));
    if (startup == NULL)
        return application_startup_fail(
            error == NULL ? &failure : error, MC_CLIENT_STARTUP_ALLOCATION_FAILED,
            "startup native call-record allocation", NULL, NULL, NULL, 0);
    startup->app = app;
    startup->services = *services;
    startup->options = selected;
    startup->last_manager_pc = -1;
    app->startup = startup;
    *out = startup; /* Retain any later native admission-failure prefix. */
    if (mc_java_files_retain(services->files, &startup->failure.file) != MC_JAVA_FILE_OK) {
        application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_FILE_FAILED,
                                 "startup concrete File namespace hold", NULL, NULL, NULL, 0);
        return stop_attempt(startup, error);
    }
    startup->files_held = true;
    if (mc_client_save_formats_retain(services->save_formats, &startup->failure.save) !=
        MC_CLIENT_SAVE_OK) {
        application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_SAVE_FAILED,
                                 "startup concrete SaveFormat namespace hold", NULL, NULL, NULL, 0);
        return stop_attempt(startup, error);
    }
    startup->formats_held = true;
    if (mc_client_directory_inputs_retain(services->directory_inputs,
                                          &startup->failure.directory) !=
        MC_CLIENT_GAME_DIRECTORY_OK) {
        application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_DIRECTORY_FAILED,
                                 "startup native directory-input scope hold", NULL, NULL, NULL, 0);
        return stop_attempt(startup, error);
    }
    startup->inputs_held = true;
    if (mc_game_settings_classes_retain(services->settings_classes, &startup->failure.settings) !=
        MC_GAME_SETTINGS_OK) {
        application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_SETTINGS_FAILED,
                                 "startup actual kv class namespace occurrence", NULL, NULL, NULL,
                                 0);
        return stop_attempt(startup, error);
    }
    startup->settings_classes_held = true;
    if (services->icons != NULL) {
        if (mc_client_sdk_icons_retain(services->icons, &startup->failure.icons) !=
            MC_CLIENT_SDK_ICONS_OK) {
            application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_DISPLAY_FAILED,
                                     "startup native imported icon occurrence", NULL, NULL, NULL,
                                     0);
            return stop_attempt(startup, error);
        }
        startup->icons_held = true;
    }
    return MC_CLIENT_STARTUP_OK;
}

mc_client_startup_status application_startup_title(mc_client_application_classes *classes,
                                                   mc_player_string **out,
                                                   mc_client_startup_error *error) {
    if (mtx_lock(&classes->mutex) != thrd_success)
        return application_startup_fail(error, MC_CLIENT_STARTUP_OWNER_FAILED,
                                        "startup title native namespace lock", NULL, NULL, NULL, 0);
    if (classes->native_retiring || classes->phase != MC_CLIENT_APPLICATION_CLASS_READY) {
        (void)mtx_unlock(&classes->mutex);
        return application_startup_fail(error, MC_CLIENT_STARTUP_INVALID_PHASE,
                                        "startup title live namespace", NULL, NULL, NULL, 0);
    }
    if (classes->startup_title == NULL) {
        static const uint16_t units[] = {'M', 'i', 'n', 'e', 'c', 'r', 'a', 'f', 't', ' ',
                                         'M', 'i', 'n', 'e', 'c', 'r', 'a', 'f', 't', ' ',
                                         'B', 'e', 't', 'a', ' ', '1', '.', '7', '.', '3'};
        error->player_string = mc_player_string_create(units, sizeof(units) / sizeof(units[0]),
                                                       &classes->startup_title);
        if (error->player_string != MC_PLAYER_OK) {
            (void)mtx_unlock(&classes->mutex);
            return application_startup_fail(error, MC_CLIENT_STARTUP_STRING_FAILED,
                                            "actual startup title ldc", MC_CLASS, "a", "()V", 130);
        }
    }
    *out = classes->startup_title;
    (void)mtx_unlock(&classes->mutex);
    return MC_CLIENT_STARTUP_OK;
}

static mc_client_startup_status display_failure(mc_client_application_startup *startup,
                                                unsigned int pc) {
    mc_client_startup_error *error = &startup->failure;
    error->caller = (mc_client_application_owner_site){MC_CLASS, "a", "()V", pc};
    if (pc == 135 && error->display.kind == MC_CLIENT_DISPLAY_SOURCE_LWJGL_EXCEPTION) {
        error->caller.pc = 143;
        return application_startup_fail(error, MC_CLIENT_STARTUP_UNIMPLEMENTED,
                                        "actual caught LWJGLException printStackTrace dependency",
                                        "java/lang/Throwable", "printStackTrace", "()V", 0);
    }
    return application_startup_fail(
        error, MC_CLIENT_STARTUP_DISPLAY_FAILED, "actual startup Display child",
        error->display.site.class_name, error->display.site.method, error->display.site.descriptor,
        error->display.site.pc < 0 ? 0u : (unsigned int)error->display.site.pc);
}

mc_client_startup_status mc_client_application_run_prefix(mc_client_application_startup *startup,
                                                          mc_client_startup_error *out) {
    mc_client_startup_error invalid = {0};
    if (out != NULL)
        *out = invalid;
    if (startup == NULL || !app_ready(startup->app) || startup->app->startup != startup ||
        startup->phase != MC_CLIENT_STARTUP_RAW || startup->active)
        return application_startup_fail(
            out == NULL ? &invalid : out, MC_CLIENT_STARTUP_INVALID_PHASE,
            "one-shot actual run-prefix admission", NULL, NULL, NULL, 0);
    startup->phase = MC_CLIENT_STARTUP_ACTIVE;
    startup->active = true;
    client_application *app = startup->app;
    atomic_store_explicit(&app->J, true, memory_order_seq_cst); /* run putfield2 */
    startup->run_J_store_completed = true;
    startup->failure.caller = (mc_client_application_owner_site){MC_CLASS, "run", "()V", 6};
    if (app->m != NULL) {
        application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_UNIMPLEMENTED,
                                 "actual nonnull Canvas getGraphics", "java/awt/Canvas",
                                 "getGraphics", "()Ljava/awt/Graphics;", 11);
        return stop_attempt(startup, out);
    }
    if (app->Q) {
        application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_UNIMPLEMENTED,
                                 "actual fullscreen Display child", "org/lwjgl/opengl/Display",
                                 "setFullscreen", "(Z)V", 62);
        return stop_attempt(startup, out);
    }
    if (!mc_client_display_mode_allocate(&startup->mode, &startup->failure.display)) {
        display_failure(startup, 112);
        return stop_attempt(startup, out);
    }
    if (!mc_client_display_mode_construct(startup->mode, app->d, app->e,
                                          &startup->failure.display)) {
        display_failure(startup, 124);
        return stop_attempt(startup, out);
    }
    if (!mc_client_display_stage_allocate(&startup->display, &startup->failure.display)) {
        display_failure(startup, 127);
        return stop_attempt(startup, out);
    }
    /* Native prerequisite only: same imported ready Icons, before any SDK
     * active use. A missing/bad binding is not a source LWJGLException. */
    if (!mc_client_display_stage_bind_icons(startup->display, startup->services.icons,
                                            &startup->failure.display)) {
        display_failure(startup, 127);
        return stop_attempt(startup, out);
    }
    if (!mc_client_display_stage_set_mode(startup->display, startup->mode,
                                          &startup->failure.display)) {
        display_failure(startup, 127);
        return stop_attempt(startup, out);
    }
    mc_player_string *title = NULL;
    if (application_startup_title(app->source_namespace, &title, &startup->failure) !=
        MC_CLIENT_STARTUP_OK)
        return stop_attempt(startup, out);
    if (!mc_client_display_stage_set_title(startup->display, title, &startup->failure.display)) {
        display_failure(startup, 132);
        return stop_attempt(startup, out);
    }
    if (!mc_client_display_create_staged(startup->display, startup->options.hidden,
                                         &startup->failure.display)) {
        display_failure(startup, 135);
        return stop_attempt(startup, out);
    }
    startup->display_created = true;
    app->display = startup->display; /* SAME concrete native resource, not a source putfield. */
    startup->failure.caller = (mc_client_application_owner_site){MC_CLASS, "a", "()V", 160};
    (void)application_startup_directory(startup);
    return stop_attempt(startup, out);
}

mc_client_startup_status mc_client_application_source_stop(client_application *app,
                                                           mc_client_startup_error *out) {
    mc_client_startup_error invalid = {0};
    if (out != NULL)
        *out = invalid;
    if (app == NULL || app->native_lifetime_phase != MC_CLIENT_APPLICATION_LIFETIME_REGISTERED ||
        app->constructor_phase == MC_CLIENT_APP_RETIRING)
        return application_startup_fail(
            out == NULL ? &invalid : out, MC_CLIENT_STARTUP_INVALID_PHASE,
            "actual f requires live registered App storage", NULL, NULL, NULL, 0);
    atomic_store_explicit(&app->J, false, memory_order_seq_cst); /* actual f putfield2 */
    return MC_CLIENT_STARTUP_OK;
}

bool mc_client_application_startup_inspect(const mc_client_application_startup *startup,
                                           mc_client_startup_state *out) {
    if (startup == NULL || out == NULL || startup->active)
        return false;
    *out = (mc_client_startup_state){
        .phase = startup->phase,
        .rendering = startup->rendering,
        .app = startup->app,
        .display = startup->display,
        .mode = startup->mode,
        .directory_candidate = startup->directory_candidate,
        .saves_candidate = startup->saves_candidate,
        .format_candidate = startup->format_candidate,
        .settings_candidate = startup->settings_candidate,
        .pack_candidate = startup->pack_candidate,
        .D_stored = startup->D_stored,
        .engine_candidate = startup->engine_candidate,
        .engine_classes = startup->engine_classes,
        .default_font = startup->default_font,
        .p_stored = startup->p_stored,
        .ji_active_use_completed = startup->ji_active_use_completed,
        .font_candidate = startup->font_candidate,
        .font_classes = startup->font_classes,
        .font_resource = startup->font_resource,
        .q_stored = startup->q_stored,
        .entity_renderer_candidate = startup->entity_renderer_candidate,
        .manager_classes = startup->manager_classes,
        .manager_capture = startup->manager_capture,
        .manager_hand_candidate = startup->manager_hand_candidate,
        .manager_consumer_attached = startup->manager_consumer.attached,
        .manager_f_stored = startup->manager_f_stored,
        .last_manager_pc = startup->last_manager_pc,
        .t_stored = startup->t_stored,
        .color_argument = startup->color_argument,
        .color_results = {startup->color_results[0], startup->color_results[1],
                          startup->color_results[2]},
        .color_literals = {startup->color_literals[0], startup->color_literals[1],
                           startup->color_literals[2]},
        .color_lookups_completed = startup->color_lookups_completed,
        .color_setters_completed = startup->color_setters_completed,
        .last_color_lookup_pc = startup->last_color_lookup_pc,
        .last_color_setter_pc = startup->last_color_setter_pc,
        .color_lookup_source_returned = startup->color_lookup_source_returned,
        .colors_entered = startup->colors_entered,
        .color_metadata_retired = startup->color_metadata_retired,
        .home = startup->home,
        .appdata = startup->appdata,
        .directory_child = startup->directory_child,
        .failure = &startup->failure,
        .retirement = &startup->retirement,
        .active = startup->active,
        .run_J_store_completed = startup->run_J_store_completed,
        .display_created = startup->display_created,
        .Z_stored = startup->Z_stored,
        .aa_stored = startup->aa_stored,
        .z_stored = startup->z_stored,
        .appdata_present = startup->appdata_present,
        .last_startup_store = startup->last_startup_store,
        .last_directory_pc = startup->last_directory_pc};
    return true;
}
