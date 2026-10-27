#include "../display/source_surface.h"
#include "../render_engine/ge.h"
#include "bitmap_font_internal.h"
#include "font_kernels.h"
#include "mc/core/java/java.h"
#include <string.h>
#include <xmmintrin.h>

static const char inner_descriptor[] = "(Ljava/lang/String;IIIZ)V";
static const char wrapper_descriptor[] = "(Ljava/lang/String;III)V";

static mc_client_graphics_site site(int32_t pc) {
    return (mc_client_graphics_site){"sj", "a", inner_descriptor, pc};
}

static bool failed(mc_client_bitmap_font_error *e, mc_client_bitmap_font_status status,
                   const char *operation, int32_t pc, bool native_policy) {
    mc_client_bitmap_font_fail(e, status, operation, "a", inner_descriptor, pc, native_policy);
    return false;
}

static bool graphics_failed(mc_client_bitmap_font_error *e, int32_t pc) {
    return failed(e,
                  e->graphics.status == MC_CLIENT_GRAPHICS_LIMIT ? MC_CLIENT_BITMAP_FONT_LIMIT
                  : e->graphics.status == MC_CLIENT_GRAPHICS_ALLOCATION_FAILED
                      ? MC_CLIENT_BITMAP_FONT_ALLOCATION_FAILED
                  : e->graphics.status == MC_CLIENT_GRAPHICS_NULL_REFERENCE
                      ? MC_CLIENT_BITMAP_FONT_NULL_REFERENCE
                  : e->graphics.status == MC_CLIENT_GRAPHICS_UNIMPLEMENTED_DEPENDENCY
                      ? MC_CLIENT_BITMAP_FONT_UNIMPLEMENTED_DEPENDENCY
                  : e->graphics.status == MC_CLIENT_GRAPHICS_UNSUPPORTED_FP
                      ? MC_CLIENT_BITMAP_FONT_UNSUPPORTED_FP
                      : MC_CLIENT_BITMAP_FONT_RENDER_FAILED,
                  "sj actual NIO/list/generation child", pc, e->graphics.native_policy);
}

static bool device_failed(mc_client_bitmap_font_error *e, int32_t pc) {
    return failed(e,
                  e->display.status == MC_CLIENT_DISPLAY_UNSUPPORTED_FP
                      ? MC_CLIENT_BITMAP_FONT_UNSUPPORTED_FP
                  : e->display.status == MC_CLIENT_DISPLAY_UNSUPPORTED_GL
                      ? MC_CLIENT_BITMAP_FONT_UNIMPLEMENTED_DEPENDENCY
                      : MC_CLIENT_BITMAP_FONT_DISPLAY_FAILED,
                  "sj actual Display child", pc, true);
}

static mc_client_direct_int_buffer *buffer(mc_client_bitmap_font *font) {
    /* Public inspection is const; the genuine source owner stores this same
     * mutable NIO object, originally allocated by its constructor. */
    return (mc_client_direct_int_buffer *)font->fields.d;
}

static bool flush(mc_client_bitmap_font *font, int32_t flip_pc, int32_t call_pc, int32_t clear_pc,
                  mc_client_bitmap_font_error *e) {
    if (!mc_client_ge_int_flip(buffer(font), site(flip_pc), &e->graphics))
        return graphics_failed(e, flip_pc);
    if (!mc_client_ge_call_lists(font->gui->graphics, font->display, buffer(font), site(call_pc),
                                 &e->graphics))
        return graphics_failed(e, call_pc);
    if (clear_pc >= 0 && !mc_client_ge_int_clear(buffer(font), site(clear_pc), &e->graphics))
        return graphics_failed(e, clear_pc);
    return true;
}

static bool remaining(mc_client_bitmap_font *font, int32_t pc, bool *empty,
                      mc_client_bitmap_font_error *e) {
    if (font->fields.d == NULL)
        return failed(e, MC_CLIENT_BITMAP_FONT_NULL_REFERENCE,
                      "sj actual IntBuffer.remaining receiver", pc, false);
    *empty = mc_client_ge_int_remaining(buffer(font)) == 0;
    return true;
}

