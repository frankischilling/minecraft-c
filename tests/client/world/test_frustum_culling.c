#include "client/application_state.h"
#include "client/display/camera_surface.h"
#include "client/world/source_chunk_renderer_internal.h"
#include "mc/client/world/frustum_culling.h"
#include "mc/core/java/java.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

static unsigned int checks;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "culling check at %d: %s\n", __LINE__, #c);                            \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

static uint64_t bits(double v) {
    uint64_t r;
    memcpy(&r, &v, sizeof r);
    return r;
}

static bool prepare(mc_client_render_engine_classes *graphics, client_application *app, int32_t j,
                    mc_client_render_global **out) {
    mc_client_render_global_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL);
    CHECK(mc_client_render_global_allocate(graphics, NULL, out, error) ==
          MC_CLIENT_RENDER_GLOBAL_OK);
    mc_client_render_global *n = *out;
    n->fields.t = app;
    n->fields.j = j;
    n->fields.o = mc_render_global_allocate(n, 20 * sizeof(*n->fields.o),
                                            (mc_client_graphics_site){"n", "a", "()V", 239}, error);
    CHECK(n->fields.o != NULL);
    n->o_length = 20;
    for (size_t i = 0; i < 20; ++i) {
        mc_client_source_dk *dk = mc_source_dk_allocate(n, error);
        CHECK(dk != NULL);
        n->fields.o[i] = dk;
        dk->fields.E = i % 5 == 0;
        dk->fields.o = i % 3 == 0;
        if (dk->fields.E) {
            dk->fields.p = mc_render_global_allocate(
                n, 2 * sizeof(bool), (mc_client_graphics_site){"dk", "<init>", "()V", 16}, error);
            CHECK(dk->fields.p != NULL);
            dk->fields.p[0] = dk->fields.p[1] = true;
        }
        const double x = i % 2 == 0 ? 3 : 0;
        CHECK(mc_entity_bounds_create(app->entities, x - .25, -.25, -.25, x + .25, .25, .25,
                                      &dk->fields.v, NULL) == MC_ENTITY_OK);
    }
    free(error);
    return true;
}

