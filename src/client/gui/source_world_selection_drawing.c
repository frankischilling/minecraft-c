#include "source_world_selection_drawing.h"
#include "../application_state.h"
#include "../display/source_surface.h"
#include "../display/source_world_list_surface.h"
#include "../display/startup_surface.h"
#include "../saves/internal.h"
#include "mc/client/texture_packs/texture_packs.h"
#include "mc/core/java/float_format.h"
#include "mc/core/java/java.h"
#include "source_screen_transitions.h"
#include "source_world_selection_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

static bool fail(mc_client_world_list_error *e, mc_client_screen_status status,
                 const char *operation, const char *owner, const char *method,
                 const char *descriptor, unsigned int pc) {
    mc_screen_fail(&e->screen, status, operation, owner, method, descriptor, pc);
    e->screen.native_policy = status != MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE &&
                              status != MC_CLIENT_SCREEN_SOURCE_ARITHMETIC;
    return false;
}

static bool lg_fail(mc_client_world_list_error *e, mc_client_screen_status status,
                    const char *operation, unsigned int pc) {
    return fail(e, status, operation, "lg", "a", "(IIF)V", pc);
}

static bool fp(mc_client_world_list_error *e, unsigned int pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           lg_fail(e, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
                   "world list strict binary32 control state", pc);
}

static float subf(float a, float b) {
    volatile float v = a - b;
    return v;
}

static float mulf(float a, float b) {
    volatile float v = a * b;
    return v;
}

static float divf(float a, float b) {
    volatile float v = a / b;
    return v;
}

static bool select_row(mc_client_source_world_list *list, int32_t index, bool twice,
                       mc_client_screen_transitions *router, mc_client_world_list_error *e) {
    mc_client_source_rq *rq = list->a;
    if (!mc_screen_lock(rq->base.classes, &e->screen))
        return false;
    rq->fields.m = index; /* Actual synthetic rq.a store through mg5. */
    int32_t n;
    bool ok = mc_screen_world_list_count(list, &n, e);
    const bool enabled = ok && index >= 0 && index < n;
    mc_client_source_ke *buttons[] = {rq->fields.t, rq->fields.s, rq->fields.u};
    const unsigned int pcs[] = {47, 58, 69};
    for (size_t i = 0; ok && i < 3; ++i) {
        if (buttons[i] == NULL)
            ok = fail(e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "MG actual selection button store",
                      "mg", "a", "(IZ)V", pcs[i]);
        else
            buttons[i]->fields.g = enabled;
    }
    mtx_unlock(&rq->base.classes->mutex);
    if (!ok)
        return false;
    ++e->selections;
    if (twice && enabled)
        return mc_client_screen_transitions_select(router, rq, index, &e->screen) ==
               MC_CLIENT_SCREEN_OK;
    return true;
}

