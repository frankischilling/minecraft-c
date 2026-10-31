#ifndef MC_CLIENT_GUI_LOADING_GEOMETRY_H
#define MC_CLIENT_GUI_LOADING_GEOMETRY_H
#include "../display/display.h"

/* Pure arithmetic used at the real nw vertex call sites. These do not active
 * use nw, begin/end tessellation, bind, draw, or acknowledge a lower body. */
void mc_client_loading_background_vertices(int32_t width, int32_t height,
                                           mc_client_vertex vertices[4]);
void mc_client_loading_bar_vertices(int32_t width, int32_t height, int32_t progress,
                                    mc_client_vertex gray[4], mc_client_vertex green[4]);
int32_t mc_client_loading_center_x(int32_t width, int32_t measured);
#endif
