#ifndef MC_CLIENT_WORLD_RENDER_GLOBAL_SKY_H
#define MC_CLIENT_WORLD_RENDER_GLOBAL_SKY_H

#include "mc/client/world/render_global.h"
#include "mc/core/math/classes.h"
#include "mc/entity/vector/vector.h"

typedef struct mc_client_render_global_sky_call mc_client_render_global_sky_call;
typedef struct mc_client_render_global_sky_error mc_client_render_global_sky_error;

typedef struct mc_client_render_global_sky_state {
    bool active, retiring, returned, nether_skipped;
    bool sky_returned, sky_anaglyph, top_called, lighting_disabled;
    bool sunrise_returned, sunrise_present, sunrise_anaglyph, fan_drawn;
    bool rain_returned, sun_drawn, moon_drawn, stars_returned, stars_called;
    bool horizon_returned, horizon_tint, bottom_called, depth_restored;
    unsigned int source_pc, gl_calls, vertices, draws, matrix_pushes, matrix_pops;
    float partial, sky_rgb[3], center_rgba[4], edge_rgb[3];
    float sunrise_angle, sun_angle, rain_attenuation, star_brightness;
    int32_t top_list, star_list, bottom_list;
    uint32_t sun_texture, moon_texture;
    mc_client_render_global *owner;
    const mc_entity_vector *sky_vector;
    const float *sunrise; /* Same h[4], borrowed through captured Dimension hold. */
    const mc_client_render_global_sky_error *failure, *cleanup;
} mc_client_render_global_sky_state;

/* Actual client n.a(F)V, distinct from constructor sky-list compilation.
 * Uses this SAME n's k/t/l/y/z/A, NW singleton and actual compiled source
 * programs. The initial App.f.t.c true branch returns16 before any GL, NW,
 * Math or n.k operation. Normal execution draws the upper fogged list, real
 * sunrise fan, sun/moon textures, stars and lower list in source order.
 *
 * Native admission preserves initially NULL out. An allocated admitted call
 * owns an independent n occurrence and blocks overlapping n calls until
 * retirement; success/failure is terminal. Reached source, list, texture, NW
 * and GL effects survive. No inspection scene, implicit fog/camera setup,
 * frame, pool reset, swap, rollback or source-finally is inserted.
 *
 * App/Settings/Registry/Entity/Display and complete source graph stay live,
 * exclusive and on the Display thread through checked retirement. Captured
 * Worlds, Dimensions, Engines, Math and pooled sky Vec3 have independent
 * occurrences. Pooled vectors and canonical h arrays may still be overwritten
 * by actual later source calls; copied observed scalar diagnostics are stable.
 * Outputs/errors are live and disjoint from all graph/storage owners. */
mc_client_render_global_status
mc_client_render_global_sky_begin(mc_client_render_global *, mc_math_classes *, float partial,
                                  mc_client_render_global_sky_call **out,
                                  mc_client_render_global_sky_error *admission_error);
bool mc_client_render_global_sky_inspect(const mc_client_render_global_sky_call *,
                                         mc_client_render_global_sky_state *);
/* Native cleanup only. Ends released borrowed views and retained lookup
 * prefixes without PopMatrix, Draw, GL restoration or source method replay.
 * A refusal preserves every remaining occurrence and diagnostic for retry. */
mc_client_render_global_status
mc_client_render_global_sky_retire(mc_client_render_global_sky_call **,
                                   mc_client_render_global_sky_error *cleanup_error);

#endif
