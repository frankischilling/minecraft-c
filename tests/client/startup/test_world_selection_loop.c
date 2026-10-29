/* Genuine native bootstrap/menu composition with authored SDL transport events. */
#include "../world/test_load_world_foreign_loading.h"
#include "client/application_state.h"
#include "client/display/sdk_input.h"
#include "client/display/staged.h"
#include "client/gui/display_screen.h"
#include "client/gui/source_screen_initialize_internal.h"
#include "client/gui/source_screen_transitions.h"
#include "client/gui/source_world_list.h"
#include "client/gui/source_world_selection_drawing.h"
#include "client/gui/source_world_selection_internal.h"
#include "client/runtime/menu_loop.h"
#include "client/startup/application_startup.h"
#include "client/world/load_world.h"
#include "client/world/prepare_world.h"
#include "client/world/render_global_internal.h"
#include "mc/client/controller/singleplayer.h"
#include "mc/client/input/movement_input.h"
#include "mc/client/particles/effect_renderer.h"
#include "mc/core/java/java.h"
#include "mc/entity/player/player.h"
#include "mc/world/runtime/blocks.h"
#include "platform/time/millis.h"

#include <SDL3/SDL.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "World selection loop check%u at%d: %s\n", checks, __LINE__,           \
                    #condition);                                                                   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    mc_client_bootstrap *bootstrap;
    client_application *app;
    mc_client_menu_loop *loop;
    mc_client_menu_loop_state state;
} fixture;

static int frame(fixture *f) {
    const bool result = mc_client_menu_loop_frame(f->loop);
    CHECK(mc_client_menu_loop_inspect(f->loop, &f->state));
    if (!result)
        fprintf(stderr, "Reached %s.%s:%" PRId64 ": %s\n",
                f->state.source_class == NULL ? "native" : f->state.source_class,
                f->state.source_method == NULL ? "frame" : f->state.source_method,
                f->state.source_pc,
                f->state.operation == NULL ? "unknown operation" : f->state.operation);
    CHECK(result && !f->state.failed && f->state.running);
    SDL_Delay(1);
    return 0;
}

static int ticks(fixture *f, uint64_t count) {
    const uint64_t target = f->state.ticks + count, deadline = SDL_GetTicks() + 3000;
    while (f->state.ticks < target && SDL_GetTicks() < deadline)
        CHECK(frame(f) == 0);
    CHECK(f->state.ticks >= target);
    return 0;
}

static int transfer(fixture *f) {
    mc_client_events_error e = {0};
    mc_client_events_state before, after;
    mc_client_event_port *events = mc_client_display_event_port(f->app->display);
    CHECK(mc_client_events_inspect(events, &before));
    CHECK(mc_client_events_retain_pending(events, &e) == MC_CLIENT_EVENTS_OK);
    CHECK(mc_client_events_inspect(events, &after));
    CHECK(after.pump_generation == before.pump_generation && after.records > before.records);
    return 0;
}

static int push_mouse(fixture *f, mc_client_source_da *screen, int32_t x, int32_t y, bool down) {
    int width, height;
    SDL_Window *window = mc_client_display_window(f->app->display);
    CHECK(SDL_GetWindowSize(window, &width, &height));
    CHECK(screen->fields.c > 0 && screen->fields.d > 0);
    const float native_x = (float)((int64_t)x * width / screen->fields.c),
                native_y = (float)((int64_t)y * height / screen->fields.d);
    /* LG reads polled Mouse coordinates during drawing. Pushed event records
     * alone do not change SDL's device state; establish the real local pointer
     * position as well as the authored event/button occurrences below. */
    SDL_WarpMouseInWindow(window, native_x, native_y);
    SDL_Event motion = {0};
    motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.windowID = SDL_GetWindowID(window);
    motion.motion.x = native_x;
    motion.motion.y = native_y;
    CHECK(SDL_PushEvent(&motion));
    SDL_Event button = {0};
    button.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    button.button.windowID = SDL_GetWindowID(window);
    button.button.button = SDL_BUTTON_LEFT;
    button.button.down = down;
    button.button.x = native_x;
    button.button.y = native_y;
    CHECK(SDL_PushEvent(&button));
    return 0;
}

/* Keep the real button state held while moving the native pointer. No
 * additional DOWN/UP record is used to stand in for a scrollbar drag. */
static int move_mouse(fixture *f, mc_client_source_da *screen, int32_t x, int32_t y) {
    int width, height;
    SDL_Window *window = mc_client_display_window(f->app->display);
    CHECK(SDL_GetWindowSize(window, &width, &height));
    CHECK(screen->fields.c > 0 && screen->fields.d > 0);
    const float native_x = (float)((int64_t)x * width / screen->fields.c),
                native_y = (float)((int64_t)y * height / screen->fields.d);
    SDL_WarpMouseInWindow(window, native_x, native_y);
    SDL_Event motion = {0};
    motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.windowID = SDL_GetWindowID(window);
    motion.motion.x = native_x;
    motion.motion.y = native_y;
    CHECK(SDL_PushEvent(&motion));
    return 0;
}

