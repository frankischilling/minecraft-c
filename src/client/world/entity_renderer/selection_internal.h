#ifndef MC_CLIENT_WORLD_FRAME_SELECTION_INTERNAL_H
#define MC_CLIENT_WORLD_FRAME_SELECTION_INTERNAL_H

#include "../../../world/runtime/collision/blocks_internal.h"
#include "../../render/blocks/internal.h"
#include "../../render_engine/internal.h"
#include "../../render_engine/nw.h"
#include "../../render_engine/texture_lookup.h"
#include "../../render_engine/textures.h"
#include "mc/client/world/render_global.h"
#include "mc/client/world/selection_overlay.h"
#include "mc/world/runtime/collision/boxes.h"

struct mc_client_selection_error {
    const char *operation;
    mc_client_graphics_site site, child;
    bool native_policy;
    mc_platform_millis_status clock;
    mc_math_class_error math;
    mc_client_graphics_error graphics;
    mc_client_display_error display;
    mc_world_storage_error world;
    mc_registry_error registry;
    mc_entity_bounds_error bounds;
    mc_entity_hit_error hit;
    mc_entity_status native_entity;
    const mc_world_collision_error *shape;
    const mc_client_texture_lookup_error *texture;
    const mc_cv_context *cv;
    const mc_client_render_global_error *cv_owner;
    const mc_world_selected_box_error *selected;
};

typedef struct mc_world_frame_selection_work {
    mc_client_selection_state state;
    mc_client_selection_error failure, cleanup;
    mc_entity_hit_result *hits[2];
    mc_item_stack *stacks[2];
    mc_entity_bounds *boxes[3];
    mc_entity_classes *pool_classes;
    mc_world_collision_error *shape;
    mc_world_selected_box_error *selected;
    mc_client_tessellator *nw;
    mc_client_render_engine *engine;
    mc_client_texture_lookup_error texture, texture_cleanup;
    mc_cv_context *cv;
    mc_world_runtime *cv_world;
    mc_client_render_global_error cv_owner;
    bool lookup_owned;
    unsigned int parent_pc;
    bool outline;
} mc_world_frame_selection_work;

#endif
