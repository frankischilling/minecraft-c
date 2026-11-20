#include "source_death_drawing.h"

#include "../application_state.h"
#include "mc/client/texture_packs/texture_packs.h"
#include "mc/entity/player/player.h"
#include "source_death_screen_internal.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <xmmintrin.h>

static bool fail(mc_client_death_draw_error *e, mc_client_screen_status status,
                 const char *operation, unsigned int pc, bool native) {
    mc_screen_fail(&e->screen, status, operation, "ch", "a", "(IIF)V", pc);
    e->screen.native_policy = native;
    return false;
}

typedef struct measurement {
    mc_client_bitmap_font *font;
    mc_client_death_draw_error *error;
} measurement;

static bool measure(void *context, const mc_font_text *text, int32_t *out) {
    measurement *m = context;
    mc_player_string_view view;
    const mc_player_string_view *argument = NULL;
    if (text != NULL) {
        view = (mc_player_string_view){text->units, text->length};
        argument = &view;
    }
    ++m->error->font_measurements;
    return mc_client_bitmap_font_measure_view(m->font, argument, out, &m->error->font) ==
           MC_CLIENT_BITMAP_FONT_OK;
}

static bool label(mc_gui_main_menu *renderer, const char *ascii, int32_t x, int32_t y, float scale,
                  unsigned int pc, mc_client_death_draw_error *e) {
    uint16_t units[64];
    const size_t length = strlen(ascii);
    if (length > sizeof(units) / sizeof(units[0]))
        return fail(e, MC_CLIENT_SCREEN_LIMIT, "ch native text transport", pc, true);
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint16_t)(unsigned char)ascii[i];
    const mc_font_text text = {units, length};
    const bool returned =
        scale == 1.0f
            ? mc_gui_canvas_text(renderer, &text, x, y, UINT32_C(0xffffff), true, &e->renderer)
            : mc_gui_canvas_text_scaled(renderer, &text, x, y, UINT32_C(0xffffff), true, scale,
                                        &e->renderer);
    return returned || fail(e, MC_CLIENT_SCREEN_FONT_FAILED, e->renderer.operation, pc, true);
}

