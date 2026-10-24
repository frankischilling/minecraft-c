#include "gui_main_menu.h"

#include "mc/core/math/math.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct text_transform {
    double x, y, scale, cosine, sine;
} text_transform;

struct mc_gui_main_menu {
    mc_client_display *display;
    mc_client_assets *assets;
    mc_font_renderer *font;
    mc_image_view font_atlas;
    mc_client_texture font_texture, background_texture, logo_texture, button_texture;
    mc_math_trig trig;
    bool trig_ready;
};

static bool fail(mc_gui_main_menu_error *error, mc_gui_main_menu_status status,
                 const char *operation) {
    if (error != NULL)
        *error = (mc_gui_main_menu_error){.status = status, .operation = operation};
    return false;
}

static bool asset_failure(mc_gui_main_menu_error *error, const char *operation,
                          const mc_client_assets_error *cause) {
    fail(error, MC_GUI_MAIN_MENU_ASSETS_FAILED, operation);
    if (error != NULL)
        error->assets = *cause;
    return false;
}

static bool display_failure(mc_gui_main_menu_error *error, const char *operation,
                            const mc_client_display_error *cause) {
    fail(error, MC_GUI_MAIN_MENU_DISPLAY_FAILED, operation);
    if (error != NULL)
        error->display = *cause;
    return false;
}

static bool font_failure(mc_gui_main_menu_error *error, const char *operation,
                         mc_font_renderer_status status) {
    fail(error, MC_GUI_MAIN_MENU_FONT_FAILED, operation);
    if (error != NULL)
        error->font = status;
    return false;
}

static bool success(mc_gui_main_menu_error *error) {
    if (error != NULL)
        *error = (mc_gui_main_menu_error){0};
    return true;
}

static bool texture(mc_gui_main_menu *menu, const char *name, mc_client_texture *out,
                    mc_gui_main_menu_error *error) {
    const mc_image *image = NULL;
    mc_client_assets_error assets = {0};
    if (mc_client_assets_image(menu->assets, name, &image, &assets) != MC_CLIENT_ASSETS_OK)
        return asset_failure(error, "load GuiMainMenu image", &assets);
    mc_image_view view = {0};
    if (!mc_image_view_get(image, &view))
        return fail(error, MC_GUI_MAIN_MENU_ASSETS_FAILED, "inspect GuiMainMenu image");
    mc_client_display_error display = {0};
    if (!mc_client_display_upload_texture(menu->display, &view, false, out, &display))
        return display_failure(error, "upload GuiMainMenu image", &display);
    return true;
}

static bool quad(mc_gui_main_menu *menu, mc_client_texture image, double x, double y, double width,
                 double height, float u, float v, float du, float dv, uint32_t argb,
                 const text_transform *transform, mc_gui_main_menu_error *error) {
    /* Source quads start at bottom left, preserving the Tessellator's diagonal. */
    mc_client_vertex vertices[4] = {{x, y + height, u, v + dv, argb},
                                    {x + width, y + height, u + du, v + dv, argb},
                                    {x + width, y, u + du, v, argb},
                                    {x, y, u, v, argb}};
    if (transform != NULL) {
        for (size_t i = 0; i < 4; ++i) {
            const double px = vertices[i].x * transform->scale;
            const double py = vertices[i].y * transform->scale;
            vertices[i].x = transform->x + px * transform->cosine - py * transform->sine;
            vertices[i].y = transform->y + px * transform->sine + py * transform->cosine;
        }
    }
    mc_client_display_error display = {0};
    if (!mc_client_display_draw_quad(menu->display, image, vertices, false, true, &display))
        return display_failure(error, "draw GuiMainMenu quad", &display);
    return true;
}

static bool font_ready(mc_gui_main_menu *menu, mc_gui_main_menu_error *error) {
    if (menu->font != NULL)
        return true;
    mc_font_text mapping = {0};
    mc_client_assets_error assets = {0};
    if (mc_client_assets_font_txt(menu->assets, &mapping, &assets) != MC_CLIENT_ASSETS_OK)
        return asset_failure(error, "load FontRenderer mapping", &assets);
    const mc_font_renderer_status status = mc_font_renderer_create(
        &menu->font_atlas, mapping.units, mapping.length, false, &menu->font);
    return status == MC_FONT_RENDERER_OK || font_failure(error, "create FontRenderer", status);
}

