#include "client/application_state.h"
#include "client/display/camera_surface.h"
#include "client/render_engine/internal.h"
#include "client/world/render_batch_internal.h"
#include "client/world/source_chunk_renderer_internal.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <xmmintrin.h>

static unsigned int checks, cpu_rows, device_rows;
static int32_t base;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "Batch check%u line%d: %s\n", checks, __LINE__, #c);                   \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

static uint32_t bits(float f) {
    uint32_t v;
    memcpy(&v, &f, sizeof v);
    return v;
}

static mc_client_render_batch *batch(mc_client_render_global *n) {
    mc_client_render_global_error e = {0};
    mc_client_render_batch *b = mc_render_global_allocate(
        n, sizeof(*b), (mc_client_graphics_site){"native-test", "batch", "()V", 0}, &e);
    if (b != NULL && !mc_render_batch_construct(n, b, 0))
        return NULL;
    return b;
}

static bool row(mc_client_render_global *n, mc_client_render_batch *b, const char *label,
                const char *exception, bool gl) {
    const mc_client_render_batch_state *s = &b->state;
    const mc_client_direct_int_buffer *g = s->g;
    CHECK(printf("%s %s %s %" PRId32 " %" PRId32 " %" PRId32 " %08" PRIx32 " %08" PRIx32
                 " %08" PRIx32 " %d %d",
                 gl ? "DRAW" : "TG", label, exception, s->a, s->b, s->c, bits(s->d), bits(s->e),
                 bits(s->f), s->h, s->i) >= 0);
    if (g == NULL) {
        CHECK(printf(" -1 -1 -1") >= 0);
    } else {
        CHECK(printf(" %zu %zu %zu", g->capacity, g->position, g->limit) >= 0);
        for (size_t i = 0; i < g->capacity && i < 4; ++i) {
            int32_t v;
            memcpy(&v, g->bytes->data + i * sizeof(v), sizeof v);
            CHECK(printf(" %" PRId32, gl && v >= base && v < base + 4 ? v - base : v) >= 0);
        }
    }
    if (gl) {
        mc_client_source_draw_state color;
        float projection[16], modelview[16];
        CHECK(mc_client_display_source_draw_inspect(n->display, &color) && color.color_known);
        CHECK(mc_client_display_camera_matrices_inspect(n->display, projection, modelview));
        for (size_t i = 0; i < 4; ++i)
            CHECK(printf(" %08" PRIx32, bits(color.color[i])) >= 0);
        for (size_t i = 0; i < 16; ++i)
            CHECK(printf(" %08" PRIx32, bits(modelview[i])) >= 0);
        ++device_rows;
    } else {
        ++cpu_rows;
    }
    CHECK(putchar('\n') != EOF);
    return true;
}

static bool invoke(mc_client_render_global *n, mc_client_render_batch *b, int op, int32_t value,
                   const char *label, bool gl) {
    mc_client_render_global_error e = {0};
    const bool ok = op == 0   ? mc_render_batch_reset(n, b, &e)
                    : op == 1 ? mc_render_batch_append(n, b, value, &e)
                              : mc_render_batch_draw(n, b, &e);
    const char *exception = "NONE";
    if (!ok) {
        CHECK(!e.native_policy);
        if (e.status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE)
            exception = "NULL";
        else {
            CHECK(e.graphics.status == MC_CLIENT_GRAPHICS_BUFFER_OVERFLOW && e.source_pc == 5);
            exception = "OVERFLOW";
        }
    }
    return row(n, b, label, exception, gl);
}

