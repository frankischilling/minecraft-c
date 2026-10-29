#include "mc/client/sound/sound_manager.h"

#include "mc/client/settings/game_option.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define COUNT(values) (sizeof(values) / sizeof((values)[0]))
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct recorded_call {
    int kind;
    mc_sound_manager_command command;
    char name[16];
} recorded_call;

typedef struct backend_fixture {
    recorded_call calls[64];
    size_t count, boundaries, fail_at;
    mc_sound_status failure;
    bool music_playing, streaming_playing, invalid;
    unsigned retires, diagnostics;
    float diagnostic_music[4], diagnostic_sound[4];
    mc_sound_manager *manager;
    mc_sound_manager_shared *shared;
    mc_game_settings *settings;
    bool check_reentry;
    unsigned fail_diagnostic_at;
} backend_fixture;

static mc_sound_status boundary(backend_fixture *fixture, int kind, const char *name,
                                const mc_sound_manager_command *command) {
    ++fixture->boundaries;
    if (fixture->count >= COUNT(fixture->calls)) {
        fixture->invalid = true;
        return MC_SOUND_LIMIT;
    }
    recorded_call *call = &fixture->calls[fixture->count++];
    *call = (recorded_call){.kind = kind};
    if (command != NULL)
        call->command = *command;
    if (name != NULL) {
        const size_t length = strlen(name);
        if (length >= sizeof(call->name)) {
            fixture->invalid = true;
            return MC_SOUND_LIMIT;
        }
        memcpy(call->name, name, length + 1u);
        call->command.source_name = call->name;
    }
    if (fixture->check_reentry) {
        mc_sound_manager_state state;
        mc_sound_manager_report report = {.source_pc = 12345};
        if (!mc_sound_manager_read_state(fixture->manager, &state) || !state.active ||
            mc_sound_manager_ui(fixture->manager, (mc_sound_text){.is_null = true}, 1.0f, 1.0f,
                                &report, NULL) != MC_SOUND_BUSY ||
            report.source_pc != 12345 ||
            mc_sound_manager_detach_settings(fixture->manager, fixture->settings, NULL) !=
                MC_SOUND_BUSY ||
            mc_sound_manager_retire(&fixture->manager, NULL) != MC_SOUND_BUSY ||
            mc_sound_manager_shared_retire(&fixture->shared, NULL) != MC_SOUND_BUSY)
            fixture->invalid = true;
    }
    return fixture->boundaries == fixture->fail_at ? fixture->failure : MC_SOUND_OK;
}

static mc_sound_status configure(void *context, mc_sound_manager_configuration configuration,
                                 mc_sound_error *error) {
    (void)error;
    backend_fixture *fixture = context;
    mc_game_settings_state settings;
    if (!mc_game_settings_read_state(fixture->settings, &settings) ||
        settings.values.music_volume != 0.0f || settings.values.sound_volume != 0.0f)
        fixture->invalid = true;
    return boundary(fixture, 100 + (int)configuration, NULL, NULL);
}

static mc_sound_status construct(void *context, void **out, mc_sound_error *error) {
    (void)error;
    backend_fixture *fixture = context;
    const mc_sound_status status = boundary(fixture, 200, NULL, NULL);
    if (status == MC_SOUND_OK)
        *out = fixture;
    return status;
}

static mc_sound_status issue_command(void *context, void *backend,
                                     const mc_sound_manager_command *command,
                                     mc_sound_error *error) {
    (void)error;
    backend_fixture *fixture = context;
    if (backend != fixture)
        fixture->invalid = true;
    return boundary(fixture, (int)command->kind, command->source_name, command);
}

static mc_sound_status query_playing(void *context, void *backend, const char *name, bool *out,
                                     mc_sound_error *error) {
    (void)error;
    backend_fixture *fixture = context;
    if (backend != fixture)
        fixture->invalid = true;
    const mc_sound_status status = boundary(fixture, 300, name, NULL);
    if (status == MC_SOUND_OK)
        *out = strcmp(name, "BgMusic") == 0 ? fixture->music_playing : fixture->streaming_playing;
    return status;
}

static mc_sound_status retire_backend(void *context, void **backend, mc_sound_error *error) {
    (void)error;
    backend_fixture *fixture = context;
    if (*backend != fixture)
        fixture->invalid = true;
    ++fixture->retires;
    *backend = NULL;
    return MC_SOUND_OK;
}

