#ifndef MC_CLIENT_RENDER_ITEM_OVERLAY_H
#define MC_CLIENT_RENDER_ITEM_OVERLAY_H

#include "client/gui/bitmap_font.h"
#include "item/render/internal.h"
#include "mc/client/render/model.h"

typedef struct mc_client_item_overlay_context {
    mc_client_item_renderer *owner;
    mc_registry_classes *registry;
    mc_client_bitmap_font *font;
    mc_client_display *display;
    mc_item_stack *stack;
    int32_t x, y;
    bool returned, null_stack, count_read, count_drawn, durability_returned, damage_read, damaged;
    int32_t count, maximum, damage;
    unsigned int source_pc;
    mc_client_graphics_site site, child;
    const char *operation, *exception_class;
    bool native_policy, source_exception;
    mc_item_render_throwable throwable;
    mc_item_stack_status stack_error;
    mc_registry_error registry_error;
    mc_client_display_error display_error;
    mc_client_bitmap_font_error font_error;
    uint16_t count_units[12];
} mc_client_item_overlay_context;

/* Actual bb.b(SJ,JI,IZ,II), entered under a retained parent. The caller keeps
 * the actual BB/model namespace, Registry, Stack, Font and Display live and
 * exclusive through this call and retains this diagnostic on failure. JI is
 * an unused source argument in this method. No source GL state is unwound. */
bool mc_client_item_overlay_draw(mc_client_item_overlay_context *);

#endif
