#ifndef MC_CLIENT_GUI_SOURCE_INVENTORY_QUAD_KERNELS_H
#define MC_CLIENT_GUI_SOURCE_INVENTORY_QUAD_KERNELS_H

#include "../render_engine/nw.h"

typedef struct mc_inventory_quad_rgba {
    float red, green, blue, alpha;
} mc_inventory_quad_rgba;

typedef struct mc_inventory_quad_colors {
    mc_inventory_quad_rgba upper, lower;
} mc_inventory_quad_colors;

typedef struct mc_inventory_quad_context {
    mc_client_tessellator *nw;
    mc_client_graphics_error error;
    unsigned int source_pc, vertices;
    bool source_exception;
    const char *exception_class;
} mc_inventory_quad_context;

/* Actual UB.a color locals through112. Call before the parent's GL117..144;
 * the entered NW body consumes these same captured binary32 values. */
bool mc_inventory_quad_gradient_colors_capture(mc_inventory_quad_context *, uint32_t upper,
                                               uint32_t lower, mc_inventory_quad_colors *);

/* Entered UB.a154..222 and UB.b15..163 on the parent's captured genuine NW.
 * No active use, GL, draw, allocation or cleanup occurs here. Success leaves
 * the actual batch open at the source draw227/168 continuation. The parent
 * retains the namespace and live UB.k field borrow exclusively through use.
 * source_k addresses the already genuine nonnull UB receiver's field; a NULL
 * borrow is a native argument failure at first use30, after the actual begin.
 * Tile reads that field afresh at30/68/106/142, before each UV conversion. */
bool mc_inventory_quad_gradient_entered(mc_inventory_quad_context *,
                                        const mc_inventory_quad_colors *, int32_t left, int32_t top,
                                        int32_t right, int32_t bottom);
bool mc_inventory_quad_tile_entered(mc_inventory_quad_context *, const float *source_k, int32_t x,
                                    int32_t y, int32_t u, int32_t v, int32_t width, int32_t height);

#endif
