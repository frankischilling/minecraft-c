#include "client/display/camera_surface.h"
#include "client/display/world_frame_surface.h"
#include "client/render_engine/ge.h"
#include "client/render_engine/internal.h"
#include "mc/client/world/frustum.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

static unsigned int checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "frustum check failed at %d: %s\n", __LINE__, #condition);             \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool constructor_limits(mc_entity_classes *entities, mc_java_random_profile *profile) {
    for (size_t maximum = 0; maximum < 28; ++maximum) {
        mc_client_render_classes *models = NULL;
        mc_client_render_engine_classes *graphics = NULL;
        CHECK(mc_client_render_classes_create(entities, profile, &models) == MC_CLIENT_MODEL_OK);
        mc_client_render_engine_options options = mc_client_render_engine_default_options();
        options.max_constructor_allocations = maximum;
        CHECK(mc_client_render_engine_classes_create(&options, &graphics) ==
              MC_CLIENT_RENDER_ENGINE_OK);
        const mc_client_source_dr *dr = NULL;
        mc_client_frustum_error error;
        mc_client_frustum_state state;
        const mc_client_frustum_status status =
            mc_client_frustum_update(models, graphics, NULL, NULL, &dr, &error);
        CHECK(mc_client_frustum_inspect(models, &state) && dr == NULL);
        if (maximum < 27) {
            CHECK(status == MC_CLIENT_FRUSTUM_LIMIT && state.failed && !state.static_published &&
                  state.allocations == maximum);
            CHECK(mc_client_frustum_update(models, graphics, NULL, NULL, &dr, &error) ==
                      MC_CLIENT_FRUSTUM_CLASS_FAILED &&
                  mc_client_frustum_inspect(models, &state) && state.allocations == maximum);
        } else
            CHECK(status == MC_CLIENT_FRUSTUM_NATIVE_FAILED && state.initialized &&
                  state.static_published && state.allocations == 27 && error.site.pc == 31);
        CHECK(mc_client_render_classes_free(models) == MC_CLIENT_MODEL_BUSY);
        CHECK(mc_client_frustum_namespace_retire(models, &error) == MC_CLIENT_FRUSTUM_OK);
        CHECK(mc_client_render_engine_classes_release(graphics) == MC_CLIENT_RENDER_ENGINE_OK);
        CHECK(mc_client_render_classes_free(models) == MC_CLIENT_MODEL_OK);
    }
    return true;
}

static bool buffer_controls(mc_client_render_engine_classes *graphics) {
    mc_client_render_arena arena = {.options = mc_client_render_engine_default_options()};
    mc_client_graphics_error error = {0};
    const mc_client_graphics_site site = {"w", "b", "()V", 65};
    mc_client_direct_float_buffer *buffer = NULL;
    CHECK(mc_client_ge_e(graphics, &arena, 4, site, &buffer, &error));
    void *storage = NULL;
    size_t remaining = 0;
    CHECK(mc_client_ge_float_writable_remaining(buffer, &storage, &remaining, site, &error) &&
          remaining == 4);
    static const uint32_t data[] = {0x80000000u, 0x00000001u, 0x3f800000u, 0x7fc12345u};
    memcpy(storage, data, sizeof(data));
    CHECK(buffer->position == 0 && mc_client_ge_float_flip(buffer, site, &error) &&
          buffer->limit == 0 && mc_client_ge_float_limit(buffer, 4, site, &error));
    float copied[4];
    CHECK(mc_client_ge_float_get_array(buffer, copied, 4, site, &error) && buffer->position == 4 &&
          memcmp(copied, data, sizeof(data)) == 0);
    CHECK(!mc_client_ge_float_get_array(buffer, copied, 1, site, &error) &&
          error.status == MC_CLIENT_GRAPHICS_BUFFER_UNDERFLOW && buffer->position == 4 &&
          memcmp(copied, data, sizeof(data)) == 0);
    CHECK(!mc_client_ge_float_limit(buffer, 5, site, &error) &&
          error.status == MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_ARGUMENT && buffer->limit == 4);
    CHECK(mc_client_ge_float_clear(buffer, site, &error) && buffer->position == 0 &&
          buffer->limit == 4);
    mc_client_render_arena_free(&arena);
    return true;
}

