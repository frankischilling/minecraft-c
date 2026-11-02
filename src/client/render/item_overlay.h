#ifndef MC_CLIENT_RENDER_ITEM_OVERLAY_H
#define MC_CLIENT_RENDER_ITEM_OVERLAY_H

#include "client/gui/bitmap_font.h"
#include "client/render_engine/nw.h"
#include "item/render/internal.h"
#include "mc/client/render/model.h"

typedef struct mc_client_item_overlay_context {
    mc_client_item_renderer *owner;
    mc_registry_classes *registry;
    mc_client_render_engine_classes *graphics;
    mc_client_bitmap_font *font;
    mc_client_display *display;
    mc_item_stack *stack;
    int32_t x, y;
    bool returned, null_stack, count_read, count_drawn, durability_returned, damage_read, damaged;
    int32_t count, maximum, damage;
    int32_t bar_damage[2], bar_maximum[2], bar_width, bar_intensity, bar_color, bar_shade;
    unsigned int bar_samples, bar_rectangles, bar_vertices;
    bool bar_returned;
    mc_client_tessellator *nw;
    size_t bar_word_counts[3];
    int32_t bar_words[3][48];
    unsigned int source_pc;
    mc_client_graphics_site site, child;
    const char *operation, *exception_class;
    bool native_policy, source_exception;
    mc_item_render_throwable throwable;
    mc_item_stack_status stack_error;
    mc_registry_error registry_error;
    mc_client_display_error display_error;
    mc_client_bitmap_font_error font_error;
    mc_client_graphics_error graphics_error;
    uint16_t count_units[12];
} mc_client_item_overlay_context;

typedef struct mc_client_item_overlay_rectangle_context {
    mc_client_tessellator *nw;
    int32_t x, y, width, height, color;
    unsigned int source_pc, vertices;
    bool source_exception;
    const char *exception_class;
    mc_client_graphics_error error;
} mc_client_item_overlay_rectangle_context;

/* Entered private BB.a prefix through its four actual NW vertices. The
 * retained parent supplies the real BB and NW namespace. No GL, draw or
 * cleanup occurs here; success leaves the actual NW batch open at draw67. */
bool mc_client_item_overlay_rectangle_entered(mc_client_item_overlay_rectangle_context *);
/* Entered BB.b170..202 integer locals, without a class/GL call or field store. */
void mc_client_item_overlay_bar_colors(int32_t intensity, int32_t *color, int32_t *shade);

/* Actual bb.b(SJ,JI,IZ,II), entered under a retained parent. The caller keeps
 * the actual BB/model and graphics namespaces, Registry, Stack, Font and Display live and
 * exclusive through this call and retains this diagnostic on failure. JI is
 * an unused source argument in this method. No source GL state is unwound. */
bool mc_client_item_overlay_draw(mc_client_item_overlay_context *);

#endif
