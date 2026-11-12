#include "client/application_state.h"
#include "client/display/staged.h"
#include "client/gui/source_screen_initialize.h"
#include "client/gui/source_screen_transitions.h"
#include "client/render_engine/effect_upload.h"
#include "client/runtime/menu_loop.h"
#include "client/startup/application_startup_internal.h"
#include "mc/client/gui/gui_ingame.h"

#include <SDL3/SDL.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "menu loop check failed at%d: %s\n", __LINE__, #condition);            \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static bool same_source_state(const mc_client_menu_loop_state *a,
                              const mc_client_menu_loop_state *b) {
    return a->frames == b->frames && a->ticks == b->ticks && a->texture_ticks == b->texture_ticks &&
           a->account_checks_omitted == b->account_checks_omitted &&
           a->font_measurements == b->font_measurements && a->mouse_events == b->mouse_events &&
           a->keyboard_events == b->keyboard_events && a->failed == b->failed &&
           a->running == b->running && a->operation == b->operation &&
           a->source_class == b->source_class && a->source_method == b->source_method &&
           a->source_pc == b->source_pc;
}

static bool getters_closed(mc_client_bootstrap *bootstrap) {
    return mc_client_bootstrap_app(bootstrap) == NULL &&
           mc_client_bootstrap_resources(bootstrap) == NULL &&
           mc_client_bootstrap_class_files(bootstrap) == NULL &&
           mc_client_bootstrap_trig(bootstrap) == NULL &&
           mc_client_bootstrap_math(bootstrap) == NULL &&
           mc_client_bootstrap_graphics(bootstrap) == NULL;
}

/* Compare actual source references and native graph ownership while a real
 * foreign consumer keeps TH READY. Consumer count is checked separately at
 * each actual attach/detach boundary; no count grants cleanup authority. */