static bool input(mc_client_source_world_list *list, int32_t n, mc_gui_mouse_position mouse,
                  mc_client_sdk_input *sdk, mc_gui_main_menu_millis_fn clock, void *context,
                  mc_client_screen_transitions *router, mc_client_world_list_error *e) {
    bool down;
    if (mc_client_sdk_mouse_button_down(sdk, 0, &down, &e->input) != MC_CLIENT_SDK_INPUT_OK)
        return lg_fail(e, MC_CLIENT_SCREEN_INPUT_FAILED, "LG actual Mouse.isButtonDown", 29);
    if (!fp(e, 35))
        return false;
    mc_client_world_list_fields *f = &list->fields;
    const int32_t scrollbar = mc_java_i32_add(f->b / 2, 124);
    if (down) {
        if (f->k == -1.0f) {
            bool drag = true;
            if (mouse.y >= f->h && mouse.y <= f->i) {
                const int32_t left = mc_java_i32_sub(f->b / 2, 110),
                              right = mc_java_i32_add(f->b / 2, 110);
                const int32_t offset = mc_java_i32_sub(
                    mc_java_i32_add(mc_java_i32_sub(mc_java_i32_sub(mouse.y, f->h), f->r),
                                    mc_java_float_to_i32(f->m)),
                    4);
                int32_t index;
                if (!mc_java_i32_div(offset, f->j, &index))
                    return lg_fail(e, MC_CLIENT_SCREEN_SOURCE_ARITHMETIC, "LG actual row division",
                                   113);
                if (mouse.x >= left && mouse.x <= right && index >= 0 && offset >= 0 && index < n) {
                    bool twice = false;
                    int64_t now;
                    if (index == f->n) {
                        ++e->clock_samples;
                        if (!clock(context, &now))
                            return lg_fail(e, MC_CLIENT_SCREEN_APP_FAILED,
                                           "LG double-click currentTimeMillis", 154);
                        twice = mc_java_i64_sub(now, f->o) < 250;
                    }
                    if (!select_row(list, index, twice, router, e))
                        return false;
                    f->n = index;
                    ++e->clock_samples;
                    if (!clock(context, &now))
                        return lg_fail(e, MC_CLIENT_SCREEN_APP_FAILED,
                                       "LG last-click currentTimeMillis", 191);
                    f->o = now;
                } else if (mouse.x >= left && mouse.x <= right && offset < 0) {
                    /* Actual inherited lg.a(II) is a literal return. */
                    drag = false;
                }
                if (!fp(e, 257))
                    return false;
                if (mouse.x >= scrollbar && mouse.x <= mc_java_i32_add(scrollbar, 6)) {
                    f->l = -1;
                    int32_t extent = mc_screen_world_list_maximum(list, n);
                    if (extent < 1)
                        extent = 1;
                    const int32_t height = mc_java_i32_sub(f->i, f->h);
                    int32_t handle = mc_java_float_to_i32(divf(
                        (float)mc_java_i32_mul(height, height), (float)mc_java_i32_mul(n, 36)));
                    if (handle < 32)
                        handle = 32;
                    if (handle > mc_java_i32_sub(height, 8))
                        handle = mc_java_i32_sub(height, 8);
                    f->l = divf(f->l, divf((float)mc_java_i32_sub(height, handle), (float)extent));
                } else
                    f->l = 1;
                f->k = drag ? (float)mouse.y : -2.0f;
            } else
                f->k = -2;
        } else if (f->k >= 0) {
            f->m = subf(f->m, mulf(subf((float)mouse.y, f->k), f->l));
            f->k = (float)mouse.y;
        }
    } else
        f->k = -1;
    if (!fp(e, 470))
        return false;
    mc_screen_world_list_clamp(list, n);
    return true;
}

static bool rectangle(mc_gui_main_menu *renderer, bool dirt, int32_t left, int32_t top,
                      int32_t right, int32_t bottom, int32_t scroll, uint32_t upper, uint32_t lower,
                      bool blend, bool alpha, mc_client_world_list_error *e, unsigned int pc) {
    if (!fp(e, pc))
        return false;
    const float u0 = (float)left / 32.0f, u1 = (float)right / 32.0f;
    const float v0 = (float)mc_java_i32_add(top, scroll) / 32.0f;
    const float v1 = (float)mc_java_i32_add(bottom, scroll) / 32.0f;
    const mc_client_vertex vertices[4] = {{left, bottom, u0, v1, lower},
                                          {right, bottom, u1, v1, lower},
                                          {right, top, u1, v0, upper},
                                          {left, top, u0, v0, upper}};
    if (!mc_gui_canvas_quad(renderer, dirt, vertices, blend, alpha, &e->renderer))
        return lg_fail(e, MC_CLIENT_SCREEN_TEXTURE_FAILED, e->renderer.operation, pc);
    return true;
}

typedef struct row_text {
    uint16_t *units;
    size_t size, capacity, maximum;
} row_text;

static bool append(row_text *text, const uint16_t *units, size_t length,
                   mc_client_world_list_error *e, unsigned int pc) {
    if (length > text->maximum || text->size > text->maximum - length)
        return fail(e, MC_CLIENT_SCREEN_LIMIT, "MG native row text bound", "mg", "a", "(IIIILnw;)V",
                    pc);
    const size_t needed = text->size + length;
    if (needed > text->capacity) {
        uint16_t *next = realloc(text->units, needed * sizeof(*next));
        if (next == NULL)
            return fail(e, MC_CLIENT_SCREEN_ALLOCATION_FAILED, "MG actual row text storage", "mg",
                        "a", "(IIIILnw;)V", pc);
        text->units = next;
        text->capacity = needed;
    }
    if (length != 0)
        memcpy(text->units + text->size, units, length * sizeof(*units));
    text->size = needed;
    return true;
}

