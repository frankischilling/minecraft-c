#include "mc/client/gui/font_renderer.h"
#include "font_kernels.h"

#include "mc/core/java/java.h"

#include <stdlib.h>
#include <string.h>

struct mc_font_renderer {
    uint16_t mapping[MC_FONT_RENDERER_MAX_MAPPING_UNITS];
    size_t mapping_length;
    uint8_t advances[256];
    uint32_t palette[32];
    mc_font_glyph *scratch;
    size_t capacity;
    mc_font_command *commands;
    size_t command_capacity;
};

static mc_font_renderer_status text_valid(const mc_font_text *text) {
    if (text == NULL)
        return MC_FONT_RENDERER_OK;
    if (text->length > MC_FONT_RENDERER_MAX_TEXT_UNITS)
        return MC_FONT_RENDERER_LIMIT;
    return text->units == NULL && text->length != 0 ? MC_FONT_RENDERER_INVALID_ARGUMENT
                                                    : MC_FONT_RENDERER_OK;
}

static mc_font_renderer_status mapping_read(const uint16_t *text, size_t length, uint16_t *mapping,
                                            size_t *out_length) {
    size_t count = 0, first = 0;
    while (first < length) {
        size_t end = first;
        while (end < length && text[end] != '\r' && text[end] != '\n')
            ++end;
        if (first == end || text[first] != '#') {
            if (end - first > MC_FONT_RENDERER_MAX_MAPPING_UNITS - count)
                return MC_FONT_RENDERER_LIMIT;
            for (size_t i = first; i < end; ++i)
                mapping[count++] = text[i];
        }
        first = end;
        if (first < length && text[first++] == '\r' && first < length && text[first] == '\n')
            ++first;
    }
    *out_length = count;
    return MC_FONT_RENDERER_OK;
}

mc_font_renderer_status mc_font_renderer_create(const mc_image_view *atlas,
                                                const uint16_t *font_txt, size_t units,
                                                bool anaglyph, mc_font_renderer **out) {
    if (out == NULL || atlas == NULL || atlas->pixels == NULL ||
        atlas->width != MC_FONT_RENDERER_ATLAS_SIZE ||
        atlas->height != MC_FONT_RENDERER_ATLAS_SIZE ||
        atlas->stride < 4u * MC_FONT_RENDERER_ATLAS_SIZE ||
        atlas->stride > SIZE_MAX / MC_FONT_RENDERER_ATLAS_SIZE || (font_txt == NULL && units != 0))
        return MC_FONT_RENDERER_INVALID_ARGUMENT;
    if (units > MC_FONT_RENDERER_MAX_TEXT_UNITS)
        return MC_FONT_RENDERER_LIMIT;

    uint16_t mapping[MC_FONT_RENDERER_MAX_MAPPING_UNITS];
    size_t mapping_length;
    const mc_font_renderer_status status = mapping_read(font_txt, units, mapping, &mapping_length);
    if (status != MC_FONT_RENDERER_OK)
        return status;
    mc_font_renderer *font = calloc(1, sizeof(*font));
    if (font == NULL)
        return MC_FONT_RENDERER_ALLOCATION_FAILED;
    memcpy(font->mapping, mapping, mapping_length * sizeof(*mapping));
    font->mapping_length = mapping_length;
    for (unsigned int glyph = 0; glyph < 256u; ++glyph) {
        int column = 7;
        for (; column >= 0; --column) {
            bool occupied = false;
            const size_t x = (glyph % 16u) * 8u + (unsigned int)column;
            for (unsigned int row = 0; row < 8u && !occupied; ++row) {
                const size_t y = (glyph / 16u) * 8u + row;
                occupied = atlas->pixels[y * atlas->stride + x * 4u + 2u] > 0;
            }
            if (occupied)
                break;
        }
        if (glyph == 32u)
            column = 2;
        font->advances[glyph] = (uint8_t)(column + 2);
    }
    for (unsigned int index = 0; index < 32u; ++index)
        font->palette[index] = mc_font_source_palette(index, anaglyph);
    *out = font;
    return MC_FONT_RENDERER_OK;
}

