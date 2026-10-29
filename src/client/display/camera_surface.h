#ifndef MC_CLIENT_DISPLAY_CAMERA_SURFACE_H
#define MC_CLIENT_DISPLAY_CAMERA_SURFACE_H
#include "source_surface.h"
/* Ordered GL3.3 source-matrix operations on the canonical selected matrix.
 * Each checks context and FP immediately before native matrix work, retains
 * previous effects on failure, and performs no error query/reset/frame work.
 * Matrix storage/composition uses the existing native binary32 profile. */
bool mc_client_display_source_rotate(mc_client_display *, float, float, float, float,
                                     mc_client_display_error *);
bool mc_client_display_source_scale(mc_client_display *, double, double, double,
                                    mc_client_display_error *);
bool mc_client_display_source_perspective(mc_client_display *, float, float, float, float,
                                          mc_client_display_error *);
/* Passive native ledger snapshot for tests and later frustum composition.
 * No GL query, context switch, FP arithmetic or source call is implied. */
bool mc_client_display_camera_matrices_inspect(const mc_client_display *, float projection[16],
                                               float modelview[16]);
#endif
