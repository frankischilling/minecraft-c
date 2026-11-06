#include "mc/client/gui/scaled_resolution.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

/* Literal source-contract cases, not executed-original observations.
 * qq.<init>(kv,II)V PCs35..88 select the factor; PCs91..142 retain exact
 * divisions then ceil/d2i. da.f() PCs13/21,34/42,43/45 establish mouse order.
 * Expected factors, integer results and binary64 bits are fixed literals;
 * this test does not calculate them with a second copy of the source loop. */
static unsigned int checks;
#define COUNT(values) (sizeof(values) / sizeof((values)[0]))
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static uint64_t double_bits(double value) {
    uint64_t result;
    _Static_assert(sizeof(result) == sizeof(value), "binary64 dimension bits");
    memcpy(&result, &value, sizeof(result));
    return result;
}

typedef struct resolution_case {
    int32_t display_width, display_height, gui_scale;
    int32_t factor, width, height;
    uint64_t width_bits, height_bits;
} resolution_case;

static const resolution_case resolutions[] = {
    {1, 1, 0, 1, 1, 1, UINT64_C(0x3ff0000000000000), UINT64_C(0x3ff0000000000000)},
    {319, 239, INT32_MAX, 1, 319, 239, UINT64_C(0x4073f00000000000), UINT64_C(0x406de00000000000)},
    {639, 480, 0, 1, 639, 480, UINT64_C(0x4083f80000000000), UINT64_C(0x407e000000000000)},
    {640, 479, 0, 1, 640, 479, UINT64_C(0x4084000000000000), UINT64_C(0x407df00000000000)},
    {640, 480, 0, 2, 320, 240, UINT64_C(0x4074000000000000), UINT64_C(0x406e000000000000)},
    {641, 481, 0, 2, 321, 241, UINT64_C(0x4074080000000000), UINT64_C(0x406e100000000000)},
    {854, 480, 0, 2, 427, 240, UINT64_C(0x407ab00000000000), UINT64_C(0x406e000000000000)},
    {960, 720, 0, 3, 320, 240, UINT64_C(0x4074000000000000), UINT64_C(0x406e000000000000)},
    {959, 720, 0, 2, 480, 360, UINT64_C(0x407df80000000000), UINT64_C(0x4076800000000000)},
    {960, 719, 0, 2, 480, 360, UINT64_C(0x407e000000000000), UINT64_C(0x4076780000000000)},
    {960, 720, 1, 1, 960, 720, UINT64_C(0x408e000000000000), UINT64_C(0x4086800000000000)},
    {960, 720, 2, 2, 480, 360, UINT64_C(0x407e000000000000), UINT64_C(0x4076800000000000)},
    {960, 720, -1, 1, 960, 720, UINT64_C(0x408e000000000000), UINT64_C(0x4086800000000000)},
    {960, 720, INT32_MIN, 1, 960, 720, UINT64_C(0x408e000000000000), UINT64_C(0x4086800000000000)},
    {960, 720, INT32_MAX, 3, 320, 240, UINT64_C(0x4074000000000000), UINT64_C(0x406e000000000000)},
    {1280, 960, 3, 3, 427, 320, UINT64_C(0x407aaaaaaaaaaaab), UINT64_C(0x4074000000000000)},
    {640000, 480000, 0, 1000, 640, 480, UINT64_C(0x4084000000000000), UINT64_C(0x407e000000000000)},
    {640000, 480000, INT32_MAX, 2000, 320, 240, UINT64_C(0x4074000000000000),
     UINT64_C(0x406e000000000000)},
    {INT32_MAX, INT32_MAX, 1, 1, INT32_MAX, INT32_MAX, UINT64_C(0x41dfffffffc00000),
     UINT64_C(0x41dfffffffc00000)},
    {INT32_MAX, INT32_MAX, 0, 1000, 2147484, 2147484, UINT64_C(0x4140624dd2d0e560),
     UINT64_C(0x4140624dd2d0e560)},
    {INT32_MAX, INT32_MAX, INT32_MAX, 6710886, 321, 321, UINT64_C(0x4074000013d80014),
     UINT64_C(0x4074000013d80014)},
    {INT32_MAX, 1, 0, 1, INT32_MAX, 1, UINT64_C(0x41dfffffffc00000), UINT64_C(0x3ff0000000000000)},
};

static int test_resolutions(void) {
    for (size_t i = 0; i < COUNT(resolutions); ++i) {
        const resolution_case *c = &resolutions[i];
        mc_scaled_resolution result;
        CHECK(
            mc_scaled_resolution_init(c->display_width, c->display_height, c->gui_scale, &result));
        CHECK(result.scale_factor == c->factor);
        CHECK(result.width == c->width && result.height == c->height);
        CHECK(double_bits(result.width_exact) == c->width_bits);
        CHECK(double_bits(result.height_exact) == c->height_bits);
    }
    return 0;
}

typedef struct mouse_case {
    int32_t initial_width, initial_height, gui_scale;
    int32_t display_width, display_height, event_x, event_y;
    int32_t x, y;
} mouse_case;

