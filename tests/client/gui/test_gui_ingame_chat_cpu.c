/* Genuine cold namespaces/UQ and reached failure prefixes; no prepared Font. */
int mc_chat_startup_fixture_main(int, char **);
#define main mc_chat_startup_fixture_main
#include "../startup/test_application_startup.c"
#undef main
#include "client/gui/gui_ingame_chat.h"
#include "client/gui/gui_ingame_chat_internal.h"

static int chat_translation_failure(const char *resources, const char *profile, int32_t expected_pc,
                                    bool missing_provider) {
    const size_t length = strlen(resources), suffix = strlen(profile);
    CHECK(length <= SIZE_MAX - suffix - 2);
    char *directory = malloc(length + suffix + 2);
    CHECK(directory != NULL);
    memcpy(directory, resources, length);
    directory[length] = '/';
    memcpy(directory + length + 1, profile, suffix + 1);
    fixture f;
    CHECK(fixture_open(&f, directory) == 0 && SDL_WasInit(0) == 0);
    free(directory);
    if (missing_provider) {
        CHECK(mc_registry_classes_free(f.services.registry, NULL) == MC_REGISTRY_OK);
        f.services.registry = NULL;
        const mc_registry_options options = {.side = MC_REGISTRY_CLIENT,
                                             .random_profile = f.services.profile,
                                             .materials = f.materials,
                                             .resources = NULL};
        CHECK(mc_registry_classes_allocate(&options, &f.services.registry, NULL) == MC_REGISTRY_OK);
    }
    mc_client_gui_ingame_classes *classes = NULL;
    mc_client_gui_ingame_error owner_error;
    CHECK(mc_client_gui_ingame_classes_create(f.services.render_classes, f.services.profile, 0,
                                              &classes, &owner_error) == MC_CLIENT_GUI_INGAME_OK);
    mc_client_source_uq *owner = NULL;
    CHECK(mc_client_gui_ingame_allocate(classes, NULL, &owner, &owner_error) ==
              MC_CLIENT_GUI_INGAME_OK &&
          mc_client_gui_ingame_construct(owner, NULL, &owner_error) == MC_CLIENT_GUI_INGAME_OK);
    mc_client_gui_ingame_chat_call *call = NULL;
    mc_client_gui_ingame_chat_error error;
    for (unsigned int invocation = 0; invocation < 2; ++invocation) {
        const mc_registry_status expected = invocation != 0 ? MC_REGISTRY_FAILED_INITIALIZATION
                                            : missing_provider
                                                ? MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY
                                                : MC_REGISTRY_NULL_REFERENCE;
        CHECK(mc_client_gui_ingame_chat_translate_begin(owner, f.services.registry, NULL, &call,
                                                        &error) ==
                  MC_CLIENT_GUI_INGAME_CHAT_CHILD_FAILED &&
              call != NULL && error.native_policy == missing_provider && !error.source_returned &&
              error.site.pc == 0 && strcmp(error.site.class_name, "uq") == 0 &&
              strcmp(error.site.method, "c") == 0 && error.registry.status == expected &&
              error.registry.constructor_pc == expected_pc &&
              strcmp(error.registry.constructor_class, "nh") == 0);
        mc_client_gui_ingame_chat_state state;
        CHECK(mc_client_gui_ingame_chat_inspect(call, &state) && !state.returned &&
              !state.translated && state.widths == 0 && state.messages == 0 &&
              owner->fields.e->size == 0 && owner->fields.g == NULL);
        mc_registry_translation_view translation;
        CHECK(mc_registry_classes_inspect_translation(f.services.registry, &translation, NULL) ==
                  MC_REGISTRY_OK &&
              translation.singleton_phase == MC_REGISTRY_FAILED &&
              translation.singleton_a == NULL && translation.wrapper_phase == MC_REGISTRY_COLD);
        CHECK(mc_client_gui_ingame_tick(owner, &owner_error) == MC_CLIENT_GUI_INGAME_BUSY);
        CHECK(mc_client_gui_ingame_chat_retire(&call, &error) == MC_CLIENT_GUI_INGAME_CHAT_OK &&
              call == NULL && owner->holds == 1 && owner->chat_call == NULL);
    }
    CHECK(mc_client_gui_ingame_retire(&owner, &owner_error) == MC_CLIENT_GUI_INGAME_OK &&
          mc_client_gui_ingame_classes_release(&classes, &owner_error) == MC_CLIENT_GUI_INGAME_OK &&
          fixture_close(&f) == 0 && SDL_WasInit(0) == 0);
    return 0;
}

