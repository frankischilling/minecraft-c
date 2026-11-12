#include "mc/client/settings/game_option.h"
#include "mc/client/sound/sound_manager.h"

#include <stdio.h>
#include <string.h>

static unsigned checks;

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(expression)) {                                                                       \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression);                       \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    mc_game_settings *settings;
    mc_sound_manager *manager;
    unsigned configurations, constructions, commands, diagnostics, retirements;
    bool refuse_diagnostic, throw_configuration, reentry_blocked, observed_zero, observed_restore;
    float restored_music, restored_sound;
    mc_sound_manager_command last;
} fixture;

static mc_sound_status configure(void *context, mc_sound_manager_configuration kind,
                                 mc_sound_error *error) {
    fixture *test = context;
    if ((unsigned)kind != test->configurations)
        return MC_SOUND_INVALID_ARGUMENT;
    ++test->configurations;
    if (test->throw_configuration) {
        if (error != NULL)
            *error = (mc_sound_error){.status = MC_SOUND_SOURCE_EXCEPTION};
        return MC_SOUND_SOURCE_EXCEPTION;
    }
    return MC_SOUND_OK;
}

static mc_sound_status construct(void *context, void **out, mc_sound_error *error) {
    (void)error;
    fixture *test = context;
    ++test->constructions;
    *out = test;
    return MC_SOUND_OK;
}

static mc_sound_status command(void *context, void *backend,
                               const mc_sound_manager_command *request, mc_sound_error *error) {
    (void)error;
    fixture *test = context;
    if (backend != test)
        return MC_SOUND_INVALID_ARGUMENT;
    ++test->commands;
    test->last = *request;
    return MC_SOUND_OK;
}

static mc_sound_status playing(void *context, void *backend, const char *name, bool *out,
                               mc_sound_error *error) {
    (void)name;
    (void)error;
    if (context != backend)
        return MC_SOUND_INVALID_ARGUMENT;
    *out = false;
    return MC_SOUND_OK;
}

static mc_sound_status retire_backend(void *context, void **backend, mc_sound_error *error) {
    (void)error;
    fixture *test = context;
    if (*backend != test)
        return MC_SOUND_INVALID_ARGUMENT;
    ++test->retirements;
    *backend = NULL;
    return MC_SOUND_OK;
}

static mc_game_settings_status diagnostic(void *context, const mc_game_settings_diagnostic *event,
                                          mc_game_settings_error *error) {
    fixture *test = context;
    ++test->diagnostics;
    mc_game_settings_state state;
    if (event->kind != MC_GAME_SETTINGS_SAVE_FAILED ||
        !mc_game_settings_read_state(test->settings, &state))
        return MC_GAME_SETTINGS_DIAGNOSTIC_FAILED;
    if (test->diagnostics == 1)
        test->observed_zero =
            state.values.music_volume == 0.0f && state.values.sound_volume == 0.0f;
    else if (test->diagnostics == 2)
        test->observed_restore = state.values.music_volume == test->restored_music &&
                                 state.values.sound_volume == test->restored_sound;
    mc_game_settings_option_action_report nested = {.source_pc = 999};
    mc_game_settings *alias = test->settings;
    test->reentry_blocked =
        mc_game_settings_set_option_float(test->settings,
                                          mc_game_option_by_id(MC_GAME_OPTION_SENSITIVITY), 0.1f,
                                          NULL, &nested, NULL) == MC_GAME_SETTINGS_BUSY &&
        nested.source_pc == 999 &&
        mc_game_settings_set_values(test->settings, &state.values, NULL) == MC_GAME_SETTINGS_BUSY &&
        mc_game_settings_save(test->settings, NULL, NULL, NULL) == MC_GAME_SETTINGS_BUSY &&
        mc_game_settings_attach_sound_manager(test->settings, NULL, NULL) ==
            MC_GAME_SETTINGS_BUSY &&
        mc_game_settings_retire(&alias, NULL) == MC_GAME_SETTINGS_BUSY && alias == test->settings &&
        mc_sound_manager_options_changed(test->manager, NULL, NULL, NULL) == MC_SOUND_BUSY;
    if (test->refuse_diagnostic) {
        if (error != NULL)
            *error = (mc_game_settings_error){MC_GAME_SETTINGS_DIAGNOSTIC_FAILED, 79,
                                              "authored nested sound diagnostic failure"};
        return MC_GAME_SETTINGS_DIAGNOSTIC_FAILED;
    }
    return test->reentry_blocked ? MC_GAME_SETTINGS_OK : MC_GAME_SETTINGS_DIAGNOSTIC_FAILED;
}

/* A noarg owner has a null File; genuine source-caught saves still deliver
 * diagnostics. The file constructor supplies a nonnull Minecraft identity,
 * then removing its file role would require an invented public field setter.
 * Instead use an actual nonexistent directory: both saves catch file-open IO. */
