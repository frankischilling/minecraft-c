/* Entered YC Event component on a genuine bootstrap graph. App.r remains the
 * real startup FU when the retained YC receiver is initialized and invoked.
 * This does not claim ordinary current-App.r input selection or visible input.
 * Two SDL clicks reach real Mouse.next, sound, dirty and display/Stats bodies;
 * checked partial retirement proves the resulting FU/RQ parent custody order. */
#include "client/application_state.h"
#include "client/display/events.h"
#include "client/display/sdk_input.h"
#include "client/display/staged.h"
#include "client/gui/display_screen.h"
#include "client/gui/loading_screen.h"
#include "client/gui/source_screen_events.h"
#include "client/gui/source_screen_initialize.h"
#include "client/gui/source_screen_internal.h"
#include "client/gui/source_screen_transitions.h"
#include "client/gui/source_sign_screen.h"
#include "client/gui/source_world_selection.h"
#include "client/runtime/menu_loop.h"
#include "client/startup/application_startup_internal.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned int checks;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "Sign Event check%u line%d: %s\n", checks, __LINE__, #c);              \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    mc_client_bootstrap *bootstrap;
    mc_client_menu_loop *loop;
    client_application *app;
    mc_client_source_yc *sign;
    mc_tile_entity *tile;
    mc_client_source_da *older, *newer, *selection; /* Actual independent external holds. */
    mc_client_screen_event_error event;
    mc_client_screen_event_report report;
    SDL_WindowID window;
} fixture;

static mc_client_source_ke *button(mc_client_source_da *screen, int32_t id) {
    size_t count = 0;
    if (!mc_client_screen_list_size(screen->fields.e, &count))
        return NULL;
    for (size_t i = 0; i < count; ++i) {
        const mc_client_source_ke *candidate = NULL;
        mc_client_screen_ke_state state;
        if (!mc_client_screen_list_button(screen->fields.e, i, &candidate) ||
            mc_client_screen_button_inspect((mc_client_source_ke *)candidate, &state, NULL) !=
                MC_CLIENT_SCREEN_OK)
            return NULL;
        if (state.fields.f == id)
            return (mc_client_source_ke *)candidate;
    }
    return NULL;
}

static bool push_click(fixture *f, mc_client_source_da *screen, int32_t id, unsigned int count) {
    mc_client_source_ke *actual = button(screen, id);
    mc_client_screen_ke_state state;
    SDL_Window *window = mc_client_display_window(f->app->display);
    int width = 0, height = 0;
    CHECK(actual != NULL &&
          mc_client_screen_button_inspect(actual, &state, NULL) == MC_CLIENT_SCREEN_OK &&
          state.fields.g && state.fields.h && screen->fields.c > 0 && screen->fields.d > 0 &&
          SDL_GetWindowSize(window, &width, &height));
    const int32_t x = state.fields.c + state.fields.a / 2;
    const int32_t y = state.fields.d + state.fields.b / 2;
    SDL_Event event = {0};
    event.button.windowID = f->window;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = (float)((int64_t)x * width / screen->fields.c);
    event.button.y = (float)((int64_t)y * height / screen->fields.d);
    for (unsigned int i = 0; i < count; ++i) {
        event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
        event.button.down = true;
        CHECK(SDL_PushEvent(&event));
        event.type = SDL_EVENT_MOUSE_BUTTON_UP;
        event.button.down = false;
        CHECK(SDL_PushEvent(&event));
    }
    return true;
}

static bool input(fixture *f, mc_client_source_da *receiver) {
    mc_client_events_error events = {0};
    mc_client_sdk_input_error sdk = {0};
    mc_client_sdk_input *actual = mc_client_display_inputs(f->app->display);
    CHECK(mc_client_events_retain_pending(mc_client_display_event_port(f->app->display), &events) ==
              MC_CLIENT_EVENTS_OK &&
          mc_client_sdk_mouse_poll(actual, &sdk) == MC_CLIENT_SDK_INPUT_OK &&
          mc_client_sdk_keyboard_poll(actual, &sdk) == MC_CLIENT_SDK_INPUT_OK);
    CHECK(mc_client_screen_handle_input_with_transitions(receiver, actual,
                                                         f->app->screen_transitions, &f->report,
                                                         &f->event) == MC_CLIENT_SCREEN_OK &&
          f->report.source_returned && f->event.pending_receiver == NULL && !f->event.monitor_lost);
    return true;
}

