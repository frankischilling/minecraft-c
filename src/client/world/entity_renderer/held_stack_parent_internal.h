#ifndef MC_CLIENT_HELD_STACK_PARENT_INTERNAL_H
#define MC_CLIENT_HELD_STACK_PARENT_INTERNAL_H

#include "block/registry/runtime_internal.h"
#include "client/render_engine/texture_lookup.h"
#include "entity/render/internal.h"
#include "held_bow_geometry_internal.h"
#include "mc/client/render/manager/manager.h"

typedef struct mc_client_draw_scope mc_client_draw_scope;
typedef struct mc_cv_context mc_cv_context;

typedef enum mc_held_stack_parent_status {
    MC_HELD_STACK_PARENT_OK,
    MC_HELD_STACK_PARENT_ARGUMENT,
    MC_HELD_STACK_PARENT_STATE,
    MC_HELD_STACK_PARENT_GUARD_FAILED,
    MC_HELD_STACK_PARENT_CAPTURE_FAILED,
    MC_HELD_STACK_PARENT_STACK_FAILED,
    MC_HELD_STACK_PARENT_REGISTRY_FAILED,
    MC_HELD_STACK_PARENT_ENTITY_FAILED,
    MC_HELD_STACK_PARENT_LOOKUP_FAILED,
    MC_HELD_STACK_PARENT_GRAPHICS_FAILED,
    MC_HELD_STACK_PARENT_DISPLAY_FAILED,
    MC_HELD_STACK_PARENT_GEOMETRY_FAILED,
    MC_HELD_STACK_PARENT_SOURCE_EXCEPTION,
    MC_HELD_STACK_PARENT_UNIMPLEMENTED
} mc_held_stack_parent_status;

typedef enum mc_held_stack_parent_throwable {
    MC_HELD_STACK_PARENT_NO_THROWABLE,
    MC_HELD_STACK_PARENT_EXCEPTION,
    MC_HELD_STACK_PARENT_ERROR,
    MC_HELD_STACK_PARENT_UNCLASSIFIED_THROWABLE
} mc_held_stack_parent_throwable;

typedef enum mc_held_stack_parent_child {
    MC_HELD_STACK_PARENT_CHILD_NONE,
    MC_HELD_STACK_PARENT_CHILD_GUARD,
    MC_HELD_STACK_PARENT_CHILD_STACK,
    MC_HELD_STACK_PARENT_CHILD_REGISTRY,
    MC_HELD_STACK_PARENT_CHILD_ENTITY,
    MC_HELD_STACK_PARENT_CHILD_LOOKUP,
    MC_HELD_STACK_PARENT_CHILD_GRAPHICS,
    MC_HELD_STACK_PARENT_CHILD_DISPLAY,
    MC_HELD_STACK_PARENT_CHILD_GEOMETRY,
    MC_HELD_STACK_PARENT_CHILD_INVENTORY
} mc_held_stack_parent_child;

typedef struct mc_held_stack_parent_state {
    mc_client_held_item *hand;
    const mc_client_source_ra *ra;
    mc_entity *actor;
    mc_item_stack *stack;
    client_application *application;
    mc_client_render_engine *engine;
    const mc_player_string *key;
    mc_registry_classes *registry;
    mc_registry_block_m_view blocks;
    const mc_block *block;
    mc_client_tessellator *nw;
    mc_client_texture texture;
    int32_t id4, id17, id76, icon;
    int32_t block_metadata;
    float block_brightness;
    bool block_inventory_returned;
    unsigned int source_pc;
    bool guard_acquired, guard_discharged, source_entered, push_returned;
    bool id4_read, block_array_read, id17_read, block_read, id76_read;
    bool application_read, engine_read, key_loaded, terrain_atlas;
    bool lookup_attempted, lookup_entered, lookup_returned;
    bool pending_on_entry, pending_after_call, pending_adopted;
    bool bind_attempted, bind_returned, nw_attempted, nw_returned;
    bool icon_attempted, icon_entered, icon_returned;
    bool entity_queries_published, entity_worlds_adopted;
    bool geometry_attempted, geometry_returned, source_returned;
} mc_held_stack_parent_state;

typedef struct mc_held_stack_parent_failure_summary {
    bool present;
    mc_held_stack_parent_child selected;
    mc_held_stack_parent_throwable throwable;
    const char *operation, *exception_class;
    mc_client_graphics_site site, child;
    bool native_policy, source_exception;
} mc_held_stack_parent_failure_summary;

/* Fresh/disjoint charged parent storage, live until checked guard discharge.
 * Full children are separate so a later native capture failure cannot erase
 * the preceding Source failure or its remaining native owner slots. */
typedef struct mc_held_stack_parent_error {
    mc_held_stack_parent_status status;
    mc_held_stack_parent_child selected;
    mc_held_stack_parent_throwable throwable;
    const char *operation, *exception_class;
    mc_client_graphics_site caller, site, child;
    bool native_policy, source_exception, capture_failed;
    mc_held_stack_parent_failure_summary before_capture_failure;
    mc_held_stack_parent_state state;
    mc_client_entity_renderer_error guard;
    mc_client_manager_error capture_failure;
    mc_item_stack_status stack;
    mc_registry_error registry;
    mc_entity_render_error entity; /* Includes complete Item cause. */
    mc_client_texture_lookup_error lookup;
    mc_client_graphics_error graphics;
    mc_client_display_error display;
    mc_held_bow_error geometry;
    mc_cv_context *inventory;
} mc_held_stack_parent_error;

/* Entered body. Parent has already retained exact TH.f hand via
 * capture_hand(scope, actualTH, capturedSourceRA, ...), and holds the genuine
 * nullable LS argument. Stack capture is deduplicated here before Source work.
 * Parent proves Display/native namespace associations and every borrowed owner.
 * Never initialize/reuse this error over a live guard or unresolved child.
 * No source repair/cleanup, guard discharge or catch is performed here. */
bool entity_renderer_held_stack_entered(mc_client_draw_scope *, mc_client_held_item *,
                                        mc_entity *nullable_actor, mc_item_stack *nullable_stack,
                                        mc_client_graphics_site caller,
                                        mc_held_stack_parent_error *);

#endif
