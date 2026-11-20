#include "display.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static unsigned int checks;

#define REQUIRE(condition)                                                                         \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static bool quad(mc_client_display *display, mc_client_texture texture, double x, double y,
                 double width, double height, float u, float v, float du, float dv, uint32_t argb,
                 bool blend, bool alpha_test, mc_client_display_error *error) {
    const mc_client_vertex vertices[4] = {{x, y, u, v, argb},
                                          {x, y + height, u, v + dv, argb},
                                          {x + width, y + height, u + du, v + dv, argb},
                                          {x + width, y, u + du, v, argb}};
    return mc_client_display_draw_quad(display, texture, vertices, blend, alpha_test, error);
}

static bool model_quad(mc_client_display *display, mc_client_texture texture, double x, double y,
                       double width, double height, const mc_client_modelview *matrix,
                       uint32_t argb, bool blend, bool alpha_test, mc_client_display_error *error) {
    const mc_client_vertex vertices[4] = {{x, y + height, 0, 1, argb},
                                          {x + width, y + height, 1, 1, argb},
                                          {x + width, y, 1, 0, argb},
                                          {x, y, 0, 0, argb}};
    return mc_client_display_draw_quad_modelview(display, texture, vertices, matrix, blend,
                                                 alpha_test, error);
}

/* Check interior samples using physical dimensions: high-DPI scale is supplied
 * by the real window/context, not assumed from its logical request. Alpha is
 * outside this assertion because the desktop framebuffer may omit it. */