static bool nested_actions(const char *missing_directory, bool refuse, bool source_throw) {
    bool ok = false;
    fixture test = {.refuse_diagnostic = refuse,
                    .throw_configuration = source_throw,
                    .restored_music = 0.625f,
                    .restored_sound = 0.0f};
    mc_java_random_profile *random = NULL;
    mc_sound_manager_shared *shared = NULL;
    const mc_sound_manager_backend backend = {&test,   configure, construct,
                                              command, playing,   retire_backend};
    const mc_game_settings_profile text = {MC_JAVA_READER_UTF8, MC_JAVA_WRITER_LF};
    const mc_game_settings_diagnostics sink = {&test, diagnostic};
    mc_game_settings_state state;
    mc_sound_manager_state sound;
    mc_game_settings_error error;
    mc_game_settings_option_action_report report = {0};
    CHECK(mc_java_random_profile_create(4096, &random) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_sound_manager_shared_create(&backend, &shared, NULL) == MC_SOUND_OK);
    CHECK(mc_sound_manager_create(shared, random, MC_SOUND_POOL_MAX_ALLOCATION_BYTES, &test.manager,
                                  NULL) == MC_SOUND_OK);
    CHECK(mc_game_settings_create_file(&test, missing_directory, &text, NULL, &test.settings, NULL,
                                       NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_read_state(test.settings, &state));
    state.values.music_volume = state.values.sound_volume = 0.0f;
    CHECK(mc_game_settings_set_values(test.settings, &state.values, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_sound_manager_bind(test.manager, test.settings, &sink, NULL, NULL) == MC_SOUND_OK);
    CHECK(test.configurations == 0 && test.diagnostics == 0);
    CHECK(mc_game_settings_attach_sound_manager(test.settings, test.manager, NULL) ==
          MC_GAME_SETTINGS_OK);
    const mc_game_settings_status status =
        mc_game_settings_set_option_float(test.settings, mc_game_option_by_id(MC_GAME_OPTION_MUSIC),
                                          test.restored_music, &sink, &report, &error);
    CHECK(report.value_assigned && report.sound_manager_reached && !report.save_reached);
    CHECK(test.reentry_blocked && test.observed_zero);
    CHECK(mc_game_settings_read_state(test.settings, &state));
    CHECK(mc_sound_manager_read_state(test.manager, &sound));
    if (refuse) {
        CHECK(status == MC_GAME_SETTINGS_DIAGNOSTIC_FAILED && error.system_error == 79);
        CHECK(!sound.shared_initialized && !sound.backend_present);
        CHECK(test.diagnostics == 1 && test.configurations == 0 && test.constructions == 0);
        CHECK(state.values.music_volume == 0.0f && state.values.sound_volume == 0.0f);
        CHECK(mc_game_settings_set_option_float(
                  test.settings, mc_game_option_by_id(MC_GAME_OPTION_SOUND), 1.0f, &sink, &report,
                  &error) == MC_GAME_SETTINGS_DIAGNOSTIC_FAILED);
        CHECK(error.system_error == 79 && test.diagnostics == 1 && test.configurations == 0);
    } else if (source_throw) {
        /* Setup catches Throwable, latches initialized and leaves zero fields.
         * optionsChanged then reaches a null backend at its actual stop call. */
        CHECK(status == MC_GAME_SETTINGS_SOURCE_EXCEPTION);
        CHECK(sound.shared_initialized && !sound.backend_present);
        CHECK(test.diagnostics == 1 && test.configurations == 1 && test.constructions == 0);
        CHECK(state.values.music_volume == 0.0f && state.values.sound_volume == 0.0f);
    } else {
        CHECK(status == MC_GAME_SETTINGS_OK && report.source_pc == 56);
        CHECK(sound.shared_initialized && sound.backend_present);
        CHECK(test.diagnostics == 2 && test.observed_restore && test.configurations == 4 &&
              test.constructions == 1);
        CHECK(state.values.music_volume == test.restored_music &&
              state.values.sound_volume == 0.0f);
        CHECK(test.commands == 1 && test.last.kind == MC_SOUND_MANAGER_SET_VOLUME &&
              strcmp(test.last.source_name, "BgMusic") == 0 &&
              test.last.value == test.restored_music);
        CHECK(mc_game_settings_set_option_float(test.settings,
                                                mc_game_option_by_id(MC_GAME_OPTION_SOUND), 0.5f,
                                                &sink, &report, NULL) == MC_GAME_SETTINGS_OK);
        CHECK(test.diagnostics == 2 && test.configurations == 4 && report.sound_manager_reached);
    }
    /* A nested failure releases both owners' guards and retains source fields. */
    CHECK(mc_game_settings_set_option_float(test.settings,
                                            mc_game_option_by_id(MC_GAME_OPTION_SENSITIVITY), 0.25f,
                                            NULL, &report, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_attach_sound_manager(test.settings, NULL, NULL) == MC_GAME_SETTINGS_OK);
    ok = true;
cleanup:
    if (test.settings != NULL)
        mc_game_settings_attach_sound_manager(test.settings, NULL, NULL);
    mc_sound_manager_retire(&test.manager, NULL);
    mc_game_settings_retire(&test.settings, NULL);
    mc_sound_manager_shared_retire(&shared, NULL);
    mc_java_random_profile_free(random);
    return ok;
}

int main(int argc, char **argv) {
    if (argc != 2 || !nested_actions(argv[1], false, false) ||
        !nested_actions(argv[1], true, false) || !nested_actions(argv[1], false, true))
        return 1;
    printf("Settings SoundManager checks: %u\n", checks);
    return 0;
}
