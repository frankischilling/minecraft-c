#include "image_io.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

typedef struct png_input {
    const mc_client_image_io_input *stream;
    mc_client_image_io_error *error;
    const mc_client_image_io_limits *limits;
    size_t consumed, bound;
} png_input;

typedef struct png_palette {
    uint8_t rgb[768], alpha[256];
    size_t colors, alpha_count;
    uint16_t transparent[3];
    bool transparent_rgb;
} png_palette;

typedef struct png_memory {
    size_t used, bound;
    bool limited, failed;
} png_memory;

typedef union png_allocation {
    size_t bytes;
    long double alignment;
    void *pointer_alignment;
} png_allocation;

static uint32_t big32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static bool image_fail(png_input *input, mc_client_image_io_status status, const char *operation) {
    mc_client_image_io_fail(input->error, status, operation);
    return false;
}

static bool read_bytes(png_input *input, uint8_t *out, size_t bytes, bool allow_eof,
                       size_t *actual) {
    if (bytes > input->bound - input->consumed)
        return image_fail(input, MC_CLIENT_IMAGE_IO_LIMIT, "ImageIO native encoded intake bound");
    size_t made = 0;
    while (made < bytes) {
        size_t count = 0;
        const mc_client_image_io_status result = input->stream->read(
            input->stream->context, out + made, bytes - made, &count, input->error);
        if (result != MC_CLIENT_IMAGE_IO_OK) {
            if (count != 0 || input->error->status == MC_CLIENT_IMAGE_IO_OK)
                return image_fail(input, MC_CLIENT_IMAGE_IO_INVALID_ARGUMENT,
                                  "ImageIO malformed byte-source failure");
            return false;
        }
        if (count > bytes - made)
            return image_fail(input, MC_CLIENT_IMAGE_IO_INVALID_ARGUMENT,
                              "ImageIO malformed byte-source count");
        if (count == 0) {
            if (!allow_eof)
                return image_fail(input, MC_CLIENT_IMAGE_IO_SOURCE_IO,
                                  "ImageIO truncated PNG input");
            break;
        }
        made += count;
        input->consumed += count;
    }
    if (actual != NULL)
        *actual = made;
    return true;
}

static bool skip_bytes(png_input *input, size_t bytes) {
    uint8_t block[512];
    while (bytes != 0) {
        const size_t part = bytes < sizeof(block) ? bytes : sizeof(block);
        if (!read_bytes(input, block, part, false, NULL))
            return false;
        bytes -= part;
    }
    return true;
}

static voidpf allocate_png(voidpf context, uInt count, uInt width) {
    png_memory *memory = context;
    if ((size_t)count > SIZE_MAX / (size_t)(width == 0 ? 1 : width)) {
        memory->limited = true;
        return NULL;
    }
    const size_t bytes = (size_t)count * width;
    if (bytes > SIZE_MAX - sizeof(png_allocation) ||
        bytes + sizeof(png_allocation) > memory->bound - memory->used) {
        memory->limited = true;
        return NULL;
    }
    png_allocation *block = calloc(1, sizeof(*block) + bytes);
    if (block == NULL) {
        memory->failed = true;
        return NULL;
    }
    block->bytes = sizeof(*block) + bytes;
    memory->used += block->bytes;
    return block + 1;
}

static void free_png(voidpf context, voidpf address) {
    if (address == NULL)
        return;
    png_memory *memory = context;
    png_allocation *block = (png_allocation *)address - 1;
    memory->used -= block->bytes;
    free(block);
}

