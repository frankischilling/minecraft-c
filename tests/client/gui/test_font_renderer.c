#include "mc/client/gui/font_renderer.h"

#include <stdio.h>
#include <string.h>

/* Authored pixels/text only. Literal expectations follow sj's blue scan,
 * UTF-16 lookup, color lists, shadow entry and separate width method. They are
 * native source contracts, not original rendered-image observations. */
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
static uint16_t long_text[MC_FONT_RENDERER_MAX_TEXT_UNITS];
static const uint16_t font_txt[] = {'#',    'x',    '\r',   '\n', ' ',  'A', 'B', 'C',  'D', 0x00a7,
                                    0x00e9, 0xd83d, 0xde00, '\r', '\n', '#', 'x', '\n', 'A'};

static uint32_t float_bits(float value) {
    uint32_t result;
    _Static_assert(sizeof(result) == sizeof(value), "binary32 glyph coordinates");
    memcpy(&result, &value, sizeof(result));
    return result;
}

static void pixel(unsigned int glyph, unsigned int column, unsigned int row, uint8_t red,
                  uint8_t green, uint8_t blue, uint8_t alpha) {
    const size_t x = (glyph % 16u) * 8u + column;
    const size_t y = (glyph / 16u) * 8u + row;
    const size_t offset = (y * 128u + x) * 4u;
    pixels[offset] = red;
    pixels[offset + 1] = green;
    pixels[offset + 2] = blue;
    pixels[offset + 3] = alpha;
}

static void authored_atlas(void) {
    memset(pixels, 0, sizeof(pixels));
    pixel(32, 7, 0, 0, 0, 255, 255);   /* Source space override still advances4. */
    pixel(33, 3, 7, 0, 0, 1, 0);       /* Blue1/alpha0 advances5. */
    pixel(34, 7, 0, 255, 255, 0, 255); /* Red/green/alpha do not occupy it. */
    pixel(35, 7, 0, 0, 0, 255, 255);
    pixel(36, 0, 0, 0, 0, 1, 255);
    pixel(37, 1, 0, 0, 0, 1, 255);
    pixel(38, 2, 0, 0, 0, 1, 255);
    pixel(39, 4, 0, 0, 0, 1, 255);
    pixel(40, 5, 0, 0, 0, 1, 255);
    pixel(41, 7, 0, 0, 0, 255, 255); /* Duplicate A must use its earlier cell33. */
}

static bool glyph_is(const mc_font_glyph *glyph, unsigned int index, float x, float y,
                     uint32_t argb) {
    return (unsigned int)glyph->glyph_index == index && float_bits(glyph->x) == float_bits(x) &&
           float_bits(glyph->y) == float_bits(y) && glyph->argb == argb;
}

static int measure_is(const mc_font_renderer *font, const uint16_t *units, size_t length,
                      int32_t expected) {
    const mc_font_text text = {units, length};
    int32_t width = -71;
    CHECK(mc_font_renderer_measure(font, &text, &width) == MC_FONT_RENDERER_OK);
    CHECK(width == expected);
    return 0;
}