static bool button_rectangle(mc_gui_main_menu *menu, int32_t x, int32_t y, int32_t texture_x,
                             int32_t texture_y, int32_t width, int32_t height,
                             mc_gui_main_menu_error *error) {
    const double right = mc_java_i32_add(x, width), bottom = mc_java_i32_add(y, height);
    const float left_u = (float)texture_x / 256.0f;
    const float right_u = (float)mc_java_i32_add(texture_x, width) / 256.0f;
    const float top_v = (float)texture_y / 256.0f;
    const float bottom_v = (float)mc_java_i32_add(texture_y, height) / 256.0f;
    const mc_client_vertex vertices[4] = {{x, bottom, left_u, bottom_v, UINT32_C(0xffffffff)},
                                          {right, bottom, right_u, bottom_v, UINT32_C(0xffffffff)},
                                          {right, y, right_u, top_v, UINT32_C(0xffffffff)},
                                          {x, y, left_u, top_v, UINT32_C(0xffffffff)}};
    mc_client_display_error display = {0};
    if (!mc_client_display_draw_quad(menu->display, menu->button_texture, vertices, false, true,
                                     &display))
        return display_failure(error, "draw GuiButton rectangle", &display);
    return true;
}

static bool measure(mc_gui_main_menu *menu, const mc_font_text *value, int32_t *out,
                    mc_gui_main_menu_error *error) {
    if (value == NULL) {
        *out = 0;
        return true;
    }
    if (!font_ready(menu, error))
        return false;
    const mc_font_renderer_status status = mc_font_renderer_measure(menu->font, value, out);
    return status == MC_FONT_RENDERER_OK || font_failure(error, "measure GuiMainMenu text", status);
}

static bool text(mc_gui_main_menu *menu, const mc_font_text *value, int32_t x, int32_t y,
                 uint32_t color, bool centered, const text_transform *transform,
                 mc_gui_main_menu_error *error) {
    if (value == NULL)
        return true;
    int32_t width = 0;
    if (!measure(menu, value, &width, error))
        return false;
    if (centered)
        x = mc_java_i32_sub(x, width / 2);
    mc_font_run run = {0};
    const mc_font_renderer_status status =
        mc_font_renderer_layout(menu->font, value, x, y, color, true, &run);
    if (status != MC_FONT_RENDERER_OK)
        return font_failure(error, "draw GuiMainMenu text", status);
    for (size_t i = 0; i < run.count; ++i) {
        const mc_font_glyph *glyph = &run.glyphs[i];
        const float u = (float)(glyph->glyph_index % 16u) / 16.0f;
        const float v = (float)(glyph->glyph_index / 16u) / 16.0f;
        if (!quad(menu, menu->font_texture, glyph->x, glyph->y, MC_FONT_RENDERER_GLYPH_EXTENT,
                  MC_FONT_RENDERER_GLYPH_EXTENT, u, v, MC_FONT_RENDERER_GLYPH_EXTENT / 128.0f,
                  MC_FONT_RENDERER_GLYPH_EXTENT / 128.0f, glyph->argb, transform, error))
            return false;
    }
    return true;
}

static bool ascii_text(mc_gui_main_menu *menu, const char *value, int32_t x, int32_t y,
                       uint32_t color, mc_gui_main_menu_error *error) {
    uint16_t units[128];
    const size_t length = strlen(value);
    if (length > sizeof(units) / sizeof(units[0]))
        return fail(error, MC_GUI_MAIN_MENU_LIMIT, "constant GuiMainMenu text");
    for (size_t i = 0; i < length; ++i)
        units[i] = (unsigned char)value[i];
    const mc_font_text value_view = {units, length};
    return text(menu, &value_view, x, y, color, false, NULL, error);
}