static bool metadata(png_input *input, uint32_t type, uint32_t bytes, unsigned int color,
                     png_palette *palette) {
    uint8_t crc[4];
    if (type == UINT32_C(0x504c5445) && color == 3) { /* PLTE */
        if (bytes == 0 || bytes > sizeof(palette->rgb) || bytes % 3 != 0 || palette->colors != 0)
            return image_fail(input, MC_CLIENT_IMAGE_IO_UNSUPPORTED_NATIVE,
                              "ImageIO unqualified PNG palette shape");
        if (!read_bytes(input, palette->rgb, bytes, false, NULL))
            return false;
        palette->colors = bytes / 3;
    } else if (type == UINT32_C(0x74524e53)) { /* tRNS */
        if (color == 3) {
            if (bytes > sizeof(palette->alpha) || palette->alpha_count != 0)
                return image_fail(input, MC_CLIENT_IMAGE_IO_UNSUPPORTED_NATIVE,
                                  "ImageIO unqualified PNG alpha palette");
            if (!read_bytes(input, palette->alpha, bytes, false, NULL))
                return false;
            palette->alpha_count = bytes;
        } else if (color == 2) {
            uint8_t transparent[6];
            if (bytes != sizeof(transparent))
                return image_fail(input, MC_CLIENT_IMAGE_IO_UNSUPPORTED_NATIVE,
                                  "ImageIO unqualified PNG transparent color");
            if (!read_bytes(input, transparent, bytes, false, NULL))
                return false;
            for (size_t i = 0; i < 3; ++i)
                palette->transparent[i] =
                    (uint16_t)(((uint16_t)transparent[i * 2] << 8) | transparent[i * 2 + 1]);
            palette->transparent_rgb = true;
        } else if (!skip_bytes(input, bytes))
            return false;
    } else if (!skip_bytes(input, bytes))
        return false;
    /* The selected ImageIO PNG reader ignores chunk CRCs. Consume the bytes
     * only when its metadata/data traversal reaches them; do not ask mc_image
     * to reinterpret that source policy or globally change its validation. */
    return read_bytes(input, crc, sizeof(crc), false, NULL);
}

static bool collect_palette_data(png_input *input, png_palette *palette, uint8_t **compressed,
                                 size_t *compressed_bytes) {
    size_t used = 0;
    uint8_t *bytes = NULL;
    bool in_data = false, data_closed = false;
    for (size_t chunks = 0; chunks < 1024; ++chunks) {
        uint8_t header[8];
        if (!read_bytes(input, header, sizeof(header), false, NULL))
            goto failed;
        const uint32_t count = big32(header), type = big32(header + 4);
        if (count > INT32_MAX) {
            image_fail(input, MC_CLIENT_IMAGE_IO_SOURCE_IO, "ImageIO negative PNG chunk length");
            goto failed;
        }
        if (type == UINT32_C(0x49454e44)) { /* IEND: source leaves its CRC unread. */
            if (!in_data || palette->colors == 0) {
                image_fail(input, MC_CLIENT_IMAGE_IO_SOURCE_IO, "ImageIO missing PNG palette/data");
                goto failed;
            }
            *compressed = bytes;
            *compressed_bytes = used;
            return true;
        }
        if (type == UINT32_C(0x49444154)) {
            if (data_closed || count > input->bound - used) {
                image_fail(input, MC_CLIENT_IMAGE_IO_UNSUPPORTED_NATIVE,
                           "ImageIO unqualified nonconsecutive/oversize PNG data");
                goto failed;
            }
            in_data = true;
            uint8_t *grown = realloc(bytes, used + count == 0 ? 1 : used + count);
            if (grown == NULL) {
                image_fail(input, MC_CLIENT_IMAGE_IO_ALLOCATION_FAILED, "ImageIO PNG data cache");
                goto failed;
            }
            bytes = grown;
            if (!read_bytes(input, bytes + used, count, false, NULL))
                goto failed;
            used += count;
            if (!skip_bytes(input, 4))
                goto failed;
        } else {
            if (in_data)
                data_closed = true;
            if (!metadata(input, type, count, 3, palette))
                goto failed;
        }
    }
    image_fail(input, MC_CLIENT_IMAGE_IO_LIMIT, "ImageIO native PNG chunk count");
failed:
    free(bytes);
    return false;
}

