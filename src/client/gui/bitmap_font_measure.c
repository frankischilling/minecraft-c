#include "bitmap_font_internal.h"
#include "mc/core/java/java.h"

static mc_client_bitmap_font_status fail(mc_client_bitmap_font_error *e,
                                         mc_client_bitmap_font_status status, const char *operation,
                                         int32_t pc, bool native_policy) {
    return mc_client_bitmap_font_fail(e, status, operation, "a", "(Ljava/lang/String;)I", pc,
                                      native_policy);
}

static void measure_width(mc_client_bitmap_font *font, mc_player_string_view input, int32_t *out,
                          mc_client_bitmap_font_error *e) {
    int32_t measured = 0;
    for (size_t index = 0; index < input.length; ++index) {
        if (input.units[index] == UINT16_C(0x00a7)) {
            ++index; /* Actual iinc29, including a trailing section sign. */
            continue;
        }
        const mc_player_string *mapping;
        if (mc_client_font_mapping_active_use(font->gui->mapping, &mapping, &e->mapping) !=
            MC_CLIENT_FONT_MAPPING_OK) {
            fail(e, MC_CLIENT_BITMAP_FONT_MAPPING_FAILED, "sj reached fp active use", 35,
                 e->mapping.status != MC_CLIENT_FONT_MAPPING_OK);
            break;
        }
        mc_player_string_view joined;
        if (!mc_player_string_read(mapping, &joined)) {
            fail(e, MC_CLIENT_BITMAP_FONT_NULL_REFERENCE, "sj actual fp.a String receiver", 43,
                 false);
            break;
        }
        size_t first = 0;
        while (first < joined.length && joined.units[first] != input.units[index])
            ++first;
        if (first == joined.length)
            continue;
        if (font->fields.b == NULL) {
            fail(e, MC_CLIENT_BITMAP_FONT_NULL_REFERENCE, "sj actual width-array receiver", 63,
                 false);
            break;
        }
        const int32_t glyph = mc_java_i32_add((int32_t)first, 32);
        if (glyph < 0 || glyph >= 256) {
            e->actual_index = glyph;
            fail(e, MC_CLIENT_BITMAP_FONT_ARRAY_INDEX, "sj actual width-array index", 63, false);
            break;
        }
        measured = mc_java_i32_add(measured, font->fields.b[glyph]);
    }
    if (e->status == MC_CLIENT_BITMAP_FONT_OK)
        *out = measured;
}

mc_client_bitmap_font_status mc_client_bitmap_font_measure(mc_client_bitmap_font *font,
                                                           const mc_player_string *text,
                                                           int32_t *out,
                                                           mc_client_bitmap_font_error *error) {
    mc_client_bitmap_font_status status =
        mc_client_bitmap_font_admit(font, "a", "(Ljava/lang/String;)I", error);
    if (status != MC_CLIENT_BITMAP_FONT_OK)
        return status;
    if (out == NULL) {
        if (error != NULL)
            fail(error, MC_CLIENT_BITMAP_FONT_INVALID_ARGUMENT, "sj native measure output", -1,
                 true);
        return MC_CLIENT_BITMAP_FONT_INVALID_ARGUMENT;
    }
    if (text == NULL) {
        *out = 0; /* Actual return5: no diagnostics allocation, width read or fp. */
        return MC_CLIENT_BITMAP_FONT_OK;
    }
    mc_player_string_view input;
    mc_client_bitmap_font_error *e;
    status = mc_client_bitmap_font_call_enter(font, text, "a", "(Ljava/lang/String;)I", &input, &e,
                                              error);
    if (status != MC_CLIENT_BITMAP_FONT_OK)
        return status;
    measure_width(font, input, out, e);
    return mc_client_bitmap_font_call_leave(font, text, e, error);
}

mc_client_bitmap_font_status
mc_client_bitmap_font_measure_view(mc_client_bitmap_font *font, const mc_player_string_view *text,
                                   int32_t *out, mc_client_bitmap_font_error *error) {
    mc_client_bitmap_font_status status =
        mc_client_bitmap_font_admit(font, "a", "(Ljava/lang/String;)I", error);
    if (status != MC_CLIENT_BITMAP_FONT_OK)
        return status;
    if (out == NULL) {
        if (error != NULL)
            fail(error, MC_CLIENT_BITMAP_FONT_INVALID_ARGUMENT, "sj native measure output", -1,
                 true);
        return MC_CLIENT_BITMAP_FONT_INVALID_ARGUMENT;
    }
    if (text == NULL) {
        *out = 0;
        return MC_CLIENT_BITMAP_FONT_OK;
    }
    mc_client_bitmap_font_error *e;
    status = mc_client_bitmap_font_view_enter(font, text, "a", "(Ljava/lang/String;)I", &e, error);
    if (status != MC_CLIENT_BITMAP_FONT_OK)
        return status;
    measure_width(font, *text, out, e);
    return mc_client_bitmap_font_view_leave(font, e, error);
}