static bool format_index(mc_player_string_view input, size_t next, int32_t *out,
                         mc_client_bitmap_font_error *e) {
    /* Actual toLowerCase178 acts on the WHOLE String at every format pair.
     * ASCII plus section signs has stable single-unit behavior for this
     * lookup, including Turkish I (which is outside the hex alphabet).
     * Wider locale/expansion semantics remain a reached dependency. */
    for (size_t i = 0; i < input.length; ++i)
        if (input.units[i] > 127u && input.units[i] != UINT16_C(0x00a7))
            return failed(e, MC_CLIENT_BITMAP_FONT_UNSUPPORTED_LOWERCASE,
                          "sj actual default-locale whole-String lowercase", 178, true);
    uint16_t unit = input.units[next];
    if (unit >= 'A' && unit <= 'Z')
        unit = (uint16_t)(unit + ('a' - 'A'));
    const char hex[] = "0123456789abcdef";
    int32_t index = 0;
    while (index < 16 && (uint16_t)hex[index] != unit)
        ++index;
    *out = index == 16 ? 15 : index;
    return true;
}

static bool draw_body(mc_client_bitmap_font *font, mc_player_string_view input, int32_t x,
                      int32_t y, uint32_t argb, bool shadow, mc_client_bitmap_font_error *e) {
    const uint32_t color = mc_font_source_initial_color(argb, shadow);
    if (!mc_client_font_lease_bind(font->lease, &e->graphics))
        return graphics_failed(e, 40);
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return failed(e, MC_CLIENT_BITMAP_FONT_UNSUPPORTED_FP, "sj reached color FP division", 55,
                      true);
    const float red = (float)((color >> 16u) & 255u) / 255.0f;
    const float green = (float)((color >> 8u) & 255u) / 255.0f;
    const float blue = (float)(color & 255u) / 255.0f;
    const float alpha = (float)(color >> 24u) / 255.0f;
    if (!mc_client_display_source_color(font->display, red, green, blue, alpha, &e->display))
        return device_failed(e, 118);
    if (!mc_client_ge_int_clear(buffer(font), site(125), &e->graphics))
        return graphics_failed(e, 125);
    if (!mc_client_display_source_push(font->display, &e->display))
        return device_failed(e, 129);
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return failed(e, MC_CLIENT_BITMAP_FONT_UNSUPPORTED_FP,
                      "sj reached translation integer conversion", 133, true);
    if (!mc_client_display_source_translate(font->display, (float)x, (float)y, 0, &e->display))
        return device_failed(e, 137);
    for (size_t index = 0; index < input.length; ++index) {
        while (index + 1u < input.length && input.units[index] == UINT16_C(0x00a7)) {
            int32_t palette;
            if (!format_index(input, index + 1u, &palette, e))
                return false;
            const int32_t name = mc_java_i32_add(
                mc_java_i32_add(mc_java_i32_add(font->fields.c, 256), palette), shadow ? 16 : 0);
            if (!mc_client_ge_int_put(buffer(font), name, site(236), &e->graphics))
                return graphics_failed(e, 236);
            bool empty;
            if (!remaining(font, 244, &empty, e))
                return false;
            if (empty && !flush(font, 254, 262, 269, e))
                return false;
            index += 2u; /* Actual repeated format loop273 ->152. */
        }
        if (index < input.length) {
            const mc_player_string *mapping;
            if (mc_client_font_mapping_active_use(font->gui->mapping, &mapping, &e->mapping) !=
                MC_CLIENT_FONT_MAPPING_OK)
                return failed(e, MC_CLIENT_BITMAP_FONT_MAPPING_FAILED, "sj reached fp active use",
                              288, true);
            mc_player_string_view joined;
            if (!mc_player_string_read(mapping, &joined))
                return failed(e, MC_CLIENT_BITMAP_FONT_NULL_REFERENCE,
                              "sj actual fp.a String receiver", 297, false);
            size_t first = 0;
            while (first < joined.length && joined.units[first] != input.units[index])
                ++first;
            if (first < joined.length) {
                const int32_t name =
                    mc_java_i32_add(mc_java_i32_add(font->fields.c, (int32_t)first), 32);
                if (!mc_client_ge_int_put(buffer(font), name, site(321), &e->graphics))
                    return graphics_failed(e, 321);
            }
        }
        bool empty;
        if (!remaining(font, 329, &empty, e))
            return false;
        if (empty && !flush(font, 339, 347, 354, e))
            return false;
    }
    if (!flush(font, 368, 376, -1, e))
        return false;
    if (!mc_client_display_source_pop(font->display, &e->display))
        return device_failed(e, 379);
    return true;
}