bool mc_client_source_death_draw(mc_client_source_ch *o, mc_gui_main_menu *renderer,
                                 const mc_scaled_resolution *projection,
                                 mc_gui_mouse_position mouse, float partial_tick,
                                 mc_client_death_draw_error *e) {
    if (e == NULL)
        return false;
    memset(e, 0, sizeof(*e));
    if (o == NULL || renderer == NULL || projection == NULL)
        return fail(e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "ch drawing native arguments", 0, true);
    mc_client_source_da *base = &o->base;
    mc_client_screen_classes *classes = base->classes;
    if (!mc_screen_lock(classes, &e->screen))
        return false;
    if (base->kind != MC_CLIENT_SCREEN_DEATH || base->phase != MC_CLIENT_SCREEN_READY ||
        base->active_calls != 0 || o->last_initialization == NULL ||
        !o->last_initialization->returned || base->fields.b == NULL || base->fields.g == NULL ||
        base->fields.e == NULL) {
        mtx_unlock(&classes->mutex);
        return fail(e, MC_CLIENT_SCREEN_INVALID_PHASE, "ch initialized exclusive drawing owner", 0,
                    true);
    }
    ++base->active_calls;
    e->pending_receiver = base;
    mtx_unlock(&classes->mutex);
    bool ok = false, canvas = false;
    client_application *app = base->fields.b;
    mc_game_settings_state settings;
    mc_texture_pack_list_state packs;
    if (base->fields.g != app->font_renderer || classes->registry != app->registry ||
        !mc_game_settings_read_state(app->settings, &settings) ||
        !mc_client_texture_pack_list_inspect(app->D, &packs) ||
        packs.phase != MC_TEXTURE_PACK_READY || packs.default_pack == NULL ||
        packs.selected_pack != packs.default_pack ||
        !mc_gui_main_menu_same_source(renderer, app->display, classes->resources, classes->registry,
                                      settings.values.anaglyph)) {
        fail(e, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
             "ch same App/Font/default-Pack/resource drawing profile", 0, true);
        goto leave;
    }
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        fail(e, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY, "ch native GUI floating profile", 15,
             true);
        goto leave;
    }
    mc_scaled_resolution scale = *projection;
    scale.width = base->fields.c;
    scale.height = base->fields.d;
    measurement m = {base->fields.g, e};
    if (!mc_gui_canvas_begin(renderer, &scale, measure, &m, &e->renderer)) {
        fail(e, MC_CLIENT_SCREEN_TEXTURE_FAILED, e->renderer.operation, 15, true);
        goto leave;
    }
    canvas = true;
    const mc_client_vertex gradient[4] = {
        {0, base->fields.d, 0, 0, UINT32_C(0xa0803030)},
        {base->fields.c, base->fields.d, 0, 0, UINT32_C(0xa0803030)},
        {base->fields.c, 0, 0, 0, UINT32_C(0x60500000)},
        {0, 0, 0, 0, UINT32_C(0x60500000)}};
    if (!mc_gui_canvas_quad(renderer, false, gradient, true, false, &e->renderer)) {
        fail(e, MC_CLIENT_SCREEN_TEXTURE_FAILED, e->renderer.operation, 15, true);
        goto leave;
    }
    e->background_returned = true;
    if (!label(renderer, "Game over!", (base->fields.c / 2) / 2, 30, 2.0f, 46, e))
        goto leave;
    e->title_drawn = true;
    /* CH captures sj57 before allocating its StringBuilder. The actual GS.C
     * reloads the current App.h at73 and reads its score; no cached Player. */
    if (base->fields.b == NULL) {
        fail(e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "ch score null Minecraft receiver", 73,
             false);
        goto leave;
    }
    mc_entity *player = base->fields.b->h;
    if (player == NULL) {
        fail(e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "ch score null Player receiver", 76, false);
        goto leave;
    }
    mc_local_player_snapshot fields;
    if (mc_local_player_get_snapshot(player, &fields) != MC_ENTITY_OK) {
        fail(e, MC_CLIENT_SCREEN_APP_FAILED, "ch actual Player score field", 76, true);
        goto leave;
    }
    e->score = fields.player.g;
    e->score_read = true;
    char score[32];
    const int length = snprintf(score, sizeof(score), "Score: &e%" PRId32, e->score);
    if (length < 0 || (size_t)length >= sizeof(score)) {
        fail(e, MC_CLIENT_SCREEN_LIMIT, "ch native decimal String transport", 82, true);
        goto leave;
    }
    if (!label(renderer, score, base->fields.c / 2, 100, 1.0f, 95, e))
        goto leave;
    e->score_drawn = true;
    const mc_client_screen_list *list = base->fields.e;
    for (size_t i = 0; i < list->size; ++i) {
        const mc_client_source_ke *button = list->entries[i];
        if (button == NULL) {
            fail(e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "ch inherited DA null Button", 102,
                 false);
            goto leave;
        }
        const mc_client_screen_ke_fields *f = &button->fields;
        mc_font_text text;
        const mc_font_text *argument = NULL;
        mc_player_string_view view;
        if (f->h && f->e.kind != MC_CLIENT_SCREEN_TEXT_NULL) {
            if (!mc_client_screen_text_read(f->e, &view)) {
                fail(e, MC_CLIENT_SCREEN_STRING_FAILED, "ch Button canonical label", 102, true);
                goto leave;
            }
            text = (mc_font_text){view.units, view.length};
            argument = &text;
        }
        const mc_gui_button drawn = {.id = f->f,
                                     .x = f->c,
                                     .y = f->d,
                                     .width = f->a,
                                     .height = f->b,
                                     .label = argument,
                                     .enabled = f->g,
                                     .visible = f->h,
                                     .z_level = f->k};
        if (!mc_gui_canvas_button(renderer, &drawn, mouse, &e->renderer)) {
            fail(e, MC_CLIENT_SCREEN_TEXTURE_FAILED, e->renderer.operation, 102, true);
            goto leave;
        }
        ++e->buttons_drawn;
    }
    (void)partial_tick; /* Actual inherited DA draw does not read this argument. */
    o->last_pc = 105;
    e->source_returned = e->screen.source_returned = true;
    e->screen.site = (mc_client_screen_site){"ch", "a", "(IIF)V", 105};
    ok = true;
leave:
    if (canvas)
        mc_gui_canvas_end(renderer);
    mc_client_screen_error cleanup = {0};
    if (!mc_screen_lock(classes, &cleanup)) {
        e->monitor_lost = true;
        e->screen = cleanup;
        return false;
    }
    --base->active_calls;
    e->pending_receiver = NULL;
    mtx_unlock(&classes->mutex);
    return ok;
}

mc_client_screen_status mc_client_source_death_draw_retire(mc_client_death_draw_error *e) {
    if (e == NULL)
        return MC_CLIENT_SCREEN_INVALID_ARGUMENT;
    if (e->pending_receiver == NULL)
        return MC_CLIENT_SCREEN_OK;
    if (!e->monitor_lost)
        return MC_CLIENT_SCREEN_BUSY;
    mc_client_source_da *base = e->pending_receiver;
    mc_client_screen_error cleanup = {0};
    if (!mc_screen_lock(base->classes, &cleanup))
        return cleanup.status;
    --base->active_calls;
    e->pending_receiver = NULL;
    e->monitor_lost = false;
    mtx_unlock(&base->classes->mutex);
    return MC_CLIENT_SCREEN_OK;
}