static int chat_cpu(const char *resources) {
    fixture f;
    CHECK(fixture_open(&f, resources) == 0 && SDL_WasInit(0) == 0);
    mc_client_gui_ingame_classes *classes = NULL;
    mc_client_gui_ingame_error owner_error;
    CHECK(mc_client_gui_ingame_classes_create(f.services.render_classes, f.services.profile, 0,
                                              &classes, &owner_error) == MC_CLIENT_GUI_INGAME_OK);
    mc_client_source_uq *owner = NULL;
    CHECK(mc_client_gui_ingame_allocate(classes, NULL, &owner, &owner_error) ==
              MC_CLIENT_GUI_INGAME_OK &&
          mc_client_gui_ingame_construct(owner, NULL, &owner_error) == MC_CLIENT_GUI_INGAME_OK);
    mc_client_gui_ingame_chat_call *call = NULL, *second = NULL;
    mc_client_gui_ingame_chat_error error;
    CHECK(mc_client_gui_ingame_chat_add_begin(owner, NULL, &call, &error) ==
              MC_CLIENT_GUI_INGAME_CHAT_SOURCE_NULL &&
          call != NULL && error.site.pc == 4 && !error.native_policy);
    mc_client_gui_ingame_chat_state state;
    CHECK(mc_client_gui_ingame_chat_inspect(call, &state) && !state.returned &&
          state.messages == 0 && owner->fields.e->size == 0);
    CHECK(mc_client_gui_ingame_chat_add_begin(owner, NULL, &second, &error) ==
              MC_CLIENT_GUI_INGAME_CHAT_BUSY &&
          second == NULL);
    CHECK(mc_client_gui_ingame_clear_messages(owner, &owner_error) == MC_CLIENT_GUI_INGAME_BUSY &&
          mc_client_gui_ingame_tick(owner, &owner_error) == MC_CLIENT_GUI_INGAME_BUSY &&
          mc_client_gui_ingame_retain(owner, &owner_error) == MC_CLIENT_GUI_INGAME_BUSY);
    mc_client_source_uq *alias = owner;
    CHECK(mc_client_gui_ingame_retire(&alias, &owner_error) == MC_CLIENT_GUI_INGAME_BUSY &&
          alias == owner && owner->holds == 2);
    CHECK(mc_client_gui_ingame_chat_retire(&call, &error) == MC_CLIENT_GUI_INGAME_CHAT_OK &&
          call == NULL && owner->holds == 1 && owner->chat_call == NULL);
    client_application app = {
        0}; /* Actual complete RAW App operand, stable through UQ retirement. */
    owner->fields.g = &app;
    CHECK(mc_client_gui_ingame_chat_add_begin(owner, NULL, &call, &error) ==
              MC_CLIENT_GUI_INGAME_CHAT_SOURCE_NULL &&
          call != NULL && error.site.pc == 8 && !error.native_policy);
    CHECK(mc_client_gui_ingame_chat_retire(&call, &error) == MC_CLIENT_GUI_INGAME_CHAT_OK);
    CHECK(mc_client_gui_ingame_chat_translate_begin(owner, f.services.registry, NULL, &call,
                                                    &error) ==
              MC_CLIENT_GUI_INGAME_CHAT_SOURCE_NULL &&
          call != NULL && error.site.pc == 6 && error.child.pc == 6 &&
          strcmp(error.child.class_name, "nh") == 0 && !error.native_policy);
    mc_registry_translation_view translation;
    CHECK(mc_registry_classes_inspect_translation(f.services.registry, &translation, NULL) ==
              MC_REGISTRY_OK &&
          translation.singleton_phase == MC_REGISTRY_READY && translation.singleton_a != NULL &&
          translation.wrapper_phase == MC_REGISTRY_COLD);
    CHECK(mc_client_gui_ingame_chat_retire(&call, &error) == MC_CLIENT_GUI_INGAME_CHAT_OK);
    const uint16_t missing[] = {'n', 'o', '.', 'k', 'e', 'y'};
    mc_player_string *key = NULL;
    CHECK(mc_player_string_create(missing, sizeof(missing) / sizeof(missing[0]), &key) ==
          MC_PLAYER_OK);
    CHECK(
        mc_client_gui_ingame_chat_translate_begin(owner, f.services.registry, key, &call, &error) ==
            MC_CLIENT_GUI_INGAME_CHAT_SOURCE_NULL &&
        call != NULL && error.site.pc == 8 && !error.native_policy &&
        mc_client_gui_ingame_chat_inspect(call, &state) && state.translated &&
        state.messages == 0 && owner->fields.e->size == 0);
    mc_player_string_release(key); /* The retained invocation owns the same actual key now. */
    CHECK(mc_client_gui_ingame_chat_retire(&call, &error) == MC_CLIENT_GUI_INGAME_CHAT_OK &&
          call == NULL && owner->messages == NULL);
    CHECK(mc_client_gui_ingame_clear_messages(owner, &owner_error) == MC_CLIENT_GUI_INGAME_OK &&
          mc_client_gui_ingame_tick(owner, &owner_error) == MC_CLIENT_GUI_INGAME_OK);
    CHECK(mc_client_gui_ingame_retire(&owner, &owner_error) == MC_CLIENT_GUI_INGAME_OK &&
          mc_client_gui_ingame_classes_release(&classes, &owner_error) == MC_CLIENT_GUI_INGAME_OK &&
          fixture_close(&f) == 0 && SDL_WasInit(0) == 0);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 2 && chat_cpu(argv[1]) == 0 &&
          chat_translation_failure(argv[1], "missing-language", 26, false) == 0 &&
          chat_translation_failure(argv[1], "missing-statistics", 40, false) == 0 &&
          chat_translation_failure(argv[1], "missing-language", 23, true) == 0);
    printf("GuiIngame chat CPU: %u checks; real failure prefixes, no Video/GL\n", checks);
    return 0;
}
