#include "../application_access.h"
#include "description_registry.h"
#include "mc/client/settings/game_option.h"

static mc_client_notice_status result(mc_client_notice_error *e, mc_client_notice_status s,
                                      const char *source, int32_t pc) {
    if (e != NULL)
        *e = (mc_client_notice_error){
            .status = s, .source_class = source, .source_pc = pc, .notice_pc = -1};
    return s;
}

mc_client_notice_status mc_client_achievement_description(mc_registry_classes *c,
                                                          const mc_statistic *s,
                                                          mc_client_description_result *out,
                                                          mc_client_notice_error *error) {
    mc_registry_error registry = {0};
    if (out == NULL)
        return result(error, MC_CLIENT_NOTICE_INVALID_ARGUMENT, NULL, -1);
    if (mc_client_achievement_admit(c, s, &registry) != MC_REGISTRY_OK)
        goto registry_failure;
    if (s == NULL)
        return result(error, MC_CLIENT_NOTICE_SOURCE_EXCEPTION, "ny", 1);
    const mc_achievement_fields *fields = mc_statistic_achievement_fields_get(s);
    if (fields->client_callback == NULL) {
        *out = (mc_client_description_result){.string = fields->l};
        return result(error, MC_CLIENT_NOTICE_OK, NULL, -1);
    }
    client_application *app = NULL;
    if (mc_client_inventory_callback_client(c, fields->client_callback, &app, &registry) !=
        MC_REGISTRY_OK)
        goto registry_failure;
    if (app == NULL)
        return result(error, MC_CLIENT_NOTICE_SOURCE_EXCEPTION, "kh", 11);
    mc_game_settings *settings = mc_client_application_live_settings(app);
    if (settings == NULL)
        return result(error, MC_CLIENT_NOTICE_SOURCE_EXCEPTION, "kh", 14);
    mc_game_settings_state live;
    if (!mc_game_settings_read_state(settings, &live))
        return result(error, MC_CLIENT_NOTICE_INVALID_ARGUMENT, NULL, -1);
    const mc_key_binding *binding = live.named[5]; /* kv.r; independent of current w. */
    if (binding == NULL)
        return result(error, MC_CLIENT_NOTICE_SOURCE_EXCEPTION, "kh", 17);
    int32_t code;
    mc_game_settings_text name;
    if (!mc_key_binding_read(binding, &name, &code))
        return result(error, MC_CLIENT_NOTICE_INVALID_ARGUMENT, NULL, -1);
    const char *key = NULL;
    if (mc_game_key_name(code, &key) != MC_GAME_OPTION_OK)
        return result(error, MC_CLIENT_NOTICE_SOURCE_EXCEPTION, "Keyboard", 4);
    if (mc_client_description_format(c, fields->l, key, out, &registry) != MC_REGISTRY_OK) {
        (void)result(error, MC_CLIENT_NOTICE_REGISTRY_FAILED, "kh", 24);
        if (error != NULL) {
            error->registry = registry;
            error->format = registry.format_error;
        }
        return MC_CLIENT_NOTICE_REGISTRY_FAILED;
    }
    return result(error, MC_CLIENT_NOTICE_OK, NULL, -1);
registry_failure:
    (void)result(error, MC_CLIENT_NOTICE_REGISTRY_FAILED, "ny", 15);
    if (error != NULL)
        error->registry = registry;
    return MC_CLIENT_NOTICE_REGISTRY_FAILED;
}
