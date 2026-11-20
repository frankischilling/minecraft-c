#ifndef MC_CLIENT_WORLD_SELECTION_OVERLAY_H
#define MC_CLIENT_WORLD_SELECTION_OVERLAY_H

#include "mc/block/registry/registry.h"
#include "mc/client/world/render_global.h"
#include "mc/core/math/classes.h"
#include "mc/entity/bounds/bounds.h"
#include "mc/entity/hit_result.h"
#include "mc/item/stack/stack.h"
#include "mc/world/storage/status.h"
#include "millis.h"

/* Compact composed-child diagnostic. The enclosing retained World frame owns
 * all pointed-to values; no recursive World/Entity error is copied into the
 * public frame. Failure keeps every reached source/GL/NW/pool effect. */
typedef struct mc_client_selection_error mc_client_selection_error;
typedef struct mc_world_selected_box_error mc_world_selected_box_error;

typedef struct mc_client_selection_outline_batch {
    /* Actual returned NW direct-buffer words, not rebuilt geometry. Capture
     * availability is data only and never changes a source return. Draw resets
     * the ByteBuffer range; the shared IntBuffer position retains the uploaded
     * word count. Keep the reset range separate from the submitted byte count. */
    bool captured, draw_state_captured, fog_captured;
    const void *nw;
    int32_t mode;
    size_t vertices, words, submitted_bytes, byte_position, byte_limit, byte_capacity;
    uint32_t raw_words[64]; /* At most8 vertices, each8 source words. */
    double translation[3];
    bool texture_coordinates, vertex_colors, normals, color_disabled;
    float color[4], line_width;
    bool color_known, texture_enabled, alpha_test, lighting_enabled, fog_enabled;
    bool depth_mask_known, requested_depth_mask;
    bool blend_known, requested_blend_enabled, blend_function_known;
    int32_t requested_blend_source, requested_blend_destination;
} mc_client_selection_outline_batch;

typedef struct mc_client_selection_state {
    bool underwater, overlay_returned, outline_returned, alpha_restored;
    bool clock_sampled, texture_bound, matrix_pushed, polygon_offset_enabled;
    bool crack_cv_returned, crack_draw_returned, crack_state_restored;
    bool outline_block, shape_returned, selected_returned, expanded_returned, shifted_returned;
    unsigned int overlay_pc, outline_pc, vertices, draws;
    int64_t wall_clock;
    float pulse_alpha, progress;
    double camera[3];
    int32_t x, y, z, block_id;
    int32_t crack_texture;
    const mc_block *crack_block;
    const void *crack_renderer;
    const void *crack_context;
    const mc_entity_hit_result *overlay_hit, *outline_hit;
    const mc_item_stack *overlay_stack, *outline_stack;
    const mc_entity_bounds *selected, *expanded, *shifted;
    const mc_world_selected_box_error *selected_getter;
    bool bounds_cursor_captured[4];
    int32_t bounds_cursor[4]; /* Before/after selected, after expand, after shift. */
    mc_client_selection_outline_batch outline_batches[3];
    const mc_client_selection_error *failure;
} mc_client_selection_state;

/* These are passive receipts of n.a/n.b inside the same retained px.a(FJ).
 * Stage is the caller's actual literal zero. No standalone API supplies a Hit,
 * clock, progress field or alternate World. Borrowed views expire when that
 * World frame retires; completed scalar fields remain inspectable until then.
 * Native cleanup releases occurrences only, without source GL restoration,
 * pool reset, NW completion, texture retry or replay of an interrupted child.
 * Positive progress invokes the actual CV with its own World implementation;
 * missing specialized CV geometry retains its reached dependency. All closed
 * client selected-box getters use their actual virtual/state/query bodies. */
#endif
