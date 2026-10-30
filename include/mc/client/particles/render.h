#ifndef MC_CLIENT_PARTICLES_RENDER_H
#define MC_CLIENT_PARTICLES_RENDER_H

#include "ge.h"
#include "mc/client/particles/effect_renderer.h"
#include "mc/client/render/manager/manager.h"
#include "mc/core/math/classes.h"
#include "mc/entity/particles/origin.h"
#include "mc/world/runtime/brightness.h"

typedef struct mc_client_effect_draw_call mc_client_effect_draw_call;
typedef struct mc_client_texture_lookup_error mc_client_texture_lookup_error;
typedef struct mc_client_tessellator mc_client_tessellator;

typedef enum mc_effect_draw_status {
    MC_EFFECT_DRAW_OK,
    MC_EFFECT_DRAW_ARGUMENT,
    MC_EFFECT_DRAW_BUSY,
    MC_EFFECT_DRAW_LIMIT,
    MC_EFFECT_DRAW_ALLOCATION,
    MC_EFFECT_DRAW_SOURCE_NULL,
    MC_EFFECT_DRAW_SOURCE_INDEX,
    MC_EFFECT_DRAW_UNSUPPORTED_FP,
    MC_EFFECT_DRAW_UNSUPPORTED_NUMERIC,
    MC_EFFECT_DRAW_CHILD_FAILED,
    MC_EFFECT_DRAW_DEPENDENCY,
    MC_EFFECT_DRAW_CLEANUP_FAILED
} mc_effect_draw_status;

typedef struct mc_effect_draw_error {
    mc_effect_draw_status status;
    const char *operation;
    mc_client_graphics_site site, child;
    bool native_policy;
    mc_entity_error entity;
    mc_math_class_error math;
    mc_world_brightness_error brightness;
    mc_client_graphics_error graphics;
    mc_client_display_error display;
    const mc_client_texture_lookup_error *texture;
    mc_effect_renderer_status owner;
    mc_client_manager_error manager;
} mc_effect_draw_error;

/* Actual enclosing namespaces, not source DN World/Engine/camera operands.
 * The empty lit branch never reads this tuple. Retain only reached services. */
typedef struct mc_effect_draw_services {
    mc_client_render_engine_classes *graphics;
    mc_math_classes *math;
    mc_client_display *display;
    mc_client_render_manager_classes *manager;
} mc_effect_draw_services;

typedef struct mc_effect_draw_state {
    bool active, returned, retiring, lit, empty, nw_captured;
    unsigned int source_pc, last_store_pc;
    int32_t layer, index;
    size_t size_reads, visited, orientation_values, origins_stored;
    float partial, cos_yaw, sin_yaw, horizontal_y, horizontal_x, cos_pitch;
    mc_particle_origin origin;
    float fraction, brightness;
    double draw_x, draw_y, draw_z;
    unsigned int particle_pc;
    bool brightness_returned, color_set;
    mc_client_effect_renderer *owner;
    const mc_entity *camera; /* Borrowed; its enclosing graph stays live/exclusive. */
    mc_entity *current;
    const mc_client_tessellator *nw;
    const mc_effect_draw_error *failure, *cleanup;
} mc_effect_draw_state;

/* Actual DN.b(SN,F) and DN.a(SN,F), respectively, on the existing four Lists.
 * Native admission owns one call/EffectRenderer occurrence before the first
 * source read. A failed admitted call is returned and must retire, not replay.
 * Lit checks actual layer3 first; an empty List returns16 without camera,
 * Math/NW/Display, FP or other layers. Sprites always evaluates orientation and
 * the three ordered XW class-origin stores before inspecting layers0..2.
 * Reached unsupported virtual draws remain explicit dependencies.
 *
 * Out starts NULL. Caller keeps the complete camera/World/Engine/service graph
 * live and exclusive through checked retirement. Outputs/errors are fresh and
 * disjoint from that graph and each other. Borrowed views expire as their
 * actual native owners are consumed. Pending calls block source mutation and
 * terminal EffectRenderer cleanup. No List, NW batch or source field is reset
 * by native call retirement. */
mc_effect_draw_status mc_client_effect_renderer_lit_begin(mc_client_effect_renderer *,
                                                          mc_entity *nullable_camera, float partial,
                                                          const mc_effect_draw_services *,
                                                          mc_client_effect_draw_call **,
                                                          mc_effect_draw_error *);
mc_effect_draw_status
mc_client_effect_renderer_sprites_begin(mc_client_effect_renderer *, mc_entity *nullable_camera,
                                        float partial, const mc_effect_draw_services *,
                                        mc_client_effect_draw_call **, mc_effect_draw_error *);
bool mc_client_effect_renderer_draw_inspect(const mc_client_effect_draw_call *,
                                            mc_effect_draw_state *);
mc_effect_draw_status mc_client_effect_renderer_draw_retire(mc_client_effect_draw_call **,
                                                            mc_effect_draw_error *);

#endif
