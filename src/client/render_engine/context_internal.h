#ifndef MC_CLIENT_RENDER_ENGINE_CONTEXT_INTERNAL_H
#define MC_CLIENT_RENDER_ENGINE_CONTEXT_INTERNAL_H
#include "mc/client/render_engine/render_engine.h"
#include "source.h"

/* Actual native context binding. Caller owns a retained class and an active
 * source/resource call. CPU ge/nw construction never invokes these bodies. */
bool mc_client_render_context_bind(mc_client_render_engine_classes *, mc_client_display *,
                                   mc_client_graphics_site, mc_client_graphics_error *);
bool mc_client_render_context_unbind_empty(mc_client_render_engine_classes *,
                                           mc_client_graphics_error *);
bool mc_client_ge_retire_ranges(mc_client_render_engine_classes *, mc_client_graphics_error *);
#endif
