#ifndef MC_CLIENT_WORLD_HELD_ITEM_DRAW_H
#define MC_CLIENT_WORLD_HELD_ITEM_DRAW_H

#include "mc/client/render/manager/manager.h"
#include "mc/client/world/entity_renderer.h"
#include "mc/core/math/classes.h"
#include "mc/entity/movement/material.h"
#include "mc/entity/tick/environment.h"
#include "mc/world/runtime/brightness.h"

typedef struct mc_client_held_item_draw_call mc_client_held_item_draw_call;

typedef enum mc_client_held_item_draw_status {
    MC_CLIENT_HELD_ITEM_DRAW_OK,
    MC_CLIENT_HELD_ITEM_DRAW_ARGUMENT,
    MC_CLIENT_HELD_ITEM_DRAW_BUSY,
    MC_CLIENT_HELD_ITEM_DRAW_ALLOCATION,
    MC_CLIENT_HELD_ITEM_DRAW_SOURCE_NULL,
    MC_CLIENT_HELD_ITEM_DRAW_CHILD_FAILED,
    MC_CLIENT_HELD_ITEM_DRAW_UNSUPPORTED_FP,
    MC_CLIENT_HELD_ITEM_DRAW_DEPENDENCY
} mc_client_held_item_draw_status;

typedef struct mc_client_held_item_draw_error {
    mc_client_held_item_draw_status status;
    const char *operation;
    mc_client_graphics_site site, child;
    bool native_policy;
    mc_client_entity_renderer_error owner;
    mc_client_display_error display;
    mc_client_graphics_error graphics;
    mc_math_class_error math;
    mc_world_brightness_error brightness;
    mc_entity_tick_error tick;
    mc_entity_material_error material;
    mc_registry_error registry;
    mc_entity_status entity;
    mc_item_stack_status stack;
    mc_client_manager_error manager;
    const struct mc_client_lighting_error *lighting;
    const struct mc_client_texture_lookup_error *texture;
    const struct mc_client_download_texture_error *download;
    const struct mc_client_model_draw_error *model;
} mc_client_held_item_draw_error;

typedef struct mc_client_held_item_draw_state {
    bool active, returned, overlays, player_captured, stack_captured;
    bool lighting_enabled, brightness_returned, color_set, empty_hand;
    bool swing_returned, rescale_enabled, skin_arguments_read;
    bool alpha_disabled, burning_read, burning, suffocating_read, suffocating;
    bool water_read, water, alpha_restored;
    bool skin_returned, renderer_returned, pose_returned, arm_returned;
    unsigned int faces, vertices;
    unsigned int source_pc, gl_calls, matrix_pushes, matrix_pops;
    float partial, equipped, pitch, brightness, swing;
    mc_client_held_item *owner;
    mc_entity *player;
    const mc_item_stack *stack;
    const mc_client_held_item_draw_error *failure;
    const mc_client_held_item_draw_error *cleanup;
} mc_client_held_item_draw_state;

/* Actual RA.a(F)V or RA.b(F)V. Reached source effects remain on failure;
 * missing item/model/image descendants are explicit dependencies. An empty
 * held slot follows the actual arm route and never supplies a no-arm success.
 * Keep the complete App/Player/World/graphics graph live and exclusive through
 * retirement. Outputs start NULL; admitted failed calls remain inspectable. */
mc_client_held_item_draw_status
mc_client_held_item_draw_begin(mc_client_held_item *, mc_math_classes *, float partial,
                               bool overlays, mc_client_held_item_draw_call **owned_out,
                               mc_client_held_item_draw_error *);
bool mc_client_held_item_draw_inspect(const mc_client_held_item_draw_call *,
                                      mc_client_held_item_draw_state *);
mc_client_held_item_draw_status mc_client_held_item_draw_retire(mc_client_held_item_draw_call **,
                                                                mc_client_held_item_draw_error *);

#endif
