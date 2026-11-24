#include "test_load_world_foreign_loading.h"

#include "client/application_state.h"
#include "client/gui/loading_internal.h"
#include "client/gui/loading_screen.h"
#include "client/world/load_world.h"
#include "mc/client/saves/save_format.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "foreign Loading control line%d: %s\n", __LINE__, #condition);         \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static int foreign_save_format(client_application *app) {
    CHECK(app->aa != NULL);
    mc_client_save_error *foreign = calloc(1, sizeof(*foreign));
    mc_client_save_error *scratch = calloc(1, sizeof(*scratch));
    CHECK(foreign != NULL && scratch != NULL);
    mc_client_save_format_state initial;
    CHECK(mc_client_save_format_read_state(app->aa, &initial, scratch) == MC_CLIENT_SAVE_OK &&
          initial.phase == MC_CLIENT_SAVE_READY && !initial.active &&
          initial.pending_candidate == NULL && initial.pending_world == NULL &&
          initial.pending_reader == NULL && initial.pending_handler == NULL);
    bool untouched = true;
    CHECK(mc_client_save_format_needs_conversion(app->aa, NULL, &untouched, foreign) ==
              MC_CLIENT_SAVE_SOURCE_NULL_REFERENCE &&
          untouched);
    mc_client_save_format_state before;
    CHECK(mc_client_save_format_read_state(app->aa, &before, scratch) == MC_CLIENT_SAVE_OK &&
          before.pending_candidate != NULL);
    mc_java_file_state file_before;
    CHECK(mc_java_file_read_state(before.pending_candidate, &file_before, &scratch->file) ==
              MC_JAVA_FILE_OK &&
          file_before.phase == MC_JAVA_FILE_FAILED);
    mc_client_world_load_classes *classes = NULL;
    CHECK(mc_client_world_load_classes_create(app, &classes) == MC_CLIENT_WORLD_LOAD_OK);
    mc_client_world_load_call *call = NULL;
    CHECK(mc_client_world_load_begin(classes, NULL, NULL, 0, &call) == MC_CLIENT_WORLD_LOAD_BUSY &&
          call != NULL);
    mc_client_world_load_state load;
    CHECK(mc_client_world_load_inspect(call, &load) && load.initial_detach_returned &&
          load.last_outer_pc == 13 && load.failure != NULL && load.failure->save != NULL &&
          load.failure->save->status == MC_CLIENT_SAVE_BUSY && load.handler == NULL &&
          load.world == NULL);
    CHECK(mc_client_world_load_retire(&call) == MC_CLIENT_WORLD_LOAD_OK && call == NULL);
    CHECK(mc_client_world_load_classes_retire(&classes) == MC_CLIENT_WORLD_LOAD_OK &&
          classes == NULL);
    mc_client_save_format_state after;
    CHECK(mc_client_save_format_read_state(app->aa, &after, scratch) == MC_CLIENT_SAVE_OK &&
          after.pending_candidate == before.pending_candidate && after.holds == before.holds &&
          after.source_a == before.source_a && after.phase == before.phase && !after.active);
    mc_java_file_state file_after;
    CHECK(mc_java_file_read_state(after.pending_candidate, &file_after, &scratch->file) ==
              MC_JAVA_FILE_OK &&
          file_after.phase == file_before.phase && file_after.holds == file_before.holds &&
          file_after.path == file_before.path);
    CHECK(foreign->status == MC_CLIENT_SAVE_SOURCE_NULL_REFERENCE &&
          foreign->file.status == MC_JAVA_FILE_SOURCE_NULL_STRING);
    /* Only the original creator clears its actual failed File prefix. */
    CHECK(mc_client_save_format_clear_failure(app->aa, scratch) == MC_CLIENT_SAVE_OK);
    CHECK(mc_client_save_format_read_state(app->aa, &after, scratch) == MC_CLIENT_SAVE_OK &&
          after.pending_candidate == NULL);
    free(scratch);
    free(foreign);
    return 0;
}

