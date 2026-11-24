#include "application.h"
#include "../../entity/player/player_internal.h"
#include "../application_state.h"
#include "../startup/game_directory.h"
#include <stdlib.h>
#include <string.h>

static void session_cause(mc_client_statistics_owner_error *error, const mc_player_error *cause) {
    error->session.status = cause->status;
    error->session.operation = cause->operation;
    error->session.site = cause->site;
    error->session.registry = cause->registry;
    error->session.entity = cause->entity.status;
}

static mc_client_statistics_owner_status fail(mc_client_statistics_owner_error *error,
                                              mc_client_statistics_owner_status status,
                                              const char *operation, const char *class_name,
                                              const char *method, const char *descriptor,
                                              unsigned int pc) {
    error->status = status;
    error->operation = operation;
    error->site = (mc_player_site){class_name, method, descriptor, pc};
    return status;
}

static bool admitted(client_application *app, mc_client_statistics_owner_error *error) {
    if (app == NULL || app->registry == NULL || app->entities == NULL || app->profile == NULL) {
        fail(error, MC_CLIENT_STATISTICS_OWNER_INVALID_ARGUMENT, "application statistics owners",
             NULL, NULL, NULL, 0);
        return false;
    }
    if (app->constructor_phase == MC_CLIENT_APP_RETIRING) {
        fail(error, MC_CLIENT_STATISTICS_OWNER_INVALID_PHASE,
             "actual application source work stopped", NULL, NULL, NULL, 0);
        return false;
    }
    if (app->statistics_retired) {
        fail(error, MC_CLIENT_STATISTICS_OWNER_INVALID_PHASE, "application statistics retired",
             NULL, NULL, NULL, 0);
        return false;
    }
    if (app->statistics_retiring) {
        fail(error, MC_CLIENT_STATISTICS_OWNER_BUSY, "application statistics retirement entered",
             NULL, NULL, NULL, 0);
        return false;
    }
    return true;
}

mc_client_statistics_owner_status
mc_client_application_construct_water_effect(client_application *app,
                                             const mc_water_texture_options *options,
                                             mc_client_statistics_owner_error *out) {
    mc_client_statistics_owner_error error = {0};
    if (admitted(app, &error)) {
        if (app->water_effect_constructing)
            fail(&error, MC_CLIENT_STATISTICS_OWNER_BUSY, "application vs constructor entered",
                 NULL, NULL, NULL, 0);
        else if (app->water_effect != NULL || app->water_effect_retained != NULL)
            fail(&error, MC_CLIENT_STATISTICS_OWNER_INVALID_PHASE, "application vs one-shot owner",
                 NULL, NULL, NULL, 0);
        else {
            app->water_effect_constructing = true;
            if (mc_client_water_texture_allocate(app->registry, app->profile, options,
                                                 &app->water_effect_retained,
                                                 &error.water) != MC_WATER_TEXTURE_OK)
                fail(&error, MC_CLIENT_STATISTICS_OWNER_WATER_FAILED,
                     "Minecraft native vs allocation", NULL, NULL, NULL, 0);
            else if (mc_client_water_texture_construct(app->water_effect_retained, &error.water) !=
                     MC_WATER_TEXTURE_OK)
                fail(&error, MC_CLIENT_STATISTICS_OWNER_WATER_FAILED,
                     "Minecraft actual vs constructor", "net/minecraft/client/Minecraft", "<init>",
                     "(Ljava/awt/Component;Ljava/awt/Canvas;Lnet/minecraft/client/"
                     "MinecraftApplet;IIZ)V",
                     119);
            else
                app->water_effect = app->water_effect_retained; /* actual ad122 */
            app->water_effect_constructing = false;
        }
    }
    if (out != NULL)
        *out = error;
    return error.status;
}

