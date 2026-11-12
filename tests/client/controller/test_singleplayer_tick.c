#include "../../../src/client/application_state.h"
#include "../../../src/client/controller/singleplayer_internal.h"
#include "../../../src/core/java/random_profile_internal.h"
#include "mc/client/controller/singleplayer_tick.h"
#include "mc/client/sound/sound_manager.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                        \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    mc_java_random_profile *profile;
    mc_java_random_constructor_input inputs[4];
    mc_entity_classes *entities;
    mc_client_singleplayer_controller *controller;
    client_application *app;
    mc_sound_manager_shared *shared;
    mc_sound_manager *manager;
    mc_game_settings *settings;
    mc_singleplayer_tick_call *call;
    mc_sound_manager_report *setup;
    size_t queries, configurations, constructions, retires;
    const char *names[4];
    size_t fail_query;
    mc_sound_status failure;
    bool music_playing, streaming_playing, reentry, invalid;
    bool fail_configuration;
} fixture;

static uint32_t bits(const float *value) {
    uint32_t result;
    memcpy(&result, value, sizeof(result));
    return result;
}

static void install(float *value, uint32_t raw) {
    memcpy(value, &raw, sizeof(raw));
}

static mc_sound_status configure(void *context, mc_sound_manager_configuration kind,
                                 mc_sound_error *error) {
    (void)kind;
    (void)error;
    fixture *f = context;
    ++f->configurations;
    return f->fail_configuration ? MC_SOUND_SOURCE_EXCEPTION : MC_SOUND_OK;
}

static mc_sound_status construct(void *context, void **out, mc_sound_error *error) {
    (void)error;
    fixture *f = context;
    ++f->constructions;
    *out = f;
    return MC_SOUND_OK;
}

static mc_sound_status command(void *context, void *backend, const mc_sound_manager_command *value,
                               mc_sound_error *error) {
    (void)backend;
    (void)value;
    (void)error;
    ((fixture *)context)->invalid = true; /* No source command exists in these empty-pool cases. */
    return MC_SOUND_INVALID_ARGUMENT;
}

static mc_sound_status playing(void *context, void *backend, const char *name, bool *out,
                               mc_sound_error *error) {
    fixture *f = context;
    if (backend != f || f->queries >= sizeof(f->names) / sizeof(f->names[0])) {
        f->invalid = true;
        return MC_SOUND_LIMIT;
    }
    f->names[f->queries++] = name;
    if (f->reentry) {
        mc_singleplayer_tick_state call;
        mc_singleplayer_tick_error small;
        mc_singleplayer_tick_call *other = NULL;
        mc_singleplayer_controller_snapshot before, after;
        mc_sound_manager_state sound;
        if (!mc_client_singleplayer_controller_read(f->controller, &before) || !before.entered ||
            !mc_client_singleplayer_controller_tick_inspect(f->call, &call) || !call.active ||
            call.source_pc != 15 || !call.g_stored || !call.sound_called ||
            call.controller != f->controller || call.app != f->app ||
            call.sound_manager != f->manager ||
            mc_client_singleplayer_controller_tick_retire(&f->call, &small) !=
                MC_SINGLEPLAYER_TICK_BUSY ||
            mc_client_singleplayer_controller_tick_begin(f->controller, &other, &small) !=
                MC_SINGLEPLAYER_TICK_BUSY ||
            other != NULL ||
            mc_client_singleplayer_controller_reset(f->controller, &small) !=
                MC_SINGLEPLAYER_TICK_BUSY ||
            mc_client_singleplayer_controller_free(&f->controller, NULL) !=
                MC_SINGLEPLAYER_CONTROLLER_BUSY ||
            !mc_client_singleplayer_controller_read(f->controller, &after) ||
            bits(&before.f) != bits(&after.f) || bits(&before.g) != bits(&after.g) ||
            before.i != after.i || before.native_holds != after.native_holds ||
            !mc_sound_manager_read_state(f->manager, &sound) || !sound.active)
            f->invalid = true;
    }
    if (f->queries == f->fail_query) {
        *error = (mc_sound_error){.status = f->failure,
                                  .operation = "authored backend query failure",
                                  .system_error = 317,
                                  .source_pc = -1,
                                  .exception = MC_SOUND_EXCEPTION_MALFORMED_URL,
                                  .random_status = MC_JAVA_RANDOM_PROFILE_RANGE};
        return f->failure;
    }
    *out = strcmp(name, "BgMusic") == 0 ? f->music_playing : f->streaming_playing;
    return MC_SOUND_OK;
}

