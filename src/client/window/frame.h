#ifndef MC_CLIENT_WINDOW_FRAME_H
#define MC_CLIENT_WINDOW_FRAME_H
#include "../display/events.h"
#include "canvas.h"
typedef struct client_application client_application;
typedef struct mc_client_source_frame mc_client_source_frame;

typedef struct mc_client_frame_error {
    mc_client_canvas_status status;
    const char *operation;
    mc_client_canvas_error canvas;
    mc_client_events_error events;
    int source_stop_status;
    unsigned long system_error;
} mc_client_frame_error;

/* Main-thread native host. Frame owns a decorated window and a distinct
 * actual embedded GL-capable Canvas peer. No source SDK active use/context.
 * Even failed allocation returns the retained partial owner for retirement. */
bool mc_client_source_frame_open(int32_t, int32_t, mc_client_source_frame **,
                                 mc_client_frame_error *);
mc_client_source_canvas *mc_client_source_frame_canvas(mc_client_source_frame *);
/* Borrow App only while its bootstrap graph is exclusive and live. Unbind
 * after Display retirement and before App retirement. No eager stop/focus. */
bool mc_client_source_frame_bind_app(mc_client_source_frame *, client_application *,
                                     mc_client_frame_error *);
bool mc_client_source_frame_show(mc_client_source_frame *, bool hidden, mc_client_frame_error *);
bool mc_client_source_frame_retire(mc_client_source_frame **, mc_client_frame_error *);

typedef struct mc_client_frame_state {
    SDL_Window *frame_window, *canvas_window;
    const mc_client_source_canvas *canvas;
    const client_application *app;
    bool active, failed, close_dispatched;
    const mc_client_frame_error *failure;
} mc_client_frame_state;

bool mc_client_source_frame_inspect(const mc_client_source_frame *, mc_client_frame_state *);
#endif
