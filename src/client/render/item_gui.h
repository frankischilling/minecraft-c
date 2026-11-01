#ifndef MC_CLIENT_RENDER_ITEM_GUI_H
#define MC_CLIENT_RENDER_ITEM_GUI_H

#include "client/render_engine/nw.h"
#include "client/render_engine/texture_lookup.h"
#include "item/render/internal.h"
#include "mc/client/render/model.h"

typedef struct mc_client_bitmap_font mc_client_bitmap_font;
typedef struct mc_client_item_gui_call mc_client_item_gui_call;

typedef enum mc_client_item_gui_status {
    MC_CLIENT_ITEM_GUI_OK,
    MC_CLIENT_ITEM_GUI_INVALID_ARGUMENT,
    MC_CLIENT_ITEM_GUI_BUSY,
    MC_CLIENT_ITEM_GUI_LIMIT,
    MC_CLIENT_ITEM_GUI_ALLOCATION_FAILED,
    MC_CLIENT_ITEM_GUI_NULL_REFERENCE,
    MC_CLIENT_ITEM_GUI_ARRAY_INDEX,
    MC_CLIENT_ITEM_GUI_UNIMPLEMENTED,
    MC_CLIENT_ITEM_GUI_UNSUPPORTED_FP,
    MC_CLIENT_ITEM_GUI_DISPLAY_FAILED,
    MC_CLIENT_ITEM_GUI_REGISTRY_FAILED,
    MC_CLIENT_ITEM_GUI_ITEM_FAILED,
    MC_CLIENT_ITEM_GUI_TEXTURE_FAILED,
    MC_CLIENT_ITEM_GUI_GRAPHICS_FAILED,
    MC_CLIENT_ITEM_GUI_CLEANUP_FAILED
} mc_client_item_gui_status;

typedef struct mc_client_item_gui_error {
    mc_client_item_gui_status status;
    const char *operation, *exception_class;
    mc_client_graphics_site caller, site, child;
    bool native_policy, source_exception;
    mc_item_render_throwable throwable;
    mc_client_display_error display, cleanup_display;
    mc_client_graphics_error graphics;
    mc_registry_error registry;
    mc_item_stack_status stack;
    mc_item_render_error item;
    mc_client_texture_lookup_error texture;
    mc_client_render_engine_status owner;
} mc_client_item_gui_error;

typedef struct mc_client_item_gui_state {
    const mc_client_item_renderer *owner;
    const mc_item_stack *stack;
    const mc_registry_classes *registry;
    const mc_client_render_engine_classes *graphics;
    const mc_client_render_engine *engine;
    const mc_client_tessellator *nw;
    const mc_registry_item_array *item_array;
    const mc_item *color_item;
    const mc_block *block;
    mc_client_display *display;
    bool active, retiring, returned, null_stack;
    bool id_read, metadata_read, icon_returned, raw_entered;
    bool block_type_returned, block_3d, negative_icon;
    bool lookup_returned, lookup_cache_hit, pending_lookup_owned, texture_bound;
    bool color_returned, color_set, block_color_stored;
    bool quad_started, quad_drawn, quad_returned;
    bool lighting_disabled, lighting_enabled, culling_enabled;
    int32_t entry_pc, raw_pc, quad_pc;
    int32_t id, metadata, icon, x, y, texture_x, texture_y, color, block_type;
    mc_client_texture texture_id;
    mc_texture_pack_text texture_key;
    float color_rgba[4];
    size_t vertices_returned;
    double vertices[4][5]; /* Exact production x/y/z/u/v arguments, not NW copies. */
    const mc_client_item_gui_error *failure, *cleanup;
} mc_client_item_gui_state;

/* Actual BB.a(SJ,JI,IZ,int,int), including its distinct raw-ID and local-Z
 * quad overloads. Font is unused by these source methods and may be NULL.
 * Native receiver/output admission is separate from source operand reads:
 * NULL Stack returns at4 before Registry, Engine, graphics or Display use.
 * Nonnull Stack getters are phase transparent. Services and objects must be
 * genuine, exclusive owners in the same client namespace; reached Registry
 * borrows use the RenderClasses' exact profile. No overlay BB.b call occurs.
 *
 * After native call allocation, out receives one retained record on success
 * AND source/native failure. Inspect remains valid until checked retirement.
 * The BB owner rejects free, construction, manager mutation and another draw
 * while that record remains. Only reached operands acquire native borrows;
 * the unused Font argument acquires none. Error/output storage must be live,
 * aligned, disjoint from each other and from every owner. */
mc_client_item_gui_status mc_client_item_gui_begin(
    mc_client_item_renderer *, mc_client_render_engine_classes *, mc_registry_classes *,
    mc_client_display *, mc_client_bitmap_font *nullable_unused_font,
    mc_client_render_engine *nullable_engine, mc_item_stack *nullable_stack, int32_t x, int32_t y,
    mc_client_item_gui_call **out, mc_client_item_gui_error *);
bool mc_client_item_gui_inspect(const mc_client_item_gui_call *, mc_client_item_gui_state *);
/* Native ownership only. No matrix/state restoration, draw, cache/list/ID
 * deletion or source CV.b rewrite. Refusal preserves remaining rights for a
 * retry. A foreign preexisting texture lookup is never adopted or retired. */
mc_client_item_gui_status mc_client_item_gui_retire(mc_client_item_gui_call **,
                                                    mc_client_item_gui_error *);
#endif