mc_client_statistics_owner_status
mc_client_application_retire_water_effect(client_application *app,
                                          mc_client_statistics_owner_error *out) {
    mc_client_statistics_owner_error error = {0};
    if (app == NULL)
        fail(&error, MC_CLIENT_STATISTICS_OWNER_INVALID_ARGUMENT,
             "application water retirement owner", NULL, NULL, NULL, 0);
    else if (app->water_effect_constructing)
        fail(&error, MC_CLIENT_STATISTICS_OWNER_BUSY, "application vs constructor entered", NULL,
             NULL, NULL, 0);
    else if (app->water_effect != NULL && app->water_effect != app->water_effect_retained)
        fail(&error, MC_CLIENT_STATISTICS_OWNER_INVALID_PHASE,
             "application ad is not retained native owner", NULL, NULL, NULL, 0);
    else {
        mc_client_water_texture_effect *previous = app->water_effect_retained;
        if (mc_client_water_texture_retire(&app->water_effect_retained, &error.water) !=
            MC_WATER_TEXTURE_OK)
            fail(&error, MC_CLIENT_STATISTICS_OWNER_WATER_FAILED,
                 "application water texture retirement", NULL, NULL, NULL, 0);
        if (app->water_effect_retained == NULL && app->water_effect == previous)
            app->water_effect = NULL;
    }
    if (out != NULL)
        *out = error;
    return error.status;
}

mc_client_statistics_owner_status
mc_client_application_statistics_active_use(client_application *app,
                                            mc_client_statistics_owner_error *out) {
    mc_client_statistics_owner_error error = {0};
    if (admitted(app, &error)) {
        /* Actual jl.a()V is return0. Its invokestatic actively initializes jl. */
        if (mc_registry_classes_initialize_statistics(app->registry, &error.registry) !=
            MC_REGISTRY_OK)
            fail(
                &error, MC_CLIENT_STATISTICS_OWNER_REGISTRY_FAILED,
                "Minecraft constructor jl.a active use", "net/minecraft/client/Minecraft", "<init>",
                "(Ljava/awt/Component;Ljava/awt/Canvas;Lnet/minecraft/client/MinecraftApplet;IIZ)V",
                186);
        else
            app->statistics_active_use_completed = true;
    }
    if (out != NULL)
        *out = error;
    return error.status;
}

mc_client_statistics_owner_status
mc_client_application_publish_session(client_application *app, mc_player_session *session,
                                      mc_client_statistics_owner_error *out) {
    mc_client_statistics_owner_error error = {0};
    if (admitted(app, &error)) {
        if (session != NULL && (!app->statistics_active_use_completed ||
                                !mc_player_session_matches(session, app->entities, app->registry)))
            fail(&error,
                 app->statistics_active_use_completed ? MC_CLIENT_STATISTICS_OWNER_INVALID_ARGUMENT
                                                      : MC_CLIENT_STATISTICS_OWNER_INVALID_PHASE,
                 "application Session binding/constructor order", NULL, NULL, NULL, 0);
        else {
            const mc_player_status status =
                session == NULL ? MC_PLAYER_OK : mc_player_session_retain(session);
            if (status != MC_PLAYER_OK) {
                error.session.status = status;
                fail(&error, MC_CLIENT_STATISTICS_OWNER_SESSION_FAILED, "application Session hold",
                     NULL, NULL, NULL, 0);
            } else {
                mc_player_session *previous = app->session;
                app->session = session;
                mc_player_session_retire(&previous);
            }
        }
    }
    if (out != NULL)
        *out = error;
    return error.status;
}