static bool matrices(mc_client_display *display, unsigned int mode) {
    mc_client_display_error error;
    CHECK(mc_client_display_source_matrix_mode(display, 5889, &error));
    CHECK(mc_client_display_source_load_identity(display, &error));
    if (mode == 1)
        CHECK(mc_client_display_source_translate(display, 0.25f, -0.5f, 0.75f, &error));
    if (mode >= 2)
        CHECK(mc_client_display_source_perspective(display, 90, mode == 2 ? 1.0f : 2.0f, 1, 17,
                                                   &error));
    CHECK(mc_client_display_source_matrix_mode(display, 5888, &error));
    CHECK(mc_client_display_source_load_identity(display, &error));
    if (mode == 1)
        CHECK(mc_client_display_source_translate(display, 1, -2, -4, &error));
    if (mode == 2) {
        CHECK(mc_client_display_source_translate(display, 3.25f, -0.5f, -4, &error));
        CHECK(mc_client_display_source_scale(display, 2, 0.5, 4, &error));
    }
    if (mode == 3) {
        CHECK(mc_client_display_source_translate(display, -0.5f, 1.25f, -8, &error));
        CHECK(mc_client_display_source_scale(display, 1, 2, 0.5, &error));
    }
    return true;
}

static bool observe(mc_client_render_classes *models, const mc_client_source_dr *dr,
                    unsigned int mode) {
    mc_client_frustum_state state;
    CHECK(mc_client_frustum_inspect(models, &state));
    CHECK(state.initialized && !state.failed && state.returned && state.static_published &&
          state.singleton == dr && state.matrix_stores == 16 && state.plane_stores == 24 &&
          state.normalized_planes == 6 && state.last_store_pc == 74 &&
          state.buffers[0].position == 16 && state.buffers[1].position == 16 &&
          state.buffers[2].position == 0);
    for (size_t i = 0; i < 3; ++i)
        CHECK(state.buffers[i].capacity == 16 && state.buffers[i].limit == 16 &&
              state.buffers[i].direct && state.buffers[i].native_order);
    for (size_t row = 0; row < 16; ++row)
        for (size_t col = 0; col < 16; ++col)
            if (row >= 6 || col >= 4)
                CHECK(bits(state.planes[row][col]) == 0);
    static const double boxes[][6] = {{-0.5, -0.5, -0.5, 0.5, 0.5, 0.5},
                                      {2, 2, 2, 3, 3, 3},
                                      {-1, -1, -1, 1, 1, 1},
                                      {1, 0, 0, 1, 0.25, 0.25},
                                      {-100, -100, -100, 100, 100, 100},
                                      {0, 0, 0, 0, 0, 0},
                                      {2, 2, 2, -2, -2, -2}};
    unsigned int mask = 0;
    mc_client_frustum_error error;
    for (size_t i = 0; i < sizeof(boxes) / sizeof(boxes[0]); ++i) {
        bool visible;
        CHECK(mc_client_frustum_box(dr, boxes[i][0], boxes[i][1], boxes[i][2], boxes[i][3],
                                    boxes[i][4], boxes[i][5], &visible,
                                    &error) == MC_CLIENT_FRUSTUM_OK);
        if (visible)
            mask |= 1u << i;
    }
    uint64_t nan_bits = UINT64_C(0x7ff8000000000000);
    double nan;
    memcpy(&nan, &nan_bits, sizeof(nan));
    bool visible = true;
    CHECK(mc_client_frustum_box(dr, nan, -1, -1, nan, 1, 1, &visible, &error) ==
              MC_CLIENT_FRUSTUM_OK &&
          !visible);
    printf("FRUSTUM %u 16 16 16 16 0 16", mode);
    const float *arrays[] = {state.projection, state.modelview, state.combined};
    for (size_t array = 0; array < 3; ++array)
        for (size_t i = 0; i < 16; ++i)
            printf(" %08" PRIx32, bits(arrays[array][i]));
    for (size_t plane = 0; plane < 6; ++plane)
        for (size_t component = 0; component < 4; ++component)
            printf(" %08" PRIx32, bits(state.planes[plane][component]));
    printf(" %u\n", mask);
    return true;
}