static int push_escape(fixture *f, bool down) {
    SDL_Event key = {0};
    key.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    key.key.windowID = SDL_GetWindowID(mc_client_display_window(f->app->display));
    key.key.scancode = SDL_SCANCODE_ESCAPE;
    key.key.key = SDLK_ESCAPE;
    key.key.down = down;
    CHECK(SDL_PushEvent(&key));
    return 0;
}

static mc_client_source_ke *button(mc_client_source_da *screen, int32_t id) {
    size_t count;
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

static int push_button(fixture *f, mc_client_source_da *screen, int32_t id, bool down) {
    mc_client_source_ke *b = button(screen, id);
    CHECK(b != NULL && b->fields.g && b->fields.h);
    return push_mouse(f, screen, mc_java_i32_add(b->fields.c, b->fields.a / 2),
                      mc_java_i32_add(b->fields.d, b->fields.b / 2), down);
}

static int open_selection(fixture *f, mc_client_source_da *initial, mc_client_source_rq **out) {
    const size_t keyboard_before = f->state.keyboard_events;
    CHECK(push_button(f, initial, 1, true) == 0);
    /* FU.a(CI) is empty; RQ's inherited Escape would enter its explicit close
     * dependency. These trailing real keyboard events must drain on FU even
     * after its Singleplayer call synchronously publishes RQ. */
    CHECK(push_escape(f, true) == 0 && push_escape(f, false) == 0 && transfer(f) == 0);
    const uint64_t deadline = SDL_GetTicks() + 3000;
    while ((f->app->r == initial || f->state.keyboard_events < keyboard_before + 2) &&
           SDL_GetTicks() < deadline)
        CHECK(frame(f) == 0);
    CHECK(f->app->r != NULL && f->app->r != initial &&
          f->app->r->kind == MC_CLIENT_SCREEN_WORLD_SELECTION &&
          f->state.keyboard_events >= keyboard_before + 2);
    mc_client_source_rq *rq = (mc_client_source_rq *)f->app->r;
    CHECK(rq->fields.a == initial && rq->fields.m == -1 && rq->fields.o != NULL &&
          rq->fields.o->fields.k == -2.0f);
    bool down = false;
    mc_client_sdk_input_error input;
    CHECK(mc_client_sdk_mouse_button_down(mc_client_display_inputs(f->app->display), 0, &down,
                                          &input) == MC_CLIENT_SDK_INPUT_OK &&
          down);
    CHECK(!rq->fields.t->fields.g && !rq->fields.s->fields.g && !rq->fields.u->fields.g);
    CHECK(ticks(f, 2) == 0 && rq->fields.m == -1 && rq->fields.o->fields.k == -2.0f);
    CHECK(initial->fields.a != NULL && initial->fields.a->fields.f == 1);
    *out = rq;
    return 0;
}

static int release_mouse(fixture *f, mc_client_source_rq *rq) {
    CHECK(push_mouse(f, &rq->base, rq->base.fields.c / 2, 40, false) == 0 && transfer(f) == 0);
    const uint64_t deadline = SDL_GetTicks() + 3000;
    while (rq->fields.o->fields.k != -1.0f && SDL_GetTicks() < deadline)
        CHECK(frame(f) == 0);
    CHECK(rq->fields.o->fields.k == -1.0f && ticks(f, 1) == 0);
    return 0;
}

static int drawing_monitor_recovery(mc_client_source_rq *rq) {
    mc_client_world_selection_draw_error *pending = calloc(1, sizeof(*pending));
    CHECK(pending != NULL);
    CHECK(mc_client_source_world_selection_draw_retire(NULL) == MC_CLIENT_SCREEN_INVALID_ARGUMENT);
    CHECK(mc_client_source_world_selection_draw_retire(pending) == MC_CLIENT_SCREEN_OK);
    /* Prepared native failure receipt: no live drawing call runs here. Avoid
     * destroying a mutex to force an undefined host failure. This control
     * checks the same retained occurrence used by final draw reacquisition. */
    CHECK(mtx_lock(&rq->base.classes->mutex) == thrd_success);
    CHECK(rq->base.active_calls == 0);
    ++rq->base.active_calls;
    pending->pending_receiver = &rq->base;
    CHECK(mtx_unlock(&rq->base.classes->mutex) == thrd_success);
    CHECK(mc_client_source_world_selection_draw_retire(pending) == MC_CLIENT_SCREEN_BUSY &&
          pending->pending_receiver == &rq->base && rq->base.active_calls == 1);
    mc_client_source_da *screen = &rq->base;
    CHECK(mc_client_screen_retire(&screen, NULL) == MC_CLIENT_SCREEN_BUSY && screen == &rq->base);
    const int32_t selection = rq->fields.m;
    const uint64_t stores = rq->store_count;
    pending->monitor_lost = true;
    CHECK(mc_client_source_world_selection_draw_retire(pending) == MC_CLIENT_SCREEN_OK &&
          pending->pending_receiver == NULL && !pending->monitor_lost &&
          rq->base.active_calls == 0 && rq->fields.m == selection && rq->store_count == stores);
    CHECK(mc_client_source_world_selection_draw_retire(pending) == MC_CLIENT_SCREEN_OK);
    free(pending);
    return 0;
}

static int row_press(fixture *f, mc_client_source_rq *rq, bool populated) {
    const mc_client_world_list_fields *list = &rq->fields.o->fields;
    const int32_t y = mc_java_i32_add(
        mc_java_i32_sub(mc_java_i32_add(list->h, 4), mc_java_float_to_i32(list->m)), 8);
    CHECK(y >= list->h && y <= list->i);
    CHECK(push_mouse(f, &rq->base, rq->base.fields.c / 2, y, true) == 0 && transfer(f) == 0);
    CHECK(frame(f) == 0);
    if (populated) {
        const uint64_t deadline = SDL_GetTicks() + 3000;
        while (rq->fields.m != 0 && SDL_GetTicks() < deadline)
            CHECK(frame(f) == 0);
        if (rq->fields.m != 0 || rq->fields.o->fields.n != 0) {
            int32_t sx = 0, sy = 0;
            mc_client_sdk_input_error input_error;
            (void)mc_client_sdk_mouse_x(mc_client_display_inputs(f->app->display), &sx,
                                        &input_error);
            (void)mc_client_sdk_mouse_y(mc_client_display_inputs(f->app->display), &sy,
                                        &input_error);
            fprintf(stderr,
                    "Row state selected=%d previous=%d drag=%g scroll=%g target=%d,%d SDK=%d,%d "
                    "screen=%d,%d display=%d,%d\n",
                    rq->fields.m, rq->fields.o->fields.n, (double)rq->fields.o->fields.k,
                    (double)rq->fields.o->fields.m, rq->base.fields.c / 2, y, sx, sy,
                    rq->base.fields.c, rq->base.fields.d, f->app->d, f->app->e);
        }
        CHECK(rq->fields.m == 0 && rq->fields.o->fields.n == 0);
        CHECK(rq->fields.t->fields.g && rq->fields.s->fields.g && rq->fields.u->fields.g);
    } else {
        CHECK(ticks(f, 1) == 0 && rq->fields.m == -1);
        CHECK(!rq->fields.t->fields.g && !rq->fields.s->fields.g && !rq->fields.u->fields.g);
    }
    CHECK(f->app->active_world == NULL && f->app->h == NULL && f->app->i == NULL &&
          f->app->controller == NULL && !f->app->N);
    return 0;
}

static int capture(fixture *f, const char *path) {
    mc_client_capture pixels = {0};
    mc_client_display_error e;
    CHECK(mc_client_display_capture_rgba(f->app->display, &pixels, &e));
    CHECK(pixels.width == 854 && pixels.height == 480 && pixels.pixels != NULL);
    /* PNG presentation alpha only; the retained native RGB pixels are unchanged. */
    for (size_t y = 0; y < (size_t)pixels.height; ++y)
        for (size_t x = 0; x < (size_t)pixels.width; ++x)
            pixels.pixels[y * pixels.stride + x * 4 + 3] = 255;
    SDL_Surface *surface = SDL_CreateSurfaceFrom(
        pixels.width, pixels.height, SDL_PIXELFORMAT_RGBA32, pixels.pixels, (int)pixels.stride);
    CHECK(surface != NULL);
    const bool saved = SDL_SavePNG(surface, path);
    SDL_DestroySurface(surface);
    mc_client_capture_free(&pixels);
    CHECK(saved);
    return 0;
}

static int world_display_controls(fixture *f, mc_client_screen_classes *classes) {
    mc_client_display_screen_call *call = NULL;
    const bool had_focus = f->app->N;
    CHECK(mc_client_display_screen_run(f->app, classes, NULL, &call) ==
          MC_CLIENT_DISPLAY_SCREEN_OK);
    CHECK(call != NULL && call->source_returned && call->site.pc == 168 && call->health_read &&
          call->player_health > 0 && call->r_stored && call->target == NULL &&
          call->stats_c_returned && call->focus_returned && !call->initialized &&
          f->app->r == NULL);
    if (had_focus && call->display_active)
        CHECK(call->focus_pc == 14 && !call->focus_stored && !call->mouse_grabbed &&
              call->focus_display == NULL);
    else if (!call->display_active)
        CHECK(call->focus_pc == 6 && !call->focus_stored && !call->mouse_grabbed &&
              call->focus_display == NULL);
    else
        CHECK(call->focus_pc == 51 && call->focus_stored && call->mouse_grabbed &&
              call->focus_display != NULL && call->focus_display->source_returned &&
              call->focus_display->health_read && call->focus_display->focus_pc == 14 &&
              call->focus_display->focus_display == NULL && f->app->W == 10000 &&
              f->app->ag == mc_java_i32_add(f->app->V, 10000));
    CHECK(mc_client_display_screen_retire(&call) == MC_CLIENT_DISPLAY_SCREEN_OK && call == NULL);
    mc_entity *player = f->app->h;
    const bool focus = f->app->N;
    f->app->h = NULL;
    const mc_client_display_screen_status status =
        mc_client_display_screen_run(f->app, classes, NULL, &call);
    f->app->h = player;
    CHECK(status == MC_CLIENT_DISPLAY_SCREEN_NULL_REFERENCE && call != NULL &&
          call->site.pc == 76 && call->stats_c_returned && !call->health_read && !call->r_stored &&
          !call->focus_returned && !call->source_returned && f->app->N == focus &&
          f->app->r == NULL && f->app->i == player);
    CHECK(mc_client_display_screen_retire(&call) == MC_CLIENT_DISPLAY_SCREEN_OK && call == NULL);
    return 0;
}

static int large_drag(fixture *f, mc_client_source_rq *rq, const char *capture_path) {
    mc_client_world_list_fields *list = &rq->fields.o->fields;
    CHECK(mc_client_save_list_size(rq->fields.n) == 16 && list->m == 0.0f && list->k == -1.0f);
    const int32_t x = list->b / 2 + 127, upper = list->h + 8, lower = list->i - 8;
    const int32_t maximum = 16 * 36 - (list->i - list->h - 4);
    CHECK(maximum > 0 && lower > upper);
    CHECK(push_mouse(f, &rq->base, x, upper, true) == 0 && transfer(f) == 0);
    const uint64_t start_deadline = SDL_GetTicks() + 3000;
    while (list->k < 0 && SDL_GetTicks() < start_deadline)
        CHECK(frame(f) == 0);
    CHECK(list->k >= 0 && list->l < 0.0f && list->m == 0.0f);
    bool down = false;
    mc_client_sdk_input_error input;
    CHECK(mc_client_sdk_mouse_button_down(mc_client_display_inputs(f->app->display), 0, &down,
                                          &input) == MC_CLIENT_SDK_INPUT_OK &&
          down);
    CHECK(move_mouse(f, &rq->base, x, lower) == 0 && transfer(f) == 0);
    const uint64_t down_deadline = SDL_GetTicks() + 3000;
    while (list->m != (float)maximum && SDL_GetTicks() < down_deadline)
        CHECK(frame(f) == 0);
    CHECK(list->m == (float)maximum && list->k >= 0 && rq->fields.m == -1);
    CHECK(list->n == -1 && list->o == 0 && !rq->fields.t->fields.g && !rq->fields.s->fields.g &&
          !rq->fields.u->fields.g);
    CHECK(capture(f, capture_path) == 0);
    CHECK(mc_client_sdk_mouse_button_down(mc_client_display_inputs(f->app->display), 0, &down,
                                          &input) == MC_CLIENT_SDK_INPUT_OK &&
          down);
    CHECK(move_mouse(f, &rq->base, x, upper) == 0 && transfer(f) == 0);
    const uint64_t up_deadline = SDL_GetTicks() + 3000;
    while (list->m != 0.0f && SDL_GetTicks() < up_deadline)
        CHECK(frame(f) == 0);
    CHECK(list->m == 0.0f && list->k >= 0 && rq->fields.m == -1 && list->n == -1 && list->o == 0);
    CHECK(release_mouse(f, rq) == 0 && list->k == -1.0f);
    printf("WORLD_SELECTION drag-count=16 bottom=%d restored=0 button-held=1\n", maximum);
    return 0;
}

static int release_for_double(fixture *f, mc_client_source_rq *rq) {
    mc_client_world_list_fields *list = &rq->fields.o->fields;
    const int32_t y = list->h + 4 - mc_java_float_to_i32(list->m) + 8;
    CHECK(push_mouse(f, &rq->base, rq->base.fields.c / 2, y, false) == 0 && transfer(f) == 0);
    const uint64_t deadline = SDL_GetTicks() + 3000;
    while (list->k != -1.0f && SDL_GetTicks() < deadline)
        CHECK(frame(f) == 0);
    CHECK(list->k == -1.0f);
    return 0;
}

static bool filename_is(const mc_client_save_selection *selection, const char *ascii) {
    mc_client_save_selection_fields fields;
    if (mc_client_save_selection_read(selection, &fields, NULL) != MC_CLIENT_SAVE_OK ||
        fields.filename == NULL)
        return false;
    mc_nbt_string view = mc_world_info_name_text(fields.filename);
    if (view.length != strlen(ascii))
        return false;
    for (size_t i = 0; i < view.length; ++i)
        if (view.units[i] != (uint8_t)ascii[i])
            return false;
    return true;
}

int main(int argc, char **argv) {
    CHECK(argc == 4 && (strcmp(argv[2], "empty") == 0 || strcmp(argv[2], "populated") == 0 ||
                        strcmp(argv[2], "large") == 0 || strcmp(argv[2], "double") == 0 ||
                        strcmp(argv[2], "load") == 0 || strcmp(argv[2], "generate") == 0));
    const bool double_click = strcmp(argv[2], "double") == 0;
    const bool loading_world = strcmp(argv[2], "load") == 0 || strcmp(argv[2], "generate") == 0;
    const bool populated = strcmp(argv[2], "populated") == 0 || double_click || loading_world;
    const bool large = strcmp(argv[2], "large") == 0;
    fixture f = {0};
    const mc_client_application_options options = {.assets = argv[1],
                                                   .username = "Selection control",
                                                   .width = 854,
                                                   .height = 480,
                                                   .hidden = true};
    CHECK(mc_client_bootstrap_open(&options, &f.bootstrap) == MC_CLIENT_BOOTSTRAP_OK);
    f.app = mc_client_bootstrap_app(f.bootstrap);
    CHECK(f.app != NULL && f.app->constructor_phase == MC_CLIENT_APP_READY && f.app->r != NULL &&
          f.app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU);
    CHECK(f.app->active_world == NULL && f.app->h == NULL && f.app->i == NULL &&
          f.app->controller == NULL && !f.app->N);
    mc_client_source_da *initial = f.app->r;
    const size_t initializations = initial->initialization_count;
    mc_client_source_ke *original_press = button(initial, 1);
    CHECK(original_press != NULL);
    CHECK(mc_client_menu_loop_create(f.bootstrap, &f.loop));
    mc_client_application_startup *startup_attempt = f.app->startup;
    mc_client_startup_error *blocked = calloc(1, sizeof(*blocked));
    CHECK(blocked != NULL && f.app->screen_transitions != NULL);
    CHECK(mc_client_application_startup_retire(&startup_attempt, blocked) ==
              MC_CLIENT_STARTUP_BUSY &&
          startup_attempt == f.app->startup && f.app->display != NULL && f.app->r == initial &&
          f.app->constructor_phase == MC_CLIENT_APP_READY);
    free(blocked);
    CHECK(mc_client_menu_loop_inspect(f.loop, &f.state));
    CHECK(ticks(&f, 2) == 0);
    if (!populated && !large)
        CHECK(mc_test_world_load_foreign_loading(f.app) == 0);
    mc_client_source_rq *rq = NULL;
    CHECK(open_selection(&f, initial, &rq) == 0);
    CHECK(drawing_monitor_recovery(rq) == 0);
    CHECK(rq->fields.n != NULL && mc_client_save_list_size(rq->fields.n) == (large       ? 16u
                                                                             : populated ? 3u
                                                                                         : 0u));
    if (large) {
        CHECK(filename_is(mc_client_save_list_at(rq->fields.n, 0), "world00"));
        CHECK(filename_is(mc_client_save_list_at(rq->fields.n, 15), "world15"));
    } else {
        const int32_t count = populated ? 3 : 0;
        const mc_client_world_list_fields *list = &rq->fields.o->fields;
        const int32_t negative_limit = (count * 36 - (list->i - list->h - 4)) / 2;
        CHECK(negative_limit < 0 && list->m == (float)negative_limit);
    }
    if (populated) {
        CHECK(filename_is(mc_client_save_list_at(rq->fields.n, 0), "alpha"));
        CHECK(filename_is(mc_client_save_list_at(rq->fields.n, 1), "bravo"));
        CHECK(filename_is(mc_client_save_list_at(rq->fields.n, 2), "older"));
    }
    mc_client_source_da *external = &rq->base;
    CHECK(mc_client_screen_retain(external, NULL) == MC_CLIENT_SCREEN_OK);
    if (large) {
        CHECK(release_mouse(&f, rq) == 0 && large_drag(&f, rq, argv[3]) == 0 &&
              row_press(&f, rq, true) == 0);
    } else {
        CHECK(release_mouse(&f, rq) == 0 && row_press(&f, rq, populated) == 0);
        CHECK(capture(&f, argv[3]) == 0);
    }
    CHECK(release_mouse(&f, rq) == 0);
    CHECK(push_button(&f, &rq->base, 0, true) == 0 && push_button(&f, &rq->base, 0, false) == 0 &&
          transfer(&f) == 0);
    const uint64_t cancel_deadline = SDL_GetTicks() + 3000;
    while (f.app->r != initial && SDL_GetTicks() < cancel_deadline)
        CHECK(frame(&f) == 0);
    CHECK(f.app->r == initial && initial->initialization_count == initializations + 1);
    CHECK(rq->fields.a == initial && rq->base.fields.a == NULL &&
          initial->fields.a == original_press); /* Trailing mouse-up ran on original RQ. */
    CHECK(initial->last_initialization != NULL &&
          initial->last_initialization->phase == MC_CLIENT_SCREEN_INIT_RETURNED);
    CHECK(f.app->active_world == NULL && f.app->h == NULL && f.app->i == NULL && !f.app->N);
    if (populated) {
        mc_client_source_rq *next = NULL;
        CHECK(open_selection(&f, initial, &next) == 0 && next != rq);
        int32_t previous_index = -1;
        int64_t previous_click = 0, double_started = 0;
        if (double_click) {
            CHECK(release_mouse(&f, next) == 0 && row_press(&f, next, true) == 0);
            previous_index = next->fields.o->fields.n;
            previous_click = next->fields.o->fields.o;
            CHECK(previous_index == 0 && previous_click != 0);
            CHECK(release_for_double(&f, next) == 0);
            CHECK(next->fields.o->fields.n == previous_index &&
                  next->fields.o->fields.o == previous_click);
            CHECK(mc_platform_current_time_millis(&double_started) == MC_PLATFORM_MILLIS_OK &&
                  mc_java_i64_sub(double_started, previous_click) < 250);
            const mc_client_world_list_fields *list = &next->fields.o->fields;
            const int32_t y = list->h + 4 - mc_java_float_to_i32(list->m) + 8;
            CHECK(push_mouse(&f, &next->base, next->base.fields.c / 2, y, true) == 0 &&
                  transfer(&f) == 0);
        } else {
            CHECK(release_mouse(&f, next) == 0 && row_press(&f, next, true) == 0 &&
                  release_mouse(&f, next) == 0);
            CHECK(push_button(&f, &next->base, 1, true) == 0 &&
                  push_button(&f, &next->base, 1, false) == 0 && transfer(&f) == 0);
        }
        bool stopped = false;
        const uint64_t select_deadline = SDL_GetTicks() + 3000;
        while (!stopped && SDL_GetTicks() < select_deadline) {
            stopped = !mc_client_menu_loop_frame(f.loop);
            CHECK(mc_client_menu_loop_inspect(f.loop, &f.state));
            SDL_Delay(1);
        }
        CHECK(stopped && f.state.failed && f.state.source_class != NULL);
        mc_client_screen_transition_state transitions;
        mc_client_world_load_state load;
        CHECK(mc_client_screen_transitions_inspect(f.app->screen_transitions, &transitions) &&
              transitions.world_load != NULL &&
              mc_client_world_load_inspect(transitions.world_load, &load));
        if (loading_world &&
            (load.failure == NULL || load.failure->status != MC_CLIENT_WORLD_LOAD_OK ||
             !load.source_returned || !load.player_published)) {
            fprintf(stderr,
                    "World load reached status=%d %s.%s:%d child=%s:%d operation=%s terrain=%zu\n",
                    load.failure == NULL ? -1 : (int)load.failure->status,
                    load.failure == NULL || load.failure->site.class_name == NULL
                        ? "native"
                        : load.failure->site.class_name,
                    load.failure == NULL || load.failure->site.method == NULL
                        ? "none"
                        : load.failure->site.method,
                    load.failure == NULL ? -1 : load.failure->site.pc,
                    load.failure == NULL || load.failure->child.class_name == NULL
                        ? "none"
                        : load.failure->child.class_name,
                    load.failure == NULL ? -1 : load.failure->child.pc,
                    load.failure == NULL || load.failure->operation == NULL
                        ? "none"
                        : load.failure->operation,
                    load.terrain_samples);
            if (load.failure != NULL && load.failure->terrain != NULL &&
                load.failure->terrain->loading != NULL) {
                const mc_client_loading_error *detail = load.failure->terrain->loading;
                fprintf(stderr, "Loading failure status=%d %s.%s:%d child=%s:%d op=%s display=%s\n",
                        (int)detail->status,
                        detail->site.class_name == NULL ? "native" : detail->site.class_name,
                        detail->site.method == NULL ? "none" : detail->site.method, detail->site.pc,
                        detail->child.class_name == NULL ? "none" : detail->child.class_name,
                        detail->child.pc, detail->operation == NULL ? "none" : detail->operation,
                        detail->display.operation == NULL ? "none" : detail->display.operation);
                fprintf(stderr, "Graphics status=%d op=%s display=%d:%s detail=%s\n",
                        (int)detail->graphics.status,
                        detail->graphics.operation == NULL ? "none" : detail->graphics.operation,
                        (int)detail->graphics.display.status,
                        detail->graphics.display.operation == NULL
                            ? "none"
                            : detail->graphics.display.operation,
                        detail->graphics.display.detail);
            }
        }
        CHECK(load.failure != NULL);
        if (loading_world)
            CHECK(load.failure->status == MC_CLIENT_WORLD_LOAD_OK && load.source_returned &&
                  load.last_outer_pc == 130 && load.last_change_pc == 366 &&
                  load.renderer_returned && load.effects_returned && load.player_load_returned &&
                  load.view_published);
        else
            CHECK(load.failure->status == MC_CLIENT_WORLD_LOAD_UNIMPLEMENTED &&
                  strcmp(load.failure->site.class_name, "net/minecraft/client/Minecraft") == 0 &&
                  strcmp(load.failure->site.method, "b") == 0 && load.failure->site.pc == 52);
        CHECK(load.initial_detach_returned && load.initial_i_cleared && load.initial_h_cleared &&
              load.initial_P_stored && load.gc_requests == (loading_world ? 3u : 2u) &&
              load.conversion_checked);
        if (loading_world) {
            CHECK(!load.needs_conversion && load.handler_opened && load.world_constructed &&
                  load.world_published && load.player_published && load.input_published &&
                  load.world == f.app->active_world && load.player == f.app->h &&
                  f.app->i == f.app->h && load.input != NULL && load.terrain_samples == 289);
            mc_world_runtime_construction constructed;
            mc_client_world_prepare_state prepared;
            mc_local_player_snapshot player;
            CHECK(mc_world_runtime_construction_get(load.world, &constructed) ==
                      MC_WORLD_STORAGE_OK &&
                  constructed.mode == MC_WORLD_RUNTIME_GAME &&
                  constructed.phase == MC_WORLD_RUNTIME_READY &&
                  constructed.entry == MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE);
            CHECK(mc_client_world_prepare_inspect(load.prepare, &prepared) &&
                  prepared.source_returned && prepared.count == 289 && prepared.total == 289 &&
                  prepared.block_samples == 289 && prepared.progress_calls == 289 &&
                  prepared.progress_returns == 289 && prepared.last_progress == 99 &&
                  prepared.unused_simulation_count == 2000 && prepared.spawn_constructed &&
                  !prepared.player_x_stored && !prepared.player_z_stored);
            for (size_t sample = 0; sample < 289; ++sample) {
                const mc_client_world_prepare_sample *actual = &prepared.samples[sample];
                CHECK(actual->dx == -128 + (int32_t)(sample / 17) * 16 &&
                      actual->dz == -128 + (int32_t)(sample % 17) * 16 &&
                      actual->progress == (int32_t)(sample * 100 / 289) &&
                      actual->progress_returned && actual->block_returned &&
                      actual->block_world == load.world && actual->light_calls != 0);
            }
            CHECK(mc_local_player_get_snapshot(load.player, &player) == MC_ENTITY_OK &&
                  player.minecraft == f.app && player.input == load.input);
            int32_t marker = 0;
            mc_world_storage_error stored;
            CHECK(mc_world_runtime_block_get(load.world, 0, 127, 0, &marker, &stored) ==
                      MC_WORLD_STORAGE_OK &&
                  marker == 57);
            mc_client_sdk_input_state input;
            CHECK(f.app->r == NULL &&
                  mc_client_sdk_input_inspect(mc_client_display_inputs(f.app->display), &input) &&
                  input.grabbed == f.app->N && player.living.Y > 0);
            CHECK(strcmp(f.state.source_class, "net/minecraft/client/Minecraft") == 0 &&
                  strcmp(f.state.source_method, "k") == 0 && f.state.source_pc == 337);
            CHECK(world_display_controls(&f, initial->classes) == 0);
            printf("WORLD_LOAD ready=1 terrain=289 stored-marker=57 player=1 input=1 "
                   "renderer=1 effects=1 publication=1 view=1 returned=130 next=k337\n");
        } else {
            CHECK(load.needs_conversion && !load.handler_opened && load.world == NULL &&
                  load.player == NULL && !load.world_published);
        }
        if (!loading_world) {
            const mc_client_world_load_site *reached =
                load.failure->child.class_name == NULL ? &load.failure->site : &load.failure->child;
            CHECK(strcmp(f.state.source_class, reached->class_name) == 0 &&
                  strcmp(f.state.source_method, reached->method) == 0 &&
                  f.state.source_pc == reached->pc);
        }
        if (double_click) {
            /* MG85 failed before LG's n187/o194 and k400 stores. Both clicks
             * came through the real SDK and the formatter/clock is unmodified. */
            CHECK(next->fields.o->fields.n == previous_index &&
                  next->fields.o->fields.o == previous_click && next->fields.o->fields.k == -1.0f);
            printf("WORLD_SELECTION double-clock-gap=%" PRId64 " failed-callback-n-o-preserved=1\n",
                   mc_java_i64_sub(double_started, previous_click));
        }
        CHECK(next->fields.l && next->fields.m == 0 && f.app->controller != NULL);
        if (!loading_world)
            CHECK(f.app->r != NULL && f.app->r != &next->base &&
                  f.app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU);
        mc_singleplayer_controller_snapshot controller;
        CHECK(mc_client_singleplayer_controller_read(
            (const mc_client_singleplayer_controller *)f.app->controller, &controller));
        CHECK(controller.phase == MC_SINGLEPLAYER_CONTROLLER_READY && controller.a == f.app &&
              controller.constructor_pc == 40 && controller.c == -1 && controller.d == -1 &&
              controller.e == -1 && controller.failed_player == NULL);
        CHECK(loading_world ||
              (f.app->active_world == NULL && f.app->h == NULL && f.app->i == NULL && !f.app->N));
    } else {
        CHECK(push_button(&f, initial, 4, true) == 0 && push_button(&f, initial, 4, false) == 0 &&
              transfer(&f) == 0);
        const uint64_t quit_deadline = SDL_GetTicks() + 3000;
        while (f.state.running && SDL_GetTicks() < quit_deadline) {
            CHECK(mc_client_menu_loop_frame(f.loop));
            CHECK(mc_client_menu_loop_inspect(f.loop, &f.state));
            SDL_Delay(1);
        }
        CHECK(!f.state.running && !f.state.failed && !atomic_load(&f.app->J));
    }
    printf("WORLD_SELECTION mode=%s frames=%" PRIu64 " ticks=%" PRIu64
           " mouse=%zu keyboard=%zu held-entry=1 row=1 cancel-same-parent=1 select-frontier=%d\n",
           argv[2], f.state.frames, f.state.ticks, f.state.mouse_events, f.state.keyboard_events,
           populated ? 1 : 0);
    if (loading_world) {
        mc_world_runtime *world = f.app->active_world;
        mc_entity *player = f.app->h;
        mc_client_render_global *renderer = f.app->g;
        mc_client_render_global_error *error = calloc(1, sizeof(*error));
        CHECK(error != NULL &&
              mc_client_render_global_retain(renderer, error) == MC_CLIENT_RENDER_GLOBAL_OK);
        CHECK(!mc_client_menu_loop_retire(&f.loop) && f.loop != NULL);
        mc_client_screen_transition_state transitions;
        mc_client_world_load_state load;
        CHECK(mc_client_screen_transitions_inspect(f.app->screen_transitions, &transitions) &&
              transitions.world_load != NULL &&
              mc_client_world_load_inspect(transitions.world_load, &load) && load.source_returned &&
              load.cleanup->status == MC_CLIENT_WORLD_LOAD_BUSY && load.cleanup->renderer == NULL &&
              load.cleanup->render_global != NULL &&
              load.cleanup->render_global->status == MC_CLIENT_RENDER_GLOBAL_BUSY &&
              renderer->fields.k == world && f.app->active_world == world && f.app->h == player &&
              f.app->i == player);
        CHECK(mc_client_render_global_release(&renderer, error) == MC_CLIENT_RENDER_GLOBAL_OK);
        free(error);
        mc_client_effect_renderer *effects = f.app->effect_renderer;
        CHECK(mc_client_effect_renderer_retain(effects) == MC_EFFECT_RENDERER_OK);
        CHECK(!mc_client_menu_loop_retire(&f.loop) && f.loop != NULL);
        mc_effect_renderer_snapshot state;
        CHECK(mc_client_effect_renderer_read(effects, &state) && state.world == world &&
              f.app->active_world == world && f.app->h == player && f.app->i == player);
        CHECK(mc_client_effect_renderer_release(effects) == MC_EFFECT_RENDERER_OK);
    }
    CHECK(!mc_client_menu_loop_retire(&f.loop) && f.loop != NULL && external != NULL);
    CHECK(mc_client_bootstrap_app(f.bootstrap) == f.app &&
          f.app->native_lifetime_phase == MC_CLIENT_APPLICATION_LIFETIME_REGISTERED);
    CHECK(mc_client_screen_release(&external, NULL) == MC_CLIENT_SCREEN_OK && external == NULL);
    CHECK(mc_client_menu_loop_retire(&f.loop) && f.loop == NULL);
    mc_client_bootstrap_status closed;
    const uint64_t close_deadline = SDL_GetTicks() + 35000;
    do {
        closed = mc_client_bootstrap_close(&f.bootstrap);
        if (closed == MC_CLIENT_BOOTSTRAP_BUSY)
            SDL_Delay(1);
    } while (closed == MC_CLIENT_BOOTSTRAP_BUSY && SDL_GetTicks() < close_deadline);
    CHECK(closed == MC_CLIENT_BOOTSTRAP_OK && f.bootstrap == NULL);
    printf("WORLD_SELECTION mode=%s cleanup=1 checks=%u\n", argv[2], checks);
    return 0;
}
