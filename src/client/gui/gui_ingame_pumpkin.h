#ifndef MC_CLIENT_GUI_GUI_INGAME_PUMPKIN_H
#define MC_CLIENT_GUI_GUI_INGAME_PUMPKIN_H

#include "client/render_engine/nw.h"

/* Exact entered UQ.a(II)V local-NW prefix55..103. The containing HUD owns
 * the actual capture50 and preceding GL/texture work. This fragment performs
 * no draw107 or success-only state restoration. Keep NW/classes exclusive
 * and live through the containing call; words are an independent pre-draw
 * copy, rather than a borrow of backing storage used by later HUD draws. */
typedef struct mc_client_gui_pumpkin_geometry {
    unsigned int source_pc, begin_attempts, begin_returns, vertices;
    bool source_null_invocation, prefix_returned;
    size_t word_count;
    int32_t words[48];
    mc_client_graphics_error graphics;
} mc_client_gui_pumpkin_geometry;

bool mc_client_gui_pumpkin_geometry_entered(mc_client_tessellator *, int32_t width, int32_t height,
                                            mc_client_gui_pumpkin_geometry *);

#endif
