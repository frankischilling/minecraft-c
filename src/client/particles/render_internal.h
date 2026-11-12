#ifndef MC_CLIENT_PARTICLES_RENDER_INTERNAL_H
#define MC_CLIENT_PARTICLES_RENDER_INTERNAL_H

#include "../render_engine/texture_lookup.h"
#include "effect_renderer_internal.h"
#include "entity/render/internal.h"
#include "mc/client/particles/render.h"
#include "mc/world/storage/source_chunk_loader.h"

typedef struct mc_effect_pickup_draw_record {
    struct mc_effect_pickup_draw_record *next;
    mc_effect_pickup_draw_receipt value;
} mc_effect_pickup_draw_record;

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
    mc_client_render_manager_draw_call *manager_draw;
    mc_client_manager_draw_error manager_failure, manager_cleanup, manager_retirement;
    mc_effect_pickup_draw_record *pickup_first, *pickup_last;
    size_t pickup_count;
    mc_client_render_engine *engines[3], *pending_lookup;
    size_t engine_count;
    mc_client_texture_lookup_error texture, texture_cleanup;
    struct mc_effect_item_draw *item;
};

bool mc_effect_draw_fail(mc_client_effect_draw_call *, mc_effect_draw_status, const char *,
                         unsigned int pc, bool native_policy);
bool mc_effect_draw_fp(mc_client_effect_draw_call *, const char *source_class, unsigned int pc);
bool mc_effect_draw_math(mc_client_effect_draw_call *, unsigned int pc);
bool mc_effect_draw_capture_math(mc_client_effect_draw_call *, unsigned int pc);
bool mc_effect_draw_particle(mc_client_effect_draw_call *, unsigned int invoke_pc);
bool mc_effect_draw_item(mc_client_effect_draw_call *, unsigned int invoke_pc);
/* Actual XW full-atlas billboard, invoked by GY.a's super call at12. */
bool mc_effect_draw_sprite(mc_client_effect_draw_call *, unsigned int invoke_pc);
/* Actual XN/QU/IM size overrides, then their inherited XW billboard call. */
bool mc_effect_draw_animated_sprite(mc_client_effect_draw_call *, unsigned int invoke_pc);
bool mc_effect_draw_item_capture(mc_client_effect_draw_call *);
bool mc_effect_draw_item_discharge(mc_client_effect_draw_call *);
void mc_effect_draw_item_free(mc_client_effect_draw_call *);
bool mc_effect_draw_pickup_retire(mc_client_effect_draw_call *);
void mc_effect_draw_pickup_free(mc_client_effect_draw_call *);

#endif