static bool run(void) {
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *entities = NULL;
    mc_client_render_classes *models = NULL;
    mc_client_render_engine_classes *graphics = NULL;
    mc_math_classes *math = NULL;
    mc_client_display *display = NULL;
    client_application *app = calloc(1, sizeof(*app));
    CHECK(app != NULL);
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &entities) == MC_ENTITY_OK);
    app->entities = entities;
    CHECK(mc_client_render_classes_create(entities, profile, &models) == MC_CLIENT_MODEL_OK);
    CHECK(mc_client_render_engine_classes_create(NULL, &graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK);
    mc_client_display_error de;
    CHECK(mc_client_display_create("Source culling controls", 64, 64, true, &display, &de));
    CHECK(mc_client_display_source_matrix_mode(display, 5889, &de) &&
          mc_client_display_source_load_identity(display, &de));
    CHECK(mc_client_display_source_matrix_mode(display, 5888, &de) &&
          mc_client_display_source_load_identity(display, &de));
    double tiny;
    uint64_t raw = 1;
    memcpy(&tiny, &raw, sizeof tiny);
    const double centers[4][3] = {{0, 0, 0}, {10, -3, 5}, {-0.0, tiny, 0}, {-16, 2, 17}};
    mc_client_culling_error error;
    for (unsigned int mode = 0; mode < 4; ++mode) {
        mc_client_source_frustum *sr = NULL;
        CHECK(mc_client_source_frustum_begin(models, graphics, display, math, &sr, &error) ==
              MC_CLIENT_FRUSTUM_OK);
        const double *v = centers[mode];
        CHECK(mc_client_source_frustum_recenter(sr, v[0], v[1], v[2], &error) ==
              MC_CLIENT_FRUSTUM_OK);
        mc_client_source_frustum_state state;
        CHECK(mc_client_source_frustum_inspect(sr, &state) && state.constructed &&
              state.source_pc == 16);
        const double b[3][6] = {{v[0] - .5, v[1] - .5, v[2] - .5, v[0] + .5, v[1] + .5, v[2] + .5},
                                {v[0] + 2, v[1], v[2], v[0] + 3, v[1] + .5, v[2] + .5},
                                {v[0] - 1, v[1] - 1, v[2] - 1, v[0] + 1, v[1] + 1, v[2] + 1}};
        unsigned int mask = 0;
        for (unsigned int i = 0; i < 3; ++i) {
            bool a, c;
            CHECK(mc_client_source_frustum_box(sr, b[i][0], b[i][1], b[i][2], b[i][3], b[i][4],
                                               b[i][5], &a, &error) == MC_CLIENT_FRUSTUM_OK);
            mc_entity_bounds *box = NULL;
            CHECK(mc_entity_bounds_create(entities, b[i][0], b[i][1], b[i][2], b[i][3], b[i][4],
                                          b[i][5], &box, NULL) == MC_ENTITY_OK);
            CHECK(mc_client_source_frustum_bounds(sr, box, &c, &error) == MC_CLIENT_FRUSTUM_OK &&
                  a == c);
            CHECK(mc_entity_bounds_release(entities, box) == MC_ENTITY_OK);
            if (a)
                mask |= 1u << i;
        }
        printf("WRAP %u %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %u\n", mode, bits(state.b),
               bits(state.c), bits(state.d), mask);
        bool unchanged = true;
        CHECK(mc_client_source_frustum_bounds(sr, NULL, &unchanged, &error) ==
                  MC_CLIENT_FRUSTUM_SOURCE_NULL &&
              error.site.pc == 2 && unchanged);
        CHECK(mc_client_frustum_namespace_retire(models, NULL) == MC_CLIENT_FRUSTUM_BUSY);
        CHECK(mc_client_source_frustum_retire(&sr, &error) == MC_CLIENT_FRUSTUM_OK && sr == NULL);
    }
    mc_client_source_frustum *sr = NULL;
    CHECK(mc_client_source_frustum_begin(models, graphics, display, math, &sr, &error) ==
          MC_CLIENT_FRUSTUM_OK);
    const int32_t initial[] = {0, 4, 10, INT32_MAX};
    for (unsigned int mode = 0; mode < 4; ++mode) {
        mc_client_render_global *n = NULL;
        CHECK(prepare(graphics, app, initial[mode], &n));
        mc_client_culling_call *call = NULL;
        CHECK(mc_client_render_global_cull_begin(n, sr, 0.0f, &call, &error) ==
              MC_CLIENT_FRUSTUM_OK);
        mc_client_culling_state state;
        CHECK(mc_client_render_global_cull_inspect(call, &state) && state.returned &&
              state.source_pc == 73 && state.visited == 20 && state.empty_skips == 4);
        unsigned int mask = 0;
        for (size_t i = 0; i < 20; ++i)
            if (n->fields.o[i]->fields.o)
                mask |= 1u << i;
        printf("CULL %u %d %d %u\n", mode, initial[mode], n->fields.j, mask);
        CHECK(n->fields.j == mc_java_i32_add(initial[mode], 1) && state.tested == state.stores);
        mc_client_culling_call *other = NULL;
        CHECK(mc_client_render_global_cull_begin(n, sr, 0, &other, &error) ==
                  MC_CLIENT_FRUSTUM_BUSY &&
              other == NULL);
        CHECK(mc_client_render_global_retire(&n, NULL) == MC_CLIENT_RENDER_GLOBAL_BUSY);
        CHECK(mc_client_render_global_cull_retire(&call, &error) == MC_CLIENT_FRUSTUM_OK &&
              call == NULL);
        CHECK(mc_client_render_global_retire(&n, NULL) == MC_CLIENT_RENDER_GLOBAL_OK && n == NULL);
    }
    /* Missing pass-array failure preserves earlier visibility stores and j. */
    mc_client_render_global *n = NULL;
    CHECK(prepare(graphics, app, 0, &n));
    n->fields.o[1]->fields.E = true;
    mc_client_culling_call *call = NULL;
    CHECK(mc_client_render_global_cull_begin(n, sr, 0, &call, &error) ==
              MC_CLIENT_FRUSTUM_SOURCE_NULL &&
          n->fields.j == 0 && error.child.pc == 14);
    CHECK(mc_client_render_global_cull_retire(&call, &error) == MC_CLIENT_FRUSTUM_OK);
    n->fields.o[1]->fields.E = false;
    CHECK(mc_client_render_global_cull_begin(n, NULL, 0, &call, &error) ==
              MC_CLIENT_FRUSTUM_SOURCE_NULL &&
          error.child.pc == 6 && n->fields.j == 0);
    CHECK(mc_client_render_global_cull_retire(&call, &error) == MC_CLIENT_FRUSTUM_OK);
    CHECK(mc_client_render_global_retire(&n, NULL) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_client_source_frustum_retire(&sr, &error) == MC_CLIENT_FRUSTUM_OK);
    CHECK(mc_client_frustum_namespace_retire(models, NULL) == MC_CLIENT_FRUSTUM_OK);
    CHECK(mc_client_display_check_gl(display, "source culling", &de));
    CHECK(mc_client_display_destroy(display, &de));
    CHECK(mc_math_classes_release(&math, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_client_render_engine_classes_release(graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_classes_free(models) == MC_CLIENT_MODEL_OK);
    CHECK(mc_entity_classes_free(entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(profile);
    free(app);
    return true;
}

int main(void) {
    if (!run())
        return 1;
    printf("Source culling: %u checks; wrapper and live DK fields retained\n", checks);
    return 0;
}
