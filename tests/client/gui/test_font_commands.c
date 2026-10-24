#include "mc/client/gui/font_renderer.h"

#include <stdio.h>
#include <string.h>

/* Authored atlas/text and hand-chosen source-contract expectations only. No
 * original assets, renderer, matrix backend or GL context is used. Commands
 * are checked before an independent identity-translation replay is compared
 * with the existing flat view; flat layout never produces expected commands. */
static unsigned int checks;
#define COUNT(values) (sizeof(values) / sizeof((values)[0]))
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static uint8_t pixels[128u * 128u * 4u];
static uint16_t maximum_text[65536];
static const uint16_t mapping[] = {' ',    'A',    'B',    'C', 'D', 0x00a7,
                                   0x00e9, 0xd83d, 0xde00, 'A', 'E'};

static uint32_t float_bits(float value) {
    uint32_t result;
    _Static_assert(sizeof(result) == sizeof(value), "binary32 command translation");
    memcpy(&result, &value, sizeof(result));
    return result;
}

static void pixel(unsigned int glyph, unsigned int column, uint8_t red, uint8_t green, uint8_t blue,
                  uint8_t alpha) {
    const size_t x = (glyph % 16u) * 8u + column;
    const size_t y = (glyph / 16u) * 8u;
    const size_t offset = (y * 128u + x) * 4u;
    pixels[offset] = red;
    pixels[offset + 1] = green;
    pixels[offset + 2] = blue;
    pixels[offset + 3] = alpha;
}

static void authored_inputs(void) {
    memset(pixels, 0, sizeof(pixels));
    pixel(32, 7, 0, 0, 255, 255);   /* Source override: space advance4. */
    pixel(33, 3, 0, 0, 1, 0);       /* A: advance5 despite transparent blue. */
    pixel(34, 7, 255, 255, 0, 255); /* B: blue-empty, advance1. */
    pixel(35, 7, 0, 0, 255, 255);   /* C: advance9. */
    pixel(36, 0, 0, 0, 1, 255);     /* D: advance2. */
    pixel(37, 1, 0, 0, 1, 255);     /* Mapped trailing section: advance3. */
    pixel(38, 2, 0, 0, 1, 255);
    pixel(39, 4, 0, 0, 1, 255);
    pixel(40, 5, 0, 0, 1, 255);
    pixel(41, 7, 0, 0, 255, 255); /* Duplicate A must still resolve to33. */
    /* E/index42 is wholly zero: a no-pixel list still advances1. */
    for (size_t i = 0; i < COUNT(maximum_text); ++i)
        maximum_text[i] = 'A';
}

static bool begin_is(const mc_font_command *command, float x, float y, uint32_t argb) {
    return command->kind == MC_FONT_COMMAND_BEGIN_PASS &&
           float_bits(command->value.begin_pass.x) == float_bits(x) &&
           float_bits(command->value.begin_pass.y) == float_bits(y) &&
           command->value.begin_pass.argb == argb;
}

static bool glyph_command_is(const mc_font_command *command, unsigned int index,
                             unsigned int advance) {
    return command->kind == MC_FONT_COMMAND_GLYPH_LIST &&
           (unsigned int)command->value.glyph_list.glyph_index == index &&
           (unsigned int)command->value.glyph_list.advance == advance;
}

static bool color_is(const mc_font_command *command, uint32_t argb) {
    return command->kind == MC_FONT_COMMAND_COLOR && command->value.argb == argb;
}

static bool command_equal(const mc_font_command *first, const mc_font_command *second) {
    if (first->kind != second->kind)
        return false;
    switch (first->kind) {
        case MC_FONT_COMMAND_BEGIN_PASS:
            return begin_is(second, first->value.begin_pass.x, first->value.begin_pass.y,
                            first->value.begin_pass.argb);
        case MC_FONT_COMMAND_COLOR:
            return first->value.argb == second->value.argb;
        case MC_FONT_COMMAND_GLYPH_LIST:
            return first->value.glyph_list.glyph_index == second->value.glyph_list.glyph_index &&
                   first->value.glyph_list.advance == second->value.glyph_list.advance;
        case MC_FONT_COMMAND_END_PASS:
            return true;
    }
    return false;
}

