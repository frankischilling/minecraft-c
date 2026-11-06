#include "source_screen_drawing.h"
#include "../application_state.h"
#include "source_screen_initialize_internal.h"

#include "mc/client/texture_packs/texture_packs.h"

#include <string.h>

static bool failed(mc_client_source_screen_draw_error *error,
                   mc_client_source_screen_draw_status status, const char *operation) {
    error->status = status;
    error->operation = operation;
    return false;
}

typedef struct source_measurement {
    mc_client_bitmap_font *font;
    mc_client_source_screen_draw_error *error;
} source_measurement;

static bool measure(void *context, const mc_font_text *text, int32_t *out) {
    source_measurement *call = context;
    mc_player_string_view view;
    const mc_player_string_view *argument = NULL;
    if (text != NULL) {
        view = (mc_player_string_view){text->units, text->length};
        argument = &view;
    }
    ++call->error->font_measurements;
    return mc_client_bitmap_font_measure_view(call->font, argument, out, &call->error->font) ==
           MC_CLIENT_BITMAP_FONT_OK;
}

bool mc_client_source_screen_draw(mc_client_source_da *screen, mc_gui_main_menu *renderer,
                                  const mc_scaled_resolution *projection,
                                  mc_gui_mouse_position mouse, float partial_tick,
                                  mc_gui_main_menu_millis_fn read_millis, void *clock_context,
                                  mc_client_source_screen_draw_error *error) {
    if (error == NULL)
        return false;
    memset(error, 0, sizeof(*error));
    if (screen == NULL || renderer == NULL || projection == NULL || read_millis == NULL)
        return failed(error, MC_CLIENT_SOURCE_SCREEN_DRAW_ARGUMENT,
                      "source menu drawing arguments");
    mc_client_screen_state state;
    if (mc_client_screen_inspect(screen, &state, &error->screen) != MC_CLIENT_SCREEN_OK)
        return failed(error, MC_CLIENT_SOURCE_SCREEN_DRAW_OWNER_FAILED,
                      "source menu owner inspection");
    if (!state.is_menu || state.phase != MC_CLIENT_SCREEN_READY ||
        screen->last_initialization == NULL ||
        screen->last_initialization->phase != MC_CLIENT_SCREEN_INIT_RETURNED)
        return failed(error, MC_CLIENT_SOURCE_SCREEN_DRAW_UNSUPPORTED,
                      "completed source GuiMainMenu initialization required");
    client_application *app = state.da.b;
    if (app == NULL || state.da.g == NULL || state.da.e == NULL || app->active_world != NULL)
        return failed(error, MC_CLIENT_SOURCE_SCREEN_DRAW_UNSUPPORTED,
                      "source menu App/Font/List/no-World profile required");
    /* FU measures through captured da.g, while each ordinary KE captures the
     * current App.q. This native bridge admits their common-font profile. */
    if (state.da.g != app->font_renderer)
        return failed(error, MC_CLIENT_SOURCE_SCREEN_DRAW_UNSUPPORTED,
                      "source menu and buttons require the same canonical Font profile");
    mc_texture_pack_list_state packs;
    mc_game_settings_state settings;
    if (!mc_client_texture_pack_list_inspect(app->D, &packs) ||
        packs.phase != MC_TEXTURE_PACK_READY || packs.default_pack == NULL ||
        packs.selected_pack != packs.default_pack ||
        !mc_game_settings_read_state(app->settings, &settings))
        return failed(error, MC_CLIENT_SOURCE_SCREEN_DRAW_UNSUPPORTED,
                      "source menu selected default texture pack and Settings required");
    if (screen->classes->registry != app->registry ||
        !mc_gui_main_menu_same_source(renderer, app->display, screen->classes->resources,
                                      screen->classes->registry, settings.values.anaglyph))
        return failed(error, MC_CLIENT_SOURCE_SCREEN_DRAW_UNSUPPORTED,
                      "source menu and native renderer must share resource identities");
    mc_client_screen_list_state list;
    if (!mc_client_screen_list_inspect(state.da.e, &list) || !list.native_live ||
        list.size > MC_GUI_MAIN_MENU_MAX_BUTTONS)
        return failed(error, MC_CLIENT_SOURCE_SCREEN_DRAW_UNSUPPORTED,
                      "source menu live bounded button list required");

    mc_gui_button buttons[MC_GUI_MAIN_MENU_MAX_BUTTONS];
    mc_font_text labels[MC_GUI_MAIN_MENU_MAX_BUTTONS];
    for (size_t i = 0; i < list.size; ++i) {
        const mc_client_source_ke *button = NULL;
        mc_client_screen_ke_state current;
        if (!mc_client_screen_list_button(state.da.e, i, &button) || button == NULL ||
            mc_client_screen_button_inspect((mc_client_source_ke *)button, &current,
                                            &error->screen) != MC_CLIENT_SCREEN_OK ||
            current.phase != MC_CLIENT_SCREEN_READY)
            return failed(error, MC_CLIENT_SOURCE_SCREEN_DRAW_OWNER_FAILED,
                          "source menu concrete GuiButton owner required");
        const mc_client_screen_ke_fields *value = &current.fields;
        const mc_font_text *label = NULL;
        if (value->h && value->e.kind != MC_CLIENT_SCREEN_TEXT_NULL) {
            mc_player_string_view view;
            if (!mc_client_screen_text_read(value->e, &view))
                return failed(error, MC_CLIENT_SOURCE_SCREEN_DRAW_OWNER_FAILED,
                              "source GuiButton canonical label view");
            labels[i] = (mc_font_text){view.units, view.length};
            label = &labels[i];
        }
        buttons[i] = (mc_gui_button){.id = value->f,
                                     .x = value->c,
                                     .y = value->d,
                                     .width = value->a,
                                     .height = value->b,
                                     .label = label,
                                     .enabled = value->g,
                                     .visible = value->h,
                                     .z_level = value->k};
    }
    mc_font_text splash;
    const mc_font_text *splash_argument = NULL;
    if (state.fu.j != NULL) {
        mc_player_string_view view;
        if (!mc_player_string_read(state.fu.j, &view))
            return failed(error, MC_CLIENT_SOURCE_SCREEN_DRAW_OWNER_FAILED,
                          "source GuiMainMenu canonical splash view");
        splash = (mc_font_text){view.units, view.length};
        splash_argument = &splash;
    }
    mc_gui_main_menu_frame frame = {.resolution = *projection,
                                    .splash = splash_argument,
                                    .buttons = buttons,
                                    .button_count = list.size,
                                    .mouse = mouse,
                                    .z_level = state.da.k,
                                    .partial_tick = partial_tick};
    frame.resolution.width = state.da.c;
    frame.resolution.height = state.da.d;
    source_measurement font = {state.da.g, error};
    error->button_count = list.size;
    if (!mc_gui_main_menu_draw_measured(renderer, &frame, read_millis, clock_context, measure,
                                        &font, &error->renderer))
        return failed(error,
                      error->font.status == MC_CLIENT_BITMAP_FONT_OK
                          ? MC_CLIENT_SOURCE_SCREEN_DRAW_RENDER_FAILED
                          : MC_CLIENT_SOURCE_SCREEN_DRAW_FONT_FAILED,
                      error->renderer.operation);
    return true;
}
