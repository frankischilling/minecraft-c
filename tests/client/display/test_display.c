#include "display.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

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
    REQUIRE(!mc_client_display_begin(display, NAN, 24.0, &error) &&
            error.status == MC_CLIENT_DISPLAY_INVALID_ARGUMENT);
    REQUIRE(
        !quad(display, 0, 0, 0, 32, 24, 0, 0, 0, 0, UINT32_C(0xffffffff), false, false, &error));

    /* A second real context changes current ownership. Calls on the first
     * display must restore its context before using retained texture IDs. */
    REQUIRE(mc_client_display_create("Second display", 32, 32, true, &other, &error));
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