static mc_game_settings_status diagnostic(void *context, const mc_game_settings_diagnostic *event,
                                          mc_game_settings_error *error) {
    (void)error;
    backend_fixture *fixture = context;
    mc_game_settings_state state;
    if (fixture->diagnostics >= COUNT(fixture->diagnostic_music) ||
        !mc_game_settings_read_state(fixture->settings, &state)) {
        fixture->invalid = true;
        return MC_GAME_SETTINGS_DIAGNOSTIC_FAILED;
    }
    fixture->diagnostic_music[fixture->diagnostics] = state.values.music_volume;
    fixture->diagnostic_sound[fixture->diagnostics++] = state.values.sound_volume;
    if (event->kind != MC_GAME_SETTINGS_SAVE_FAILED ||
        mc_game_settings_set_values(fixture->settings, &state.values, NULL) !=
            MC_GAME_SETTINGS_BUSY ||
        mc_game_settings_save(fixture->settings, NULL, NULL, NULL) != MC_GAME_SETTINGS_BUSY ||
        mc_sound_manager_cleanup(fixture->manager, NULL, NULL) != MC_SOUND_BUSY)
        fixture->invalid = true;
    return fixture->diagnostics == fixture->fail_diagnostic_at ? MC_GAME_SETTINGS_DIAGNOSTIC_FAILED
                                                               : MC_GAME_SETTINGS_OK;
}

static bool fixture_create(backend_fixture *fixture, mc_java_random_profile *profile) {
    const mc_sound_manager_backend backend = {.context = fixture,
                                              .configure = configure,
                                              .construct = construct,
                                              .command = issue_command,
                                              .playing = query_playing,
                                              .retire = retire_backend};
    return mc_game_settings_create_defaults(NULL, &fixture->settings, NULL) ==
               MC_GAME_SETTINGS_OK &&
           mc_sound_manager_shared_create(&backend, &fixture->shared, NULL) == MC_SOUND_OK &&
           mc_sound_manager_create(fixture->shared, profile, MC_SOUND_POOL_MAX_ALLOCATION_BYTES,
                                   &fixture->manager, NULL) == MC_SOUND_OK;
}

static void fixture_free(backend_fixture *fixture) {
    mc_game_settings_attach_sound_manager(fixture->settings, NULL, NULL);
    mc_sound_manager_retire(&fixture->manager, NULL);
    mc_sound_manager_shared_retire(&fixture->shared, NULL);
    mc_game_settings_retire(&fixture->settings, NULL);
}

static bool volumes(mc_game_settings *settings, float music, float sound) {
    mc_game_settings_state state;
    if (!mc_game_settings_read_state(settings, &state))
        return false;
    state.values.music_volume = music;
    state.values.sound_volume = sound;
    return mc_game_settings_set_values(settings, &state.values, NULL) == MC_GAME_SETTINGS_OK;
}

static void clear_calls(backend_fixture *fixture) {
    fixture->count = 0;
    fixture->boundaries = 0;
    fixture->fail_at = 0;
}

static mc_sound_text ascii(const char *value, uint16_t *units) {
    const size_t length = strlen(value);
    for (size_t i = 0; i < length; ++i)
        units[i] = (unsigned char)value[i];
    return (mc_sound_text){.units = units, .length = length};
}

static bool add(backend_fixture *fixture, mc_sound_manager_pool_kind pool, const char *name) {
    uint16_t units[64];
    const mc_sound_pool_entry *entry = NULL;
#ifdef _WIN32
    const char *path = "C:/authored/sound/unused.ogg";
#else
    const char *path = "/authored/sound/unused.ogg";
#endif
    return mc_sound_manager_add_file(fixture->manager, pool, ascii(name, units), path, &entry, NULL,
                                     NULL) == MC_SOUND_OK &&
           entry != NULL;
}

/* Independent Java Random LCG/rejection continuation from an observed state.
 * No result or seed is installed into any production owner. */
static int32_t draw(uint64_t *state, uint32_t bound) {
    for (;;) {
        *state = (*state * UINT64_C(25214903917) + UINT64_C(11)) & UINT64_C(281474976710655);
        const uint32_t bits = (uint32_t)(*state / UINT64_C(131072));
        if ((bound & (bound - 1u)) == 0)
            return (int32_t)(((uint64_t)bound * bits) / UINT64_C(2147483648));
        const uint32_t value = bits % bound;
        if ((uint64_t)bits - value + bound - 1u < UINT64_C(2147483648))
            return (int32_t)value;
    }
}

static bool pool_state(backend_fixture *fixture, mc_sound_manager_pool_kind pool,
                       mc_sound_pool_state *out) {
    return mc_sound_pool_read_state(mc_sound_manager_pool(fixture->manager, pool), out, NULL) ==
           MC_SOUND_OK;
}

