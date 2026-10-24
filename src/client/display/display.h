#ifndef MC_CLIENT_DISPLAY_DISPLAY_H
#define MC_CLIENT_DISPLAY_DISPLAY_H

#include "mc/resources/image/image.h"

#include <SDL3/SDL_video.h>

typedef struct mc_client_display mc_client_display;
typedef uint32_t mc_client_texture; /* Zero selects an untextured colored quad. */

enum { MC_CLIENT_DEFAULT_WIDTH = 854, MC_CLIENT_DEFAULT_HEIGHT = 480 };

typedef enum mc_client_display_status {
    MC_CLIENT_DISPLAY_OK = 0,
    MC_CLIENT_DISPLAY_INVALID_ARGUMENT,
    MC_CLIENT_DISPLAY_ALLOCATION_FAILED,
    MC_CLIENT_DISPLAY_LIMIT,
    MC_CLIENT_DISPLAY_SDL_ERROR,
    MC_CLIENT_DISPLAY_UNSUPPORTED_GL,
    MC_CLIENT_DISPLAY_GL_ERROR,
    MC_CLIENT_DISPLAY_CONTEXT_LOST,
    MC_CLIENT_DISPLAY_NOT_DRAWABLE
} mc_client_display_status;

typedef struct mc_client_display_error {
    mc_client_display_status status;
    const char *operation;
    uint32_t gl_error;
    char detail[256];
} mc_client_display_error;

typedef struct mc_client_vertex {
    double x, y;
    float u, v;
    uint32_t argb;
} mc_client_vertex;

typedef struct mc_client_capture {
    int width, height;
    size_t stride;
    uint8_t *pixels;
} mc_client_capture;

typedef struct mc_client_display_info {
    int gl_major, gl_minor, profile;
    bool swap_interval_zero;
    char vendor[96], renderer[160], version[96];
} mc_client_display_info;

typedef struct mc_client_framebuffer_info {
    int red_bits, green_bits, blue_bits, alpha_bits, depth_bits, stencil_bits;
    bool double_buffer, stereo, srgb_enabled;
    int sample_buffers, samples;
    uint32_t color_attachment, color_encoding, draw_buffer, read_buffer;
} mc_client_framebuffer_info;

/* All operations require the SDL main/video thread and exclusive owner use.
 * Each owner retains exactly one SDL video reference, a resizable centered
 * high-DPI window, and its GL context/objects. It never quits other subsystems.
 * Initial requested size is capped at16384 on each axis and16M logical pixels;
 * actual high-DPI/resize extents are queried separately from that request.
 * Creation requests and verifies a GL >=3.3 core context without MSAA. A failed
 * swap-interval-zero request is recorded in info and is not a creation failure.
 * out is unchanged on failure; error is reset on success and copied on failure. */
bool mc_client_display_create(const char *title, int width, int height, bool hidden,
                              mc_client_display **out, mc_client_display_error *error);

/* Borrowed for events/window changes. Callers must not destroy it or replace
 * its GL context. Successful GL operations make this owner's context current.
 * No World, input routing, frame timing, background geometry or GUI ownership
 * is supplied by this module. */
SDL_Window *mc_client_display_window(mc_client_display *display);
const mc_client_display_info *mc_client_display_info_get(const mc_client_display *display);
/* Read the current owner's effective default framebuffer, not merely SDL's
 * requested attributes. Query makes its context current but changes no format,
 * framebuffer binding, draw/read selection or sRGB state. Requires default draw
 * and read framebuffer bindings; a foreign binding fails explicitly. Attachment
 * sizes/encoding use the GL3.3 query for back-left when double buffered, or
 * front-left otherwise, including zero for absent depth/stencil. The chosen
 * color attachment and current draw/read selections are reported explicitly.
 * out is unchanged on failure. No original/native format equality is implied. */
bool mc_client_display_framebuffer_info_get(mc_client_display *display,
                                            mc_client_framebuffer_info *out,
                                            mc_client_display_error *error);
