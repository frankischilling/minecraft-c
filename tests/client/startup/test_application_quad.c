#include "client/startup/application_quad.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <xmmintrin.h>

static unsigned checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "application rectangle check %u failed at %d\n", checks, __LINE__);    \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

typedef struct rectangle_case {
    int32_t x, y, u, v, width, height;
    float vertices[4][5];
    double final_u, final_v;
} rectangle_case;

/* Expected positions/UVs are explicit, including signed-int wrap and the
 * binary32 rounding of16777217 before widening to the singleton's doubles. */
static const rectangle_case cases[] = {
    {10,
     20,
     0,
     0,
     256,
     256,
     {{10, 276, 0, 0, 1}, {266, 276, 0, 1, 1}, {266, 20, 0, 1, 0}, {10, 20, 0, 0, 0}},
     0,
     0},
    {INT32_MAX - 3,
     INT32_MIN + 2,
     INT32_MAX - 3,
     INT32_MIN + 2,
     8,
     -9,
     {{2147483648.0f, 2147483648.0f, 0, 8388608, 8388608},
      {-2147483648.0f, 2147483648.0f, 0, -8388608, 8388608},
      {-2147483648.0f, -2147483648.0f, 0, -8388608, -8388608},
      {2147483648.0f, -2147483648.0f, 0, 8388608, -8388608}},
     8388608,
     -8388608},
    {0,
     0,
     16777217,
     -16777217,
     1,
     1,
     {{0, 1, 0, 65536, -65536},
      {1, 1, 0, 65536.0078125f, -65536},
      {1, 0, 0, 65536.0078125f, -65536},
      {0, 0, 0, 65536, -65536}},
     65536,
     -65536},
    {10,
     10,
     32,
     64,
     -4,
     -8,
     {{10, 2, 0, 0.125f, 0.21875f},
      {6, 2, 0, 0.109375f, 0.21875f},
      {6, 10, 0, 0.109375f, 0.25f},
      {10, 10, 0, 0.125f, 0.25f}},
     0.125,
     0.25}};

int main(void) {
    const unsigned initial_fp = _mm_getcsr();
    _mm_setcsr(0x1f80u);
    mc_client_render_engine_classes *classes = NULL;
    CHECK(mc_client_render_engine_classes_allocate_context(NULL, NULL, NULL, &classes) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_application_quad_error error;
    mc_client_nw_state state;
    static const size_t order[] = {0, 1, 2, 0, 2, 3};
    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); ++c) {
        const rectangle_case *r = &cases[c];
        /* A real absent Display fails only after the real singleton's six
         * vertices and buffer cursor writes. This is CPU/failure coverage;
         * it never substitutes a successful native draw or window profile. */
        CHECK(!mc_client_application_textured_rectangle(classes, NULL, r->x, r->y, r->u, r->v,
                                                        r->width, r->height, &error));
        CHECK(error.pc == 152 && error.graphics.native_policy);
        CHECK(mc_client_nw_inspect(classes, &state));
        CHECK(state.fields.h == 6 && state.fields.o == 48 && state.fields.p == 4 &&
              state.fields.m && !state.fields.w);
        CHECK(state.fields.i == r->final_u && state.fields.j == r->final_v);
        for (size_t vertex = 0; vertex < 6; ++vertex)
            for (size_t component = 0; component < 5; ++component) {
                uint32_t actual, expected;
                memcpy(&actual, state.fields.g + vertex * 8 + component, sizeof(actual));
                memcpy(&expected, &r->vertices[order[vertex]][component], sizeof(expected));
                CHECK(actual == expected);
            }
    }
    _mm_setcsr(0x5f80u);
    CHECK(!mc_client_application_textured_rectangle(classes, NULL, 1, 2, 3, 4, 5, 6, &error));
    CHECK(error.pc == 23 && error.graphics.status == MC_CLIENT_GRAPHICS_UNSUPPORTED_FP &&
          error.graphics.native_policy && _mm_getcsr() == 0x5f80u);
    CHECK(mc_client_nw_inspect(classes, &state) && state.fields.w && state.fields.h == 0 &&
          state.fields.o == 0 && state.fields.p == 0);
    _mm_setcsr(0x1f80u);
    CHECK(!mc_client_application_textured_rectangle(NULL, NULL, 1, 2, 3, 4, 5, 6, &error) &&
          error.pc == 8 && error.graphics.status == MC_CLIENT_GRAPHICS_INVALID_ARGUMENT);
    CHECK(mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_OK);
    _mm_setcsr(initial_fp);
    return printf("Application textured rectangle CPU: %u checks passed\n", checks) < 0;
}
