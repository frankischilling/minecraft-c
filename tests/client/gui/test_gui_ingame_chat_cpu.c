/* Genuine cold namespaces/UQ and reached failure prefixes; no prepared Font. */
int mc_chat_startup_fixture_main(int, char **);
#define main mc_chat_startup_fixture_main
#include "../startup/test_application_startup.c"
#undef main
#include "block/registry/registry_internal.h"
#include "block/registry/runtime_internal.h"
#include "client/entity/player_message.h"
#include "client/gui/gui_ingame_chat.h"
#include "client/gui/gui_ingame_chat_internal.h"
#include "entity/player/player_internal.h"

static int chat_registry_keys(fixture *f, mc_client_source_uq *owner, client_application *app) {
    mc_registry_classes *registry = f->services.registry, *other = NULL;
    const mc_registry_options options = {.side = MC_REGISTRY_CLIENT,
                                         .random_profile = f->services.profile,
                                         .materials = f->materials,
                                         .resources = mc_resource_files_source(f->files)};
    CHECK(mc_registry_classes_allocate(&options, &other, NULL) == MC_REGISTRY_OK);
    const mc_registry_string *key = NULL, *foreign = NULL;
    CHECK(mc_registry_runtime_literal_ascii(registry, "no.key", &key, NULL) == MC_REGISTRY_OK &&
          mc_registry_runtime_literal_ascii(other, "no.key", &foreign, NULL) == MC_REGISTRY_OK &&
          key != foreign && mc_registry_runtime_literal_owned(registry, key) &&
          !mc_registry_runtime_literal_owned(other, key));
    const size_t held = registry->runtime_borrows;
    mc_client_gui_ingame_chat_call *call = NULL;
    mc_client_gui_ingame_chat_error error;
    mc_client_gui_ingame_chat_state state;
    CHECK(mc_client_gui_ingame_chat_translate_registry_begin(owner, registry, registry, key, &call,
                                                             &error) ==
              MC_CLIENT_GUI_INGAME_CHAT_SOURCE_NULL &&
          error.site.pc == 8 && !error.native_policy &&
          mc_client_gui_ingame_chat_inspect(call, &state) && state.translated &&
          registry->runtime_borrows == held + 2);
    CHECK(mc_client_gui_ingame_chat_retire(&call, &error) == MC_CLIENT_GUI_INGAME_CHAT_OK &&
          registry->runtime_borrows == held);
    CHECK(mc_client_gui_ingame_chat_translate_registry_begin(
              owner, registry, NULL, key, &call, &error) == MC_CLIENT_GUI_INGAME_CHAT_ARGUMENT &&
          error.native_policy && call != NULL);
    CHECK(mc_client_gui_ingame_chat_retire(&call, &error) == MC_CLIENT_GUI_INGAME_CHAT_OK);
    CHECK(mc_client_gui_ingame_chat_translate_registry_begin(
              owner, registry, other, key, &call, &error) == MC_CLIENT_GUI_INGAME_CHAT_ARGUMENT &&
          error.native_policy && call != NULL && other->runtime_borrows == 1 &&
          registry->runtime_borrows == held);
    CHECK(mc_client_gui_ingame_chat_retire(&call, &error) == MC_CLIENT_GUI_INGAME_CHAT_OK &&
          other->runtime_borrows == 0);
    /* A genuine owned allocation with exactly String's extent is not a String literal. */
    const mc_registry_string *wrong_type = mc_registry_allocate(other, sizeof(mc_registry_string));
    size_t extent = 0;
    CHECK(wrong_type != NULL && mc_registry_permanent_extent(other, wrong_type, &extent) &&
          extent == sizeof(mc_registry_string) &&
          !mc_registry_runtime_literal_owned(other, wrong_type));
    CHECK(mc_client_gui_ingame_chat_translate_registry_begin(owner, registry, other, wrong_type,
                                                             &call, &error) ==
              MC_CLIENT_GUI_INGAME_CHAT_ARGUMENT &&
          error.native_policy && call != NULL);
    CHECK(mc_client_gui_ingame_chat_retire(&call, &error) == MC_CLIENT_GUI_INGAME_CHAT_OK &&
          other->runtime_borrows == 0);
    CHECK(mc_client_gui_ingame_chat_translate_registry_begin(owner, registry, NULL, NULL, &call,
                                                             &error) ==
              MC_CLIENT_GUI_INGAME_CHAT_SOURCE_NULL &&
          !error.native_policy && error.child.pc == 6 && strcmp(error.child.class_name, "nh") == 0);
    CHECK(mc_client_gui_ingame_chat_retire(&call, &error) == MC_CLIENT_GUI_INGAME_CHAT_OK);

    /* RAW concrete DC and RAW App operands exercise only reached field/null prefixes. */
    mc_entity *player = NULL;
    CHECK(mc_local_player_allocate(f->services.entities, &player, NULL) == MC_ENTITY_OK);
    const size_t player_holds = player->holds;
    mc_client_player_message_call *message = NULL;
    mc_client_player_message_error message_error;
    mc_client_player_message_state message_state;
    CHECK(mc_client_player_message_begin(NULL, NULL, wrong_type, &message, &message_error) ==
              MC_CLIENT_PLAYER_MESSAGE_SOURCE_NULL &&
          message_error.site.pc == 0 && strcmp(message_error.site.class_name, "gs") == 0 &&
          !message_error.native_policy);
    CHECK(mc_client_player_message_retire(&message, &message_error) == MC_CLIENT_PLAYER_MESSAGE_OK);
    CHECK(mc_client_player_message_begin(player, NULL, wrong_type, &message, &message_error) ==
              MC_CLIENT_PLAYER_MESSAGE_SOURCE_NULL &&
          message_error.site.pc == 4 && !message_error.native_policy &&
          player->holds == player_holds + 1);
    CHECK(mc_client_player_message_inspect(message, &message_state) &&
          message_state.app_identity == 0 && message_state.uq_identity == 0 &&
          message_state.chat == NULL);
    CHECK(mc_client_player_message_retire(&message, &message_error) ==
              MC_CLIENT_PLAYER_MESSAGE_OK &&
          player->holds == player_holds);
    player->local_player->fields.minecraft = app;
    CHECK(app->v == NULL);
    CHECK(mc_client_player_message_begin(player, NULL, wrong_type, &message, &message_error) ==
              MC_CLIENT_PLAYER_MESSAGE_SOURCE_NULL &&
          message_error.site.pc == 8 && !message_error.native_policy && message_error.chat == NULL);
    CHECK(mc_client_player_message_retire(&message, &message_error) == MC_CLIENT_PLAYER_MESSAGE_OK);
    app->v = owner;
    app->registry = registry;
    CHECK(mc_client_player_message_begin(player, other, foreign, &message, &message_error) ==
              MC_CLIENT_PLAYER_MESSAGE_SOURCE_NULL &&
          message_error.site.pc == 8 && message_error.chat != NULL &&
          message_error.chat->site.pc == 8 && !message_error.native_policy &&
          other->runtime_borrows == 1 &&
          mc_client_player_message_inspect(message, &message_state) &&
          message_state.app_identity == (uintptr_t)app &&
          message_state.uq_identity == (uintptr_t)owner);
    /* Failed checked cleanup preserves the wrapper, exact child, and actual Player hold. */
    other->runtime_borrows = 0;
    const mc_client_player_message_status refused =
        mc_client_player_message_retire(&message, &message_error);
    other->runtime_borrows = 1;
    CHECK(refused == MC_CLIENT_PLAYER_MESSAGE_CLEANUP_FAILED && message != NULL &&
          player->holds == player_holds + 1 && owner->chat_call != NULL);
    CHECK(mc_client_player_message_retire(&message, &message_error) ==
              MC_CLIENT_PLAYER_MESSAGE_OK &&
          message == NULL && player->holds == player_holds && other->runtime_borrows == 0 &&
          registry->runtime_borrows == held && owner->chat_call == NULL);
    app->v = NULL;
    app->registry = NULL;
    mc_entity_release(player);
    CHECK(mc_registry_classes_free(other, NULL) == MC_REGISTRY_OK);
    return 0;
}

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
    CHECK(chat_registry_keys(&f, owner, &app) == 0);
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
