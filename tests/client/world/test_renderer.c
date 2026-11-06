#include "renderer.h"

#include <SDL3/SDL_main.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(expression)) {                                                                       \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression);                       \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static void wall(mc_client_world_vertex vertices[6], float z, uint32_t color, bool reverse) {
    const mc_client_world_vertex corners[4] = {{-1, -1, z, 0, 1, color},
                                               {-1, 1, z, 0, 0, color},
                                               {1, 1, z, 1, 0, color},
                                               {1, -1, z, 1, 1, color}};
    static const unsigned forward[] = {0, 1, 2, 0, 2, 3};
    static const unsigned backward[] = {0, 2, 1, 0, 3, 2};
    for (size_t i = 0; i < 6; ++i)
        vertices[i] = corners[reverse ? backward[i] : forward[i]];
}

static bool pixel(const mc_client_capture *image, int x, int y, uint32_t rgb) {
    if (x < 0 || y < 0 || x >= image->width || y >= image->height)
        return false;
    const uint8_t *value = image->pixels + (size_t)y * image->stride + (size_t)x * 4u;
    const uint32_t actual = ((uint32_t)value[0] << 16u) | ((uint32_t)value[1] << 8u) | value[2];
    if (actual != rgb)
        fprintf(stderr, "Pixel(%d,%d): %06x != %06x\n", x, y, (unsigned)actual, (unsigned)rgb);
    return actual == rgb;
}

