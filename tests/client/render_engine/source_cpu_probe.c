/* Actual typed ge/nw CPU observer. Include the unchanged production TU once
 * solely to reach the same private reset body used by the original observer.
 * Link the remaining real libraries; do not also compile nw.c for this target.
 * No Display creation, source draw, copied renderer or captured answer is used. */
#include "../../../src/client/render_engine/nw.c"
#include <SDL3/SDL_init.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>

static const mc_client_graphics_site observer = {"native-observer", "cpu", "()V", -1};

static const char *truth(bool value) {
    return value ? "true" : "false";
}

static const char *native_order(void) {
    const uint32_t word = 1;
    uint8_t first;
    memcpy(&first, &word, 1);
    return first == 1 ? "LITTLE_ENDIAN" : "BIG_ENDIAN";
}

static bool emit(const char *format, ...) {
    va_list args;
    va_start(args, format);
    const int written = vprintf(format, args);
    va_end(args);
    return written >= 0;
}

static bool cursor(size_t position, size_t limit, size_t capacity, bool direct) {
    return emit("%zu,%zu,%zu,%s", position, limit, capacity, truth(direct));
}

static bool byte_cursor(const mc_client_direct_byte_buffer *value) {
    mc_client_direct_buffer_state state;
    return mc_client_direct_byte_buffer_inspect(value, &state) &&
           cursor(state.position, state.limit, state.capacity, state.direct);
}

static bool int_cursor(const mc_client_direct_int_buffer *value) {
    mc_client_direct_buffer_state state;
    return mc_client_direct_int_buffer_inspect(value, &state) &&
           cursor(state.position, state.limit, state.capacity, state.direct);
}

static bool float_cursor(const mc_client_direct_float_buffer *value) {
    mc_client_direct_buffer_state owner;
    return value != NULL && mc_client_direct_byte_buffer_inspect(value->bytes, &owner) &&
           cursor(value->position, value->limit, value->capacity, owner.direct);
}

static bool ge_buffer(mc_client_render_engine_classes *classes, mc_client_render_arena *arena,
                      int kind, int32_t count, const char *label) {
    mc_client_graphics_error error;
    mc_client_direct_byte_buffer *bytes = NULL;
    mc_client_direct_int_buffer *ints = NULL;
    mc_client_direct_float_buffer *floats = NULL;
    const bool ok = kind == 0   ? mc_client_ge_c(classes, arena, count, observer, &bytes, &error)
                    : kind == 1 ? mc_client_ge_d(classes, arena, count, observer, &ints, &error)
                                : mc_client_ge_e(classes, arena, count, observer, &floats, &error);
    if (!ok) {
        if (error.status != MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_ARGUMENT || error.native_policy ||
            error.site.pc != 1 || error.site.class_name == NULL ||
            strcmp(error.site.class_name, "ge") != 0 || strcmp(error.site.method, "c") != 0) {
            fprintf(stderr, "%s unexpected native status%d/site%d\n", label, (int)error.status,
                    error.site.pc);
            return false;
        }
        /* Protocol label for this checked source status, not a JVM object. */
        return emit("%s|java.lang.IllegalArgumentException\n", label);
    }
    if (!emit("%s|OK|", label))
        return false;
    const bool observed = kind == 0   ? byte_cursor(bytes)
                          : kind == 1 ? int_cursor(ints)
                                      : float_cursor(floats);
    return observed && emit(",%s\n", native_order());
}

static bool nw_state(const mc_client_render_engine_classes *classes) {
    mc_client_nw_state state;
    if (!mc_client_nw_inspect(classes, &state) || state.phase != MC_CLIENT_RENDER_ENGINE_READY ||
        state.a == NULL || state.fields.o < 0 || state.fields.B < 0 ||
        state.fields.o > state.fields.B || (state.fields.o > 0 && state.fields.g == NULL))
        return false;
    const mc_client_nw_fields *f = &state.fields;
    if (!emit("h=%" PRId32 ";o=%" PRId32 ";p=%" PRId32 ";r=%" PRId32 ";k=%" PRId32
              ";l=%s;m=%s;n=%s;q=%s;w=%s;x=%s;z=%" PRId32 ";A=%" PRId32 ";B=%" PRId32 ";d=",
              f->h, f->o, f->p, f->r, f->k, truth(f->l), truth(f->m), truth(f->n), truth(f->q),
              truth(f->w), truth(f->x), f->z, f->A, f->B))
        return false;
    if (!byte_cursor(f->d) || !emit(";e=") || !int_cursor(f->e) || !emit(";f=") ||
        !float_cursor(f->f) || !emit(";gLength=%" PRId32 ";g=", f->B))
        return false;
    for (int32_t i = 0; i < f->o; ++i)
        if (!emit("%s%08" PRIx32, i == 0 ? "" : ",", (uint32_t)f->g[i]))
            return false;
    return true;
}

static bool observed(bool condition, int line) {
    if (!condition)
        fprintf(stderr, "CPU source observer failure at%d\n", line);
    return condition;
}

