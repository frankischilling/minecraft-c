#include "client/application_state.h"
#include "client/display/camera_surface.h"
#include "client/display/fog_surface.h"
#include "client/display/startup_surface.h"
#include "client/display/world_frame_surface.h"
#include "client/render_engine/internal.h"
#include "client/world/render_batch_internal.h"
#include "client/world/source_terrain_internal.h"
#include "entity/entity_internal.h"

#define WIN32_LEAN_AND_MEAN
#include <SDL3/SDL.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

enum { side = 384, radius = 3, visible = 98, samples = 160, warmup = 40 };

static mc_client_render_global *renderer;
static mc_client_display *display;
static mc_entity *camera;
static mc_math_classes *math;
static mc_client_render_global_error error;
static mc_client_display_error display_error;
typedef void(APIENTRY *finish_fn)(void);
static finish_fn finish_draws;
static uint64_t work_hash;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr,                                                                        \
                    "assertion at line %d: %s | error=%d, operation=%s, pc=%u | display=%d:%s\n",  \
                    __LINE__, #condition, error.status,                                            \
                    error.operation ? error.operation : "(none)", error.source_pc,                 \
                    display_error.status,                                                          \
                    display_error.operation ? display_error.operation : "(none)");                 \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static int32_t coordinate(int32_t slot, int32_t width) {
    const int32_t position = slot * 16;
    return position - ((position + width / 2 + 8) / width) * width;
}

static int define_tile(int32_t id, int32_t local_x, int32_t y, int32_t local_z, int style) {
    const mc_client_graphics_site site = {"fixture", "tile_build", "()V", -1};
    mc_client_tessellator *mesh = NULL;
    CHECK(mc_client_display_lists_begin(display, id, &display_error));
    CHECK(mc_client_nw_active_use(renderer->classes, site, &mesh, &error.graphics));
    CHECK(mc_client_nw_translation(mesh, 0, 0, 0, &error.graphics));
    CHECK(mc_client_nw_begin_quads(mesh, &error.graphics));
    for (int k = 0; k < 3; ++k) {
        const double x0 = local_x + 2.0 + style * 2.0 + (k % 2) * 4.0;
        const double z0 = local_z + 2.0 + (k / 2) * 4.0 + style * 2.0;
        const int red = 70 + (id * 17 + k * 9) % 160, green = 52 + (id * 7 + k * 41) % 160;
        CHECK(mc_client_nw_color(mesh, red, green, 120 + style * 60, 255, &error.graphics));
        CHECK(mc_client_nw_vertex(mesh, x0, y, z0, &error.graphics));
        CHECK(mc_client_nw_vertex(mesh, x0 + 4, y, z0, &error.graphics));
        CHECK(mc_client_nw_vertex(mesh, x0 + 4, y, z0 + 4, &error.graphics));
        CHECK(mc_client_nw_vertex(mesh, x0, y, z0 + 4, &error.graphics));
    }
    CHECK(mc_client_nw_draw(mesh, display, &error.graphics));
    CHECK(mc_client_display_lists_end(display, &display_error));
    return 0;
}

static int render(int mode, size_t frame) {
    /* Moving vertically crosses the recenter threshold without moving a section. */
    camera->base.y = camera->base.bm = mode && (frame & 1u) ? 5.0 : 0.0;
    CHECK(mc_client_display_source_clear(display, UINT32_C(0x4100), &display_error));
    for (int32_t pass = 0; pass < 2; ++pass) {
        mc_client_render_global_submit_call *call = NULL;
        CHECK(mc_client_render_global_submit_begin(renderer, camera, pass, 0.5, math, &call,
                                                   &error) == MC_CLIENT_RENDER_GLOBAL_OK);
        mc_client_render_global_submit_state state;
        CHECK(mc_client_render_global_submit_inspect(call, &state) && state.returned &&
              state.result == visible && state.dirty_polls == 10 && state.queued_chunks == 0 &&
              state.configured_batches == 4 && state.appended_lists == visible &&
              renderer->fields.g == camera->base.y);
        work_hash = (work_hash ^ (uint32_t)renderer->fields.R) * UINT64_C(1099511628211);
        CHECK(mc_client_render_global_submit_retire(&call, &error) == MC_CLIENT_RENDER_GLOBAL_OK);
    }
    finish_draws();
    return 0;
}

static int pixels_hash(uint64_t *out, size_t *covered) {
    mc_client_capture capture = {0};
    CHECK(mc_client_display_capture_rgba(display, &capture, &display_error));
    CHECK(capture.width == side && capture.height == side);
    uint64_t hash = UINT64_C(14695981039346656037);
    *covered = 0;
    for (int y = 0; y < side; ++y) {
        const uint8_t *row = capture.pixels + (size_t)y * capture.stride;
        for (int x = 0; x < side; ++x) {
            const uint8_t *pixel = row + (size_t)x * 4u;
            *covered += pixel[3] != 0;
            for (size_t channel = 0; channel < 4; ++channel)
                hash = (hash ^ pixel[channel]) * UINT64_C(1099511628211);
        }
    }
    *out = hash;
    mc_client_capture_free(&capture);
    CHECK(*covered > 0 && *covered < (size_t)side * side);
    return 0;
}