static bool construction_and_setup(mc_java_random_profile *profile) {
    bool ok = false;
    backend_fixture fixture = {0};
    mc_sound_manager *second = NULL;
    mc_sound_manager_state state;
    mc_sound_pool_state pool;
    mc_sound_manager_report report;
    CHECK(fixture_create(&fixture, profile));
    CHECK(mc_sound_manager_read_state(fixture.manager, &state));
    CHECK(!state.shared_initialized && !state.backend_present && !state.active &&
          state.settings == NULL && state.sound_counter == 0 && state.music_delay >= 0 &&
          state.music_delay < 12000 && !state.random.have_next_next_gaussian);
    for (int i = 0; i < (int)MC_SOUND_MANAGER_POOL_COUNT; ++i) {
        CHECK(pool_state(&fixture, (mc_sound_manager_pool_kind)i, &pool));
        CHECK(pool.strip_digits && pool.full_entry_count == 0 &&
              !pool.random.have_next_next_gaussian);
    }
    CHECK(mc_sound_manager_cleanup(fixture.manager, &report, NULL) == MC_SOUND_OK &&
          report.source_pc == 12 && fixture.count == 0);
    CHECK(mc_sound_manager_options_changed(fixture.manager, NULL, &report, NULL) ==
          MC_SOUND_SOURCE_EXCEPTION);
    CHECK(report.source_pc == 10 && report.exception == MC_SOUND_MANAGER_EXCEPTION_NULL_SETTINGS);
    CHECK(volumes(fixture.settings, -0.0f, 0.0f));
    CHECK(mc_sound_manager_bind(fixture.manager, fixture.settings, NULL, &report, NULL) ==
          MC_SOUND_OK);
    CHECK(!report.setup.entered && fixture.count == 0);
    CHECK(pool_state(&fixture, MC_SOUND_MANAGER_STREAMING, &pool) && !pool.strip_digits);
    CHECK(mc_sound_manager_read_state(fixture.manager, &state) && !state.shared_initialized);
    CHECK(mc_sound_manager_music_tick(fixture.manager, &report, NULL) == MC_SOUND_OK &&
          report.source_pc == 18 && fixture.count == 0);
    CHECK(volumes(fixture.settings, 0.375f, 0.625f));
    fixture.check_reentry = true;
    const mc_game_settings_diagnostics diagnostics = {.context = &fixture, .emit = diagnostic};
    CHECK(mc_sound_manager_bind(fixture.manager, fixture.settings, &diagnostics, &report, NULL) ==
          MC_SOUND_OK);
    CHECK(report.source_pc == 45 && report.setup.entered && report.setup.volumes_zeroed &&
          report.setup.first_save_reached && report.setup.backend_published &&
          report.setup.volumes_restored && report.setup.second_save_reached &&
          !report.setup.throwable_caught && report.setup.source_pc == 118);
    CHECK(fixture.count == 5 && fixture.calls[0].kind == 100 && fixture.calls[1].kind == 101 &&
          fixture.calls[2].kind == 102 && fixture.calls[3].kind == 103 &&
          fixture.calls[4].kind == 200);
    CHECK(fixture.diagnostics == 2 && fixture.diagnostic_music[0] == 0.0f &&
          fixture.diagnostic_sound[0] == 0.0f && fixture.diagnostic_music[1] == 0.375f &&
          fixture.diagnostic_sound[1] == 0.625f && !fixture.invalid);
    CHECK(report.setup.saves[0].outer_exception_caught &&
          report.setup.saves[1].outer_exception_caught);
    CHECK(mc_sound_manager_read_state(fixture.manager, &state) && state.shared_initialized &&
          state.backend_present && state.settings == fixture.settings);
    fixture.check_reentry = false;
    CHECK(mc_sound_manager_create(fixture.shared, profile, MC_SOUND_POOL_MAX_ALLOCATION_BYTES,
                                  &second, NULL) == MC_SOUND_OK);
    CHECK(mc_sound_manager_read_state(second, &state) && state.shared_initialized &&
          state.backend_present && state.settings == NULL);
    CHECK(mc_sound_manager_bind(second, fixture.settings, NULL, &report, NULL) == MC_SOUND_OK &&
          !report.setup.entered && fixture.count == 5);
    CHECK(mc_sound_manager_shared_retire(&fixture.shared, NULL) == MC_SOUND_BUSY);
    clear_calls(&fixture);
    CHECK(mc_sound_manager_cleanup(second, &report, NULL) == MC_SOUND_OK && fixture.count == 1 &&
          fixture.calls[0].kind == MC_SOUND_MANAGER_CLEANUP);
    CHECK(mc_sound_manager_read_state(second, &state) && state.shared_initialized &&
          state.backend_present);
    CHECK(mc_sound_manager_detach_settings(second, NULL, NULL) == MC_SOUND_INVALID_ARGUMENT);
    CHECK(mc_sound_manager_read_state(second, &state) && state.settings == fixture.settings);
    CHECK(mc_sound_manager_detach_settings(second, fixture.settings, NULL) == MC_SOUND_OK &&
          fixture.count == 1);
    CHECK(mc_sound_manager_read_state(second, &state) && state.settings == NULL &&
          state.shared_initialized && state.backend_present);
    CHECK(mc_sound_manager_retire(&second, NULL) == MC_SOUND_OK);
    CHECK(!fixture.invalid);
    ok = true;
cleanup:
    mc_sound_manager_retire(&second, NULL);
    fixture_free(&fixture);
    return ok;
}

