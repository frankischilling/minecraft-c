#ifndef MC_CLIENT_PARTICLES_RENDER_INTERNAL_H
#define MC_CLIENT_PARTICLES_RENDER_INTERNAL_H

#include "../render_engine/texture_lookup.h"
#include "effect_renderer_internal.h"
#include "mc/client/particles/render.h"

struct mc_client_effect_draw_call {
    mc_effect_draw_state state;
    mc_effect_draw_error failure, cleanup;
    mc_effect_draw_services services;
    bool services_captured, graphics_lease;
    mc_client_render_engine_classes *graphics;
    mc_math_classes *math;
    mc_client_tessellator *nw;
    mc_entity *current;
    mc_world_runtime *world_binding;
    mc_client_render_manager *manager;
    mc_client_render_engine *engines[3], *pending_lookup;
    size_t engine_count;
    mc_client_texture_lookup_error texture, texture_cleanup;
};

bool mc_effect_draw_fail(mc_client_effect_draw_call *, mc_effect_draw_status, const char *,
                         unsigned int pc, bool native_policy);
bool mc_effect_draw_fp(mc_client_effect_draw_call *, const char *source_class, unsigned int pc);
bool mc_effect_draw_math(mc_client_effect_draw_call *, unsigned int pc);
bool mc_effect_draw_particle(mc_client_effect_draw_call *, unsigned int invoke_pc);

#endif