static bool glyph_equal(const mc_font_glyph *first, const mc_font_glyph *second) {
    return first->glyph_index == second->glyph_index &&
           float_bits(first->x) == float_bits(second->x) &&
           float_bits(first->y) == float_bits(second->y) && first->argb == second->argb;
}

static int test_null_empty(mc_font_renderer *font) {
    mc_font_command_run run = {0};
    CHECK(mc_font_renderer_layout_commands(font, NULL, 10, 20, UINT32_C(0x80112233), true, &run) ==
          MC_FONT_RENDERER_OK);
    CHECK(run.commands == NULL && run.count == 0 && run.width == 0);
    const mc_font_text empty = {NULL, 0};
    CHECK(mc_font_renderer_layout_commands(font, &empty, 10, 20, UINT32_C(0x00112233), false,
                                           &run) == MC_FONT_RENDERER_OK);
    CHECK(run.commands != NULL && run.count == 2 && run.width == 0);
    CHECK(begin_is(&run.commands[0], 10, 20, UINT32_C(0xff112233)));
    CHECK(run.commands[1].kind == MC_FONT_COMMAND_END_PASS);
    CHECK(mc_font_renderer_layout_commands(font, &empty, 10, 20, UINT32_C(0x80112233), true,
                                           &run) == MC_FONT_RENDERER_OK);
    CHECK(run.count == 4 && run.width == 0);
    CHECK(begin_is(&run.commands[0], 11, 21, UINT32_C(0x8004080c)));
    CHECK(run.commands[1].kind == MC_FONT_COMMAND_END_PASS);
    CHECK(begin_is(&run.commands[2], 10, 20, UINT32_C(0x80112233)));
    CHECK(run.commands[3].kind == MC_FONT_COMMAND_END_PASS);
    CHECK(mc_font_renderer_layout_commands(font, NULL, 10, 20, 0, false, &run) ==
          MC_FONT_RENDERER_OK);
    CHECK(run.commands == NULL && run.count == 0 && run.width == 0);
    return 0;
}

static int test_passes_and_advances(mc_font_renderer *font) {
    const uint16_t units[] = {'A', ' ', 'E', 'Z', 'B'};
    const mc_font_text text = {units, COUNT(units)};
    mc_font_command_run run = {0};
    CHECK(mc_font_renderer_layout_commands(font, &text, 3, 4, UINT32_C(0xff123456), true, &run) ==
          MC_FONT_RENDERER_OK);
    CHECK(run.count == 12 && run.width == 11);
    CHECK(begin_is(&run.commands[0], 4, 5, UINT32_C(0xff040d15)));
    CHECK(begin_is(&run.commands[6], 3, 4, UINT32_C(0xff123456)));
    for (size_t pass = 0; pass < 2; ++pass) {
        const size_t base = pass * 6u;
        CHECK(glyph_command_is(&run.commands[base + 1], 33, 5));
        CHECK(glyph_command_is(&run.commands[base + 2], 32, 4));
        CHECK(glyph_command_is(&run.commands[base + 3], 42, 1));
        CHECK(glyph_command_is(&run.commands[base + 4], 34, 1));
        CHECK(run.commands[base + 5].kind == MC_FONT_COMMAND_END_PASS);
    }
    const uint16_t unsupported[] = {'Z', 0, 0x2603};
    const mc_font_text missing = {unsupported, COUNT(unsupported)};
    CHECK(mc_font_renderer_layout_commands(font, &missing, 3, 4, 0, false, &run) ==
          MC_FONT_RENDERER_OK);
    CHECK(run.count == 2 && run.width == 0);
    CHECK(begin_is(&run.commands[0], 3, 4, UINT32_C(0xff000000)));
    CHECK(run.commands[1].kind == MC_FONT_COMMAND_END_PASS);
    const uint16_t a[] = {'A'};
    const mc_font_text one = {a, COUNT(a)};
    CHECK(mc_font_renderer_layout_commands(font, &one, INT32_MAX, INT32_MAX, 0, true, &run) ==
          MC_FONT_RENDERER_OK);
    CHECK(run.count == 6 && run.width == 5);
    CHECK(run.commands[0].kind == MC_FONT_COMMAND_BEGIN_PASS &&
          float_bits(run.commands[0].value.begin_pass.x) == UINT32_C(0xcf000000) &&
          float_bits(run.commands[0].value.begin_pass.y) == UINT32_C(0xcf000000));
    CHECK(run.commands[3].kind == MC_FONT_COMMAND_BEGIN_PASS &&
          float_bits(run.commands[3].value.begin_pass.x) == UINT32_C(0x4f000000) &&
          float_bits(run.commands[3].value.begin_pass.y) == UINT32_C(0x4f000000));
    CHECK(glyph_command_is(&run.commands[1], 33, 5) && glyph_command_is(&run.commands[4], 33, 5));
    CHECK(run.commands[2].kind == MC_FONT_COMMAND_END_PASS &&
          run.commands[5].kind == MC_FONT_COMMAND_END_PASS);
    return 0;
}

