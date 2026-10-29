#include "client/application_state.h"
#include "client/display/staged.h"
#include "client/gui/source_screen_initialize.h"
#include "client/render_engine/effect_upload.h"
#include "client/runtime/menu_loop.h"
#include "mc/client/gui/gui_ingame.h"

#include <SDL3/SDL.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "menu loop check failed at%d: %s\n", __LINE__, #condition);            \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

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
    mc_client_menu_loop_free(loop);
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
