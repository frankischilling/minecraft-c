#ifndef MC_CLIENT_DISPLAY_WORLD_FRAME_SURFACE_H
#define MC_CLIENT_DISPLAY_WORLD_FRAME_SURFACE_H

#include "display.h"

/* Reached source World-frame state only, on the canonical current Display.
 * No matrix reset, error query, swap, pump or inspection renderer is
 * supplied. Compiled-state paths remain explicit native dependencies. */
bool mc_client_display_source_cull_test(mc_client_display *, bool enabled,
                                        mc_client_display_error *);
bool mc_client_display_source_color_mask(mc_client_display *, bool red, bool green, bool blue,
                                         bool alpha, mc_client_display_error *);
bool mc_client_display_source_clear_color(mc_client_display *, float red, float green, float blue,
                                          float alpha, mc_client_display_error *);
bool mc_client_display_source_clear(mc_client_display *, uint32_t mask, mc_client_display_error *);
/* GL3.3 bridge for the reached w matrix reads, with real context admission. */
bool mc_client_display_source_frustum_matrix(mc_client_display *, bool projection, float values[16],
                                             mc_client_display_error *);

#endif
