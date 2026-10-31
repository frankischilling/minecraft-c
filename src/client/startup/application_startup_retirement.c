#include "../view_slot.h"
#include "application_startup_effects.h"
#include "application_startup_gui_ingame.h"
#include "application_startup_input.h"
#include "application_startup_internal.h"
#include "application_startup_rendering.h"
#include "application_startup_statistics.h"

#include <stdlib.h>

static mc_client_startup_status cleanup_failure(mc_client_application_startup *startup,
                                                mc_client_startup_status status,
                                                const char *operation,
                                                mc_client_startup_error *out) {
    application_startup_fail(&startup->retirement, status, operation, NULL, NULL, NULL, 0);
    startup->active = false;
    if (out != NULL)
        *out = startup->retirement;
    return status;
}

static mc_client_startup_status manager_consumer_end_failure(mc_client_startup_error *out,
                                                             mc_client_startup_status status,
                                                             const char *operation) {
    if (out != NULL)
        (void)application_startup_fail(out, status, operation, NULL, NULL, NULL, 0);
    return status;
}

/* The admitted caller owns startup's active guard. Both legacy full cleanup
 * and the earlier native prefix consume this SAME record, never a count guess. */
static mc_client_startup_status manager_consumer_end_entered(mc_client_application_startup *startup,
                                                             mc_client_startup_error *error) {
    if (mc_client_render_manager_consumer_detach(&startup->manager_consumer, &error->manager) !=
        MC_CLIENT_MANAGER_OK)
        return application_startup_fail(error, MC_CLIENT_STARTUP_BUSY,
                                        "startup actual manager consumer detach", NULL, NULL, NULL,
                                        0);
    startup->manager_capture = NULL;
    startup->manager_classes = NULL;
    return MC_CLIENT_STARTUP_OK;
}

mc_client_startup_status
application_startup_terminal_end_manager_consumer(mc_client_application_startup *startup,
                                                  mc_client_startup_error *caller_error) {
    if (caller_error != NULL)
        *caller_error = (mc_client_startup_error){0};
    if (startup == NULL)
        return manager_consumer_end_failure(caller_error, MC_CLIENT_STARTUP_INVALID_ARGUMENT,
                                            "startup terminal consumer receiver");
    if (startup->active || startup->app == NULL || startup->app->startup != startup)
        return manager_consumer_end_failure(caller_error, MC_CLIENT_STARTUP_BUSY,
                                            "startup terminal consumer active/live App borrow");
    if (startup->phase == MC_CLIENT_STARTUP_RETIRING &&
        !startup->terminal_manager_consumer_selected)
        return manager_consumer_end_failure(caller_error, MC_CLIENT_STARTUP_BUSY,
                                            "startup legacy retirement already entered");
    if (startup->terminal_manager_consumer_ended) {
        if (startup->manager_consumer.attached || startup->manager_consumer.classes != NULL ||
            startup->manager_classes != NULL || startup->manager_capture != NULL)
            return manager_consumer_end_failure(caller_error, MC_CLIENT_STARTUP_INVALID_PHASE,
                                                "startup ended manager consumer changed");
        return MC_CLIENT_STARTUP_OK;
    }
    const mc_client_manager_consumer *consumer = &startup->manager_consumer;
    if ((consumer->attached &&
         (consumer->classes == NULL || consumer->classes != startup->manager_classes)) ||
        (!consumer->attached && consumer->classes != NULL) ||
        (startup->manager_classes == NULL && startup->manager_capture != NULL))
        return manager_consumer_end_failure(caller_error, MC_CLIENT_STARTUP_INVALID_PHASE,
                                            "startup actual manager consumer association");
    if (startup->manager_classes != NULL) {
        mc_client_manager_state state;
        if (!mc_client_render_manager_inspect(startup->manager_classes, &state))
            return manager_consumer_end_failure(caller_error, MC_CLIENT_STARTUP_BUSY,
                                                "startup actual manager consumer owner active");
        if (startup->manager_capture != NULL &&
            (!state.source_views_available || state.a != startup->manager_capture))
            return manager_consumer_end_failure(caller_error, MC_CLIENT_STARTUP_INVALID_PHASE,
                                                "startup actual captured manager identity changed");
    }
    startup->phase = MC_CLIENT_STARTUP_RETIRING;
    startup->terminal_manager_consumer_selected = true;
    startup->active = true;
    startup->retirement = (mc_client_startup_error){0};
    const mc_client_startup_status status =
        manager_consumer_end_entered(startup, &startup->retirement);
    if (status == MC_CLIENT_STARTUP_OK)
        startup->terminal_manager_consumer_ended = true;
    startup->active = false;
    if (caller_error != NULL)
        *caller_error = startup->retirement;
    return status;
}

