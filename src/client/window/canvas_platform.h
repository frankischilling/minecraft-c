#ifndef MC_CLIENT_WINDOW_CANVAS_PLATFORM_H
#define MC_CLIENT_WINDOW_CANVAS_PLATFORM_H
#include "canvas.h"

mc_client_canvas_status mc_client_canvas_peer_graphics_acquire(SDL_Window *, void **,
                                                               mc_client_canvas_error *);
mc_client_canvas_status mc_client_canvas_peer_graphics_black(void *, int32_t, int32_t, int32_t,
                                                             int32_t, mc_client_canvas_error *);
mc_client_canvas_status mc_client_canvas_peer_graphics_release(void **, mc_client_canvas_error *);

#endif