static int test_measure(const mc_font_renderer *font) {
    int32_t width = 71;
    CHECK(mc_font_renderer_measure(font, NULL, &width) == MC_FONT_RENDERER_OK && width == 0);
    CHECK(measure_is(font, NULL, 0, 0) == 0);
    const uint16_t a[] = {'A'}, b[] = {'B'}, c[] = {'C'}, d[] = {'D'}, space[] = {' '};
    CHECK(measure_is(font, a, COUNT(a), 5) == 0);
    CHECK(measure_is(font, b, COUNT(b), 1) == 0);
    CHECK(measure_is(font, c, COUNT(c), 9) == 0);
    CHECK(measure_is(font, d, COUNT(d), 2) == 0);
    CHECK(measure_is(font, space, COUNT(space), 4) == 0);
    const uint16_t all[] = {'A', 'B', 'C', 'D'};
    const uint16_t unsupported[] = {'A', ' ', 'Z', 0, 'B'};
    const uint16_t accent[] = {0x00e9};
    const uint16_t pair[] = {0xd83d, 0xde00};
    const uint16_t lone[] = {0xd83d};
    CHECK(measure_is(font, all, COUNT(all), 17) == 0);
    CHECK(measure_is(font, unsupported, COUNT(unsupported), 10) == 0);
    CHECK(measure_is(font, accent, COUNT(accent), 4) == 0);
    CHECK(measure_is(font, pair, COUNT(pair), 13) == 0);
    CHECK(measure_is(font, lone, COUNT(lone), 6) == 0);
    const uint16_t formatted[] = {'A', 0x00a7, 'c', 'C'};
    const uint16_t trailing[] = {'A', 0x00a7};
    const uint16_t section[] = {0x00a7};
    const uint16_t repeated[] = {0x00a7, 0x00a7, 'A'};
    const uint16_t after_code[] = {0x00a7, 'a', 0x00a7};
    CHECK(measure_is(font, formatted, COUNT(formatted), 14) == 0);
    CHECK(measure_is(font, trailing, COUNT(trailing), 5) == 0);
    CHECK(measure_is(font, section, COUNT(section), 0) == 0);
    CHECK(measure_is(font, repeated, COUNT(repeated), 5) == 0);
    CHECK(measure_is(font, after_code, COUNT(after_code), 0) == 0);
    return 0;
}

