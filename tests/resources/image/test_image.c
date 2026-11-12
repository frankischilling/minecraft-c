#include "mc/resources/image/image.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

enum { FIXTURE_CAPACITY = 128 * 1024 };

typedef struct png_buffer {
    uint8_t bytes[FIXTURE_CAPACITY];
    size_t length;
} png_buffer;

typedef struct png_input {
    uint32_t width, height;
    uint8_t depth, color, interlace;
    const uint8_t *rows;
    size_t row_bytes;
    const uint8_t *palette;
    size_t palette_bytes;
    const uint8_t *transparency;
    size_t transparency_bytes;
} png_input;

static unsigned int checks;

static bool check(bool value, const char *expression, unsigned int line) {
    ++checks;
    if (!value)
        fprintf(stderr, "%s:%u: failed: %s\n", __FILE__, line, expression);
    return value;
}

#define CHECK(expression) check((expression), #expression, __LINE__)
#define REQUIRE(expression)                                                                        \
    do {                                                                                           \
        if (!CHECK(expression))                                                                    \
            goto cleanup;                                                                          \
    } while (0)

static void write_integer(uint8_t *out, uint32_t value) {
    out[0] = (uint8_t)(value >> 24);
    out[1] = (uint8_t)(value >> 16);
    out[2] = (uint8_t)(value >> 8);
    out[3] = (uint8_t)value;
}

/* These fixtures author PNG framing and unfiltered scanlines directly. Only
 * zlib compression/CRC are delegated; no PNG encoder or game asset is used. */
static bool chunk(png_buffer *out, const char *type, const uint8_t *data, size_t length) {
    if (length > UINT32_MAX || length > UINT_MAX || out->length > FIXTURE_CAPACITY - 12u ||
        length > FIXTURE_CAPACITY - out->length - 12u)
        return false;
    uint8_t *next = out->bytes + out->length;
    write_integer(next, (uint32_t)length);
    memcpy(next + 4, type, 4);
    if (length != 0)
        memcpy(next + 8, data, length);
    uLong checksum = crc32(0, next + 4, 4);
    if (length != 0)
        checksum = crc32(checksum, data, (uInt)length);
    write_integer(next + 8 + length, (uint32_t)checksum);
    out->length += length + 12;
    return true;
}

static bool begin(png_buffer *out, const png_input *input) {
    static const uint8_t signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
    memcpy(out->bytes, signature, sizeof(signature));
    out->length = sizeof(signature);
    uint8_t header[13] = {0};
    write_integer(header, input->width);
    write_integer(header + 4, input->height);
    header[8] = input->depth;
    header[9] = input->color;
    header[12] = input->interlace;
    return chunk(out, "IHDR", header, sizeof(header));
}

static bool image_data(png_buffer *out, const png_input *input) {
    uint8_t compressed[4096];
    uLongf length = sizeof(compressed);
    const uLong input_bytes = (uLong)input->row_bytes;
    if ((size_t)input_bytes != input->row_bytes ||
        compress2(compressed, &length, input->rows, input_bytes, Z_BEST_SPEED) != Z_OK)
        return false;
    /* Two real IDAT chunks also exercise concatenated compressed-stream input. */
    const size_t first = (size_t)length / 2u;
    return chunk(out, "IDAT", compressed, first) &&
           chunk(out, "IDAT", compressed + first, (size_t)length - first);
}

static bool make_png(png_buffer *out, const png_input *input) {
    return begin(out, input) &&
           (input->palette_bytes == 0 ||
            chunk(out, "PLTE", input->palette, input->palette_bytes)) &&
           (input->transparency_bytes == 0 ||
            chunk(out, "tRNS", input->transparency, input->transparency_bytes)) &&
           image_data(out, input) && chunk(out, "IEND", NULL, 0);
}

static bool pixels(const png_buffer *source, uint32_t width, uint32_t height,
                   const uint8_t *expected, size_t expected_bytes) {
    mc_image *image = NULL;
    bool result =
        CHECK(mc_image_decode_png(source->bytes, source->length, NULL, &image) == MC_IMAGE_OK);
    mc_image_view view = {0};
    if (result)
        result = CHECK(image != NULL && mc_image_view_get(image, &view));
    if (result)
        result = CHECK(view.width == width && view.height == height &&
                       view.stride == (size_t)width * 4u &&
                       view.stride * (size_t)height == expected_bytes && view.pixels != NULL &&
                       memcmp(view.pixels, expected, expected_bytes) == 0);
    mc_image_free(image);
    return result;
}