static int test_colors_and_trailing_section(mc_font_renderer *font) {
    const uint16_t units[] = {0x00a7, '4', 0x00a7, '4', 'A', 0x00a7, 'c'};
    const mc_font_text text = {units, COUNT(units)};
    mc_font_command_run run = {0};
    CHECK(mc_font_renderer_layout_commands(font, &text, 0, 0, UINT32_C(0x40102030), true, &run) ==
          MC_FONT_RENDERER_OK);
    CHECK(run.count == 12 && run.width == 5);
    CHECK(begin_is(&run.commands[0], 1, 1, UINT32_C(0x4004080c)));
    CHECK(color_is(&run.commands[1], UINT32_C(0xff2a0000)));
    CHECK(color_is(&run.commands[2], UINT32_C(0xff2a0000)));
    CHECK(glyph_command_is(&run.commands[3], 33, 5));
    CHECK(color_is(&run.commands[4], UINT32_C(0xff3f1515)));
    CHECK(run.commands[5].kind == MC_FONT_COMMAND_END_PASS);
    CHECK(begin_is(&run.commands[6], 0, 0, UINT32_C(0x40102030)));
    CHECK(color_is(&run.commands[7], UINT32_C(0xffaa0000)));
    CHECK(color_is(&run.commands[8], UINT32_C(0xffaa0000)));
    CHECK(glyph_command_is(&run.commands[9], 33, 5));
    CHECK(color_is(&run.commands[10], UINT32_C(0xffff5555)));
    CHECK(run.commands[11].kind == MC_FONT_COMMAND_END_PASS);
    const uint16_t only_color[] = {0x00a7, 'e', 0x00a7, 'e'};
    const mc_font_text colors = {only_color, COUNT(only_color)};
    CHECK(mc_font_renderer_layout_commands(font, &colors, 0, 0, UINT32_C(0x40102030), false,
                                           &run) == MC_FONT_RENDERER_OK);
    CHECK(run.count == 4 && run.width == 0);
    CHECK(color_is(&run.commands[1], UINT32_C(0xffffff55)) &&
          color_is(&run.commands[2], UINT32_C(0xffffff55)));
    CHECK(run.commands[3].kind == MC_FONT_COMMAND_END_PASS);
    const uint16_t section[] = {0x00a7};
    const mc_font_text trailing = {section, COUNT(section)};
    CHECK(mc_font_renderer_layout_commands(font, &trailing, 3, 4, 0, false, &run) ==
          MC_FONT_RENDERER_OK);
    CHECK(run.count == 3 && run.width == 0);
    CHECK(glyph_command_is(&run.commands[1], 37, 3));
    int32_t measured = -71;
    CHECK(mc_font_renderer_measure(font, &trailing, &measured) == MC_FONT_RENDERER_OK &&
          measured == 0);
    const uint16_t after_color[] = {0x00a7, 'a', 0x00a7};
    const mc_font_text final_section = {after_color, COUNT(after_color)};
    CHECK(mc_font_renderer_layout_commands(font, &final_section, 3, 4, 0, false, &run) ==
          MC_FONT_RENDERER_OK);
    CHECK(run.count == 4 && run.width == 0);
    CHECK(color_is(&run.commands[1], UINT32_C(0xff55ff55)));
    CHECK(glyph_command_is(&run.commands[2], 37, 3));
    CHECK(run.commands[3].kind == MC_FONT_COMMAND_END_PASS);
    return 0;
}