static bool cpu(mc_client_render_global *n, mc_client_source_dk *d) {
    mc_client_render_global_error e = {0};
    bool *flags = mc_render_global_allocate(
        n, 2u * sizeof(*flags), (mc_client_graphics_site){"native-test", "passes", "()V", 0}, &e);
    CHECK(flags != NULL);
    d->fields.p = flags;
    d->fields.C = 100;
    const int32_t passes[] = {-1, 0, 1, 2, INT32_MIN, INT32_MAX};
    unsigned int index = 0;
    for (int visible = 0; visible < 2; ++visible)
        for (int mask = 0; mask < 4; ++mask)
            for (size_t j = 0; j < sizeof passes / sizeof *passes; ++j) {
                const int32_t pass = passes[j];
                d->fields.o = visible != 0;
                flags[0] = (mask & 1) != 0;
                flags[1] = (mask & 2) != 0;
                int32_t result = 123456;
                e = (mc_client_render_global_error){0};
                const bool ok = mc_source_dk_render_list(d, pass, &result, &e);
                CHECK(ok ||
                      (visible && (pass < 0 || pass > 1) && !e.native_policy && e.source_pc == 14 &&
                       e.graphics.status == MC_CLIENT_GRAPHICS_ARRAY_INDEX));
                CHECK(printf("DK %u %d %d %" PRId32 " %" PRId32 " %s\n", index++, visible, mask,
                             pass, result, ok ? "NONE" : "INDEX") >= 0);
                ++cpu_rows;
            }
    d->fields.C = INT32_MAX;
    d->fields.o = true;
    flags[0] = flags[1] = false;
    int32_t result;
    CHECK(mc_source_dk_render_list(d, 1, &result, &e) && result == INT32_MIN);
    CHECK(printf("DK %u 1 0 1 %" PRId32 " NONE\n", index++, result) >= 0);
    ++cpu_rows;
    d->fields.p = NULL;
    for (int visible = 0; visible < 2; ++visible) {
        d->fields.o = visible != 0;
        result = 123456;
        const bool ok = mc_source_dk_render_list(d, -1, &result, &e);
        CHECK(ok == !visible && (ok || (e.status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
                                        e.source_pc == 14 && !e.native_policy)));
        CHECK(printf("DK %u %d -1 -1 %" PRId32 " %s\n", index++, visible, result,
                     ok ? "NONE" : "NULL") >= 0);
        ++cpu_rows;
    }
    d->fields.p = flags;
    for (int built = 0; built < 2; ++built)
        for (int mask = 0; mask < 4; ++mask) {
            d->fields.E = built != 0;
            flags[0] = (mask & 1) != 0;
            flags[1] = (mask & 2) != 0;
            bool empty;
            CHECK(mc_source_dk_empty(d, &empty, &e));
            CHECK(printf("EMPTY %d %d %d\n", built, mask, empty) >= 0);
            ++cpu_rows;
        }
    d->fields.p = NULL;
    for (int built = 0; built < 2; ++built) {
        d->fields.E = built != 0;
        bool empty = true;
        const bool ok = mc_source_dk_empty(d, &empty, &e);
        CHECK(ok == !built && (ok || (empty && e.source_pc == 14 && !e.native_policy)));
        CHECK(printf("EMPTY_NULL %d %d %s\n", built, ok ? (empty ? 1 : 0) : -1,
                     ok ? "NONE" : "NULL") >= 0);
        ++cpu_rows;
    }
    mc_client_render_batch *b = batch(n);
    CHECK(b != NULL && row(n, b, "ctor", "NONE", false));
    CHECK(invoke(n, b, 2, 0, "inactive", false));
    CHECK(mc_render_batch_configure(n, b, 4096, -64, -2048, 4095.75, -63.5, -2047.125, &e));
    CHECK(row(n, b, "configure", "NONE", false));
    bool matched;
    unsigned int mask = 0;
    CHECK(mc_render_batch_matches(n, b, 4096, -64, -2048, &matched, &e));
    mask |= matched ? 1u : 0;
    CHECK(mc_render_batch_matches(n, b, 4095, -64, -2048, &matched, &e));
    mask |= matched ? 2u : 0;
    CHECK(mc_render_batch_matches(n, b, 4096, -63, -2048, &matched, &e));
    mask |= matched ? 4u : 0;
    CHECK(mc_render_batch_matches(n, b, 4096, -64, -2047, &matched, &e));
    mask |= matched ? 8u : 0;
    CHECK(printf("MATCH active %u\n", mask) >= 0);
    ++cpu_rows;
    CHECK(mc_render_batch_append(n, b, 123, &e) && mc_render_batch_append(n, b, -1, &e));
    CHECK(row(n, b, "append", "NONE", false) && invoke(n, b, 0, 0, "reset", false));
    CHECK(mc_render_batch_matches(n, b, 4096, -64, -2048, &matched, &e));
    CHECK(printf("MATCH reset %d\n", matched) >= 0);
    ++cpu_rows;
    CHECK(mc_render_batch_configure(n, b, 1, 2, 3, 16777217.0, -0.0, 0x1p-1022, &e));
    CHECK(row(n, b, "narrow", "NONE", false) && invoke(n, b, 2, 0, "empty_flip", false) &&
          invoke(n, b, 1, 7, "empty_overflow", false));
    const uint64_t nan_bits = UINT64_C(0x7ff8000000000000);
    double quiet_nan;
    memcpy(&quiet_nan, &nan_bits, sizeof quiet_nan);
    CHECK(mc_render_batch_configure(n, b, 4, 5, 6, INFINITY, quiet_nan, -INFINITY, &e));
    CHECK(row(n, b, "special", "NONE", false) && invoke(n, b, 0, 0, "special_reset", false));
    b->state.g = NULL;
    CHECK(invoke(n, b, 2, 0, "null_inactive", false));
    CHECK(!mc_render_batch_configure(n, b, 9, 8, 7, 6, 5, 4, &e) && b->state.h &&
          b->state.last_store_pc == 2 && e.source_pc == 9 && !e.native_policy);
    CHECK(row(n, b, "null_configure", "NULL", false) && invoke(n, b, 2, 0, "null_flip", false));
    b->state.i = true;
    CHECK(invoke(n, b, 2, 0, "null_remaining", false) && invoke(n, b, 1, 7, "null_put", false));
    /* Native strict-FP refusal follows h, clear and a/b/c stores. */
    b = batch(n);
    CHECK(b != NULL);
    b->state.d = 9;
    const unsigned int saved = _mm_getcsr();
    _mm_setcsr(saved | 0x8040u);
    const bool configured = mc_render_batch_configure(n, b, 1, 2, 3, 4, 5, 6, &e);
    _mm_setcsr(saved);
    CHECK(!configured && e.status == MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP && e.source_pc == 31 &&
          b->state.h && b->state.a == 1 && b->state.b == 2 && b->state.c == 3 && b->state.d == 9 &&
          b->state.last_store_pc == 25 && b->state.g->position == 0 && b->state.g->limit == 65536);
    CHECK(cpu_rows == 78);
    return true;
}