static int test_layout(mc_font_renderer *font) {
    const uint16_t plain[] = {'A', ' ', 'Z', 'B'};
    mc_font_text text = {plain, COUNT(plain)};
    mc_font_run run;
    CHECK(mc_font_renderer_layout(font, &text, 10, 20, UINT32_C(0x00112233), false, &run) ==
          MC_FONT_RENDERER_OK);
    CHECK(run.count == 3 && run.width == 10);
    CHECK(glyph_is(&run.glyphs[0], 33, 10, 20, UINT32_C(0xff112233)));
    CHECK(glyph_is(&run.glyphs[1], 32, 15, 20, UINT32_C(0xff112233)));
    CHECK(glyph_is(&run.glyphs[2], 34, 19, 20, UINT32_C(0xff112233)));
    const uint16_t a[] = {'A'};
    text = (mc_font_text){a, COUNT(a)};
    CHECK(mc_font_renderer_layout(font, &text, 10, 20, UINT32_C(0x80112233), true, &run) ==
          MC_FONT_RENDERER_OK);
    CHECK(run.count == 2 && run.width == 5);
    CHECK(glyph_is(&run.glyphs[0], 33, 11, 21, UINT32_C(0x8004080c)));
    CHECK(glyph_is(&run.glyphs[1], 33, 10, 20, UINT32_C(0x80112233)));
    const uint16_t colored[] = {'A', 0x00a7, '4', 'C', 0x00a7, 'z', 'D'};
    text = (mc_font_text){colored, COUNT(colored)};
    CHECK(mc_font_renderer_layout(font, &text, 0, 0, UINT32_C(0x40102030), false, &run) ==
          MC_FONT_RENDERER_OK);
    CHECK(run.count == 3 && run.width == 16);
    CHECK(glyph_is(&run.glyphs[0], 33, 0, 0, UINT32_C(0x40102030)));
    CHECK(glyph_is(&run.glyphs[1], 35, 5, 0, UINT32_C(0xffaa0000)));
    CHECK(glyph_is(&run.glyphs[2], 36, 14, 0, UINT32_C(0xffffffff)));
    const uint16_t uppercase[] = {0x00a7, 'E', 'A'};
    text = (mc_font_text){uppercase, COUNT(uppercase)};
    CHECK(mc_font_renderer_layout(font, &text, 0, 0, 0, false, &run) == MC_FONT_RENDERER_OK);
    CHECK(run.count == 1 && glyph_is(&run.glyphs[0], 33, 0, 0, UINT32_C(0xffffff55)));
    const uint16_t trail[] = {0x00a7};
    text = (mc_font_text){trail, COUNT(trail)};
    CHECK(mc_font_renderer_layout(font, &text, 3, 4, UINT32_C(0xff123456), false, &run) ==
          MC_FONT_RENDERER_OK);
    CHECK(run.count == 1 && run.width == 0);
    CHECK(glyph_is(&run.glyphs[0], 37, 3, 4, UINT32_C(0xff123456)));
    const uint16_t trailing_color[] = {0x00a7, 'a', 0x00a7};
    text = (mc_font_text){trailing_color, COUNT(trailing_color)};
    CHECK(mc_font_renderer_layout(font, &text, 0, 0, 0, false, &run) == MC_FONT_RENDERER_OK);
    CHECK(run.count == 1 && run.width == 0);
    CHECK(glyph_is(&run.glyphs[0], 37, 0, 0, UINT32_C(0xff55ff55)));
    const uint16_t repeat[] = {0x00a7, 0x00a7, 'A'};
    text = (mc_font_text){repeat, COUNT(repeat)};
    CHECK(mc_font_renderer_layout(font, &text, 0, 0, 0, false, &run) == MC_FONT_RENDERER_OK);
    CHECK(run.count == 1 && run.width == 5);
    CHECK(glyph_is(&run.glyphs[0], 33, 0, 0, UINT32_C(0xffffffff)));
    const uint16_t utf16[] = {0x00e9, 0xd83d, 0xde00};
    text = (mc_font_text){utf16, COUNT(utf16)};
    CHECK(mc_font_renderer_layout(font, &text, 0, 0, 0, false, &run) == MC_FONT_RENDERER_OK);
    CHECK(run.count == 3 && run.width == 17);
    CHECK(glyph_is(&run.glyphs[0], 38, 0, 0, UINT32_C(0xff000000)));
    CHECK(glyph_is(&run.glyphs[1], 39, 4, 0, UINT32_C(0xff000000)));
    CHECK(glyph_is(&run.glyphs[2], 40, 10, 0, UINT32_C(0xff000000)));
    const uint16_t unicode_trailing[] = {0x00e9, 0x00a7};
    text = (mc_font_text){unicode_trailing, COUNT(unicode_trailing)};
    CHECK(mc_font_renderer_layout(font, &text, 0, 0, 0, false, &run) == MC_FONT_RENDERER_OK);
    CHECK(run.count == 2 && run.width == 4);
    CHECK(glyph_is(&run.glyphs[0], 38, 0, 0, UINT32_C(0xff000000)));
    CHECK(glyph_is(&run.glyphs[1], 37, 4, 0, UINT32_C(0xff000000)));
    const uint16_t twice[] = {'A', 'A'};
    text = (mc_font_text){twice, COUNT(twice)};
    CHECK(mc_font_renderer_layout(font, &text, INT32_MAX, INT32_MAX, UINT32_C(0xffffffff), true,
                                  &run) == MC_FONT_RENDERER_OK);
    CHECK(run.count == 4 && run.width == 10);
    CHECK(float_bits(run.glyphs[0].x) == UINT32_C(0xcf000000) &&
          float_bits(run.glyphs[0].y) == UINT32_C(0xcf000000));
    CHECK(float_bits(run.glyphs[1].x) == UINT32_C(0xcf000000));
    CHECK(float_bits(run.glyphs[2].x) == UINT32_C(0x4f000000) &&
          float_bits(run.glyphs[2].y) == UINT32_C(0x4f000000));
    CHECK(float_bits(run.glyphs[3].x) == UINT32_C(0x4f000000));
    CHECK(float_bits(MC_FONT_RENDERER_GLYPH_EXTENT) == UINT32_C(0x40ffae14));
    CHECK(mc_font_renderer_layout(font, NULL, 0, 0, 0, true, &run) == MC_FONT_RENDERER_OK);
    CHECK(run.count == 0 && run.width == 0 && run.glyphs == NULL);
    return 0;
}