/* This interpreter consumes commands only. It does not inspect source text,
 * atlas, mapping or font tables. Small integer coordinates keep identity
 * translations exact; this is no claim about an original GL matrix backend. */
static int replay_identity(const mc_font_command_run *run, mc_font_glyph *glyphs, size_t capacity,
                           size_t *out_count) {
    bool active = false;
    volatile float x = 0;
    float y = 0;
    uint32_t color = 0;
    size_t count = 0;
    for (size_t i = 0; i < run->count; ++i) {
        const mc_font_command *command = &run->commands[i];
        switch (command->kind) {
            case MC_FONT_COMMAND_BEGIN_PASS:
                CHECK(!active);
                active = true;
                x = command->value.begin_pass.x;
                y = command->value.begin_pass.y;
                color = command->value.begin_pass.argb;
                break;
            case MC_FONT_COMMAND_COLOR:
                CHECK(active);
                color = command->value.argb;
                break;
            case MC_FONT_COMMAND_GLYPH_LIST:
                CHECK(active && count < capacity);
                glyphs[count++] =
                    (mc_font_glyph){command->value.glyph_list.glyph_index, x, y, color};
                x = x + (float)command->value.glyph_list.advance;
                break;
            case MC_FONT_COMMAND_END_PASS:
                CHECK(active);
                active = false;
                break;
            default:
                CHECK(false);
        }
    }
    CHECK(!active);
    *out_count = count;
    return 0;
}

static int test_identity_replay(mc_font_renderer *font) {
    const uint16_t units[] = {'A', ' ', 'E', 'Z', 0x00a7, '4', 'B'};
    const mc_font_text text = {units, COUNT(units)};
    mc_font_command_run commands = {0};
    CHECK(mc_font_renderer_layout_commands(font, &text, 3, 4, UINT32_C(0x80112233), true,
                                           &commands) == MC_FONT_RENDERER_OK);
    CHECK(commands.count == 14 && commands.width == 11);
    CHECK(begin_is(&commands.commands[0], 4, 5, UINT32_C(0x8004080c)));
    CHECK(glyph_command_is(&commands.commands[1], 33, 5));
    CHECK(glyph_command_is(&commands.commands[2], 32, 4));
    CHECK(glyph_command_is(&commands.commands[3], 42, 1));
    CHECK(color_is(&commands.commands[4], UINT32_C(0xff2a0000)));
    CHECK(glyph_command_is(&commands.commands[5], 34, 1));
    CHECK(commands.commands[6].kind == MC_FONT_COMMAND_END_PASS);
    CHECK(begin_is(&commands.commands[7], 3, 4, UINT32_C(0x80112233)));
    CHECK(color_is(&commands.commands[11], UINT32_C(0xffaa0000)));
    mc_font_glyph replay[8];
    size_t replay_count = 0;
    CHECK(replay_identity(&commands, replay, COUNT(replay), &replay_count) == 0);
    const mc_font_glyph expected[] = {
        {33, 4, 5, UINT32_C(0x8004080c)},  {32, 9, 5, UINT32_C(0x8004080c)},
        {42, 13, 5, UINT32_C(0x8004080c)}, {34, 14, 5, UINT32_C(0xff2a0000)},
        {33, 3, 4, UINT32_C(0x80112233)},  {32, 8, 4, UINT32_C(0x80112233)},
        {42, 12, 4, UINT32_C(0x80112233)}, {34, 13, 4, UINT32_C(0xffaa0000)}};
    CHECK(replay_count == COUNT(expected));
    for (size_t i = 0; i < COUNT(expected); ++i)
        CHECK(glyph_equal(&replay[i], &expected[i]));
    mc_font_run flat = {0};
    CHECK(mc_font_renderer_layout(font, &text, 3, 4, UINT32_C(0x80112233), true, &flat) ==
          MC_FONT_RENDERER_OK);
    CHECK(flat.count == replay_count && flat.width == 11);
    for (size_t i = 0; i < replay_count; ++i)
        CHECK(glyph_equal(&flat.glyphs[i], &replay[i]));
    return 0;
}

