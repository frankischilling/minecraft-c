#include "source_workbench_drawing.h"
#include "source_inventory_drawing_internal.h"
#include "source_workbench_screen_internal.h"

bool mc_workbench_draw_background(mc_client_inventory_draw_call *c) {
    mc_client_source_id *o = c->owner;
    client_application *app = o->base.fields.b;
    if (app == NULL)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                      "OO actual App texture field null receiver", "oo", "a",
                                      "(F)V", 4, false);
    mc_client_texture texture;
    if (!mc_inventory_draw_texture(c, app->render_engine, "/gui/crafting.png", "oo", 9, &texture))
        return false;
    mc_client_display *display = c->services.display;
    if (!mc_inventory_draw_gl(
            c, mc_client_display_source_color(display, 1, 1, 1, 1, &c->error.display), "oo", 17))
        return false;
    /* The bind receiver is independently evaluated after lookup and white. */
    app = o->base.fields.b;
    if (app == NULL)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                      "OO fresh App bind field null receiver", "oo", "a", "(F)V",
                                      24, false);
    mc_client_render_engine *bind_receiver = app->render_engine;
    if (bind_receiver == NULL)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                      "OO actual captured JI bind null receiver", "oo", "a", "(F)V",
                                      28, false);
    if (mc_java_i32_narrow((int64_t)texture) >= 0 &&
        !mc_inventory_draw_gl(
            c, mc_client_display_source_bind_id(display, texture, &c->error.display), "oo", 28))
        return false;
    const int32_t x = mc_java_i32_sub(o->base.fields.c, o->id.a) / 2;
    const int32_t y = mc_java_i32_sub(o->base.fields.d, o->id.i) / 2;
    if (!mc_inventory_draw_tile(c, x, y, 0, 0, o->id.a, o->id.i))
        return false;
    c->state.background_returned = true;
    return true;
}

static bool literal(mc_client_inventory_draw_call *c, const char *text, mc_player_string **out) {
    mc_client_screen_classes *classes = c->owner->base.classes;
    if (!mc_screen_lock(classes, &c->error.screen))
        return false;
    const bool found = mc_screen_literal_locked(classes, text, out, &c->error.screen);
    mtx_unlock(&classes->mutex);
    return found;
}

static bool label(mc_client_inventory_draw_call *c, mc_client_bitmap_font *font,
                  mc_player_string *text, int32_t x, int32_t y, unsigned int pc) {
    if (font == NULL)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                      "OO actual label Font null receiver", "oo", "k", "()V", pc,
                                      false);
    if (mc_client_bitmap_font_draw(font, text, x, y, 4210752, false, &c->error.font) !=
        MC_CLIENT_BITMAP_FONT_OK)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_FONT_FAILED, c->error.font.operation,
                                      "oo", "k", "()V", pc, c->error.font.native_policy);
    return true;
}

bool mc_workbench_draw_foreground(mc_client_inventory_draw_call *c) {
    mc_client_source_id *o = c->owner;
    mc_client_bitmap_font *font = o->base.fields.g;
    mc_player_string *text = NULL;
    if (!literal(c, "Crafting", &text) || !label(c, font, text, 28, 6, 12))
        return false;
    /* Source OO.k captures a fresh Font before its separate Inventory label
     * and only then reads the inherited height for the second y operand. */
    font = o->base.fields.g;
    if (!literal(c, "Inventory", &text))
        return false;
    const int32_t y = mc_java_i32_add(mc_java_i32_sub(o->id.i, 96), 2);
    if (!label(c, font, text, 8, y, 34))
        return false;
    c->state.foreground_returned = true;
    return true;
}

mc_client_screen_status mc_client_source_workbench_draw_begin(
    mc_client_source_oo *o, mc_gui_mouse_position mouse, float partial,
    const mc_client_manager_draw_services *services, mc_client_inventory_draw_call **out,
    mc_client_inventory_draw_error *error) {
    return mc_inventory_draw_begin_id(o == NULL ? NULL : &o->container, MC_CLIENT_SCREEN_WORKBENCH,
                                      mouse, partial, services, out, error);
}