void mc_font_renderer_free(mc_font_renderer *font) {
    if (font != NULL) {
        free(font->scratch);
        free(font->commands);
        free(font);
    }
}

static int glyph_index(const mc_font_renderer *font, uint16_t unit) {
    for (size_t index = 0; index < font->mapping_length; ++index)
        if (font->mapping[index] == unit)
            return (int)index + 32;
    return -1;
}

static int32_t measured(const mc_font_renderer *font, const mc_font_text *text) {
    int32_t width = 0;
    if (text != NULL)
        for (size_t i = 0; i < text->length; ++i) {
            if (text->units[i] == UINT16_C(0x00a7)) {
                ++i;
                continue;
            }
            const int glyph = glyph_index(font, text->units[i]);
            if (glyph >= 0)
                width = mc_java_i32_add(width, font->advances[glyph]);
        }
    return width;
}

mc_font_renderer_status mc_font_renderer_measure(const mc_font_renderer *font,
                                                 const mc_font_text *text, int32_t *out) {
    if (font == NULL || out == NULL)
        return MC_FONT_RENDERER_INVALID_ARGUMENT;
    const mc_font_renderer_status status = text_valid(text);
    if (status != MC_FONT_RENDERER_OK)
        return status;
    *out = measured(font, text);
    return MC_FONT_RENDERER_OK;
}

static unsigned int color_code(uint16_t unit) {
    if (unit >= '0' && unit <= '9')
        return (unsigned int)(unit - '0');
    if (unit >= 'A' && unit <= 'F')
        return (unsigned int)(unit - 'A') + 10u;
    if (unit >= 'a' && unit <= 'f')
        return (unsigned int)(unit - 'a') + 10u;
    return 15u;
}

typedef struct layout_counts {
    size_t glyphs, commands;
} layout_counts;

static void command_emit(mc_font_command *commands, layout_counts *count, mc_font_command command) {
    if (commands != NULL)
        commands[count->commands] = command;
    ++count->commands;
}

/* One walk owns mapping, format handling, color changes and source advances.
 * The optional flat sink retains the existing compatibility projection; the
 * command sink retains the operations instead of absolute cursor geometry. */
static layout_counts walk_pass(const mc_font_renderer *font, const mc_font_text *text, int32_t x,
                               int32_t y, uint32_t argb, bool shadow, mc_font_glyph *glyphs,
                               mc_font_command *commands) {
    layout_counts count = {0};
    if (text == NULL)
        return count;
    volatile float cursor = (float)x;
    const float baseline = (float)y;
    uint32_t color = mc_font_source_initial_color(argb, shadow);
    command_emit(commands, &count,
                 (mc_font_command){.kind = MC_FONT_COMMAND_BEGIN_PASS,
                                   .value.begin_pass = {(float)x, baseline, color}});
    for (size_t i = 0; i < text->length; ++i) {
        while (i + 1 < text->length && text->units[i] == UINT16_C(0x00a7)) {
            color = font->palette[color_code(text->units[i + 1]) + (shadow ? 16u : 0u)];
            command_emit(commands, &count,
                         (mc_font_command){.kind = MC_FONT_COMMAND_COLOR, .value.argb = color});
            i += 2;
        }
        if (i < text->length) {
            const int index = glyph_index(font, text->units[i]);
            if (index >= 0) {
                if (glyphs != NULL) {
                    glyphs[count.glyphs] =
                        (mc_font_glyph){(uint16_t)index, cursor, baseline, color};
                    cursor = cursor + (float)font->advances[index];
                }
                ++count.glyphs;
                command_emit(commands, &count,
                             (mc_font_command){
                                 .kind = MC_FONT_COMMAND_GLYPH_LIST,
                                 .value.glyph_list = {(uint16_t)index, font->advances[index]}});
            }
        }
    }
    command_emit(commands, &count, (mc_font_command){.kind = MC_FONT_COMMAND_END_PASS});
    return count;
}