static int verify_maximum(const mc_font_command_run *run) {
    CHECK(MC_FONT_RENDERER_MAX_TEXT_UNITS == 65536u && MC_FONT_RENDERER_MAX_COMMANDS == 131076u);
    CHECK(run->commands != NULL && run->count == 131076u && run->width == 327680);
    CHECK(begin_is(&run->commands[0], 11, 21, UINT32_C(0xff040d15)));
    CHECK(begin_is(&run->commands[65538], 10, 20, UINT32_C(0xff123456)));
    for (size_t pass = 0; pass < 2; ++pass) {
        const size_t base = pass * 65538u;
        for (size_t i = 0; i < 65536u; ++i)
            CHECK(glyph_command_is(&run->commands[base + 1u + i], 33, 5));
        CHECK(run->commands[base + 65537u].kind == MC_FONT_COMMAND_END_PASS);
    }
    return 0;
}

static int test_scratch_and_maximum(mc_font_renderer *font) {
    const uint16_t a[] = {'A'};
    const mc_font_text one = {a, COUNT(a)};
    mc_font_run flat = {0};
    CHECK(mc_font_renderer_layout(font, &one, 1, 2, UINT32_C(0xff123456), true, &flat) ==
          MC_FONT_RENDERER_OK);
    CHECK(flat.count == 2 && flat.width == 5);
    const mc_font_run flat_before = flat;
    const mc_font_glyph saved[] = {flat.glyphs[0], flat.glyphs[1]};
    const mc_font_text maximum = {maximum_text, COUNT(maximum_text)};
    mc_font_command_run commands = {0};
    CHECK(mc_font_renderer_layout_commands(font, &maximum, 10, 20, UINT32_C(0xff123456), true,
                                           &commands) == MC_FONT_RENDERER_OK);
    CHECK(verify_maximum(&commands) == 0);
    CHECK(flat.glyphs == flat_before.glyphs && flat.count == flat_before.count &&
          flat.width == flat_before.width);
    CHECK(glyph_equal(&flat.glyphs[0], &saved[0]) && glyph_equal(&flat.glyphs[1], &saved[1]));
    const mc_font_command_run commands_before = commands;
    CHECK(mc_font_renderer_layout(font, &maximum, 0, 0, 0, true, &flat) == MC_FONT_RENDERER_OK);
    CHECK(flat.count == 131072u && flat.width == 327680);
    CHECK(commands.commands == commands_before.commands &&
          commands.count == commands_before.count && commands.width == commands_before.width);
    CHECK(verify_maximum(&commands) == 0);
    return 0;
}

static int preserved(const mc_font_command_run *run, const mc_font_command_run *before,
                     const mc_font_command *saved, const mc_font_run *flat,
                     const mc_font_run *flat_before, const mc_font_glyph *glyphs) {
    CHECK(run->commands == before->commands && run->count == before->count &&
          run->width == before->width);
    for (size_t i = 0; i < before->count; ++i)
        CHECK(command_equal(&run->commands[i], &saved[i]));
    CHECK(flat->glyphs == flat_before->glyphs && flat->count == flat_before->count &&
          flat->width == flat_before->width);
    for (size_t i = 0; i < flat_before->count; ++i)
        CHECK(glyph_equal(&flat->glyphs[i], &glyphs[i]));
    return 0;
}

