/* Authored native ownership test. No original behavior/parity authority. */
#include "client/window/frame.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Frame/Canvas check failed at%d: %s\n", __LINE__, #x);                 \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

typedef struct dispatch_control {
    SDL_Window *window;
    mc_client_event_port **port;
    unsigned int quit_calls;
    bool fail, reentry_refused;
} dispatch_control;

static bool controlled_dispatch(void *context, const SDL_Event *event) {
    dispatch_control *control = context;
    if (event->type != SDL_EVENT_QUIT)
        return true;
    ++control->quit_calls;
    mc_client_event_port *extra = NULL;
    mc_client_event_record *record = NULL;
    mc_client_events_error error = {0};
    control->reentry_refused =
        control->reentry_refused &&
        mc_client_events_retain_pending(*control->port, &error) == MC_CLIENT_EVENTS_BUSY &&
        mc_client_events_take(*control->port, SDL_EVENT_QUIT, SDL_EVENT_QUIT, false, &record,
                              &error) == MC_CLIENT_EVENTS_BUSY &&
        record == NULL &&
        mc_client_events_attach(control->window, 0, &extra, &error) == MC_CLIENT_EVENTS_BUSY &&
        extra == NULL && mc_client_events_retire(control->port, &error) == MC_CLIENT_EVENTS_BUSY &&
        *control->port != NULL;
    return !control->fail;
}

