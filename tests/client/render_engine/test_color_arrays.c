#include "client/render_engine/color_array_internal.h"
#include "client/render_engine/colorizer.h"
#include "client/render_engine/internal.h"
#include <stdio.h>

/* Authored/unexecuted real CPU owner contracts. No fake App/Pack/Display,
 * captured array or successful child is supplied. Active flags below are
 * declared native lifetime fixtures on genuine allocated storage. */
static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "color-array check%u line%d: %s\n", checks, __LINE__, #x);             \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static int arrays(void) {
    mc_client_argb_image *image = NULL;
    CHECK(mc_client_argb_image_create(3, 2, 4096, &image) == MC_CLIENT_RENDER_ENGINE_OK);
    for (int32_t y = 0; y < 2; ++y)
        for (int32_t x = 0; x < 3; ++x)
            CHECK(mc_client_argb_image_set_pixel(
                      image, x, y,
                      ((x + y) & 1 ? UINT32_C(0xff000000) : 0u) | ((uint32_t)(x * 31) << 16) |
                          ((uint32_t)(y * 67) << 8) | (uint32_t)(x + y)) ==
                  MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_color_array_error error;
    mc_client_color_array *array = NULL;
    CHECK(mc_client_color_array_from_image(image, NULL, &array, &error) ==
          MC_CLIENT_COLOR_ARRAY_OK);
    CHECK(error.site.pc == 32 && error.width == 3 && error.height == 2 && error.actual_length == 6);
    mc_client_color_array_state state;
    mc_client_argb_image_state input;
    CHECK(mc_client_color_array_inspect(array, &state) &&
          state.phase == MC_CLIENT_COLOR_ARRAY_READY && state.length == 6);
    CHECK(mc_client_argb_image_inspect(image, &input) && state.bits != input.pixels);
    for (size_t i = 0; i < 6; ++i)
        CHECK(state.bits[i] == input.pixels[i]);
    CHECK(mc_client_color_array_write(array, 0, UINT32_C(0x12345678)) == MC_CLIENT_COLOR_ARRAY_OK);
    CHECK(input.pixels[0] != UINT32_C(0x12345678));
    CHECK(mc_client_color_array_fill(image, array, &error) == MC_CLIENT_COLOR_ARRAY_OK &&
          error.site.pc == 25);
    CHECK(state.bits[0] == input.pixels[0]);

    mc_client_color_array *alias = array;
    CHECK(mc_client_color_array_retain(alias) == MC_CLIENT_COLOR_ARRAY_OK);
    array->active_calls = 1;
    CHECK(mc_client_color_array_release(&alias, &error) == MC_CLIENT_COLOR_ARRAY_OK &&
          alias == NULL);
    CHECK(mc_client_color_array_release(&array, &error) == MC_CLIENT_COLOR_ARRAY_BUSY &&
          array != NULL);
    CHECK(array->holds == 1 && array->active_calls == 1);
    array->active_calls = 0;
    CHECK(mc_client_color_array_release(&array, &error) == MC_CLIENT_COLOR_ARRAY_OK &&
          array == NULL);

    for (size_t limit = 0; limit < 3; ++limit) {
        mc_client_color_array_limits limits = mc_client_color_array_default_limits();
        limits.max_allocations = limit;
        CHECK(mc_client_color_array_from_image(image, &limits, &array, &error) ==
              (limit < 2 ? MC_CLIENT_COLOR_ARRAY_LIMIT : MC_CLIENT_COLOR_ARRAY_OK));
        if (limit == 0)
            CHECK(array == NULL && error.site.pc == 13 && error.width == 3 && error.height == 2);
        if (limit == 1) {
            CHECK(mc_client_color_array_inspect(array, &state) &&
                  state.phase == MC_CLIENT_COLOR_ARRAY_FAILED);
            CHECK(state.native_allocations == 1 && state.bits == NULL && state.length == 6 &&
                  error.site.pc == 13);
        }
        CHECK(mc_client_color_array_release(&array, &error) == MC_CLIENT_COLOR_ARRAY_OK &&
              array == NULL);
    }
    CHECK(mc_client_color_array_from_image(NULL, NULL, &array, &error) ==
          MC_CLIENT_COLOR_ARRAY_NULL_REFERENCE);
    CHECK(error.site.pc == 1 && array == NULL);
    CHECK(mc_client_color_array_create(2, NULL, (mc_client_graphics_site){NULL, NULL, NULL, -1},
                                       &array, &error) == MC_CLIENT_COLOR_ARRAY_OK);
    CHECK(mc_client_color_array_write(array, 0, UINT32_C(0x87654321)) == MC_CLIENT_COLOR_ARRAY_OK);
    CHECK(mc_client_color_array_fill(image, array, &error) == MC_CLIENT_COLOR_ARRAY_UNIMPLEMENTED &&
          error.site.pc == 20);
    CHECK(mc_client_color_array_inspect(array, &state) && state.bits[0] == UINT32_C(0x87654321));
    CHECK(mc_client_color_array_fill(image, NULL, &error) == MC_CLIENT_COLOR_ARRAY_UNIMPLEMENTED &&
          error.site.pc == 20);
    CHECK(mc_client_color_array_fill(NULL, array, &error) == MC_CLIENT_COLOR_ARRAY_NULL_REFERENCE &&
          error.site.pc == 1);
    CHECK(mc_client_color_array_release(&array, &error) == MC_CLIENT_COLOR_ARRAY_OK);
    mc_client_argb_image_release(image);
    return 0;
}