static bool same_manager_graph(const mc_client_manager_state *a, const mc_client_manager_state *b) {
    return a->phase == MC_CLIENT_MANAGER_READY && b->phase == a->phase &&
           a->source_views_available && b->source_views_available && a->a == b->a &&
           a->candidate == b->candidate && a->entries == b->entries &&
           a->completed_bw_stores == b->completed_bw_stores && a->class_bytes == b->class_bytes &&
           a->manager_holds == b->manager_holds && a->displaced_hands == b->displaced_hands &&
           a->hand_occurrence == b->hand_occurrence && a->fields.o == b->fields.o &&
           a->fields.p == b->fields.p && a->fields.e == b->fields.e && a->fields.f == b->fields.f &&
           a->fields.g == b->fields.g && a->fields.h == b->fields.h && a->fields.k == b->fields.k &&
           memcmp(&a->fields.i, &b->fields.i, sizeof(a->fields.i)) == 0 &&
           memcmp(&a->fields.j, &b->fields.j, sizeof(a->fields.j)) == 0 &&
           memcmp(&a->fields.l, &b->fields.l, sizeof(a->fields.l)) == 0 &&
           memcmp(&a->fields.m, &b->fields.m, sizeof(a->fields.m)) == 0 &&
           memcmp(&a->fields.n, &b->fields.n, sizeof(a->fields.n)) == 0 &&
           memcmp(&a->b, &b->b, sizeof(a->b)) == 0 && memcmp(&a->c, &b->c, sizeof(a->c)) == 0 &&
           memcmp(&a->d, &b->d, sizeof(a->d)) == 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 2);
    mc_client_application_options options = {
        .assets = argv[1], .username = "Menu control", .width = 854, .height = 480, .hidden = true};
    mc_client_bootstrap *bootstrap = NULL;
    CHECK(mc_client_bootstrap_open(&options, &bootstrap) == MC_CLIENT_BOOTSTRAP_OK);
    client_application *app = mc_client_bootstrap_app(bootstrap);
    CHECK(app != NULL && app->constructor_phase == MC_CLIENT_APP_READY && app->r != NULL);
    CHECK(app->active_world == NULL && app->h == NULL && app->i == NULL && !app->N);
    /* Independently exercise the actual nine startup registrations on the
     * genuine App before entering the timed menu profile. No World is supplied. */
    mc_client_effect_upload_error *effects = calloc(1, sizeof(*effects));
    CHECK(effects != NULL);
    CHECK(mc_client_render_engine_update_effects(app->render_engine, app->display, effects));
    CHECK(effects->source_returned && effects->updated_occurrences == 9 &&
          effects->atlas_uploads == 15);
    free(effects);
    mc_client_menu_loop *loop = NULL;
    CHECK(mc_client_menu_loop_create(bootstrap, &loop));
    mc_client_menu_loop_state state;
    const uint64_t start = SDL_GetTicks();
    do {
        CHECK(mc_client_menu_loop_frame(loop));
        CHECK(mc_client_menu_loop_inspect(loop, &state));
        CHECK(!state.failed && state.running);
        SDL_Delay(1);
    } while (SDL_GetTicks() - start < 1200);
    CHECK(state.frames > 1 && state.ticks >= 10 && state.ticks == (uint64_t)app->V);
    CHECK(state.font_measurements >= state.frames * 5 && state.texture_ticks <= state.ticks);
    CHECK(atomic_load(&app->o) && app->W == 10000 && app->ag == app->V + 10000 && app->P != 0);
    CHECK(app->K != NULL); /* The real one-second FPS String path executed. */
    mc_client_gui_ingame_state ingame;
    CHECK(mc_client_gui_ingame_inspect(app->v, &ingame, NULL) == MC_CLIENT_GUI_INGAME_OK);
    CHECK(ingame.fields.h == app->V && ingame.fields.j == 0);
    mc_client_screen_state screen;
    CHECK(mc_client_screen_inspect(app->r, &screen, NULL) == MC_CLIENT_SCREEN_OK);
    CHECK(screen.is_menu && screen.fu.i == (float)state.ticks);
    /* Prepared threshold control, using the real Timer for subsequent ticks.
     * This is separate from the normal elapsed-time observations above. */
    app->V = 5999;
    const uint64_t account_deadline = SDL_GetTicks() + 1000;
    while (app->V == 5999 && SDL_GetTicks() < account_deadline) {
        CHECK(mc_client_menu_loop_frame(loop));
        CHECK(mc_client_menu_loop_inspect(loop, &state));
        SDL_Delay(1);
    }
    CHECK(app->V >= 6000 && state.account_checks_omitted == 1 && !state.failed && state.running);
    mc_client_application_class_state application_class;
    CHECK(mc_client_application_classes_inspect(app->source_namespace, &application_class));
    CHECK(application_class.H == 0 && atomic_load(&app->J));
    size_t button_count;
    CHECK(mc_client_screen_list_size(screen.da.e, &button_count) && button_count == 5);
    mc_client_screen_ke_state quit = {0};
    for (size_t i = 0; i < button_count; ++i) {
        const mc_client_source_ke *button = NULL;
        mc_client_screen_ke_state candidate;
        CHECK(mc_client_screen_list_button(screen.da.e, i, &button));
        CHECK(mc_client_screen_button_inspect((mc_client_source_ke *)button, &candidate, NULL) ==
              MC_CLIENT_SCREEN_OK);
        if (candidate.fields.f == 4)
            quit = candidate;
    }
    CHECK(quit.fields.f == 4 && quit.fields.g && quit.fields.h);
    const int32_t gui_x = quit.fields.c + quit.fields.a / 2;
    const int32_t gui_y = quit.fields.d + quit.fields.b / 2;
    int width, height;
    CHECK(SDL_GetWindowSize(mc_client_display_window(app->display), &width, &height));
    const float x = (float)((int64_t)gui_x * width / screen.da.c);
    const float y = (float)((int64_t)gui_y * height / screen.da.d);
    SDL_Event event = {0};
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.windowID = SDL_GetWindowID(mc_client_display_window(app->display));
    event.button.button = SDL_BUTTON_LEFT;
    event.button.down = true;
    event.button.x = x;
    event.button.y = y;
    CHECK(SDL_PushEvent(&event));
    event.type = SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.down = false;
    CHECK(SDL_PushEvent(&event));
    const uint64_t deadline = SDL_GetTicks() + 2000;
    while (state.running && SDL_GetTicks() < deadline) {
        CHECK(mc_client_menu_loop_frame(loop));
        CHECK(mc_client_menu_loop_inspect(loop, &state));
        SDL_Delay(1);
    }
    CHECK(!state.running && !state.failed && !atomic_load(&app->J));
    CHECK(state.mouse_events >= 2);
    CHECK(mc_client_screen_inspect(app->r, &screen, NULL) == MC_CLIENT_SCREEN_OK &&
          screen.da.a == NULL);
    CHECK(app->active_world == NULL && app->h == NULL && app->i == NULL && !app->N);
    printf("SOURCE_MENU frames=%" PRIu64 " ticks=%" PRIu64 " mouse=%zu measured=%zu quit=1\n",
           state.frames, state.ticks, state.mouse_events, state.font_measurements);
    /* The real source Quit above is unchanged. These controls exercise only
     * native no-World shutdown on that same fully constructed App and loop. */
    const mc_client_menu_loop_state source = state;
    const bool source_J = atomic_load(&app->J), source_o = atomic_load(&app->o);
    mc_client_application_startup *startup = app->startup;
    mc_client_render_manager_classes *manager = startup->manager_classes;
    mc_client_manager_state manager_before, manager_after;
    mc_client_manager_consumer foreign = {0};
    CHECK(manager != NULL && mc_client_render_manager_inspect(manager, &manager_before));
    CHECK(manager_before.phase == MC_CLIENT_MANAGER_READY && manager_before.native_consumers == 1 &&
          manager_before.entries == 28 && manager_before.hand_occurrence != NULL);
    CHECK(mc_client_render_manager_consumer_attach(manager, &foreign, NULL) ==
          MC_CLIENT_MANAGER_OK);
    CHECK(foreign.attached && foreign.classes == manager);
    CHECK(mc_client_render_manager_inspect(manager, &manager_before) &&
          manager_before.native_consumers == 2);
    mc_client_screen_transitions *transitions = app->screen_transitions;
    mc_client_source_da *menu = app->r;
    mc_client_render_engine *engine = app->render_engine;
    mc_client_bitmap_font *font = app->font_renderer;
    mc_game_settings *settings = app->settings;
    CHECK(transitions != NULL && !state.terminal_prepare_selected && !state.terminal_prepared &&
          !state.terminal_tail_entered && startup->app == app);
    CHECK(mc_client_bootstrap_close(&bootstrap) == MC_CLIENT_BOOTSTRAP_BUSY && bootstrap != NULL);
    CHECK(getters_closed(bootstrap) && app->startup == startup && startup->app == app &&
          app->screen_transitions == transitions && app->r == menu);
    CHECK(mc_client_render_manager_inspect(manager, &manager_after) &&
          same_manager_graph(&manager_before, &manager_after) &&
          manager_after.native_consumers == 2);
    CHECK(mc_client_menu_loop_inspect(loop, &state) && same_source_state(&source, &state) &&
          !state.terminal_prepare_selected && !state.terminal_prepared &&
          !state.terminal_tail_entered);
    mc_client_menu_loop *blocked = NULL;
    CHECK(!mc_client_menu_loop_create(bootstrap, &blocked) && blocked == NULL);

    mc_client_menu_loop *same_loop = loop;
    CHECK(!mc_client_menu_loop_terminal_retire(&loop) && loop == same_loop);
    CHECK(mc_client_menu_loop_inspect(loop, &state) && same_source_state(&source, &state));
    CHECK(state.terminal_prepare_selected && state.terminal_prepared &&
          !state.terminal_tail_entered && state.retirement_bootstrap == MC_CLIENT_BOOTSTRAP_BUSY &&
          state.retirement_world == NULL && state.retirement_operation != NULL &&
          strcmp(state.retirement_operation, "menu terminal dispatcher retirement") == 0);
    CHECK(startup->terminal_manager_consumer_selected && startup->terminal_manager_consumer_ended &&
          !startup->manager_consumer.attached && startup->manager_consumer.classes == NULL &&
          startup->manager_classes == NULL && startup->manager_capture == NULL);
    CHECK(foreign.attached && foreign.classes == manager &&
          mc_client_render_manager_inspect(manager, &manager_after) &&
          same_manager_graph(&manager_before, &manager_after) &&
          manager_after.native_consumers == 1);
    CHECK(!mc_client_menu_loop_frame(loop));
    CHECK(mc_client_menu_loop_inspect(loop, &state) && same_source_state(&source, &state));
    CHECK(atomic_load(&app->J) == source_J && atomic_load(&app->o) == source_o &&
          app->active_world == NULL && app->h == NULL && app->i == NULL && !app->N &&
          app->r == menu && app->render_engine == engine && app->font_renderer == font &&
          app->settings == settings && app->screen_transitions == transitions);
    /* Early close must not bypass LL-before-TH after its own startup record
     * has ended. The actual foreign consumer still owns its admission right. */
    CHECK(mc_client_bootstrap_close(&bootstrap) == MC_CLIENT_BOOTSTRAP_BUSY && bootstrap != NULL);
    CHECK(getters_closed(bootstrap) && mc_client_render_manager_inspect(manager, &manager_after) &&
          same_manager_graph(&manager_before, &manager_after) &&
          manager_after.native_consumers == 1);
    CHECK(!mc_client_menu_loop_terminal_retire(&loop) && loop == same_loop);
    CHECK(mc_client_menu_loop_inspect(loop, &state) && same_source_state(&source, &state) &&
          state.terminal_prepared && !state.terminal_tail_entered &&
          state.retirement_bootstrap == MC_CLIENT_BOOTSTRAP_BUSY && state.retirement_world == NULL);
    CHECK(mc_client_render_manager_consumer_detach(&foreign, NULL) == MC_CLIENT_MANAGER_OK &&
          !foreign.attached && foreign.classes == NULL);
    CHECK(mc_client_render_manager_inspect(manager, &manager_after) &&
          same_manager_graph(&manager_before, &manager_after) &&
          manager_after.native_consumers == 0);
    const bool retired = mc_client_menu_loop_terminal_retire(&loop);
    if (!retired && mc_client_menu_loop_inspect(loop, &state))
        fprintf(stderr, "Menu terminal refusal: %s (bootstrap=%d, tail=%d)\n",
                state.retirement_operation == NULL ? "none" : state.retirement_operation,
                (int)state.retirement_bootstrap, (int)state.terminal_tail_entered);
    CHECK(retired && loop == NULL);
    CHECK(mc_client_menu_loop_terminal_retire(&loop) && loop == NULL);
    CHECK(getters_closed(bootstrap) && app->screen_transitions == NULL &&
          atomic_load(&app->J) == source_J && atomic_load(&app->o) == source_o &&
          app->active_world == NULL && app->h == NULL && app->i == NULL && !app->N);
    puts("SOURCE_MENU terminal=1 early_close=1 foreign_th=1 source_preserved=1");
    mc_client_bootstrap_status closed;
    const uint64_t close_deadline = SDL_GetTicks() + 35000;
    do {
        closed = mc_client_bootstrap_close(&bootstrap);
        if (closed == MC_CLIENT_BOOTSTRAP_BUSY)
            SDL_Delay(1);
    } while (closed == MC_CLIENT_BOOTSTRAP_BUSY && SDL_GetTicks() < close_deadline);
    CHECK(closed == MC_CLIENT_BOOTSTRAP_OK && bootstrap == NULL);
    puts("SOURCE_MENU cleanup=1");
    return 0;
}
