#include "source_sign_drawing_internal.h"

#include <stdlib.h>

static const char ke_desc[] = "(Lnet/minecraft/client/Minecraft;II)V";
static const char quad_desc[] = "(IIIIII)V";

static bool button_state(mc_client_sign_draw_call *c, mc_client_source_ke *button,
                         mc_client_screen_ke_state *out) {
    return mc_client_screen_button_inspect(button, out, &c->error.screen) == MC_CLIENT_SCREEN_OK;
}

/* UB.b uses the actual button's inherited k afresh before each f2d, not a
 * synthesized zero z-level or a Canvas quad. Public inspection captures that
 * genuine field at each reached read while the call owns the Button. */
static bool quad(mc_client_sign_draw_call *c, mc_client_source_ke *button, int32_t x, int32_t y,
                 int32_t u, int32_t v, int32_t width, int32_t height) {
    mc_client_tessellator *nw = NULL;
    if (!mc_sign_draw_nw(c,
                         mc_client_nw_active_use(c->services.draw.graphics,
                                                 (mc_client_graphics_site){"ub", "b", quad_desc, 8},
                                                 &nw, &c->error.graphics),
                         "b", quad_desc, 8) ||
        !mc_sign_draw_nw(c, mc_client_nw_begin_quads(nw, &c->error.graphics), "b", quad_desc, 15))
        return false;
    static const unsigned int field_pc[] = {30, 68, 106, 142};
    static const unsigned int vertex_pc[] = {52, 91, 128, 163};
    for (size_t i = 0; i < 4; ++i) {
        mc_client_screen_ke_state source;
        if (!button_state(c, button, &source))
            return false;
        const float k = source.fields.k;
        if (!mc_sign_draw_fp(c, "ub", "b", quad_desc, field_pc[i] + 3u))
            return false;
        const bool right = i == 1 || i == 2, bottom = i < 2;
        const double z = at_widen(k);
        const double tu =
            at_widen(at_fmul((float)mc_java_i32_add(u, right ? width : 0), 0.00390625f));
        const double tv =
            at_widen(at_fmul((float)mc_java_i32_add(v, bottom ? height : 0), 0.00390625f));
        if (!mc_sign_draw_nw(c,
                             mc_client_nw_vertex_uv(nw, mc_java_i32_add(x, right ? width : 0),
                                                    mc_java_i32_add(y, bottom ? height : 0), z, tu,
                                                    tv, &c->error.graphics),
                             "b", quad_desc, vertex_pc[i]))
            return false;
    }
    return mc_sign_draw_nw(c, mc_client_nw_draw(nw, c->services.draw.display, &c->error.graphics),
                           "b", quad_desc, 168);
}

static bool texture(mc_client_sign_draw_call *c, mc_client_render_engine *engine,
                    mc_client_texture *out) {
    if (engine == NULL)
        return mc_sign_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                 "KE actual JI texture null receiver", "ke", "a", ke_desc, 23,
                                 false);
    static const uint16_t path[] = {'/', 'g', 'u', 'i', '/', 'g', 'u', 'i', '.', 'p', 'n', 'g'};
    const bool pending = mc_client_texture_lookup_pending(engine);
    const mc_client_texture_lookup_status status = mc_client_render_engine_texture(
        engine, c->services.draw.display, NULL,
        (mc_texture_pack_text){path, sizeof(path) / sizeof(*path), false}, out, &c->texture);
    if (status == MC_CLIENT_LOOKUP_OK)
        return true;
    if (!pending && mc_client_texture_lookup_pending(engine))
        c->pending_lookup = engine;
    c->error.texture = &c->texture;
    mc_sign_draw_fail(
        c, MC_CLIENT_SCREEN_TEXTURE_FAILED, c->texture.operation, c->texture.site.class_name,
        c->texture.site.method, c->texture.site.descriptor,
        c->texture.site.pc < 0 ? 0u : (unsigned int)c->texture.site.pc, c->texture.native_policy);
    c->error.helper = (mc_client_screen_site){"ke", "a", ke_desc, 23};
    c->error.screen.source_exception = !c->texture.native_policy;
    if (!c->texture.native_policy)
        c->error.screen.exception_class = status == MC_CLIENT_LOOKUP_NULL_REFERENCE
                                              ? "java/lang/NullPointerException"
                                          : status == MC_CLIENT_LOOKUP_SOURCE_ILLEGAL_ARGUMENT
                                              ? "java/lang/IllegalArgumentException"
                                          : status == MC_CLIENT_LOOKUP_SOURCE_NEGATIVE_SIZE
                                              ? "java/lang/NegativeArraySizeException"
                                              : NULL;
    return false;
}