static bool inflate_rows(png_input *input, uint8_t *rows, size_t required, uint32_t first_data,
                         const uint8_t *cached, size_t cached_bytes, bool palette) {
    png_memory memory = {0, input->limits->max_inflater_bytes, false, false};
    z_stream z = {0};
    z.zalloc = allocate_png;
    z.zfree = free_png;
    z.opaque = &memory;
    if (inflateInit(&z) != Z_OK)
        return image_fail(
            input, memory.limited ? MC_CLIENT_IMAGE_IO_LIMIT : MC_CLIENT_IMAGE_IO_ALLOCATION_FAILED,
            "ImageIO PNG inflater owner");
    uint8_t compressed[512], output[8192];
    size_t made = 0, cached_at = 0;
    uint32_t remaining = first_data;
    bool ok = false;
    size_t chunks = 1;
    while (made < required) {
        if (z.avail_in == 0) {
            size_t bytes;
            if (palette) {
                bytes = cached_bytes - cached_at;
                if (bytes > sizeof(compressed))
                    bytes = sizeof(compressed);
                if (bytes == 0) {
                    image_fail(input, MC_CLIENT_IMAGE_IO_SOURCE_IO,
                               "ImageIO incomplete PNG pixels");
                    goto done;
                }
                memcpy(compressed, cached + cached_at, bytes);
                cached_at += bytes;
            } else {
                if (remaining == 0) {
                    uint8_t next[12]; /* Previous CRC, next length and type. */
                    if (++chunks > 1024) {
                        image_fail(input, MC_CLIENT_IMAGE_IO_LIMIT,
                                   "ImageIO native PNG chunk count");
                        goto done;
                    }
                    if (!read_bytes(input, next, sizeof(next), false, NULL))
                        goto done;
                    remaining = big32(next + 4);
                    if (remaining > INT32_MAX || big32(next + 8) != UINT32_C(0x49444154)) {
                        image_fail(input, MC_CLIENT_IMAGE_IO_SOURCE_IO,
                                   "ImageIO incomplete PNG data");
                        goto done;
                    }
                    if (remaining == 0)
                        continue;
                }
                bytes = remaining < sizeof(compressed) ? remaining : sizeof(compressed);
                if (!read_bytes(input, compressed, bytes, false, NULL))
                    goto done;
                remaining -= (uint32_t)bytes;
            }
            z.next_in = compressed;
            z.avail_in = (uInt)bytes;
        }
        z.next_out = output;
        z.avail_out = sizeof(output);
        const int result = inflate(&z, Z_NO_FLUSH);
        const size_t produced = sizeof(output) - z.avail_out;
        const size_t copied = produced < required - made ? produced : required - made;
        memcpy(rows + made, output, copied);
        made += copied;
        if (result != Z_OK && result != Z_STREAM_END) {
            input->error->zlib_error = result;
            image_fail(input,
                       memory.limited  ? MC_CLIENT_IMAGE_IO_LIMIT
                       : memory.failed ? MC_CLIENT_IMAGE_IO_ALLOCATION_FAILED
                                       : MC_CLIENT_IMAGE_IO_SOURCE_IO,
                       "ImageIO PNG compressed pixels");
            goto done;
        }
        if (result == Z_STREAM_END && made < required) {
            image_fail(input, MC_CLIENT_IMAGE_IO_SOURCE_IO, "ImageIO incomplete PNG scanlines");
            goto done;
        }
        if (produced == 0 && z.avail_in != 0) {
            image_fail(input, MC_CLIENT_IMAGE_IO_UNSUPPORTED_NATIVE,
                       "ImageIO PNG inflater made no progress");
            goto done;
        }
    }
    ok = true;
done:
    (void)inflateEnd(&z);
    return ok;
}

static int paeth(int left, int up, int diagonal) {
    const int p = left + up - diagonal;
    const int a = abs(p - left), b = abs(p - up), c = abs(p - diagonal);
    return a <= b && a <= c ? left : b <= c ? up : diagonal;
}