static int test_palette(mc_font_renderer *font, mc_font_renderer *anaglyph) {
    static const uint16_t codes[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                     '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    static const uint32_t normal[] = {0xff000000, 0xff0000aa, 0xff00aa00, 0xff00aaaa,
                                      0xffaa0000, 0xffaa00aa, 0xffffaa00, 0xffaaaaaa,
                                      0xff555555, 0xff5555ff, 0xff55ff55, 0xff55ffff,
                                      0xffff5555, 0xffff55ff, 0xffffff55, 0xffffffff};
    static const uint32_t shadow[] = {0xff000000, 0xff00002a, 0xff002a00, 0xff002a2a,
                                      0xff2a0000, 0xff2a002a, 0xff2a2a00, 0xff2a2a2a,
                                      0xff151515, 0xff15153f, 0xff153f15, 0xff153f3f,
                                      0xff3f1515, 0xff3f153f, 0xff3f3f15, 0xff3f3f3f};
    for (size_t i = 0; i < COUNT(codes); ++i) {
        const uint16_t units[] = {0x00a7, codes[i], 'A'};
        const mc_font_text text = {units, COUNT(units)};
        mc_font_run run;
        CHECK(mc_font_renderer_layout(font, &text, 0, 0, UINT32_C(0x80123456), true, &run) ==
              MC_FONT_RENDERER_OK);
        CHECK(run.count == 2 && run.width == 5);
        CHECK(glyph_is(&run.glyphs[0], 33, 1, 1, shadow[i]));
        CHECK(glyph_is(&run.glyphs[1], 33, 0, 0, normal[i]));
    }

    static const struct {
        uint16_t code;
        uint32_t normal, shadow;
    } converted[] = {{'1', 0xff120077, 0xff04001d},
                     {'4', 0xff333333, 0xff0c0c0c},
                     {'6', 0xffb0c34c, 0xff252a0c}};

    for (size_t i = 0; i < COUNT(converted); ++i) {
        const uint16_t units[] = {0x00a7, converted[i].code, 'A'};
        const mc_font_text text = {units, COUNT(units)};
        mc_font_run run;
        CHECK(mc_font_renderer_layout(anaglyph, &text, 0, 0, 0, true, &run) == MC_FONT_RENDERER_OK);
        CHECK(run.count == 2 && run.glyphs[0].argb == converted[i].shadow &&
              run.glyphs[1].argb == converted[i].normal);
    }
    const uint16_t a[] = {'A'};
    const mc_font_text text = {a, COUNT(a)};
    mc_font_run run;
    CHECK(mc_font_renderer_layout(anaglyph, &text, 0, 0, UINT32_C(0x80204060), true, &run) ==
          MC_FONT_RENDERER_OK);
    CHECK(run.count == 2 && run.glyphs[0].argb == UINT32_C(0x80081018) &&
          run.glyphs[1].argb == UINT32_C(0x80204060));
    return 0;
}

static int test_failures(mc_font_renderer *font) {
    const uint16_t a[] = {'A'};
    mc_font_text text = {a, COUNT(a)};
    mc_font_run run;
    CHECK(mc_font_renderer_layout(font, &text, 1, 2, UINT32_C(0xff123456), true, &run) ==
          MC_FONT_RENDERER_OK);
    unsigned char before[sizeof(run)], glyphs[2u * sizeof(mc_font_glyph)];
    memcpy(before, &run, sizeof(before));
    memcpy(glyphs, run.glyphs, sizeof(glyphs));
    const uint16_t expanding_case[] = {0x0130, 0x00a7, 'c', 'A'};
    text = (mc_font_text){expanding_case, COUNT(expanding_case)};
    CHECK(mc_font_renderer_layout(font, &text, 0, 0, 0, false, &run) ==
          MC_FONT_RENDERER_UNSUPPORTED_LOWERCASE);
    CHECK(memcmp(before, &run, sizeof(before)) == 0 &&
          memcmp(glyphs, run.glyphs, sizeof(glyphs)) == 0);
    CHECK(measure_is(font, expanding_case, COUNT(expanding_case), 5) == 0);
    const uint16_t unicode_format[] = {0x00e9, 0x00a7, 'c', 'A'};
    text = (mc_font_text){unicode_format, COUNT(unicode_format)};
    CHECK(mc_font_renderer_layout(font, &text, 0, 0, 0, true, &run) ==
          MC_FONT_RENDERER_UNSUPPORTED_LOWERCASE);
    CHECK(memcmp(before, &run, sizeof(before)) == 0 &&
          memcmp(glyphs, run.glyphs, sizeof(glyphs)) == 0);
    text = (mc_font_text){a, MC_FONT_RENDERER_MAX_TEXT_UNITS + 1u};
    CHECK(mc_font_renderer_layout(font, &text, 0, 0, 0, true, &run) == MC_FONT_RENDERER_LIMIT);
    CHECK(memcmp(before, &run, sizeof(before)) == 0 &&
          memcmp(glyphs, run.glyphs, sizeof(glyphs)) == 0);
    int32_t width = 31;
    CHECK(mc_font_renderer_measure(font, &text, &width) == MC_FONT_RENDERER_LIMIT && width == 31);
    text = (mc_font_text){NULL, 1};
    CHECK(mc_font_renderer_layout(font, &text, 0, 0, 0, false, &run) ==
          MC_FONT_RENDERER_INVALID_ARGUMENT);
    CHECK(memcmp(before, &run, sizeof(before)) == 0);
    CHECK(mc_font_renderer_measure(font, &text, &width) == MC_FONT_RENDERER_INVALID_ARGUMENT &&
          width == 31);
    CHECK(mc_font_renderer_measure(NULL, NULL, &width) == MC_FONT_RENDERER_INVALID_ARGUMENT &&
          width == 31);
    CHECK(mc_font_renderer_measure(font, NULL, NULL) == MC_FONT_RENDERER_INVALID_ARGUMENT);
    CHECK(mc_font_renderer_layout(NULL, NULL, 0, 0, 0, false, &run) ==
          MC_FONT_RENDERER_INVALID_ARGUMENT);
    CHECK(memcmp(before, &run, sizeof(before)) == 0);
    CHECK(mc_font_renderer_layout(font, NULL, 0, 0, 0, false, NULL) ==
          MC_FONT_RENDERER_INVALID_ARGUMENT);
    return 0;
}