static bool rejected(const uint8_t *encoded, size_t length, const mc_image_limits *limits,
                     mc_image_status expected, mc_image *held) {
    mc_image *out = held;
    const mc_image_status status = mc_image_decode_png(encoded, length, limits, &out);
    const bool result = CHECK(status == expected && out == held);
    if (out != held)
        mc_image_free(out);
    return result;
}

static bool color_cases(png_buffer *buffer) {
    bool result = false;
    static const uint8_t rgb_rows[] = {0, 255, 0, 0, 0, 255, 0, 0, 0, 0, 255, 255, 255, 255};
    static const uint8_t rgb_pixels[] = {255, 0, 0,   255, 0,   255, 0,   255,
                                         0,   0, 255, 255, 255, 255, 255, 255};
    png_input input = {.width = 2,
                       .height = 2,
                       .depth = 8,
                       .color = 2,
                       .rows = rgb_rows,
                       .row_bytes = sizeof(rgb_rows)};
    REQUIRE(make_png(buffer, &input));
    REQUIRE(pixels(buffer, 2, 2, rgb_pixels, sizeof(rgb_pixels)));

    static const uint8_t rgba_rows[] = {0, 90, 80, 70, 0, 1, 2, 3, 128};
    static const uint8_t rgba_pixels[] = {90, 80, 70, 0, 1, 2, 3, 128};
    input = (png_input){.width = 2,
                        .height = 1,
                        .depth = 8,
                        .color = 6,
                        .rows = rgba_rows,
                        .row_bytes = sizeof(rgba_rows)};
    REQUIRE(make_png(buffer, &input));
    REQUIRE(pixels(buffer, 2, 1, rgba_pixels, sizeof(rgba_pixels)));

    static const uint8_t palette[] = {255, 0, 0, 0, 255, 0, 0, 0, 255};
    static const uint8_t alpha[] = {0, 128};
    static const uint8_t indices[] = {0, 0x18}; /* Three packed 2-bit indices: 0,1,2. */
    static const uint8_t indexed_pixels[] = {255, 0, 0, 0, 0, 255, 0, 128, 0, 0, 255, 255};
    input = (png_input){.width = 3,
                        .height = 1,
                        .depth = 2,
                        .color = 3,
                        .rows = indices,
                        .row_bytes = sizeof(indices),
                        .palette = palette,
                        .palette_bytes = sizeof(palette),
                        .transparency = alpha,
                        .transparency_bytes = sizeof(alpha)};
    REQUIRE(make_png(buffer, &input));
    REQUIRE(pixels(buffer, 3, 1, indexed_pixels, sizeof(indexed_pixels)));
    input.transparency_bytes = 0;
    static const uint8_t opaque_palette[] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255};
    REQUIRE(make_png(buffer, &input));
    REQUIRE(pixels(buffer, 3, 1, opaque_palette, sizeof(opaque_palette)));

    static const uint8_t transparent_rgb[] = {0, 1, 0, 2, 0, 3};
    static const uint8_t transparent_rows[] = {0, 1, 2, 3, 3, 2, 1};
    static const uint8_t transparent_pixels[] = {1, 2, 3, 0, 3, 2, 1, 255};
    input = (png_input){.width = 2,
                        .height = 1,
                        .depth = 8,
                        .color = 2,
                        .rows = transparent_rows,
                        .row_bytes = sizeof(transparent_rows),
                        .transparency = transparent_rgb,
                        .transparency_bytes = sizeof(transparent_rgb)};
    REQUIRE(make_png(buffer, &input));
    REQUIRE(pixels(buffer, 2, 1, transparent_pixels, sizeof(transparent_pixels)));

    static const uint8_t rgb16[] = {0, 0x12, 0x34, 0xab, 0xcd, 0x00, 0xff};
    static const uint8_t rgb16_pixels[] = {0x12, 0xab, 0, 255};
    input = (png_input){.width = 1,
                        .height = 1,
                        .depth = 16,
                        .color = 2,
                        .rows = rgb16,
                        .row_bytes = sizeof(rgb16)};
    REQUIRE(make_png(buffer, &input));
    REQUIRE(pixels(buffer, 1, 1, rgb16_pixels, sizeof(rgb16_pixels)));

    static const uint8_t rgba16[] = {0, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    static const uint8_t rgba16_pixels[] = {0x11, 0x33, 0x55, 0x77};
    input = (png_input){.width = 1,
                        .height = 1,
                        .depth = 16,
                        .color = 6,
                        .rows = rgba16,
                        .row_bytes = sizeof(rgba16)};
    REQUIRE(make_png(buffer, &input));
    REQUIRE(pixels(buffer, 1, 1, rgba16_pixels, sizeof(rgba16_pixels)));

    /* Adam7 2x2 RGB: pass1 red, pass6 green, pass7 blue/white; other passes empty. */
    static const uint8_t adam7[] = {0, 255, 0, 0, 0, 0, 255, 0, 0, 0, 0, 255, 255, 255, 255};
    input = (png_input){.width = 2,
                        .height = 2,
                        .depth = 8,
                        .color = 2,
                        .interlace = 1,
                        .rows = adam7,
                        .row_bytes = sizeof(adam7)};
    REQUIRE(make_png(buffer, &input));
    REQUIRE(pixels(buffer, 2, 2, rgb_pixels, sizeof(rgb_pixels)));
    result = true;
cleanup:
    return result;
}

