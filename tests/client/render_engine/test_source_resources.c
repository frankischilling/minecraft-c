#include "client/gui/loading_screen.h"
#include "client/render_engine/internal.h"
#include "client/render_engine/nw.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

static unsigned checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "source resources check %u failed at line %d\n", checks, __LINE__);    \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static const mc_client_graphics_site caller = {"native-test", "source", "()V", -1};

#include "test_lighting.h"

int main(void) {
    const unsigned initial_fp = _mm_getcsr();
    _mm_setcsr(0x1f80u);
    CHECK(lighting_controls() == 0);
    mc_client_render_engine_classes *classes = NULL;
    CHECK(mc_client_render_engine_classes_allocate_context(NULL, NULL, NULL, &classes) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(classes->a == NULL && classes->b == NULL &&
          classes->ge_phase == MC_CLIENT_RENDER_ENGINE_RAW);
    CHECK(classes->nw.phase == MC_CLIENT_RENDER_ENGINE_RAW && classes->bound_display == NULL &&
          classes->context_lease == NULL);
    /* Two separately held occurrences of the genuine RAW sj owner. Native
     * allocation has no font/resource/GL constructor effects. A successful
     * release consumes only its own slot; the other occurrence remains live. */
    mc_client_loading_classes *gui = NULL;
    mc_client_bitmap_font *first_font = NULL, *second_font = NULL;
    mc_client_bitmap_font_state *font_state = calloc(1, sizeof(*font_state));
    CHECK(font_state != NULL);
    CHECK(mc_client_loading_classes_create(classes, &gui) == MC_CLIENT_LOADING_OK);
    CHECK(mc_client_bitmap_font_allocate(gui, NULL, &first_font) == MC_CLIENT_BITMAP_FONT_OK);
    CHECK(mc_client_bitmap_font_retain(first_font) == MC_CLIENT_BITMAP_FONT_OK);
    second_font = first_font;
    CHECK(mc_client_bitmap_font_inspect(second_font, font_state) && font_state->holds == 2);
    CHECK(mc_client_bitmap_font_release(&first_font, NULL) == MC_CLIENT_BITMAP_FONT_OK &&
          first_font == NULL);
    CHECK(mc_client_bitmap_font_inspect(second_font, font_state) && font_state->holds == 1 &&
          font_state->phase == MC_CLIENT_BITMAP_FONT_RAW);
    CHECK(mc_client_bitmap_font_release(&first_font, NULL) == MC_CLIENT_BITMAP_FONT_OK &&
          first_font == NULL);
    CHECK(mc_client_bitmap_font_inspect(second_font, font_state) && font_state->holds == 1);
    CHECK(mc_client_bitmap_font_release(&second_font, NULL) == MC_CLIENT_BITMAP_FONT_OK &&
          second_font == NULL);
    CHECK(mc_client_loading_classes_release(gui) == MC_CLIENT_LOADING_OK);
    free(font_state);
    CHECK(classes->a == NULL && classes->b == NULL && classes->bound_display == NULL);
    mc_client_render_arena arena = {.options = {1024u * 1024u, SIZE_MAX}};
    mc_client_graphics_error error = {0};
    mc_client_direct_byte_buffer *bytes = NULL;
    CHECK(mc_client_ge_c(classes, &arena, 32, caller, &bytes, &error));
    CHECK(classes->a != NULL && classes->b != NULL && classes->a != classes->b);
    CHECK(classes->a->count == 0 && classes->b->count == 0);
    mc_client_render_list *list_a = classes->a, *list_b = classes->b;
    mc_client_direct_int_buffer *ints = NULL;
    mc_client_direct_float_buffer *floats = NULL;
    CHECK(mc_client_ge_int_view(&arena, bytes, caller, &ints, &error));
    CHECK(mc_client_ge_float_view(&arena, bytes, caller, &floats, &error));
    CHECK(ints != (void *)floats && ints->bytes == bytes && floats->bytes == bytes);
    CHECK(ints->capacity == 8 && floats->capacity == 8 && bytes->capacity == 32);
    CHECK(mc_client_ge_int_put(ints, 0x3f800001, caller, &error));
    float actual;
    memcpy(&actual, floats->bytes->data, sizeof(actual));
    uint32_t actual_bits;
    memcpy(&actual_bits, &actual, sizeof(actual_bits));
    CHECK(actual_bits == 0x3f800001u && floats->position == 0 && bytes->position == 0 &&
          ints->position == 1);
    CHECK(mc_client_ge_int_flip(ints, caller, &error));
    int32_t read = 0;
    CHECK(mc_client_ge_int_get(ints, 0, &read, caller, &error) && read == 0x3f800001);
    CHECK(ints->position == 0 && ints->limit == 1);
    CHECK(mc_client_ge_int_clear(ints, caller, &error) && ints->limit == 8);
    mc_client_direct_int_buffer *sentinel = ints;
    CHECK(!mc_client_ge_d(classes, &arena, -1, caller, &sentinel, &error));
    CHECK(sentinel == ints && error.status == MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_ARGUMENT &&
          error.site.pc == 1);
    mc_client_direct_int_buffer *wrapped = NULL;
    CHECK(mc_client_ge_d(classes, &arena, 1073741824, caller, &wrapped, &error));
    CHECK(wrapped->capacity == 0 && wrapped->bytes->capacity == 0);

    mc_client_tessellator *nw = NULL;
    CHECK(mc_client_nw_active_use(classes, caller, &nw, &error));
    mc_client_nw_state state;
    CHECK(mc_client_nw_inspect(classes, &state));
    CHECK(state.static_b && !state.static_c && state.a == nw && state.failed_candidate == NULL);
    CHECK(state.fields.B == 2097152 && state.fields.A == 10 && !state.fields.x);
    CHECK(state.fields.d->capacity == 8388608 && state.fields.e->capacity == 2097152 &&
          state.fields.f->capacity == 2097152);
    CHECK(state.fields.e->bytes == state.fields.d && state.fields.f->bytes == state.fields.d);
    CHECK((const void *)state.fields.g != (const void *)state.fields.d->data &&
          classes->bound_display == NULL);
    /* Unchanged original nw.b(DDD) stores arbitrary binary64 values without
     * arithmetic, a begin predicate, buffer work or FP-control admission. */
    static const uint64_t offsets[][3] = {
        {0, UINT64_C(0x8000000000000000), 1},
        {UINT64_C(0x8000000000000001), UINT64_C(0x0010000000000000), UINT64_C(0x8010000000000000)},
        {UINT64_C(0x7ff0000000000000), UINT64_C(0xfff0000000000000), UINT64_C(0x7ff8000000001234)},
        {UINT64_C(0xfff8000000005678), UINT64_C(0x7ff0000000001234), UINT64_C(0xfff0000000005678)},
        {UINT64_C(0x4170000000000000), UINT64_C(0xc050200000000000), UINT64_C(0xc070000000000000)}};
    for (size_t row = 0; row < sizeof(offsets) / sizeof(offsets[0]); ++row) {
        double values[3];
        memcpy(values, offsets[row], sizeof(values));
        _mm_setcsr(0x9fc0u);
        const bool stored = mc_client_nw_translation(nw, values[0], values[1], values[2], &error);
        const unsigned after_controls = _mm_getcsr();
        _mm_setcsr(0x1f80u);
        CHECK(stored && after_controls == 0x9fc0u && mc_client_nw_inspect(classes, &state));
        CHECK(memcmp(&state.fields.s, &offsets[row][0], sizeof(double)) == 0 &&
              memcmp(&state.fields.t, &offsets[row][1], sizeof(double)) == 0 &&
              memcmp(&state.fields.u, &offsets[row][2], sizeof(double)) == 0 &&
              state.last_store_pc == 13 && !state.fields.w && state.fields.h == 0 &&
              state.fields.o == 0 && state.fields.p == 0 && !state.fields.m && !state.fields.l &&
              !state.fields.n && state.fields.d->position == 0 && state.fields.e->position == 0 &&
              state.fields.f->position == 0 && classes->active_source_calls == 0 &&
              classes->bound_display == NULL);
    }
    CHECK(mc_client_nw_begin_quads(nw, &error));
    CHECK(mc_client_nw_inspect(classes, &state) &&
          memcmp(&state.fields.s, &offsets[4][0], sizeof(double)) == 0 &&
          memcmp(&state.fields.t, &offsets[4][1], sizeof(double)) == 0 &&
          memcmp(&state.fields.u, &offsets[4][2], sizeof(double)) == 0);
    CHECK(!mc_client_nw_begin_quads(nw, &error) &&
          error.status == MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_STATE && error.site.pc == 16);
    CHECK(mc_client_nw_rgb(nw, 0x123456, &error));
    CHECK(mc_client_nw_inspect(classes, &state) && (uint32_t)state.fields.k == 0xff563412u);
    CHECK(mc_client_nw_color(nw, -1, 300, 17, 0, &error));
    CHECK(mc_client_nw_inspect(classes, &state) && (uint32_t)state.fields.k == 0x0011ff00u);
    CHECK(mc_client_nw_vertex_uv(nw, -16777215.75, 65.0, 256.75, 0, 7.99f / 128.0f, &error));
    CHECK(mc_client_nw_inspect(classes, &state) && (uint32_t)state.fields.g[0] == 0x3e800000u &&
          (uint32_t)state.fields.g[1] == 0x3f000000u && (uint32_t)state.fields.g[2] == 0x3f400000u);
    CHECK(mc_client_nw_translation(nw, 0, 0, 0, &error));
    CHECK(mc_client_nw_vertex_uv(nw, 7.99f, 7.99f, 0, 7.99f / 128.0f, 7.99f / 128.0f, &error));
    CHECK(mc_client_nw_vertex_uv(nw, 7.99f, 0, 0, 7.99f / 128.0f, 0, &error));
    CHECK(mc_client_nw_vertex_uv(nw, 0, 0, 0, 0, 0, &error));
    CHECK(mc_client_nw_inspect(classes, &state) && state.fields.h == 6 && state.fields.o == 48 &&
          state.fields.p == 4);
    /* Source duplicate vertices copy XYZ/UV/color from0 and2. The unused
     * normal/padding cells are not claimed as copied source data. */
    CHECK(memcmp(state.fields.g, state.fields.g + 24, 6u * sizeof(int32_t)) == 0);
    CHECK(memcmp(state.fields.g + 16, state.fields.g + 32, 6u * sizeof(int32_t)) == 0);
    CHECK(state.fields.d->position == 0 && state.fields.e->position == 0 &&
          state.fields.f->position == 0);
    CHECK(classes->a == list_a && classes->b == list_b && classes->a->count == 0 &&
          classes->b->count == 0);
    /* This fixture reaches the genuine EMPTY draw/reset branch. It makes no
     * claim about the nonempty GL child. No proxy/callback replaces that child. */
    classes->nw.a->fields.h = 0;
    CHECK(mc_client_nw_draw(nw, NULL, &error));
    CHECK(mc_client_nw_inspect(classes, &state) && state.fields.h == 0 && state.fields.o == 0 &&
          state.fields.p == 0 && !state.fields.w);
    CHECK(mc_client_nw_begin_quads(nw, &error));
    _mm_setcsr(0x9fc0u);
    const bool vertex_bad_fp = mc_client_nw_vertex_uv(nw, 1, 2, 3, 0.5, 0.25, &error);
    _mm_setcsr(0x1f80u);
    CHECK(!vertex_bad_fp && error.status == MC_CLIENT_GRAPHICS_UNSUPPORTED_FP &&
          error.site.pc == 264);
    CHECK(mc_client_nw_inspect(classes, &state) && state.fields.p == 1 && state.fields.h == 0 &&
          state.fields.o == 0 && state.fields.m);
    CHECK(classes->active_source_calls == 0 && !classes->nw.a->active &&
          classes->bound_display == NULL);
    mc_client_render_arena_free(&arena);
    CHECK(mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_OK);

    const mc_client_render_engine_options one_list = {sizeof(mc_client_render_list), SIZE_MAX};
    CHECK(mc_client_render_engine_classes_allocate_context(&one_list, NULL, NULL, &classes) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    memset(&error, 0, sizeof(error));
    CHECK(!mc_client_ge_initialize(classes, caller, &error));
    CHECK(error.status == MC_CLIENT_GRAPHICS_LIMIT && error.site.pc == 10 && classes->a != NULL &&
          classes->b == NULL);
    list_a = classes->a;
    CHECK(!mc_client_ge_initialize(classes, caller, &error) &&
          error.status == MC_CLIENT_GRAPHICS_CLASS_FAILED);
    CHECK(classes->a == list_a && error.class_cause.status == MC_CLIENT_GRAPHICS_LIMIT &&
          error.class_cause.site.pc == 10);
    CHECK(mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_OK);

    const mc_client_render_engine_options no_nw_bytes = {0, SIZE_MAX};
    CHECK(mc_client_render_engine_classes_allocate_context(NULL, &no_nw_bytes, NULL, &classes) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    nw = NULL;
    CHECK(!mc_client_nw_active_use(classes, caller, &nw, &error));
    CHECK(nw == NULL && error.status == MC_CLIENT_GRAPHICS_LIMIT && error.site.pc == 1 &&
          error.parent.pc == 14);
    CHECK(mc_client_nw_inspect(classes, &state) && state.phase == MC_CLIENT_RENDER_ENGINE_FAILED &&
          state.a == NULL && state.failed_candidate != NULL);
    CHECK(state.static_b && !state.static_c && state.fields.B == 2097152 &&
          state.fields.d == NULL && state.last_store_pc == 64);
    CHECK(!mc_client_nw_active_use(classes, caller, &nw, &error) &&
          error.status == MC_CLIENT_GRAPHICS_CLASS_FAILED);
    CHECK(error.class_cause.status == MC_CLIENT_GRAPHICS_LIMIT &&
          classes->active_source_calls == 0);
    CHECK(mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_OK);
    _mm_setcsr(initial_fp);
    printf("Source resources CPU: %u checks\n", checks);
    return 0;
}