static bool device(mc_client_render_global *n, mc_client_source_dk *d) {
    mc_client_display_error de;
    CHECK(mc_client_display_create("Source TG controls", 64, 64, true, &n->display, &de));
    mc_client_render_global_error e = {0};
    CHECK(mc_client_ge_generate_lists(n->classes, n->display, 4,
                                      (mc_client_graphics_site){"native-test", "lists", "()V", 0},
                                      &base, &e.graphics));
    const float colors[4][4] = {{.125f, .25f, .5f, .75f},
                                {.75f, .5f, .25f, .125f},
                                {.25f, .75f, .125f, .5f},
                                {1, 0, .5f, 1}};
    for (int32_t i = 0; i < 4; ++i) {
        const float *c = colors[i];
        CHECK(mc_client_display_lists_begin(n->display, base + i, &de));
        CHECK(mc_client_display_source_color(n->display, c[0], c[1], c[2], c[3], &de));
        CHECK(mc_client_display_source_translate(n->display, (float)i + .25f, -(float)i - .5f,
                                                 (float)i * .125f, &de));
        CHECK(mc_client_display_lists_end(n->display, &de));
    }
    CHECK(mc_client_display_source_matrix_mode(n->display, 5888, &de) &&
          mc_client_display_source_load_identity(n->display, &de) &&
          mc_client_display_source_translate(n->display, .25f, .5f, .75f, &de) &&
          mc_client_display_source_color(n->display, .5f, .25f, .75f, 1, &de));
    mc_client_render_batch *b = batch(n);
    mc_client_direct_int_buffer *g = NULL;
    CHECK(b != NULL && mc_client_ge_d(n->classes, &n->arena, 2,
                                      (mc_client_graphics_site){"native-test", "short", "()V", 0},
                                      &g, &e.graphics));
    b->state.g = g;
    CHECK(mc_render_batch_configure(n, b, 16, 32, 48, 15.5, 31.25, 47.125, &e));
    CHECK(row(n, b, "configure2", "NONE", true));
    CHECK(mc_render_batch_append(n, b, base, &e) && row(n, b, "put0", "NONE", true));
    CHECK(mc_render_batch_append(n, b, base + 1, &e) && row(n, b, "auto_full", "NONE", true));
    CHECK(invoke(n, b, 2, 0, "replay", true));
    CHECK(mc_render_batch_append(n, b, base + 2, &e) && row(n, b, "overwrite0", "NONE", true));
    CHECK(invoke(n, b, 2, 0, "suffix_only", true));
    CHECK(mc_render_batch_append(n, b, base + 3, &e) && row(n, b, "full_no_flip", "NONE", true));
    CHECK(invoke(n, b, 1, base, "overflow", true) && invoke(n, b, 0, 0, "reset_full", true) &&
          invoke(n, b, 2, 0, "inactive_full", true) &&
          invoke(n, b, 1, base, "inactive_overflow", true));
    CHECK(mc_render_batch_configure(n, b, -16, 0, 32, -15.5, 0, 31.25, &e));
    CHECK(row(n, b, "configure_again", "NONE", true) && invoke(n, b, 2, 0, "empty_again", true) &&
          invoke(n, b, 1, base, "flipped_overflow", true));
    CHECK(mc_render_batch_configure(n, b, 4096, 64, -2048, 4095.75, 63.5, -2047.125, &e) &&
          mc_render_batch_append(n, b, base, &e));
    CHECK(row(n, b, "configure_keeps_i", "NONE", true) &&
          invoke(n, b, 2, 0, "kept_i_suffix", true));
    d->fields.C = base;
    CHECK(mc_source_dk_call_bounds(d, &e) && d->source_pc == 9 &&
          row(n, b, "dk_bounds", "NONE", true));
    CHECK(device_rows == 17 && !b->state.matrix_pushed);
    /* Undefined names are a concrete native frontier after valid list effects.
     * Preserve the successful Push, TG translation and first list, without Pop. */
    CHECK(mc_render_batch_reset(n, b, &e) &&
          mc_render_batch_configure(n, b, 16, 32, 48, 15.5, 31.25, 47.125, &e) &&
          mc_render_batch_append(n, b, base, &e));
    e = (mc_client_render_global_error){0};
    CHECK(!mc_render_batch_append(n, b, base + 99, &e) && b->state.matrix_pushed && b->state.i &&
          b->state.source_pc == 78 && b->state.last_store_pc == 25 && g->position == 0 &&
          g->limit == 2 && e.source_pc == 78 && e.parent.pc == 20 &&
          strcmp(e.source_class, "tg") == 0 && strcmp(e.source_descriptor, "()V") == 0 &&
          e.status == MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY && e.native_policy);
    float projection[16], matrix[16];
    mc_client_source_draw_state color;
    CHECK(mc_client_display_camera_matrices_inspect(n->display, projection, matrix) &&
          matrix[12] == 3.25f && matrix[13] == -1.75f && matrix[14] == 1.875f &&
          mc_client_display_source_draw_inspect(n->display, &color) && color.color_known &&
          color.color[0] == .125f && color.color[3] == .75f);
    CHECK(mc_client_display_check_gl(n->display, "whole TG controls", &de));
    return true;
}