static bool gray_cases(png_buffer *buffer) {
    bool result = false;
    static const uint8_t gray1[] = {0, 0xa0};
    static const uint8_t gray1_pixels[] = {255, 255, 255, 255, 0, 0, 0, 255, 255, 255, 255, 255};
    png_input input = {
        .width = 3, .height = 1, .depth = 1, .color = 0, .rows = gray1, .row_bytes = sizeof(gray1)};
    REQUIRE(make_png(buffer, &input));
    REQUIRE(pixels(buffer, 3, 1, gray1_pixels, sizeof(gray1_pixels)));
    static const uint8_t gray2[] = {0, 0x60};
    static const uint8_t gray2_pixels[] = {85, 85, 85, 255, 170, 170, 170, 255};
    input.width = 2;
    input.depth = 2;
    input.rows = gray2;
    input.row_bytes = sizeof(gray2);
    REQUIRE(make_png(buffer, &input));
    REQUIRE(pixels(buffer, 2, 1, gray2_pixels, sizeof(gray2_pixels)));
    static const uint8_t gray4[] = {0, 0x3c};
    static const uint8_t gray4_pixels[] = {51, 51, 51, 255, 204, 204, 204, 255};
    input.depth = 4;
    input.rows = gray4;
    input.row_bytes = sizeof(gray4);
    REQUIRE(make_png(buffer, &input));
    REQUIRE(pixels(buffer, 2, 1, gray4_pixels, sizeof(gray4_pixels)));

    static const uint8_t gray8[] = {0, 0, 128};
    static const uint8_t transparent[] = {0, 0};
    static const uint8_t gray8_pixels[] = {0, 0, 0, 0, 128, 128, 128, 255};
    input.depth = 8;
    input.rows = gray8;
    input.row_bytes = sizeof(gray8);
    input.transparency = transparent;
    input.transparency_bytes = sizeof(transparent);
    REQUIRE(make_png(buffer, &input));
    REQUIRE(pixels(buffer, 2, 1, gray8_pixels, sizeof(gray8_pixels)));

    static const uint8_t gray_alpha[] = {0, 32, 64};
    static const uint8_t gray_alpha_pixels[] = {32, 32, 32, 64};
    input = (png_input){.width = 1,
                        .height = 1,
                        .depth = 8,
                        .color = 4,
                        .rows = gray_alpha,
                        .row_bytes = sizeof(gray_alpha)};
    REQUIRE(make_png(buffer, &input));
    REQUIRE(pixels(buffer, 1, 1, gray_alpha_pixels, sizeof(gray_alpha_pixels)));

    static const uint8_t gray16[] = {0, 0x12, 0x34, 0x12, 0xff};
    static const uint8_t transparent16[] = {0x12, 0x34};
    static const uint8_t gray16_pixels[] = {0x12, 0x12, 0x12, 0, 0x12, 0x12, 0x12, 255};
    input = (png_input){.width = 2,
                        .height = 1,
                        .depth = 16,
                        .color = 0,
                        .rows = gray16,
                        .row_bytes = sizeof(gray16),
                        .transparency = transparent16,
                        .transparency_bytes = sizeof(transparent16)};
    REQUIRE(make_png(buffer, &input));
    REQUIRE(pixels(buffer, 2, 1, gray16_pixels, sizeof(gray16_pixels)));
    result = true;
cleanup:
    return result;
}