static bool run(void) {
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *entities = NULL;
    mc_client_render_classes *models = NULL;
    mc_client_render_engine_classes *graphics = NULL;
    mc_math_classes *math = NULL;
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &entities) == MC_ENTITY_OK);
    CHECK(constructor_limits(entities, profile));
    CHECK(mc_client_render_classes_create(entities, profile, &models) == MC_CLIENT_MODEL_OK);
    CHECK(mc_client_render_engine_classes_create(NULL, &graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(buffer_controls(graphics));
    CHECK(mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK);
    mc_client_display *display = NULL;
    mc_client_display_error display_error;
    CHECK(mc_client_display_create("Source frustum controls", 64, 64, true, &display,
                                   &display_error));
    mc_client_frustum_error error;
    mc_client_frustum_state state;
    const mc_client_source_dr *dr = NULL;
    CHECK(mc_client_frustum_update(models, graphics, NULL, math, &dr, &error) ==
              MC_CLIENT_FRUSTUM_NATIVE_FAILED &&
          dr == NULL && error.site.pc == 31 && mc_client_frustum_inspect(models, &state) &&
          state.initialized && state.static_published && state.matrix_stores == 0 &&
          state.buffers[0].position == 0);
    const mc_client_source_dr *identity = state.singleton;
    for (size_t row = 0; row < 16; ++row) {
        for (size_t previous = 0; previous < row; ++previous)
            CHECK(state.planes[row] != state.planes[previous]);
        for (size_t col = 0; col < 16; ++col)
            CHECK(bits(state.planes[row][col]) == 0);
    }
    mc_math_class_state math_state;
    CHECK(mc_math_classes_state(math, &math_state, NULL) == MC_MATH_CLASS_OK &&
          math_state.phase == MC_MATH_CLASS_COLD);
    CHECK(mc_client_frustum_update(models, graphics, display, NULL, &dr, &error) ==
              MC_CLIENT_FRUSTUM_CHILD_FAILED &&
          dr == NULL && error.site.pc == 35 && mc_client_frustum_inspect(models, &state) &&
          state.matrix_stores == 16 && state.plane_stores == 4 && state.normalized_planes == 0 &&
          state.last_store_pc == 1248);
    for (unsigned int mode = 0; mode < 4; ++mode) {
        CHECK(matrices(display, mode));
        CHECK(mc_client_frustum_update(models, graphics, display, math, &dr, &error) ==
                  MC_CLIENT_FRUSTUM_OK &&
              dr == identity && observe(models, dr, mode));
    }
    const unsigned int controls = _mm_getcsr();
    _mm_setcsr(controls | 0x8000u);
    const mc_client_frustum_status bad_fp =
        mc_client_frustum_update(models, graphics, display, math, &dr, &error);
    _mm_setcsr(controls);
    CHECK(bad_fp == MC_CLIENT_FRUSTUM_UNSUPPORTED_FP && error.site.pc == 111 &&
          mc_client_frustum_inspect(models, &state) && state.matrix_stores == 0 &&
          state.buffers[0].position == 16 && state.buffers[1].position == 16 && dr == identity);
    CHECK(mc_client_render_classes_retain(models, profile) == MC_CLIENT_MODEL_OK);
    CHECK(mc_client_frustum_namespace_retire(models, &error) == MC_CLIENT_FRUSTUM_BUSY);
    CHECK(mc_client_render_classes_release(models) == MC_CLIENT_MODEL_OK);
    CHECK(mc_client_frustum_namespace_retire(models, &error) == MC_CLIENT_FRUSTUM_OK);
    CHECK(mc_client_frustum_update(models, graphics, display, math, &dr, &error) ==
          MC_CLIENT_FRUSTUM_BUSY);
    CHECK(mc_client_display_check_gl(display, "source frustum", &display_error));
    CHECK(mc_client_display_destroy(display, &display_error));
    CHECK(mc_math_classes_release(&math, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_client_render_engine_classes_release(graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_classes_free(models) == MC_CLIENT_MODEL_OK);
    CHECK(mc_entity_classes_free(entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(profile);
    return true;
}

int main(void) {
    if (!run())
        return 1;
    printf("Source frustum: %u checks; canonical singleton and buffer prefix retained\n", checks);
    return 0;
}