static mc_font_renderer_status layout_text_valid(const mc_font_text *text) {
    const mc_font_renderer_status status = text_valid(text);
    if (status != MC_FONT_RENDERER_OK)
        return status;
    bool non_ascii = false, formatting = false;
    if (text != NULL)
        for (size_t i = 0; i < text->length; ++i) {
            non_ascii = non_ascii || (text->units[i] > 127u && text->units[i] != UINT16_C(0x00a7));
            formatting = formatting || (text->units[i] == UINT16_C(0x00a7) && i + 1 < text->length);
        }
    return non_ascii && formatting ? MC_FONT_RENDERER_UNSUPPORTED_LOWERCASE : MC_FONT_RENDERER_OK;
}

mc_font_renderer_status mc_font_renderer_layout(mc_font_renderer *font, const mc_font_text *text,
                                                int32_t x, int32_t y, uint32_t argb, bool shadow,
                                                mc_font_run *out) {
    if (font == NULL || out == NULL)
        return MC_FONT_RENDERER_INVALID_ARGUMENT;
    const mc_font_renderer_status status = layout_text_valid(text);
    if (status != MC_FONT_RENDERER_OK)
        return status;
    const size_t count = walk_pass(font, text, 0, 0, 0, false, NULL, NULL).glyphs;
    const size_t passes = shadow ? 2u : 1u;
    if (count > SIZE_MAX / passes / sizeof(mc_font_glyph))
        return MC_FONT_RENDERER_LIMIT;
    const size_t needed = count * passes;
    mc_font_glyph *records = font->scratch;
    if (needed > font->capacity) {
        records = malloc(needed * sizeof(*records));
        if (records == NULL)
            return MC_FONT_RENDERER_ALLOCATION_FAILED;
    }
    size_t written = 0;
    if (shadow)
        written = walk_pass(font, text, mc_java_i32_add(x, 1), mc_java_i32_add(y, 1), argb, true,
                            records, NULL)
                      .glyphs;
    written +=
        walk_pass(font, text, x, y, argb, false, records == NULL ? NULL : records + written, NULL)
            .glyphs;
    if (records != font->scratch) {
        free(font->scratch);
        font->scratch = records;
        font->capacity = needed;
    }
    *out = (mc_font_run){written == 0 ? NULL : font->scratch, written, measured(font, text)};
    return MC_FONT_RENDERER_OK;
}

mc_font_renderer_status mc_font_renderer_layout_commands(mc_font_renderer *font,
                                                         const mc_font_text *text, int32_t x,
                                                         int32_t y, uint32_t argb, bool shadow,
                                                         mc_font_command_run *out) {
    if (font == NULL || out == NULL)
        return MC_FONT_RENDERER_INVALID_ARGUMENT;
    const mc_font_renderer_status status = layout_text_valid(text);
    if (status != MC_FONT_RENDERER_OK)
        return status;
    const size_t count = walk_pass(font, text, 0, 0, 0, false, NULL, NULL).commands;
    const size_t passes = shadow ? 2u : 1u;
    if (count > SIZE_MAX / passes / sizeof(mc_font_command))
        return MC_FONT_RENDERER_LIMIT;
    const size_t needed = count * passes;
    if (needed > MC_FONT_RENDERER_MAX_COMMANDS ||
        needed * sizeof(mc_font_command) > MC_FONT_RENDERER_COMMAND_BYTES_LIMIT)
        return MC_FONT_RENDERER_LIMIT;
    mc_font_command *records = font->commands;
    if (needed > font->command_capacity) {
        records = malloc(needed * sizeof(*records));
        if (records == NULL)
            return MC_FONT_RENDERER_ALLOCATION_FAILED;
    }
    size_t written = 0;
    if (shadow)
        written = walk_pass(font, text, mc_java_i32_add(x, 1), mc_java_i32_add(y, 1), argb, true,
                            NULL, records)
                      .commands;
    written +=
        walk_pass(font, text, x, y, argb, false, NULL, records == NULL ? NULL : records + written)
            .commands;
    if (records != font->commands) {
        free(font->commands);
        font->commands = records;
        font->command_capacity = needed;
    }
    *out =
        (mc_font_command_run){written == 0 ? NULL : font->commands, written, measured(font, text)};
    return MC_FONT_RENDERER_OK;
}
