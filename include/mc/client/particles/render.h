#ifndef MC_CLIENT_PARTICLES_RENDER_H
#define MC_CLIENT_PARTICLES_RENDER_H

#include "ge.h"
#include "mc/client/particles/effect_renderer.h"
#include "mc/client/render/manager/draw.h"
#include "mc/client/render/manager/manager.h"
#include "mc/core/math/classes.h"
#include "mc/entity/particles/origin.h"
#include "mc/world/runtime/brightness.h"

typedef struct mc_client_effect_draw_call mc_client_effect_draw_call;
typedef struct mc_client_texture_lookup_error mc_client_texture_lookup_error;
typedef struct mc_client_tessellator mc_client_tessellator;
typedef struct mc_entity_render_error mc_entity_render_error;

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
    bool source_exception;
    const char *exception_class;
    mc_particle_throwable throwable;
    const mc_entity_render_error *entity_render;
    mc_world_storage_status receipt_status;
    const mc_client_manager_draw_error *manager_draw;
    /* Reached XN/QU invocation of XW; child remains the failing XW operation. */
    mc_client_graphics_site sprite_caller;
} mc_effect_draw_error;

/* Actual enclosing namespaces, not source DN World/Engine/camera operands.
 * The empty lit branch never reads this tuple. Retain only reached services. */
typedef struct mc_effect_draw_services {
    mc_client_render_engine_classes *graphics;
    mc_math_classes *math;
    mc_client_display *display;
    mc_client_render_manager_classes *manager;
    mc_client_application_classes *application_classes;
} mc_effect_draw_services;

/* Copied per-invocation EM286/TH coordinate draw observations. Identities are
 * scalars; source class strings have static storage. No owner/view survives
 * through this receipt. Dead actors and zero-count Stacks are never filtered. */
typedef struct mc_effect_pickup_draw_receipt {
    bool attempted, admitted, returned, retired, coordinates_only;
    bool primary_returned, post_draw_returned, bb_returned, actor_dead;
    bool th_brightness_returned, th_color_returned;
    uintptr_t actor_identity, renderer_identity;
    int32_t actor_kind;
    const char *renderer_class;
    unsigned int th_pc, primary_pc, post_pc;
    double x, y, z;
    float yaw, partial;
    size_t copies, faces;
} mc_effect_pickup_draw_receipt;

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
    size_t pickup_draw_attempts, pickup_draw_returns, pickup_draw_retirements;
    /* Actual PB/QM quarter-tile or XW full-tile billboard values. The reached
     * concrete draw determines the atlas extent and vertex UV order. */
    bool item_particle_returned;
    float item_u0, item_u1, item_v0, item_v1, item_half_size;
    float item_center[3], item_color[3], item_vertices[4][5];
    unsigned int item_vertices_returned;
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
size_t mc_client_effect_renderer_pickup_draw_count(const mc_client_effect_draw_call *);
bool mc_client_effect_renderer_pickup_draw_at(const mc_client_effect_draw_call *, size_t,
                                              mc_effect_pickup_draw_receipt *);
mc_effect_draw_status mc_client_effect_renderer_draw_retire(mc_client_effect_draw_call **,
                                                            mc_effect_draw_error *);

#endif