static bool ascii(row_text *text, const char *value, mc_client_world_list_error *e,
                  unsigned int pc) {
    for (const unsigned char *p = (const unsigned char *)value; *p != 0; ++p) {
        const uint16_t unit = *p;
        if (!append(text, &unit, 1, e, pc))
            return false;
    }
    return true;
}

static bool label(row_text *text, mc_client_screen_text source, mc_client_world_list_error *e,
                  unsigned int pc) {
    if (source.kind == MC_CLIENT_SCREEN_TEXT_NULL)
        return ascii(text, "null", e, pc);
    mc_player_string_view view;
    if (!mc_client_screen_text_read(source, &view))
        return fail(e, MC_CLIENT_SCREEN_STRING_FAILED, "MG canonical label view", "mg", "a",
                    "(IIIILnw;)V", pc);
    return append(text, view.units, view.length, e, pc);
}

static bool math_empty(mc_client_source_world_list *list, mc_client_world_list_error *e) {
    client_application *app = list->fields.a;
    if (app == NULL || app->aa == NULL || app->aa->classes->math == NULL)
        return fail(e, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
                    "MG same canonical MathHelper namespace", "mg", "a", "(IIIILnw;)V", 32);
    if (mc_math_classes_ensure(app->aa->classes->math, &e->save.math) != MC_MATH_CLASS_OK)
        return fail(e, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY, e->save.math.operation, "mg", "a",
                    "(IIIILnw;)V", 32);
    return true;
}

static bool row(mc_client_source_world_list *list, mc_gui_main_menu *renderer, int32_t index,
                int32_t x, int32_t y, mc_client_world_list_error *e) {
    mc_client_save_selection *selection = mc_client_save_list_at(list->a->fields.n, (size_t)index);
    mc_client_save_selection_fields fields;
    if (mc_client_save_selection_read(selection, &fields, &e->save) != MC_CLIENT_SAVE_OK)
        return fail(e, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY, "MG canonical vb row getter",
                    "mg", "a", "(IIIILnw;)V", 20);
    /* Bounded native StringBuilder storage; source local Strings have no
     * identity comparison or escaping occurrence in this drawing method. */
    const size_t maximum_units = 1024u * 1024u / sizeof(uint16_t);
    row_text name = {.maximum = maximum_units}, description = {.maximum = maximum_units},
             conversion = {.maximum = maximum_units};
    bool ok = false;
    const mc_nbt_string display = mc_world_info_name_text(fields.display_name);
    if (fields.display_name != NULL && !math_empty(list, e))
        goto leave;
    if (fields.display_name == NULL || display.length == 0) {
        char number[16];
        const int result = snprintf(number, sizeof(number), "%d", mc_java_i32_add(index, 1));
        if (result < 0 || (size_t)result >= sizeof(number)) {
            fail(e, MC_CLIENT_SCREEN_STRING_FAILED, "MG row index decimal", "mg", "a",
                 "(IIIILnw;)V", 63);
            goto leave;
        }
        if (!label(&name, list->a->fields.p, e, 52) || !ascii(&name, " ", e, 57) ||
            !ascii(&name, number, e, 63))
            goto leave;
    } else if (!append(&name, display.units, display.length, e, 20))
        goto leave;
    if (fields.filename == NULL) {
        if (!ascii(&description, "null", e, 87))
            goto leave;
    } else {
        const mc_nbt_string filename = mc_world_info_name_text(fields.filename);
        if (!append(&description, filename.units, filename.length, e, 87))
            goto leave;
    }
    if (!ascii(&description, " (", e, 92))
        goto leave;
    mc_platform_date_format_text date;
    if (list->a->fields.j == NULL) {
        fail(e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "MG actual DateFormat.format receiver",
             "mg", "a", "(IIIILnw;)V", 114);
        goto leave;
    }
    if (mc_platform_date_format_format(list->a->fields.j, fields.last_played, &date, &e->date) !=
        MC_PLATFORM_DATE_FORMAT_OK) {
        fail(e, MC_CLIENT_SCREEN_CALENDAR_FAILED, "MG actual DateFormat.format", "mg", "a",
             "(IIIILnw;)V", 114);
        goto leave;
    }
    if (!append(&description, date.units, date.length, e, 117) ||
        !ascii(&description, ", ", e, 146))
        goto leave;
    int64_t size = fields.size_on_disk / 1024;
    size = mc_java_i64_mul(size, 100) / 1024;
    if (!fp(e, 163))
        goto leave;
    const float megabytes = divf((float)size, 100.0f);
    mc_java_float_string decimal;
    if (mc_java_float_to_string(megabytes, &decimal) != MC_JAVA_FLOAT_FORMAT_OK) {
        fail(e, MC_CLIENT_SCREEN_STRING_FAILED, "MG actual append(float)", "mg", "a", "(IIIILnw;)V",
             167);
        goto leave;
    }
    if (!ascii(&description, decimal.text, e, 167) || !ascii(&description, " MB)", e, 172))
        goto leave;
    if (fields.requires_conversion &&
        (!label(&conversion, list->a->fields.q, e, 206) || !ascii(&conversion, " ", e, 211)))
        goto leave;
    const mc_font_text texts[3] = {{name.units, name.size},
                                   {description.units, description.size},
                                   {conversion.units, conversion.size}};
    const int32_t offsets[3] = {1, 12, 22};
    const unsigned int pcs[3] = {245, 270, 298};
    for (size_t i = 0; i < 3; ++i)
        if (!mc_gui_canvas_text(renderer, &texts[i], mc_java_i32_add(x, 2),
                                mc_java_i32_add(y, offsets[i]), i == 0 ? 0xffffffu : 0x808080u,
                                false, &e->renderer)) {
            fail(e, MC_CLIENT_SCREEN_FONT_FAILED, e->renderer.operation, "mg", "a", "(IIIILnw;)V",
                 pcs[i]);
            goto leave;
        }
    ++e->rows_drawn;
    ok = true;
leave:
    free(name.units);
    free(description.units);
    free(conversion.units);
    return ok;
}