static bool run(bool gl) {
    mc_client_render_engine_classes *graphics = NULL;
    mc_client_render_global *n = NULL;
    mc_client_render_global_error e = {0};
    client_application app = {0};
    CHECK(mc_client_render_engine_classes_create(NULL, &graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_global_allocate(graphics, NULL, &n, &e) == MC_CLIENT_RENDER_GLOBAL_OK);
    n->fields.t = &app; /* Prepared RAW App; no World or constructors are claimed. */
    mc_client_source_dk *d = mc_source_dk_allocate(n, &e);
    CHECK(d != NULL);
    n->active = true; /* Borrow the actual entered N graph for these leaf calls. */
    CHECK(cpu(n, d));
    if (gl)
        CHECK(device(n, d));
    mc_client_display *display = n->display;
    n->active = false;
    CHECK(mc_client_render_global_retire(&n, &e) == MC_CLIENT_RENDER_GLOBAL_OK && n == NULL);
    CHECK(mc_client_render_engine_classes_release(graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    if (display != NULL) {
        mc_client_display_error de;
        CHECK(mc_client_display_destroy(display, &de));
    }
    return true;
}

int main(int argc, char **argv) {
    const bool gl = argc == 2 && strcmp(argv[1], "--device") == 0;
    if (argc != 1 && !gl)
        return 2;
    if (!run(gl))
        return 1;
    printf("Source batches: %u checks; CPU=%u; device=%u\n", checks, cpu_rows, device_rows);
    return 0;
}
