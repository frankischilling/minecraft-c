#include "source_create_world_drawing.h"
#include "../application_state.h"
#include "../display/fog_surface.h"
#include "../display/source_surface.h"
#include "../display/source_world_list_surface.h"
#include "mc/client/texture_packs/texture_packs.h"
#include "mc/core/java/java.h"
#include "source_create_world_internal.h"

#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

static bool fail(mc_client_create_world_draw_error *e, mc_client_screen_status status,
                 const char *operation, const char *owner, unsigned int pc, bool native) {
    const bool ro = strcmp(owner, "ro") == 0;
    mc_screen_fail(&e->screen, status, operation, owner, ro ? "c" : "a", ro ? "()V" : "(IIF)V", pc);
    e->screen.native_policy = native;
    return false;
}

static bool translated(mc_client_source_fj *o, const mc_translation *translation, const char *key,
                       mc_client_screen_text *out, unsigned int pc,
                       mc_client_create_world_draw_error *e) {
    mc_player_string *literal = NULL;
    if (!mc_screen_lock(o->base.classes, &e->screen))
        return false;
    const bool found = mc_screen_literal_locked(o->base.classes, key, &literal, &e->screen);
    mtx_unlock(&o->base.classes->mutex);
    if (!found)
        return false;
    if (translation == NULL)
        return fail(e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                    "FJ actual NH property null receiver", "fj", pc, false);
    mc_player_string_view name;
    if (!mc_player_string_read(literal, &name))
        return fail(e, MC_CLIENT_SCREEN_STRING_FAILED, "FJ translation key String", "fj", pc, true);
    const mc_registry_string *result = NULL;
    if (mc_translation_property(translation, name.units, name.length, &result, &e->registry) !=
        MC_REGISTRY_OK)
        return fail(e, MC_CLIENT_SCREEN_REGISTRY_FAILED, "FJ actual NH property lookup", "fj", pc,
                    true);
    *out = result == NULL ? (mc_client_screen_text){MC_CLIENT_SCREEN_TEXT_PLAYER, literal, NULL}
                          : (mc_client_screen_text){MC_CLIENT_SCREEN_TEXT_REGISTRY, NULL, result};
    return true;
}

typedef struct measurement {
    mc_client_source_fj *owner;
    mc_client_create_world_draw_error *error;
} measurement;

static bool measure(void *context, const mc_font_text *text, int32_t *out) {
    measurement *m = context;
    mc_player_string_view value = {text == NULL ? NULL : text->units,
                                   text == NULL ? 0 : text->length};
    ++m->error->font_measurements;
    return mc_client_bitmap_font_measure_view(m->owner->base.fields.b->font_renderer,
                                              text == NULL ? NULL : &value, out,
                                              &m->error->font) == MC_CLIENT_BITMAP_FONT_OK;
}

static bool text(mc_client_bitmap_font *font, mc_client_screen_text value, int32_t x, int32_t y,
                 uint32_t color, bool centered, const char *owner, unsigned int pc,
                 mc_client_create_world_draw_error *e) {
    mc_player_string_view view;
    const mc_player_string_view *argument = NULL;
    if (value.kind != MC_CLIENT_SCREEN_TEXT_NULL) {
        if (!mc_client_screen_text_read(value, &view))
            return fail(e, MC_CLIENT_SCREEN_STRING_FAILED, "GUI actual String view", owner, pc,
                        true);
        argument = &view;
    }
    if (font == NULL)
        return fail(e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "UB actual Font null receiver",
                    owner, pc, false);
    if (centered) {
        int32_t width;
        ++e->font_measurements;
        if (mc_client_bitmap_font_measure_view(font, argument, &width, &e->font) !=
            MC_CLIENT_BITMAP_FONT_OK)
            return fail(e, MC_CLIENT_SCREEN_FONT_FAILED, e->font.operation, owner, pc,
                        e->font.native_policy);
        x = mc_java_i32_sub(x, width / 2);
    }
    if (mc_client_bitmap_font_draw_shadow_view(font, argument, x, y, color, &e->font) !=
        MC_CLIENT_BITMAP_FONT_OK)
        return fail(e, MC_CLIENT_SCREEN_FONT_FAILED, e->font.operation, owner, pc,
                    e->font.native_policy);
    return true;
}

