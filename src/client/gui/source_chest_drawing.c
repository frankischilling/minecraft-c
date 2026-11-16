#include "source_chest_drawing.h"
#include "source_chest_screen_internal.h"
#include "source_inventory_drawing_internal.h"

bool mc_chest_draw_background(mc_client_inventory_draw_call *c) {
    mc_client_source_hp *screen = (mc_client_source_hp *)c->owner;
    mc_client_source_id *o = c->owner;
    client_application *app = o->base.fields.b;
    if (app == NULL)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                      "HP actual App texture field null receiver", "hp", "a",
                                      "(F)V", 4, false);
    mc_client_texture texture;
    if (!mc_inventory_draw_texture(c, app->render_engine, "/gui/container.png", "hp", 9, &texture))
        return false;
    mc_client_display *display = c->services.display;
    if (!mc_inventory_draw_gl(
            c, mc_client_display_source_color(display, 1, 1, 1, 1, &c->error.display), "hp", 17))
        return false;
    app = o->base.fields.b;
    if (app == NULL)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                      "HP fresh App bind field null receiver", "hp", "a", "(F)V",
                                      24, false);
    if (app->render_engine == NULL)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                      "HP actual captured JI bind null receiver", "hp", "a", "(F)V",
                                      28, false);
    if (mc_java_i32_narrow((int64_t)texture) >= 0 &&
        !mc_inventory_draw_gl(
            c, mc_client_display_source_bind_id(display, texture, &c->error.display), "hp", 28))
        return false;
    const int32_t x = mc_java_i32_sub(o->base.fields.c, o->id.a) / 2;
    const int32_t y = mc_java_i32_sub(o->base.fields.d, o->id.i) / 2;
    const int32_t top = mc_java_i32_add(mc_java_i32_mul(screen->fields.n, 18), 17);
    if (!mc_inventory_draw_tile(c, x, y, 0, 0, o->id.a, top))
        return false;
    /* The second virtual quad call freshly reads n and a after the first
     * returned draw, just as HP.a does at83 and98. */
    const int32_t lower_y =
        mc_java_i32_add(mc_java_i32_add(y, mc_java_i32_mul(screen->fields.n, 18)), 17);
    if (!mc_inventory_draw_tile(c, x, lower_y, 0, 126, o->id.a, 96))
        return false;
    c->state.background_returned = true;
    return true;
}

static bool inventory_failure(mc_client_inventory_draw_call *c, mc_inventory_status status,
                              unsigned int pc) {
    if (status == MC_INVENTORY_OK)
        return true;
    c->owner->detail.ownership = c->error.inventory;
    c->error.screen.inventory = &c->owner->detail;
    const bool source = c->error.inventory.source_exception ||
                        status == MC_INVENTORY_NULL_REFERENCE ||
                        status == MC_INVENTORY_INDEX_OUT_OF_BOUNDS;
    const mc_client_screen_status mapped =
        status == MC_INVENTORY_NULL_REFERENCE        ? MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE
        : status == MC_INVENTORY_INDEX_OUT_OF_BOUNDS ? MC_CLIENT_SCREEN_SOURCE_INDEX
                                                     : MC_CLIENT_SCREEN_APP_FAILED;
    (void)mc_inventory_draw_fail(c, mapped, c->error.inventory.operation, "hp", "k", "()V", pc,
                                 !source);
    c->error.screen.source_exception = source;
    c->error.screen.exception_class =
        c->error.inventory.exception_class != NULL   ? c->error.inventory.exception_class
        : status == MC_INVENTORY_NULL_REFERENCE      ? "java/lang/NullPointerException"
        : status == MC_INVENTORY_INDEX_OUT_OF_BOUNDS ? "java/lang/ArrayIndexOutOfBoundsException"
                                                     : NULL;
    return false;
}

static bool literal(mc_client_inventory_draw_call *c, const char *name, mc_player_string **out) {
    if (name == NULL) {
        *out = NULL;
        return true;
    }
    mc_client_screen_classes *classes = c->owner->base.classes;
    if (!mc_screen_lock(classes, &c->error.screen))
        return false;
    const bool found = mc_screen_literal_locked(classes, name, out, &c->error.screen);
    mtx_unlock(&classes->mutex);
    return found;
}

static bool label(mc_client_inventory_draw_call *c, mc_client_bitmap_font *font,
                  mc_player_string *text, int32_t y, unsigned int pc) {
    if (font == NULL)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                      "HP actual label Font null receiver", "hp", "k", "()V", pc,
                                      false);
    if (mc_client_bitmap_font_draw(font, text, 8, y, 4210752, false, &c->error.font) !=
        MC_CLIENT_BITMAP_FONT_OK)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_FONT_FAILED, c->error.font.operation,
                                      "hp", "k", "()V", pc, c->error.font.native_policy);
    return true;
}

bool mc_chest_draw_foreground(mc_client_inventory_draw_call *c) {
    mc_client_source_hp *screen = (mc_client_source_hp *)c->owner;
    mc_client_bitmap_font *font = screen->container.base.fields.g;
    const char *name;
    if (!inventory_failure(c, mc_inventory_chest_name(screen->fields.m, &name, &c->error.inventory),
                           8))
        return false;
    mc_player_string *text = NULL;
    if (!literal(c, name, &text) || !label(c, font, text, 6, 19))
        return false;
    font = screen->container.base.fields.g;
    if (!inventory_failure(
            c, mc_inventory_player_name(screen->fields.l, &name, &c->error.inventory), 30) ||
        !literal(c, name, &text))
        return false;
    const int32_t y = mc_java_i32_add(mc_java_i32_sub(screen->container.id.i, 96), 2);
    if (!label(c, font, text, y, 48))
        return false;
    c->state.foreground_returned = true;
    return true;
}

mc_client_screen_status
mc_client_source_chest_draw_begin(mc_client_source_hp *screen, mc_gui_mouse_position mouse,
                                  float partial, const mc_client_manager_draw_services *services,
                                  mc_client_inventory_draw_call **out,
                                  mc_client_inventory_draw_error *error) {
    return mc_inventory_draw_begin_id(screen == NULL ? NULL : &screen->container,
                                      MC_CLIENT_SCREEN_CHEST, mouse, partial, services, out, error);
}