static int configure_view(void) {
    CHECK(mc_client_display_source_viewport(display, side, side, &display_error));
    CHECK(mc_client_display_source_matrix_mode(display, 5889, &display_error));
    CHECK(mc_client_display_source_load_identity(display, &display_error));
    CHECK(mc_client_display_source_ortho(display, -72, 72, -72, 72, -200, 200, &display_error));
    CHECK(mc_client_display_source_matrix_mode(display, 5888, &display_error));
    CHECK(mc_client_display_source_load_identity(display, &display_error));
    CHECK(mc_client_display_source_rotate(display, 90, 1, 0, 0, &display_error));
    CHECK(mc_client_display_source_texture_enable(display, false, &display_error));
    CHECK(mc_client_display_source_disable_unlit_feature(display, MC_CLIENT_SOURCE_LIGHTING,
                                                         &display_error));
    CHECK(mc_client_display_source_color_material_enable(display, false, &display_error));
    CHECK(mc_client_display_source_fog_enable(display, false, &display_error));
    CHECK(mc_client_display_source_blend_enable(display, false, &display_error));
    CHECK(mc_client_display_source_alpha_test(display, false, &display_error));
    CHECK(mc_client_display_source_depth_test(display, false, &display_error));
    CHECK(mc_client_display_source_cull_test(display, false, &display_error));
    CHECK(mc_client_display_source_clear_color(display, 0, 0, 0, 0, &display_error));
    return 0;
}