mc_client_statistics_owner_status
mc_client_application_create_session(client_application *app, mc_player_string *username,
                                     mc_player_string *session_id,
                                     mc_client_statistics_owner_error *out) {
    mc_client_statistics_owner_error error = {0};
    if (admitted(app, &error)) {
        if (!app->statistics_active_use_completed)
            fail(&error, MC_CLIENT_STATISTICS_OWNER_INVALID_PHASE,
                 "application Session before constructor jl.a", NULL, NULL, NULL, 0);
        else if (app->session_constructing)
            fail(&error, MC_CLIENT_STATISTICS_OWNER_BUSY, "application Session constructor entered",
                 NULL, NULL, NULL, 0);
        else {
            app->session_constructing = true;
            mc_player_error *cause = malloc(sizeof(*cause));
            if (cause == NULL) {
                fail(&error, MC_CLIENT_STATISTICS_OWNER_ALLOCATION_FAILED,
                     "application native Session diagnostic storage", NULL, NULL, NULL, 0);
            } else {
                memset(cause, 0, sizeof(*cause));
                if (app->session_classes == NULL &&
                    mc_player_session_classes_create(app->entities, app->registry,
                                                     &app->session_classes,
                                                     cause) != MC_PLAYER_OK) {
                    session_cause(&error, cause);
                    fail(&error, MC_CLIENT_STATISTICS_OWNER_SESSION_FAILED,
                         "application Session class ownership", NULL, NULL, NULL, 0);
                } else {
                    mc_player_session *candidate = NULL;
                    if (mc_player_session_create(app->session_classes, username, session_id,
                                                 &candidate, cause) != MC_PLAYER_OK) {
                        session_cause(&error, cause);
                        error.status = MC_CLIENT_STATISTICS_OWNER_SESSION_FAILED;
                        error.operation = "application actual gr constructor";
                        error.site = error.session.site;
                    } else
                        (void)mc_client_application_publish_session(app, candidate, &error);
                    mc_player_session_retire(&candidate);
                }
                free(cause);
            }
            app->session_constructing = false;
        }
    }
    if (out != NULL)
        *out = error;
    return error.status;
}

static mc_client_statistics_owner_status
construct_statistics(client_application *app, mc_client_stats_classes *classes,
                     mc_java_files *files, bool source_mode, const mc_client_stats_options *options,
                     mc_client_stats_report *out_report, mc_client_statistics_owner_error *out) {
    mc_client_statistics_owner_error error = {0};
    mc_client_stats_report report = {0};
    if (admitted(app, &error)) {
        if (!app->statistics_active_use_completed || app->stats_writer_retained != NULL ||
            app->stats_writer != NULL)
            fail(&error, MC_CLIENT_STATISTICS_OWNER_INVALID_PHASE,
                 "application statistics constructor order/one-shot owner", NULL, NULL, NULL, 0);
        else if (app->stats_writer_constructing)
            fail(&error, MC_CLIENT_STATISTICS_OWNER_BUSY, "application xi constructor entered",
                 NULL, NULL, NULL, 0);
        else {
            app->stats_writer_constructing = true;
            if (mc_client_stats_writer_allocate(app->registry, options, &app->stats_writer_retained,
                                                &error.writer) != MC_CLIENT_STATS_OK)
                fail(&error, MC_CLIENT_STATISTICS_OWNER_WRITER_FAILED,
                     "application xi native allocation", NULL, NULL, NULL, 0);
            else {
                mc_player_session *session = app->session;  /* k330, argument evaluation. */
                mc_java_file *parent = app->game_directory; /* Z334, after k330. */
                const char *directory =
                    source_mode ? NULL
                                : mc_client_game_directory_path(app->diagnostic_game_directory);
                /* The legacy diagnostic caller keeps its old explicit boundary.
                 * Source adoption never substitutes that path for actual Z. */
                if (!source_mode && parent != NULL) {
                    fail(&error, MC_CLIENT_STATISTICS_OWNER_UNIMPLEMENTED,
                         "actual xi(File) typed argument adapter", "net/minecraft/client/Minecraft",
                         "a", "()V", 337);
                    app->stats_writer_constructing = false;
                    if (out_report != NULL)
                        *out_report = report;
                    if (out != NULL)
                        *out = error;
                    return error.status;
                }
                const mc_client_stats_status constructed =
                    source_mode
                        ? mc_client_stats_writer_construct_file(app->stats_writer_retained, classes,
                                                                session, files, parent, &report,
                                                                &error.writer)
                        : mc_client_stats_writer_construct(app->stats_writer_retained, session,
                                                           directory, &report, &error.writer);
                if (constructed != MC_CLIENT_STATS_OK) {
                    if (source_mode) {
                        error.source_writer.base = error.writer;
                        mc_client_stats_file_state source_state;
                        if (mc_client_stats_writer_read_file_state(app->stats_writer_retained,
                                                                   &source_state))
                            error.source_writer = *source_state.failure;
                    }
                    fail(&error, MC_CLIENT_STATISTICS_OWNER_WRITER_FAILED,
                         "Minecraft startup xi constructor", "net/minecraft/client/Minecraft", "a",
                         "()V", 337);
                } else {
                    app->stats_writer = app->stats_writer_retained; /* I340, before kh. */
                    if (mc_client_application_install_inventory_callback(app, &error.notice) !=
                        MC_CLIENT_NOTICE_OK)
                        fail(&error, MC_CLIENT_STATISTICS_OWNER_NOTICE_FAILED,
                             "Minecraft startup kh installation", "net/minecraft/client/Minecraft",
                             "a", "()V",
                             error.notice.source_pc >= 0 ? (unsigned int)error.notice.source_pc
                                                         : 354u);
                }
            }
            app->stats_writer_constructing = false;
        }
    }
    if (out_report != NULL)
        *out_report = report;
    if (out != NULL)
        *out = error;
    return error.status;
}