static bool rgb(const mc_client_capture *capture, int x_numerator, int y_numerator, uint8_t red,
                uint8_t green, uint8_t blue, unsigned int tolerance) {
    if (capture->pixels == NULL || capture->width < 8 || capture->height < 8)
        return false;
    const size_t x = (size_t)capture->width * (size_t)x_numerator / 8u;
    const size_t y = (size_t)capture->height * (size_t)y_numerator / 8u;
    const uint8_t *pixel = capture->pixels + y * capture->stride + x * 4u;
    const uint8_t expected[3] = {red, green, blue};
    for (size_t i = 0; i < 3; ++i) {
        const unsigned int difference = pixel[i] > expected[i]
                                            ? (unsigned int)pixel[i] - expected[i]
                                            : (unsigned int)expected[i] - pixel[i];
        if (difference > tolerance) {
            fprintf(stderr, "RGB sample (%zu,%zu): %u,%u,%u expected %u,%u,%u +/- %u\n", x, y,
                    (unsigned int)pixel[0], (unsigned int)pixel[1], (unsigned int)pixel[2],
                    (unsigned int)red, (unsigned int)green, (unsigned int)blue, tolerance);
            return false;
        }
    }
    return true;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    int result = 1;
    mc_client_display *display = NULL, *other = NULL;
    mc_client_capture capture = {0};
    mc_client_display_error error = {0};
    mc_client_texture texture = 0, anaglyph = 0;
    const mc_client_modelview source_model = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, -2000, 1}};
    bool outer_video = false, destroyed = false;
    /* Retain an independent SDL reference and prove that destroying either
     * display retires only the reference it acquired. */
    REQUIRE((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0);
    REQUIRE(SDL_InitSubSystem(SDL_INIT_VIDEO));
    outer_video = true;
    REQUIRE(mc_client_display_create("Display integration", 64, 48, true, &display, &error));
    const mc_client_display_info *info = mc_client_display_info_get(display);
    REQUIRE(info != NULL && (info->gl_major > 3 || (info->gl_major == 3 && info->gl_minor >= 3)) &&
            info->vendor[0] != '\0' && info->renderer[0] != '\0');
    printf("OpenGL %d.%d core: %s / %s\n", info->gl_major, info->gl_minor, info->vendor,
           info->renderer);
    REQUIRE(mc_client_display_window(display) != NULL);
    int width = 0, height = 0;
    REQUIRE(mc_client_display_pixel_size(display, &width, &height, &error) && width >= 8 &&
            height >= 8);
    mc_client_display *held = display;
    REQUIRE(!mc_client_display_create("invalid", 0, 48, true, &held, &error) && held == display &&
            error.status == MC_CLIENT_DISPLAY_INVALID_ARGUMENT);
    REQUIRE(!mc_client_display_create("over budget", 16384, 16384, true, &held, &error) &&
            held == display && error.status == MC_CLIENT_DISPLAY_LIMIT);

    /* Distinct authored top/bottom pixels reveal upload or readback flips.
     * Nearest filtering must preserve each complete quadrant. */
    static const uint8_t pixels[] = {255, 0, 0,   255, 0,   255, 0, 255,
                                     0,   0, 255, 255, 255, 255, 0, 255};
    const mc_image_view image = {.width = 2, .height = 2, .stride = 8, .pixels = pixels};
    REQUIRE(mc_client_display_upload_texture(display, &image, false, &texture, &error) &&
            texture != 0);
    mc_client_texture held_texture = texture;
    const mc_image_view invalid_image = {.width = 2, .height = 2, .stride = 7, .pixels = pixels};
    REQUIRE(
        !mc_client_display_upload_texture(display, &invalid_image, false, &held_texture, &error) &&
        held_texture == texture && error.status == MC_CLIENT_DISPLAY_INVALID_ARGUMENT);
    REQUIRE(mc_client_display_begin(display, 32.0, 24.0, &error));
    REQUIRE(quad(display, texture, 0, 0, 32, 24, 0, 0, 1, 1, UINT32_C(0xffffffff), false, true,
                 &error));
    REQUIRE(mc_client_display_capture_rgba(display, &capture, &error));
    REQUIRE(capture.width == width && capture.height == height &&
            capture.stride == (size_t)width * 4u);
    REQUIRE(rgb(&capture, 1, 1, 255, 0, 0, 0) && rgb(&capture, 6, 1, 0, 255, 0, 0) &&
            rgb(&capture, 1, 6, 0, 0, 255, 0) && rgb(&capture, 6, 6, 255, 255, 0, 0));
    mc_client_capture_free(&capture);
    REQUIRE(capture.pixels == NULL && capture.width == 0 && capture.height == 0 &&
            capture.stride == 0);

    /* Repeat is tested in all quadrants with an entire-period UV offset. */
    REQUIRE(mc_client_display_begin(display, 32.0, 24.0, &error));
    REQUIRE(quad(display, texture, 0, 0, 32, 24, 1, -1, 1, 1, UINT32_C(0xffffffff), false, false,
                 &error));
    REQUIRE(mc_client_display_capture_rgba(display, &capture, &error));
    REQUIRE(rgb(&capture, 1, 1, 255, 0, 0, 0) && rgb(&capture, 6, 1, 0, 255, 0, 0) &&
            rgb(&capture, 1, 6, 0, 0, 255, 0) && rgb(&capture, 6, 6, 255, 255, 0, 0));
    mc_client_capture_free(&capture);

    /* The source integer anaglyph transform precedes texture upload. */
    static const uint8_t color[] = {100, 150, 200, 255};
    const mc_image_view single = {.width = 1, .height = 1, .stride = 4, .pixels = color};
    REQUIRE(mc_client_display_upload_texture(display, &single, true, &anaglyph, &error));
    REQUIRE(mc_client_display_begin(display, 32.0, 24.0, &error));
    REQUIRE(quad(display, anaglyph, 0, 0, 32, 24, 0, 0, 1, 1, UINT32_C(0xffffffff), false, false,
                 &error));
    REQUIRE(mc_client_display_capture_rgba(display, &capture, &error));
    REQUIRE(rgb(&capture, 3, 3, 140, 135, 170, 0));
    mc_client_capture_free(&capture);

    /* Equal-depth later quads must win under LEQUAL. Alpha25/255 is below the
     * source0.1 threshold;26/255 is above it. The latter blends over white.
     * One RGB unit allows framebuffer blend/conversion rounding, not a screen
     * comparison tolerance or an original-renderer parity claim. */
    REQUIRE(mc_client_display_begin(display, 32.0, 24.0, &error));
    REQUIRE(quad(display, 0, 0, 0, 32, 24, 0, 0, 0, 0, UINT32_C(0xffffffff), false, false, &error));
    REQUIRE(quad(display, 0, 0, 0, 16, 24, 0, 0, 0, 0, UINT32_C(0x19000000), true, true, &error));
    REQUIRE(quad(display, 0, 16, 0, 16, 24, 0, 0, 0, 0, UINT32_C(0x1a000000), true, true, &error));
    REQUIRE(mc_client_display_capture_rgba(display, &capture, &error));
    REQUIRE(rgb(&capture, 1, 3, 255, 255, 255, 0) && rgb(&capture, 6, 3, 229, 229, 229, 1));
    mc_client_capture_free(&capture);

    /* Clear authored color through the legacy API, then begin again to leave
     * that color while resetting depth. Geometry assertions do not depend on
     * equal-depth rounding between the legacy and matrix shader branches. */
    REQUIRE(mc_client_display_begin(display, 32.0, 24.0, &error));
    REQUIRE(quad(display, 0, 0, 0, 32, 24, 0, 0, 0, 0, UINT32_C(0xff000000), false, false, &error));
    REQUIRE(mc_client_display_begin(display, 32.0, 24.0, &error));
    mc_client_modelview translated = source_model;
    translated.column_major[12] = 8.0f;
    translated.column_major[13] = 6.0f;
    REQUIRE(model_quad(display, 0, 0, 0, 8, 6, &translated, UINT32_C(0xffff0000), false, false,
                       &error));
    REQUIRE(mc_client_display_capture_rgba(display, &capture, &error));
    REQUIRE(rgb(&capture, 3, 3, 255, 0, 0, 0) && rgb(&capture, 1, 1, 0, 0, 0, 0) &&
            rgb(&capture, 5, 5, 0, 0, 0, 0));
    mc_client_capture_free(&capture);

    /* Identity z=0 is outside source Ortho's near/far range; z=-2000 is inside.
     * This catches a path that ignores the matrix's z translation. */
    REQUIRE(mc_client_display_begin(display, 32.0, 24.0, &error));
    REQUIRE(quad(display, 0, 0, 0, 32, 24, 0, 0, 0, 0, UINT32_C(0xff000000), false, false, &error));
    REQUIRE(mc_client_display_begin(display, 32.0, 24.0, &error));
    mc_client_modelview clipped = source_model;
    clipped.column_major[14] = 0.0f;
    REQUIRE(
        model_quad(display, 0, 0, 0, 32, 24, &clipped, UINT32_C(0xff00ff00), false, false, &error));
    REQUIRE(mc_client_display_capture_rgba(display, &capture, &error));
    REQUIRE(rgb(&capture, 3, 3, 0, 0, 0, 0));
    mc_client_capture_free(&capture);
    REQUIRE(model_quad(display, 0, 0, 0, 32, 24, &source_model, UINT32_C(0xffff0000), false, false,
                       &error));
    REQUIRE(mc_client_display_capture_rgba(display, &capture, &error));
    REQUIRE(rgb(&capture, 3, 3, 255, 0, 0, 0));
    mc_client_capture_free(&capture);

    /* Alternate both entry points in one frame, using disjoint interiors.
     * Every call must explicitly select its own model/legacy shader path. */
    REQUIRE(mc_client_display_begin(display, 32.0, 24.0, &error));
    REQUIRE(model_quad(display, 0, 0, 0, 8, 12, &source_model, UINT32_C(0xff0000ff), false, false,
                       &error));
    REQUIRE(quad(display, 0, 16, 0, 8, 12, 0, 0, 0, 0, UINT32_C(0xff00ff00), false, false, &error));
    mc_client_modelview lower = source_model;
    lower.column_major[13] = 12.0f;
    REQUIRE(
        model_quad(display, 0, 0, 0, 8, 12, &lower, UINT32_C(0xffff0000), false, false, &error));
    REQUIRE(
        quad(display, 0, 16, 12, 8, 12, 0, 0, 0, 0, UINT32_C(0xffffffff), false, false, &error));
    REQUIRE(mc_client_display_capture_rgba(display, &capture, &error));
    REQUIRE(rgb(&capture, 1, 1, 0, 0, 255, 0) && rgb(&capture, 5, 1, 0, 255, 0, 0) &&
            rgb(&capture, 1, 6, 255, 0, 0, 0) && rgb(&capture, 5, 6, 255, 255, 255, 0));
    mc_client_capture_free(&capture);

    /* Source z=-2000 and the legacy path both intend depth0.5. Check both
     * overlapping orders: an intermediate shader-depth difference must not
     * silently defeat the source LEQUAL layering contract. */
    REQUIRE(mc_client_display_begin(display, 32.0, 24.0, &error));
    REQUIRE(quad(display, 0, 0, 0, 32, 24, 0, 0, 0, 0, UINT32_C(0xffffffff), false, false, &error));
    REQUIRE(model_quad(display, 0, 0, 0, 32, 24, &source_model, UINT32_C(0xff0000ff), false, false,
                       &error));
    REQUIRE(mc_client_display_capture_rgba(display, &capture, &error));
    REQUIRE(rgb(&capture, 3, 3, 0, 0, 255, 0));
    mc_client_capture_free(&capture);
    REQUIRE(quad(display, 0, 0, 0, 32, 24, 0, 0, 0, 0, UINT32_C(0xffff0000), false, false, &error));
    REQUIRE(mc_client_display_capture_rgba(display, &capture, &error));
    REQUIRE(rgb(&capture, 3, 3, 255, 0, 0, 0));
    mc_client_capture_free(&capture);

    /* The same real texture and alpha-test/blend rules apply to local geometry. */
    REQUIRE(mc_client_display_begin(display, 32.0, 24.0, &error));
    REQUIRE(model_quad(display, texture, 0, 0, 16, 12, &translated, UINT32_C(0xffffffff), false,
                       true, &error));
    REQUIRE(mc_client_display_capture_rgba(display, &capture, &error));
    REQUIRE(rgb(&capture, 3, 3, 255, 0, 0, 0) && rgb(&capture, 5, 3, 0, 255, 0, 0) &&
            rgb(&capture, 3, 5, 0, 0, 255, 0) && rgb(&capture, 5, 5, 255, 255, 0, 0));
    mc_client_capture_free(&capture);
    REQUIRE(mc_client_display_begin(display, 32.0, 24.0, &error));
    REQUIRE(quad(display, 0, 0, 0, 32, 24, 0, 0, 0, 0, UINT32_C(0xffffffff), false, false, &error));
    REQUIRE(mc_client_display_begin(display, 32.0, 24.0, &error));
    REQUIRE(model_quad(display, 0, 0, 0, 16, 24, &source_model, UINT32_C(0x19000000), true, true,
                       &error));
    REQUIRE(model_quad(display, 0, 16, 0, 16, 24, &source_model, UINT32_C(0x1a000000), true, true,
                       &error));
    REQUIRE(mc_client_display_capture_rgba(display, &capture, &error));
    REQUIRE(rgb(&capture, 1, 3, 255, 255, 255, 0) && rgb(&capture, 6, 3, 229, 229, 229, 1));
    mc_client_capture_free(&capture);
    REQUIRE(!mc_client_display_begin(display, NAN, 24.0, &error) &&
            error.status == MC_CLIENT_DISPLAY_INVALID_ARGUMENT);
    REQUIRE(
        !quad(display, 0, 0, 0, 32, 24, 0, 0, 0, 0, UINT32_C(0xffffffff), false, false, &error));
    REQUIRE(!mc_client_display_begin(display, 1e-40, 24.0, &error) &&
            error.status == MC_CLIENT_DISPLAY_INVALID_ARGUMENT);
    REQUIRE(mc_client_display_begin(display, 32.0, 24.0, &error));
    REQUIRE(!quad(display, 0, 1e39, 0, 32, 24, 0, 0, 0, 0, UINT32_C(0xffffffff), false, false,
                  &error) &&
            error.status == MC_CLIENT_DISPLAY_INVALID_ARGUMENT);

    /* A second real context changes current ownership. Calls on the first
     * display must restore its context before using retained texture IDs. */
    REQUIRE(mc_client_display_create("Second display", 32, 32, true, &other, &error));
    SDL_Window *other_window = mc_client_display_window(other);
    REQUIRE(SDL_GL_GetCurrentWindow() == other_window);
    REQUIRE(!model_quad(display, 0, 0, 0, 8, 6, NULL, UINT32_C(0xffffffff), false, false, &error) &&
            error.status == MC_CLIENT_DISPLAY_INVALID_ARGUMENT &&
            SDL_GL_GetCurrentWindow() == other_window);
    mc_client_modelview invalid_model = source_model;
    invalid_model.column_major[0] = NAN;
    REQUIRE(!model_quad(display, 0, 0, 0, 8, 6, &invalid_model, UINT32_C(0xffffffff), false, false,
                        &error) &&
            error.status == MC_CLIENT_DISPLAY_INVALID_ARGUMENT &&
            SDL_GL_GetCurrentWindow() == other_window);
    invalid_model = source_model;
    invalid_model.column_major[14] = INFINITY;
    REQUIRE(!model_quad(display, 0, 0, 0, 8, 6, &invalid_model, UINT32_C(0xffffffff), false, false,
                        &error) &&
            error.status == MC_CLIENT_DISPLAY_INVALID_ARGUMENT &&
            SDL_GL_GetCurrentWindow() == other_window);
    static const size_t affine_slots[] = {3, 7, 11, 15};
    for (size_t i = 0; i < sizeof(affine_slots) / sizeof(affine_slots[0]); ++i) {
        invalid_model = source_model;
        invalid_model.column_major[affine_slots[i]] = affine_slots[i] == 15 ? 0.0f : 1.0f;
        REQUIRE(!model_quad(display, 0, 0, 0, 8, 6, &invalid_model, UINT32_C(0xffffffff), false,
                            false, &error) &&
                error.status == MC_CLIENT_DISPLAY_INVALID_ARGUMENT &&
                SDL_GL_GetCurrentWindow() == other_window);
    }
    invalid_model = source_model;
    invalid_model.column_major[0] = FLT_MAX;
    REQUIRE(!model_quad(display, 0, 0, 0, 8, 6, &invalid_model, UINT32_C(0xffffffff), false, false,
                        &error) &&
            error.status == MC_CLIENT_DISPLAY_INVALID_ARGUMENT &&
            SDL_GL_GetCurrentWindow() == other_window);
    mc_client_framebuffer_info framebuffer = {0};
    REQUIRE(mc_client_display_framebuffer_info_get(display, &framebuffer, &error));
    REQUIRE(framebuffer.red_bits > 0 && framebuffer.green_bits > 0 && framebuffer.blue_bits > 0 &&
            framebuffer.alpha_bits >= 0 && framebuffer.depth_bits >= 0 &&
            framebuffer.stencil_bits >= 0 && framebuffer.sample_buffers >= 0 &&
            framebuffer.samples >= 0 &&
            SDL_GL_GetCurrentWindow() == mc_client_display_window(display));
    mc_client_framebuffer_info untouched = {.red_bits = -1, .depth_bits = -2};
    REQUIRE(!mc_client_display_framebuffer_info_get(NULL, &untouched, &error) &&
            error.status == MC_CLIENT_DISPLAY_INVALID_ARGUMENT && untouched.red_bits == -1 &&
            untouched.depth_bits == -2);
    REQUIRE(mc_client_display_begin(display, 32.0, 24.0, &error));
    REQUIRE(quad(display, texture, 0, 0, 32, 24, 0, 0, 1, 1, UINT32_C(0xffffffff), false, true,
                 &error));
    REQUIRE(mc_client_display_capture_rgba(display, &capture, &error));
    REQUIRE(rgb(&capture, 1, 1, 255, 0, 0, 0) && rgb(&capture, 6, 6, 255, 255, 0, 0));
    mc_client_capture_free(&capture);
    destroyed = mc_client_display_destroy(other, &error);
    other = NULL;
    REQUIRE(destroyed);
    REQUIRE(mc_client_display_swap(display, &error));
    destroyed = mc_client_display_destroy(display, &error);
    display = NULL;
    REQUIRE(destroyed);
    REQUIRE((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) != 0);
    REQUIRE(mc_client_display_destroy(NULL, &error) && error.status == MC_CLIENT_DISPLAY_OK);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    outer_video = false;
    REQUIRE((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0);
    result = 0;
cleanup:
    if (result != 0)
        fprintf(stderr, "Display: %s: %s (status%d, GL%u)\n",
                error.operation == NULL ? "test assertion" : error.operation, error.detail,
                (int)error.status, error.gl_error);
    mc_client_capture_free(&capture);
    if (!mc_client_display_destroy(other, &error))
        result = 1;
    if (!mc_client_display_destroy(display, &error))
        result = 1;
    if (outer_video)
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    if (result == 0)
        printf("%u display checks passed\n", checks);
    return result;
}
