#ifndef MC_CLIENT_DISPLAY_STARTUP_SURFACE_H
#define MC_CLIENT_DISPLAY_STARTUP_SURFACE_H

#include "display.h"

typedef enum mc_client_startup_projection_stage {
    MC_CLIENT_STARTUP_PROJECTION_CLEAR = 23,
    MC_CLIENT_STARTUP_PROJECTION_ORTHO = 51,
    MC_CLIENT_STARTUP_PROJECTION_MODELVIEW = 67,
    MC_CLIENT_STARTUP_PROJECTION_VIEWPORT = 80,
    MC_CLIENT_STARTUP_PROJECTION_CLEAR_COLOR = 87
} mc_client_startup_projection_stage;

/* Actual Minecraft.x23..67 on the same source surface: clear inherited color
 * and depth first, then ortho1000/3000 and modelview-2000. No blend/depth/scissor/mask reset,
 * texture lookup, draw, present or input pump. Positive finite native extents
 * are required. Failure retains completed effects and reports the reached PC. */
bool mc_client_display_startup_projection(mc_client_display *, double exact_width,
                                          double exact_height, mc_client_startup_projection_stage *,
                                          mc_client_display_error *);
/* Separate x80/87 tail: the caller reads the real App.d/e only after the
 * projection calls, then sets that viewport before the next clear color. */
bool mc_client_display_startup_viewport(mc_client_display *, int32_t width, int32_t height,
                                        mc_client_startup_projection_stage *,
                                        mc_client_display_error *);

typedef enum mc_client_source_unlit_feature {
    MC_CLIENT_SOURCE_LIGHTING,
    MC_CLIENT_SOURCE_FOG
} mc_client_source_unlit_feature;

/* The concrete source shader implements the disabled lighting/fog states.
 * These are real source-state stores; enabled shading needs its later renderer.
 * Compiling either operation into a display list remains unsupported. */
bool mc_client_display_source_disable_unlit_feature(mc_client_display *,
                                                    mc_client_source_unlit_feature,
                                                    mc_client_display_error *);
/* Actual x244..264: lighting/fog disabled, alpha test enabled with GREATER0.1,
 * the comparison implemented by the source shader. No draw/swap/pump. */
bool mc_client_display_startup_alpha(mc_client_display *, mc_client_display_error *);

#endif