static int test_failures(mc_font_renderer *font) {
    const uint16_t units[] = {'A', 'B'};
    mc_font_text text = {units, COUNT(units)};
    mc_font_command_run run = {0};
    mc_font_run flat = {0};
    CHECK(mc_font_renderer_layout_commands(font, &text, 1, 2, UINT32_C(0xff123456), true, &run) ==
          MC_FONT_RENDERER_OK);
    CHECK(run.count == 8 && run.width == 6);
    CHECK(mc_font_renderer_layout(font, &text, 1, 2, UINT32_C(0xff123456), true, &flat) ==
          MC_FONT_RENDERER_OK);
    CHECK(flat.count == 4 && flat.width == 6);
    const mc_font_command_run before = run;
    const mc_font_run flat_before = flat;
    mc_font_command saved[8];
    mc_font_glyph glyphs[4];
    for (size_t i = 0; i < COUNT(saved); ++i)
        saved[i] = run.commands[i];
    for (size_t i = 0; i < COUNT(glyphs); ++i)
        glyphs[i] = flat.glyphs[i];
    const uint16_t unicode_format[] = {0x00e9, 0x00a7, 'c', 'A'};
    text = (mc_font_text){unicode_format, COUNT(unicode_format)};
    CHECK(mc_font_renderer_layout_commands(font, &text, 0, 0, 0, false, &run) ==
          MC_FONT_RENDERER_UNSUPPORTED_LOWERCASE);
    CHECK(preserved(&run, &before, saved, &flat, &flat_before, glyphs) == 0);
    CHECK(mc_font_renderer_layout(font, &text, 0, 0, 0, false, &flat) ==
          MC_FONT_RENDERER_UNSUPPORTED_LOWERCASE);
    CHECK(preserved(&run, &before, saved, &flat, &flat_before, glyphs) == 0);
    text = (mc_font_text){units, 65537}; /* Must reject length before reading. */
    CHECK(mc_font_renderer_layout_commands(font, &text, 0, 0, 0, true, &run) ==
          MC_FONT_RENDERER_LIMIT);
    CHECK(preserved(&run, &before, saved, &flat, &flat_before, glyphs) == 0);
    text = (mc_font_text){NULL, 1};
    CHECK(mc_font_renderer_layout_commands(font, &text, 0, 0, 0, false, &run) ==
          MC_FONT_RENDERER_INVALID_ARGUMENT);
    CHECK(preserved(&run, &before, saved, &flat, &flat_before, glyphs) == 0);
    CHECK(mc_font_renderer_layout_commands(NULL, NULL, 0, 0, 0, false, &run) ==
          MC_FONT_RENDERER_INVALID_ARGUMENT);
    CHECK(preserved(&run, &before, saved, &flat, &flat_before, glyphs) == 0);
    CHECK(mc_font_renderer_layout_commands(font, NULL, 0, 0, 0, false, NULL) ==
          MC_FONT_RENDERER_INVALID_ARGUMENT);
    CHECK(preserved(&run, &before, saved, &flat, &flat_before, glyphs) == 0);
    return 0;
}

static int create_fonts(mc_font_renderer **font, mc_font_renderer **scratch) {
    const mc_image_view atlas = {128, 128, 512, pixels};
    CHECK(mc_font_renderer_create(&atlas, mapping, COUNT(mapping), false, font) ==
          MC_FONT_RENDERER_OK);
    CHECK(mc_font_renderer_create(&atlas, mapping, COUNT(mapping), false, scratch) ==
          MC_FONT_RENDERER_OK);
    return 0;
}

int main(void) {
    authored_inputs();
    mc_font_renderer *font = NULL, *scratch = NULL;
    int result = create_fonts(&font, &scratch);
    if (result == 0)
        result = test_null_empty(font) || test_passes_and_advances(font) ||
                 test_colors_and_trailing_section(font) || test_identity_replay(font) ||
                 test_failures(font) || test_scratch_and_maximum(scratch);
    mc_font_renderer_free(scratch);
    mc_font_renderer_free(font);
    if (result == 0)
        printf("PASS: %u FontRenderer command source-contract checks\n", checks);
    return result;
}
