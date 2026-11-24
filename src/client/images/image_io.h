#ifndef MC_CLIENT_IMAGES_IMAGE_IO_H
#define MC_CLIENT_IMAGES_IMAGE_IO_H
#include "mc/client/render_engine/render_engine.h"

typedef enum mc_client_image_io_status {
    MC_CLIENT_IMAGE_IO_OK,
    MC_CLIENT_IMAGE_IO_INVALID_ARGUMENT,
    MC_CLIENT_IMAGE_IO_ALLOCATION_FAILED,
    MC_CLIENT_IMAGE_IO_LIMIT,
    MC_CLIENT_IMAGE_IO_SOURCE_IO,
    MC_CLIENT_IMAGE_IO_SOURCE_ILLEGAL_ARGUMENT,
    MC_CLIENT_IMAGE_IO_UNSUPPORTED_NATIVE,
    MC_CLIENT_IMAGE_IO_PROVIDER_FAILED,
    MC_CLIENT_IMAGE_IO_BUSY
} mc_client_image_io_status;

typedef struct mc_client_image_io_error {
    mc_client_image_io_status status;
    const char *operation;
    int zlib_error;
} mc_client_image_io_error;

/* One real byte-stream primitive, shared by the concrete pack and class-loader
 * streams. This callback reads bytes and preserves typed I/O/native failures;
 * it never supplies an image, texture ID, font or completed drawing body.
 * On error count is zero. All admitted owners remain live/exclusive and may
 * not be mutated/reentered/retired by an input callback. No source close. */
typedef struct mc_client_image_io_input {
    void *context;
    mc_client_image_io_status (*read)(void *, uint8_t *, size_t capacity, size_t *count,
                                      mc_client_image_io_error *);
    bool is_null;
} mc_client_image_io_input;

typedef struct mc_client_image_io_limits {
    size_t max_encoded_bytes, max_decoded_bytes, max_inflater_bytes;
} mc_client_image_io_limits;

static inline mc_client_image_io_status mc_client_image_io_fail(mc_client_image_io_error *e,
                                                                mc_client_image_io_status status,
                                                                const char *operation) {
    e->status = status;
    e->operation = operation;
    return status;
}

/* Exact scoped ImageIO PNG8/getRGB body factored from pinned e/images.c:
 * RGB/RGBA/palette, filters0..4, bounded zlib, measured intake/CRC quirks.
 * It leaves the actual source InputStream OPEN on every return. Unrecognized
 * data succeeds with NULL; selected other provider families/gray/16-bit/Adam7
 * remain explicit dependencies. No generic mc_image fallback or global CRC
 * policy change. Canonical ARGB owner is a getRGB representation, not a claim
 * about ImageIO's internal raster/getType. Source/input/error/output are live,
 * exclusive and disjoint. out is unchanged on failure. No GL/Display call. */
mc_client_image_io_status mc_client_image_io_read(const mc_client_image_io_input *,
                                                  const mc_client_image_io_limits *,
                                                  mc_client_argb_image **out,
                                                  mc_client_image_io_error *);
#endif
