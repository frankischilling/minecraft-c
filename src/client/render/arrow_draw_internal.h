#ifndef MC_CLIENT_RENDER_ARROW_DRAW_INTERNAL_H
#define MC_CLIENT_RENDER_ARROW_DRAW_INTERNAL_H
#include "living_draw_internal.h"

/* Actual MC bridge/typed entry on an independently retained genuine renderer
 * and Arrow. Neither entry acquires services before their reached Source use.
 * The typed seam also supports original-emitter callers which bypass TH. */
bool mc_client_arrow_draw_entered(mc_client_draw_scope *, mc_client_managed_renderer *, mc_entity *,
                                  double, double, double, float, float, mc_client_graphics_site,
                                  mc_client_living_draw_error *);
bool mc_client_arrow_draw_typed_entered(mc_client_draw_scope *, mc_client_managed_renderer *,
                                        mc_entity *, double, double, double, float, float,
                                        mc_client_graphics_site, mc_client_living_draw_error *);
#endif
