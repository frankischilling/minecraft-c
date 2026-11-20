/* The genuine App/Session fixture supplies the nullable fields read by CH. */
#define main mc_death_screen_unused_statistics_main
#include "../statistics/test_player_dispatch.c"
#undef main

#include "client/gui/source_death_screen_internal.h"
#include "client/gui/source_screen_events.h"
#include "client/gui/source_screen_initialize.h"

#include <limits.h>

static bool text_equals(mc_client_screen_text value, const char *ascii) {
    mc_player_string_view text;
    if (!mc_client_screen_text_read(value, &text) || text.length != strlen(ascii))
        return false;
    for (size_t i = 0; i < text.length; ++i)
        if (text.units[i] != (uint16_t)(unsigned char)ascii[i])
            return false;
    return true;
}

static bool screen_rows(fixture *f, mc_client_screen_classes *classes) {
    const int32_t sizes[][2] = {{427, 240}, {320, 200}, {-1, -1}, {INT32_MIN, INT32_MAX}};
    mc_player_session *actual_session = f->app.session;
    CHECK(actual_session != NULL);
    for (unsigned int mode = 0; mode < 3; ++mode) {
        for (size_t size = 0; size < sizeof(sizes) / sizeof(sizes[0]); ++size) {
            mc_client_source_ch *screen = NULL;
            mc_client_screen_error e;
            CHECK(mc_client_screen_death_allocate(classes, NULL, &screen, &e) ==
                  MC_CLIENT_SCREEN_OK);
            CHECK(mc_client_screen_death_construct(screen, &e) == MC_CLIENT_SCREEN_OK &&
                  e.source_returned && e.site.pc == 4 && screen->base.store_count == 4 &&
                  screen->base.fields.k == 0.0f && screen->base.fields.b == NULL &&
                  screen->base.fields.c == 0 && screen->base.fields.d == 0 &&
                  screen->base.fields.e != NULL && !screen->base.fields.f &&
                  screen->base.fields.g == NULL && screen->base.fields.h == NULL &&
                  screen->base.fields.a == NULL);
            screen->base.fields.b = mode == 0 ? NULL : &f->app;
            screen->base.fields.c = sizes[size][0];
            screen->base.fields.d = sizes[size][1];
            f->app.session = mode == 1 ? NULL : actual_session;
            const mc_client_screen_status status = mc_client_screen_death_initialize(screen, &e);
            f->app.session = actual_session;
            CHECK(status ==
                  (mode == 0 ? MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE : MC_CLIENT_SCREEN_OK));
            CHECK(mode != 0 || (!e.native_policy && e.site.pc == 89 && !e.source_returned));
            mc_client_screen_list_state list;
            CHECK(mc_client_screen_list_inspect(screen->base.fields.e, &list) && list.size == 2 &&
                  list.mod_count == 3);
            bool pauses = true;
            CHECK(mc_client_screen_pauses_game(&screen->base, &pauses, &e) == MC_CLIENT_SCREEN_OK &&
                  !pauses && e.source_returned && !strcmp(e.site.class_name, "ch") &&
                  e.site.pc == 1);
            printf("DEATH %u %zu %d %zu %d\n", mode, size, status == MC_CLIENT_SCREEN_OK ? 1 : 0,
                   list.size, pauses ? 1 : 0);
            for (size_t i = 0; i < 2; ++i) {
                const mc_client_source_ke *button = NULL;
                CHECK(mc_client_screen_list_button(screen->base.fields.e, i, &button) &&
                      button != NULL);
                mc_client_screen_ke_state state;
                CHECK(mc_client_screen_button_inspect((mc_client_source_ke *)button, &state, &e) ==
                      MC_CLIENT_SCREEN_OK);
                CHECK(state.phase == MC_CLIENT_SCREEN_READY && state.holds == 2 &&
                      state.fields.f == (int32_t)i + 1 && state.fields.a == 200 &&
                      state.fields.b == 20 &&
                      state.fields.c == mc_java_i32_sub(sizes[size][0] / 2, 100) &&
                      state.fields.d == mc_java_i32_add(sizes[size][1] / 4, i == 0 ? 72 : 96) &&
                      state.fields.h && state.fields.g == !(mode == 1 && i == 1) &&
                      text_equals(state.fields.e, i == 0 ? "Respawn" : "Title menu"));
                mc_player_string_view label;
                CHECK(mc_client_screen_text_read(state.fields.e, &label));
                printf("DEATHBUTTON %u %zu %zu %d %d %d %d %d %d %d ", mode, size, i,
                       state.fields.f, state.fields.c, state.fields.d, state.fields.a,
                       state.fields.b, state.fields.g ? 1 : 0, state.fields.h ? 1 : 0);
                for (size_t unit = 0; unit < label.length; ++unit)
                    printf("%04x", (unsigned int)label.units[unit]);
                putchar('\n');
            }
            /* The override reads neither App nor fields: Escape and arbitrary
             * characters return with no transition, even after failed b(). */
            screen->base.fields.b = NULL;
            mc_client_screen_event_error *event_error = calloc(1, sizeof(*event_error));
            mc_client_screen_event_report report;
            CHECK(event_error != NULL);
            for (int32_t key = 0; key < 3; ++key) {
                const mc_client_sdk_key_event event = {
                    .pressed = true, .key = key, .character = 'x'};
                CHECK(mc_client_screen_key_event(&screen->base, &event, &report, event_error) ==
                          MC_CLIENT_SCREEN_OK &&
                      report.action == MC_CLIENT_SCREEN_ACTION_NONE && report.source_returned &&
                      event_error->pending_receiver == NULL && screen->base.active_calls == 0);
                CHECK(mc_client_screen_event_retire(event_error) == MC_CLIENT_SCREEN_OK);
                CHECK(screen->base.fields.b == NULL &&
                      mc_client_screen_list_inspect(screen->base.fields.e, &list) &&
                      list.size == 2);
                printf("DEATHKEY %u %zu %d 1 2 0\n", mode, size, key);
            }
            free(event_error);
            CHECK(mc_client_screen_death_retire(&screen, &e) == MC_CLIENT_SCREEN_OK &&
                  screen == NULL);
        }
    }
    return true;
}