static bool setup_failures(mc_java_random_profile *profile) {
    bool ok = false;
    backend_fixture fixture = {0};
    mc_sound_manager_report report;
    mc_sound_manager_state state;
    mc_game_settings_state settings;
    static const int32_t failure_pcs[] = {41, 48, 55, 62, 69};
    for (size_t i = 0; i < COUNT(failure_pcs); ++i) {
        fixture = (backend_fixture){0};
        CHECK(fixture_create(&fixture, profile));
        CHECK(volumes(fixture.settings, 0.25f, 0.75f));
        fixture.fail_at = i + 1u;
        fixture.failure = MC_SOUND_SOURCE_EXCEPTION;
        CHECK(mc_sound_manager_bind(fixture.manager, fixture.settings, NULL, &report, NULL) ==
              MC_SOUND_OK);
        CHECK(report.setup.throwable_caught && report.setup.source_pc == failure_pcs[i] &&
              report.setup.exception == MC_SOUND_MANAGER_EXCEPTION_BACKEND_THROWABLE &&
              report.setup.volumes_zeroed && report.setup.first_save_reached &&
              !report.setup.backend_published && !report.setup.volumes_restored &&
              !report.setup.second_save_reached && fixture.count == i + 1u);
        CHECK(mc_sound_manager_read_state(fixture.manager, &state) && state.shared_initialized &&
              !state.backend_present);
        CHECK(mc_game_settings_read_state(fixture.settings, &settings) &&
              settings.values.music_volume == 0.0f && settings.values.sound_volume == 0.0f);
        CHECK(volumes(fixture.settings, 0.5f, 0.5f));
        CHECK(mc_sound_manager_bind(fixture.manager, fixture.settings, NULL, &report, NULL) ==
              MC_SOUND_OK);
        CHECK(!report.setup.entered && fixture.count == i + 1u);
        CHECK(mc_sound_manager_options_changed(fixture.manager, NULL, &report, NULL) ==
              MC_SOUND_SOURCE_EXCEPTION);
        CHECK(report.exception == MC_SOUND_MANAGER_EXCEPTION_NULL_BACKEND &&
              report.source_pc == 75);
        CHECK(mc_sound_manager_cleanup(fixture.manager, &report, NULL) ==
                  MC_SOUND_SOURCE_EXCEPTION &&
              report.exception == MC_SOUND_MANAGER_EXCEPTION_NULL_BACKEND && report.source_pc == 9);
        CHECK(!fixture.invalid);
        fixture_free(&fixture);
    }
    fixture = (backend_fixture){0};
    CHECK(fixture_create(&fixture, profile));
    CHECK(mc_sound_manager_bind(fixture.manager, NULL, NULL, &report, NULL) == MC_SOUND_OK);
    CHECK(report.setup.throwable_caught && report.setup.source_pc == 4 &&
          report.setup.exception == MC_SOUND_MANAGER_EXCEPTION_NULL_SETTINGS &&
          !report.setup.volumes_zeroed && fixture.count == 0);
    CHECK(mc_sound_manager_read_state(fixture.manager, &state) && state.shared_initialized &&
          !state.backend_present && state.settings == NULL);
    CHECK(mc_sound_manager_options_changed(fixture.manager, NULL, &report, NULL) ==
              MC_SOUND_SOURCE_EXCEPTION &&
          report.source_pc == 44 && report.exception == MC_SOUND_MANAGER_EXCEPTION_NULL_SETTINGS);
    fixture_free(&fixture);
    fixture = (backend_fixture){0};
    CHECK(fixture_create(&fixture, profile));
    fixture.fail_at = 3;
    fixture.failure = MC_SOUND_LIMIT;
    CHECK(mc_sound_manager_bind(fixture.manager, fixture.settings, NULL, &report, NULL) ==
          MC_SOUND_LIMIT);
    CHECK(!report.setup.throwable_caught && report.setup.source_pc == 55 &&
          report.setup.volumes_zeroed && !report.setup.volumes_restored && fixture.count == 3);
    CHECK(mc_sound_manager_read_state(fixture.manager, &state) && !state.shared_initialized &&
          !state.backend_present);
    fixture_free(&fixture);
    fixture = (backend_fixture){0};
    CHECK(fixture_create(&fixture, profile));
    fixture.fail_diagnostic_at = 1;
    const mc_game_settings_diagnostics diagnostics = {.context = &fixture, .emit = diagnostic};
    CHECK(mc_sound_manager_bind(fixture.manager, fixture.settings, &diagnostics, &report, NULL) ==
          MC_SOUND_IO_FAILED);
    CHECK(report.setup.first_save_reached && !report.setup.backend_published &&
          report.setup.settings_status == MC_GAME_SETTINGS_DIAGNOSTIC_FAILED &&
          report.setup.source_pc == 36 && fixture.count == 0 && !fixture.invalid);
    CHECK(mc_sound_manager_read_state(fixture.manager, &state) && !state.shared_initialized);
    fixture_free(&fixture);
    fixture = (backend_fixture){0};
    CHECK(fixture_create(&fixture, profile));
    CHECK(volumes(fixture.settings, 0.125f, 0.875f));
    fixture.fail_diagnostic_at = 2;
    CHECK(mc_sound_manager_bind(fixture.manager, fixture.settings, &diagnostics, &report, NULL) ==
          MC_SOUND_IO_FAILED);
    CHECK(report.setup.backend_published && report.setup.volumes_restored &&
          report.setup.second_save_reached && report.setup.source_pc == 95 &&
          report.setup.settings_status == MC_GAME_SETTINGS_DIAGNOSTIC_FAILED &&
          !report.setup.throwable_caught && fixture.count == 5 && !fixture.invalid);
    CHECK(mc_sound_manager_read_state(fixture.manager, &state) && !state.shared_initialized &&
          state.backend_present);
    CHECK(mc_game_settings_read_state(fixture.settings, &settings) &&
          settings.values.music_volume == 0.125f && settings.values.sound_volume == 0.875f);
    CHECK(report.setup.native_terminal);
    clear_calls(&fixture);
    CHECK(mc_sound_manager_bind(fixture.manager, fixture.settings, NULL, &report, NULL) ==
          MC_SOUND_IO_FAILED);
    CHECK(report.setup.native_terminal && report.setup.source_pc == 95 &&
          !report.setup.first_save_reached && !report.setup.backend_published &&
          fixture.count == 0);
    CHECK(mc_sound_manager_read_state(fixture.manager, &state) && !state.shared_initialized &&
          state.backend_present);
    ok = true;
cleanup:
    fixture_free(&fixture);
    return ok;
}

