#ifndef MC_CLIENT_CONTROLLER_RENDER_INTERPOLATION_H
#define MC_CLIENT_CONTROLLER_RENDER_INTERPOLATION_H

#include "mc/client/controller/singleplayer.h"

typedef enum mc_singleplayer_render_interpolation_status {
    MC_SINGLEPLAYER_RENDER_INTERPOLATION_OK,
    MC_SINGLEPLAYER_RENDER_INTERPOLATION_ARGUMENT,
    MC_SINGLEPLAYER_RENDER_INTERPOLATION_PHASE,
    MC_SINGLEPLAYER_RENDER_INTERPOLATION_BUSY,
    MC_SINGLEPLAYER_RENDER_INTERPOLATION_SOURCE_NULL,
    MC_SINGLEPLAYER_RENDER_INTERPOLATION_GUI_OWNER,
    MC_SINGLEPLAYER_RENDER_INTERPOLATION_GLOBAL_OWNER,
    MC_SINGLEPLAYER_RENDER_INTERPOLATION_FLOATING
} mc_singleplayer_render_interpolation_status;

typedef struct mc_singleplayer_render_interpolation_error {
    mc_singleplayer_render_interpolation_status status;
    const char *operation;
    mc_player_site site;
    bool native_policy;
} mc_singleplayer_render_interpolation_error;

typedef struct mc_singleplayer_render_interpolation_report {
    bool source_entered, arithmetic_branch, arithmetic_returned, gui_stored, global_stored;
    bool source_returned;
    unsigned int source_pc;
    float branch_f, f, g, partial, value;
    client_application *gui_app, *global_app;
} mc_singleplayer_render_interpolation_report;

/* Actual os.a(F)V. The f<=0 branch stores literal positive zero even for a
 * negative zero/negative infinity; unordered NaN follows the arithmetic branch.
 * The latter uses binary32 g+(f-g)*partial, then stores actual App.v/uq.b before
 * reloading App.g and storing n.i. A later null/admission failure preserves the
 * earlier GUI store. No tick, clamp, save, draw, GL call or fallback is supplied.
 *
 * Uses the canonical controller's existing entered guard; synchronous and
 * allocation-free. Callers keep the complete App/GUI/RenderGlobal graph live
 * and exclusive. RAW child fields are source operands; their constructors are
 * not claimed. Constructing/retiring child owners reject native field access. Owner native
 * admission preserves report. Once source is entered, report records the exact
 * reached prefix. Optional report/error must be live and disjoint from owners.
 * Unsupported floating control is checked before arithmetic46; literal-zero
 * selection is decoded from float bits without adding native FP arithmetic. */
mc_singleplayer_render_interpolation_status mc_client_singleplayer_controller_render_interpolation(
    mc_client_singleplayer_controller *, float partial,
    mc_singleplayer_render_interpolation_report *, mc_singleplayer_render_interpolation_error *);

#endif