#define NEED(condition)                                                                            \
    do {                                                                                           \
        if (!observed((condition), __LINE__)) {                                                    \
            result = 1;                                                                            \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

int main(int argc, char **argv) {
    (void)argv;
    if (argc != 1)
        return 2;
    _Static_assert(CHAR_BIT == 8 && sizeof(float) == 4 && sizeof(int32_t) == 4,
                   "CPU source protocol requires actual binary32/32-bit storage");
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        fputs("CPU source observer requires admitted strict FP controls\n", stderr);
        return 2;
    }
    int result = 0;
    mc_client_render_engine_classes *classes = NULL;
    mc_client_render_arena arena = {0};
    arena.options = mc_client_render_engine_default_options();
    mc_client_graphics_error error;
    mc_client_tessellator *nw = NULL;
    NEED(SDL_WasInit(0) == 0);
    NEED(mc_client_render_engine_classes_allocate_context(NULL, NULL, NULL, &classes) ==
         MC_CLIENT_RENDER_ENGINE_OK);
    NEED(mc_client_ge_initialize(classes, observer, &error));
    const mc_client_render_list *list_a = classes->a, *list_b = classes->b;
    size_t a = 0, b = 0;
    NEED(mc_client_render_list_count(list_a, &a) && mc_client_render_list_count(list_b, &b));
    NEED(emit("geLists|%s|%zu,%zu\n", truth(list_a != list_b), a, b));
    NEED(ge_buffer(classes, &arena, 0, 0, "geC0"));
    NEED(ge_buffer(classes, &arena, 0, 17, "geC17"));
    NEED(ge_buffer(classes, &arena, 0, -1, "geCNegative"));
    NEED(ge_buffer(classes, &arena, 1, 0, "geD0"));
    NEED(ge_buffer(classes, &arena, 1, 1024, "geD1024"));
    NEED(ge_buffer(classes, &arena, 1, INT32_C(1073741824), "geDWrappedZero"));
    NEED(ge_buffer(classes, &arena, 1, -1, "geDNegative"));
    NEED(ge_buffer(classes, &arena, 2, 3, "geE3"));
    NEED(ge_buffer(classes, &arena, 2, -1, "geENegative"));

    mc_client_direct_byte_buffer *shared = NULL;
    mc_client_direct_int_buffer *ints = NULL;
    mc_client_direct_float_buffer *floats = NULL;
    NEED(mc_client_ge_c(classes, &arena, 32, observer, &shared, &error));
    NEED(mc_client_ge_int_view(&arena, shared, observer, &ints, &error));
    NEED(mc_client_ge_float_view(&arena, shared, observer, &floats, &error));
    NEED(ints->bytes == shared && floats->bytes == shared);
    /* Fixture preparation through existing typed operations. The original
     * fixture used absolute puts; only the final alias/cursor state is compared. */
    const float input_float = -2.25f;
    int32_t float_bits;
    memcpy(&float_bits, &input_float, sizeof(float_bits));
    NEED(mc_client_ge_int_put(ints, INT32_C(0x3f800001), observer, &error));
    NEED(mc_client_ge_int_put(ints, float_bits, observer, &error));
    NEED(mc_client_ge_int_clear(ints, observer, &error));
    int32_t second;
    uint32_t first;
    memcpy(&first, floats->bytes->data, sizeof(first));
    NEED(mc_client_ge_int_get(ints, 1, &second, observer, &error));
    NEED(emit("views|%08" PRIx32 ",%08" PRIx32 "|", first, (uint32_t)second));
    NEED(byte_cursor(shared) && emit("|") && int_cursor(ints) && emit("|") &&
         float_cursor(floats) && emit("\n"));

    NEED(mc_client_nw_active_use(classes, observer, &nw, &error));
    mc_client_nw_state initial;
    NEED(mc_client_nw_inspect(classes, &initial));
    NEED(emit("nwInitial|b=%s;c=%s;", truth(initial.static_b), truth(initial.static_c)) &&
         nw_state(classes) && emit("\n"));
    NEED(mc_client_nw_begin_quads(nw, &error));
    NEED(emit("nwBegin|") && nw_state(classes) && emit("\n"));
    NEED(!mc_client_nw_begin_quads(nw, &error) &&
         error.status == MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_STATE && !error.native_policy &&
         error.site.pc == 16 && strcmp(error.operation, "Already tesselating!") == 0);
    NEED(emit("nwBeginAgain|java.lang.IllegalStateException|%s|", error.operation) &&
         nw_state(classes) && emit("\n"));
    NEED(mc_client_nw_color(nw, -1, 300, 17, 0, &error));
    volatile float extent = 7.99f;
    volatile float uv = extent / 128.0f;
    const double vertices[4][5] = {{0, (double)extent, 0, 0, (double)uv},
                                   {(double)extent, (double)extent, 0, (double)uv, (double)uv},
                                   {(double)extent, 0, 0, (double)uv, 0},
                                   {0, 0, 0, 0, 0}};
    for (size_t i = 0; i < 4; ++i) {
        NEED(mc_client_nw_vertex_uv(nw, vertices[i][0], vertices[i][1], vertices[i][2],
                                    vertices[i][3], vertices[i][4], &error));
        NEED(emit("nwVertex%zu|", i) && nw_state(classes) && emit("\n"));
    }
    /* Real private body, plus real native call guards, on the same singleton.
     * No draw or source GL operation is acknowledged by this isolated control. */
    NEED(enter(nw, &error));
    const bool reset_ok = reset(nw, &error);
    const bool reset_left = leave(nw, reset_ok, &error);
    NEED(reset_ok && reset_left);
    NEED(emit("nwResetBody|") && nw_state(classes) && emit("\n"));
    NEED(mc_client_render_list_count(classes->a, &a) &&
         mc_client_render_list_count(classes->b, &b));
    NEED(emit("geListsAfterCpu|%s,%s|%zu,%zu\n", truth(list_a == classes->a),
              truth(list_b == classes->b), a, b));
    NEED(classes->bound_display == NULL && classes->context_lease == NULL &&
         classes->active_source_calls == 0);
    NEED(SDL_WasInit(0) == 0);
cleanup:
    mc_client_render_arena_free(&arena);
    if (classes != NULL &&
        mc_client_render_engine_classes_release(classes) != MC_CLIENT_RENDER_ENGINE_OK)
        result = 1;
    if (fflush(stdout) != 0 || ferror(stdout))
        result = 1;
    return result;
}