mc_client_startup_status mc_client_application_startup_retire(mc_client_application_startup **slot,
                                                              mc_client_startup_error *out) {
    mc_client_startup_error error = {0};
    if (out != NULL)
        *out = error;
    if (slot == NULL)
        return application_startup_fail(out == NULL ? &error : out,
                                        MC_CLIENT_STARTUP_INVALID_ARGUMENT,
                                        "startup native retirement output", NULL, NULL, NULL, 0);
    mc_client_application_startup *startup = *slot;
    if (startup == NULL)
        return MC_CLIENT_STARTUP_OK;
    if (startup->active || startup->app == NULL || startup->app->startup != startup)
        return application_startup_fail(out == NULL ? &error : out, MC_CLIENT_STARTUP_BUSY,
                                        "startup active/live App borrow", NULL, NULL, NULL, 0);
    if (startup->app->screen_transitions != NULL)
        return application_startup_fail(out == NULL ? &error : out, MC_CLIENT_STARTUP_BUSY,
                                        "runtime screen graph still leases startup dependencies",
                                        NULL, NULL, NULL, 0);
    startup->phase = MC_CLIENT_STARTUP_RETIRING;
    startup->active = true;
    startup->retirement = error;
    client_application *app = startup->app;
    const mc_client_startup_status menu = application_startup_menu_retire(startup);
    if (menu != MC_CLIENT_STARTUP_OK)
        return cleanup_failure(startup, menu, "startup menu/screen retirement", out);
    if (atomic_load_explicit(&app->screen_borrows, memory_order_acquire) != 0)
        return cleanup_failure(startup, MC_CLIENT_STARTUP_BUSY,
                               "external screen descendants still borrow App dependencies", out);
    const mc_client_startup_status gui_ingame = application_startup_gui_ingame_retire(startup);
    if (gui_ingame != MC_CLIENT_STARTUP_OK)
        return cleanup_failure(startup, gui_ingame, "startup actual GuiIngame native retirement",
                               out);
    const mc_client_startup_status resources = application_startup_resources_retire(startup);
    if (resources != MC_CLIENT_STARTUP_OK)
        return cleanup_failure(startup, resources, "startup resource worker retirement", out);
    /* Do not consume unrelated later children. Until their actual caller and
     * retirement route exist they keep this App and its startup services live. */
    if ((app->g != NULL && (startup->rendering == NULL || !startup->rendering->g_stored)) ||
        app->active_world != NULL ||
        (app->S != NULL && (startup->effects == NULL || !startup->effects->S_stored)) ||
        (app->C != NULL && (startup->input == NULL || !startup->input->C_stored)) ||
        ((app->stats_writer != NULL || app->stats_writer_retained != NULL) &&
         (startup->statistics == NULL || !startup->statistics->entered)))
        return cleanup_failure(startup, MC_CLIENT_STARTUP_BUSY,
                               "later source children still borrow startup resources", out);
    const mc_client_startup_status rendering = application_startup_rendering_retire(startup);
    if (rendering != MC_CLIENT_STARTUP_OK)
        return cleanup_failure(startup, rendering,
                               "startup actual RenderGlobal/dn native retirement", out);
    const mc_client_startup_status effects = application_startup_effects_retire(startup);
    if (effects != MC_CLIENT_STARTUP_OK)
        return cleanup_failure(startup, effects,
                               "startup actual effect occurrences and App borrows", out);
    const mc_client_startup_status graphics = application_startup_graphics_retire(startup);
    if (graphics != MC_CLIENT_STARTUP_OK)
        return cleanup_failure(startup, graphics,
                               "startup retained GL diagnostic calls before Display retirement",
                               out);
    const mc_client_startup_status input = application_startup_input_retire(startup);
    if (input != MC_CLIENT_STARTUP_OK)
        return cleanup_failure(startup, input, "startup actual vy/Cursor native retirement", out);
    const mc_client_startup_status statistics = application_startup_statistics_retire(startup);
    if (statistics != MC_CLIENT_STARTUP_OK)
        return cleanup_failure(startup, statistics,
                               "startup actual xi worker/File/Session retirement", out);
    /* End this actual App's source admission BEFORE any global F dependency
     * can block sj/Engine/App. No counted App<->TH ownership cycle exists. */
    if (manager_consumer_end_entered(startup, &startup->retirement) != MC_CLIENT_STARTUP_OK)
        return cleanup_failure(startup, MC_CLIENT_STARTUP_BUSY,
                               "startup actual manager consumer detach", out);
    const mc_client_entity_renderer_status local_hand = mc_client_held_item_retire(
        &startup->manager_hand_candidate, &startup->retirement.entity_renderer);
    if (local_hand != MC_CLIENT_ENTITY_RENDERER_OK)
        return cleanup_failure(startup,
                               local_hand == MC_CLIENT_ENTITY_RENDERER_BUSY
                                   ? MC_CLIENT_STARTUP_BUSY
                                   : MC_CLIENT_STARTUP_ENTITY_RENDERER_FAILED,
                               "startup unpublished actual RA strict retirement", out);
    if ((startup->t_stored && app->t != startup->entity_renderer_candidate) ||
        (!startup->t_stored && app->t != NULL))
        return cleanup_failure(startup, MC_CLIENT_STARTUP_INVALID_PHASE,
                               "startup actual t identity changed outside owner", out);
    if (app->view_occurrence != NULL && mc_client_view_slot_store(app, NULL) != MC_ENTITY_OK)
        return cleanup_failure(startup, MC_CLIENT_STARTUP_BUSY,
                               "startup actual view occurrence cleanup", out);
    const mc_client_entity_renderer_status renderer_status = mc_client_entity_renderer_retire(
        &startup->entity_renderer_candidate, &startup->retirement.entity_renderer);
    if (startup->entity_renderer_candidate == NULL && startup->t_stored) {
        app->t = NULL; /* Native source view ended, no source putfield. */
        startup->t_stored = false;
    }
    if (renderer_status != MC_CLIENT_ENTITY_RENDERER_OK)
        return cleanup_failure(startup,
                               renderer_status == MC_CLIENT_ENTITY_RENDERER_BUSY
                                   ? MC_CLIENT_STARTUP_BUSY
                                   : MC_CLIENT_STARTUP_ENTITY_RENDERER_FAILED,
                               "startup actual px strict retirement before sj/Engine/Settings",
                               out);
    if (app->font_renderer_constructing || app->font_renderer_retained != startup->font_candidate ||
        (startup->q_stored && app->font_renderer != startup->font_candidate) ||
        (!startup->q_stored && app->font_renderer != NULL))
        return cleanup_failure(startup, MC_CLIENT_STARTUP_INVALID_PHASE,
                               "startup actual q/retained sj identity changed outside owner", out);
    const mc_client_bitmap_font_status font_status = mc_client_bitmap_font_retire(
        &app->font_renderer_retained, &startup->retirement.bitmap_font);
    if (app->font_renderer_retained == NULL) {
        startup->font_candidate = NULL;
        if (startup->q_stored) {
            app->font_renderer = NULL; /* Native source view ended; no source putfield. */
            startup->q_stored = false;
        }
    }
    if (font_status != MC_CLIENT_BITMAP_FONT_OK)
        return cleanup_failure(
            startup,
            font_status == MC_CLIENT_BITMAP_FONT_BUSY ? MC_CLIENT_STARTUP_BUSY
                                                      : MC_CLIENT_STARTUP_BITMAP_FONT_FAILED,
            "startup actual sj strict retirement before Engine/Pack/Settings/Display", out);
    /* Pending real IO/array prefixes and even successful lookup metadata must
     * end before strict Engine retirement. The font gate above protects every
     * App/Settings/provider dependency first. No lookup/source body is rerun. */
    if (startup->colors_entered && !startup->color_metadata_retired) {
        const mc_client_texture_lookup_status lookup = mc_client_render_engine_retire_lookup(
            startup->engine_candidate, &startup->retirement.colors);
        if (lookup != MC_CLIENT_LOOKUP_OK) {
            if (!startup->color_cleanup_latched) {
                startup->color_cleanup = startup->retirement.colors;
                startup->color_cleanup_latched = true;
            }
            return cleanup_failure(startup,
                                   lookup == MC_CLIENT_LOOKUP_BUSY
                                       ? MC_CLIENT_STARTUP_BUSY
                                       : MC_CLIENT_STARTUP_COLORS_FAILED,
                                   "startup real color lookup prefix native retirement", out);
        }
        if (startup->color_cleanup_latched)
            startup->retirement.colors = startup->color_cleanup;
        const mc_client_color_array_status argument = mc_client_color_array_release(
            &startup->color_argument, &startup->retirement.color_argument);
        if (argument != MC_CLIENT_COLOR_ARRAY_OK)
            return cleanup_failure(
                startup,
                argument == MC_CLIENT_COLOR_ARRAY_BUSY ? MC_CLIENT_STARTUP_BUSY
                                                       : MC_CLIENT_STARTUP_COLORS_FAILED,
                "startup actual incoming color argument occurrence retirement", out);
        if (!mc_client_texture_lookup_free_metadata(startup->engine_candidate))
            return cleanup_failure(startup, MC_CLIENT_STARTUP_BUSY,
                                   "startup final color lookup metadata retirement", out);
        startup->color_metadata_retired = true;
    } else if (startup->color_cleanup_latched) {
        startup->retirement.colors = startup->color_cleanup;
    }
    /* This per-App record cannot end a shared namespace. The actual outer
     * owner calls its canonical terminal retirement with Display alive. Retain
     * all dependent owners while actual resources/context/other font leases
     * remain; an unrelated source call alone is not global quiescence here. */
    if (mc_client_render_engine_resources_pending(startup->engine_classes))
        return cleanup_failure(
            startup, MC_CLIENT_STARTUP_BUSY,
            "startup graphics namespace requires outer-owner terminal retirement", out);
    if ((startup->p_stored && app->render_engine != startup->engine_candidate) ||
        (!startup->p_stored && app->render_engine != NULL))
        return cleanup_failure(startup, MC_CLIENT_STARTUP_INVALID_PHASE,
                               "startup actual p identity changed outside owner", out);
    const mc_client_render_engine_status engine_status =
        mc_client_render_engine_retire(&startup->engine_candidate, &startup->retirement.engine);
    if (startup->engine_candidate == NULL) {
        for (size_t index = 0; index < MC_CLIENT_COLORIZER_COUNT; ++index)
            startup->color_results[index] = NULL; /* Borrowed Engine.c trace ended. */
    }
    if (startup->engine_candidate == NULL && startup->p_stored) {
        app->render_engine = NULL; /* Native source view ended; no source putfield. */
        startup->p_stored = false;
    }
    if (engine_status != MC_CLIENT_RENDER_ENGINE_OK)
        return cleanup_failure(startup,
                               engine_status == MC_CLIENT_RENDER_ENGINE_BUSY
                                   ? MC_CLIENT_STARTUP_BUSY
                                   : MC_CLIENT_STARTUP_RENDER_ENGINE_FAILED,
                               "startup actual ji retirement before Pack/Settings/resources", out);
    startup->retirement.default_font = mc_client_default_font_release(startup->default_font);
    if (startup->retirement.default_font != MC_CLIENT_FONT_OK)
        return cleanup_failure(startup,
                               startup->retirement.default_font == MC_CLIENT_FONT_BUSY
                                   ? MC_CLIENT_STARTUP_BUSY
                                   : MC_CLIENT_STARTUP_RENDER_ENGINE_FAILED,
                               "startup native default-font occurrence retirement", out);
    startup->default_font = NULL;
    if ((startup->D_stored && app->D != startup->pack_candidate) ||
        (!startup->D_stored && app->D != NULL))
        return cleanup_failure(startup, MC_CLIENT_STARTUP_INVALID_PHASE,
                               "startup actual D identity changed outside owner", out);
    const mc_texture_pack_status pack_status = mc_client_texture_pack_list_source_retire(
        &startup->pack_candidate, &startup->retirement.pack);
    if (startup->pack_candidate == NULL && startup->D_stored) {
        app->D = NULL; /* Native source view ended; no source putfield. */
        startup->D_stored = false;
    }
    if (pack_status != MC_TEXTURE_PACK_OK)
        return cleanup_failure(startup,
                               pack_status == MC_TEXTURE_PACK_BUSY ? MC_CLIENT_STARTUP_BUSY
                                                                   : MC_CLIENT_STARTUP_PACK_FAILED,
                               "startup actual ik native retirement before Settings/resources",
                               out);
    if ((startup->z_stored && app->settings != startup->settings_candidate) ||
        (!startup->z_stored && app->settings != NULL))
        return cleanup_failure(startup, MC_CLIENT_STARTUP_INVALID_PHASE,
                               "startup actual z identity changed outside owner", out);
    const mc_client_startup_status retained =
        application_startup_retain_settings_error(startup, &startup->retirement);
    if (retained != MC_CLIENT_STARTUP_OK)
        return cleanup_failure(startup, retained,
                               "startup Settings diagnostic retention before consumption", out);
    const mc_game_settings_status settings_status =
        mc_game_settings_retire(&startup->settings_candidate, &startup->retirement.settings.native);
    if (settings_status != MC_GAME_SETTINGS_OK) {
        mc_game_settings_source_state state;
        if (mc_game_settings_source_inspect(startup->settings_candidate, &state))
            startup->retirement.settings = *state.retirement;
        return cleanup_failure(startup,
                               settings_status == MC_GAME_SETTINGS_BUSY
                                   ? MC_CLIENT_STARTUP_BUSY
                                   : MC_CLIENT_STARTUP_SETTINGS_FAILED,
                               "startup actual kv native retirement", out);
    }
    if (startup->z_stored) {
        app->settings = NULL; /* Native source view ended; no source putfield. */
        startup->z_stored = false;
    }
    mc_client_save_format *format = startup->format_candidate;
    if (startup->aa_stored && app->aa != format)
        return cleanup_failure(startup, MC_CLIENT_STARTUP_INVALID_PHASE,
                               "startup actual aa identity changed outside owner", out);
    const mc_client_save_status save_status =
        mc_client_save_format_release(&startup->format_candidate, &startup->retirement.save);
    if (save_status != MC_CLIENT_SAVE_OK)
        return cleanup_failure(startup,
                               save_status == MC_CLIENT_SAVE_BUSY ? MC_CLIENT_STARTUP_BUSY
                                                                  : MC_CLIENT_STARTUP_SAVE_FAILED,
                               "startup actual hi candidate/occurrence retirement", out);
    if (startup->aa_stored) {
        app->aa = NULL; /* Native source view ended, not source startup putfield. */
        startup->aa_stored = false;
    }
    if (mc_java_file_release(&startup->saves_candidate, &startup->retirement.file) !=
        MC_JAVA_FILE_OK)
        return cleanup_failure(startup,
                               startup->retirement.file.status == MC_JAVA_FILE_BUSY
                                   ? MC_CLIENT_STARTUP_BUSY
                                   : MC_CLIENT_STARTUP_FILE_FAILED,
                               "startup actual saves File occurrence retirement", out);
    if (mc_java_file_release(&startup->directory_candidate, &startup->retirement.file) !=
        MC_JAVA_FILE_OK)
        return cleanup_failure(startup,
                               startup->retirement.file.status == MC_JAVA_FILE_BUSY
                                   ? MC_CLIENT_STARTUP_BUSY
                                   : MC_CLIENT_STARTUP_FILE_FAILED,
                               "startup unpublished working-directory File retirement", out);
    if (startup->Z_stored) {
        if (app->game_directory != startup->published_Z)
            return cleanup_failure(startup, MC_CLIENT_STARTUP_INVALID_PHASE,
                                   "startup actual Z identity changed outside owner", out);
        if (mc_java_file_release(&app->game_directory, &startup->retirement.file) !=
            MC_JAVA_FILE_OK)
            return cleanup_failure(startup,
                                   startup->retirement.file.status == MC_JAVA_FILE_BUSY
                                       ? MC_CLIENT_STARTUP_BUSY
                                       : MC_CLIENT_STARTUP_FILE_FAILED,
                                   "startup actual Z File occurrence retirement", out);
        startup->Z_stored = false;
        startup->published_Z = NULL;
    }
    if (startup->display != NULL) {
        if (startup->display_created && app->display != startup->display)
            return cleanup_failure(startup, MC_CLIENT_STARTUP_INVALID_PHASE,
                                   "startup concrete Display identity changed", out);
        const bool retired =
            mc_client_display_stage_retire(&startup->display, &startup->retirement.display);
        /* A completed native destruction may consume the owner while still
         * reporting an earlier GL cleanup error. Inspect the real owned slot;
         * never leave an App view pointing at an already consumed Display. */
        if (startup->display == NULL && startup->display_created) {
            app->display = NULL;
            startup->display_created = false;
        }
        if (!retired)
            return cleanup_failure(startup, MC_CLIENT_STARTUP_DISPLAY_FAILED,
                                   "startup concrete staged Display retirement", out);
    }
    mc_client_display_mode_release(&startup->mode);
    application_directory_builder_destroy(&startup->builder);
    mc_world_info_name_release(startup->directory_child);
    startup->directory_child = NULL;
    mc_world_info_name_release(startup->appdata);
    startup->appdata = NULL;
    mc_world_info_name_release(startup->home);
    startup->home = NULL;
    if (startup->icons_held) {
        const mc_client_sdk_icons_status status =
            mc_client_sdk_icons_release(&startup->services.icons, &startup->retirement.icons);
        if (status != MC_CLIENT_SDK_ICONS_OK)
            return cleanup_failure(startup,
                                   status == MC_CLIENT_SDK_ICONS_BUSY
                                       ? MC_CLIENT_STARTUP_BUSY
                                       : MC_CLIENT_STARTUP_DISPLAY_FAILED,
                                   "startup native imported icon occurrence retirement", out);
        startup->icons_held = false;
    }
    if (startup->settings_classes_held) {
        const mc_game_settings_status status = mc_game_settings_classes_release(
            &startup->services.settings_classes, &startup->retirement.settings);
        if (status != MC_GAME_SETTINGS_OK)
            return cleanup_failure(startup,
                                   status == MC_GAME_SETTINGS_BUSY
                                       ? MC_CLIENT_STARTUP_BUSY
                                       : MC_CLIENT_STARTUP_SETTINGS_FAILED,
                                   "startup actual kv class namespace occurrence retirement", out);
        startup->settings_classes_held = false;
    }
    if (startup->inputs_held) {
        if (mc_client_directory_inputs_release(&startup->services.directory_inputs,
                                               &startup->retirement.directory) !=
            MC_CLIENT_GAME_DIRECTORY_OK)
            return cleanup_failure(startup,
                                   startup->retirement.directory.status ==
                                           MC_CLIENT_GAME_DIRECTORY_BUSY
                                       ? MC_CLIENT_STARTUP_BUSY
                                       : MC_CLIENT_STARTUP_DIRECTORY_FAILED,
                                   "startup native directory-provider occurrence retirement", out);
        startup->inputs_held = false;
    }
    if (startup->formats_held) {
        const mc_client_save_status status = mc_client_save_formats_release(
            &startup->services.save_formats, &startup->retirement.save);
        if (status != MC_CLIENT_SAVE_OK)
            return cleanup_failure(startup,
                                   status == MC_CLIENT_SAVE_BUSY ? MC_CLIENT_STARTUP_BUSY
                                                                 : MC_CLIENT_STARTUP_SAVE_FAILED,
                                   "startup SaveFormat namespace occurrence retirement", out);
        startup->formats_held = false;
    }
    if (startup->files_held) {
        if (mc_java_files_release(&startup->services.files, &startup->retirement.file) !=
            MC_JAVA_FILE_OK)
            return cleanup_failure(startup,
                                   startup->retirement.file.status == MC_JAVA_FILE_BUSY
                                       ? MC_CLIENT_STARTUP_BUSY
                                       : MC_CLIENT_STARTUP_FILE_FAILED,
                                   "startup File namespace occurrence retirement", out);
        startup->files_held = false;
    }
    /* Every potentially failing child/context cleanup completed. Error copies
     * now expire with this final record, never with earlier Settings cleanup. */
    mc_world_info_name_release(startup->settings_error_line);
    startup->settings_error_line = NULL;
    free(startup->logo);
    startup->logo = NULL;
    free(startup->statistics);
    startup->statistics = NULL;
    free(startup->input);
    startup->input = NULL;
    free(startup->graphics);
    startup->graphics = NULL;
    free(startup->effects);
    startup->effects = NULL;
    free(startup->rendering);
    free(startup->resources);
    free(startup->menu);
    free(startup->gui_ingame);
    startup->rendering = NULL;
    app->startup = NULL;
    /* No source f/J/o mutation and no kg join: constructor retirement owns
     * native worker cancellation after this actual borrowed App is detached. */
    free(startup);
    *slot = NULL;
    return MC_CLIENT_STARTUP_OK;
}
