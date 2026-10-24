#include "mc/resources/image/image.h"

#include <png.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#if !defined(PNG_USER_MEM_SUPPORTED) || !defined(PNG_SET_USER_LIMITS_SUPPORTED) ||                 \
    !defined(PNG_HANDLE_AS_UNKNOWN_SUPPORTED)
#error "The image decoder requires libpng memory, limit and metadata controls"
#endif

enum { IMAGE_CHUNKS = 1024, IMAGE_ANCILLARY_BYTES = 65536 };

struct mc_image {
    uint32_t width, height;
    size_t stride;
    uint8_t *pixels;
};

typedef union image_allocation image_allocation;

union image_allocation {
    /* Some Windows C runtimes omit max_align_t. Fundamental scalar alignment
     * and a 32-byte extent also preserve malloc's x64 vector alignment. */
    long double scalar_alignment;
    void *pointer_alignment;
    uint64_t extent_alignment[4];

    struct {
        image_allocation *previous, *next;
        size_t bytes;
    } record;
};

_Static_assert(sizeof(image_allocation) % 16u == 0, "preserve x64 allocation alignment");

typedef struct image_decode {
    jmp_buf jump;
    mc_image_status status;
    mc_image_limits limits;
    const uint8_t *encoded;
    size_t length, position, live_bytes;
    image_allocation *allocations;
    png_structp png;
    png_infop info;
    mc_image *image;
} image_decode;

mc_image_limits mc_image_default_limits(void) {
    return (mc_image_limits){.max_encoded_bytes = 16u * 1024u * 1024u,
                             .max_decoded_bytes = 64u * 1024u * 1024u,
                             .max_allocation_bytes = 96u * 1024u * 1024u,
                             .max_dimension = 8192};
}

const char *mc_image_status_string(mc_image_status status) {
    switch (status) {
        case MC_IMAGE_OK:
            return "ok";
        case MC_IMAGE_INVALID_ARGUMENT:
            return "invalid image argument";
        case MC_IMAGE_INVALID_PNG:
            return "invalid PNG";
        case MC_IMAGE_TRUNCATED:
            return "truncated PNG";
        case MC_IMAGE_LIMIT:
            return "image resource limit";
        case MC_IMAGE_ALLOCATION_FAILED:
            return "image allocation failed";
    }
    return "unknown image status";
}

static uint32_t big_endian(const uint8_t *bytes) {
    return (uint32_t)bytes[0] << 24 | (uint32_t)bytes[1] << 16 | (uint32_t)bytes[2] << 8 |
           (uint32_t)bytes[3];
}

static uint32_t crc(const uint8_t *bytes, size_t length) {
    uint32_t value = UINT32_MAX;
    for (size_t i = 0; i < length; ++i) {
        value ^= bytes[i];
        for (unsigned int bit = 0; bit < 8; ++bit)
            value = (value >> 1) ^ ((value & 1u) != 0 ? UINT32_C(0xedb88320) : 0);
    }
    return value ^ UINT32_MAX;
}

static bool chunk_is(const uint8_t *type, const char *name) {
    return memcmp(type, name, 4) == 0;
}

static bool product(size_t first, size_t second, size_t *out) {
    if (first != 0 && second > SIZE_MAX / first)
        return false;
    *out = first * second;
    return true;
}

/* Validate bounded framing before any allocation. Libpng still validates the
 * pixel format, chunk ordering and complete compressed stream. */
static mc_image_status preflight(const uint8_t *bytes, size_t length, const mc_image_limits *limits,
                                 uint32_t *width, uint32_t *height, size_t *decoded) {
    static const uint8_t signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (length > limits->max_encoded_bytes)
        return MC_IMAGE_LIMIT;
    if (length < sizeof(signature))
        return MC_IMAGE_TRUNCATED;
    if (memcmp(bytes, signature, sizeof(signature)) != 0)
        return MC_IMAGE_INVALID_PNG;
    size_t position = sizeof(signature), count = 0;
    bool idat = false;
    while (position < length) {
        if (length - position < 12)
            return MC_IMAGE_TRUNCATED;
        const size_t size = big_endian(bytes + position);
        const uint8_t *type = bytes + position + 4;
        if (size > INT32_MAX)
            return MC_IMAGE_INVALID_PNG;
        if (size > length - position - 12)
            return MC_IMAGE_TRUNCATED;
        if (++count > IMAGE_CHUNKS)
            return MC_IMAGE_LIMIT;
        for (size_t i = 0; i < 4; ++i)
            if (!((type[i] >= 'A' && type[i] <= 'Z') || (type[i] >= 'a' && type[i] <= 'z')))
                return MC_IMAGE_INVALID_PNG;
        if ((type[2] & 32u) != 0)
            return MC_IMAGE_INVALID_PNG;
        if ((type[0] & 32u) != 0 && size > IMAGE_ANCILLARY_BYTES)
            return MC_IMAGE_LIMIT;
        if (crc(type, size + 4) != big_endian(type + 4 + size))
            return MC_IMAGE_INVALID_PNG;
        if (count == 1) {
            if (!chunk_is(type, "IHDR") || size != 13)
                return MC_IMAGE_INVALID_PNG;
            *width = big_endian(type + 4);
            *height = big_endian(type + 8);
            if (*width == 0 || *height == 0 || *width > INT32_MAX || *height > INT32_MAX)
                return MC_IMAGE_INVALID_PNG;
            if (*width > limits->max_dimension || *height > limits->max_dimension)
                return MC_IMAGE_LIMIT;
            size_t stride;
            if (!product((size_t)*width, 4u, &stride) || !product(stride, (size_t)*height, decoded))
                return MC_IMAGE_LIMIT;
            if (*decoded > limits->max_decoded_bytes || *decoded > SIZE_MAX - sizeof(mc_image))
                return MC_IMAGE_LIMIT;
        } else if (chunk_is(type, "IHDR")) {
            return MC_IMAGE_INVALID_PNG;
        }
        if (chunk_is(type, "IDAT"))
            idat = true;
        if ((type[0] & 32u) == 0 && !chunk_is(type, "IHDR") && !chunk_is(type, "PLTE") &&
            !chunk_is(type, "IDAT") && !chunk_is(type, "IEND"))
            return MC_IMAGE_INVALID_PNG;
        position += size + 12;
        if (chunk_is(type, "IEND"))
            return size == 0 && idat && position == length ? MC_IMAGE_OK : MC_IMAGE_INVALID_PNG;
    }
    return MC_IMAGE_TRUNCATED;
}

