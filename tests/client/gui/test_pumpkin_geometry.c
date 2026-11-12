/* Entered UQ.a(II) NW prefix only, with genuine fresh NW/class owners.
 * No Display, texture lookup, draw107 or whole HUD return is claimed. */
#include "client/gui/gui_ingame_pumpkin.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static unsigned int checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "Pumpkin check%u line%d: %s\n", checks, __LINE__, #condition);         \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static int profile(int32_t width, int32_t height, unsigned int index) {
    mc_client_render_engine_classes *graphics = NULL;
    mc_client_tessellator *nw = NULL;
    mc_client_graphics_error error;
    CHECK(mc_client_render_engine_classes_allocate_context(NULL, NULL, NULL, &graphics) ==
              MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_nw_active_use(graphics, (mc_client_graphics_site){"uq", "a", "(II)V", 50}, &nw,
                                  &error));
    mc_client_gui_pumpkin_geometry geometry;
    CHECK(mc_client_gui_pumpkin_geometry_entered(nw, width, height, &geometry) &&
          geometry.prefix_returned && geometry.source_pc == 103 && geometry.begin_attempts == 1 &&
          geometry.begin_returns == 1 && geometry.vertices == 4 && geometry.word_count == 48 &&
          !geometry.source_null_invocation);
    mc_client_nw_fields actual;
    CHECK(mc_client_nw_fields_get(nw, &actual) && actual.w && actual.h == 6 && actual.o == 48 &&
          actual.m && !actual.l && !actual.n &&
          memcmp(actual.g, geometry.words, sizeof(geometry.words)) == 0);
    /* Independent source four corners projected through NW's original
     * default quad expansion0,1,2,0,2,3; inactive color/normal/reserved words
     * remain the genuine fresh Java/C array defaults. */
    static const unsigned int corners[] = {0, 1, 2, 0, 2, 3};
    for (size_t i = 0; i < 6; ++i) {
        const unsigned int corner = corners[i];
        const bool right = corner == 1 || corner == 2, bottom = corner < 2;
        const uint32_t expected[] = {bits(right ? (float)width : 0.0f),
                                     bits(bottom ? (float)height : 0.0f),
                                     bits(-90.0f),
                                     bits(right ? 1.0f : 0.0f),
                                     bits(bottom ? 1.0f : 0.0f),
                                     0,
                                     0,
                                     0};
        for (size_t word = 0; word < 8; ++word)
            CHECK((uint32_t)geometry.words[i * 8 + word] == expected[word]);
    }
    printf("PUMPKIN_GEOMETRY %u %" PRId32 " %" PRId32 " 48\n", index, width, height);
    const mc_client_nw_fields before = actual;
    int32_t words[48];
    memcpy(words, actual.g, sizeof(words));
    /* Real second-begin rejection keeps the first batch exactly live. */
    CHECK(!mc_client_gui_pumpkin_geometry_entered(nw, 17, 19, &geometry) &&
          !geometry.prefix_returned && geometry.source_pc == 55 && geometry.begin_attempts == 1 &&
          geometry.begin_returns == 0 && geometry.vertices == 0 &&
          geometry.graphics.status == MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_STATE &&
          geometry.graphics.site.pc == 16 && !geometry.graphics.native_policy &&
          mc_client_nw_fields_get(nw, &actual) && actual.w == before.w && actual.h == before.h &&
          actual.o == before.o && actual.p == before.p && actual.m == before.m &&
          actual.l == before.l && actual.n == before.n &&
          memcmp(actual.g, words, sizeof(words)) == 0);
    CHECK(!mc_client_gui_pumpkin_geometry_entered(NULL, width, height, &geometry) &&
          geometry.source_null_invocation && geometry.source_pc == 55 &&
          geometry.begin_attempts == 1 && geometry.begin_returns == 0 && geometry.vertices == 0 &&
          geometry.graphics.status == MC_CLIENT_GRAPHICS_OK);
    CHECK(mc_client_render_engine_classes_release(graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    return 0;
}

int main(void) {
    CHECK(profile(320, 240, 0) == 0 && profile(321, 241, 1) == 0 && profile(0, 0, 2) == 0 &&
          profile(-17, 19, 3) == 0 && profile(INT32_MIN, INT32_MAX, 4) == 0);
    printf("Pumpkin entered geometry: %u checks; no Display or whole HUD claim\n", checks);
    return 0;
}
