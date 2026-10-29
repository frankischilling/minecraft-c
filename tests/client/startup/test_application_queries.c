#include "client/startup/application_owner_internal.h"
#include "client/startup/application_queries.h"
#include "mc/client/settings/game_settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "Minecraft query check%u at%d: %s\n", checks, __LINE__, #value);       \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

static void row(mc_client_application_classes *classes, const char *name, bool failure) {
    const bool sentinel = true;
    bool result = sentinel;
    mc_client_application_query_error error;
    mc_client_application_class_state before, after;
    CHECK(mc_client_application_classes_inspect(classes, &before));
    CHECK(mc_client_application_ambient_occlusion(classes, &result, &error) ==
          (failure ? MC_CLIENT_APPLICATION_QUERY_SOURCE_NULL : MC_CLIENT_APPLICATION_QUERY_OK));
    CHECK(mc_client_application_classes_inspect(classes, &after));
    CHECK(after.native_holds == before.native_holds && after.a == before.a);
    CHECK(strcmp(error.site.class_name, "net/minecraft/client/Minecraft") == 0 &&
          strcmp(error.site.method, "v") == 0 && strcmp(error.site.descriptor, "()Z") == 0);
    CHECK(!error.native_policy && error.source_returned == !failure);
    CHECK(error.site.pc == (failure ? 12u : 23u));
    if (failure)
        CHECK(result == sentinel);
    printf("AO %s %s %d %u\n", name, failure ? "NULL" : "OK", failure ? -1 : (result ? 1 : 0),
           error.site.pc);
}

int main(void) {
    mc_client_application_classes *classes = NULL;
    CHECK(mc_client_application_classes_create(NULL, &classes, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    row(classes, "COLD_NULL_APP", false);
    mc_client_application_class_state state;
    CHECK(mc_client_application_classes_inspect(classes, &state));
    CHECK(state.phase == MC_CLIENT_APPLICATION_CLASS_READY && state.b != NULL && state.E != NULL &&
          state.F != NULL && state.a == NULL && state.last_store_pc == 32 &&
          state.array_allocations == 3 && state.native_holds == 1);
    client_application app = {0}, other = {0};
    CHECK(mc_client_application_lifetime_register(&app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_lifetime_register(&other, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_namespace_attach(classes, &app, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_namespace_attach(classes, &other, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    app.constructor_phase = MC_CLIENT_APP_CONSTRUCTING;
    other.constructor_phase = MC_CLIENT_APP_CONSTRUCTING;
    CHECK(mc_client_application_classes_publish(classes, &app, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    row(classes, "NULL_SETTINGS", true);
    mc_game_settings *settings = NULL, *other_settings = NULL;
    CHECK(mc_game_settings_create_defaults(NULL, &settings, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_create_defaults(NULL, &other_settings, NULL) == MC_GAME_SETTINGS_OK);
    app.settings = settings;
    other.settings = other_settings;
    mc_game_settings_state settings_state;
    CHECK(mc_game_settings_read_state(settings, &settings_state));
    settings_state.values.ambient_occlusion = false;
    CHECK(mc_game_settings_set_values(settings, &settings_state.values, NULL) ==
          MC_GAME_SETTINGS_OK);
    row(classes, "OFF", false);
    settings_state.values.ambient_occlusion = true;
    CHECK(mc_game_settings_set_values(settings, &settings_state.values, NULL) ==
          MC_GAME_SETTINGS_OK);
    row(classes, "ON", false);
    CHECK(mc_game_settings_read_state(other_settings, &settings_state));
    settings_state.values.ambient_occlusion = false;
    CHECK(mc_game_settings_set_values(other_settings, &settings_state.values, NULL) ==
          MC_GAME_SETTINGS_OK);
    CHECK(mc_client_application_classes_publish(classes, &other, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    row(classes, "OTHER_PUBLISHED_OFF", false);
    CHECK(mc_client_application_classes_publish(classes, &app, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    row(classes, "FIRST_REPUBLISHED_ON", false);
    /* An independent cold namespace must not borrow the first namespace's
     * actual published App or an earlier renderer's AO value. */
    mc_client_application_classes *independent = NULL;
    CHECK(mc_client_application_classes_create(NULL, &independent, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    row(independent, "INDEPENDENT_NULL_APP", false);
    CHECK(mc_client_application_classes_release(&independent, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    app.constructor_phase = other.constructor_phase = MC_CLIENT_APP_RETIRING;
    CHECK(mc_client_application_namespace_detach(&app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_namespace_detach(&other, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_classes_release(&classes, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_lifetime_retire(&app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_lifetime_retire(&other, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_game_settings_retire(&settings, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_retire(&other_settings, NULL) == MC_GAME_SETTINGS_OK);
    /* Native admission/class failure controls are separate from source NULL. */
    const mc_client_application_class_options zero = {0, 0};
    CHECK(mc_client_application_classes_create(&zero, &classes, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    bool unchanged = true;
    mc_client_application_query_error error;
    CHECK(mc_client_application_ambient_occlusion(classes, &unchanged, &error) ==
          MC_CLIENT_APPLICATION_QUERY_NATIVE_FAILED);
    CHECK(unchanged && error.native_policy && !error.source_returned &&
          error.owner.status == MC_CLIENT_APPLICATION_OWNER_LIMIT && error.owner.site.pc == 2);
    CHECK(mc_client_application_ambient_occlusion(classes, &unchanged, &error) ==
          MC_CLIENT_APPLICATION_QUERY_NATIVE_FAILED);
    CHECK(error.owner.status == MC_CLIENT_APPLICATION_OWNER_CLASS_ERRONEOUS &&
          error.owner.site.pc == 2);
    CHECK(mc_client_application_classes_release(&classes, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_ambient_occlusion(NULL, &unchanged, &error) ==
          MC_CLIENT_APPLICATION_QUERY_INVALID_ARGUMENT);
    CHECK(unchanged && error.native_policy);
    printf("%u Minecraft AO query checks passed\n", checks);
    return 0;
}