mc_client_statistics_owner_status mc_client_application_construct_statistics(
    client_application *app, const mc_client_stats_options *options, mc_client_stats_report *report,
    mc_client_statistics_owner_error *error) {
    return construct_statistics(app, NULL, NULL, false, options, report, error);
}

mc_client_statistics_owner_status mc_client_application_construct_statistics_file(
    client_application *app, mc_client_stats_classes *classes, mc_java_files *files,
    const mc_client_stats_options *options, mc_client_stats_report *report,
    mc_client_statistics_owner_error *error) {
    return construct_statistics(app, classes, files, true, options, report, error);
}

mc_client_statistics_owner_status
mc_client_application_retire_statistics(client_application *app,
                                        mc_client_statistics_owner_error *out) {
    mc_client_statistics_owner_error error = {0};
    if (app == NULL)
        fail(&error, MC_CLIENT_STATISTICS_OWNER_INVALID_ARGUMENT,
             "application statistics retirement owner", NULL, NULL, NULL, 0);
    else if (app->statistics_retiring || app->session_constructing ||
             app->stats_writer_constructing)
        fail(&error, MC_CLIENT_STATISTICS_OWNER_BUSY, "application statistics operation entered",
             NULL, NULL, NULL, 0);
    else if (app->stats_writer != NULL && app->stats_writer != app->stats_writer_retained)
        fail(&error, MC_CLIENT_STATISTICS_OWNER_INVALID_PHASE,
             "application I is not this native retained owner", NULL, NULL, NULL, 0);
    else {
        app->statistics_retiring = true;
        mc_client_stats_writer *previous = app->stats_writer_retained;
        const mc_client_stats_status status =
            mc_client_stats_writer_retire_file(&app->stats_writer_retained, &error.source_writer);
        error.writer = error.source_writer.base;
        if (status != MC_CLIENT_STATS_OK)
            fail(&error, MC_CLIENT_STATISTICS_OWNER_WRITER_FAILED,
                 "application writer joined retirement", NULL, NULL, NULL, 0);
        if (app->stats_writer_retained == NULL) {
            if (app->stats_writer == previous)
                app->stats_writer = NULL;
            mc_player_session_retire(&app->session);
            mc_player_error *cause = malloc(sizeof(*cause));
            if (cause == NULL) {
                if (error.status == MC_CLIENT_STATISTICS_OWNER_OK)
                    fail(&error, MC_CLIENT_STATISTICS_OWNER_ALLOCATION_FAILED,
                         "application native retirement diagnostic storage", NULL, NULL, NULL, 0);
            } else {
                memset(cause, 0, sizeof(*cause));
                if (mc_player_session_classes_free(&app->session_classes, cause) != MC_PLAYER_OK) {
                    session_cause(&error, cause);
                    if (error.status == MC_CLIENT_STATISTICS_OWNER_OK)
                        fail(&error, MC_CLIENT_STATISTICS_OWNER_SESSION_FAILED,
                             "application Session class retirement", NULL, NULL, NULL, 0);
                } else
                    app->statistics_retired = true;
                free(cause);
            }
        }
        app->statistics_retiring = false;
    }
    if (out != NULL)
        *out = error;
    return error.status;
}
