#ifndef MC_CLIENT_STARTUP_APPLICATION_QUAD_H
#define MC_CLIENT_STARTUP_APPLICATION_QUAD_H

#include "../render_engine/nw.h"

typedef struct mc_client_application_quad_error {
    int32_t pc;
    mc_client_graphics_error graphics;
} mc_client_application_quad_error;

/* Actual Minecraft.a(IIIIII)V, including the shared nw active use at8. The
 * rectangle inherits the singleton's color and the Display's live source state.
 * Coordinates wrap as Java ints before conversion; UV arithmetic uses binary32.
 * Caller keeps the genuine class context and Display live/exclusive. No GUI
 * begin, texture lookup/bind, color reset, swap or event pump is supplied here.
 * Failure retains the actual nw prefix. Error storage is live and disjoint. */
bool mc_client_application_textured_rectangle(mc_client_render_engine_classes *,
                                              mc_client_display *, int32_t x, int32_t y,
                                              int32_t texture_x, int32_t texture_y, int32_t width,
                                              int32_t height, mc_client_application_quad_error *);

#endif
