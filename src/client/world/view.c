#include "view.h"

#include "mc/core/java/java.h"

#include <stdlib.h>

enum { VIEW_SECTIONS = 3 * 3 * 8 };

typedef struct view_section {
    mc_client_world_mesh *meshes[2];
    double distance;
} view_section;

struct mc_client_world_view {
    mc_client_world_renderer *renderer;
    mc_client_world_camera camera;
    view_section sections[VIEW_SECTIONS];
    size_t count;
};

static uint32_t *colors(mc_client_assets *assets, const char *name,
                        mc_client_world_view_error *error) {
    const mc_image *image = NULL;
    mc_image_view pixels;
    if (mc_client_assets_image(assets, name, &image, &error->assets) != MC_CLIENT_ASSETS_OK ||
        !mc_image_view_get(image, &pixels) || pixels.width != 256 || pixels.height != 256) {
        error->operation = "World color map";
        return NULL;
    }
    uint32_t *map = malloc(65536u * sizeof(*map));
    if (map == NULL) {
        error->operation = "World color map allocation";
        return NULL;
    }
    for (size_t y = 0; y < 256; ++y) {
        const uint8_t *row = pixels.pixels + y * pixels.stride;
        for (size_t x = 0; x < 256; ++x)
            map[y * 256u + x] = ((uint32_t)row[x * 4u + 3u] << 24u) |
                                ((uint32_t)row[x * 4u] << 16u) |
                                ((uint32_t)row[x * 4u + 1u] << 8u) | row[x * 4u + 2u];
    }
    return map;
}

static int farther_first(const void *left, const void *right) {
    const view_section *a = left, *b = right;
    return a->distance > b->distance ? -1 : a->distance < b->distance ? 1 : 0;
}

bool mc_client_world_view_create(mc_world_runtime *world, mc_client_display *display,
                                 mc_client_assets *assets, const mc_math_trig *trig, bool anaglyph,
                                 bool ambient_occlusion, bool fancy_leaves,
                                 mc_client_world_view **out, mc_client_world_view_error *error) {
    mc_client_world_view_error frame = {0};
    mc_client_world_cpu_mesh cpu = {0};
    mc_client_world_view *view = NULL;
    uint32_t *grass = NULL, *foliage = NULL;
    const mc_world_runtime_state *live = mc_world_runtime_live(world);
    if (display == NULL || assets == NULL || trig == NULL || out == NULL || live == NULL ||
        live->spawn_x == NULL || live->spawn_z == NULL) {
        frame.operation = "World view arguments";
        goto failure;
    }
    view = calloc(1, sizeof(*view));
    if (view == NULL) {
        frame.operation = "World view allocation";
        goto failure;
    }
    const int32_t center_x = mc_java_i32_shr(*live->spawn_x, 4) * 16;
    const int32_t center_z = mc_java_i32_shr(*live->spawn_z, 4) * 16;
    if (center_x < -32000000 + 32 || center_x > 32000000 - 32 || center_z < -32000000 + 32 ||
        center_z > 32000000 - 32) {
        frame.operation = "World inspection spawn bounds";
        goto failure;
    }
    view->camera = (mc_client_world_camera){.eye_x = (double)*live->spawn_x + .5,
                                            .eye_y = 96,
                                            .eye_z = (double)*live->spawn_z - 24 + .5,
                                            .pitch = 35,
                                            .vertical_fov = 70,
                                            .near_clip = .05f,
                                            .far_clip = 512};
    const mc_image *terrain = NULL;
    mc_image_view atlas;
    if (mc_client_assets_image(assets, "/terrain.png", &terrain, &frame.assets) !=
            MC_CLIENT_ASSETS_OK ||
        !mc_image_view_get(terrain, &atlas)) {
        frame.operation = "World terrain atlas";
        goto failure;
    }
    if (!mc_client_world_renderer_create(display, &atlas, anaglyph, NULL, &view->renderer,
                                         &frame.display)) {
        frame.operation = "World renderer creation";
        goto failure;
    }
    grass = colors(assets, "/misc/grasscolor.png", &frame);
    if (grass == NULL)
        goto failure;
    foliage = colors(assets, "/misc/foliagecolor.png", &frame);
    if (foliage == NULL)
        goto failure;
    mc_client_world_mesher_options options = mc_client_world_mesher_default_options();
    options.ambient_occlusion = ambient_occlusion;
    options.anaglyph = anaglyph;
    options.fancy_leaves = fancy_leaves;
    options.grass_colors = grass;
    options.foliage_colors = foliage;
    options.grass_color_count = options.foliage_color_count = 65536;
    options.trig = trig;
    for (int32_t x = -1; x <= 1; ++x)
        for (int32_t z = -1; z <= 1; ++z)
            for (int32_t y = 0; y < 128; y += 16) {
                const int32_t ox = center_x + x * 16, oz = center_z + z * 16;
                if (mc_client_world_mesher_build(world, ox, y, oz, &options, &cpu, &frame.mesher) !=
                    MC_CLIENT_WORLD_MESHER_OK) {
                    frame.operation = "World section mesh";
                    goto failure;
                }
                view_section *section = &view->sections[view->count++];
                const double dx = (double)ox + 8 - view->camera.eye_x;
                const double dy = (double)y + 8 - view->camera.eye_y;
                const double dz = (double)oz + 8 - view->camera.eye_z;
                section->distance = dx * dx + dy * dy + dz * dz;
                for (size_t pass = 0; pass < 2; ++pass)
                    if (cpu.vertex_count[pass] != 0 &&
                        !mc_client_world_mesh_create(view->renderer, ox, y, oz, cpu.vertices[pass],
                                                     cpu.vertex_count[pass], &section->meshes[pass],
                                                     &frame.display)) {
                        frame.operation = "World section upload";
                        goto failure;
                    }
                mc_client_world_cpu_mesh_free(&cpu);
            }
    qsort(view->sections, view->count, sizeof(*view->sections), farther_first);
    free(grass);
    free(foliage);
    *out = view;
    if (error != NULL)
        *error = frame;
    return true;
failure:
    free(grass);
    free(foliage);
    mc_client_world_cpu_mesh_free(&cpu);
    (void)mc_client_world_view_retire(&view, NULL);
    if (error != NULL)
        *error = frame;
    return false;
}

bool mc_client_world_view_draw(mc_client_world_view *view, mc_client_display_error *error) {
    if (view == NULL)
        return false;
    const mc_client_world_fog fog = {
        .kind = MC_CLIENT_WORLD_FOG_NONE, .red = .6f, .green = .8f, .blue = 1};
    if (!mc_client_world_renderer_begin(view->renderer, &view->camera, &fog, error))
        return false;
    for (size_t pass = 0; pass < 2; ++pass)
        for (size_t i = 0; i < view->count; ++i)
            if (view->sections[i].meshes[pass] != NULL &&
                !mc_client_world_renderer_draw(view->renderer, view->sections[i].meshes[pass],
                                               pass == 1, true, pass == 0, error))
                return false;
    return true;
}

bool mc_client_world_view_retire(mc_client_world_view **owner, mc_client_display_error *error) {
    if (owner == NULL)
        return false;
    mc_client_world_view *view = *owner;
    if (view == NULL)
        return true;
    const bool result = mc_client_world_renderer_retire(&view->renderer, error);
    free(view);
    *owner = NULL;
    return result;
}
