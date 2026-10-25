#include "gui_main_menu.h"

#include "mc/core/math/math.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

struct mc_gui_main_menu {
    mc_client_display *display;
    mc_client_assets *assets;
    mc_font_renderer *font;
    mc_image_view font_atlas;
    mc_client_texture font_texture, background_texture, logo_texture, button_texture;
    mc_math_trig trig;
    bool trig_ready, anaglyph;
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
    if (!mc_client_display_upload_texture(menu->display, &view, menu->anaglyph, out, &display))
        return display_failure(error, "upload GuiMainMenu image", &display);
    return true;
}

static bool quad(mc_gui_main_menu *menu, mc_client_texture image, double x, double y, double width,
                 double height, float u, float v, float du, float dv, uint32_t argb,
                 mc_gui_main_menu_error *error) {
    /* Source quads start at bottom left, preserving the Tessellator's diagonal. */
    mc_client_vertex vertices[4] = {{x, y + height, u, v + dv, argb},
                                    {x + width, y + height, u + du, v + dv, argb},
                                    {x + width, y, u + du, v, argb},
                                    {x, y, u, v, argb}};
    mc_client_display_error display = {0};
    if (!mc_client_display_draw_quad(menu->display, image, vertices, false, true, &display))
        return display_failure(error, "draw GuiMainMenu quad", &display);
    return true;
}

/* Keep each source binary32 product/add as a separate rounded operation.
 * Volatile stores also prevent a contracted multiply-add across helpers. */
static float model_product(float left, float right) {
    volatile float rounded = left * right;
    return rounded;
}

static float model_sum(float left, float right) {
    volatile float rounded = left + right;
    return rounded;
}

static void model_translate(mc_client_modelview *matrix, float x, float y, float z) {
    for (size_t row = 0; row < 4; ++row) {
        const float px = model_product(matrix->column_major[row], x);
        const float py = model_product(matrix->column_major[4 + row], y);
        const float pz = model_product(matrix->column_major[8 + row], z);
        const float xy = model_sum(px, py);
        const float xyz = model_sum(xy, pz);
        matrix->column_major[12 + row] = model_sum(xyz, matrix->column_major[12 + row]);
    }
}

static mc_client_modelview model_base(void) {
    mc_client_modelview matrix = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
    model_translate(&matrix, 0.0f, 0.0f, -2000.0f);
    return matrix;
}

static void model_rotate_z(mc_client_modelview *matrix, float degrees) {
    const float radians = (float)((double)degrees * 3.14159265358979323846 / 180.0);
    const float cosine = (float)cos((double)radians);
    const float sine = (float)sin((double)radians);
    for (size_t row = 0; row < 4; ++row) {
        const float x = matrix->column_major[row], y = matrix->column_major[4 + row];
        const float xc = model_product(x, cosine);
        const float ys = model_product(y, sine);
        const float column_x = model_sum(xc, ys);
        const float xns = model_product(x, -sine);
        const float yc = model_product(y, cosine);
        const float column_y = model_sum(xns, yc);
        matrix->column_major[row] = column_x;
        matrix->column_major[4 + row] = column_y;
    }
}

static void model_scale(mc_client_modelview *matrix, float x, float y, float z) {
    const float scale[3] = {x, y, z};
    for (size_t column = 0; column < 3; ++column)
        for (size_t row = 0; row < 4; ++row)
            matrix->column_major[4 * column + row] =
                model_product(matrix->column_major[4 * column + row], scale[column]);
}

