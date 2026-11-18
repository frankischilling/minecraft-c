#include "application_queries.h"

#include "../settings/source_access.h"
#include "application_owner_internal.h"

static mc_client_application_query_status fail(mc_client_application_query_error *error,
                                               mc_client_application_query_status status,
                                               const char *operation, unsigned int pc,
                                               bool native_policy) {
    error->status = status;
    error->operation = operation;
    error->site =
        (mc_client_application_owner_site){"net/minecraft/client/Minecraft", "v", "()Z", pc};
    error->native_policy = native_policy;
    return status;
}

mc_client_application_query_status
mc_client_application_ambient_occlusion(mc_client_application_classes *classes, bool *out,
                                        mc_client_application_query_error *error) {
    mc_client_application_query_error e = {0};
    bool held = false, value = false;
    if (classes == NULL || out == NULL) {
        fail(&e, MC_CLIENT_APPLICATION_QUERY_INVALID_ARGUMENT, "Minecraft.v native arguments", 0,
             true);
        goto done;
    }
    e.owner.status = mc_client_application_classes_retain(classes);
    if (e.owner.status != MC_CLIENT_APPLICATION_OWNER_OK) {
        fail(&e, MC_CLIENT_APPLICATION_QUERY_NATIVE_FAILED, "Minecraft.v namespace hold", 0, true);
        goto done;
    }
    held = true;
    if (mc_client_application_classes_active_use(classes, &e.owner) !=
        MC_CLIENT_APPLICATION_OWNER_OK) {
        fail(&e, MC_CLIENT_APPLICATION_QUERY_NATIVE_FAILED, "Minecraft.v defining class active use",
             0, true);
        goto done;
    }
    client_application *first;
    if (!application_static_app(classes, &first)) {
        fail(&e, MC_CLIENT_APPLICATION_QUERY_NATIVE_FAILED, "Minecraft.v getstatic0 native view", 0,
             true);
        goto done;
    }
    if (first != NULL) {
        client_application *second;
        if (!application_static_app(classes, &second)) {
            fail(&e, MC_CLIENT_APPLICATION_QUERY_NATIVE_FAILED,
                 "Minecraft.v getstatic6 native view", 6, true);
            goto done;
        }
        if (second == NULL) {
            fail(&e, MC_CLIENT_APPLICATION_QUERY_SOURCE_NULL, "Minecraft.v App.z receiver", 9,
                 false);
            goto done;
        }
        mc_game_settings *settings = second->settings;
        if (settings == NULL) {
            fail(&e, MC_CLIENT_APPLICATION_QUERY_SOURCE_NULL, "Minecraft.v Settings.k receiver", 12,
                 false);
            goto done;
        }
        if (!mc_game_settings_read_ambient_occlusion(settings, &value)) {
            fail(&e, MC_CLIENT_APPLICATION_QUERY_NATIVE_FAILED, "Minecraft.v Settings native view",
                 12, true);
            goto done;
        }
    }
    e.site = (mc_client_application_owner_site){"net/minecraft/client/Minecraft", "v", "()Z", 23};
    e.source_returned = true;
    *out = value;
done:
    if (held) {
        /* The caller retains the namespace; this balanced occurrence cannot
         * perform final class/App retirement or change source a. */
        mc_client_application_classes *occurrence = classes;
        (void)mc_client_application_classes_release(&occurrence, NULL);
    }
    if (error != NULL)
        *error = e;
    return e.status;
}
