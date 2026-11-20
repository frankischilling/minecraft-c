#ifndef MC_CLIENT_WINDOW_FRAME_PLATFORM_H
#define MC_CLIENT_WINDOW_FRAME_PLATFORM_H
#include "frame.h"
bool mc_client_frame_native_embed(SDL_Window *, SDL_Window *, mc_client_frame_error *);
bool mc_client_frame_native_focus(SDL_Window *, mc_client_frame_error *);
#endif