static mc_sound_status retire_backend(void *context, void **backend, mc_sound_error *error) {
    (void)error;
    fixture *f = context;
    if (*backend != f)
        f->invalid = true;
    ++f->retires;
    *backend = NULL;
    return MC_SOUND_OK;
}

static bool create(fixture *f) {
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    uint64_t u = UINT64_C(8682522807148012);
    for (size_t n = 0; n < 4; ++n) {
        u *= UINT64_C(1181783497276652981);
        f->inputs[n] = (mc_java_random_constructor_input){u, 0};
    }
    CHECK(mc_java_random_profile_bind_constructor_inputs(f->profile, f->inputs, 4) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entities) ==
          MC_ENTITY_OK);
    CHECK(mc_client_singleplayer_controller_allocate(f->entities, f->profile, 0, &f->controller,
                                                     NULL) == MC_SINGLEPLAYER_CONTROLLER_OK);
    f->app = calloc(1, sizeof(*f->app));
    f->setup = calloc(1, sizeof(*f->setup));
    CHECK(f->app != NULL && f->setup != NULL);
    CHECK(mc_client_singleplayer_controller_construct(f->controller, f->app, NULL) ==
          MC_SINGLEPLAYER_CONTROLLER_OK);
    CHECK(mc_game_settings_create_defaults(NULL, &f->settings, NULL) == MC_GAME_SETTINGS_OK);
    const mc_sound_manager_backend backend = {.context = f,
                                              .configure = configure,
                                              .construct = construct,
                                              .command = command,
                                              .playing = playing,
                                              .retire = retire_backend};
    CHECK(mc_sound_manager_shared_create(&backend, &f->shared, NULL) == MC_SOUND_OK);
    CHECK(mc_sound_manager_create(f->shared, f->profile, MC_SOUND_POOL_MAX_ALLOCATION_BYTES,
                                  &f->manager, NULL) == MC_SOUND_OK);
    CHECK(mc_java_random_profile_constructor_inputs_complete(f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    f->app->B = f->manager;
    return true;
}

static bool destroy(fixture *f) {
    CHECK(mc_client_singleplayer_controller_tick_retire(&f->call, NULL) == MC_SINGLEPLAYER_TICK_OK);
    CHECK(mc_client_singleplayer_controller_free(&f->controller, NULL) ==
          MC_SINGLEPLAYER_CONTROLLER_OK);
    CHECK(mc_sound_manager_retire(&f->manager, NULL) == MC_SOUND_OK);
    CHECK(mc_sound_manager_shared_retire(&f->shared, NULL) == MC_SOUND_OK);
    CHECK(mc_game_settings_retire(&f->settings, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_entity_classes_free(f->entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(f->profile);
    free(f->app);
    free(f->setup);
    return true;
}

static bool same_except(const mc_singleplayer_controller_snapshot *a,
                        const mc_singleplayer_controller_snapshot *b, bool reset) {
    return a->phase == b->phase && a->a == b->a && a->b == b->b && a->c == b->c && a->d == b->d &&
           a->e == b->e && bits(&a->h) == bits(&b->h) &&
           a->base_constructor_pc == b->base_constructor_pc &&
           a->constructor_pc == b->constructor_pc && a->failed_player == b->failed_player &&
           !b->entered &&
           (reset ? bits(&b->f) == 0 && b->i == 0 && bits(&a->g) == bits(&b->g)
                  : bits(&a->f) == bits(&b->f) && a->i == b->i && bits(&a->f) == bits(&b->g));
}

static bool bits_and_nulls(fixture *f) {
    static const uint32_t values[] = {
        UINT32_C(0),          UINT32_C(0x80000000), UINT32_C(0x3f800001), UINT32_C(0xbf800001),
        UINT32_C(0x00000001), UINT32_C(0x7f800000), UINT32_C(0xff800000), UINT32_C(0x7fc12345),
        UINT32_C(0xffc54321), UINT32_C(0x7fa12345)};
    mc_singleplayer_tick_error error;
    mc_singleplayer_tick_state state;
    mc_singleplayer_controller_snapshot before, after;
    for (unsigned int mode = 0; mode < 3; ++mode) {
        for (size_t n = 0; n < sizeof(values) / sizeof(values[0]); ++n) {
            f->controller->fields.a = mode == 0 ? NULL : f->app;
            f->app->B = mode == 1 ? NULL : f->manager;
            f->controller->fields.b = true;
            f->controller->fields.c = INT32_MIN;
            f->controller->fields.d = 37;
            f->controller->fields.e = INT32_MAX;
            f->controller->fields.i = INT32_MIN + 7;
            install(&f->controller->fields.f, values[n]);
            install(&f->controller->fields.g, UINT32_C(0x7fc23456));
            install(&f->controller->fields.h, UINT32_C(0xffa34567));
            CHECK(mc_client_singleplayer_controller_read(f->controller, &before));
            CHECK(
                mc_client_singleplayer_controller_tick_begin(f->controller, &f->call, &error) ==
                (mode < 2 ? MC_SINGLEPLAYER_TICK_SOURCE_NULL_REFERENCE : MC_SINGLEPLAYER_TICK_OK));
            CHECK(mc_client_singleplayer_controller_tick_inspect(f->call, &state) &&
                  state.g_stored && !state.active && state.source_returned == (mode == 2) &&
                  state.sound_called == (mode == 2) &&
                  state.source_pc == (mode == 0   ? 12u
                                      : mode == 1 ? 15u
                                                  : 18u));
            CHECK(!error.native_policy && error.site.pc == state.source_pc &&
                  strcmp(error.site.class_name, "os") == 0 && strcmp(error.site.method, "c") == 0);
            CHECK(mc_client_singleplayer_controller_read(f->controller, &after) &&
                  same_except(&before, &after, false) &&
                  after.native_holds == before.native_holds + 1);
            if (mode == 2)
                CHECK(state.sound->status == MC_SOUND_OK && state.sound_report->source_pc == 18);
            CHECK(f->queries == 0);
            printf("CTICK %u %zu %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %d\n", mode, n,
                   bits(&after.f), bits(&after.g), bits(&after.h), after.i);
            CHECK(mc_client_singleplayer_controller_tick_retire(&f->call, NULL) ==
                  MC_SINGLEPLAYER_TICK_OK);
            before = after;
            CHECK(mc_client_singleplayer_controller_reset(f->controller, &error) ==
                      MC_SINGLEPLAYER_TICK_OK &&
                  error.site.pc == 10 && strcmp(error.site.method, "a") == 0);
            CHECK(mc_client_singleplayer_controller_read(f->controller, &after) &&
                  same_except(&before, &after, true));
            printf("CRESET %u %zu %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %d\n", mode, n,
                   bits(&after.f), bits(&after.g), bits(&after.h), after.i);
        }
    }
    f->controller->fields.a = f->app;
    f->app->B = f->manager;
    return true;
}

static void queries_clear(fixture *f) {
    f->queries = 0;
    for (size_t index = 0; index < sizeof(f->names) / sizeof(f->names[0]); ++index)
        f->names[index] = NULL;
}

static bool tick_ok(fixture *f, unsigned int queries, int32_t child_pc) {
    mc_singleplayer_tick_state view;
    queries_clear(f);
    CHECK(mc_client_singleplayer_controller_tick_begin(f->controller, &f->call, NULL) ==
          MC_SINGLEPLAYER_TICK_OK);
    CHECK(mc_client_singleplayer_controller_tick_inspect(f->call, &view) && view.source_returned &&
          view.source_pc == 18 && view.sound_returned && view.sound_report->source_pc == child_pc &&
          f->queries == queries && !f->invalid);
    if (queries > 0)
        CHECK(strcmp(f->names[0], "BgMusic") == 0);
    if (queries > 1)
        CHECK(strcmp(f->names[1], "streaming") == 0);
    CHECK(mc_client_singleplayer_controller_tick_retire(&f->call, NULL) == MC_SINGLEPLAYER_TICK_OK);
    return true;
}

static bool sound_cases(fixture *f) {
    mc_singleplayer_tick_state view;
    mc_sound_manager_state before, after;
    CHECK(mc_sound_manager_read_state(f->manager, &before) && before.music_delay == 7128);
    CHECK(mc_sound_manager_bind(f->manager, f->settings, NULL, f->setup, NULL) == MC_SOUND_OK &&
          f->setup->setup.backend_published && f->constructions == 1 && f->configurations == 4);
    f->reentry = true;
    CHECK(tick_ok(f, 2, 58));
    f->reentry = false;
    CHECK(mc_sound_manager_read_state(f->manager, &after) &&
          after.music_delay == before.music_delay - 1);
    before = after;
    f->music_playing = true;
    CHECK(tick_ok(f, 1, 129));
    f->music_playing = false;
    f->streaming_playing = true;
    CHECK(tick_ok(f, 2, 129));
    f->streaming_playing = false;
    CHECK(mc_sound_manager_read_state(f->manager, &after) &&
          after.music_delay == before.music_delay);
    /* Advance the real positive countdown, never assign a private delay. */
    for (int32_t n = 0; n < before.music_delay; ++n)
        CHECK(tick_ok(f, 2, 58));
    CHECK(tick_ok(f, 2, 129)); /* Actual empty full music list: no Random draw/command. */
    CHECK(mc_sound_manager_read_state(f->manager, &after) && after.music_delay == 0 &&
          after.random.state == before.random.state);
    mc_game_settings_state settings;
    CHECK(mc_game_settings_read_state(f->settings, &settings));
    settings.values.music_volume = -0.0f;
    CHECK(mc_game_settings_set_values(f->settings, &settings.values, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(tick_ok(f, 0, 18));
    settings.values.music_volume = 1.0f;
    CHECK(mc_game_settings_set_values(f->settings, &settings.values, NULL) == MC_GAME_SETTINGS_OK);

    for (size_t n = 1; n <= 2; ++n) {
        for (unsigned int native = 0; native < 2; ++native) {
            queries_clear(f);
            f->fail_query = n;
            f->failure = native ? MC_SOUND_IO_FAILED : MC_SOUND_SOURCE_EXCEPTION;
            install(&f->controller->fields.f, UINT32_C(0xffc12345));
            CHECK(mc_client_singleplayer_controller_tick_begin(f->controller, &f->call, NULL) ==
                  MC_SINGLEPLAYER_TICK_SOUND_FAILED);
            CHECK(mc_client_singleplayer_controller_tick_inspect(f->call, &view) && view.g_stored &&
                  !view.source_returned && !view.sound_returned &&
                  bits(&f->controller->fields.g) == UINT32_C(0xffc12345) && f->queries == n &&
                  view.error->site.pc == 15 && view.error->child.pc == (n == 1 ? 24u : 35u) &&
                  view.error->native_policy == (native != 0) && view.error->sound == view.sound &&
                  view.sound->status == f->failure && view.sound->system_error == 317 &&
                  view.sound->exception == MC_SOUND_EXCEPTION_MALFORMED_URL &&
                  view.sound->random_status == MC_JAVA_RANDOM_PROFILE_RANGE &&
                  view.sound_report->exception ==
                      (native ? MC_SOUND_MANAGER_EXCEPTION_NONE
                              : MC_SOUND_MANAGER_EXCEPTION_BACKEND_THROWABLE));
            mc_singleplayer_tick_call *original = f->call;
            const mc_sound_error captured = *view.sound;
            f->call = NULL;
            f->fail_query = 0;
            CHECK(tick_ok(f, 2, 129));
            CHECK(mc_client_singleplayer_controller_tick_inspect(original, &view) &&
                  view.sound->status == captured.status &&
                  view.sound->source_pc == captured.source_pc &&
                  view.sound->system_error == captured.system_error);
            f->call = original;
            CHECK(mc_client_singleplayer_controller_retain(f->controller) ==
                  MC_SINGLEPLAYER_CONTROLLER_OK);
            CHECK(mc_client_singleplayer_controller_free(&f->controller, NULL) ==
                  MC_SINGLEPLAYER_CONTROLLER_BUSY);
            const size_t held = f->controller->fields.native_holds;
            const size_t query_count = f->queries;
            CHECK(mc_client_singleplayer_controller_tick_retire(&f->call, NULL) ==
                      MC_SINGLEPLAYER_TICK_OK &&
                  f->controller->fields.native_holds == held - 1 && f->queries == query_count);
            CHECK(mc_client_singleplayer_controller_free(&f->controller, NULL) ==
                  MC_SINGLEPLAYER_CONTROLLER_BUSY);
            CHECK(mc_client_singleplayer_controller_release(f->controller) ==
                  MC_SINGLEPLAYER_CONTROLLER_OK);
        }
    }
    CHECK(mc_sound_manager_detach_settings(f->manager, f->settings, NULL) == MC_SOUND_OK);
    queries_clear(f);
    CHECK(mc_client_singleplayer_controller_tick_begin(f->controller, &f->call, NULL) ==
          MC_SINGLEPLAYER_TICK_SOUND_FAILED);
    CHECK(mc_client_singleplayer_controller_tick_inspect(f->call, &view) && view.g_stored &&
          view.error->child.pc == 10 && !view.error->native_policy &&
          view.sound_report->exception == MC_SOUND_MANAGER_EXCEPTION_NULL_SETTINGS &&
          f->queries == 0);
    CHECK(mc_client_singleplayer_controller_tick_retire(&f->call, NULL) == MC_SINGLEPLAYER_TICK_OK);
    return true;
}

static bool native_admission(fixture *f) {
    mc_singleplayer_tick_error error;
    const mc_singleplayer_controller_snapshot before = f->controller->fields;
    CHECK(mc_client_singleplayer_controller_tick_begin(NULL, &f->call, &error) ==
              MC_SINGLEPLAYER_TICK_INVALID_ARGUMENT &&
          error.native_policy && error.site.class_name == NULL && f->call == NULL);
    CHECK(mc_client_singleplayer_controller_tick_begin(f->controller, NULL, &error) ==
          MC_SINGLEPLAYER_TICK_INVALID_ARGUMENT);
    CHECK(mc_client_singleplayer_controller_reset(NULL, &error) ==
          MC_SINGLEPLAYER_TICK_INVALID_ARGUMENT);
    f->controller->fields.phase = MC_SINGLEPLAYER_CONTROLLER_RAW;
    CHECK(mc_client_singleplayer_controller_tick_begin(f->controller, &f->call, &error) ==
              MC_SINGLEPLAYER_TICK_INVALID_PHASE &&
          f->call == NULL);
    CHECK(mc_client_singleplayer_controller_reset(f->controller, &error) ==
          MC_SINGLEPLAYER_TICK_INVALID_PHASE);
    f->controller->fields.phase = before.phase;
    f->controller->fields.native_holds = SIZE_MAX;
    CHECK(mc_client_singleplayer_controller_tick_begin(f->controller, &f->call, &error) ==
              MC_SINGLEPLAYER_TICK_LIMIT &&
          f->call == NULL && error.native_policy && error.site.class_name == NULL);
    CHECK(bits(&before.f) == bits(&f->controller->fields.f) &&
          bits(&before.g) == bits(&f->controller->fields.g) && before.i == f->controller->fields.i);
    f->controller->fields.native_holds = before.native_holds;
    CHECK(mc_client_singleplayer_controller_tick_retire(NULL, &error) ==
          MC_SINGLEPLAYER_TICK_INVALID_ARGUMENT);
    CHECK(mc_client_singleplayer_controller_tick_retire(&f->call, &error) ==
          MC_SINGLEPLAYER_TICK_OK);
    CHECK(mc_client_singleplayer_controller_tick_begin(f->controller, &f->call, &error) ==
          MC_SINGLEPLAYER_TICK_OK);
    mc_singleplayer_tick_call *owned = f->call;
    mc_singleplayer_tick_state call_before, call_after;
    CHECK(mc_client_singleplayer_controller_tick_inspect(owned, &call_before));
    install(&f->controller->fields.f, UINT32_C(0x3f800001));
    f->controller->fields.i = 37;
    mc_singleplayer_controller_snapshot held_before, held_after;
    CHECK(mc_client_singleplayer_controller_read(f->controller, &held_before));
    const size_t queries = f->queries;
    CHECK(mc_client_singleplayer_controller_tick_begin(f->controller, &f->call, &error) ==
              MC_SINGLEPLAYER_TICK_INVALID_ARGUMENT &&
          error.native_policy && error.site.class_name == NULL && f->call == owned);
    CHECK(mc_client_singleplayer_controller_read(f->controller, &held_after) &&
          bits(&held_before.f) == bits(&held_after.f) &&
          bits(&held_before.g) == bits(&held_after.g) && held_before.i == held_after.i &&
          !held_after.entered && held_before.native_holds == held_after.native_holds &&
          f->queries == queries);
    CHECK(mc_client_singleplayer_controller_tick_inspect(owned, &call_after) &&
          call_before.status == call_after.status &&
          call_before.source_pc == call_after.source_pc && call_before.error == call_after.error &&
          call_before.sound_report == call_after.sound_report);
    CHECK(mc_client_singleplayer_controller_tick_retire(&f->call, &error) ==
              MC_SINGLEPLAYER_TICK_OK &&
          f->controller->fields.native_holds == before.native_holds);
    return true;
}

static bool null_backend(fixture *f) {
    f->fail_configuration = true;
    CHECK(mc_sound_manager_bind(f->manager, f->settings, NULL, f->setup, NULL) == MC_SOUND_OK &&
          f->setup->setup.throwable_caught && !f->setup->setup.backend_published);
    mc_game_settings_state settings;
    CHECK(mc_game_settings_read_state(f->settings, &settings));
    settings.values.music_volume = 1.0f;
    CHECK(mc_game_settings_set_values(f->settings, &settings.values, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_client_singleplayer_controller_tick_begin(f->controller, &f->call, NULL) ==
          MC_SINGLEPLAYER_TICK_SOUND_FAILED);
    mc_singleplayer_tick_state view;
    CHECK(mc_client_singleplayer_controller_tick_inspect(f->call, &view) && view.g_stored &&
          !view.error->native_policy && view.error->site.pc == 15 && view.error->child.pc == 24 &&
          view.sound_report->exception == MC_SOUND_MANAGER_EXCEPTION_NULL_BACKEND &&
          f->queries == 0);
    CHECK(mc_client_singleplayer_controller_tick_retire(&f->call, NULL) == MC_SINGLEPLAYER_TICK_OK);
    return true;
}

int main(void) {
    fixture *f = calloc(1, sizeof(*f));
    if (f == NULL)
        return 1;
    bool ok = create(f) && native_admission(f) && bits_and_nulls(f) && sound_cases(f);
    if (ok)
        ok = destroy(f);
    if (ok) {
        memset(f, 0, sizeof(*f));
        ok = create(f) && null_backend(f) && destroy(f);
    }
    free(f);
    if (!ok)
        return 1;
    printf("%u singleplayer tick/reset checks passed\n", checks);
    return 0;
}
