#include "achievements/description_registry.h"
#include "application_state.h"
#include <assert.h>

mc_game_settings *mc_client_application_live_settings(const client_application *app) {
    return app == NULL ? NULL : app->settings;
}

mc_client_stats_writer *mc_client_application_live_stats_writer(const client_application *app) {
    return app == NULL ? NULL : app->stats_writer;
}

mc_world_runtime *mc_client_application_live_world(const client_application *app) {
    return app == NULL ? NULL : app->active_world;
}

mc_client_effect_renderer *
mc_client_application_live_effect_renderer(const client_application *app) {
    return app == NULL ? NULL : app->effect_renderer;
}

mc_client_achievement_notice *
mc_client_application_live_achievement_notice(const client_application *app) {
    return app == NULL ? NULL : app->achievement_notice;
}

bool mc_client_application_callback_dispatch_admitted(const client_application *app) {
    return app != NULL && app->native_lifetime_phase == MC_CLIENT_APPLICATION_LIFETIME_REGISTERED &&
           app->constructor_phase != MC_CLIENT_APP_RETIRING;
}

bool mc_client_application_effect_borrow(client_application *app, bool *counted) {
    if (counted == NULL)
        return false;
    *counted = false;
    if (app == NULL)
        return true;
    if (app->constructor_phase == MC_CLIENT_APP_RETIRING)
        return false;
    if (app->native_lifetime_phase == MC_CLIENT_APPLICATION_LIFETIME_RAW)
        return true;
    if (app->native_lifetime_phase != MC_CLIENT_APPLICATION_LIFETIME_REGISTERED)
        return false;
    size_t holds = atomic_load_explicit(&app->effect_borrows, memory_order_seq_cst);
    do {
        if (holds == SIZE_MAX)
            return false;
    } while (!atomic_compare_exchange_weak_explicit(&app->effect_borrows, &holds, holds + 1u,
                                                    memory_order_seq_cst, memory_order_seq_cst));
    *counted = true;
    return true;
}

void mc_client_application_effect_release(client_application *app, bool counted) {
    if (!counted)
        return;
    assert(app != NULL);
    const size_t previous =
        atomic_fetch_sub_explicit(&app->effect_borrows, 1u, memory_order_seq_cst);
    assert(previous != 0);
    (void)previous;
}

bool mc_client_application_effect_borrows_pending(const client_application *app) {
    return app != NULL && app->native_lifetime_phase == MC_CLIENT_APPLICATION_LIFETIME_REGISTERED &&
           atomic_load_explicit(&app->effect_borrows, memory_order_seq_cst) != 0;
}

bool mc_client_application_resource_thread_borrow(client_application *app, bool *counted) {
    if (counted == NULL)
        return false;
    *counted = false;
    if (app == NULL)
        return true;
    if (app->constructor_phase == MC_CLIENT_APP_RETIRING)
        return false;
    if (app->native_lifetime_phase == MC_CLIENT_APPLICATION_LIFETIME_RAW)
        return true;
    if (app->native_lifetime_phase != MC_CLIENT_APPLICATION_LIFETIME_REGISTERED)
        return false;
    size_t holds = atomic_load_explicit(&app->resource_thread_borrows, memory_order_seq_cst);
    do {
        if (holds == SIZE_MAX)
            return false;
    } while (!atomic_compare_exchange_weak_explicit(&app->resource_thread_borrows, &holds,
                                                    holds + 1u, memory_order_seq_cst,
                                                    memory_order_seq_cst));
    *counted = true;
    return true;
}

void mc_client_application_resource_thread_release(client_application *app, bool counted) {
    if (!counted)
        return;
    assert(app != NULL);
    const size_t previous =
        atomic_fetch_sub_explicit(&app->resource_thread_borrows, 1u, memory_order_seq_cst);
    assert(previous != 0);
    (void)previous;
}

bool mc_client_application_resource_thread_borrows_pending(const client_application *app) {
    return app != NULL && app->native_lifetime_phase == MC_CLIENT_APPLICATION_LIFETIME_REGISTERED &&
           atomic_load_explicit(&app->resource_thread_borrows, memory_order_seq_cst) != 0;
}

bool mc_client_application_screen_borrow(client_application *app, bool *counted) {
    if (counted == NULL)
        return false;
    *counted = false;
    if (app == NULL)
        return true;
    if (app->constructor_phase == MC_CLIENT_APP_RETIRING)
        return false;
    if (app->native_lifetime_phase == MC_CLIENT_APPLICATION_LIFETIME_RAW)
        return true;
    if (app->native_lifetime_phase != MC_CLIENT_APPLICATION_LIFETIME_REGISTERED)
        return false;
    size_t holds = atomic_load_explicit(&app->screen_borrows, memory_order_seq_cst);
    do {
        if (holds == SIZE_MAX)
            return false;
    } while (!atomic_compare_exchange_weak_explicit(&app->screen_borrows, &holds, holds + 1u,
                                                    memory_order_seq_cst, memory_order_seq_cst));
    *counted = true;
    return true;
}