static bool joined(mc_client_screen_text first, const uint16_t *separator, size_t separator_length,
                   mc_client_screen_text second, mc_player_string **out,
                   mc_client_create_world_draw_error *e, const char *owner, unsigned int pc) {
    static const uint16_t null_string[] = {'n', 'u', 'l', 'l'};
    mc_player_string_view a = {null_string, 4}, b = {null_string, 4};
    if ((first.kind != MC_CLIENT_SCREEN_TEXT_NULL && !mc_client_screen_text_read(first, &a)) ||
        (second.kind != MC_CLIENT_SCREEN_TEXT_NULL && !mc_client_screen_text_read(second, &b)))
        return fail(e, MC_CLIENT_SCREEN_STRING_FAILED, "GUI StringBuilder actual append String",
                    owner, pc, true);
    if (a.length > 1048576 || b.length > 1048576 || separator_length > 1048576)
        return fail(e, MC_CLIENT_SCREEN_LIMIT, "GUI native concatenation extent", owner, pc, true);
    const size_t length = a.length + separator_length + b.length;
    uint16_t *units = length == 0 ? NULL : malloc(length * sizeof(*units));
    if (length != 0 && units == NULL)
        return fail(e, MC_CLIENT_SCREEN_ALLOCATION_FAILED, "GUI StringBuilder native backing",
                    owner, pc, true);
    if (a.length != 0)
        memcpy(units, a.units, a.length * sizeof(*units));
    if (separator_length != 0)
        memcpy(units + a.length, separator, separator_length * sizeof(*units));
    if (b.length != 0)
        memcpy(units + a.length + separator_length, b.units, b.length * sizeof(*units));
    e->screen.string = mc_player_string_create(units, length, out);
    free(units);
    return e->screen.string == MC_PLAYER_OK ||
           fail(e, MC_CLIENT_SCREEN_STRING_FAILED, "GUI StringBuilder canonical result", owner, pc,
                true);
}

static bool rectangle(mc_client_source_fj *o, mc_gui_main_menu *renderer, int32_t x, int32_t y,
                      int32_t right, int32_t bottom, uint32_t argb, unsigned int pc,
                      mc_client_create_world_draw_error *e) {
    if (x < right) {
        const int32_t v = x;
        x = right;
        right = v;
    }
    if (y < bottom) {
        const int32_t v = y;
        y = bottom;
        bottom = v;
    }
    mc_client_display *display = o->base.fields.b->display;
    if (!mc_client_display_world_list_blend(display, true, &e->display) ||
        !mc_client_display_source_texture_enable(display, false, &e->display) ||
        !mc_client_display_world_list_blend_function(display, &e->display))
        return fail(e, MC_CLIENT_SCREEN_TEXTURE_FAILED, e->display.operation, "ro", pc, true);
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return fail(e, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY, "RO rectangle strict FP profile",
                    "ro", pc, true);
    if (!mc_client_display_source_color(
            display, (float)((argb >> 16) & 255u) / 255.0f, (float)((argb >> 8) & 255u) / 255.0f,
            (float)(argb & 255u) / 255.0f, (float)(argb >> 24) / 255.0f, &e->display))
        return fail(e, MC_CLIENT_SCREEN_TEXTURE_FAILED, e->display.operation, "ro", pc, true);
    const mc_client_vertex quad[] = {{x, bottom, 0, 0, argb},
                                     {right, bottom, 0, 0, argb},
                                     {right, y, 0, 0, argb},
                                     {x, y, 0, 0, argb}};
    if (!mc_gui_canvas_quad(renderer, false, quad, true, false, &e->renderer))
        return fail(e, MC_CLIENT_SCREEN_TEXTURE_FAILED, e->renderer.operation, "ro", pc, true);
    ++e->rectangles_drawn;
    if (!mc_client_display_source_texture_enable(display, true, &e->display) ||
        !mc_client_display_world_list_blend(display, false, &e->display))
        return fail(e, MC_CLIENT_SCREEN_TEXTURE_FAILED, e->display.operation, "ro", pc, true);
    return true;
}