static bool device(bool ok, mc_client_world_list_error *e, unsigned int pc) {
    return ok || lg_fail(e, MC_CLIENT_SCREEN_TEXTURE_FAILED, e->display.operation, pc);
}

bool mc_client_world_list_draw(mc_client_source_world_list *list, mc_gui_main_menu *renderer,
                               mc_gui_mouse_position mouse, float partial_tick,
                               mc_client_sdk_input *sdk, mc_gui_main_menu_millis_fn clock,
                               void *context, mc_client_screen_transitions *router,
                               mc_client_world_list_error *e) {
    (void)partial_tick; /* No source LG instruction reads its float argument. */
    if (e == NULL)
        return false;
    memset(e, 0, sizeof(*e));
    if (list == NULL || renderer == NULL || sdk == NULL || clock == NULL || list->a == NULL ||
        list->fields.a == NULL || list->phase != MC_CLIENT_SCREEN_READY || list->retired ||
        list->active)
        return lg_fail(e, MC_CLIENT_SCREEN_INVALID_PHASE, "MG actual drawing owner", 0);
    list->active = true;
    bool ok = false;
    mc_client_world_list_fields *f = &list->fields;
    mc_client_display *display = f->a->display;
    /* mg.c -> rq.i -> da.a(0), selected no-World background body. */
    if (f->a->active_world != NULL) {
        fail(e, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
             "DA World background requires its renderer", "da", "a", "(I)V", 8);
        goto leave;
    }
    if (!mc_client_display_source_disable_unlit_feature(display, MC_CLIENT_SOURCE_LIGHTING,
                                                        &e->display)) {
        fail(e, MC_CLIENT_SCREEN_TEXTURE_FAILED, e->display.operation, "da", "b", "(I)V", 3);
        goto leave;
    }
    if (!mc_client_display_source_disable_unlit_feature(display, MC_CLIENT_SOURCE_FOG,
                                                        &e->display)) {
        fail(e, MC_CLIENT_SCREEN_TEXTURE_FAILED, e->display.operation, "da", "b", "(I)V", 9);
        goto leave;
    }
    if (!rectangle(renderer, true, 0, 0, f->b, f->c, 0, 0xff404040u, 0xff404040u, false, true, e,
                   1))
        goto leave;
    int32_t n;
    if (!mc_screen_world_list_count(list, &n, e) ||
        !input(list, n, mouse, sdk, clock, context, router, e))
        goto leave;
    if (!device(mc_client_display_source_disable_unlit_feature(display, MC_CLIENT_SOURCE_LIGHTING,
                                                               &e->display),
                e, 476) ||
        !device(mc_client_display_source_disable_unlit_feature(display, MC_CLIENT_SOURCE_FOG,
                                                               &e->display),
                e, 482))
        goto leave;
    if (!rectangle(renderer, true, f->e, f->h, f->d, f->i, mc_java_float_to_i32(f->m), 0xff202020u,
                   0xff202020u, false, true, e, 693))
        goto leave;
    const int32_t x = mc_java_i32_sub(mc_java_i32_sub(f->b / 2, 92), 16);
    const int32_t origin = mc_java_i32_sub(mc_java_i32_add(f->h, 4), mc_java_float_to_i32(f->m));
    for (int32_t at = 0; at < n; ++at) {
        const int32_t y = mc_java_i32_add(mc_java_i32_add(origin, mc_java_i32_mul(at, f->j)), f->r);
        const int32_t height = mc_java_i32_sub(f->j, 4);
        if (y > f->i || mc_java_i32_add(y, height) < f->h)
            continue;
        if (f->p && at == list->a->fields.m) {
            const int32_t left = mc_java_i32_sub(f->b / 2, 110),
                          right = mc_java_i32_add(f->b / 2, 110);
            if (!rectangle(renderer, false, left, mc_java_i32_sub(y, 2), right,
                           mc_java_i32_add(mc_java_i32_add(y, height), 2), 0, 0xff808080u,
                           0xff808080u, false, true, e, 1019) ||
                !rectangle(renderer, false, mc_java_i32_add(left, 1), mc_java_i32_sub(y, 1),
                           mc_java_i32_sub(right, 1),
                           mc_java_i32_add(mc_java_i32_add(y, height), 1), 0, 0xff000000u,
                           0xff000000u, false, true, e, 1019))
                goto leave;
        }
        if (!row(list, renderer, at, x, y, e))
            goto leave;
    }
    if (!device(mc_client_display_source_depth_test(display, false, &e->display), e, 1051))
        goto leave;
    if (!rectangle(renderer, true, 0, 0, f->b, f->h, 0, 0xff404040u, 0xff404040u, false, true, e,
                   1069) ||
        !rectangle(renderer, true, 0, f->i, f->b, f->c, 0, 0xff404040u, 0xff404040u, false, true, e,
                   1087))
        goto leave;
    if (!device(mc_client_display_world_list_blend(display, true, &e->display), e, 1093) ||
        !device(mc_client_display_world_list_blend_function(display, &e->display), e, 1102) ||
        !device(mc_client_display_source_alpha_test(display, false, &e->display), e, 1108) ||
        !device(mc_client_display_source_shade_model(display, 7425, &e->display), e, 1114) ||
        !device(mc_client_display_source_texture_enable(display, false, &e->display), e, 1120))
        goto leave;
    if (!rectangle(renderer, false, f->e, f->h, f->d, mc_java_i32_add(f->h, 4), 0, 0xff000000u, 0,
                   true, false, e, 1224) ||
        !rectangle(renderer, false, f->e, mc_java_i32_sub(f->i, 4), f->d, f->i, 0, 0, 0xff000000u,
                   true, false, e, 1327))
        goto leave;
    const int32_t extent = mc_screen_world_list_maximum(list, n);
    if (extent > 0) {
        const int32_t height = mc_java_i32_sub(f->i, f->h);
        int32_t handle;
        if (!mc_java_i32_div(mc_java_i32_mul(height, height), mc_java_i32_mul(n, 36), &handle)) {
            lg_fail(e, MC_CLIENT_SCREEN_SOURCE_ARITHMETIC, "LG actual scrollbar height division",
                    1377);
            goto leave;
        }
        if (handle < 32)
            handle = 32;
        if (handle > mc_java_i32_sub(height, 8))
            handle = mc_java_i32_sub(height, 8);
        int32_t y;
        if (!mc_java_i32_div(
                mc_java_i32_mul(mc_java_float_to_i32(f->m), mc_java_i32_sub(height, handle)),
                extent, &y)) {
            lg_fail(e, MC_CLIENT_SCREEN_SOURCE_ARITHMETIC, "LG actual scrollbar position division",
                    1442);
            goto leave;
        }
        y = mc_java_i32_add(y, f->h);
        if (y < f->h)
            y = f->h;
        const int32_t left = mc_java_i32_add(f->b / 2, 124), right = mc_java_i32_add(left, 6);
        if (!rectangle(renderer, false, left, f->h, right, f->i, 0, 0xff000000u, 0xff000000u, true,
                       false, e, 1532) ||
            !rectangle(renderer, false, left, y, right, mc_java_i32_add(y, handle), 0, 0xff808080u,
                       0xff808080u, true, false, e, 1624) ||
            !rectangle(renderer, false, left, y, mc_java_i32_sub(right, 1),
                       mc_java_i32_sub(mc_java_i32_add(y, handle), 1), 0, 0xffc0c0c0u, 0xffc0c0c0u,
                       true, false, e, 1717))
            goto leave;
    }
    /* Inherited b(II)1723 is the literal no-op. */
    if (!device(mc_client_display_source_texture_enable(display, true, &e->display), e, 1729) ||
        !device(mc_client_display_world_list_flat(display, &e->display), e, 1735) ||
        !device(mc_client_display_source_alpha_test(display, true, &e->display), e, 1741) ||
        !device(mc_client_display_world_list_blend(display, false, &e->display), e, 1747))
        goto leave;
    list->last_pc = 1750;
    e->source_returned = true;
    ok = true;
leave:
    list->active = false;
    return ok;
}