bool mc_client_application_screen_release(client_application *app, bool counted) {
    if (!counted)
        return true;
    if (app == NULL || app->native_lifetime_phase != MC_CLIENT_APPLICATION_LIFETIME_REGISTERED)
        return false;
    size_t holds = atomic_load_explicit(&app->screen_borrows, memory_order_seq_cst);
    do {
        if (holds == 0)
            return false;
    } while (!atomic_compare_exchange_weak_explicit(&app->screen_borrows, &holds, holds - 1u,
                                                    memory_order_seq_cst, memory_order_seq_cst));
    return true;
}

bool mc_client_application_screen_borrows_pending(const client_application *app) {
    return app != NULL && app->native_lifetime_phase == MC_CLIENT_APPLICATION_LIFETIME_REGISTERED &&
           atomic_load_explicit(&app->screen_borrows, memory_order_seq_cst) != 0;
}

static void callback_release(client_application *app) {
    const size_t previous =
        atomic_fetch_sub_explicit(&app->callback_holds, 1u, memory_order_seq_cst);
    assert(previous != 0);
    (void)previous;
}

mc_client_notice_status mc_client_application_inventory_callback_create_owned(
    client_application *app, const mc_achievement_description_callback **out,
    mc_client_notice_error *error) {
    mc_registry_error cause = {0};
    int32_t pc = 346;
    const mc_achievement_description_callback *callback = NULL;
    if (app == NULL || app->registry == NULL || out == NULL) {
        if (error != NULL)
            *error = (mc_client_notice_error){.status = MC_CLIENT_NOTICE_INVALID_ARGUMENT};
        return MC_CLIENT_NOTICE_INVALID_ARGUMENT;
    }
    mc_registry_status status =
        mc_client_inventory_callback_owned_allocate(app->registry, &callback, &cause);
    if (status == MC_REGISTRY_OK) {
        pc = 351;
        if (!mc_client_application_callback_dispatch_admitted(app)) {
            if (error != NULL)
                *error = (mc_client_notice_error){
                    .status = MC_CLIENT_NOTICE_INVALID_PHASE, .source_pc = -1, .notice_pc = -1};
            return MC_CLIENT_NOTICE_INVALID_PHASE;
        }
        size_t holds = atomic_load_explicit(&app->callback_holds, memory_order_seq_cst);
        do {
            if (holds == SIZE_MAX) {
                if (error != NULL)
                    *error = (mc_client_notice_error){
                        .status = MC_CLIENT_NOTICE_BUSY, .source_pc = -1, .notice_pc = -1};
                return MC_CLIENT_NOTICE_BUSY;
            }
        } while (!atomic_compare_exchange_weak_explicit(
            &app->callback_holds, &holds, holds + 1u, memory_order_seq_cst, memory_order_seq_cst));
        status = mc_client_inventory_callback_owned_finish(app->registry, callback, app,
                                                           callback_release, &cause);
        if (status != MC_REGISTRY_OK)
            callback_release(app); /* Failed finish did not transfer the borrow. */
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
    *out = callback;
    if (error != NULL)
        *error = (mc_client_notice_error){0};
    return MC_CLIENT_NOTICE_OK;
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
    if (app->constructor_phase == MC_CLIENT_APP_RETIRING ||
        (app->native_lifetime_phase != MC_CLIENT_APPLICATION_LIFETIME_RAW &&
         app->native_lifetime_phase != MC_CLIENT_APPLICATION_LIFETIME_REGISTERED)) {
        if (error != NULL)
            *error = (mc_client_notice_error){
                .status = MC_CLIENT_NOTICE_INVALID_PHASE, .source_pc = -1, .notice_pc = -1};
        return MC_CLIENT_NOTICE_INVALID_PHASE;
    }
    mc_registry_achievement_view view = {0};
    const mc_achievement_description_callback *callback = NULL;
    const mc_statistic *receiver = NULL;
    int32_t pc = 343;
    mc_registry_status status = mc_registry_classes_initialize_achievements(app->registry, &cause);
    if (status == MC_REGISTRY_OK)
        status = mc_registry_classes_inspect_achievements(app->registry, &view, &cause);
    if (status == MC_REGISTRY_OK) {
        receiver = view.named[0]; /* Actual getstatic ep.f343, before NEW346. */
        if (app->native_lifetime_phase == MC_CLIENT_APPLICATION_LIFETIME_RAW) {
            /* Existing component/prepared fixture contract: actual App remains
             * live until Registry retirement. No atomic registration is made. */
            pc = 351;
            status = mc_client_inventory_callback_create(app->registry, app, &callback, &cause);
        } else {
            const mc_client_notice_status created =
                mc_client_application_inventory_callback_create_owned(app, &callback, error);
            if (created != MC_CLIENT_NOTICE_OK)
                return created;
        }
    }
    if (status == MC_REGISTRY_OK) {
        pc = 354;
        status = mc_client_achievement_callback_set(app->registry, receiver, callback, &cause);
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