static bool effects_and_streaming(mc_java_random_profile *profile) {
    bool ok = false;
    backend_fixture fixture = {0};
    mc_sound_manager_state state;
    mc_sound_pool_state before, after;
    mc_sound_manager_report report;
    uint16_t effect_units[32], stream_units[32];
    const mc_sound_text effect = ascii("random.click", effect_units);
    const mc_sound_text stream = ascii("13", stream_units);
    CHECK(fixture_create(&fixture, profile));
    CHECK(volumes(fixture.settings, 0.5f, 0.75f));
    CHECK(mc_sound_manager_bind(fixture.manager, fixture.settings, NULL, NULL, NULL) ==
          MC_SOUND_OK);
    CHECK(add(&fixture, MC_SOUND_MANAGER_EFFECTS, "random/click1.ogg"));
    CHECK(add(&fixture, MC_SOUND_MANAGER_STREAMING, "13.ogg"));
    clear_calls(&fixture);
    fixture.check_reentry = true;
    CHECK(mc_sound_manager_ui(fixture.manager, effect, 2.0f, 0.8f, &report, NULL) == MC_SOUND_OK);
    CHECK(report.source_pc == 144 && fixture.count == 4 &&
          fixture.calls[0].kind == MC_SOUND_MANAGER_NEW_SOURCE &&
          strcmp(fixture.calls[0].name, "sound_1") == 0 && !fixture.calls[0].command.priority &&
          !fixture.calls[0].command.looping && fixture.calls[0].command.entry != NULL &&
          fixture.calls[0].command.x == 0.0f && fixture.calls[0].command.y == 0.0f &&
          fixture.calls[0].command.z == 0.0f && fixture.calls[0].command.attenuation == 0 &&
          fixture.calls[0].command.distance == 0.0f &&
          fixture.calls[1].kind == MC_SOUND_MANAGER_SET_PITCH &&
          fixture.calls[1].command.value == 0.8f &&
          fixture.calls[2].kind == MC_SOUND_MANAGER_SET_VOLUME &&
          fixture.calls[2].command.value == 0.1875f &&
          fixture.calls[3].kind == MC_SOUND_MANAGER_PLAY);
    clear_calls(&fixture);
    CHECK(mc_sound_manager_effect(fixture.manager, effect, 1.0f, -2.0f, 3.0f, 3.0f, 1.2f, &report,
                                  NULL) == MC_SOUND_OK);
    CHECK(report.source_pc == 181 && fixture.count == 4 && fixture.calls[0].command.priority &&
          fixture.calls[0].command.distance == 48.0f && fixture.calls[0].command.attenuation == 2 &&
          fixture.calls[0].command.x == 1.0f && fixture.calls[0].command.y == -2.0f &&
          fixture.calls[0].command.z == 3.0f && strcmp(fixture.calls[0].name, "sound_2") == 0 &&
          fixture.calls[1].command.value == 1.2f && fixture.calls[2].command.value == 0.75f);
    CHECK(pool_state(&fixture, MC_SOUND_MANAGER_EFFECTS, &before));
    uint64_t expected_random = before.random.state;
    for (size_t i = 0; i < 3; ++i) {
        const float rejected[] = {0.0f, -1.0f, NAN};
        clear_calls(&fixture);
        (void)draw(&expected_random, 1);
        CHECK(mc_sound_manager_effect(fixture.manager, effect, 0.0f, 0.0f, 0.0f, rejected[i], 1.0f,
                                      &report, NULL) == MC_SOUND_OK);
        CHECK(fixture.count == 0 && pool_state(&fixture, MC_SOUND_MANAGER_EFFECTS, &after) &&
              after.random.state == expected_random);
    }
    CHECK(mc_sound_manager_read_state(fixture.manager, &state) && state.sound_counter == 2);
    const float unbounded[] = {-2.0f, -0.0f, NAN, INFINITY};
    for (size_t i = 0; i < COUNT(unbounded); ++i) {
        clear_calls(&fixture);
        CHECK(mc_sound_manager_ui(fixture.manager, effect, unbounded[i], -4.0f, &report, NULL) ==
              MC_SOUND_OK);
        CHECK(fixture.count == 4 && fixture.calls[1].command.value == -4.0f);
        const float value = fixture.calls[2].command.value;
        if (i == 0)
            CHECK(value == -0.375f);
        else if (i == 1)
            CHECK(value == 0.0f && signbit(value));
        else if (i == 2)
            CHECK(isnan(value));
        else
            CHECK(value == 0.1875f);
    }
    clear_calls(&fixture);
    fixture.streaming_playing = true;
    CHECK(mc_sound_manager_stream(fixture.manager, (mc_sound_text){.is_null = true}, 0.0f, 0.0f,
                                  0.0f, 1.0f, 1.0f, &report, NULL) == MC_SOUND_OK);
    CHECK(report.source_pc == 46 && fixture.count == 2 && fixture.calls[0].kind == 300 &&
          strcmp(fixture.calls[0].name, "streaming") == 0 &&
          fixture.calls[1].kind == MC_SOUND_MANAGER_STOP);
    clear_calls(&fixture);
    fixture.music_playing = true;
    CHECK(mc_sound_manager_stream(fixture.manager, stream, 1.0f, 2.0f, 3.0f, 99.0f, -5.0f, &report,
                                  NULL) == MC_SOUND_OK);
    CHECK(report.source_pc == 148 && fixture.count == 7 && fixture.calls[0].kind == 300 &&
          fixture.calls[1].kind == MC_SOUND_MANAGER_STOP && fixture.calls[2].kind == 300 &&
          strcmp(fixture.calls[2].name, "BgMusic") == 0 &&
          fixture.calls[3].kind == MC_SOUND_MANAGER_STOP &&
          fixture.calls[4].kind == MC_SOUND_MANAGER_NEW_STREAMING_SOURCE &&
          fixture.calls[4].command.priority && fixture.calls[4].command.distance == 64.0f &&
          fixture.calls[4].command.attenuation == 2 && fixture.calls[4].command.x == 1.0f &&
          fixture.calls[4].command.y == 2.0f && fixture.calls[4].command.z == 3.0f &&
          fixture.calls[5].kind == MC_SOUND_MANAGER_SET_VOLUME &&
          fixture.calls[5].command.value == 0.375f &&
          fixture.calls[6].kind == MC_SOUND_MANAGER_PLAY);
    clear_calls(&fixture);
    CHECK(pool_state(&fixture, MC_SOUND_MANAGER_STREAMING, &before));
    expected_random = before.random.state;
    (void)draw(&expected_random, 1);
    CHECK(mc_sound_manager_stream(fixture.manager, stream, 0.0f, 0.0f, 0.0f, NAN, 1.0f, NULL,
                                  NULL) == MC_SOUND_OK);
    CHECK(fixture.count == 2 && pool_state(&fixture, MC_SOUND_MANAGER_STREAMING, &after) &&
          after.random.state == expected_random);
    clear_calls(&fixture);
    fixture.fail_at = 2;
    fixture.failure = MC_SOUND_LIMIT;
    CHECK(mc_sound_manager_ui(fixture.manager, effect, 1.0f, 1.0f, &report, NULL) ==
          MC_SOUND_LIMIT);
    CHECK(fixture.count == 2 && report.source_pc == 116);
    CHECK(mc_sound_manager_read_state(fixture.manager, &state) && state.sound_counter == 7);
    CHECK(!fixture.invalid);
    ok = true;
cleanup:
    fixture_free(&fixture);
    return ok;
}