static void first_failure(image_decode *state, mc_image_status status) {
    if (state->status == MC_IMAGE_OK)
        state->status = status;
}

static void PNGCBAPI image_error(png_structp png, png_const_charp message) {
    image_decode *state = png_get_error_ptr(png);
    (void)message;
    first_failure(state, MC_IMAGE_INVALID_PNG);
    longjmp(state->jump, 1);
}

static void PNGCBAPI image_warning(png_structp png, png_const_charp message) {
    /* Reject recoverable malformed input rather than publishing repaired pixels.
     * The first allocator/read cause remains more specific than this warning. */
    image_error(png, message);
}

static png_voidp PNGCBAPI image_allocate(png_structp png, png_alloc_size_t requested) {
    image_decode *state = png_get_mem_ptr(png);
    const size_t bytes = (size_t)requested;
    if (requested == 0)
        return NULL;
    if ((png_alloc_size_t)bytes != requested || bytes > SIZE_MAX - sizeof(image_allocation) ||
        bytes + sizeof(image_allocation) > state->limits.max_allocation_bytes - state->live_bytes) {
        first_failure(state, MC_IMAGE_LIMIT);
        return NULL;
    }
    const size_t charge = bytes + sizeof(image_allocation);
    image_allocation *allocation = malloc(charge);
    if (allocation == NULL) {
        first_failure(state, MC_IMAGE_ALLOCATION_FAILED);
        return NULL;
    }
    allocation->record.bytes = charge;
    allocation->record.previous = NULL;
    allocation->record.next = state->allocations;
    if (state->allocations != NULL)
        state->allocations->record.previous = allocation;
    state->allocations = allocation;
    state->live_bytes += charge;
    return allocation + 1;
}

static void PNGCBAPI image_release(png_structp png, png_voidp pointer) {
    if (pointer == NULL)
        return;
    image_decode *state = png_get_mem_ptr(png);
    image_allocation *allocation = (image_allocation *)pointer - 1;
    if (allocation->record.previous != NULL)
        allocation->record.previous->record.next = allocation->record.next;
    else
        state->allocations = allocation->record.next;
    if (allocation->record.next != NULL)
        allocation->record.next->record.previous = allocation->record.previous;
    state->live_bytes -= allocation->record.bytes;
    free(allocation);
}

static void PNGCBAPI image_read(png_structp png, png_bytep destination, size_t count) {
    image_decode *state = png_get_io_ptr(png);
    if (count > state->length - state->position) {
        first_failure(state, MC_IMAGE_TRUNCATED);
        png_error(png, "bounded PNG input exhausted");
    }
    memcpy(destination, state->encoded + state->position, count);
    state->position += count;
}

static void decode_free(image_decode *state) {
    if (state->png != NULL)
        png_destroy_read_struct(&state->png, &state->info, NULL);
    /* A creation-time longjmp can precede publication of png into state.
     * Every allocation is still in this heap ledger, independent of libpng. */
    while (state->allocations != NULL) {
        image_allocation *allocation = state->allocations;
        state->allocations = allocation->record.next;
        free(allocation);
    }
    free(state->image);
    free(state);
}

