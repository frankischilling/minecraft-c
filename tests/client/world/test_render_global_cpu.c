#include "client/render_engine/internal.h"
#include "client/render_engine/textures.h"
#include "client/world/render_global_internal.h"

#include <emmintrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "RenderGlobal CPU check%u line%d: %s\n", checks, __LINE__,             \
                    #condition);                                                                   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static int raw(void) {
    mc_client_render_engine_classes *classes = NULL;
    CHECK(mc_client_render_engine_classes_create(NULL, &classes) == MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_render_global *owner = NULL;
    mc_client_render_global_error error = {0};
    CHECK(mc_client_render_global_allocate(classes, NULL, &owner, &error) ==
          MC_CLIENT_RENDER_GLOBAL_OK);
    mc_client_render_global_state state;
    CHECK(mc_client_render_global_inspect(owner, &state));
    CHECK(state.phase == MC_CLIENT_RENDER_GLOBAL_RAW && state.holds == 1 &&
          state.arena_bytes == 0 && state.arena_allocations == 0 && state.classes == classes &&
          state.fields.a == NULL && state.fields.m == NULL && state.fields.S == NULL &&
          state.fields.b == NULL && state.fields.c == NULL && state.fields.T == NULL &&
          state.fields.t == NULL && state.fields.l == NULL && state.fields.u == NULL &&
          !state.fields.w && state.fields.H == 0 && state.fields.I == 0 && state.fields.f == 0.0 &&
          state.fields.g == 0.0 && state.fields.h == 0.0 && !state.star_random_created &&
          classes->ge_phase == MC_CLIENT_RENDER_ENGINE_RAW &&
          classes->nw.phase == MC_CLIENT_RENDER_ENGINE_RAW && classes->scene_lease_count == 1);
    CHECK(mc_client_render_engine_retire_resources(classes, NULL) == MC_CLIENT_RENDER_TEXTURE_BUSY);
    CHECK(!classes->resource_retirement_started);
    mc_client_render_global *other = owner;
    CHECK(mc_client_render_global_retain(other, &error) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_client_render_global_release(&owner, &error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          owner == NULL);
    CHECK(classes->scene_lease_count == 1 && other->holds == 1);
    CHECK(mc_client_render_global_release(&other, &error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          other == NULL);
    CHECK(classes->scene_lease_count == 0 && !classes->resource_retirement_started);
    CHECK(mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_OK);
    return 0;
}

static int prefixes(void) {
    for (size_t quota = 0; quota < 25; ++quota) {
        mc_client_render_engine_classes *classes = NULL;
        CHECK(mc_client_render_engine_classes_create(NULL, &classes) == MC_CLIENT_RENDER_ENGINE_OK);
        mc_client_render_global *owner = NULL;
        mc_client_render_global_error error = {0};
        const mc_client_render_engine_options options = {8u * 1024u * 1024u, quota};
        CHECK(mc_client_render_global_allocate(classes, &options, &owner, &error) ==
              MC_CLIENT_RENDER_GLOBAL_OK);
        const mc_client_render_global_status result =
            mc_client_render_global_construct(owner, NULL, NULL, NULL, &error);
        CHECK(result == MC_CLIENT_RENDER_GLOBAL_LIMIT ||
              result == MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED);
        mc_client_render_global_state state;
        CHECK(mc_client_render_global_inspect(owner, &state));
        CHECK(state.phase == MC_CLIENT_RENDER_GLOBAL_FAILED && !state.source_returned &&
              !state.star_random_created && !state.queries_issued && state.fields.e == 0 &&
              state.fields.t == NULL && state.fields.l == NULL && state.fields.f == 0.0 &&
              (state.fields.T == NULL) == (state.last_store_pc < 119) &&
              state.arena_allocations <= quota && classes->scene_lease_count == 1);
        if (state.fields.a != NULL && state.fields.m != NULL)
            CHECK(state.fields.a != state.fields.m && state.fields.a->count == 0 &&
                  state.fields.m->count == 0);
        if (state.fields.S != NULL)
            CHECK(state.fields.S != state.fields.a && state.fields.S != state.fields.m &&
                  state.fields.S->count == 0);
        mc_client_render_batch *const *batches =
            state.fields.T == NULL ? state.pending_T : state.fields.T;
        for (unsigned int i = 0; i < state.published_batches; ++i) {
            mc_client_render_batch_state batch;
            CHECK(batches != NULL && mc_client_render_batch_inspect(batches[i], &batch));
            CHECK(batch.phase == MC_CLIENT_RENDER_GLOBAL_READY && batch.last_store_pc == 20 &&
                  !batch.h && !batch.i && batch.a == 0 && batch.d == 0.0f && batch.g != NULL &&
                  batch.g->position == 0 && batch.g->limit == 65536 && batch.g->capacity == 65536);
            for (unsigned int j = 0; j < i; ++j)
                CHECK(batches[i] != batches[j] && batch.g != batches[j]->state.g &&
                      batch.g->bytes->data != batches[j]->state.g->bytes->data);
        }
        if (result == MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED) {
            CHECK(state.last_constructor_pc == 129 && state.last_store_pc == 124 &&
                  state.published_batches == 4 && state.constructed_batches == 4 &&
                  state.pending_T == NULL && state.pending_batch == NULL &&
                  state.fields.b != NULL && state.fields.c != NULL && state.fields.T != NULL &&
                  error.graphics.caller.pc == 129 && error.graphics.native_policy &&
                  classes->a->count == 0 && classes->b->count == 0);
            for (size_t i = 0; i < 50000; ++i)
                CHECK(state.fields.b[i] == 0);
        }
        CHECK(mc_client_render_global_construct(owner, NULL, NULL, NULL, &error) ==
              MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE);
        CHECK(mc_client_render_global_release(&owner, &error) == MC_CLIENT_RENDER_GLOBAL_OK &&
              owner == NULL);
        CHECK(classes->scene_lease_count == 0);
        CHECK(mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_OK);
    }
    return 0;
}

static int buffer_operations(void) {
    mc_client_render_engine_classes *classes = NULL;
    CHECK(mc_client_render_engine_classes_create(NULL, &classes) == MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_render_arena arena = {.options = {1048576, SIZE_MAX}};
    mc_client_graphics_error error = {0};
    const mc_client_graphics_site site = {"n", "<init>", "(Lnet/minecraft/client/Minecraft;Lji;)V",
                                          239};
    mc_client_direct_int_buffer *buffer = NULL;
    CHECK(mc_client_ge_d(classes, &arena, 8, site, &buffer, &error));
    CHECK(mc_client_ge_int_position(buffer, 3, site, &error));
    CHECK(mc_client_ge_int_limit(buffer, 6, site, &error));
    void *span = NULL;
    size_t count = 0;
    CHECK(mc_client_ge_int_writable_remaining(buffer, &span, &count, site, &error));
    CHECK(span == buffer->bytes->data + 12 && count == 3 && buffer->position == 3 &&
          buffer->limit == 6);
    CHECK(!mc_client_ge_int_position(buffer, 7, site, &error) &&
          error.status == MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_ARGUMENT && buffer->position == 3);
    CHECK(mc_client_ge_int_limit(buffer, 2, site, &error) && buffer->position == 2 &&
          buffer->limit == 2);
    CHECK(!mc_client_ge_int_limit(buffer, -1, site, &error) && buffer->position == 2 &&
          buffer->limit == 2);
    CHECK(mc_client_ge_int_clear(buffer, site, &error));
    CHECK(buffer->position == 0 && buffer->limit == 8);
    mc_client_render_arena_free(&arena);
    CHECK(mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_OK);
    return 0;
}

static int delayed_fp_admission(void) {
    mc_client_render_engine_classes *classes = NULL;
    CHECK(mc_client_render_engine_classes_create(NULL, &classes) == MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_render_global *owner = NULL;
    mc_client_render_global_error error = {0};
    const mc_client_render_engine_options options = {8u * 1024u * 1024u, 0};
    CHECK(mc_client_render_global_allocate(classes, &options, &owner, &error) ==
          MC_CLIENT_RENDER_GLOBAL_OK);
    const unsigned int saved = _mm_getcsr();
    const unsigned int changed = (saved & ~0x6000u) | 0x6000u;
    _mm_setcsr(changed);
    const mc_client_render_global_status result =
        mc_client_render_global_construct(owner, NULL, NULL, NULL, &error);
    const unsigned int observed = _mm_getcsr();
    _mm_setcsr(saved);
    CHECK(result == MC_CLIENT_RENDER_GLOBAL_LIMIT && error.source_pc == 5 &&
          owner->last_constructor_pc == 5 && owner->last_store_pc == 0 && observed == changed);
    CHECK(mc_client_render_global_release(&owner, &error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          owner == NULL);
    CHECK(mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_OK);
    return 0;
}

static bool same_text(const char *a, const char *b) {
    return a == b || (a != NULL && b != NULL && strcmp(a, b) == 0);
}

static bool same_graphics_site(mc_client_graphics_site a, mc_client_graphics_site b) {
    return a.pc == b.pc && same_text(a.class_name, b.class_name) && same_text(a.method, b.method) &&
           same_text(a.descriptor, b.descriptor);
}

static bool same_graphics_cause(const mc_client_graphics_cause *a,
                                const mc_client_graphics_cause *b) {
    return a->status == b->status && a->native_policy == b->native_policy &&
           same_text(a->operation, b->operation) && same_graphics_site(a->caller, b->caller) &&
           same_graphics_site(a->site, b->site);
}

static bool same_display_error(const mc_client_display_error *a, const mc_client_display_error *b) {
    return a->status == b->status && a->gl_error == b->gl_error &&
           same_text(a->operation, b->operation) &&
           memcmp(a->detail, b->detail, sizeof(a->detail)) == 0;
}

static bool same_print_site(mc_java_print_site a, mc_java_print_site b) {
    return a.pc == b.pc && same_text(a.class_name, b.class_name) && same_text(a.method, b.method) &&
           same_text(a.descriptor, b.descriptor);
}

static bool same_print_error(const mc_java_print_error *a, const mc_java_print_error *b) {
    for (size_t i = 0; i < sizeof(a->frames) / sizeof(a->frames[0]); ++i)
        if (!same_print_site(a->frames[i], b->frames[i]))
            return false;
    return a->status == b->status && same_text(a->operation, b->operation) &&
           same_print_site(a->site, b->site) && a->frame_count == b->frame_count &&
           a->system_error == b->system_error && a->native_policy == b->native_policy &&
           a->source_returned == b->source_returned && a->interrupted_io == b->interrupted_io &&
           a->encoder.status == b->encoder.status &&
           a->encoder.system_error == b->encoder.system_error &&
           same_text(a->encoder.operation, b->encoder.operation) &&
           a->encoder.native_cleanup_failed == b->encoder.native_cleanup_failed &&
           a->encoder.native_cleanup_system_error == b->encoder.native_cleanup_system_error;
}

/* Compare diagnostics by member because native C structure padding is unspecified. */
static bool same_graphics_error(const mc_client_graphics_error *a,
                                const mc_client_graphics_error *b) {
    return a->status == b->status && same_text(a->operation, b->operation) &&
           same_graphics_site(a->caller, b->caller) && same_graphics_site(a->site, b->site) &&
           same_graphics_site(a->entry, b->entry) && same_graphics_site(a->parent, b->parent) &&
           same_graphics_cause(&a->class_cause, &b->class_cause) &&
           same_graphics_cause(&a->dependency_class_cause, &b->dependency_class_cause) &&
           same_display_error(&a->display, &b->display) &&
           same_display_error(&a->cleanup_display, &b->cleanup_display) &&
           same_print_error(&a->print, &b->print) &&
           same_print_error(&a->cleanup_print, &b->cleanup_print) &&
           a->actual_index == b->actual_index && a->retained_count == b->retained_count &&
           a->native_policy == b->native_policy && a->generated == b->generated &&
           a->first_append == b->first_append && a->second_append == b->second_append &&
           a->cleanup_failed == b->cleanup_failed;
}

static int scene_vertices(void) {
    /* A cold scene owns the real counted class occurrence without constructing
     * a World or invoking any scene rendering body. */
    mc_client_render_engine_classes *classes = NULL, *other = NULL;
    mc_client_render_global *scene = NULL;
    mc_client_render_global_error scene_error = {0};
    mc_client_tessellator *nw = NULL;
    mc_client_graphics_error error;
    const mc_client_graphics_site caller = {"fixture", "scene_vertices", "()V", -1};
    CHECK(mc_client_render_engine_classes_create(NULL, &classes) == MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_render_engine_classes_create(NULL, &other) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_global_allocate(classes, NULL, &scene, &scene_error) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          classes->scene_lease_count == 1 && atomic_load(&classes->holds) == 2);
    CHECK(mc_client_nw_active_use(classes, caller, &nw, &error) &&
          mc_client_nw_begin(nw, 4, &error) && mc_client_nw_color(nw, 17, 34, 51, 68, &error) &&
          mc_client_nw_normal(nw, 1, 0, 0, &error) &&
          mc_client_nw_translation(nw, .25, -.5, 1, &error));
    const mc_client_nw_fields initial = nw->fields;
    const int32_t initial_pc = nw->last_store_pc;
    mc_client_direct_byte_buffer *bytes = (mc_client_direct_byte_buffer *)(void *)initial.d;
    mc_client_direct_int_buffer *ints = (mc_client_direct_int_buffer *)(void *)initial.e;
    mc_client_direct_float_buffer *floats = (mc_client_direct_float_buffer *)(void *)initial.f;
    const mc_client_direct_byte_buffer initial_bytes = *bytes;
    const mc_client_direct_int_buffer initial_ints = *ints;
    const mc_client_direct_float_buffer initial_floats = *floats;
    const size_t size = (size_t)initial.B * sizeof(*initial.g);
    int32_t *const canonical = (int32_t *)(void *)initial.g;
    void *expected_words = malloc(size), *expected_upload = malloc(size);
    CHECK(expected_words != NULL && expected_upload != NULL && initial.B == MC_NW_BUFFER_WORDS &&
          initial_bytes.capacity == size && canonical == (void *)(nw->arena.head + 1));
    const unsigned int saved_controls = _mm_getcsr();

    enum {
        TRIANGLE,
        QUAD_DUPLICATES,
        TRIANGLE_WRAP,
        QUAD_WRAP,
        NEGATIVE_COUNTER,
        WRONG_SCENE,
        NULL_SCENE,
        NO_SCENE_LEASE,
        FOREIGN_ARRAY,
        SHORT_ARRAY,
        FIRST_DUPLICATE_PREFIX,
        SECOND_DUPLICATE_PREFIX,
        AUTO_FLUSH,
        STRICT_FP,
        SIGNALING_UV,
        RECEIVER_BUSY,
        CLASS_BUSY,
        RETIRING,
        HOLD_LIMIT,
        ZERO_HOLDS,
        NEGATIVE_OFFSET,
        BEFORE_FLUSH,
        CASE_COUNT
    };

    for (unsigned int sample = 0; sample < CASE_COUNT; ++sample) {
        if (sample == NO_SCENE_LEASE)
            CHECK(mc_client_render_global_release(&scene, &scene_error) ==
                      MC_CLIENT_RENDER_GLOBAL_OK &&
                  scene == NULL && classes->scene_lease_count == 0);
        const size_t live_holds = atomic_load(&classes->holds);
        mc_client_nw_fields expected_fields = {0};
        mc_client_direct_byte_buffer expected_bytes = {0};
        mc_client_direct_int_buffer expected_ints = {0};
        mc_client_direct_float_buffer expected_floats = {0};
        mc_client_graphics_error expected_error = {0};
        int32_t words[64], expected_foreign[64], expected_pc = 0;
        unsigned int expected_controls = 0;
        bool expected_result = false;
        for (unsigned int pass = 0; pass < 2; ++pass) {
            memcpy(&nw->fields, &initial, sizeof(initial));
            nw->last_store_pc = initial_pc;
            nw->active = sample == RECEIVER_BUSY;
            classes->active_source_calls = sample == CLASS_BUSY ? 1u : 0u;
            classes->resource_retirement_started = sample == RETIRING;
            const size_t holds = sample == HOLD_LIMIT   ? SIZE_MAX
                                 : sample == ZERO_HOLDS ? 0
                                                        : live_holds;
            atomic_store(&classes->holds, holds);
            memcpy(bytes, &initial_bytes, sizeof(initial_bytes));
            memcpy(ints, &initial_ints, sizeof(initial_ints));
            memcpy(floats, &initial_floats, sizeof(initial_floats));
            memset(canonical, 0x5a, size);
            memset(bytes->data, 0xa5, size);
            for (size_t i = 0; i < sizeof(words) / sizeof(*words); ++i)
                words[i] = 100 + (int32_t)i;
            const mc_client_render_engine_classes *scene_classes = classes;
            double u = .5, v = .25;
            switch (sample) {
                case QUAD_DUPLICATES:
                case QUAD_WRAP:
                case NEGATIVE_COUNTER:
                case FIRST_DUPLICATE_PREFIX:
                case SECOND_DUPLICATE_PREFIX:
                    nw->fields.r = 7;
                    nw->fields.h = nw->fields.p = 3;
                    nw->fields.o = 24;
                    if (sample == QUAD_WRAP) {
                        nw->fields.h = INT32_MAX - 1;
                        nw->fields.p = INT32_MAX;
                    } else if (sample == NEGATIVE_COUNTER)
                        nw->fields.p = -1;
                    else if (sample == FIRST_DUPLICATE_PREFIX ||
                             sample == SECOND_DUPLICATE_PREFIX) {
                        nw->fields.g = words;
                        nw->fields.B = sample == FIRST_DUPLICATE_PREFIX ? 37 : 40;
                    }
                    break;
                case TRIANGLE_WRAP:
                    nw->fields.h = nw->fields.p = INT32_MAX;
                    break;
                case WRONG_SCENE:
                    scene_classes = other;
                    break;
                case NULL_SCENE:
                    scene_classes = NULL;
                    break;
                case FOREIGN_ARRAY:
                case SHORT_ARRAY:
                    nw->fields.g = words;
                    nw->fields.B = sample == FOREIGN_ARRAY ? 7 : 6;
                    break;
                case AUTO_FLUSH:
                case BEFORE_FLUSH:
                    nw->fields.h = nw->fields.p = 3;
                    nw->fields.o = initial.B - (sample == AUTO_FLUSH ? 40 : 48);
                    break;
                case SIGNALING_UV: {
                    const uint64_t signaling_bits = UINT64_C(0x7ff0000000000001);
                    u = -0.0;
                    memcpy(&v, &signaling_bits, sizeof(v));
                    break;
                }
                case NEGATIVE_OFFSET:
                    nw->fields.o = -1;
                    break;
                default:
                    break;
            }
            memset(&error, 0, sizeof(error));
            error.status = MC_CLIENT_GRAPHICS_LIMIT;
            error.actual_index = 123;
            error.retained_count = 456;
            error.cleanup_failed = true;
            error.cleanup_display.status = MC_CLIENT_DISPLAY_BUSY;
            error.print.frame_count = 3;
            _mm_setcsr(sample == STRICT_FP ? 0x9fc0u : 0x1f80u);
            const bool result = pass == 0 ? mc_client_nw_vertex_uv(nw, 1, -2, 3, u, v, &error)
                                          : mc_client_nw_vertex_uv_in_scene(nw, 1, -2, 3, u, v,
                                                                            scene_classes, &error);
            const unsigned int controls = _mm_getcsr();
            _mm_setcsr(saved_controls);
            CHECK(atomic_load(&classes->holds) == holds &&
                  classes->active_source_calls == (sample == CLASS_BUSY ? 1u : 0u) &&
                  nw->active == (sample == RECEIVER_BUSY) && classes->context_lease == NULL &&
                  classes->bound_display == NULL &&
                  classes->resource_retirement_started == (sample == RETIRING) &&
                  classes->scene_lease_count == (sample == NO_SCENE_LEASE ? 0u : 1u));
            if (pass == 0) {
                memcpy(&expected_fields, &nw->fields, sizeof(expected_fields));
                memcpy(&expected_bytes, bytes, sizeof(expected_bytes));
                memcpy(&expected_ints, ints, sizeof(expected_ints));
                memcpy(&expected_floats, floats, sizeof(expected_floats));
                memcpy(&expected_error, &error, sizeof(error));
                memcpy(expected_words, canonical, size);
                memcpy(expected_upload, bytes->data, size);
                memcpy(expected_foreign, words, sizeof(words));
                expected_pc = nw->last_store_pc;
                expected_controls = controls;
                expected_result = result;
            } else {
                if (result != expected_result || error.status != expected_error.status)
                    fprintf(stderr, "Scene vertex case%u public%d internal%d status%d/%d\n", sample,
                            expected_result, result, (int)expected_error.status, (int)error.status);
                /* Compare complete backing stores, including untouched padding
                 * and the copied upload prefix on an automatic draw failure. */
                CHECK(result == expected_result && controls == expected_controls &&
                      nw->last_store_pc == expected_pc &&
                      memcmp(&nw->fields, &expected_fields, sizeof(expected_fields)) == 0 &&
                      memcmp(bytes, &expected_bytes, sizeof(expected_bytes)) == 0 &&
                      memcmp(ints, &expected_ints, sizeof(expected_ints)) == 0 &&
                      memcmp(floats, &expected_floats, sizeof(expected_floats)) == 0 &&
                      same_graphics_error(&error, &expected_error) &&
                      memcmp(canonical, expected_words, size) == 0 &&
                      memcmp(bytes->data, expected_upload, size) == 0 &&
                      memcmp(words, expected_foreign, sizeof(words)) == 0);
            }
        }
        nw->active = false;
        classes->active_source_calls = 0;
        classes->resource_retirement_started = false;
        atomic_store(&classes->holds, live_holds);
        if (sample == AUTO_FLUSH)
            CHECK(!expected_result &&
                  expected_error.status == MC_CLIENT_GRAPHICS_INVALID_ARGUMENT &&
                  expected_error.site.pc == 171 && expected_error.parent.pc == 13 &&
                  !expected_fields.w && expected_fields.h == 4 &&
                  expected_fields.o == initial.B - 32 && expected_pc == 19 &&
                  expected_ints.position == (size_t)initial.B - 32 &&
                  expected_bytes.limit == ((size_t)initial.B - 32) * sizeof(int32_t));
        if (sample == TRIANGLE_WRAP)
            CHECK(expected_result && expected_fields.h == INT32_MIN &&
                  expected_fields.p == INT32_MIN && expected_fields.o == 8);
        if (sample == QUAD_WRAP)
            CHECK(expected_result && expected_fields.h == INT32_MIN + 1 &&
                  expected_fields.p == INT32_MIN && expected_fields.o == 48);
        if (sample == NO_SCENE_LEASE)
            CHECK(mc_client_render_global_allocate(classes, NULL, &scene, &scene_error) ==
                      MC_CLIENT_RENDER_GLOBAL_OK &&
                  classes->scene_lease_count == 1);
    }
    nw->fields = initial;
    free(expected_words);
    free(expected_upload);
    CHECK(mc_client_render_global_release(&scene, &scene_error) == MC_CLIENT_RENDER_GLOBAL_OK &&
          scene == NULL && classes->scene_lease_count == 0);
    CHECK(mc_client_render_engine_classes_release(other) == MC_CLIENT_RENDER_ENGINE_OK &&
          mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_OK);
    return 0;
}

int main(void) {
    CHECK(raw() == 0);
    CHECK(prefixes() == 0);
    CHECK(buffer_operations() == 0);
    CHECK(delayed_fp_admission() == 0);
    CHECK(scene_vertices() == 0);
    printf("Actual RenderGlobal CPU contracts: %u checks\n", checks);
    return 0;
}