static bool music_and_listener(mc_java_random_profile *profile) {
    bool ok = false;
    backend_fixture fixture = {0};
    mc_sound_manager_state before, after;
    mc_sound_manager_report report;
    mc_math_trig *trig = NULL;
    CHECK(fixture_create(&fixture, profile));
    CHECK(volumes(fixture.settings, 0.625f, 0.5f));
    CHECK(mc_sound_manager_bind(fixture.manager, fixture.settings, NULL, NULL, NULL) ==
          MC_SOUND_OK);
    CHECK(mc_sound_manager_read_state(fixture.manager, &before));
    clear_calls(&fixture);
    fixture.music_playing = true;
    CHECK(mc_sound_manager_music_tick(fixture.manager, &report, NULL) == MC_SOUND_OK);
    CHECK(fixture.count == 1 && strcmp(fixture.calls[0].name, "BgMusic") == 0);
    CHECK(mc_sound_manager_read_state(fixture.manager, &after) &&
          after.music_delay == before.music_delay && after.random.state == before.random.state);
    clear_calls(&fixture);
    fixture.music_playing = false;
    fixture.streaming_playing = true;
    CHECK(mc_sound_manager_music_tick(fixture.manager, &report, NULL) == MC_SOUND_OK);
    CHECK(fixture.count == 2 && strcmp(fixture.calls[1].name, "streaming") == 0);
    CHECK(mc_sound_manager_read_state(fixture.manager, &after) &&
          after.music_delay == before.music_delay && after.random.state == before.random.state);
    fixture.streaming_playing = false;
    for (int32_t i = before.music_delay; i > 0; --i) {
        clear_calls(&fixture);
        CHECK(mc_sound_manager_music_tick(fixture.manager, &report, NULL) == MC_SOUND_OK);
        CHECK(report.source_pc == 58 && fixture.count == 2 &&
              mc_sound_manager_read_state(fixture.manager, &after) && after.music_delay == i - 1 &&
              after.random.state == before.random.state);
    }
    clear_calls(&fixture);
    CHECK(mc_sound_manager_music_tick(fixture.manager, &report, NULL) == MC_SOUND_OK &&
          report.source_pc == 129 && fixture.count == 2);
    CHECK(mc_sound_manager_read_state(fixture.manager, &after) && after.music_delay == 0 &&
          after.random.state == before.random.state);
    CHECK(add(&fixture, MC_SOUND_MANAGER_MUSIC, "music/calm1.ogg"));
    uint64_t random = before.random.state;
    const int32_t expected_delay = draw(&random, 12000) + 12000;
    clear_calls(&fixture);
    fixture.check_reentry = true;
    CHECK(mc_sound_manager_music_tick(fixture.manager, &report, NULL) == MC_SOUND_OK);
    CHECK(
        report.source_pc == 129 && fixture.count == 5 && fixture.calls[0].kind == 300 &&
        fixture.calls[1].kind == 300 &&
        fixture.calls[2].kind == MC_SOUND_MANAGER_BACKGROUND_MUSIC &&
        strcmp(fixture.calls[2].name, "BgMusic") == 0 && fixture.calls[2].command.entry != NULL &&
        !fixture.calls[2].command.looping && fixture.calls[3].kind == MC_SOUND_MANAGER_SET_VOLUME &&
        fixture.calls[3].command.value == 0.625f && fixture.calls[4].kind == MC_SOUND_MANAGER_PLAY);
    CHECK(mc_sound_manager_read_state(fixture.manager, &after) &&
          after.music_delay == expected_delay && after.random.state == random);
    clear_calls(&fixture);
    CHECK(mc_sound_manager_listener_update(fixture.manager, NULL, 0.5f, NULL, &report, NULL) ==
              MC_SOUND_OK &&
          report.source_pc == 23 && fixture.count == 0);
    trig = malloc(sizeof(*trig));
    CHECK(trig != NULL && mc_math_trig_init(trig));
    const mc_sound_manager_listener listener = {.previous_yaw = 0.0f,
                                                .yaw = 0.0f,
                                                .previous_x = 16777217.0,
                                                .previous_y = -3.0,
                                                .previous_z = 5.0,
                                                .x = 16777219.0,
                                                .y = 9.0,
                                                .z = -11.0};
    CHECK(mc_sound_manager_listener_update(fixture.manager, &listener, 0.5f, trig, &report, NULL) ==
          MC_SOUND_OK);
    CHECK(report.source_pc == 179 && fixture.count == 2 &&
          fixture.calls[0].kind == MC_SOUND_MANAGER_LISTENER_POSITION &&
          fixture.calls[0].command.x == 16777218.0f && fixture.calls[0].command.y == 3.0f &&
          fixture.calls[0].command.z == -3.0f &&
          fixture.calls[1].kind == MC_SOUND_MANAGER_LISTENER_ORIENTATION);
    CHECK(fixture.calls[1].command.orientation[0] == -trig->values[32768] &&
          fixture.calls[1].command.orientation[1] == 0.0f &&
          fixture.calls[1].command.orientation[2] == 1.0f &&
          fixture.calls[1].command.orientation[3] == 0.0f &&
          fixture.calls[1].command.orientation[4] == 1.0f &&
          fixture.calls[1].command.orientation[5] == 0.0f);
    clear_calls(&fixture);
    fixture.fail_at = 1;
    fixture.failure = MC_SOUND_IO_FAILED;
    CHECK(mc_sound_manager_listener_update(fixture.manager, &listener, 0.5f, trig, &report, NULL) ==
              MC_SOUND_IO_FAILED &&
          fixture.count == 1 && report.source_pc == 158);
    clear_calls(&fixture);
    CHECK(volumes(fixture.settings, 0.625f, -0.0f));
    CHECK(mc_sound_manager_listener_update(fixture.manager, &listener, NAN, NULL, &report, NULL) ==
              MC_SOUND_OK &&
          report.source_pc == 18 && fixture.count == 0);
    CHECK(!fixture.invalid);
    ok = true;
cleanup:
    free(trig);
    fixture_free(&fixture);
    return ok;
}

