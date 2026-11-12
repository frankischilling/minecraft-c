#ifndef MC_RESOURCES_IMAGE_IMAGE_H
#define MC_RESOURCES_IMAGE_IMAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct mc_image mc_image;

typedef enum mc_image_status {
    MC_IMAGE_OK = 0,
    MC_IMAGE_INVALID_ARGUMENT,
    MC_IMAGE_INVALID_PNG,
    MC_IMAGE_TRUNCATED,
    MC_IMAGE_LIMIT,
    MC_IMAGE_ALLOCATION_FAILED
} mc_image_status;

typedef struct mc_image_limits {
    size_t max_encoded_bytes;
    size_t max_decoded_bytes;
    size_t max_allocation_bytes;
    uint32_t max_dimension;
} mc_image_limits;

typedef struct mc_image_view {
    uint32_t width, height;
    size_t stride;
    const uint8_t *pixels;
} mc_image_view;

/* Defaults are 16 MiB encoded, 64 MiB RGBA, 96 MiB peak live allocation,
 * and 8192 pixels on each axis. Zero limits are valid and admit no image.
 * Peak accounting includes this module's decode state, image owner/pixels,
 * allocation headers and all libpng/zlib allocations. Borrowed encoded bytes,
 * stack storage and the system allocator's hidden overhead are excluded. */
mc_image_limits mc_image_default_limits(void);
const char *mc_image_status_string(mc_image_status status);

/* Decode one complete PNG into independent owned, top-down, tightly packed
 * straight RGBA8. Grayscale expands to RGB, palette and tRNS expand to RGBA,
 * and missing alpha becomes 255. Sub-byte grayscale expands to the full range.
 * Sixteen-bit samples retain their high byte after full-width tRNS comparison.
 * Adam7 input is combined into ordinary rows. No gamma/color-profile, sBIT,
 * premultiplication or orientation transform is applied. Other metadata is
 * ignored rather than retained or decompressed.
 *
 * All chunk CRCs are checked, including ignored metadata. The native input
 * policy allows at most 1024 chunks and 64 KiB per ancillary chunk, requires
 * IEND, and rejects trailing bytes and unknown critical chunks. These fixed
 * bounds supplement caller limits; this is not an unrestricted PNG reader.
 *
 * NULL limits selects defaults. Input may be NULL only when length is zero.
 * Input/limits are borrowed only for this call and must remain unchanged.
 * out is required, disjoint from inputs, and unchanged on failure. Release
 * any previous owned image before replacing it on success. The decoder has no
 * callbacks to application code, I/O, stderr output or mutable global state. */
mc_image_status mc_image_decode_png(const uint8_t *encoded, size_t length,
                                    const mc_image_limits *limits, mc_image **out);

/* The view is borrowed and immutable until image free. A null image or output
 * returns false without changing out. Calls on one image may share read-only
 * views; destruction requires all readers to have retired. */
bool mc_image_view_get(const mc_image *image, mc_image_view *out);
void mc_image_free(mc_image *image); /* NULL is harmless. */

#endif
