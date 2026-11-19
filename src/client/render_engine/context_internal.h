#ifndef MC_CLIENT_RENDER_ENGINE_CONTEXT_INTERNAL_H
#define MC_CLIENT_RENDER_ENGINE_CONTEXT_INTERNAL_H
#include "internal.h"
#include "mc/client/render_engine/render_engine.h"
#include "source.h"

/* Actual native context binding. Caller owns a retained class and an active
 * source/resource call. CPU ge/nw construction never invokes these bodies. */
bool mc_client_render_context_bind(mc_client_render_engine_classes *, mc_client_display *,
                                   mc_client_graphics_site, mc_client_graphics_error *);

static inline bool mc_client_render_context_has_users(const mc_client_render_engine_classes *c) {
    return c->native_resource_count != 0 || c->command_range_count != 0 ||
           c->font_lease_count != 0 || c->scene_lease_count != 0;
}

bool mc_client_render_context_unbind_empty(mc_client_render_engine_classes *,
                                           mc_client_graphics_error *);
bool mc_client_ge_retire_ranges(mc_client_render_engine_classes *, mc_client_graphics_error *);
#endif