static bool draw_button(mc_client_sign_draw_call *c, mc_client_source_ke *button,
                        client_application *app) {
    mc_client_screen_ke_state source;
    if (!button_state(c, button, &source))
        return false;
    if (!source.fields.h)
        return true; /* Actual KE return7 precedes every App/Font/Texture read. */
    if (app == NULL)
        return mc_sign_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                 "KE actual App Font field null receiver", "ke", "a", ke_desc, 9,
                                 false);
    mc_client_bitmap_font *font = app->font_renderer; /* Actual q9, retained when reached below. */
    mc_client_render_engine *engine = app->render_engine; /* Actual p18 on the same App argument. */
    mc_client_texture name;
    if (!texture(c, engine, &name))
        return false;
    mc_client_display *d = c->services.draw.display;
    if (!mc_sign_draw_gl(c, mc_client_display_source_bind_id(d, name, &c->error.display), "ke", "a",
                         ke_desc, 26) ||
        !mc_sign_draw_gl(c, mc_client_display_source_color(d, 1, 1, 1, 1, &c->error.display), "ke",
                         "a", ke_desc, 33))
        return false;
    if (!button_state(c, button, &source))
        return false;
    const mc_client_screen_ke_fields *f = &source.fields;
    const bool hover = c->mouse.x >= f->c && c->mouse.y >= f->d &&
                       c->mouse.x < mc_java_i32_add(f->c, f->a) &&
                       c->mouse.y < mc_java_i32_add(f->d, f->b);
    /* Actual concrete KE.a(Z) is a pure 0/1/2 selector, with a fresh g read. */
    if (!button_state(c, button, &source))
        return false;
    const int32_t row = !source.fields.g ? 0 : hover ? 2 : 1;
    if (!button_state(c, button, &source))
        return false;
    f = &source.fields;
    if (!quad(c, button, f->c, f->d, 0, mc_java_i32_add(46, mc_java_i32_mul(row, 20)), f->a / 2,
              f->b))
        return false;
    if (!button_state(c, button, &source))
        return false;
    f = &source.fields;
    if (!quad(c, button, mc_java_i32_add(f->c, f->a / 2), f->d, mc_java_i32_sub(200, f->a / 2),
              mc_java_i32_add(46, mc_java_i32_mul(row, 20)), f->a / 2, f->b))
        return false;
    /* KE.b(App,II)175 is its concrete literal-return hook; no subclass is fabricated. */
    if (!button_state(c, button, &source))
        return false;
    f = &source.fields;
    const uint32_t color = !f->g   ? UINT32_C(0xffa0a0a0)
                           : hover ? UINT32_C(0xffffa0)
                                   : UINT32_C(0xe0e0e0);
    const unsigned int center_pc = !f->g ? 219u : hover ? 264u : 304u;
    mc_player_string_view view;
    const mc_player_string_view *label = NULL;
    if (f->e.kind != MC_CLIENT_SCREEN_TEXT_NULL) {
        if (!mc_client_screen_text_read(f->e, &view))
            return mc_sign_draw_fail(c, MC_CLIENT_SCREEN_STRING_FAILED,
                                     "KE actual label String native view", "ke", "a", ke_desc,
                                     center_pc, true);
        label = &view;
    }
    if (!mc_sign_draw_centered(c, font, label, mc_java_i32_add(f->c, f->a / 2),
                               mc_java_i32_add(f->d, mc_java_i32_sub(f->b, 8) / 2), color,
                               (mc_client_screen_site){"ke", "a", ke_desc, center_pc}, NULL, NULL))
        return false;
    ++c->state.buttons_drawn;
    return true;
}

bool mc_sign_draw_buttons(mc_client_sign_draw_call *c) {
    for (size_t index = 0;; ++index) {
        mc_client_screen_state base;
        mc_client_screen_list_state list;
        if (!mc_sign_draw_base(c, &base))
            return false;
        if (base.da.e == NULL)
            return mc_sign_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                     "DA actual List.size null receiver", "da", "a", "(IIF)V", 9,
                                     false);
        if (!mc_client_screen_list_inspect(base.da.e, &list) || !list.native_live)
            return mc_sign_draw_fail(c, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
                                     "DA actual live Button List native profile", "da", "a",
                                     "(IIF)V", 9, true);
        if (index >= list.size)
            return true;
        if (index == c->limits.max_buttons)
            return mc_sign_draw_fail(c, MC_CLIENT_SCREEN_LIMIT, "DA Button loop native work bound",
                                     "da", "a", "(IIF)V", 9, true);
        if (!mc_sign_draw_base(c, &base))
            return false;
        if (base.da.e == NULL)
            return mc_sign_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                     "DA fresh List.get null receiver", "da", "a", "(IIF)V", 23,
                                     false);
        const mc_client_source_ke *borrowed = NULL;
        if (!mc_client_screen_list_button(base.da.e, index, &borrowed))
            return mc_sign_draw_fail(c, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
                                     "DA actual typed Button List.get native profile", "da", "a",
                                     "(IIF)V", 23, true);
        if (borrowed == NULL)
            return mc_sign_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                     "DA actual null Button draw receiver", "da", "a", "(IIF)V", 41,
                                     false);
        mc_sign_draw_button *record =
            mc_sign_draw_allocate(c, sizeof(*record), "DA Button local native custody", 233);
        if (record == NULL)
            return false;
        mc_client_source_ke *button = (mc_client_source_ke *)borrowed;
        if (mc_client_screen_button_retain(button, &c->error.screen) != MC_CLIENT_SCREEN_OK) {
            free(record);
            c->state.allocation_bytes -= sizeof(*record);
            return false;
        }
        record->button = button;
        record->next = c->buttons;
        c->buttons = record;
        ++c->state.buttons_seen;
        if (!mc_sign_draw_base(c, &base))
            return false;
        if (!draw_button(c, button, base.da.b))
            return false;
    }
}