static int test_bounds(mc_font_renderer *font, const mc_image_view *atlas) {
    mc_font_renderer *unchanged = font;
    mc_image_view invalid = *atlas;
    invalid.width = 127;
    CHECK(mc_font_renderer_create(&invalid, font_txt, COUNT(font_txt), false, &unchanged) ==
              MC_FONT_RENDERER_INVALID_ARGUMENT &&
          unchanged == font);
    invalid = *atlas;
    invalid.height = 127;
    CHECK(mc_font_renderer_create(&invalid, font_txt, COUNT(font_txt), false, &unchanged) ==
              MC_FONT_RENDERER_INVALID_ARGUMENT &&
          unchanged == font);
    invalid = *atlas;
    invalid.stride = 511;
    CHECK(mc_font_renderer_create(&invalid, font_txt, COUNT(font_txt), false, &unchanged) ==
              MC_FONT_RENDERER_INVALID_ARGUMENT &&
          unchanged == font);
    invalid.stride = SIZE_MAX;
    CHECK(mc_font_renderer_create(&invalid, font_txt, COUNT(font_txt), false, &unchanged) ==
              MC_FONT_RENDERER_INVALID_ARGUMENT &&
          unchanged == font);
    invalid = *atlas;
    invalid.pixels = NULL;
    CHECK(mc_font_renderer_create(&invalid, font_txt, COUNT(font_txt), false, &unchanged) ==
              MC_FONT_RENDERER_INVALID_ARGUMENT &&
          unchanged == font);
    CHECK(mc_font_renderer_create(NULL, font_txt, COUNT(font_txt), false, &unchanged) ==
              MC_FONT_RENDERER_INVALID_ARGUMENT &&
          unchanged == font);
    CHECK(mc_font_renderer_create(atlas, NULL, 1, false, &unchanged) ==
              MC_FONT_RENDERER_INVALID_ARGUMENT &&
          unchanged == font);
    CHECK(mc_font_renderer_create(atlas, font_txt, MC_FONT_RENDERER_MAX_TEXT_UNITS + 1u, false,
                                  &unchanged) == MC_FONT_RENDERER_LIMIT &&
          unchanged == font);
    CHECK(mc_font_renderer_create(atlas, font_txt, COUNT(font_txt), false, NULL) ==
          MC_FONT_RENDERER_INVALID_ARGUMENT);
    uint16_t oversized[MC_FONT_RENDERER_MAX_MAPPING_UNITS + 1u];
    for (size_t i = 0; i < COUNT(oversized); ++i)
        oversized[i] = 'A';
    CHECK(mc_font_renderer_create(atlas, oversized, COUNT(oversized), false, &unchanged) ==
              MC_FONT_RENDERER_LIMIT &&
          unchanged == font);
    mc_font_renderer *maximum = NULL, *empty = NULL, *leading_space = NULL, *embedded_null = NULL;
    CHECK(mc_font_renderer_create(atlas, oversized, MC_FONT_RENDERER_MAX_MAPPING_UNITS, false,
                                  &maximum) == MC_FONT_RENDERER_OK);
    CHECK(mc_font_renderer_create(atlas, NULL, 0, false, &empty) == MC_FONT_RENDERER_OK);
    const uint16_t lines[] = {' ', '#', '\r', 'A', '\n', '#', 'A'};
    CHECK(mc_font_renderer_create(atlas, lines, COUNT(lines), false, &leading_space) ==
          MC_FONT_RENDERER_OK);
    const uint16_t with_null[] = {' ', 0, 'A'};
    CHECK(mc_font_renderer_create(atlas, with_null, COUNT(with_null), false, &embedded_null) ==
          MC_FONT_RENDERER_OK);
    const uint16_t a[] = {'A'};
    CHECK(measure_is(maximum, a, COUNT(a), 4) == 0);
    CHECK(measure_is(empty, a, COUNT(a), 0) == 0);
    const uint16_t hash_a[] = {'#', 'A'};
    CHECK(measure_is(leading_space, hash_a, COUNT(hash_a), 6) == 0);
    const uint16_t null_a[] = {0, 'A'};
    CHECK(measure_is(embedded_null, null_a, COUNT(null_a), 6) == 0);
    mc_font_renderer_free(maximum);
    mc_font_renderer_free(empty);
    mc_font_renderer_free(leading_space);
    mc_font_renderer_free(embedded_null);
    for (size_t i = 0; i < COUNT(long_text); ++i)
        long_text[i] = 'A';
    mc_font_text text = {long_text, COUNT(long_text)};
    mc_font_run run;
    CHECK(mc_font_renderer_layout(font, &text, 0, 0, UINT32_C(0xff123456), true, &run) ==
          MC_FONT_RENDERER_OK);
    CHECK(run.count == 131072u && run.width == 327680);
    CHECK(glyph_is(&run.glyphs[65535], 33, 327676, 1, UINT32_C(0xff040d15)));
    CHECK(glyph_is(&run.glyphs[131071], 33, 327675, 0, UINT32_C(0xff123456)));
    const mc_font_glyph *retained = run.glyphs;
    text = (mc_font_text){a, COUNT(a)};
    CHECK(mc_font_renderer_layout(font, &text, 0, 0, 0, false, &run) == MC_FONT_RENDERER_OK);
    CHECK(run.glyphs == retained && run.count == 1 && run.width == 5);
    return 0;
}

