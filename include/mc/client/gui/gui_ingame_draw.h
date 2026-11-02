#ifndef MC_CLIENT_GUI_GUI_INGAME_DRAW_H
#define MC_CLIENT_GUI_GUI_INGAME_DRAW_H
#include "client/render_engine/source.h"
#include "mc/client/gui/gui_ingame.h"
#include "mc/client/gui/scaled_resolution.h"
#include "mc/client/render_engine/render_engine.h"
#include "mc/client/settings/game_settings.h"
#include "mc/client/world/render_global.h"
#include "mc/core/math/classes.h"
#include "mc/entity/movement/material.h"
#include "mc/inventory/player.h"
#include "mc/world/runtime/brightness.h"
#include "mc/world/runtime/movement.h"

typedef struct mc_client_gui_ingame_draw_call mc_client_gui_ingame_draw_call;

typedef enum mc_client_gui_ingame_draw_status {
    MC_CLIENT_GUI_INGAME_DRAW_OK,
    MC_CLIENT_GUI_INGAME_DRAW_ARGUMENT,
    MC_CLIENT_GUI_INGAME_DRAW_BUSY,
    MC_CLIENT_GUI_INGAME_DRAW_ALLOCATION,
    MC_CLIENT_GUI_INGAME_DRAW_SOURCE_NULL,
    MC_CLIENT_GUI_INGAME_DRAW_CHILD_FAILED,
    MC_CLIENT_GUI_INGAME_DRAW_UNSUPPORTED_FP,
    MC_CLIENT_GUI_INGAME_DRAW_DEPENDENCY
} mc_client_gui_ingame_draw_status;

/* Copied source/CV values survive retirement of a failed Item child's native
 * workspace. Source site/operation strings have static storage; no CV, Block,
 * World, Registry, model or graphics owner pointer is retained here. */
typedef struct mc_client_gui_ingame_inventory_receipt {
    bool valid, failed, returned, native_policy;
    mc_client_render_global_status status;
    const char *operation;
    mc_client_graphics_site site, child, frames[16];
    size_t depth, faces;
    unsigned int source_pc, last_store_pc;
    mc_client_graphics_error graphics;
    mc_client_display_error display;
} mc_client_gui_ingame_inventory_receipt;

typedef struct mc_client_gui_ingame_draw_error {
    mc_client_gui_ingame_draw_status status;
    const char *operation;
    mc_client_graphics_site site, child;
    bool native_policy;
    mc_client_gui_ingame_error owner;
    mc_client_display_error display;
    mc_client_graphics_error graphics;
    mc_math_class_error math;
    mc_game_settings_error settings;
    mc_world_brightness_error brightness;
    mc_world_movement_error world;
    mc_entity_material_error material;
    mc_registry_error registry;
    mc_entity_status entity;
    mc_item_stack_status stack;
    mc_inventory_error inventory;
    const struct mc_client_item_gui_error *item;
    mc_client_gui_ingame_inventory_receipt item_inventory;
    const struct mc_client_item_overlay_context *item_overlay;
    const struct mc_client_texture_lookup_error *texture;
    const struct mc_client_lighting_error *lighting;
    const struct mc_client_overlay_setup_error *projection;
    const struct mc_client_application_owner_error *application;
} mc_client_gui_ingame_draw_error;

typedef struct mc_client_gui_ingame_draw_state {
    bool active, returned, projection_returned, fancy, vignette_returned;
    bool pumpkin_entered, pumpkin_returned;
    bool hotbar_returned, crosshair_returned, blinking, random_seeded;
    bool stats_enabled, stats_returned, water, slots_returned, chat_returned;
    unsigned int source_pc, gl_calls, textured_quads, vertices;
    unsigned int heart_backgrounds, heart_foregrounds, armor_icons, empty_slots;
    unsigned int item_slots, item_overlays, animated_slots, item_faces;
    unsigned int pumpkin_pc, pumpkin_vertices;
    size_t pumpkin_word_count;
    int32_t pumpkin_words[48]; /* Actual NW backing copied before UQ.a(II) draw107. */
    mc_scaled_resolution resolution;
    float partial, brightness, vignette;
    int32_t mouse_x, mouse_y, health, previous_health, armor, selected;
    int64_t seed;
    mc_client_source_uq *owner;
    const mc_client_gui_ingame_draw_error *failure, *cleanup;
} mc_client_gui_ingame_draw_state;

/* Actual UQ.a(FZII)V, retained once. Reached source/UI/Random/pool effects
 * survive failure. Keep actual App, namespaces and Display live/exclusive
 * through retirement; foreign Engine lookup attempts remain untouched.
 * Outputs start NULL. Unsupported nondefault descendants fail explicitly. */
mc_client_gui_ingame_draw_status
mc_client_gui_ingame_draw_begin(mc_client_source_uq *, mc_client_render_engine_classes *,
                                mc_math_classes *, mc_client_display *, float partial, bool flag,
                                int32_t mouse_x, int32_t mouse_y, mc_client_gui_ingame_draw_call **,
                                mc_client_gui_ingame_draw_error *);
bool mc_client_gui_ingame_draw_inspect(const mc_client_gui_ingame_draw_call *,
                                       mc_client_gui_ingame_draw_state *);
mc_client_gui_ingame_draw_status
mc_client_gui_ingame_draw_retire(mc_client_gui_ingame_draw_call **,
                                 mc_client_gui_ingame_draw_error *);
#endif
