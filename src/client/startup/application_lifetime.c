#include "application_owner_internal.h"

mc_client_application_owner_status
mc_client_application_lifetime_register(client_application *app,
                                        mc_client_application_owner_error *error) {
    if (error != NULL)
        *error = (mc_client_application_owner_error){0};
    if (app == NULL)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_INVALID_ARGUMENT,
                                      "Minecraft native lifetime owner", NULL, NULL, NULL, 0, 0);
    if (app->native_lifetime_phase != MC_CLIENT_APPLICATION_LIFETIME_RAW)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_INVALID_PHASE,
                                      "Minecraft native lifetime already registered", NULL, NULL,
                                      NULL, 0, 0);
    atomic_init(&app->J, false);
    atomic_init(&app->o, false);
    atomic_init(&app->timer_hack_holds, 0);
    atomic_init(&app->source_static_holds, 0);
    atomic_init(&app->callback_holds, 0);
    atomic_init(&app->effect_borrows, 0);
    atomic_init(&app->resource_thread_borrows, 0);
    atomic_init(&app->screen_borrows, 0);
    app->native_lifetime_phase = MC_CLIENT_APPLICATION_LIFETIME_REGISTERED;
    return MC_CLIENT_APPLICATION_OWNER_OK;
}

mc_client_application_owner_status
mc_client_application_lifetime_retire(client_application *app,
                                      mc_client_application_owner_error *error) {
    if (error != NULL)
        *error = (mc_client_application_owner_error){0};
    if (app == NULL)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_INVALID_ARGUMENT,
                                      "Minecraft native retirement owner", NULL, NULL, NULL, 0, 0);
    if (app->native_lifetime_phase == MC_CLIENT_APPLICATION_LIFETIME_RETIRED)
        return MC_CLIENT_APPLICATION_OWNER_OK;
    if (app->native_lifetime_phase != MC_CLIENT_APPLICATION_LIFETIME_REGISTERED)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_INVALID_PHASE,
                                      "Minecraft native lifetime not registered", NULL, NULL, NULL,
                                      0, 0);
    if (app->startup != NULL)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_BUSY,
                                      "Minecraft attached startup record", NULL, NULL, NULL, 0, 0);
    if (atomic_load_explicit(&app->resource_thread_borrows, memory_order_seq_cst) != 0)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_BUSY,
                                      "Minecraft borrowed by actual resource thread", NULL, NULL,
                                      NULL, 0, 0);
    if (atomic_load_explicit(&app->screen_borrows, memory_order_seq_cst) != 0)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_BUSY,
                                      "Minecraft borrowed by actual screen or GUI particles", NULL,
                                      NULL, NULL, 0, 0);
    if (atomic_load_explicit(&app->timer_hack_holds, memory_order_seq_cst) != 0)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_BUSY,
                                      "Minecraft retained by Timer hack owner", NULL, NULL, NULL, 0,
                                      0);
    if (atomic_load_explicit(&app->callback_holds, memory_order_seq_cst) != 0)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_BUSY,
                                      "Minecraft retained by actual kh native borrow", NULL, NULL,
                                      NULL, 0, 0);
    if (atomic_load_explicit(&app->effect_borrows, memory_order_seq_cst) != 0)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_BUSY,
                                      "Minecraft borrowed by actual compass/watch effects", NULL,
                                      NULL, NULL, 0, 0);
    if (app->source_namespace_borrowed ||
        atomic_load_explicit(&app->source_static_holds, memory_order_seq_cst) != 0)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_BUSY,
                                      "Minecraft namespace/global App holds", NULL, NULL, NULL, 0,
                                      0);
    app->native_lifetime_phase = MC_CLIENT_APPLICATION_LIFETIME_RETIRED;
    return MC_CLIENT_APPLICATION_OWNER_OK;
}

bool mc_client_application_volatile_inspect(const client_application *app,
                                            mc_client_application_volatile_state *out) {
    if (app == NULL || out == NULL ||
        app->native_lifetime_phase == MC_CLIENT_APPLICATION_LIFETIME_RAW)
        return false;
    *out = (mc_client_application_volatile_state){
        .J = atomic_load_explicit(&app->J, memory_order_seq_cst),
        .o = atomic_load_explicit(&app->o, memory_order_seq_cst),
        .native_worker_holds = atomic_load_explicit(&app->timer_hack_holds, memory_order_seq_cst),
        .native_static_holds =
            atomic_load_explicit(&app->source_static_holds, memory_order_seq_cst),
        .native_callback_holds = atomic_load_explicit(&app->callback_holds, memory_order_seq_cst),
        .native_effect_borrows = atomic_load_explicit(&app->effect_borrows, memory_order_seq_cst),
        .native_resource_thread_borrows =
            atomic_load_explicit(&app->resource_thread_borrows, memory_order_seq_cst),
        .native_screen_borrows = atomic_load_explicit(&app->screen_borrows, memory_order_seq_cst)};
    return true;
}

mc_client_application_owner_status
application_worker_hold(client_application *app, mc_client_application_owner_error *error) {
    if (app == NULL || app->native_lifetime_phase != MC_CLIENT_APPLICATION_LIFETIME_REGISTERED ||
        app->constructor_phase == MC_CLIENT_APP_RETIRING)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_INVALID_PHASE,
                                      "Timer hack requires registered actual Minecraft", NULL, NULL,
                                      NULL, 0, 0);
    size_t holds = atomic_load_explicit(&app->timer_hack_holds, memory_order_seq_cst);
    do {
        if (holds == SIZE_MAX)
            return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_LIMIT,
                                          "Timer hack actual Minecraft hold count", NULL, NULL,
                                          NULL, 0, 0);
    } while (!atomic_compare_exchange_weak_explicit(&app->timer_hack_holds, &holds, holds + 1u,
                                                    memory_order_seq_cst, memory_order_seq_cst));
    return MC_CLIENT_APPLICATION_OWNER_OK;
}

void application_worker_release(client_application *app) {
    (void)atomic_fetch_sub_explicit(&app->timer_hack_holds, 1u, memory_order_seq_cst);
}

mc_client_application_owner_status
application_static_hold(client_application *app, mc_client_application_owner_error *error) {
    if (app == NULL || app->native_lifetime_phase != MC_CLIENT_APPLICATION_LIFETIME_REGISTERED ||
        app->constructor_phase != MC_CLIENT_APP_CONSTRUCTING)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_INVALID_PHASE,
                                      "Minecraft global publication actual App", NULL, NULL, NULL,
                                      0, 0);
    size_t holds = atomic_load_explicit(&app->source_static_holds, memory_order_seq_cst);
    do {
        if (holds == SIZE_MAX)
            return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_LIMIT,
                                          "Minecraft native static hold limit", NULL, NULL, NULL, 0,
                                          0);
    } while (!atomic_compare_exchange_weak_explicit(&app->source_static_holds, &holds, holds + 1u,
                                                    memory_order_seq_cst, memory_order_seq_cst));
    return MC_CLIENT_APPLICATION_OWNER_OK;
}

void application_static_release(client_application *app) {
    (void)atomic_fetch_sub_explicit(&app->source_static_holds, 1u, memory_order_seq_cst);
}