static int classes(void) {
    mc_client_render_engine_classes *classes = NULL;
    CHECK(mc_client_render_engine_classes_create(NULL, &classes) == MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_colorizer_error error;
    mc_client_colorizer_state states[MC_CLIENT_COLORIZER_COUNT];
    for (size_t k = 0; k < MC_CLIENT_COLORIZER_COUNT; ++k) {
        CHECK(mc_client_colorizer_inspect(classes, (mc_client_colorizer_kind)k, &states[k]));
        CHECK(states[k].phase == MC_CLIENT_COLORIZER_RAW && states[k].a == NULL);
        CHECK(mc_client_colorizer_initialize(classes, (mc_client_colorizer_kind)k, NULL, &error) ==
              MC_CLIENT_COLORIZER_OK);
        CHECK(mc_client_colorizer_inspect(classes, (mc_client_colorizer_kind)k, &states[k]));
        CHECK(states[k].phase == MC_CLIENT_COLORIZER_READY && states[k].last_store_pc == 4);
        mc_client_color_array_state array;
        CHECK(mc_client_color_array_inspect(states[k].a, &array) && array.length == 65536 &&
              array.native_holds == 1);
        for (size_t i = 0; i < array.length; ++i)
            CHECK(array.bits[i] == 0);
    }
    CHECK(states[0].a != states[1].a && states[1].a != states[2].a && states[0].a != states[2].a);
    CHECK(classes->ge_phase == MC_CLIENT_RENDER_ENGINE_RAW &&
          classes->nw.phase == MC_CLIENT_RENDER_ENGINE_RAW);
    CHECK(classes->a == NULL && classes->b == NULL && classes->native_resource_count == 0);
    mc_client_color_array *array = NULL;
    CHECK(mc_client_color_array_create(2, NULL, (mc_client_graphics_site){NULL, NULL, NULL, -1},
                                       &array, NULL) == MC_CLIENT_COLOR_ARRAY_OK);
    for (size_t k = 0; k < MC_CLIENT_COLORIZER_COUNT; ++k)
        CHECK(mc_client_colorizer_set(classes, (mc_client_colorizer_kind)k, array, NULL, &error) ==
              MC_CLIENT_COLORIZER_OK);
    CHECK(array->holds == 4);
    CHECK(mc_client_colorizer_set(classes, MC_CLIENT_COLORIZER_WATER, array, NULL, &error) ==
          MC_CLIENT_COLORIZER_OK);
    CHECK(array->holds == 4); /* Same-array gain-before-store, no leak or extra copy. */
    CHECK(mc_client_color_array_write(array, 1, UINT32_C(0xaabbccdd)) == MC_CLIENT_COLOR_ARRAY_OK);
    for (size_t k = 0; k < MC_CLIENT_COLORIZER_COUNT; ++k) {
        CHECK(mc_client_colorizer_inspect(classes, (mc_client_colorizer_kind)k, &states[k]) &&
              states[k].a == array);
        mc_client_color_array_state value;
        CHECK(mc_client_color_array_inspect(states[k].a, &value) &&
              value.bits[1] == UINT32_C(0xaabbccdd));
    }
    CHECK(mc_client_colorizer_set(classes, MC_CLIENT_COLORIZER_WATER, NULL, NULL, &error) ==
          MC_CLIENT_COLORIZER_OK);
    CHECK(array->holds == 3);
    CHECK(mc_client_colorizer_set(classes, MC_CLIENT_COLORIZER_WATER, array, NULL, &error) ==
          MC_CLIENT_COLORIZER_OK);
    mc_client_color_array *borrowed = array;
    CHECK(mc_client_color_array_release(&array, NULL) == MC_CLIENT_COLOR_ARRAY_OK && array == NULL);
    borrowed->active_calls = 1;
    CHECK(mc_client_render_engine_classes_retain(classes) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_OK &&
          classes->holds == 1);
    CHECK(mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_BUSY);
    CHECK(classes->holds == 1 && borrowed->holds == 3 &&
          mc_client_colorizer_occurrences(classes, borrowed) == 3);
    CHECK(mc_client_color_array_retain(borrowed) == MC_CLIENT_COLOR_ARRAY_OK);
    array = borrowed; /* A real independent external occurrence permits parent retirement. */
    CHECK(mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_OK);
    classes = NULL;
    CHECK(array->holds == 1 && array->active_calls == 1 && array->bits[1] == UINT32_C(0xaabbccdd));
    CHECK(mc_client_color_array_release(&array, NULL) == MC_CLIENT_COLOR_ARRAY_BUSY &&
          array == borrowed);
    array->active_calls = 0;
    CHECK(mc_client_color_array_release(&array, NULL) == MC_CLIENT_COLOR_ARRAY_OK && array == NULL);

    CHECK(mc_client_render_engine_classes_create(NULL, &classes) == MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_colorizer_options options = mc_client_colorizer_default_options();
    options.initial_array[MC_CLIENT_COLORIZER_WATER].max_allocations = 1;
    CHECK(mc_client_colorizer_initialize(classes, MC_CLIENT_COLORIZER_WATER, &options, &error) ==
          MC_CLIENT_COLORIZER_ARRAY_FAILED);
    CHECK(error.array.status == MC_CLIENT_COLOR_ARRAY_LIMIT && error.array.site.pc == 2);
    CHECK(mc_client_colorizer_inspect(classes, MC_CLIENT_COLORIZER_WATER, &states[0]));
    CHECK(states[0].phase == MC_CLIENT_COLORIZER_FAILED && states[0].a == NULL &&
          states[0].failed_candidate != NULL);
    CHECK(states[0].last_store_pc == 0);
    const mc_client_color_array *failed = states[0].failed_candidate;
    CHECK(mc_client_colorizer_initialize(classes, MC_CLIENT_COLORIZER_WATER, NULL, &error) ==
          MC_CLIENT_COLORIZER_CLASS_FAILED);
    CHECK(mc_client_colorizer_inspect(classes, MC_CLIENT_COLORIZER_WATER, &states[0]) &&
          states[0].failed_candidate == failed);
    CHECK(mc_client_colorizer_initialize(classes, MC_CLIENT_COLORIZER_GRASS, NULL, &error) ==
          MC_CLIENT_COLORIZER_OK);
    CHECK(mc_client_colorizer_inspect(classes, MC_CLIENT_COLORIZER_FOLIAGE, &states[2]) &&
          states[2].phase == MC_CLIENT_COLORIZER_RAW);
    CHECK(classes->ge_phase == MC_CLIENT_RENDER_ENGINE_RAW &&
          classes->nw.phase == MC_CLIENT_RENDER_ENGINE_RAW);
    CHECK(mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_OK);
    return 0;
}

int main(void) {
    CHECK(arrays() == 0);
    CHECK(classes() == 0);
    printf("Actual color array/class owner contracts: %u checks\n", checks);
    return 0;
}