static bool button(mc_gui_main_menu *menu, const mc_gui_button *value, mc_gui_mouse_position mouse,
                   mc_gui_main_menu_error *error) {
    if (!value->visible)
        return true;
    if (menu->button_texture == 0 && !texture(menu, "/gui/gui.png", &menu->button_texture, error))
        return false;
    const bool hovered = mouse.x >= value->x && mouse.y >= value->y &&
                         mouse.x < mc_java_i32_add(value->x, value->width) &&
                         mouse.y < mc_java_i32_add(value->y, value->height);
    const int32_t row_index = !value->enabled ? 0 : hovered ? 2 : 1;
    const int32_t row = 46 + row_index * 20;
    const int32_t half = value->width / 2;
    if (value->z_level != 0.0f)
        return fail(error, MC_GUI_MAIN_MENU_UNSUPPORTED_STATE, "draw GuiButton z-level");
    if (!button_rectangle(menu, value->x, value->y, 0, row, half, value->height, error) ||
        !button_rectangle(menu, mc_java_i32_add(value->x, half), value->y,
                          mc_java_i32_sub(200, half), row, half, value->height, error))
        return false;
    const uint32_t color = !value->enabled ? UINT32_C(0xffa0a0a0)
                           : hovered       ? UINT32_C(0xffffa0)
                                           : UINT32_C(0xe0e0e0);
    return text(menu, value->label, mc_java_i32_add(value->x, half),
                mc_java_i32_add(value->y, mc_java_i32_sub(value->height, 8) / 2), color, true, NULL,
                error);
}

bool mc_gui_main_menu_create(mc_client_display *display, mc_client_assets *assets,
                             mc_gui_main_menu **out, mc_gui_main_menu_error *error) {
    if (display == NULL || assets == NULL || out == NULL)
        return fail(error, MC_GUI_MAIN_MENU_INVALID_ARGUMENT, "create GuiMainMenu renderer");
    mc_gui_main_menu *menu = calloc(1, sizeof(*menu));
    if (menu == NULL)
        return fail(error, MC_GUI_MAIN_MENU_ALLOCATION_FAILED, "create GuiMainMenu renderer");
    menu->display = display;
    menu->assets = assets;
    const mc_image *font = NULL;
    mc_client_assets_error assets_error = {0};
    if (mc_client_assets_image(assets, "/font/default.png", &font, &assets_error) !=
        MC_CLIENT_ASSETS_OK) {
        free(menu);
        return asset_failure(error, "load GuiMainMenu font image", &assets_error);
    }
    if (!mc_image_view_get(font, &menu->font_atlas)) {
        free(menu);
        return fail(error, MC_GUI_MAIN_MENU_ASSETS_FAILED, "inspect GuiMainMenu font image");
    }
    mc_client_display_error display_error = {0};
    if (!mc_client_display_upload_texture(display, &menu->font_atlas, false, &menu->font_texture,
                                          &display_error)) {
        free(menu);
        return display_failure(error, "upload GuiMainMenu font image", &display_error);
    }
    *out = menu;
    return success(error);
}

void mc_gui_main_menu_free(mc_gui_main_menu *menu) {
    if (menu != NULL) {
        mc_font_renderer_free(menu->font);
        free(menu);
    }
}