mc_image_status mc_image_decode_png(const uint8_t *encoded, size_t length,
                                    const mc_image_limits *requested, mc_image **out) {
    if (out == NULL || (encoded == NULL && length != 0))
        return MC_IMAGE_INVALID_ARGUMENT;
    const mc_image_limits limits = requested == NULL ? mc_image_default_limits() : *requested;
    uint32_t width = 0, height = 0;
    size_t decoded = 0;
    mc_image_status status = preflight(encoded, length, &limits, &width, &height, &decoded);
    if (status != MC_IMAGE_OK)
        return status;
    if (limits.max_allocation_bytes < sizeof(image_decode))
        return MC_IMAGE_LIMIT;
    image_decode *const state = calloc(1, sizeof(*state));
    if (state == NULL)
        return MC_IMAGE_ALLOCATION_FAILED;
    state->limits = limits;
    state->live_bytes = sizeof(*state);
    state->encoded = encoded;
    state->length = length;
    /* Only state has mutable cleanup/error/accounting fields. The const local
     * pointer is never changed between setjmp and a possible longjmp. */
    if (setjmp(state->jump) != 0) {
        const mc_image_status failed = state->status;
        decode_free(state);
        return failed;
    }
    state->png = png_create_read_struct_2(PNG_LIBPNG_VER_STRING, state, image_error, image_warning,
                                          state, image_allocate, image_release);
    if (state->png == NULL) {
        first_failure(state, MC_IMAGE_ALLOCATION_FAILED);
        const mc_image_status failed = state->status;
        decode_free(state);
        return failed;
    }
    state->info = png_create_info_struct(state->png);
    if (state->info == NULL) {
        first_failure(state, MC_IMAGE_ALLOCATION_FAILED);
        image_error(state->png, "PNG info allocation failed");
    }
    png_set_read_fn(state->png, state, image_read);
    png_set_user_limits(state->png, limits.max_dimension, limits.max_dimension);
    png_set_chunk_cache_max(state->png, 1);
    png_set_chunk_malloc_max(state->png, IMAGE_ANCILLARY_BYTES);
    png_set_crc_action(state->png, PNG_CRC_ERROR_QUIT, PNG_CRC_ERROR_QUIT);
    /* The documented negative count excludes IHDR/PLTE/tRNS/IDAT/IEND from
     * this discard policy. Text and color profiles are never decompressed. */
    png_set_keep_unknown_chunks(state->png, PNG_HANDLE_CHUNK_NEVER, NULL, -1);
    png_read_info(state->png, state->info);
    const int depth = png_get_bit_depth(state->png, state->info);
    const int color = png_get_color_type(state->png, state->info);
    const bool transparency = png_get_valid(state->png, state->info, PNG_INFO_tRNS) != 0;
    if (depth == 16)
        png_set_strip_16(state->png);
    if (color == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(state->png);
    if (color == PNG_COLOR_TYPE_GRAY && depth < 8)
        png_set_expand_gray_1_2_4_to_8(state->png);
    if (transparency)
        png_set_tRNS_to_alpha(state->png);
    if (color == PNG_COLOR_TYPE_GRAY || color == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(state->png);
    if ((color & PNG_COLOR_MASK_ALPHA) == 0 && !transparency)
        png_set_add_alpha(state->png, 255, PNG_FILLER_AFTER);
    const int passes = png_set_interlace_handling(state->png);
    png_read_update_info(state->png, state->info);
    const size_t stride = (size_t)width * 4u;
    if (png_get_image_width(state->png, state->info) != width ||
        png_get_image_height(state->png, state->info) != height ||
        png_get_bit_depth(state->png, state->info) != 8 ||
        png_get_channels(state->png, state->info) != 4 ||
        png_get_rowbytes(state->png, state->info) != stride)
        image_error(state->png, "unexpected transformed PNG shape");
    const size_t image_bytes = sizeof(mc_image) + decoded;
    if (image_bytes > state->limits.max_allocation_bytes - state->live_bytes) {
        first_failure(state, MC_IMAGE_LIMIT);
        image_error(state->png, "RGBA allocation limit");
    }
    state->image = malloc(image_bytes);
    if (state->image == NULL) {
        first_failure(state, MC_IMAGE_ALLOCATION_FAILED);
        image_error(state->png, "RGBA allocation failed");
    }
    state->live_bytes += image_bytes;
    state->image->width = width;
    state->image->height = height;
    state->image->stride = stride;
    state->image->pixels = (uint8_t *)(state->image + 1);
    memset(state->image->pixels, 0, decoded);
    for (int pass = 0; pass < passes; ++pass)
        for (uint32_t row = 0; row < height; ++row)
            png_read_row(state->png, state->image->pixels + (size_t)row * stride, NULL);
    png_read_end(state->png, NULL);
    if (state->position != length)
        image_error(state->png, "PNG input was not consumed");
    if (state->status != MC_IMAGE_OK)
        image_error(state->png, "PNG allocation refusal was recovered internally");
    mc_image *image = state->image;
    state->image = NULL;
    decode_free(state);
    *out = image;
    return MC_IMAGE_OK;
}

bool mc_image_view_get(const mc_image *image, mc_image_view *out) {
    if (image == NULL || out == NULL)
        return false;
    *out = (mc_image_view){.width = image->width,
                           .height = image->height,
                           .stride = image->stride,
                           .pixels = image->pixels};
    return true;
}

void mc_image_free(mc_image *image) {
    free(image);
}