static bool make_pixels(png_input *input, uint8_t *rows, uint32_t width, uint32_t height,
                        unsigned int color, unsigned int channels, const png_palette *palette,
                        mc_client_argb_image **out) {
    mc_client_argb_image *image = NULL;
    const mc_client_render_engine_status created =
        mc_client_argb_image_create((int32_t)width, (int32_t)height, SIZE_MAX, &image);
    if (created != MC_CLIENT_RENDER_ENGINE_OK)
        return image_fail(input,
                          created == MC_CLIENT_RENDER_ENGINE_LIMIT
                              ? MC_CLIENT_IMAGE_IO_LIMIT
                              : MC_CLIENT_IMAGE_IO_ALLOCATION_FAILED,
                          "ImageIO independent native ARGB owner");
    const size_t stride = (size_t)width * channels;
    for (uint32_t y = 0; y < height; ++y) {
        uint8_t *row = rows + (size_t)y * (stride + 1) + 1;
        const uint8_t filter = row[-1];
        const uint8_t *above = y == 0 ? NULL : row - stride - 1;
        if (filter > 4) {
            image_fail(input, MC_CLIENT_IMAGE_IO_SOURCE_IO, "ImageIO illegal PNG row filter");
            goto failed;
        }
        for (size_t x = 0; x < stride; ++x) {
            const int left = x < channels ? 0 : row[x - channels];
            const int up = above == NULL ? 0 : above[x];
            const int diagonal = above == NULL || x < channels ? 0 : above[x - channels];
            const int predicted = filter == 1   ? left
                                  : filter == 2 ? up
                                  : filter == 3 ? (left + up) / 2
                                  : filter == 4 ? paeth(left, up, diagonal)
                                                : 0;
            row[x] = (uint8_t)((unsigned int)row[x] + (unsigned int)predicted);
        }
        for (uint32_t x = 0; x < width; ++x) {
            const uint8_t *pixel = row + (size_t)x * channels;
            uint32_t r, g, b, a = 255;
            if (color == 3) {
                const size_t index = pixel[0];
                if (index >= palette->colors) {
                    image_fail(input, MC_CLIENT_IMAGE_IO_UNSUPPORTED_NATIVE,
                               "ImageIO unqualified PNG palette index");
                    goto failed;
                }
                r = palette->rgb[index * 3];
                g = palette->rgb[index * 3 + 1];
                b = palette->rgb[index * 3 + 2];
                if (index < palette->alpha_count)
                    a = palette->alpha[index];
            } else {
                r = pixel[0];
                g = pixel[1];
                b = pixel[2];
                if (color == 6)
                    a = pixel[3];
                else if (palette->transparent_rgb && r == palette->transparent[0] &&
                         g == palette->transparent[1] && b == palette->transparent[2])
                    a = 0;
            }
            if (mc_client_argb_image_set_pixel(image, (int32_t)x, (int32_t)y,
                                               (a << 24) | (r << 16) | (g << 8) | b) !=
                MC_CLIENT_RENDER_ENGINE_OK) {
                image_fail(input, MC_CLIENT_IMAGE_IO_PROVIDER_FAILED, "ImageIO ARGB storage write");
                goto failed;
            }
        }
    }
    *out = image;
    return true;
failed:
    mc_client_argb_image_release(image);
    return false;
}