static bool field(mc_client_source_fj *o, mc_client_source_ro *ro, mc_gui_main_menu *renderer,
                  size_t index, unsigned int caller_pc, mc_client_create_world_draw_error *e) {
    if (ro == NULL)
        return fail(e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "FJ actual RO.c null receiver", "fj",
                    caller_pc, false);
    e->screen.parent_site = (mc_client_screen_site){"fj", "a", "(IIF)V", caller_pc};
    const mc_client_text_field_fields *f = &ro->fields;
    const int32_t right = mc_java_i32_add(f->d, f->f), bottom = mc_java_i32_add(f->e, f->g);
    if (!rectangle(o, renderer, mc_java_i32_sub(f->d, 1), mc_java_i32_sub(f->e, 1),
                   mc_java_i32_add(right, 1), mc_java_i32_add(bottom, 1), 0xffa0a0a0u, 37, e) ||
        !rectangle(o, renderer, f->d, f->e, right, bottom, 0xff000000u, 69, e))
        return false;
    mc_client_bitmap_font *font = f->c;
    mc_client_screen_text value = f->h;
    if (f->b) {
        const bool cursor = f->a && (f->j / 6) % 2 == 0;
        static const uint16_t underscore = '_';
        const mc_client_screen_text empty = {MC_CLIENT_SCREEN_TEXT_PLAYER,
                                             mc_player_empty_literal(), NULL};
        if (!joined(value, &underscore, cursor ? 1 : 0, empty, &e->strings[index + 1], e, "ro",
                    137))
            return false;
        value = (mc_client_screen_text){MC_CLIENT_SCREEN_TEXT_PLAYER, e->strings[index + 1], NULL};
    }
    if (!text(font, value, mc_java_i32_add(f->d, 4),
              mc_java_i32_add(f->e, mc_java_i32_sub(f->g, 8) / 2), f->b ? 0xe0e0e0u : 0x707070u,
              false, "ro", f->b ? 162 : 199, e))
        return false;
    ++e->fields_drawn;
    return true;
}

