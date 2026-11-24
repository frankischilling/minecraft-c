#include "application_owner_internal.h"
#include "mc/entity/player/player.h"

mc_client_application_owner_status
mc_client_application_namespace_attach(mc_client_application_classes *classes,
                                       client_application *app,
                                       mc_client_application_owner_error *error) {
    if (classes == NULL || app == NULL ||
        app->native_lifetime_phase != MC_CLIENT_APPLICATION_LIFETIME_REGISTERED ||
        app->constructor_phase != MC_CLIENT_APP_RAW || app->source_namespace_borrowed)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_INVALID_PHASE,
                                      "Minecraft native namespace borrow admission", NULL, NULL,
                                      NULL, 0, 0);
    if (mtx_lock(&classes->mutex) != thrd_success)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_THREAD_FAILED,
                                      "Minecraft native namespace borrow lock", NULL, NULL, NULL, 0,
                                      0);
    if (classes->native_retiring || classes->phase != MC_CLIENT_APPLICATION_CLASS_READY) {
        (void)mtx_unlock(&classes->mutex);
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_INVALID_PHASE,
                                      "Minecraft native namespace ready admission", NULL, NULL,
                                      NULL, 0, 0);
    }
    if (classes->native_app_borrows == SIZE_MAX) {
        (void)mtx_unlock(&classes->mutex);
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_LIMIT,
                                      "Minecraft native namespace borrow limit", NULL, NULL, NULL,
                                      0, 0);
    }
    ++classes->native_app_borrows;
    app->source_namespace = classes;
    app->source_namespace_borrowed = true;
    (void)mtx_unlock(&classes->mutex);
    if (error != NULL)
        *error = (mc_client_application_owner_error){0};
    return MC_CLIENT_APPLICATION_OWNER_OK;
}

mc_client_application_owner_status
mc_client_application_namespace_detach(client_application *app,
                                       mc_client_application_owner_error *error) {
    if (app == NULL || app->constructor_phase != MC_CLIENT_APP_RETIRING)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_INVALID_PHASE,
                                      "Minecraft detach requires native source stop", NULL, NULL,
                                      NULL, 0, 0);
    if (!app->source_namespace_borrowed) {
        if (error != NULL)
            *error = (mc_client_application_owner_error){0};
        return MC_CLIENT_APPLICATION_OWNER_OK;
    }
    mc_client_application_classes *classes = app->source_namespace;
    if (classes == NULL || mtx_lock(&classes->mutex) != thrd_success)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_THREAD_FAILED,
                                      "Minecraft namespace detach lock", NULL, NULL, NULL, 0, 0);
    if (classes->native_app_borrows == 0) {
        (void)mtx_unlock(&classes->mutex);
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_INVALID_PHASE,
                                      "Minecraft namespace borrow accounting", NULL, NULL, NULL, 0,
                                      0);
    }
    --classes->native_app_borrows;
    app->source_namespace_borrowed = false;
    app->source_namespace = NULL; /* Native link only; not source global a. */
    (void)mtx_unlock(&classes->mutex);
    if (error != NULL)
        *error = (mc_client_application_owner_error){0};
    return MC_CLIENT_APPLICATION_OWNER_OK;
}

mc_client_application_owner_status
mc_client_application_classes_timer_name(mc_client_application_classes *classes,
                                         mc_player_string **out,
                                         mc_client_application_owner_error *error) {
    if (classes == NULL || out == NULL)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_INVALID_ARGUMENT,
                                      "Minecraft Thread literal output", NULL, NULL, NULL, 0, 0);
    if (mtx_lock(&classes->mutex) != thrd_success)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_THREAD_FAILED,
                                      "Minecraft Thread literal lock", NULL, NULL, NULL, 0, 0);
    if (classes->native_retiring || classes->phase != MC_CLIENT_APPLICATION_CLASS_READY) {
        (void)mtx_unlock(&classes->mutex);
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_INVALID_PHASE,
                                      "Minecraft Thread literal class state", NULL, NULL, NULL, 0,
                                      0);
    }
    if (classes->timer_name == NULL) {
        static const uint16_t literal[] = {'T', 'i', 'm', 'e', 'r', ' ', 'h', 'a', 'c',
                                           'k', ' ', 't', 'h', 'r', 'e', 'a', 'd'};
        const mc_player_status status = mc_player_string_create(
            literal, sizeof(literal) / sizeof(literal[0]), &classes->timer_name);
        if (status != MC_PLAYER_OK) {
            (void)mtx_unlock(&classes->mutex);
            return application_owner_fail(
                error, MC_CLIENT_APPLICATION_OWNER_STRING_FAILED,
                "Minecraft actual ldc Thread name", "net/minecraft/client/Minecraft", "<init>",
                "(Ljava/awt/Component;Ljava/awt/Canvas;Lnet/minecraft/client/MinecraftApplet;IIZ)V",
                211, (int)status);
        }
    }
    *out = classes->timer_name;
    (void)mtx_unlock(&classes->mutex);
    if (error != NULL)
        *error = (mc_client_application_owner_error){0};
    return MC_CLIENT_APPLICATION_OWNER_OK;
}