static bool glyph_list(mc_gui_main_menu *menu, uint16_t index, uint32_t color,
                       const mc_client_modelview *matrix, mc_gui_main_menu_error *error) {
    const float extent = MC_FONT_RENDERER_GLYPH_EXTENT;
    const float atlas_x = (float)(index % 16u * MC_FONT_RENDERER_CELL_SIZE);
    const float atlas_y = (float)(index / 16u * MC_FONT_RENDERER_CELL_SIZE);
    const float left = atlas_x / 128.0f, right = model_sum(atlas_x, extent) / 128.0f;
    const float top = atlas_y / 128.0f, bottom = model_sum(atlas_y, extent) / 128.0f;
    const mc_client_vertex vertices[4] = {{0, extent, left, bottom, color},
                                          {extent, extent, right, bottom, color},
                                          {extent, 0, right, top, color},
                                          {0, 0, left, top, color}};
    mc_client_display_error display = {0};
    if (!mc_client_display_draw_quad_modelview(menu->display, menu->font_texture, vertices, matrix,
                                               false, true, &display))
        return display_failure(error, "draw FontRenderer glyph list", &display);
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
        &menu->font_atlas, mapping.units, mapping.length, menu->anaglyph, &menu->font);
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
                 uint32_t color, bool centered, const mc_client_modelview *inherited,
                 mc_gui_main_menu_error *error) {
    if (value == NULL)
        return true;
    int32_t width = 0;
    if (!measure(menu, value, &width, error))
        return false;
    if (centered)
        x = mc_java_i32_sub(x, width / 2);
    mc_font_command_run run = {0};
    const mc_font_renderer_status status =
        mc_font_renderer_layout_commands(menu->font, value, x, y, color, true, &run);
    if (status != MC_FONT_RENDERER_OK)
        return font_failure(error, "draw GuiMainMenu text", status);
    if (run.count > MC_FONT_RENDERER_MAX_COMMANDS || (run.count != 0 && run.commands == NULL))
        return fail(error, MC_GUI_MAIN_MENU_UNSUPPORTED_STATE, "FontRenderer command buffer");
    const mc_client_modelview entry = inherited == NULL ? model_base() : *inherited;
    mc_client_modelview pass = entry;
    bool active = false;
    uint32_t current_color = color;
    for (size_t i = 0; i < run.count; ++i) {
        const mc_font_command *command = &run.commands[i];
        switch (command->kind) {
            case MC_FONT_COMMAND_BEGIN_PASS:
                if (active)
                    goto invalid_command;
                pass = entry;
                model_translate(&pass, command->value.begin_pass.x, command->value.begin_pass.y,
                                0.0f);
                current_color = command->value.begin_pass.argb;
                active = true;
                break;
            case MC_FONT_COMMAND_COLOR:
                if (!active)
                    goto invalid_command;
                current_color = command->value.argb;
                break;
            case MC_FONT_COMMAND_GLYPH_LIST:
                if (!active || command->value.glyph_list.glyph_index >= 256u)
                    goto invalid_command;
                if (!glyph_list(menu, command->value.glyph_list.glyph_index, current_color, &pass,
                                error))
                    goto failure;
                /* Blank/transparent glyphs still execute their source advance. */
                model_translate(&pass, (float)command->value.glyph_list.advance, 0.0f, 0.0f);
                break;
            case MC_FONT_COMMAND_END_PASS:
                if (!active)
                    goto invalid_command;
                pass = entry;
                active = false;
                break;
            default:
                goto invalid_command;
        }
    }
    if (active)
        goto invalid_command;
    return true;
invalid_command:
    fail(error, MC_GUI_MAIN_MENU_UNSUPPORTED_STATE, "FontRenderer command order");
failure:
    /* Only these local copies are pushed; inherited/frame state is untouched. */
    pass = entry;
    active = false;
    return false;
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

bool mc_gui_main_menu_create(mc_client_display *display, mc_client_assets *assets, bool anaglyph,
                             mc_gui_main_menu **out, mc_gui_main_menu_error *error) {
    if (display == NULL || assets == NULL || out == NULL)
        return fail(error, MC_GUI_MAIN_MENU_INVALID_ARGUMENT, "create GuiMainMenu renderer");
    mc_gui_main_menu *menu = calloc(1, sizeof(*menu));
    if (menu == NULL)
        return fail(error, MC_GUI_MAIN_MENU_ALLOCATION_FAILED, "create GuiMainMenu renderer");
    menu->display = display;
    menu->assets = assets;
    menu->anaglyph = anaglyph;
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
    if (!mc_client_display_upload_texture(display, &menu->font_atlas, anaglyph, &menu->font_texture,
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
              (float)scale->width / 32.0f, (float)scale->height / 32.0f, UINT32_C(0xff404040),
              error))
        return false;
    if (menu->logo_texture == 0 && !texture(menu, "/title/mclogo.png", &menu->logo_texture, error))
        return false;
    const int32_t logo_x = mc_java_i32_sub(scale->width / 2, 274 / 2);
    if (frame->z_level != 0.0f)
        return fail(error, MC_GUI_MAIN_MENU_UNSUPPORTED_STATE, "draw GuiMainMenu logo z-level");
    if (!quad(menu, menu->logo_texture, logo_x, 30, 155, 44, 0, 0, 155.0f / 256.0f, 44.0f / 256.0f,
              UINT32_C(0xffffffff), error) ||
        !quad(menu, menu->logo_texture, mc_java_i32_add(logo_x, 155), 30, 155, 44, 0,
              45.0f / 256.0f, 155.0f / 256.0f, 44.0f / 256.0f, UINT32_C(0xffffffff), error))
        return false;
    mc_client_modelview splash = model_base();
    model_translate(&splash, (float)mc_java_i32_add(scale->width / 2, 90), 70.0f, 0.0f);
    model_rotate_z(&splash, -20.0f);
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
    model_scale(&splash, splash_scale, splash_scale, splash_scale);
    if (!text(menu, frame->splash, 0, -8, UINT32_C(0xffff00), true, &splash, error) ||
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