static int retained_dispatch_retry(void) {
    CHECK(SDL_InitSubSystem(SDL_INIT_VIDEO));
    SDL_Window *first = SDL_CreateWindow("failed listener", 32, 32, SDL_WINDOW_HIDDEN);
    SDL_Window *second = SDL_CreateWindow("successful listener", 32, 32, SDL_WINDOW_HIDDEN);
    SDL_Window *later = SDL_CreateWindow("later listener", 32, 32, SDL_WINDOW_HIDDEN);
    CHECK(first != NULL && second != NULL && later != NULL);
    mc_client_event_port *failed = NULL, *successful = NULL, *attached_later = NULL;
    mc_client_events_error error = {0};
    CHECK(mc_client_events_attach(first, 0, &failed, &error) == MC_CLIENT_EVENTS_OK);
    CHECK(mc_client_events_attach(second, 0, &successful, &error) == MC_CLIENT_EVENTS_OK);
    dispatch_control failure = {first, &failed, 0, true, true};
    dispatch_control success = {second, &successful, 0, false, true};
    dispatch_control late = {later, &attached_later, 0, false, true};
    CHECK(mc_client_events_bind_window_dispatch(failed, controlled_dispatch, &failure, &error) ==
          MC_CLIENT_EVENTS_OK);
    CHECK(mc_client_events_bind_window_dispatch(successful, controlled_dispatch, &success,
                                                &error) == MC_CLIENT_EVENTS_OK);
    CHECK(mc_client_events_retain_pending(successful, &error) == MC_CLIENT_EVENTS_OK);
    SDL_Event quit = {0};
    quit.type = SDL_EVENT_QUIT;
    CHECK(SDL_PushEvent(&quit));
    CHECK(mc_client_events_retain_pending(successful, &error) == MC_CLIENT_EVENTS_SDL_FAILED);
    CHECK(success.quit_calls == 1 && failure.quit_calls == 1);
    mc_client_events_state state;
    CHECK(mc_client_events_inspect(successful, &state) && !state.active);
    CHECK(mc_client_events_attach(later, 0, &attached_later, &error) == MC_CLIENT_EVENTS_OK);
    CHECK(mc_client_events_bind_window_dispatch(attached_later, controlled_dispatch, &late,
                                                &error) == MC_CLIENT_EVENTS_OK);
    failure.fail = false;
    CHECK(mc_client_events_retain_pending(successful, &error) == MC_CLIENT_EVENTS_OK);
    CHECK(success.quit_calls == 1 && failure.quit_calls == 2 && late.quit_calls == 0);
    mc_client_event_record *record = NULL;
    CHECK(mc_client_events_take(successful, SDL_EVENT_QUIT, SDL_EVENT_QUIT, false, &record,
                                &error) == MC_CLIENT_EVENTS_OK &&
          record != NULL);
    mc_client_event_release(&record);

    failure.fail = true;
    CHECK(SDL_PushEvent(&quit));
    CHECK(mc_client_events_retain_pending(successful, &error) == MC_CLIENT_EVENTS_SDL_FAILED);
    CHECK(success.quit_calls == 2 && failure.quit_calls == 3 && late.quit_calls == 1);
    /* Replacement may reuse the same address and real window, but it has a
     * fresh numeric identity and must not inherit the retained callback. */
    CHECK(mc_client_events_retire(&failed, &error) == MC_CLIENT_EVENTS_OK && failed == NULL);
    CHECK(mc_client_events_attach(first, 0, &failed, &error) == MC_CLIENT_EVENTS_OK);
    CHECK(mc_client_events_bind_window_dispatch(failed, controlled_dispatch, &failure, &error) ==
          MC_CLIENT_EVENTS_OK);
    CHECK(mc_client_events_retain_pending(successful, &error) == MC_CLIENT_EVENTS_OK);
    CHECK(success.quit_calls == 2 && failure.quit_calls == 3 && late.quit_calls == 1);
    CHECK(mc_client_events_take(successful, SDL_EVENT_QUIT, SDL_EVENT_QUIT, false, &record,
                                &error) == MC_CLIENT_EVENTS_OK &&
          record != NULL);
    mc_client_event_release(&record);

    failure.fail = false;
    CHECK(SDL_PushEvent(&quit));
    CHECK(mc_client_events_retain_pending(successful, &error) == MC_CLIENT_EVENTS_OK);
    CHECK(success.quit_calls == 3 && failure.quit_calls == 4 && late.quit_calls == 2 &&
          success.reentry_refused && failure.reentry_refused && late.reentry_refused);
    CHECK(mc_client_events_take(successful, SDL_EVENT_QUIT, SDL_EVENT_QUIT, false, &record,
                                &error) == MC_CLIENT_EVENTS_OK &&
          record != NULL);
    mc_client_event_release(&record);
    CHECK(mc_client_events_retire(&failed, &error) == MC_CLIENT_EVENTS_OK);
    CHECK(mc_client_events_retire(&successful, &error) == MC_CLIENT_EVENTS_OK);
    CHECK(mc_client_events_retire(&attached_later, &error) == MC_CLIENT_EVENTS_OK);
    SDL_DestroyWindow(first);
    SDL_DestroyWindow(second);
    SDL_DestroyWindow(later);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return 0;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    SDL_SetMainReady();
    mc_client_canvas_error ce = {0};
    mc_client_source_canvas *cold = NULL;
    int32_t w, h;
    bool displayable = true;
    CHECK(SDL_WasInit(SDL_INIT_VIDEO) == 0);
    CHECK(mc_client_source_canvas_allocate(&cold, &ce) == MC_CLIENT_CANVAS_OK);
    CHECK(mc_client_source_canvas_width(cold, &w, &ce) == MC_CLIENT_CANVAS_OK && w == 0);
    CHECK(mc_client_source_canvas_height(cold, &h, &ce) == MC_CLIENT_CANVAS_OK && h == 0);
    CHECK(mc_client_source_canvas_store_size(cold, -3, 7, &ce) == MC_CLIENT_CANVAS_OK);
    CHECK(mc_client_source_canvas_width(cold, &w, &ce) == MC_CLIENT_CANVAS_OK && w == -3);
    CHECK(mc_client_source_canvas_height(cold, &h, &ce) == MC_CLIENT_CANVAS_OK && h == 7);
    CHECK(mc_client_source_canvas_displayable(cold, &displayable, &ce) == MC_CLIENT_CANVAS_OK &&
          !displayable);
    mc_client_canvas_graphics *graphics = NULL;
    CHECK(mc_client_source_canvas_get_graphics(cold, &graphics, &ce) == MC_CLIENT_CANVAS_OK &&
          graphics == NULL);
    uint64_t foreign_identity = 0;
    mc_client_source_canvas *foreign = (mc_client_source_canvas *)&foreign_identity;
    CHECK(mc_client_source_canvas_width(foreign, &w, &ce) == MC_CLIENT_CANVAS_NATIVE_STATE);
    CHECK(mc_client_source_canvas_get_graphics(foreign, &graphics, &ce) ==
              MC_CLIENT_CANVAS_NATIVE_STATE &&
          graphics == NULL);
    CHECK(mc_client_source_canvas_retain(foreign, &ce) == MC_CLIENT_CANVAS_NATIVE_STATE);
    CHECK(!mc_client_source_canvas_inspect(foreign, &(mc_client_canvas_state){0}));

    CHECK(SDL_WasInit(SDL_INIT_VIDEO) == 0);
    CHECK(mc_client_source_canvas_width(NULL, &w, &ce) == MC_CLIENT_CANVAS_NULL_REFERENCE);
    CHECK(mc_client_source_canvas_release(&cold, &ce) == MC_CLIENT_CANVAS_OK && cold == NULL);
    mc_client_source_frame *frame = NULL;
    mc_client_frame_error fe = {0};
    CHECK(mc_client_source_frame_open(854, 480, &frame, &fe));
    mc_client_frame_state state;
    CHECK(mc_client_source_frame_inspect(frame, &state));
    CHECK(state.frame_window != NULL && state.canvas_window != NULL &&
          state.frame_window != state.canvas_window);
    CHECK(SDL_GetWindowID(state.frame_window) != SDL_GetWindowID(state.canvas_window));
    CHECK(SDL_GL_GetCurrentContext() == NULL); /* No SDK active use/GL context. */
    mc_client_source_canvas *canvas = mc_client_source_frame_canvas(frame);
    CHECK(canvas == state.canvas);
    const mc_client_canvas_status graphics_status =
        mc_client_source_canvas_get_graphics(canvas, &graphics, &ce);
    if (graphics_status != MC_CLIENT_CANVAS_OK)
        fprintf(stderr, "Canvas Graphics status%d: %s (native%lu)\n", (int)graphics_status,
                ce.operation == NULL ? "unknown" : ce.operation, ce.system_error);
    CHECK(graphics_status == MC_CLIENT_CANVAS_OK && graphics != NULL);
    CHECK(mc_client_canvas_graphics_black(graphics, &ce) == MC_CLIENT_CANVAS_OK);
    CHECK(mc_client_canvas_graphics_fill_rect(graphics, -2, -2, 856, 482, &ce) ==
          MC_CLIENT_CANVAS_OK);
    CHECK(!mc_client_source_frame_retire(&frame, &fe) && fe.status == MC_CLIENT_CANVAS_BUSY &&
          frame != NULL);
    CHECK(mc_client_canvas_graphics_dispose(&graphics, &ce) == MC_CLIENT_CANVAS_OK &&
          graphics == NULL);
    CHECK(mc_client_source_canvas_retain(canvas, &ce) == MC_CLIENT_CANVAS_OK);
    CHECK(!mc_client_source_frame_retire(&frame, &fe) && fe.status == MC_CLIENT_CANVAS_BUSY);
    CHECK(mc_client_source_canvas_release(&canvas, &ce) == MC_CLIENT_CANVAS_OK && canvas == NULL);
    CHECK(SDL_GetWindowFromID(SDL_GetWindowID(state.canvas_window)) == state.canvas_window);
    CHECK(mc_client_source_frame_retire(&frame, &fe) && frame == NULL);
    CHECK(SDL_WasInit(SDL_INIT_VIDEO) == 0);
    CHECK(retained_dispatch_retry() == 0);
    CHECK(SDL_WasInit(SDL_INIT_VIDEO) == 0);
    return 0;
}