static bool finish_event(fixture *f) {
    mc_client_screen_error error;
    CHECK(mc_client_screen_event_adopt_sign_displays(&f->event, f->app->screen_transitions,
                                                     &error) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_event_retire(&f->event) == MC_CLIENT_SCREEN_OK &&
          f->event.sign_display_count == 0 && f->event.sign_display == NULL);
    return true;
}

static bool run(fixture *f, const char *assets) {
    const mc_client_application_options options = {
        .assets = assets, .username = "Sign Events", .width = 854, .height = 480, .hidden = true};
    CHECK(mc_client_bootstrap_open(&options, &f->bootstrap) == MC_CLIENT_BOOTSTRAP_OK);
    f->app = mc_client_bootstrap_app(f->bootstrap);
    CHECK(f->app != NULL && f->app->constructor_phase == MC_CLIENT_APP_READY &&
          f->app->active_world == NULL && f->app->h == NULL && !f->app->N && f->app->r != NULL &&
          f->app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU);
    SDL_Window *window = mc_client_display_window(f->app->display);
    f->window = SDL_GetWindowID(window);
    CHECK(f->window != 0 && (SDL_GetWindowFlags(window) & SDL_WINDOW_HIDDEN) != 0);
#ifdef _WIN32
    /* Native hidden-component placement after bootstrap; startup GL placement
     * is outside this observation. No show, raise, grab or physical click. */
    int count = 0;
    SDL_DisplayID *displays = SDL_GetDisplays(&count);
    CHECK(displays != NULL && count >= 2);
    const SDL_DisplayID monitor = displays[1];
    SDL_free(displays);
    CHECK(SDL_SetWindowPosition(window, (int)SDL_WINDOWPOS_CENTERED_DISPLAY(monitor),
                                (int)SDL_WINDOWPOS_CENTERED_DISPLAY(monitor)) &&
          SDL_GetDisplayForWindow(window) == monitor);
#endif
    CHECK(mc_client_stats_writer_join(f->app->stats_writer, NULL) == MC_CLIENT_STATS_OK &&
          mc_client_menu_loop_create(f->bootstrap, &f->loop));
    mc_client_source_da *initial = f->app->r;
    CHECK(input(f, initial) && f->report.actions == 0 && f->report.click_calls == 0 &&
          finish_event(f)); /* Consume the real initial queue; never clear/discard it. */
    mc_client_screen_classes *classes = initial->classes;
    const mc_client_sign_screen_dependencies deps = {
        .mapping = mc_client_loading_classes_mapping(f->app->startup->font_classes),
        .input = mc_client_display_inputs(f->app->display),
        .literal_classes = f->app->world_classes};
    mc_client_screen_error error;
    mc_client_screen_init_error initialization;
    CHECK(mc_tile_entity_allocate_in_classes(f->app->world_classes, MC_TILE_ENTITY_SIGN,
                                             &f->tile) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_construct(f->tile) == MC_TILE_ENTITY_OK &&
          mc_client_screen_sign_allocate(classes, &deps, NULL, &f->sign, &error) ==
              MC_CLIENT_SCREEN_OK &&
          mc_client_screen_sign_construct(f->sign, f->tile, &error) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_initialize(mc_client_screen_sign_base(f->sign), f->app,
                                      initial->fields.c, initial->fields.d,
                                      &initialization) == MC_CLIENT_SCREEN_OK &&
          f->app->r == initial);
    mc_client_sdk_input_state sdk;
    CHECK(mc_client_sdk_input_inspect(deps.input, &sdk) && sdk.repeat_enabled && !sdk.grabbed);
    mc_client_screen_transition_state before, adopted;
    CHECK(mc_client_screen_transitions_inspect(f->app->screen_transitions, &before) &&
          push_click(f, mc_client_screen_sign_base(f->sign), 0, 2) &&
          input(f, mc_client_screen_sign_base(f->sign)) && f->report.mouse_events == 4 &&
          f->report.presses == 2 && f->report.releases == 2 && f->report.click_calls == 2 &&
          f->report.actions == 2 && f->event.sign_display_count == 2);
    const mc_client_display_screen_call *older = NULL, *newer = NULL;
    CHECK(mc_client_screen_event_sign_display(&f->event, 0, &older) &&
          mc_client_screen_event_sign_display(&f->event, 1, &newer) && older != newer &&
          older->source_returned && newer->source_returned && older->fallback != NULL &&
          newer->fallback != NULL && older->fallback != newer->fallback &&
          older->old_screen == initial &&
          newer->old_screen == mc_client_screen_menu_base(older->fallback) &&
          older->stats_c_returned && newer->stats_c_returned && older->initialized &&
          newer->initialized && older->r_stored && newer->r_stored &&
          older->stats_receiver == f->app->stats_writer &&
          newer->stats_receiver == f->app->stats_writer &&
          !older->statistics_report.file_exception_caught &&
          !newer->statistics_report.file_exception_caught &&
          f->app->r == mc_client_screen_menu_base(newer->fallback));
    mc_client_source_da *older_base = mc_client_screen_menu_base(older->fallback);
    mc_client_source_da *newer_base = mc_client_screen_menu_base(newer->fallback);
    CHECK(mc_client_screen_retain(older_base, &error) == MC_CLIENT_SCREEN_OK);
    f->older = older_base;
    CHECK(mc_client_screen_retain(newer_base, &error) == MC_CLIENT_SCREEN_OK);
    f->newer = newer_base;
    CHECK(finish_event(f) &&
          mc_client_screen_transitions_inspect(f->app->screen_transitions, &adopted) &&
          adopted.calls == before.calls && adopted.screens == before.screens);
    CHECK(push_click(f, f->newer, 1, 1) && input(f, f->newer) && f->report.click_calls == 1 &&
          f->report.actions == 1 && f->app->r != NULL &&
          f->app->r->kind == MC_CLIENT_SCREEN_WORLD_SELECTION && finish_event(f));
    mc_client_source_rq *rq = (mc_client_source_rq *)f->app->r;
    mc_client_world_selection_state selection;
    CHECK(mc_client_screen_world_selection_inspect(rq, &selection, &error) == MC_CLIENT_SCREEN_OK &&
          selection.fields.a == f->newer && selection.fields.n != NULL);
    mc_client_source_da *selection_base = mc_client_screen_world_selection_base(rq);
    CHECK(mc_client_screen_retain(selection_base, &error) == MC_CLIENT_SCREEN_OK);
    f->selection = selection_base;
    CHECK(push_click(f, f->selection, 0, 1) && input(f, f->selection) &&
          f->report.click_calls == 1 && f->report.actions == 1 && f->app->r == f->newer &&
          finish_event(f));
    mc_tile_entity_snapshot tile;
    mc_client_sign_screen_state sign;
    CHECK(mc_tile_entity_get_snapshot(f->tile, &tile) == MC_TILE_ENTITY_OK && tile.world == NULL &&
          tile.sign_read_flag && tile.sign_edit_row == -1 &&
          mc_client_screen_sign_inspect(f->sign, &sign, &error) == MC_CLIENT_SCREEN_OK &&
          sign.detail->dirty_returned && sign.detail->display_returned &&
          mc_client_screen_sign_terminal_prepare(f->sign, &error) == MC_CLIENT_SCREEN_OK &&
          mc_client_screen_sign_retire(&f->sign, &error) == MC_CLIENT_SCREEN_OK &&
          f->sign == NULL && mc_client_sdk_input_inspect(deps.input, &sdk) && sdk.repeat_enabled);
    mc_tile_entity_release(f->tile);
    f->tile = NULL;
    /* Every Source invocation has ended. Genuine external holds force checked
     * retirement at successively older owners, without forging graph slots. */
    mc_client_screen_state owner;
    CHECK(!mc_client_menu_loop_retire(&f->loop) && f->loop != NULL && f->app->r == f->newer &&
          older->fallback != NULL && newer->fallback != NULL &&
          mc_client_screen_inspect(f->selection, &owner, &error) == MC_CLIENT_SCREEN_OK &&
          owner.holds == 2 &&
          mc_client_screen_inspect(f->newer, &owner, &error) == MC_CLIENT_SCREEN_OK &&
          owner.holds == 3);
    CHECK(mc_client_screen_release(&f->selection, &error) == MC_CLIENT_SCREEN_OK &&
          f->selection == NULL && !mc_client_menu_loop_retire(&f->loop) && f->loop != NULL &&
          mc_client_screen_inspect(f->newer, &owner, &error) == MC_CLIENT_SCREEN_OK &&
          owner.holds == 2);
    CHECK(mc_client_screen_release(&f->newer, &error) == MC_CLIENT_SCREEN_OK && f->newer == NULL);
    newer = NULL; /* This borrowed call may now be consumed by the actual owner. */
    CHECK(!mc_client_menu_loop_retire(&f->loop) && f->loop != NULL && f->app->r == NULL &&
          older->fallback != NULL && mc_client_screen_menu_base(older->fallback) == f->older &&
          mc_client_screen_inspect(f->older, &owner, &error) == MC_CLIENT_SCREEN_OK &&
          owner.holds == 2);
    older = NULL;
    CHECK(mc_client_screen_release(&f->older, &error) == MC_CLIENT_SCREEN_OK && f->older == NULL &&
          mc_client_menu_loop_retire(&f->loop) && f->loop == NULL &&
          f->app->screen_transitions == NULL);
    return true;
}