static bool screen_lifecycle(fixture *f, mc_client_screen_classes *classes) {
    mc_client_source_ch *screen = NULL;
    mc_client_screen_error e;
    const mc_client_screen_options quota = {1024u * 1024u, 0, 1024u * 1024u, 4096, 512};
    CHECK(mc_client_screen_death_allocate(classes, &quota, &screen, &e) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_death_construct(screen, &e) == MC_CLIENT_SCREEN_LIMIT &&
          e.site.pc == 5 && e.parent_site.pc == 1 && screen->base.phase == MC_CLIENT_SCREEN_FAILED);
    CHECK(mc_client_screen_death_retire(&screen, &e) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_death_allocate(classes, NULL, &screen, &e) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_death_construct(screen, &e) == MC_CLIENT_SCREEN_OK);
    mc_client_screen_init_error *init = calloc(1, sizeof(*init));
    CHECK(init != NULL);
    CHECK(mc_client_screen_initialize(&screen->base, &f->app, 427, 240, init) ==
          MC_CLIENT_SCREEN_OK);
    const mc_client_source_ke *first = NULL;
    CHECK(mc_client_screen_list_button(screen->base.fields.e, 0, &first) && first != NULL);
    CHECK(mc_client_screen_initialize(&screen->base, &f->app, 320, 200, init) ==
              MC_CLIENT_SCREEN_OK &&
          screen->initialization_count == 2);
    mc_client_screen_ke_state old;
    CHECK(mc_client_screen_button_inspect((mc_client_source_ke *)first, &old, &e) ==
              MC_CLIENT_SCREEN_OK &&
          old.holds == 1 && text_equals(old.fields.e, "Respawn"));
    mc_client_screen_list_state list;
    CHECK(mc_client_screen_list_inspect(screen->base.fields.e, &list) && list.size == 2 &&
          list.mod_count == 8);
    screen->base.active_calls = 1;
    CHECK(mc_client_screen_death_initialize(screen, &e) == MC_CLIENT_SCREEN_INVALID_PHASE &&
          e.native_policy && screen->initialization_count == 2);
    screen->base.active_calls = 0;
    const mc_client_screen_list *actual_list = screen->base.fields.e;
    screen->base.fields.e = NULL;
    CHECK(mc_client_screen_death_initialize(screen, &e) == MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE &&
          !e.native_policy && e.site.pc == 4 && screen->initialization_count == 3);
    screen->base.fields.e = actual_list;
    CHECK(mc_client_screen_death_retire(&screen, &e) == MC_CLIENT_SCREEN_OK);
    free(init);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    if (CD(argv[2]) != 0) {
        perror("death screen private directory");
        return 2;
    }
    fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !setup(f, argv[1], "death-screen", true))
        return 1;
    if (mc_client_application_lifetime_register(&f->app, NULL) != MC_CLIENT_APPLICATION_OWNER_OK)
        return 1;
    mc_client_screen_classes *classes = NULL;
    if (mc_client_screen_classes_create(f->app.profile, mc_resource_files_source(f->app.files),
                                        f->app.registry, &classes, NULL) != MC_CLIENT_SCREEN_OK ||
        !screen_rows(f, classes) || !screen_lifecycle(f, classes))
        return 1;
    mc_client_screen_class_state state;
    if (mc_client_screen_classes_inspect(classes, &state, NULL) != MC_CLIENT_SCREEN_OK ||
        state.phase != MC_CLIENT_SCREEN_CLASS_COLD || state.a != NULL ||
        mc_client_screen_classes_release(&classes, NULL) != MC_CLIENT_SCREEN_OK || !teardown(f) ||
        mc_client_application_lifetime_retire(&f->app, NULL) != MC_CLIENT_APPLICATION_OWNER_OK)
        return 1;
    free(f);
    printf("Death screen source: %zu checks\n", checks);
    return 0;
}
