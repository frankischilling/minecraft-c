#include "application_state.h"

mc_game_settings *mc_client_application_live_settings(const client_application *app) {
    return app == NULL ? NULL : app->settings;
}

mc_client_stats_writer *mc_client_application_live_stats_writer(const client_application *app) {
    return app == NULL ? NULL : app->stats_writer;
}

mc_client_achievement_notice *
mc_client_application_live_achievement_notice(const client_application *app) {
    return app == NULL ? NULL : app->achievement_notice;
}

/* Place this after real StatsWriter construction/publication at the eventual
 * normal startup call site. It is deliberately not called by the menu-only
 * initializer, which has not yet constructed Minecraft.I. */
mc_client_notice_status
mc_client_application_install_inventory_callback(client_application *app,
                                                 mc_client_notice_error *error) {
    mc_registry_error cause = {0};
    if (app == NULL || app->registry == NULL) {
        if (error != NULL)
            *error = (mc_client_notice_error){.status = MC_CLIENT_NOTICE_INVALID_ARGUMENT};
        return MC_CLIENT_NOTICE_INVALID_ARGUMENT;
    }
    mc_registry_achievement_view view = {0};
    const mc_achievement_description_callback *callback = NULL;
    int32_t pc = 343;
    mc_registry_status status = mc_registry_classes_initialize_achievements(app->registry, &cause);
    if (status == MC_REGISTRY_OK)
        status = mc_registry_classes_inspect_achievements(app->registry, &view, &cause);
    if (status == MC_REGISTRY_OK) {
        pc = 351;
        status = mc_client_inventory_callback_create(app->registry, app, &callback, &cause);
    }
    if (status == MC_REGISTRY_OK) {
        pc = 354;
        status = mc_client_achievement_callback_set(app->registry, view.named[0], callback, &cause);
    }
    if (status != MC_REGISTRY_OK) {
        if (error != NULL)
            *error = (mc_client_notice_error){.status = MC_CLIENT_NOTICE_REGISTRY_FAILED,
                                              .source_class = "net/minecraft/client/Minecraft",
                                              .source_pc = pc,
                                              .notice_pc = -1,
                                              .registry = cause};
        return MC_CLIENT_NOTICE_REGISTRY_FAILED;
    }
    if (error != NULL)
        *error = (mc_client_notice_error){0};
    return MC_CLIENT_NOTICE_OK;
}