static bool cleanup(fixture *f) {
    bool ok = true;
    if (f->app != NULL && f->app->screen_transitions != NULL &&
        mc_client_screen_event_adopt_sign_displays(&f->event, f->app->screen_transitions, NULL) !=
            MC_CLIENT_SCREEN_OK)
        ok = false;
    if (mc_client_screen_event_retire(&f->event) != MC_CLIENT_SCREEN_OK)
        ok = false;
    if (mc_client_screen_release(&f->selection, NULL) != MC_CLIENT_SCREEN_OK ||
        mc_client_screen_release(&f->newer, NULL) != MC_CLIENT_SCREEN_OK ||
        mc_client_screen_release(&f->older, NULL) != MC_CLIENT_SCREEN_OK)
        ok = false;
    if (f->sign != NULL &&
        (mc_client_screen_sign_terminal_prepare(f->sign, NULL) != MC_CLIENT_SCREEN_OK ||
         mc_client_screen_sign_retire(&f->sign, NULL) != MC_CLIENT_SCREEN_OK))
        ok = false;
    mc_tile_entity_release(f->tile);
    f->tile = NULL;
    if (!mc_client_menu_loop_retire(&f->loop))
        ok = false;
    mc_client_bootstrap_status status;
    const uint64_t deadline = SDL_GetTicks() + 5000;
    do {
        status = mc_client_bootstrap_close(&f->bootstrap);
        if (status == MC_CLIENT_BOOTSTRAP_BUSY)
            SDL_Delay(5);
    } while (status == MC_CLIENT_BOOTSTRAP_BUSY && SDL_GetTicks() < deadline);
    if (status != MC_CLIENT_BOOTSTRAP_OK || f->bootstrap != NULL ||
        (f->window != 0 && SDL_GetWindowFromID(f->window) != NULL))
        ok = false;
    return ok;
}

int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    fixture *f = calloc(1, sizeof(*f));
    if (f == NULL)
        return 1;
    const bool passed = run(f, argv[1]);
    const bool retired = cleanup(f);
    free(f);
    if (!passed || !retired)
        return 1;
    printf("PASS: Sign Event component ownership, %u checks; queued-clicks=4 Done=2 "
           "sound-returns=4 Stats-Done=2 fallback-FU=2 RQ=1 cancel=1 checked-BUSY=3 "
           "windows-closed=1 current-App-r-selection=0\n",
           checks);
    return 0;
}