bool mc_client_display_pixel_size(mc_client_display *display, int *width, int *height,
                                  mc_client_display_error *error);

/* Upload an actual tightly packed RGBA8 view with nearest/repeat, level zero,
 * unsized GL_RGBA and no sRGB conversion. The optional source anaglyph formula
 * uses integer R/G/B arithmetic and preserves alpha. Input is borrowed only for
 * this call; the owner retains all successful GL IDs until destruction.
 * Bounds: 128 textures, 64 MiB per texture and 128 MiB aggregate texel input.
 * Driver-private allocation/storage precision is outside that accounting. */
bool mc_client_display_upload_texture(mc_client_display *display, const mc_image_view *image,
                                      bool anaglyph, mc_client_texture *out,
                                      mc_client_display_error *error);

/* Begin uses physical drawable pixels for the viewport and derives binary32
 * projection coefficients from the exact scaled doubles. The vertex shader
 * applies those coefficients to binary32 logical coordinates. It clears depth only to1, enables
 * depth LEQUAL, and places quads at depth0.5. It does not clear color or sleep. A zero-sized
 * drawable reports NOT_DRAWABLE. Quads are supplied in perimeter order; the
 * renderer emits triangles 0/1/2 and0/2/3. Blend selects SRC_ALPHA and
 * ONE_MINUS_SRC_ALPHA; alpha_test discards final alpha <=0.1 in the shader.
 * Coordinates/UVs must be finite; coordinates and projection coefficients must
 * fit binary32. A failed begin invalidates the prior frame. */
bool mc_client_display_begin(mc_client_display *display, double exact_width, double exact_height,
                             mc_client_display_error *error);
bool mc_client_display_draw_quad(mc_client_display *display, mc_client_texture texture,
                                 const mc_client_vertex vertices[4], bool blend, bool alpha_test,
                                 mc_client_display_error *error);

/* Optional local-geometry path. Matrix values are column-major binary32 and
 * borrowed only during this call; the uniform upload copies them. Supply the
 * modelview derived from normal GUI operations, including source z=-2000.
 * The display derives its orthographic projection from begin's exact extents;
 * no caller-provided projection or observed target coefficients are accepted.
 * Only finite affine matrices are admitted. Conservative transformed magnitude
 * bounds are checked before GL work; this is a bounded GUI profile, not every
 * representable OpenGL transform. No mutable matrix stack is kept by Display.
 * Local positions remain local through float vertex submission. Separate GLSL
 * modelview/projection expressions express the stages but do not qualify the
 * driver's intermediate precision, contraction or matrix-update behavior. */
typedef struct mc_client_modelview {
    float column_major[16];
} mc_client_modelview;

bool mc_client_display_draw_quad_modelview(mc_client_display *display, mc_client_texture texture,
                                           const mc_client_vertex vertices[4],
                                           const mc_client_modelview *modelview, bool blend,
                                           bool alpha_test, mc_client_display_error *error);
bool mc_client_display_swap(mc_client_display *display, mc_client_display_error *error);

/* Read the current back buffer before swap into owned, top-down raw RGBA8.
 * Pixel storage plus the temporary flip row is capped at64 MiB. On failure out
 * stays unchanged. Retire any previous capture before successful replacement.
 * Free does not require a GL context and resets the complete capture. */
bool mc_client_display_capture_rgba(mc_client_display *display, mc_client_capture *out,
                                    mc_client_display_error *error);
void mc_client_capture_free(mc_client_capture *capture);

/* Always retires native ownership, including after a partial creation failure.
 * If a lost/noncurrent context cannot be used, GL deletion calls are skipped;
 * context destruction retires those driver objects. No GL function-table calls
 * follow detected context loss. NULL succeeds. A cleanup failure is reported after all remaining
 * independent SDL/native releases have been attempted. */
bool mc_client_display_destroy(mc_client_display *display, mc_client_display_error *error);

#endif
