#ifndef MC_CLIENT_RENDER_ITEM_BILLBOARD_H
#define MC_CLIENT_RENDER_ITEM_BILLBOARD_H

#include "../render_engine/nw.h"

typedef struct mc_client_item_billboard_context {
    mc_client_tessellator *nw;
    float uv[4]; /* Actual captured u0/u1/v0/v1 locals. */
    unsigned int source_pc, vertices;
    bool source_exception;
    const char *exception_class;
    mc_client_graphics_error error;
} mc_client_item_billboard_context;

/* Actual BB.a408..472 signed integer and binary32 UV locals, after the
 * parent's genuine NW403 capture and before its optional color child. */
bool mc_client_item_billboard_uv_capture(mc_client_item_billboard_context *, int32_t icon);

/* Actual BB.a673..771 begin/normal/four-vertex prefix on the parent's captured
 * nullable NW. No active use, GL, allocation, draw or cleanup. Success leaves
 * the genuine batch open at draw776; the parent retains the graphics owner
 * exclusively, owns its real GL transform and performs that continuation. */
bool mc_client_item_billboard_entered(mc_client_item_billboard_context *);

#endif