int main(void) {
    authored_atlas();
    const mc_image_view atlas = {128, 128, 512, pixels};
    uint16_t borrowed[COUNT(font_txt)];
    memcpy(borrowed, font_txt, sizeof(borrowed));
    mc_font_renderer *font = NULL, *anaglyph = NULL;
    CHECK(mc_font_renderer_create(&atlas, borrowed, COUNT(borrowed), false, &font) ==
          MC_FONT_RENDERER_OK);
    CHECK(mc_font_renderer_create(&atlas, font_txt, COUNT(font_txt), true, &anaglyph) ==
          MC_FONT_RENDERER_OK);
    if (test_measure(font) != 0 || test_layout(font) != 0 || test_palette(font, anaglyph) != 0 ||
        test_failures(font) != 0 || test_bounds(font, &atlas) != 0)
        return 1;
    memset(pixels, 0, sizeof(pixels));
    memset(borrowed, 0, sizeof(borrowed));
    const uint16_t a[] = {'A'};
    CHECK(measure_is(font, a, COUNT(a), 5) == 0);
    mc_font_renderer_free(anaglyph);
    mc_font_renderer_free(font);
    mc_font_renderer_free(NULL);
    printf("PASS: %u FontRenderer source-contract checks\n", checks);
    return 0;
}
