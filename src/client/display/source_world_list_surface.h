#ifndef MC_CLIENT_DISPLAY_SOURCE_WORLD_LIST_SURFACE_H
#define MC_CLIENT_DISPLAY_SOURCE_WORLD_LIST_SURFACE_H
#include "display.h"
/* Reached LG blend calls on the actual context. No GetError/reset or draw. */
bool mc_client_display_world_list_blend(mc_client_display *, bool, mc_client_display_error *);
bool mc_client_display_world_list_blend_function(mc_client_display *, mc_client_display_error *);
/* GL3.3 has no fixed-function glShadeModel. Record the reached FLAT state;
 * subsequent canonical source batches admit constant vertex color within each
 * decoded primitive, with independent colors across primitives.
 * LG fade strips select actual SMOOTH before their varying-alpha geometry. */
bool mc_client_display_world_list_flat(mc_client_display *, mc_client_display_error *);
#endif
