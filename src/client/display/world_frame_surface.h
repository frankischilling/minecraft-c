#ifndef MC_CLIENT_DISPLAY_WORLD_FRAME_SURFACE_H
#define MC_CLIENT_DISPLAY_WORLD_FRAME_SURFACE_H

#include "display.h"

/* Reached source World-frame state only, on the canonical current Display.
 * No clear, matrix reset, error query, swap, pump or inspection renderer is
 * supplied. Compiled-state paths remain explicit native dependencies. */
bool mc_client_display_source_cull_test(mc_client_display *, bool enabled,
                                        mc_client_display_error *);
bool mc_client_display_source_color_mask(mc_client_display *, bool red, bool green, bool blue,
                                         bool alpha, mc_client_display_error *);

#endif