static mc_client_application_owner_status applet_literal(mc_client_application_classes *classes,
                                                         bool standalone, mc_player_string **out,
                                                         mc_client_application_owner_error *error) {
    if (classes == NULL || out == NULL)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_INVALID_ARGUMENT,
                                      "Minecraft Applet literal output", NULL, NULL, NULL, 0, 0);
    if (mtx_lock(&classes->mutex) != thrd_success)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_THREAD_FAILED,
                                      "Minecraft Applet literal lock", NULL, NULL, NULL, 0, 0);
    if (classes->native_retiring || classes->phase != MC_CLIENT_APPLICATION_CLASS_READY) {
        (void)mtx_unlock(&classes->mutex);
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_INVALID_PHASE,
                                      "Minecraft Applet literal class state", NULL, NULL, NULL, 0,
                                      0);
    }
    mc_player_string **slot = standalone ? &classes->applet_standalone : &classes->applet_true;
    if (*slot == NULL) {
        static const uint16_t true_units[] = {'t', 'r', 'u', 'e'};
        static const uint16_t standalone_units[] = {'s', 't', 'a', 'n', 'd', '-',
                                                    'a', 'l', 'o', 'n', 'e'};
        const mc_player_status status = mc_player_string_create(
            standalone ? standalone_units : true_units,
            standalone ? sizeof(standalone_units) / sizeof(standalone_units[0])
                       : sizeof(true_units) / sizeof(true_units[0]),
            slot);
        if (status != MC_PLAYER_OK) {
            (void)mtx_unlock(&classes->mutex);
            return application_owner_fail(
                error, MC_CLIENT_APPLICATION_OWNER_STRING_FAILED,
                "Minecraft actual Applet ldc native resolution", "net/minecraft/client/Minecraft",
                "<init>",
                "(Ljava/awt/Component;Ljava/awt/Canvas;Lnet/minecraft/client/MinecraftApplet;IIZ)V",
                standalone ? 247 : 244, (int)status);
        }
    }
    *out = *slot;
    (void)mtx_unlock(&classes->mutex);
    if (error != NULL)
        *error = (mc_client_application_owner_error){0};
    return MC_CLIENT_APPLICATION_OWNER_OK;
}

mc_client_application_owner_status
mc_client_application_classes_applet_true(mc_client_application_classes *classes,
                                          mc_player_string **out,
                                          mc_client_application_owner_error *error) {
    return applet_literal(classes, false, out, error);
}

mc_client_application_owner_status
mc_client_application_classes_applet_standalone(mc_client_application_classes *classes,
                                                mc_player_string **out,
                                                mc_client_application_owner_error *error) {
    return applet_literal(classes, true, out, error);
}

mc_client_application_owner_status
mc_client_application_classes_publish(mc_client_application_classes *classes,
                                      client_application *app,
                                      mc_client_application_owner_error *error) {
    if (classes == NULL || app == NULL || !app->source_namespace_borrowed ||
        app->source_namespace != classes || app->constructor_phase != MC_CLIENT_APP_CONSTRUCTING)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_INVALID_PHASE,
                                      "Minecraft actual a264 publication admission", NULL, NULL,
                                      NULL, 0, 0);
    if (mtx_lock(&classes->mutex) != thrd_success)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_THREAD_FAILED,
                                      "Minecraft a264 namespace lock", NULL, NULL, NULL, 0, 0);
    if (classes->native_retiring || classes->phase != MC_CLIENT_APPLICATION_CLASS_READY) {
        (void)mtx_unlock(&classes->mutex);
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_INVALID_PHASE,
                                      "Minecraft a264 namespace not ready", NULL, NULL, NULL, 0, 0);
    }
    const mc_client_application_owner_status status = application_static_hold(app, error);
    if (status != MC_CLIENT_APPLICATION_OWNER_OK) {
        (void)mtx_unlock(&classes->mutex);
        return status;
    }
    client_application *old = classes->a;
    classes->a = app; /* Actual putstatic264, including same-identity replacement. */
    classes->last_global_store_pc = 264;
    if (old != NULL)
        application_static_release(old);
    (void)mtx_unlock(&classes->mutex);
    if (error != NULL)
        *error = (mc_client_application_owner_error){0};
    return MC_CLIENT_APPLICATION_OWNER_OK;
}