int mc_test_world_load_foreign_loading(client_application *app) {
    CHECK(app != NULL && app->loading_screen != NULL && app->d > 0 && app->active_world == NULL &&
          app->h == NULL && app->i == NULL);
    mc_client_world_load_classes *classes = NULL;
    CHECK(mc_client_world_load_classes_create(app, &classes) == MC_CLIENT_WORLD_LOAD_OK &&
          classes != NULL);
    mc_client_loading_screen *loading = app->loading_screen;
    mc_client_loading_call_state initial = {0};
    CHECK(mc_client_loading_screen_call_inspect(loading, &initial) && !initial.retained);
    mc_client_loading_error *foreign_error = calloc(1, sizeof(*foreign_error));
    mc_client_loading_error *foreign_cleanup = calloc(1, sizeof(*foreign_cleanup));
    CHECK(foreign_error != NULL && foreign_cleanup != NULL);
    const int32_t original_width = app->d;
    mc_client_loading_classes *gui = mc_client_loading_screen_classes(loading);
    CHECK(gui != NULL && app->render_engine != NULL);
    mc_client_texture_lookup_state lookup_before = {0};
    CHECK(mc_client_render_engine_lookup_inspect(app->render_engine, &lookup_before) &&
          !lookup_before.pending);
    static const uint16_t key[] = {'#', '#', '/', 't', 'e', 'r', 'r',
                                   'a', 'i', 'n', '.', 'p', 'n', 'g'};
    mc_player_string *prepared_key = NULL;
    CHECK(mc_player_string_create(key, sizeof(key) / sizeof(key[0]), &prepared_key) ==
          MC_PLAYER_OK);
    CHECK(mc_player_string_retain(prepared_key) == MC_PLAYER_OK);
    mc_player_string *original_key = gui->background_literal;
    /* Declared native prepared literal: the SAME actual Loading draw opens and
     * decodes real terrain.png, then stops at the actual ## strip-transform
     * dependency77. That failure owns a real retained lookup/image prefix.
     * No completed Engine/image/frame is supplied. Restore the class slot and
     * its original occurrence immediately; keep our extra key hold through
     * original-frame retirement. A width-bound failure has no retained child. */
    gui->background_literal = prepared_key;
    const mc_client_loading_status foreign_status = mc_client_loading_screen_message(
        loading, mc_client_loading_classes_empty(gui), foreign_error);
    gui->background_literal = original_key;
    mc_player_string_release(prepared_key); /* End only the temporary class-slot hold. */
    mc_client_loading_call_state before = {0};
    const bool inspected = mc_client_loading_screen_call_inspect(loading, &before);
    if (foreign_status != MC_CLIENT_LOADING_UNIMPLEMENTED_DEPENDENCY || !inspected ||
        !before.retained || !before.lookup_failed || before.failure == NULL ||
        before.failure->lookup == NULL) {
        fprintf(stderr,
                "foreign Loading actual status=%d retained=%d lookup=%d returned=%d pc=%d "
                "operation=%.100s\n",
                (int)foreign_status, inspected && before.retained,
                inspected && before.lookup_failed, inspected && before.source_returned,
                foreign_error->site.pc,
                foreign_error->operation == NULL ? "(null)" : foreign_error->operation);
        if (foreign_error->lookup != NULL)
            fprintf(stderr, "foreign lookup actual status=%d pc=%d operation=%.100s\n",
                    (int)foreign_error->lookup->status, foreign_error->lookup->site.pc,
                    foreign_error->lookup->operation == NULL ? "(null)"
                                                             : foreign_error->lookup->operation);
    }
    CHECK(foreign_status == MC_CLIENT_LOADING_UNIMPLEMENTED_DEPENDENCY && inspected &&
          before.retained && before.lookup_failed && before.engine == app->render_engine &&
          before.failure != NULL &&
          before.failure->status == MC_CLIENT_LOADING_UNIMPLEMENTED_DEPENDENCY &&
          before.failure->site.pc == 165 && before.failure->lookup != NULL &&
          before.failure->lookup->status == MC_CLIENT_LOOKUP_UNIMPLEMENTED &&
          before.failure->lookup->site.pc == 77 && !before.source_returned);
    CHECK(mc_client_render_engine_lookup_inspect(app->render_engine, &lookup_before) &&
          lookup_before.pending && lookup_before.image_io_returned && lookup_before.image != NULL);

    mc_client_world_load_call *call = NULL;
    CHECK(mc_client_world_load_begin(classes, NULL, NULL, 0, &call) == MC_CLIENT_WORLD_LOAD_BUSY &&
          call != NULL);
    mc_client_world_load_state state;
    CHECK(mc_client_world_load_inspect(call, &state) && state.last_outer_pc == 2 &&
          state.last_change_pc == 24 && state.initial_i_cleared && !state.initial_detach_returned &&
          state.failure != NULL && state.failure->status == MC_CLIENT_WORLD_LOAD_BUSY &&
          state.failure->loading != NULL &&
          state.failure->loading->status == MC_CLIENT_LOADING_BUSY && state.handler == NULL &&
          state.world == NULL && state.player == NULL && state.input == NULL);
    CHECK(mc_client_world_load_retire(&call) == MC_CLIENT_WORLD_LOAD_OK && call == NULL);
    CHECK(mc_client_world_load_classes_retire(&classes) == MC_CLIENT_WORLD_LOAD_OK &&
          classes == NULL);

    mc_client_loading_call_state after = {0};
    CHECK(mc_client_loading_screen_call_inspect(loading, &after) && after.retained &&
          after.failure == before.failure && after.cleanup == before.cleanup &&
          after.engine == before.engine && after.source_returned == before.source_returned &&
          after.lookup_failed &&
          after.failure->status == MC_CLIENT_LOADING_UNIMPLEMENTED_DEPENDENCY &&
          after.failure->lookup == before.failure->lookup && after.failure->lookup->site.pc == 77);
    mc_client_texture_lookup_state lookup_after = {0};
    CHECK(mc_client_render_engine_lookup_inspect(app->render_engine, &lookup_after) &&
          lookup_after.pending && lookup_after.image == lookup_before.image &&
          lookup_after.stream == lookup_before.stream &&
          lookup_after.failure == lookup_before.failure &&
          lookup_after.captured_pack == lookup_before.captured_pack);
    for (size_t i = 0; i < sizeof(before.strings) / sizeof(before.strings[0]); ++i)
        CHECK(after.strings[i] == before.strings[i]);
    for (size_t i = 0; i < sizeof(before.fonts) / sizeof(before.fonts[0]); ++i)
        CHECK(after.fonts[i] == before.fonts[i]);
    CHECK(app->active_world == NULL && app->h == NULL && app->i == NULL &&
          app->d == original_width);
    /* The original creator still has its real frame and can now retire it. */
    CHECK(mc_client_loading_screen_retire_call(loading, foreign_cleanup) == MC_CLIENT_LOADING_OK);
    CHECK(mc_client_loading_screen_call_inspect(loading, &after) && !after.retained);
    CHECK(mc_client_render_engine_lookup_inspect(app->render_engine, &lookup_after) &&
          !lookup_after.pending);
    CHECK(gui->background_literal == original_key);
    mc_player_string_release(prepared_key); /* Original creator's remaining private hold. */
    free(foreign_cleanup);
    free(foreign_error);
    CHECK(foreign_save_format(app) == 0);
    return 0;
}
