#include "client/display/camera_surface.h"
#include "client/display/world_frame_surface.h"
#include "client/render_engine/ge.h"
#include "client/render_engine/internal.h"
#include "client/world/frustum_internal.h"

#include <emmintrin.h>
#include <float.h>
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

/* Native dr storage for independent box predicates, before any singleton or GL
 * work. Its six plane rows are borrowed for each synchronous call. */
struct mc_client_source_dr {
    float **a;
    float *b, *c, *d;
};

static double box_double(uint64_t value) {
    double result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static float box_float(uint32_t value) {
    float result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

/* The previous dr body is the oracle. Each product and each x+y+z+w addition
 * stays a separate binary64 SSE operation, with the source corner order. */
static bool box_original(const float planes[6][4], const double box[6]) {
    for (size_t plane = 0; plane < 6; ++plane) {
        bool visible = false;
        for (unsigned int corner = 0; corner < 8; ++corner) {
            const double x = box[(corner & 1u) != 0 ? 3 : 0];
            const double y = box[(corner & 2u) != 0 ? 4 : 1];
            const double z = box[(corner & 4u) != 0 ? 5 : 2];
            __m128d value = _mm_mul_sd(_mm_set_sd((double)planes[plane][0]), _mm_set_sd(x));
            value =
                _mm_add_sd(value, _mm_mul_sd(_mm_set_sd((double)planes[plane][1]), _mm_set_sd(y)));
            value =
                _mm_add_sd(value, _mm_mul_sd(_mm_set_sd((double)planes[plane][2]), _mm_set_sd(z)));
            value = _mm_add_sd(value, _mm_set_sd((double)planes[plane][3]));
            if (_mm_cvtsd_f64(value) > 0.0) {
                visible = true;
                break;
            }
        }
        if (!visible)
            return false;
    }
    return true;
}

static bool box_compare(float planes[6][4], const double box[6], unsigned int outcomes[2]) {
    float *rows[6];
    for (size_t plane = 0; plane < 6; ++plane)
        rows[plane] = planes[plane];
    const mc_client_source_dr dr = {.a = rows};
    const bool expected = box_original((const float(*)[4])planes, box);
    bool visible = !expected;
    mc_client_frustum_error error;
    memset(&error, 0xa5, sizeof(error));
    CHECK(mc_client_frustum_box(&dr, box[0], box[1], box[2], box[3], box[4], box[5], &visible,
                                &error) == MC_CLIENT_FRUSTUM_OK &&
          visible == expected && error.status == MC_CLIENT_FRUSTUM_OK && error.operation == NULL &&
          !error.native_policy && error.site.class_name == NULL && error.site.method == NULL &&
          error.site.descriptor == NULL && error.site.pc == 0 && error.parent.class_name == NULL &&
          error.parent.method == NULL && error.parent.descriptor == NULL && error.parent.pc == 0);
    unsigned char sentinel[sizeof(error)];
    memset(sentinel, 0xa5, sizeof(sentinel));
    memcpy(&error, sentinel, sizeof(error));
    visible = !expected;
    CHECK(mc_client_frustum_box_test(&dr, box[0], box[1], box[2], box[3], box[4], box[5], &visible,
                                     &error, false) == MC_CLIENT_FRUSTUM_OK &&
          visible == expected && memcmp(&error, sentinel, sizeof(error)) == 0);
    ++outcomes[expected ? 1 : 0];
    return true;
}

static uint64_t box_random(uint64_t *state) {
    *state ^= *state << 13u;
    *state ^= *state >> 7u;
    *state ^= *state << 17u;
    return *state;
}

static bool box_differential_controls(void) {
    const unsigned int control = _mm_getcsr();
    CHECK((control & 0xffc0u) == 0x1f80u);
    const double tiny = box_double(UINT64_C(1));
    const double near_one = box_double(UINT64_C(0x3ff0000000000001));
    const double infinity = box_double(UINT64_C(0x7ff0000000000000));
    const double nan = box_double(UINT64_C(0x7ff8000000001234));
    const double boxes[][6] = {{-1, -1, -1, 1, 1, 1},
                               {1, 2, 3, -1, -2, -3},
                               {-0.0, 0.0, -0.0, 0.0, -0.0, 0.0},
                               {tiny, -tiny, tiny, -tiny, tiny, -tiny},
                               {-DBL_MIN, DBL_MIN, 0, DBL_MIN, -DBL_MIN, tiny},
                               {1, 1, 1, near_one, near_one, near_one},
                               {-1, -1, -near_one, -near_one, -near_one, -1},
                               {0, 0, 0, tiny, tiny, tiny},
                               {DBL_MAX, -DBL_MAX, DBL_MAX, -DBL_MAX, DBL_MAX, -DBL_MAX},
                               {-DBL_MAX, -DBL_MAX, -DBL_MAX, DBL_MAX, DBL_MAX, DBL_MAX},
                               {infinity, 0, 0, infinity, 1, 1},
                               {-infinity, 0, -infinity, infinity, 1, infinity},
                               {nan, -1, -1, nan, 1, 1},
                               {-1, nan, -1, 1, nan, 1},
                               {-1, -1, nan, 1, 1, nan},
                               {0, 0, 0, nan, infinity, -infinity}};
    float coefficients[][4] = {{0, 0, 0, 0},       {-0.0f, 0, -0.0f, -0.0f},
                               {1, 0, 0, 0},       {-1, 0, 0, 0},
                               {0, 1, 0, 0},       {0, 0, -1, 0},
                               {1, 1, 1, -3},      {-1, -1, -1, 3},
                               {1, -1, 1, 0},      {FLT_MAX, 1, -1, 0},
                               {FLT_MIN, 0, 0, 0}, {1, 1, -1, FLT_MIN},
                               {0, 0, 0, 1},       {0, 0, 0, -1},
                               {0, 0, 0, 0},       {0, 0, 0, 0},
                               {0, 0, 0, 0},       {0, 0, 0, 0}};
    coefficients[14][0] = box_float(UINT32_C(1));
    coefficients[15][1] = box_float(UINT32_C(0x7f800000));
    coefficients[16][2] = box_float(UINT32_C(0xff800000));
    coefficients[17][3] = box_float(UINT32_C(0x7fc12345));
    unsigned int outcomes[2] = {0};
    float planes[6][4] = {{0}};
    for (size_t selected = 0; selected < 6; ++selected) {
        for (size_t plane = 0; plane < 6; ++plane) {
            memset(planes[plane], 0, sizeof(planes[plane]));
            planes[plane][3] = 1;
        }
        for (size_t coefficient = 0; coefficient < sizeof(coefficients) / sizeof(*coefficients);
             ++coefficient) {
            memcpy(planes[selected], coefficients[coefficient], sizeof(planes[selected]));
            for (size_t box = 0; box < sizeof(boxes) / sizeof(*boxes); ++box)
                CHECK(box_compare(planes, boxes[box], outcomes));
        }
    }
    uint64_t random = UINT64_C(0x7f3d481a96e2b5c1);
    for (unsigned int sample = 0; sample < 4096; ++sample) {
        double box[6];
        for (size_t coordinate = 0; coordinate < 6; ++coordinate) {
            const uint64_t raw = box_random(&random);
            const uint64_t exponent =
                sample < 2048 ? 1000u + ((raw >> 52u) % 47u) : 1u + ((raw >> 52u) % 2046u);
            box[coordinate] = box_double((raw & UINT64_C(0x800fffffffffffff)) | (exponent << 52u));
        }
        for (size_t plane = 0; plane < 6; ++plane)
            for (size_t coefficient = 0; coefficient < 4; ++coefficient) {
                const uint32_t raw = (uint32_t)box_random(&random);
                const uint32_t exponent = 100u + ((raw >> 23u) % 56u);
                planes[plane][coefficient] =
                    box_float((raw & UINT32_C(0x807fffff)) | (exponent << 23u));
            }
        CHECK(box_compare(planes, box, outcomes));
    }
    CHECK(outcomes[0] != 0 && outcomes[1] != 0);
    float *rows[6];
    for (size_t plane = 0; plane < 6; ++plane)
        rows[plane] = planes[plane];
    mc_client_source_dr dr = {.a = rows};
    mc_client_frustum_error error;
    bool visible = true;
    CHECK(mc_client_frustum_box(NULL, 0, 0, 0, 1, 1, 1, &visible, &error) ==
              MC_CLIENT_FRUSTUM_SOURCE_NULL &&
          visible && !error.native_policy && error.site.pc == 11);
    dr.a = NULL;
    CHECK(mc_client_frustum_box(&dr, 0, 0, 0, 1, 1, 1, &visible, &error) ==
              MC_CLIENT_FRUSTUM_SOURCE_NULL &&
          visible && !error.native_policy && error.site.pc == 16);
    memset(&error, 0xa5, sizeof(error));
    CHECK(mc_client_frustum_box_test(&dr, 0, 0, 0, 1, 1, 1, &visible, &error, false) ==
              MC_CLIENT_FRUSTUM_SOURCE_NULL &&
          visible && error.status == MC_CLIENT_FRUSTUM_SOURCE_NULL && error.operation != NULL &&
          !error.native_policy && error.site.pc == 16 && error.parent.class_name == NULL);
    dr.a = rows;
    _mm_setcsr(control | 0x8000u);
    const mc_client_frustum_status fp_status =
        mc_client_frustum_box(&dr, 0, 0, 0, 1, 1, 1, &visible, &error);
    _mm_setcsr(control);
    CHECK(fp_status == MC_CLIENT_FRUSTUM_UNSUPPORTED_FP && visible && error.native_policy);
    CHECK(mc_client_frustum_box(&dr, 0, 0, 0, 1, 1, 1, NULL, &error) ==
              MC_CLIENT_FRUSTUM_ARGUMENT &&
          error.native_policy);
    return true;
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
    CHECK(box_differential_controls());
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