bool mc_client_source_create_world_draw(mc_client_source_fj *o, mc_gui_main_menu *renderer,
                                        const mc_scaled_resolution *projection,
                                        mc_gui_mouse_position mouse, float partial,
                                        mc_client_create_world_draw_error *e) {
    (void)partial;
    if (e == NULL || e->active || e->pending_receiver != NULL || e->strings[0] != NULL ||
        e->strings[1] != NULL || e->strings[2] != NULL)
        return false;
    memset(e, 0, sizeof(*e));
    if (o == NULL || renderer == NULL || projection == NULL)
        return fail(e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "FJ draw native arguments", "fj", 0,
                    true);
    mc_client_source_da *base = &o->base;
    if (!mc_screen_lock(base->classes, &e->screen))
        return false;
    if (base->phase != MC_CLIENT_SCREEN_READY || base->active_calls != 0 ||
        o->last_initialization == NULL || !o->last_initialization->returned ||
        base->fields.b == NULL) {
        mtx_unlock(&base->classes->mutex);
        return fail(e, MC_CLIENT_SCREEN_INVALID_PHASE, "FJ initialized exclusive draw receiver",
                    "fj", 0, true);
    }
    ++base->active_calls;
    e->active = true;
    e->pending_receiver = base;
    mtx_unlock(&base->classes->mutex);
    bool ok = false, canvas = false;
    client_application *app = base->fields.b;
    mc_game_settings_state settings;
    mc_texture_pack_list_state packs;
    if (base->classes->registry != app->registry ||
        !mc_game_settings_read_state(app->settings, &settings) ||
        !mc_client_texture_pack_list_inspect(app->D, &packs) ||
        packs.selected_pack != packs.default_pack ||
        !mc_gui_main_menu_same_source(renderer, app->display, base->classes->resources,
                                      base->classes->registry, settings.values.anaglyph)) {
        fail(e, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY, "FJ native same-resource GUI profile",
             "fj", 0, true);
        goto leave;
    }
    if (mc_registry_classes_initialize_translation_singleton(app->registry, &e->registry) !=
        MC_REGISTRY_OK) {
        fail(e, MC_CLIENT_SCREEN_REGISTRY_FAILED, "FJ actual NH singleton active use", "fj", 0,
             true);
        goto leave;
    }
    mc_registry_translation_view translations;
    if (mc_registry_classes_inspect_translation(app->registry, &translations, &e->registry) !=
        MC_REGISTRY_OK) {
        fail(e, MC_CLIENT_SCREEN_REGISTRY_FAILED, "FJ actual NH singleton field", "fj", 0, true);
        goto leave;
    }
    mc_scaled_resolution scale = *projection;
    scale.width = base->fields.c;
    scale.height = base->fields.d;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        fail(e, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY, "FJ native GUI strict FP profile", "fj",
             9, true);
        goto leave;
    }
    measurement measured = {o, e};
    if (!mc_gui_canvas_begin(renderer, &scale, measure, &measured, &e->renderer)) {
        fail(e, MC_CLIENT_SCREEN_TEXTURE_FAILED, e->renderer.operation, "fj", 9, true);
        goto leave;
    }
    canvas = true;
    const bool dirt = app->active_world == NULL;
    if (dirt && (!mc_client_display_source_lighting_enable(app->display, false, &e->display) ||
                 !mc_client_display_source_fog_enable(app->display, false, &e->display))) {
        fail(e, MC_CLIENT_SCREEN_TEXTURE_FAILED, e->display.operation, "fj", 9, true);
        goto leave;
    }
    if (!dirt && (!mc_client_display_source_texture_enable(app->display, false, &e->display) ||
                  !mc_client_display_world_list_blend(app->display, true, &e->display) ||
                  !mc_client_display_source_alpha_test(app->display, false, &e->display) ||
                  !mc_client_display_world_list_blend_function(app->display, &e->display) ||
                  !mc_client_display_source_shade_model(app->display, 7425, &e->display))) {
        fail(e, MC_CLIENT_SCREEN_TEXTURE_FAILED, e->display.operation, "fj", 9, true);
        goto leave;
    }
    const int32_t w = base->fields.c, h = base->fields.d;
    const uint32_t upper = dirt ? 0xff404040u : 0xc0101010u,
                   lower = dirt ? 0xff404040u : 0xd0101010u;
    const float u = dirt ? (float)w / 32.0f : 0, v = dirt ? (float)h / 32.0f : 0;
    const mc_client_vertex background[] = {
        {0, h, 0, v, lower}, {w, h, u, v, lower}, {w, 0, u, 0, upper}, {0, 0, 0, 0, upper}};
    if (!mc_gui_canvas_quad(renderer, dirt, background, !dirt, false, &e->renderer)) {
        fail(e, MC_CLIENT_SCREEN_TEXTURE_FAILED, e->renderer.operation, "fj", 9, true);
        goto leave;
    }
    if (!dirt && (!mc_client_display_source_shade_model(app->display, 7424, &e->display) ||
                  !mc_client_display_world_list_blend(app->display, false, &e->display) ||
                  !mc_client_display_source_alpha_test(app->display, true, &e->display) ||
                  !mc_client_display_source_texture_enable(app->display, true, &e->display))) {
        fail(e, MC_CLIENT_SCREEN_TEXTURE_FAILED, e->display.operation, "fj", 9, true);
        goto leave;
    }
    const char *keys[] = {"selectWorld.create", "selectWorld.enterName", "selectWorld.resultFolder",
                          "selectWorld.enterSeed", "selectWorld.seedInfo"};
    const unsigned int pcs[] = {41, 69, 122, 150, 179}, lookups[] = {18, 53, 88, 134, 162};
    const int32_t ys[] = {mc_java_i32_add(mc_java_i32_sub(h / 4, 60), 20), 47, 85, 104, 140};
    for (size_t i = 0; i < 5; ++i) {
        mc_client_bitmap_font *font = base->fields.g;
        mc_client_screen_text label;
        if (!translated(o, translations.singleton_a, keys[i], &label, lookups[i], e))
            goto leave;
        if (i == 2) {
            static const uint16_t space = ' ';
            if (!joined(label, &space, 1, o->fields.l, &e->strings[0], e, "fj", 106))
                goto leave;
            label = (mc_client_screen_text){MC_CLIENT_SCREEN_TEXT_PLAYER, e->strings[0], NULL};
        }
        if (!text(font, label, i == 0 ? w / 2 : mc_java_i32_sub(w / 2, 100), ys[i],
                  i == 0 ? 0xffffffu : 0xa0a0a0u, i == 0, "fj", pcs[i], e))
            goto leave;
        ++e->labels_drawn;
    }
    if (!field(o, o->fields.i, renderer, 0, 186, e) || !field(o, o->fields.j, renderer, 1, 193, e))
        goto leave;
    e->screen.parent_site = (mc_client_screen_site){"fj", "a", "(IIF)V", 200};
    const mc_client_screen_list *list = base->fields.e;
    if (list == NULL) {
        fail(e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "FJ inherited DA Button list", "da", 9,
             false);
        goto leave;
    }
    for (size_t i = 0; i < list->size; ++i) {
        const mc_client_source_ke *button = list->entries[i];
        if (button == NULL) {
            fail(e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "FJ inherited KE draw null receiver",
                 "da", 41, false);
            goto leave;
        }
        const mc_client_screen_ke_fields *f = &button->fields;
        mc_player_string_view view;
        mc_font_text label;
        const mc_font_text *argument = NULL;
        if (f->h && f->e.kind != MC_CLIENT_SCREEN_TEXT_NULL) {
            if (!mc_client_screen_text_read(f->e, &view)) {
                fail(e, MC_CLIENT_SCREEN_STRING_FAILED, "FJ Button label String", "da", 41, true);
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
            e->screen.parent_site = (mc_client_screen_site){0};
            fail(e, MC_CLIENT_SCREEN_TEXTURE_FAILED, e->renderer.operation, "fj", 200, true);
            goto leave;
        }
        ++e->buttons_drawn;
    }
    o->last_pc = 203;
    e->screen.site = (mc_client_screen_site){"fj", "a", "(IIF)V", 203};
    e->screen.parent_site = (mc_client_screen_site){0};
    e->source_returned = e->screen.source_returned = ok = true;
leave:
    if (canvas)
        mc_gui_canvas_end(renderer);
    e->active = false;
    mc_client_screen_error cleanup = {0};
    if (!mc_screen_lock(base->classes, &cleanup)) {
        e->monitor_lost = true;
        e->screen = cleanup;
        return false;
    }
    --base->active_calls;
    e->pending_receiver = NULL;
    mtx_unlock(&base->classes->mutex);
    if (ok)
        (void)mc_client_source_create_world_draw_retire(e);
    return ok;
}

mc_client_screen_status
mc_client_source_create_world_draw_retire(mc_client_create_world_draw_error *e) {
    if (e == NULL)
        return MC_CLIENT_SCREEN_INVALID_ARGUMENT;
    if (e->active)
        return MC_CLIENT_SCREEN_BUSY;
    if (e->pending_receiver != NULL) {
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
    }
    for (size_t i = 0; i < 3; ++i) {
        mc_player_string_release(e->strings[i]);
        e->strings[i] = NULL;
    }
    return MC_CLIENT_SCREEN_OK;
}