bool mc_gui_main_menu_draw(mc_gui_main_menu *menu, const mc_gui_main_menu_frame *frame,
                           mc_gui_main_menu_millis_fn read_millis, void *clock_context,
                           mc_gui_main_menu_error *error) {
    if (menu == NULL || frame == NULL || read_millis == NULL ||
        (frame->button_count != 0 && frame->buttons == NULL))
        return fail(error, MC_GUI_MAIN_MENU_INVALID_ARGUMENT, "draw GuiMainMenu");
    if (frame->button_count > MC_GUI_MAIN_MENU_MAX_BUTTONS)
        return fail(error, MC_GUI_MAIN_MENU_LIMIT, "draw GuiMainMenu buttons");
    const mc_scaled_resolution *scale = &frame->resolution;
    if (scale->width <= 0 || scale->height <= 0 || !isfinite(scale->width_exact) ||
        !isfinite(scale->height_exact) || scale->width_exact <= 0 || scale->height_exact <= 0)
        return fail(error, MC_GUI_MAIN_MENU_INVALID_ARGUMENT, "GuiMainMenu projection");
    mc_client_display_error display = {0};
    if (!mc_client_display_begin(menu->display, scale->width_exact, scale->height_exact, &display))
        return display_failure(error, "begin GuiMainMenu frame", &display);
    if (menu->background_texture == 0 &&
        !texture(menu, "/gui/background.png", &menu->background_texture, error))
        return false;
    if (!quad(menu, menu->background_texture, 0, 0, scale->width, scale->height, 0, 0,
              (float)scale->width / 32.0f, (float)scale->height / 32.0f, UINT32_C(0xff404040), NULL,
              error))
        return false;
    if (menu->logo_texture == 0 && !texture(menu, "/title/mclogo.png", &menu->logo_texture, error))
        return false;
    const int32_t logo_x = mc_java_i32_sub(scale->width / 2, 274 / 2);
    if (frame->z_level != 0.0f)
        return fail(error, MC_GUI_MAIN_MENU_UNSUPPORTED_STATE, "draw GuiMainMenu logo z-level");
    if (!quad(menu, menu->logo_texture, logo_x, 30, 155, 44, 0, 0, 155.0f / 256.0f, 44.0f / 256.0f,
              UINT32_C(0xffffffff), NULL, error) ||
        !quad(menu, menu->logo_texture, mc_java_i32_add(logo_x, 155), 30, 155, 44, 0,
              45.0f / 256.0f, 155.0f / 256.0f, 44.0f / 256.0f, UINT32_C(0xffffffff), NULL, error))
        return false;
    int64_t millis = 0;
    if (!read_millis(clock_context, &millis))
        return fail(error, MC_GUI_MAIN_MENU_CLOCK_FAILED, "GuiMainMenu currentTimeMillis");
    float phase = (float)(millis % 1000) / 1000.0f;
    phase = phase * 3.1415927f;
    phase = phase * 2.0f;
    if (!menu->trig_ready) {
        if (!mc_math_trig_init(&menu->trig))
            return fail(error, MC_GUI_MAIN_MENU_UNSUPPORTED_STATE, "initialize GuiMainMenu trig");
        menu->trig_ready = true;
    }
    float splash_scale = 1.8f - fabsf(mc_math_sin(&menu->trig, phase) * 0.1f);
    int32_t splash_width = 0;
    if (!measure(menu, frame->splash, &splash_width, error))
        return false;
    splash_scale = splash_scale * 100.0f / (float)mc_java_i32_add(splash_width, 32);
    const double angle = -20.0 * 3.14159265358979323846 / 180.0;
    const text_transform transform = {(double)mc_java_i32_add(scale->width / 2, 90), 70,
                                      splash_scale, cos(angle), sin(angle)};
    if (!text(menu, frame->splash, 0, -8, UINT32_C(0xffff00), true, &transform, error) ||
        !ascii_text(menu, "Minecraft Beta 1.7.3", 2, 2, UINT32_C(0x505050), error))
        return false;
    static const char copyright[] = "Copyright Mojang AB. Do not distribute.";
    uint16_t units[sizeof(copyright) - 1];
    for (size_t i = 0; i < sizeof(units) / sizeof(units[0]); ++i)
        units[i] = (unsigned char)copyright[i];
    const mc_font_text copyright_text = {units, sizeof(copyright) - 1};
    int32_t width = 0;
    if (!measure(menu, &copyright_text, &width, error) ||
        !text(menu, &copyright_text, mc_java_i32_sub(mc_java_i32_sub(scale->width, width), 2),
              mc_java_i32_sub(scale->height, 10), UINT32_C(0xffffff), false, NULL, error))
        return false;
    for (size_t i = 0; i < frame->button_count; ++i)
        if (!button(menu, &frame->buttons[i], frame->mouse, error))
            return false;
    return success(error);
}