static bool boundaries(png_buffer *buffer, png_buffer *changed) {
    bool result = false;
    mc_image *held = NULL, *second = NULL;
    uint8_t *large = NULL;
    static const uint8_t rows[] = {0, 90, 80, 70, 0};
    static const uint8_t expected[] = {90, 80, 70, 0};
    const png_input input = {
        .width = 1, .height = 1, .depth = 8, .color = 6, .rows = rows, .row_bytes = sizeof(rows)};
    REQUIRE(make_png(buffer, &input));
    REQUIRE(mc_image_decode_png(buffer->bytes, buffer->length, NULL, &held) == MC_IMAGE_OK);
    REQUIRE(mc_image_decode_png(buffer->bytes, buffer->length, NULL, &second) == MC_IMAGE_OK);
    mc_image_view first_view = {0}, second_view = {0};
    REQUIRE(mc_image_view_get(held, &first_view) && mc_image_view_get(second, &second_view));
    REQUIRE(held != second && first_view.pixels != second_view.pixels);
    mc_image_free(second);
    second = NULL;
    REQUIRE(memcmp(first_view.pixels, expected, sizeof(expected)) == 0);
    mc_image_view unchanged = first_view;
    REQUIRE(!mc_image_view_get(NULL, &unchanged) && unchanged.pixels == first_view.pixels &&
            unchanged.width == first_view.width);
    REQUIRE(!mc_image_view_get(held, NULL));
    REQUIRE(mc_image_decode_png(buffer->bytes, buffer->length, NULL, NULL) ==
            MC_IMAGE_INVALID_ARGUMENT);
    REQUIRE(rejected(NULL, 1, NULL, MC_IMAGE_INVALID_ARGUMENT, held));
    REQUIRE(rejected(NULL, 0, NULL, MC_IMAGE_TRUNCATED, held));
    for (size_t length = 0; length < buffer->length; ++length)
        REQUIRE(rejected(buffer->bytes, length, NULL, MC_IMAGE_TRUNCATED, held));

    mc_image_limits limits = mc_image_default_limits();
    limits.max_encoded_bytes = buffer->length - 1;
    REQUIRE(rejected(buffer->bytes, buffer->length, &limits, MC_IMAGE_LIMIT, held));
    limits = mc_image_default_limits();
    limits.max_decoded_bytes = 3;
    REQUIRE(rejected(buffer->bytes, buffer->length, &limits, MC_IMAGE_LIMIT, held));
    limits = mc_image_default_limits();
    limits.max_dimension = 0;
    REQUIRE(rejected(buffer->bytes, buffer->length, &limits, MC_IMAGE_LIMIT, held));
    limits = mc_image_default_limits();
    limits.max_allocation_bytes = 0;
    REQUIRE(rejected(buffer->bytes, buffer->length, &limits, MC_IMAGE_LIMIT, held));
    limits.max_allocation_bytes = 4096;
    REQUIRE(rejected(buffer->bytes, buffer->length, &limits, MC_IMAGE_LIMIT, held));
    REQUIRE(pixels(buffer, 1, 1, expected, sizeof(expected)));

    *changed = *buffer;
    changed->bytes[0] = 0;
    REQUIRE(rejected(changed->bytes, changed->length, NULL, MC_IMAGE_INVALID_PNG, held));
    *changed = *buffer;
    changed->bytes[29] ^= 1; /* IHDR CRC. */
    REQUIRE(rejected(changed->bytes, changed->length, NULL, MC_IMAGE_INVALID_PNG, held));
    *changed = *buffer;
    changed->bytes[changed->length++] = 0;
    REQUIRE(rejected(changed->bytes, changed->length, NULL, MC_IMAGE_INVALID_PNG, held));
    static const uint8_t broken_stream[] = {1, 2, 3};
    REQUIRE(begin(changed, &input) &&
            chunk(changed, "IDAT", broken_stream, sizeof(broken_stream)) &&
            chunk(changed, "IEND", NULL, 0));
    REQUIRE(rejected(changed->bytes, changed->length, NULL, MC_IMAGE_INVALID_PNG, held));
    static const uint8_t bad_filter[] = {5, 90, 80, 70, 0};
    png_input invalid_filter = input;
    invalid_filter.rows = bad_filter;
    invalid_filter.row_bytes = sizeof(bad_filter);
    REQUIRE(make_png(changed, &invalid_filter));
    REQUIRE(rejected(changed->bytes, changed->length, NULL, MC_IMAGE_INVALID_PNG, held));
    REQUIRE(begin(changed, &input) && chunk(changed, "ABCD", NULL, 0) &&
            image_data(changed, &input) && chunk(changed, "IEND", NULL, 0));
    REQUIRE(rejected(changed->bytes, changed->length, NULL, MC_IMAGE_INVALID_PNG, held));

    png_input huge = input;
    huge.width = UINT32_C(0x7fffffff);
    REQUIRE(begin(changed, &huge) && image_data(changed, &input) &&
            chunk(changed, "IEND", NULL, 0));
    REQUIRE(rejected(changed->bytes, changed->length, NULL, MC_IMAGE_LIMIT, held));
    huge.width = 0;
    REQUIRE(begin(changed, &huge) && image_data(changed, &input) &&
            chunk(changed, "IEND", NULL, 0));
    REQUIRE(rejected(changed->bytes, changed->length, NULL, MC_IMAGE_INVALID_PNG, held));
    huge = input;
    huge.depth = 3;
    REQUIRE(make_png(changed, &huge));
    REQUIRE(rejected(changed->bytes, changed->length, NULL, MC_IMAGE_INVALID_PNG, held));

    /* Metadata is CRC-checked but ignored: neither gamma conversion nor text
     * profile decompression may alter raw straight-alpha channel values. */
    static const uint8_t gamma[] = {0, 0, 0xb1, 0x8f};
    static const uint8_t compressed_metadata[] = {'k', 0, 0, 1, 2, 3};
    REQUIRE(begin(changed, &input) && chunk(changed, "gAMA", gamma, sizeof(gamma)) &&
            chunk(changed, "iCCP", compressed_metadata, sizeof(compressed_metadata)) &&
            chunk(changed, "zTXt", compressed_metadata, sizeof(compressed_metadata)) &&
            image_data(changed, &input) && chunk(changed, "IEND", NULL, 0));
    REQUIRE(pixels(changed, 1, 1, expected, sizeof(expected)));
    changed->bytes[33 + 8 + sizeof(gamma)] ^= 1; /* Ignored gAMA CRC. */
    REQUIRE(rejected(changed->bytes, changed->length, NULL, MC_IMAGE_INVALID_PNG, held));

    large = calloc(65537, 1);
    REQUIRE(large != NULL);
    REQUIRE(begin(changed, &input) && image_data(changed, &input) &&
            chunk(changed, "tEXt", large, 65536) && chunk(changed, "IEND", NULL, 0));
    REQUIRE(pixels(changed, 1, 1, expected, sizeof(expected)));
    REQUIRE(begin(changed, &input) && image_data(changed, &input) &&
            chunk(changed, "tEXt", large, 65537) && chunk(changed, "IEND", NULL, 0));
    REQUIRE(rejected(changed->bytes, changed->length, NULL, MC_IMAGE_LIMIT, held));
    REQUIRE(begin(changed, &input) && image_data(changed, &input));
    /* IHDR + two IDAT + 1020 metadata + IEND is exactly 1024 chunks. */
    for (unsigned int index = 0; index < 1020; ++index)
        REQUIRE(chunk(changed, "tEXt", NULL, 0));
    REQUIRE(chunk(changed, "IEND", NULL, 0));
    REQUIRE(pixels(changed, 1, 1, expected, sizeof(expected)));
    changed->length -= 12;
    REQUIRE(chunk(changed, "tEXt", NULL, 0) && chunk(changed, "IEND", NULL, 0));
    REQUIRE(rejected(changed->bytes, changed->length, NULL, MC_IMAGE_LIMIT, held));
    result = true;
cleanup:
    free(large);
    mc_image_free(second);
    mc_image_free(held);
    return result;
}

int main(void) {
    png_buffer *buffer = calloc(1, sizeof(*buffer));
    png_buffer *changed = calloc(1, sizeof(*changed));
    const bool ok = CHECK(buffer != NULL && changed != NULL) && color_cases(buffer) &&
                    gray_cases(buffer) && boundaries(buffer, changed);
    free(changed);
    free(buffer);
    mc_image_free(NULL);
    if (!ok)
        return 1;
    printf("image: %u checks\n", checks);
    return 0;
}