static bool capture(mc_client_display *display, mc_client_capture *out,
                    mc_client_display_error *error) {
    mc_client_capture_free(out);
    return mc_client_display_capture_rgba(display, out, error);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    bool ok = false;
    mc_client_display *display = NULL;
    mc_client_world_renderer *renderer = NULL, *textured = NULL, *limited = NULL;
    mc_client_world_mesh *near_mesh = NULL, *far_mesh = NULL, *back_mesh = NULL;
    mc_client_world_mesh *low_alpha = NULL, *high_alpha = NULL, *large = NULL, *pattern = NULL;
    mc_client_world_mesh *blend_mesh = NULL;
    mc_client_world_mesh *limit_mesh = NULL;
    mc_client_capture image = {0}, baseline = {0};
    mc_client_display_error error = {0};
    const uint8_t white[] = {255, 255, 255, 255};
    const mc_image_view white_view = {1, 1, 4, white};
    const uint8_t atlas[] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
    const mc_image_view atlas_view = {2, 2, 8, atlas};
    mc_client_world_camera camera = {.vertical_fov = 90.0f, .near_clip = 0.05f, .far_clip = 100};
    mc_client_world_fog fog = {.kind = MC_CLIENT_WORLD_FOG_NONE, .blue = 1.0f};
    mc_client_world_vertex vertices[6];
    CHECK(mc_client_display_create("World renderer contracts", 128, 128, true, &display, &error));
    CHECK(mc_client_world_renderer_create(display, &white_view, false, NULL, &renderer, &error));
    wall(vertices, 3, UINT32_C(0xffff0000), false);
    CHECK(mc_client_world_mesh_create(renderer, 0, 0, 0, vertices, 6, &near_mesh, &error));
    wall(vertices, 6, UINT32_C(0xff00ff00), false);
    CHECK(mc_client_world_mesh_create(renderer, 0, 0, 0, vertices, 6, &far_mesh, &error));
    wall(vertices, 2, UINT32_C(0xffff0000), true);
    CHECK(mc_client_world_mesh_create(renderer, 0, 0, 0, vertices, 6, &back_mesh, &error));
    wall(vertices, 2, UINT32_C(0x19ff0000), false);
    CHECK(mc_client_world_mesh_create(renderer, 0, 0, 0, vertices, 6, &low_alpha, &error));
    wall(vertices, 2, UINT32_C(0x1aff0000), false);
    CHECK(mc_client_world_mesh_create(renderer, 0, 0, 0, vertices, 6, &high_alpha, &error));
    wall(vertices, 2, UINT32_C(0x80ff0000), false);
    CHECK(mc_client_world_mesh_create(renderer, 0, 0, 0, vertices, 6, &blend_mesh, &error));
    CHECK(!mc_client_world_renderer_draw(renderer, near_mesh, false, true, true, &error) &&
          error.status == MC_CLIENT_DISPLAY_INVALID_ARGUMENT);
    CHECK(mc_client_world_renderer_begin(renderer, &camera, &fog, &error));
    CHECK(mc_client_world_renderer_draw(renderer, near_mesh, false, true, true, &error));
    CHECK(mc_client_world_renderer_draw(renderer, far_mesh, false, true, true, &error));
    CHECK(capture(display, &image, &error));
    CHECK(pixel(&image, image.width / 2, image.height / 2, 0xff0000u));
    CHECK(pixel(&image, 0, 0, 0x0000ffu));
    CHECK(mc_client_world_renderer_begin(renderer, &camera, &fog, &error));
    CHECK(mc_client_world_renderer_draw(renderer, far_mesh, false, true, true, &error));
    CHECK(mc_client_world_renderer_draw(renderer, near_mesh, false, true, true, &error));
    CHECK(capture(display, &baseline, &error));
    CHECK(image.width == baseline.width && image.height == baseline.height &&
          image.stride == baseline.stride &&
          memcmp(image.pixels, baseline.pixels, image.stride * (size_t)image.height) == 0);

    CHECK(mc_client_world_renderer_begin(renderer, &camera, &fog, &error));
    CHECK(mc_client_world_renderer_draw(renderer, back_mesh, false, true, true, &error));
    CHECK(capture(display, &image, &error));
    CHECK(pixel(&image, image.width / 2, image.height / 2, 0x0000ffu));
    CHECK(mc_client_world_renderer_begin(renderer, &camera, &fog, &error));
    CHECK(mc_client_world_renderer_draw(renderer, low_alpha, false, true, true, &error));
    CHECK(capture(display, &image, &error));
    CHECK(pixel(&image, image.width / 2, image.height / 2, 0x0000ffu));
    CHECK(mc_client_world_renderer_draw(renderer, high_alpha, false, true, true, &error));
    CHECK(capture(display, &image, &error));
    CHECK(pixel(&image, image.width / 2, image.height / 2, 0xff0000u));

    CHECK(mc_client_world_renderer_begin(renderer, &camera, &fog, &error));
    CHECK(mc_client_world_renderer_draw(renderer, blend_mesh, true, true, false, &error));
    CHECK(capture(display, &image, &error));
    CHECK(pixel(&image, image.width / 2, image.height / 2, 0x80007fu));

    fog.kind = MC_CLIENT_WORLD_FOG_LINEAR;
    fog.start = 0;
    fog.end = 1;
    CHECK(mc_client_world_renderer_begin(renderer, &camera, &fog, &error));
    CHECK(mc_client_world_renderer_draw(renderer, near_mesh, false, true, true, &error));
    CHECK(capture(display, &image, &error));
    CHECK(pixel(&image, image.width / 2, image.height / 2, 0x0000ffu));
    fog.kind = MC_CLIENT_WORLD_FOG_EXPONENTIAL;
    fog.density = 0;
    CHECK(mc_client_world_renderer_begin(renderer, &camera, &fog, &error));
    CHECK(mc_client_world_renderer_draw(renderer, near_mesh, false, true, true, &error));
    CHECK(capture(display, &image, &error));
    CHECK(pixel(&image, image.width / 2, image.height / 2, 0xff0000u));
    fog.density = 100;
    CHECK(mc_client_world_renderer_begin(renderer, &camera, &fog, &error));
    CHECK(mc_client_world_renderer_draw(renderer, near_mesh, false, true, true, &error));
    CHECK(capture(display, &image, &error));
    CHECK(pixel(&image, image.width / 2, image.height / 2, 0x0000ffu));

    fog.kind = MC_CLIENT_WORLD_FOG_NONE;
    wall(vertices, 3, UINT32_C(0xffff0000), false);
    CHECK(mc_client_world_mesh_create(renderer, 32000000, 0, 0, vertices, 6, &large, &error));
    camera.eye_x = 32000000.5;
    CHECK(mc_client_world_renderer_begin(renderer, &camera, &fog, &error));
    CHECK(mc_client_world_renderer_draw(renderer, large, false, true, true, &error));
    CHECK(capture(display, &image, &error));
    CHECK(image.width == baseline.width && image.height == baseline.height &&
          memcmp(image.pixels, baseline.pixels, image.stride * (size_t)image.height) == 0);
    camera.eye_x = 0;

    CHECK(mc_client_world_renderer_create(display, &atlas_view, false, NULL, &textured, &error));
    wall(vertices, 3, UINT32_C(0xffffffff), false);
    CHECK(mc_client_world_mesh_create(textured, 0, 0, 0, vertices, 6, &pattern, &error));
    CHECK(mc_client_world_renderer_begin(textured, &camera, &fog, &error));
    CHECK(!mc_client_world_renderer_draw(textured, near_mesh, false, true, true, &error) &&
          error.status == MC_CLIENT_DISPLAY_INVALID_ARGUMENT);
    CHECK(mc_client_world_renderer_draw(textured, pattern, false, true, true, &error));
    CHECK(capture(display, &image, &error));
    CHECK(pixel(&image, image.width * 2 / 5, image.height * 2 / 5, 0x00ff00u));
    CHECK(pixel(&image, image.width * 3 / 5, image.height * 2 / 5, 0xff0000u));
    CHECK(pixel(&image, image.width * 2 / 5, image.height * 3 / 5, 0xffffffu));
    CHECK(pixel(&image, image.width * 3 / 5, image.height * 3 / 5, 0x0000ffu));
    CHECK(mc_client_display_begin(display, image.width, image.height, &error));
    const mc_client_vertex overlay[4] = {{0, 0, 0, 0, 0xff00ff00u},
                                         {0, 16, 0, 0, 0xff00ff00u},
                                         {16, 16, 0, 0, 0xff00ff00u},
                                         {16, 0, 0, 0, 0xff00ff00u}};
    CHECK(mc_client_display_draw_quad(display, 0, overlay, false, false, &error));
    CHECK(capture(display, &image, &error));
    CHECK(pixel(&image, 5, 5, 0x00ff00u));
    CHECK(pixel(&image, image.width * 3 / 5, image.height * 2 / 5, 0xff0000u));

    const mc_client_world_renderer_limits limits = {1, 6, 6u * 24u};
    CHECK(mc_client_world_renderer_create(display, &white_view, false, &limits, &limited, &error));
    CHECK(mc_client_world_mesh_create(limited, 0, 0, 0, vertices, 6, &limit_mesh, &error));
    mc_client_world_mesh *unchanged = limit_mesh;
    CHECK(!mc_client_world_mesh_create(limited, 0, 0, 0, vertices, 6, &unchanged, &error) &&
          unchanged == limit_mesh && error.status == MC_CLIENT_DISPLAY_LIMIT);
    CHECK(mc_client_world_mesh_retire(&limit_mesh, &error) && limit_mesh == NULL);
    vertices[0].x = NAN;
    CHECK(!mc_client_world_mesh_create(limited, 0, 0, 0, vertices, 6, &limit_mesh, &error) &&
          limit_mesh == NULL && error.status == MC_CLIENT_DISPLAY_INVALID_ARGUMENT);
    wall(vertices, 3, UINT32_C(0xffffffff), false);
    CHECK(mc_client_world_mesh_create(limited, 0, 0, 0, vertices, 6, &limit_mesh, &error));
    camera.vertical_fov = 0;
    CHECK(!mc_client_world_renderer_begin(limited, &camera, &fog, &error) &&
          error.status == MC_CLIENT_DISPLAY_INVALID_ARGUMENT);
    CHECK(!mc_client_world_renderer_draw(limited, limit_mesh, false, true, true, &error));
    CHECK(mc_client_world_mesh_retire(&limit_mesh, &error));
    CHECK(mc_client_world_renderer_retire(&limited, &error));
    CHECK(mc_client_world_mesh_retire(&pattern, &error));
    CHECK(mc_client_world_renderer_retire(&textured, &error));
    CHECK(mc_client_world_mesh_retire(&large, &error));
    CHECK(mc_client_world_mesh_retire(&blend_mesh, &error));
    CHECK(mc_client_world_mesh_retire(&high_alpha, &error));
    CHECK(mc_client_world_mesh_retire(&low_alpha, &error));
    CHECK(mc_client_world_mesh_retire(&back_mesh, &error));
    CHECK(mc_client_world_mesh_retire(&far_mesh, &error));
    CHECK(mc_client_world_mesh_retire(&near_mesh, &error));
    CHECK(mc_client_world_renderer_retire(&renderer, &error));
    ok = true;
cleanup:
    if (!ok)
        fprintf(stderr, "World GL failure: %s: %s (%u)\n",
                error.operation == NULL ? "contract" : error.operation, error.detail,
                error.gl_error);
    mc_client_capture_free(&image);
    mc_client_capture_free(&baseline);
    mc_client_world_renderer_retire(&limited, NULL);
    mc_client_world_renderer_retire(&textured, NULL);
    mc_client_world_renderer_retire(&renderer, NULL);
    if (!mc_client_display_destroy(display, &error))
        ok = false;
    if (ok)
        printf("World renderer checks: %u\n", checks);
    return ok ? 0 : 1;
}