static const mouse_case mouse_positions[] = {
    {854, 480, 0, 854, 480, 0, 0, 0, 239},
    {854, 480, 0, 854, 480, 853, 479, 426, 0},
    {854, 480, 0, 854, 480, 854, 480, 427, -1},
    {854, 480, 0, 854, 480, 855, 481, 427, -1},
    {854, 480, 0, 854, 480, -1, -1, 0, 239},
    {854, 480, 0, 854, 480, -2, -2, -1, 240},
    {854, 480, 0, 854, 480, 100, 240, 50, 119},
    {641, 481, 0, 641, 481, 640, 480, 320, 0},
    {641, 481, 0, 641, 481, 320, 240, 160, 120},
    {641, 481, 0, 641, 481, 641, 481, 321, -1},
    {641, 481, 0, 641, 481, 0, 481, 0, -1},
    {641, 481, 0, 641, 481, 641, 0, 321, 240},
    {640, 480, 0, 640, 480, INT32_MAX, INT32_MAX, 0, 239},
    {2, 2, 1, 2, 2, INT32_MAX, INT32_MAX, -1, 2},
    {2, 2, 1, 2, 2, INT32_MIN, INT32_MIN, 0, 1},
    {1, 1, 0, 1, 1, INT32_MIN, INT32_MIN, INT32_MIN, INT32_MIN},
    {1, 1, 0, 1, 1, INT32_MAX, INT32_MAX, INT32_MAX, -INT32_MAX},
    {INT32_MAX, INT32_MAX, 1, 1, 1, INT32_MIN, INT32_MIN, INT32_MIN, -2},
    {INT32_MAX, INT32_MAX, 1, 1, 1, -1, -1, -INT32_MAX, -3},
    {1073741824, 1, 1, 1073741824, 1, 2, 0, -2, 0},
    {1920, 1080, 0, 1920, 1080, 1919, 1079, 479, 0},
    {854, 480, 0, 427, 240, 426, 239, 426, 0},
};

static int test_mouse_positions(void) {
    /* Includes unbounded negative/outside events and Java imul/isub overflow.
     * The last record uses current display dimensions different from the
     * dimensions that initialized the screen, as da.f reads them separately. */
    for (size_t i = 0; i < COUNT(mouse_positions); ++i) {
        const mouse_case *c = &mouse_positions[i];
        mc_scaled_resolution resolution;
        mc_gui_mouse_position position = {17, -29};
        CHECK(mc_scaled_resolution_init(c->initial_width, c->initial_height, c->gui_scale,
                                        &resolution));
        CHECK(mc_scaled_resolution_map_mouse(&resolution, c->display_width, c->display_height,
                                             c->event_x, c->event_y, &position));
        CHECK(position.x == c->x && position.y == c->y);
    }
    return 0;
}

static int test_invalid_arguments(void) {
    mc_scaled_resolution resolution;
    unsigned char before[sizeof(resolution)];
    memset(&resolution, 0xa5, sizeof(resolution));
    memcpy(before, &resolution, sizeof(before));
    CHECK(!mc_scaled_resolution_init(0, 480, 0, &resolution));
    CHECK(memcmp(before, &resolution, sizeof(before)) == 0);
    CHECK(!mc_scaled_resolution_init(-1, 480, 0, &resolution));
    CHECK(memcmp(before, &resolution, sizeof(before)) == 0);
    CHECK(!mc_scaled_resolution_init(640, 0, 0, &resolution));
    CHECK(memcmp(before, &resolution, sizeof(before)) == 0);
    CHECK(!mc_scaled_resolution_init(640, INT32_MIN, 0, &resolution));
    CHECK(memcmp(before, &resolution, sizeof(before)) == 0);
    CHECK(!mc_scaled_resolution_init(640, 480, 0, NULL));

    CHECK(mc_scaled_resolution_init(641, 481, 0, &resolution));
    mc_gui_mouse_position position = {17, -29};
    const mc_gui_mouse_position saved = position;
    CHECK(!mc_scaled_resolution_map_mouse(NULL, 641, 481, 0, 0, &position));
    CHECK(memcmp(&position, &saved, sizeof(position)) == 0);
    CHECK(!mc_scaled_resolution_map_mouse(&resolution, 0, 481, 0, 0, &position));
    CHECK(memcmp(&position, &saved, sizeof(position)) == 0);
    CHECK(!mc_scaled_resolution_map_mouse(&resolution, -1, 481, 0, 0, &position));
    CHECK(memcmp(&position, &saved, sizeof(position)) == 0);
    CHECK(!mc_scaled_resolution_map_mouse(&resolution, 641, 0, 0, 0, &position));
    CHECK(memcmp(&position, &saved, sizeof(position)) == 0);
    CHECK(!mc_scaled_resolution_map_mouse(&resolution, 641, INT32_MIN, 0, 0, &position));
    CHECK(memcmp(&position, &saved, sizeof(position)) == 0);
    CHECK(!mc_scaled_resolution_map_mouse(&resolution, 641, 481, 0, 0, NULL));
    resolution.width = 0;
    CHECK(!mc_scaled_resolution_map_mouse(&resolution, 641, 481, 0, 0, &position));
    CHECK(memcmp(&position, &saved, sizeof(position)) == 0);
    resolution.width = 321;
    resolution.height = -1;
    CHECK(!mc_scaled_resolution_map_mouse(&resolution, 641, 481, 0, 0, &position));
    CHECK(memcmp(&position, &saved, sizeof(position)) == 0);
    return 0;
}

int main(void) {
    if (test_resolutions() != 0 || test_mouse_positions() != 0 || test_invalid_arguments() != 0)
        return 1;
    printf("PASS: %u ScaledResolution source-contract checks\n", checks);
    return 0;
}