static bool draw(mc_client_bitmap_font *font, mc_player_string_view input, int32_t x, int32_t y,
                 uint32_t argb, bool shadow, mc_client_bitmap_font_error *e) {
    if (!mc_client_font_lease_begin(font->lease, &e->graphics)) {
        graphics_failed(e, -1);
        return false;
    }
    const bool ok = draw_body(font, input, x, y, argb, shadow, e);
    if (!mc_client_font_lease_end(font->lease, &e->cleanup_graphics)) {
        e->cleanup_failed = true;
        if (ok)
            failed(e, MC_CLIENT_BITMAP_FONT_RENDER_FAILED, "sj native generation draw release", -1,
                   true);
        return false;
    }
    /* No finally-pop/clear is invented after a source failure. Only the
     * native generation guard is released; the source matrix/NIO prefix stays. */
    return ok;
}

mc_client_bitmap_font_status mc_client_bitmap_font_draw(mc_client_bitmap_font *font,
                                                        const mc_player_string *text, int32_t x,
                                                        int32_t y, uint32_t argb, bool shadow,
                                                        mc_client_bitmap_font_error *error) {
    mc_client_bitmap_font_status status =
        mc_client_bitmap_font_admit(font, "a", inner_descriptor, error);
    if (status != MC_CLIENT_BITMAP_FONT_OK || text == NULL)
        return status;
    mc_player_string_view input;
    mc_client_bitmap_font_error *e;
    status = mc_client_bitmap_font_call_enter(font, text, "a", inner_descriptor, &input, &e, error);
    if (status != MC_CLIENT_BITMAP_FONT_OK)
        return status;
    (void)draw(font, input, x, y, argb, shadow, e);
    return mc_client_bitmap_font_call_leave(font, text, e, error);
}

mc_client_bitmap_font_status mc_client_bitmap_font_draw_shadow(mc_client_bitmap_font *font,
                                                               const mc_player_string *text,
                                                               int32_t x, int32_t y, uint32_t argb,
                                                               mc_client_bitmap_font_error *error) {
    mc_client_bitmap_font_status status =
        mc_client_bitmap_font_admit(font, "a", wrapper_descriptor, error);
    if (status != MC_CLIENT_BITMAP_FONT_OK || text == NULL)
        return status;
    mc_player_string_view input;
    mc_client_bitmap_font_error *e;
    status =
        mc_client_bitmap_font_call_enter(font, text, "a", wrapper_descriptor, &input, &e, error);
    if (status != MC_CLIENT_BITMAP_FONT_OK)
        return status;
    if (!draw(font, input, mc_java_i32_add(x, 1), mc_java_i32_add(y, 1), argb, true, e)) {
        e->parent_method = "a";
        e->parent_descriptor = wrapper_descriptor;
        e->parent_pc = 11;
    } else if (!draw(font, input, x, y, argb, false, e)) {
        e->parent_method = "b";
        e->parent_descriptor = wrapper_descriptor;
        e->parent_pc = 7;
        e->entry_method = "a";
        e->entry_descriptor = wrapper_descriptor;
        e->entry_pc = 20;
    }
    return mc_client_bitmap_font_call_leave(font, text, e, error);
}