static bool options_and_counter(mc_java_random_profile *profile) {
    bool ok = false;
    backend_fixture fixture = {0};
    mc_sound_manager_report report;
    mc_sound_manager_state state;
    mc_game_settings_option_action_report action;
    uint16_t units[32];
    const mc_sound_text effect = ascii("random.click", units);
    CHECK(fixture_create(&fixture, profile));
    CHECK(mc_game_settings_retire(&fixture.settings, NULL) == MC_GAME_SETTINGS_OK);
    /* The source action dereferences Minecraft before its SoundManager field.
     * A noarg settings owner has null Minecraft even when a manager is attached.
     * This file owner supplies a real borrowed receiver and a deliberately
     * uncreated child directory, so its two real saves reach caught File I/O. */
    CHECK(mc_game_settings_create_file(&fixture, "sound-manager-uncreated-9d386ead", NULL, NULL,
                                       &fixture.settings, NULL, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(volumes(fixture.settings, 0.0f, 0.0f));
    CHECK(mc_sound_manager_bind(fixture.manager, fixture.settings, NULL, NULL, NULL) ==
          MC_SOUND_OK);
    CHECK(mc_game_settings_attach_sound_manager(fixture.settings, fixture.manager, NULL) ==
          MC_GAME_SETTINGS_OK);
    const mc_game_settings_diagnostics diagnostics = {.context = &fixture, .emit = diagnostic};
    CHECK(mc_game_settings_set_option_float(fixture.settings,
                                            mc_game_option_by_id(MC_GAME_OPTION_SOUND), 0.75f,
                                            &diagnostics, &action, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(action.value_assigned && !action.save_reached && fixture.count == 6 &&
          fixture.calls[0].kind == 100 && fixture.calls[4].kind == 200 &&
          fixture.calls[5].kind == MC_SOUND_MANAGER_STOP &&
          strcmp(fixture.calls[5].name, "BgMusic") == 0);
    CHECK(fixture.diagnostics == 2 && fixture.diagnostic_sound[0] == 0.0f &&
          fixture.diagnostic_sound[1] == 0.75f && fixture.diagnostic_music[1] == 0.0f &&
          !fixture.invalid);
    clear_calls(&fixture);
    CHECK(mc_game_settings_set_option_float(fixture.settings,
                                            mc_game_option_by_id(MC_GAME_OPTION_MUSIC), NAN, NULL,
                                            &action, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(fixture.count == 1 && fixture.calls[0].kind == MC_SOUND_MANAGER_SET_VOLUME &&
          isnan(fixture.calls[0].command.value));
    clear_calls(&fixture);
    CHECK(volumes(fixture.settings, -0.0f, 0.75f));
    CHECK(mc_sound_manager_options_changed(fixture.manager, NULL, &report, NULL) == MC_SOUND_OK &&
          fixture.count == 1 && fixture.calls[0].kind == MC_SOUND_MANAGER_STOP);
    CHECK(add(&fixture, MC_SOUND_MANAGER_EFFECTS, "random/click1.ogg"));
    fixture.check_reentry = true;
    for (unsigned i = 1; i <= 257; ++i) {
        clear_calls(&fixture);
        CHECK(mc_sound_manager_ui(fixture.manager, effect, 1.0f, 1.0f, NULL, NULL) == MC_SOUND_OK);
        CHECK(mc_sound_manager_read_state(fixture.manager, &state) &&
              state.sound_counter == (int32_t)(i % 256u));
        char expected_name[16];
        const int length = snprintf(expected_name, sizeof(expected_name), "sound_%u", i % 256u);
        CHECK(length > 0 && (size_t)length < sizeof(expected_name) && fixture.count == 4 &&
              strcmp(fixture.calls[0].name, expected_name) == 0);
    }
    CHECK(!fixture.invalid);
    ok = true;
cleanup:
    fixture_free(&fixture);
    return ok;
}

int main(void) {
    mc_java_random_profile *profile = NULL;
    if (mc_java_random_profile_create(MC_SOUND_POOL_MAX_ALLOCATION_BYTES, &profile) !=
        MC_JAVA_RANDOM_PROFILE_OK)
        return 1;
    const bool ok = construction_and_setup(profile) && setup_failures(profile) &&
                    effects_and_streaming(profile) && music_and_listener(profile) &&
                    options_and_counter(profile);
    mc_java_random_profile_free(profile);
    if (!ok)
        return 1;
    printf("%u SoundManager checks passed\n", checks);
    return 0;
}
