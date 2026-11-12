#include "source_pause_drawing.h"

#include "../application_state.h"
#include "mc/client/texture_packs/texture_packs.h"
#include "mc/core/java/java.h"
#include "source_pause_screen_internal.h"

#include <string.h>
#include <xmmintrin.h>

static bool fail(mc_client_pause_draw_error *e, mc_client_screen_status status,
                 const char *operation, unsigned int pc, bool native) {
    mc_screen_fail(&e->screen, status, operation, "oz", "a", "(IIF)V", pc);
    e->screen.native_policy = native;
    return false;
}

typedef struct measurement {
    mc_client_bitmap_font *font;
    mc_client_pause_draw_error *error;
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

static bool text(mc_client_source_oz *o, mc_gui_main_menu *renderer, const char *ascii, int32_t x,
                 int32_t y, uint32_t color, bool centered, unsigned int pc,
                 mc_client_pause_draw_error *e) {
    mc_player_string *literal = NULL;
    if (!mc_screen_lock(o->base.classes, &e->screen))
        return false;
    const bool found = mc_screen_literal_locked(o->base.classes, ascii, &literal, &e->screen);
    mtx_unlock(&o->base.classes->mutex);
    if (!found)
        return false;
    mc_player_string_view view;
    if (!mc_player_string_read(literal, &view))
        return fail(e, MC_CLIENT_SCREEN_STRING_FAILED, "oz canonical text String", pc, true);
    const mc_font_text label = {view.units, view.length};
    return mc_gui_canvas_text(renderer, &label, x, y, color, centered, &e->renderer) ||
           fail(e, MC_CLIENT_SCREEN_FONT_FAILED, e->renderer.operation, pc, true);
}

static bool background(mc_client_source_oz *o, mc_gui_main_menu *renderer,
                       mc_client_pause_draw_error *e) {
    const bool dirt = o->base.fields.b->active_world == NULL;
    const int32_t w = o->base.fields.c, h = o->base.fields.d;
    const uint32_t upper = dirt ? UINT32_C(0xff404040) : UINT32_C(0xc0101010);
    const uint32_t lower = dirt ? UINT32_C(0xff404040) : UINT32_C(0xd0101010);
    const float u = dirt ? (float)w / 32.0f : 0.0f, v = dirt ? (float)h / 32.0f : 0.0f;
    const mc_client_vertex quad[4] = {
        {0, h, 0, v, lower}, {w, h, u, v, lower}, {w, 0, u, 0, upper}, {0, 0, 0, 0, upper}};
    return mc_gui_canvas_quad(renderer, dirt, quad, !dirt, false, &e->renderer) ||
           fail(e, MC_CLIENT_SCREEN_TEXTURE_FAILED, e->renderer.operation, 1, true);
}

static float addf(float a, float b) {
    return _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static float mulf(float a, float b) {
    return _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

bool mc_client_source_pause_draw(mc_client_source_oz *o, mc_gui_main_menu *renderer,
                                 const mc_scaled_resolution *projection,
                                 mc_gui_mouse_position mouse, float partial_tick,
                                 mc_math_classes *math, mc_client_pause_draw_error *e) {
    if (e == NULL)
        return false;
    memset(e, 0, sizeof(*e));
    if (o == NULL || renderer == NULL || projection == NULL)
        return fail(e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "oz drawing native arguments", 0, true);
    mc_client_source_da *base = &o->base;
    mc_client_screen_classes *classes = base->classes;
    if (!mc_screen_lock(classes, &e->screen))
        return false;
    if (base->kind != MC_CLIENT_SCREEN_PAUSE || base->phase != MC_CLIENT_SCREEN_READY ||
        base->active_calls != 0 || o->last_initialization == NULL ||
        !o->last_initialization->returned || base->fields.b == NULL || base->fields.g == NULL ||
        base->fields.e == NULL) {
        mtx_unlock(&classes->mutex);
        return fail(e, MC_CLIENT_SCREEN_INVALID_PHASE, "oz initialized exclusive drawing owner", 0,
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
             "oz same App/Font/default-Pack/resource drawing profile", 0, true);
        goto leave;
    }
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        fail(e, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY, "oz native GUI floating profile", 1,
             true);
        goto leave;
    }
    mc_scaled_resolution scale = *projection;
    scale.width = base->fields.c;
    scale.height = base->fields.d;
    measurement m = {base->fields.g, e};
    if (!mc_gui_canvas_begin(renderer, &scale, measure, &m, &e->renderer)) {
        fail(e, MC_CLIENT_SCREEN_TEXTURE_FAILED, e->renderer.operation, 1, true);
        goto leave;
    }
    canvas = true;
    if (!background(o, renderer, e))
        goto leave;
    e->background_returned = true;
    /* Capture actual World8 before source a++19, including a null World fault22. */
    mc_world_runtime *world = base->fields.b->active_world;
    e->pass = o->save_pass;
    o->save_pass = mc_java_i32_add(o->save_pass, 1);
    e->counter_stored = true;
    o->last_pc = 19;
    if (world == NULL) {
        fail(e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "oz actual World.a(I) null receiver", 22,
             false);
        goto leave;
    }
    if (mc_world_runtime_save_client_detailed(world, e->pass, &e->save_completed, &e->save) !=
        MC_CHUNK_LIFECYCLE_OK) {
        fail(e, MC_CLIENT_SCREEN_APP_FAILED, "oz actual World.a(I) save", 22, true);
        goto leave;
    }
    e->save_returned = true;
    if (!e->save_completed || o->ticks < 20) {
        if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
            fail(e, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
                 "oz saving-label native floating profile", 56, true);
            goto leave;
        }
        const float converted = _mm_cvtss_f32(_mm_cvtsi32_ss(_mm_setzero_ps(), o->ticks % 10));
        float pulse =
            _mm_cvtss_f32(_mm_div_ss(_mm_set_ss(addf(converted, partial_tick)), _mm_set_ss(10.0f)));
        pulse = mulf(mulf(pulse, 3.1415927f), 2.0f);
        if (mc_math_classes_ensure(math, &e->math) != MC_MATH_CLASS_OK) {
            fail(e, MC_CLIENT_SCREEN_APP_FAILED, "oz actual MathHelper active use", 71, true);
            goto leave;
        }
        if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
            fail(e, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
                 "oz post-MathHelper native floating environment", 71, true);
            goto leave;
        }
        pulse = addf(mulf(mc_math_sin(mc_math_classes_table(math), pulse), 0.2f), 0.8f);
        const int32_t shade = mc_java_float_to_i32(mulf(255.0f, pulse));
        e->saving_color = (uint32_t)mc_java_i32_shl(shade, 16) |
                          (uint32_t)mc_java_i32_shl(shade, 8) | (uint32_t)shade;
        if (!text(o, renderer, "Saving level..", 8, mc_java_i32_sub(base->fields.d, 16),
                  e->saving_color, false, 120, e))
            goto leave;
        e->saving_label_drawn = true;
    }
    if (!text(o, renderer, "Game menu", base->fields.c / 2, 40, 0xffffffu, true, 140, e))
        goto leave;
    const mc_client_screen_list *list = base->fields.e;
    for (size_t index = 0; index < list->size; ++index) {
        const mc_client_source_ke *button = list->entries[index];
        if (button == NULL || button->phase != MC_CLIENT_SCREEN_READY) {
            fail(e,
                 button == NULL ? MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE
                                : MC_CLIENT_SCREEN_INVALID_PHASE,
                 "oz inherited DA Button draw receiver", 147, button != NULL);
            goto leave;
        }
        const mc_client_screen_ke_fields *f = &button->fields;
        mc_font_text label;
        const mc_font_text *argument = NULL;
        mc_player_string_view view;
        if (f->h && f->e.kind != MC_CLIENT_SCREEN_TEXT_NULL) {
            if (!mc_client_screen_text_read(f->e, &view)) {
                fail(e, MC_CLIENT_SCREEN_STRING_FAILED, "oz Button canonical label", 147, true);
                goto leave;
            }
            label = (mc_font_text){view.units, view.length};
            argument = &label;
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
            fail(e, MC_CLIENT_SCREEN_TEXTURE_FAILED, e->renderer.operation, 147, true);
            goto leave;
        }
        ++e->buttons_drawn;
    }
    o->last_pc = 150;
    e->source_returned = e->screen.source_returned = true;
    e->screen.site = (mc_client_screen_site){"oz", "a", "(IIF)V", 150};
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

mc_client_screen_status mc_client_source_pause_draw_retire(mc_client_pause_draw_error *e) {
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