static int prepare_scene(int32_t dimension, mc_client_display_list_range **lists) {
    renderer->fields.p = renderer->fields.r = dimension;
    renderer->fields.q = 8;
    const size_t sections = (size_t)dimension * (size_t)dimension * 8u;
    renderer->o_length = renderer->n_length = sections;
    const mc_client_graphics_site site = {"fixture", "authored_terrain", "()V", -1};
    renderer->fields.o =
        mc_render_global_allocate(renderer, sections * sizeof(*renderer->fields.o), site, &error);
    renderer->fields.n =
        mc_render_global_allocate(renderer, sections * sizeof(*renderer->fields.n), site, &error);
    renderer->fields.m =
        mc_render_global_allocate(renderer, sizeof(*renderer->fields.m), site, &error);
    renderer->fields.S =
        mc_render_global_allocate(renderer, sizeof(*renderer->fields.S), site, &error);
    renderer->fields.T =
        mc_render_global_allocate(renderer, 4 * sizeof(*renderer->fields.T), site, &error);
    CHECK(renderer->fields.o && renderer->fields.n && renderer->fields.m && renderer->fields.S &&
          renderer->fields.T);
    renderer->t_length = 4;
    for (size_t i = 0; i < renderer->t_length; ++i) {
        renderer->fields.T[i] =
            mc_render_global_allocate(renderer, sizeof(*renderer->fields.T[i]), site, &error);
        CHECK(renderer->fields.T[i] &&
              mc_render_batch_construct(renderer, renderer->fields.T[i], 0));
    }
    int32_t base = 0;
    CHECK(
        mc_client_display_lists_generate(display, visible * 2, 8192, lists, &base, &display_error));
    static bool enabled[2] = {false, false};
    static bool empty[2] = {true, true};
    int32_t drawn = 0;
    for (int32_t z = 0; z < dimension; ++z)
        for (int32_t y = 0; y < 8; ++y)
            for (int32_t x = 0; x < dimension; ++x) {
                const int32_t index = (z * 8 + y) * dimension + x;
                mc_client_source_dk *section = mc_source_dk_allocate(renderer, &error);
                CHECK(section != NULL);
                const int32_t px = coordinate(x, dimension * 16), py = y * 16;
                const int32_t pz = coordinate(z, dimension * 16);
                const int32_t lx = (int32_t)((uint32_t)px & 1023u);
                const int32_t lz = (int32_t)((uint32_t)pz & 1023u);
                const bool show = abs(px / 16) <= radius && abs(pz / 16) <= radius && y < 2;
                const bool dirty = index % 4 == 0;
                section->fields = (mc_client_source_dk_fields){.C = show ? base + drawn * 2 : -1,
                                                               .c = px,
                                                               .d = py,
                                                               .e = pz,
                                                               .i = px - lx,
                                                               .j = 0,
                                                               .k = pz - lz,
                                                               .l = lx,
                                                               .m = py,
                                                               .n = lz,
                                                               .q = px + 8,
                                                               .r = py + 8,
                                                               .s = pz + 8,
                                                               .o = show,
                                                               .p = show ? enabled : empty,
                                                               .u = dirty,
                                                               .w = index,
                                                               .x = true};
                renderer->fields.o[index] = renderer->fields.n[index] = section;
                if (dirty)
                    CHECK(mc_render_global_pointer_append(renderer, renderer->fields.m, section,
                                                          site, &error));
                if (show) {
                    CHECK(define_tile(base + drawn * 2, lx, py, lz, 0) == 0);
                    CHECK(define_tile(base + drawn * 2 + 1, lx, py, lz, 1) == 0);
                    ++drawn;
                }
            }
    CHECK(drawn == visible);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3 || (strcmp(argv[1], "17") != 0 && strcmp(argv[1], "33") != 0) ||
        (strcmp(argv[2], "0") != 0 && strcmp(argv[2], "1") != 0)) {
        fprintf(stderr, "usage: mc_benchmark_terrain_frames 17|33 0|1\n");
        return 2;
    }
    const int dimension = atoi(argv[1]), mode = atoi(argv[2]);
    DWORD_PTR available, machine;
    CHECK(GetProcessAffinityMask(GetCurrentProcess(), &available, &machine));
    CHECK(SetThreadAffinityMask(GetCurrentThread(), available & (0 - available)) != 0);
    mc_java_random_profile *random = NULL;
    mc_entity_classes *entities = NULL;
    mc_client_render_engine_classes *graphics = NULL;
    CHECK(mc_java_random_profile_create(SIZE_MAX, &random) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, random, NULL, &entities) == MC_ENTITY_OK);
    CHECK(mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_math_classes_ensure(math, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_client_render_engine_classes_create(NULL, &graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_display_create("Authored terrain render benchmark", side, side, true, &display,
                                   &display_error));
    finish_draws = (finish_fn)SDL_GL_GetProcAddress("glFinish");
    CHECK(finish_draws != NULL);
    const mc_client_display_info *device = mc_client_display_info_get(display);
    CHECK(device != NULL);
    printf("DEVICE %s | %s | %s\n", device->vendor, device->renderer, device->version);
    mc_game_settings *settings = NULL;
    CHECK(mc_game_settings_create_defaults(NULL, &settings, NULL) == MC_GAME_SETTINGS_OK);
    mc_game_settings_state settings_state;
    CHECK(mc_game_settings_read_state(settings, &settings_state));
    settings_state.values.render_distance = 3;
    settings_state.values.advanced_opengl = false;
    CHECK(mc_game_settings_set_values(settings, &settings_state.values, NULL) ==
          MC_GAME_SETTINGS_OK);
    CHECK(mc_local_player_allocate(entities, &camera, NULL) == MC_ENTITY_OK);
    camera->base.x = camera->base.bl = camera->base.z = camera->base.bn = 0.0;
    client_application *app = calloc(1, sizeof(*app));
    CHECK(app != NULL);
    app->settings = settings;
    app->entities = entities;
    app->i = camera;
    CHECK(mc_client_render_global_allocate(graphics, NULL, &renderer, &error) ==
          MC_CLIENT_RENDER_GLOBAL_OK);
    renderer->display = display;
    renderer->fields.t = app;
    renderer->fields.H = 3;
    renderer->fields.R = -1;
    mc_client_display_list_range *lists = NULL;
    CHECK(configure_view() == 0 && prepare_scene(dimension, &lists) == 0);
    const size_t sections = renderer->o_length;
    for (size_t i = 0; i < warmup; ++i)
        CHECK(render(mode, i) == 0);
    LARGE_INTEGER frequency, start, stop;
    CHECK(QueryPerformanceFrequency(&frequency) && frequency.QuadPart > 0);
    double times[samples];
    for (size_t i = 0; i < samples; ++i) {
        CHECK(QueryPerformanceCounter(&start));
        CHECK(render(mode, i) == 0);
        CHECK(QueryPerformanceCounter(&stop) && stop.QuadPart >= start.QuadPart);
        times[i] = 1000.0 * (double)(stop.QuadPart - start.QuadPart) / (double)frequency.QuadPart;
    }
    uint64_t final_hash, next_hash;
    size_t covered, covered_next;
    CHECK(pixels_hash(&final_hash, &covered) == 0);
    CHECK(render(mode, samples) == 0 && pixels_hash(&next_hash, &covered_next) == 0);
    uint64_t order_hash = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < sections; ++i)
        order_hash =
            (order_hash ^ (uint32_t)renderer->fields.n[i]->fields.w) * UINT64_C(1099511628211);
    printf("PROOF %zu %zu %" PRIu64 " %" PRIu64 " %zu %zu %" PRIu64 " %" PRIu64 "\n", sections,
           renderer->fields.m->count, final_hash, next_hash, covered, covered_next, order_hash,
           work_hash);
    CHECK(mc_client_display_lists_retire(&lists, &display_error));
    CHECK(mc_client_render_global_retire(&renderer, &error) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_client_render_engine_classes_release(graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_display_destroy(display, &display_error));
    mc_entity_release(camera);
    CHECK(mc_entity_classes_free(entities) == MC_ENTITY_OK);
    CHECK(mc_math_classes_release(&math, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_game_settings_retire(&settings, NULL) == MC_GAME_SETTINGS_OK);
    mc_java_random_profile_free(random);
    free(app);
    for (size_t i = 0; i < samples; ++i)
        printf("%.7f\n", times[i]);
    return 0;
}