typedef struct font_measurement {
    mc_client_bitmap_font *font;
    mc_client_world_list_error *error;
} font_measurement;

static bool measure_source(void *context, const mc_font_text *text, int32_t *out) {
    font_measurement *call = context;
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

static bool rq_fail(mc_client_world_selection_draw_error *e, mc_client_screen_status status,
                    const char *operation, unsigned int pc) {
    return fail(&e->list, status, operation, "rq", "a", "(IIF)V", pc);
}

bool mc_client_source_world_selection_draw(mc_client_source_rq *rq, mc_gui_main_menu *renderer,
                                           const mc_scaled_resolution *projection,
                                           mc_gui_mouse_position mouse, float partial_tick,
                                           mc_client_sdk_input *sdk,
                                           mc_gui_main_menu_millis_fn clock, void *context,
                                           mc_client_screen_transitions *router,
                                           mc_client_world_selection_draw_error *e) {
    if (e == NULL)
        return false;
    memset(e, 0, sizeof(*e));
    if (rq == NULL || renderer == NULL || projection == NULL || sdk == NULL || clock == NULL)
        return rq_fail(e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "RQ drawing arguments", 0);
    mc_client_source_da *base = &rq->base;
    mc_client_screen_classes *classes = base->classes;
    if (!mc_screen_lock(classes, &e->list.screen))
        return false;
    if (base->kind != MC_CLIENT_SCREEN_WORLD_SELECTION || base->phase != MC_CLIENT_SCREEN_READY ||
        base->active_calls != 0 || rq->last_initialization == NULL ||
        !rq->last_initialization->returned || base->fields.b == NULL || base->fields.g == NULL ||
        base->fields.e == NULL || rq->fields.o == NULL) {
        mtx_unlock(&classes->mutex);
        return rq_fail(e, MC_CLIENT_SCREEN_INVALID_PHASE, "initialized RQ exclusive drawing owner",
                       0);
    }
    ++base->active_calls;
    e->pending_receiver = base;
    mtx_unlock(&classes->mutex);
    bool ok = false, canvas = false;
    client_application *app = base->fields.b;
    mc_game_settings_state settings;
    mc_texture_pack_list_state packs;
    if (app->active_world != NULL || base->fields.g != app->font_renderer ||
        classes->registry != app->registry ||
        !mc_game_settings_read_state(app->settings, &settings) ||
        !mc_client_texture_pack_list_inspect(app->D, &packs) ||
        packs.phase != MC_TEXTURE_PACK_READY || packs.default_pack == NULL ||
        packs.selected_pack != packs.default_pack ||
        !mc_gui_main_menu_same_source(renderer, app->display, classes->resources, classes->registry,
                                      settings.values.anaglyph)) {
        rq_fail(e, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
                "RQ same no-World App/Font/default-Pack/resource drawing profile", 0);
        goto leave;
    }
    mc_scaled_resolution scale = *projection;
    scale.width = base->fields.c;
    scale.height = base->fields.d;
    font_measurement font = {base->fields.g, &e->list};
    if (!mc_gui_canvas_begin(renderer, &scale, measure_source, &font, &e->list.renderer)) {
        rq_fail(e, MC_CLIENT_SCREEN_TEXTURE_FAILED, e->list.renderer.operation, 7);
        goto leave;
    }
    canvas = true;
    if (!mc_client_world_list_draw(rq->fields.o, renderer, mouse, partial_tick, sdk, clock, context,
                                   router, &e->list))
        goto leave;
    mc_player_string_view title_view;
    mc_font_text title;
    const mc_font_text *title_argument = NULL;
    if (rq->fields.i.kind != MC_CLIENT_SCREEN_TEXT_NULL) {
        if (!mc_client_screen_text_read(rq->fields.i, &title_view)) {
            rq_fail(e, MC_CLIENT_SCREEN_STRING_FAILED, "RQ canonical title String", 29);
            goto leave;
        }
        title = (mc_font_text){title_view.units, title_view.length};
        title_argument = &title;
    }
    if (!mc_gui_canvas_text(renderer, title_argument, base->fields.c / 2, 20, 0xffffffu, true,
                            &e->list.renderer)) {
        rq_fail(e, MC_CLIENT_SCREEN_FONT_FAILED, e->list.renderer.operation, 29);
        goto leave;
    }
    const mc_client_screen_list *buttons = base->fields.e;
    if (!buttons->native_live || buttons->kind != MC_SCREEN_LIST_BUTTONS) {
        rq_fail(e, MC_CLIENT_SCREEN_INVALID_PHASE, "RQ actual DA button List", 36);
        goto leave;
    }
    for (size_t at = 0; at < buttons->size; ++at) {
        mc_client_source_ke *button = buttons->entries[at];
        if (button == NULL || button->phase != MC_CLIENT_SCREEN_READY) {
            fail(&e->list,
                 button == NULL ? MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE
                                : MC_CLIENT_SCREEN_INVALID_PHASE,
                 "DA actual ordinary KE draw receiver", "da", "a", "(IIF)V", 29);
            goto leave;
        }
        const mc_client_screen_ke_fields *value = &button->fields;
        mc_player_string_view label_view;
        mc_font_text label_text;
        const mc_font_text *label_argument = NULL;
        if (value->h && value->e.kind != MC_CLIENT_SCREEN_TEXT_NULL) {
            if (!mc_client_screen_text_read(value->e, &label_view)) {
                rq_fail(e, MC_CLIENT_SCREEN_STRING_FAILED, "RQ canonical button String", 36);
                goto leave;
            }
            label_text = (mc_font_text){label_view.units, label_view.length};
            label_argument = &label_text;
        }
        const mc_gui_button draw_button = {.id = value->f,
                                           .x = value->c,
                                           .y = value->d,
                                           .width = value->a,
                                           .height = value->b,
                                           .label = label_argument,
                                           .enabled = value->g,
                                           .visible = value->h,
                                           .z_level = value->k};
        if (!mc_gui_canvas_button(renderer, &draw_button, mouse, &e->list.renderer)) {
            rq_fail(e, MC_CLIENT_SCREEN_TEXTURE_FAILED, e->list.renderer.operation, 36);
            goto leave;
        }
        ++e->buttons_drawn;
    }
    e->source_returned = true;
    e->list.screen.source_returned = true;
    ok = true;
leave:
    if (canvas)
        mc_gui_canvas_end(renderer);
    mc_client_screen_error cleanup = {0};
    if (!mc_screen_lock(classes, &cleanup)) {
        e->monitor_lost = true;
        e->list.screen = cleanup;
        return false;
    }
    --base->active_calls;
    e->pending_receiver = NULL;
    mtx_unlock(&classes->mutex);
    return ok;
}

mc_client_screen_status
mc_client_source_world_selection_draw_retire(mc_client_world_selection_draw_error *e) {
    if (e == NULL)
        return MC_CLIENT_SCREEN_INVALID_ARGUMENT;
    mc_client_source_da *base = e->pending_receiver;
    if (base == NULL)
        return MC_CLIENT_SCREEN_OK;
    if (!e->monitor_lost)
        return MC_CLIENT_SCREEN_BUSY;
    mc_client_screen_error cleanup = {0};
    if (!mc_screen_lock(base->classes, &cleanup))
        return cleanup.status;
    --base->active_calls;
    e->pending_receiver = NULL;
    e->monitor_lost = false;
    mtx_unlock(&base->classes->mutex);
    return MC_CLIENT_SCREEN_OK;
}