static mc_client_image_io_status read_image(const mc_client_image_io_input *stream,
                                            const mc_client_image_io_limits *limits,
                                            mc_client_argb_image **out,
                                            mc_client_image_io_error *error) {
    if (stream == NULL || stream->is_null)
        return mc_client_image_io_fail(error, MC_CLIENT_IMAGE_IO_SOURCE_ILLEGAL_ARGUMENT,
                                       "ImageIO null input stream");
    if (out == NULL)
        return mc_client_image_io_fail(error, MC_CLIENT_IMAGE_IO_INVALID_ARGUMENT,
                                       "ImageIO native image output");
    if (stream->read == NULL || limits == NULL)
        return mc_client_image_io_fail(error, MC_CLIENT_IMAGE_IO_INVALID_ARGUMENT,
                                       "ImageIO real byte-input/limits");
    png_input input = {stream, error, limits, 0, limits->max_encoded_bytes};
    uint8_t header[33];
    size_t signature_bytes = 0;
    if (!read_bytes(&input, header, 8, true, &signature_bytes))
        return error->status;
    static const uint8_t png_signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (signature_bytes != 8 || memcmp(header, png_signature, 8) != 0) {
        /* Standard JDK plugin discovery can recognize these other families.
         * Their providers are temporary explicit dependencies, not NULL reads. */
        if (signature_bytes != 0 && (header[0] == 0 || header[0] == 0xff || header[0] == 'G' ||
                                     header[0] == 'B' || header[0] == 'I' || header[0] == 'M'))
            return mc_client_image_io_fail(error, MC_CLIENT_IMAGE_IO_UNSUPPORTED_NATIVE,
                                           "ImageIO reached non-PNG provider");
        *out = NULL;
        return MC_CLIENT_IMAGE_IO_OK;
    }
    if (!read_bytes(&input, header + 8, sizeof(header) - 8, false, NULL))
        return error->status;
    const uint32_t width = big32(header + 16), height = big32(header + 20);
    const unsigned int color = header[25];
    if (big32(header + 8) != 13 || big32(header + 12) != UINT32_C(0x49484452) || width == 0 ||
        height == 0 || width > INT32_MAX || height > INT32_MAX)
        return mc_client_image_io_fail(error, MC_CLIENT_IMAGE_IO_SOURCE_IO,
                                       "ImageIO invalid PNG header");
    if (header[24] != 8 || (color != 2 && color != 3 && color != 6) || header[26] != 0 ||
        header[27] != 0 || header[28] != 0)
        return mc_client_image_io_fail(error, MC_CLIENT_IMAGE_IO_UNSUPPORTED_NATIVE,
                                       "ImageIO gray/16-bit/interlaced provider dependency");
    const unsigned int channels = color == 2 ? 3u : color == 6 ? 4u : 1u;
    const size_t maximum = limits->max_decoded_bytes;
    if (width > 8192 || height > 8192 || (size_t)width > SIZE_MAX / height / 4 ||
        (size_t)width * height * 4 > maximum || (size_t)width * channels + 1 > SIZE_MAX / height)
        return mc_client_image_io_fail(error, MC_CLIENT_IMAGE_IO_LIMIT,
                                       "ImageIO native decoded image bound");
    const size_t row_bytes = ((size_t)width * channels + 1) * height;
    uint8_t *rows = malloc(row_bytes), *compressed = NULL;
    if (rows == NULL)
        return mc_client_image_io_fail(error, MC_CLIENT_IMAGE_IO_ALLOCATION_FAILED,
                                       "ImageIO PNG row owner");
    png_palette palette = {0};
    size_t compressed_bytes = 0;
    uint32_t data_bytes = 0;
    bool found = false;
    if (color == 3)
        found = collect_palette_data(&input, &palette, &compressed, &compressed_bytes);
    else {
        for (size_t chunks = 0; chunks < 1024; ++chunks) {
            uint8_t next[8];
            if (!read_bytes(&input, next, sizeof(next), false, NULL))
                break;
            data_bytes = big32(next);
            const uint32_t type = big32(next + 4);
            if (data_bytes > INT32_MAX) {
                image_fail(&input, MC_CLIENT_IMAGE_IO_SOURCE_IO,
                           "ImageIO negative PNG chunk length");
                break;
            }
            if (type == UINT32_C(0x49444154)) {
                found = true;
                break;
            }
            if (!metadata(&input, type, data_bytes, color, &palette))
                break;
        }
        if (!found && error->status == MC_CLIENT_IMAGE_IO_OK)
            image_fail(&input, MC_CLIENT_IMAGE_IO_LIMIT, "ImageIO native PNG metadata count");
    }
    if (found &&
        inflate_rows(&input, rows, row_bytes, data_bytes, compressed, compressed_bytes, color == 3))
        make_pixels(&input, rows, width, height, color, channels, &palette, out);
    free(compressed);
    free(rows);
    return error->status;
}

mc_client_image_io_status mc_client_image_io_read(const mc_client_image_io_input *stream,
                                                  const mc_client_image_io_limits *limits,
                                                  mc_client_argb_image **out,
                                                  mc_client_image_io_error *error) {
    mc_client_image_io_error detail = {0};
    const mc_client_image_io_status status = read_image(stream, limits, out, &detail);
    if (error != NULL)
        *error = detail;
    return status;
}
