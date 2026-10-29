/* Actual U.b owners, source-state replay and native failure-prefix controls. */
#include "client/display/camera_surface.h"
#include "client/display/fog_surface.h"
#include "client/display/gl_internal.h"
#include "client/display/source_surface.h"
#include "client/render_engine/internal.h"
#include "client/render_engine/lighting.h"
#include "entity/classes/classes_internal.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

static unsigned int checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "Lighting enable check%u at%d: %s\n", checks, __LINE__, #value);       \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

static const mc_client_graphics_site caller = {"fixture", "lighting_enable", "()V", -1};

typedef struct fixture {
    mc_java_random_profile *random;
    mc_entity_classes *entities;
    mc_math_classes *math;
    mc_client_render_engine_classes *graphics;
    mc_client_display *display;
} fixture;

static uint64_t bits64(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void words(const void *storage, size_t count) {
    const uint8_t *bytes = storage;
    for (size_t i = 0; i < count; ++i) {
        uint32_t value;
        memcpy(&value, bytes + i * sizeof(value), sizeof(value));
        printf("%s%08" PRIx32, i == 0 ? "" : ",", value);
    }
}

static void create(fixture *f, bool device, size_t maximum_math_bytes) {
    *f = (fixture){0};
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->random) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->random, NULL, &f->entities) ==
          MC_ENTITY_OK);
    CHECK(mc_math_classes_create(maximum_math_bytes, &f->math, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_client_render_engine_classes_create(NULL, &f->graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    if (device) {
        mc_client_display_error error;
        CHECK(mc_client_display_create("Source standard lighting", 64, 64, true, &f->display,
                                       &error));
    }
}

static void retire(fixture *f) {
    mc_client_display_error error;
    if (f->display != NULL)
        CHECK(mc_client_display_destroy(f->display, &error));
    CHECK(mc_client_render_engine_classes_release(f->graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_math_classes_release(&f->math, NULL) == MC_MATH_CLASS_OK && f->math == NULL);
    CHECK(mc_entity_classes_free(f->entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(f->random);
    *f = (fixture){0};
}

static void cold_failure(void) {
    fixture f;
    create(&f, false, SIZE_MAX);
    mc_client_lighting_state lighting;
    mc_entity_vector_snapshot pool;
    mc_math_class_state math;
    CHECK(mc_client_lighting_inspect(f.graphics, &lighting));
    CHECK(lighting.phase == MC_CLIENT_RENDER_ENGINE_RAW && lighting.a == NULL);
    CHECK(mc_entity_vector_get_snapshot(f.entities, &pool) == MC_ENTITY_OK);
    CHECK(pool.phase == MC_ENTITY_CLASS_COLD && pool.pool_identity == NULL && pool.holds == 0);
    const mc_client_direct_float_buffer *same = NULL;
    for (unsigned int repeat = 0; repeat < 2; ++repeat) {
        mc_client_lighting_error error;
        CHECK(!mc_client_lighting_enable(f.graphics, f.entities, f.math, NULL, caller, &error));
        CHECK(error.status == MC_CLIENT_LIGHTING_GRAPHICS_FAILED && error.native_policy &&
              !error.source_returned && !error.cleanup_failed && error.site.pc == 3 &&
              strcmp(error.site.class_name, "u") == 0 && strcmp(error.site.method, "b") == 0 &&
              error.graphics.display.status == MC_CLIENT_DISPLAY_INVALID_ARGUMENT &&
              error.completed_display_calls == 0 && error.completed_buffer_fills == 0 &&
              error.pooled_vectors == 0);
        CHECK(mc_client_lighting_inspect(f.graphics, &lighting));
        CHECK(lighting.phase == MC_CLIENT_RENDER_ENGINE_READY && lighting.a != NULL);
        if (repeat == 0)
            same = lighting.a;
        CHECK(lighting.a == same && f.graphics->ge_phase == MC_CLIENT_RENDER_ENGINE_READY &&
              f.graphics->active_source_calls == 0);
        mc_client_direct_buffer_state buffer;
        CHECK(mc_client_ge_float_inspect(lighting.a, &buffer));
        CHECK(buffer.position == 0 && buffer.limit == 16 && buffer.capacity == 16 &&
              buffer.storage_bytes == 64 && buffer.direct && buffer.native_order);
        const uint8_t *bytes = buffer.storage;
        for (size_t i = 0; i < buffer.storage_bytes; ++i)
            CHECK(bytes[i] == 0);
        CHECK(mc_entity_vector_get_snapshot(f.entities, &pool) == MC_ENTITY_OK);
        CHECK(pool.phase == MC_ENTITY_CLASS_COLD && pool.pool_identity == NULL &&
              pool.cursor == 0 && pool.size == 0 && pool.holds == 0);
        CHECK(mc_math_classes_state(f.math, &math, NULL) == MC_MATH_CLASS_OK);
        CHECK(math.phase == MC_MATH_CLASS_COLD && math.source_a == NULL && math.holds == 1);
    }
    mc_client_render_engine_class_state classes;
    CHECK(mc_client_render_engine_classes_inspect(f.graphics, &classes));
    CHECK(classes.native_holds == 1 && classes.ge_a != NULL && classes.ge_b != NULL);
    mc_client_direct_buffer_state buffer;
    CHECK(mc_client_ge_float_inspect(same, &buffer));
    printf("LIGHT_FAIL cold_no_context 1 %zu %zu %zu ", buffer.position, buffer.limit,
           buffer.capacity);
    words(buffer.storage, 16);
    putchar('\n');
    retire(&f);
}

static void prepare_matrices(mc_client_display *display, bool transformed) {
    mc_client_display_error error;
    CHECK(mc_client_display_source_matrix_mode(display, GL_MODELVIEW, &error));
    CHECK(mc_client_display_source_load_identity(display, &error));
    if (transformed) {
        CHECK(mc_client_display_source_translate(display, 7, -9, 11, &error));
        CHECK(mc_client_display_source_scale(display, 2, 3, 0.5, &error));
    }
    CHECK(mc_client_display_source_matrix_mode(display, GL_PROJECTION, &error));
    CHECK(mc_client_display_source_load_identity(display, &error));
    if (transformed)
        CHECK(mc_client_display_source_scale(display, 3, 5, 7, &error));
    CHECK(mc_client_display_source_matrix_mode(display, transformed ? GL_PROJECTION : GL_MODELVIEW,
                                               &error));
}

static void prepare_seed(fixture *f) {
    mc_client_graphics_error error;
    CHECK(mc_client_lighting_active_use(f->graphics, caller, &error));
    mc_client_direct_float_buffer *buffer = f->graphics->u_a;
    CHECK(mc_client_ge_float_clear(buffer, caller, &error));
    for (uint32_t i = 0; i < 16; ++i) {
        const uint32_t bits = UINT32_C(0x3f000000) + i * UINT32_C(0x10000);
        float value;
        memcpy(&value, &bits, sizeof(value));
        CHECK(mc_client_ge_float_put(buffer, value, caller, &error));
    }
    CHECK(mc_client_ge_float_flip(buffer, caller, &error));
    CHECK(mc_client_ge_float_limit(buffer, 11, caller, &error));
    float consumed[5];
    CHECK(mc_client_ge_float_get_array(buffer, consumed, 5, caller, &error));
    CHECK(buffer->position == 5 && buffer->limit == 11);
    const double coordinates[2][3] = {{7, 8, 9}, {-3, 5, -7}};
    for (size_t i = 0; i < 2; ++i) {
        mc_entity_vector *vector = NULL;
        CHECK(mc_entity_vector_pool_get(f->entities, coordinates[i][0], coordinates[i][1],
                                        coordinates[i][2], &vector, NULL) == MC_ENTITY_OK);
        CHECK(mc_entity_vector_release(f->entities, vector) == MC_ENTITY_OK);
    }
}

static void emit_call(const fixture *f, const char *name, unsigned int call,
                      const mc_client_direct_float_buffer *before_buffer,
                      const mc_entity_vector_snapshot *before_pool, bool prefix_same,
                      const float model_before[16], const float projection_before[16]) {
    mc_client_source_fog_state state;
    mc_client_source_draw_state draw;
    mc_client_direct_buffer_state buffer;
    mc_entity_vector_snapshot pool;
    float model_after[16], projection_after[16];
    CHECK(mc_client_display_source_fog_inspect(f->display, &state));
    CHECK(mc_client_display_source_draw_inspect(f->display, &draw));
    CHECK(mc_client_ge_float_inspect(f->graphics->u_a, &buffer));
    CHECK(mc_entity_vector_get_snapshot(f->entities, &pool) == MC_ENTITY_OK);
    CHECK(mc_client_display_camera_matrices_inspect(f->display, projection_after, model_after));
    printf("LIGHT_STATE %s %u %u %u %u %u %" PRId32 " %" PRId32 " %d %" PRId32 "\n", name, call,
           state.lighting_enabled ? 1u : 0u, state.lights[0] ? 1u : 0u, state.lights[1] ? 1u : 0u,
           state.color_material ? 1u : 0u, state.material_face, state.material_mode,
           draw.shade_flat ? GL_FLAT : GL_SMOOTH, state.matrix_mode);
    printf("LIGHT_BUFFER %s %u %d %zu %zu %zu ", name, call,
           before_buffer == NULL              ? -1
           : before_buffer == buffer.identity ? 1
                                              : 0,
           buffer.position, buffer.limit, buffer.capacity);
    words(buffer.storage, 16);
    printf("\nLIGHT_POOL %s %u %" PRId32 " %" PRId32 " %zu %zu %u %u\n", name, call,
           before_pool->cursor, pool.cursor, before_pool->size, pool.size,
           before_pool->pool_identity == pool.pool_identity ? 1u : 0u, prefix_same ? 1u : 0u);
    for (size_t i = 0; i < 4; ++i) {
        const mc_entity_vector *vector =
            f->entities->vector_pool->elements[(size_t)before_pool->cursor + i];
        printf("LIGHT_VECTOR %s %u %zu %016" PRIx64 " %016" PRIx64 " %016" PRIx64 "\n", name, call,
               i, bits64(vector->x), bits64(vector->y), bits64(vector->z));
    }
    for (unsigned int light = 0; light < 2; ++light) {
        const mc_client_source_light_state *parameters = &state.light_parameters[light];
        const int32_t names[4] = {GL_POSITION, GL_DIFFUSE, GL_AMBIENT, GL_SPECULAR};
        const float *values[4] = {parameters->position_eye, parameters->diffuse,
                                  parameters->ambient, parameters->specular};
        for (size_t i = 0; i < 4; ++i) {
            printf("LIGHT_PARAMETER %s %u %u %" PRId32 " ", name, call, light, names[i]);
            words(values[i], 4);
            putchar('\n');
        }
    }
    printf("LIGHT_MODEL %s %u ", name, call);
    words(state.light_model_ambient, 4);
    printf("\nLIGHT_MATRICES %s %u ", name, call);
    words(model_before, 16);
    putchar(' ');
    words(model_after, 16);
    putchar(' ');
    words(projection_before, 16);
    putchar(' ');
    words(projection_after, 16);
    putchar('\n');
}

static void device_profile(unsigned int profile) {
    static const char *const names[] = {"cold_no_context", "identity_cold", "seeded_projection",
                                        "seeded_replaced_modelview", "seeded_warm"};
    const bool seeded = profile >= 2;
    const bool transformed = profile == 2 || profile == 3;
    fixture f;
    create(&f, true, SIZE_MAX);
    CHECK(mc_entity_vector_pool_reset(f.entities, NULL) == MC_ENTITY_OK);
    if (seeded)
        prepare_seed(&f);
    prepare_matrices(f.display, transformed);
    mc_client_lighting_state lighting;
    CHECK(mc_client_lighting_inspect(f.graphics, &lighting));
    const mc_client_direct_float_buffer *before_buffer = lighting.a;
    for (unsigned int call = 0; call < (profile == 4 ? 2u : 1u); ++call) {
        mc_entity_vector_snapshot before, after;
        CHECK(mc_entity_vector_get_snapshot(f.entities, &before) == MC_ENTITY_OK);
        CHECK(before.size <= 16 && before.cursor >= 0 && (size_t)before.cursor == before.size &&
              before.holds == 0);
        const mc_entity_vector *prefix[16];
        mc_entity_vector prefix_values[16];
        for (size_t i = 0; i < before.size; ++i) {
            prefix[i] = f.entities->vector_pool->elements[i];
            prefix_values[i] = *prefix[i];
        }
        float model_before[16], projection_before[16];
        CHECK(
            mc_client_display_camera_matrices_inspect(f.display, projection_before, model_before));
        mc_client_lighting_error error;
        CHECK(mc_client_lighting_enable(f.graphics, f.entities, f.math, f.display, caller, &error));
        CHECK(error.status == MC_CLIENT_LIGHTING_OK && error.source_returned &&
              !error.cleanup_failed && error.site.pc == 234 &&
              error.completed_display_calls == 15 && error.completed_buffer_fills == 9 &&
              error.pooled_vectors == 4);
        CHECK(mc_entity_vector_get_snapshot(f.entities, &after) == MC_ENTITY_OK);
        bool prefix_same = before.pool_identity == after.pool_identity &&
                           after.cursor == before.cursor + 4 && after.size == before.size + 4;
        for (size_t i = 0; i < before.size; ++i)
            prefix_same = prefix_same && prefix[i] == f.entities->vector_pool->elements[i] &&
                          memcmp(prefix[i], &prefix_values[i], sizeof(prefix_values[i])) == 0;
        CHECK(prefix_same && after.holds == 0);
        mc_client_direct_buffer_state buffer;
        CHECK(mc_client_ge_float_inspect(f.graphics->u_a, &buffer));
        CHECK(buffer.position == 0 && buffer.limit == 4 && buffer.capacity == 16);
        const float expected[4] = {0.4f, 0.4f, 0.4f, 1};
        CHECK(memcmp(buffer.storage, expected, sizeof(expected)) == 0);
        const uint8_t *bytes = buffer.storage;
        for (uint32_t i = 4; i < 16; ++i) {
            uint32_t word;
            memcpy(&word, bytes + (size_t)i * sizeof(word), sizeof(word));
            CHECK(word == (seeded ? UINT32_C(0x3f000000) + i * UINT32_C(0x10000) : 0));
        }
        mc_client_source_fog_state before_replacement, after_replacement;
        CHECK(mc_client_display_source_fog_inspect(f.display, &before_replacement));
        if (profile == 3) {
            mc_client_display_error display_error;
            CHECK(mc_client_display_source_matrix_mode(f.display, GL_MODELVIEW, &display_error));
            CHECK(mc_client_display_source_load_identity(f.display, &display_error));
            CHECK(mc_client_display_source_matrix_mode(f.display, GL_PROJECTION, &display_error));
        }
        CHECK(mc_client_display_source_fog_inspect(f.display, &after_replacement));
        CHECK(memcmp(before_replacement.light_parameters, after_replacement.light_parameters,
                     sizeof(before_replacement.light_parameters)) == 0);
        emit_call(&f, names[profile], call, before_buffer, &before, prefix_same, model_before,
                  projection_before);
        before_buffer = f.graphics->u_a;
    }
    retire(&f);
}

static void default_state(mc_client_display *display) {
    mc_client_display_error error;
    CHECK(mc_client_display_source_matrix_mode(display, GL_MODELVIEW, &error));
    mc_client_source_fog_state state;
    CHECK(mc_client_display_source_fog_inspect(display, &state));
    CHECK(!state.lighting_enabled && !state.lights[0] && !state.lights[1] && !state.color_material);
    const float black[4] = {0, 0, 0, 1}, white[4] = {1, 1, 1, 1};
    const float position[4] = {0, 0, 1, 0}, ambient[4] = {0.2f, 0.2f, 0.2f, 1};
    for (size_t i = 0; i < 2; ++i) {
        CHECK(memcmp(state.light_parameters[i].ambient, black, sizeof(black)) == 0);
        CHECK(memcmp(state.light_parameters[i].diffuse, i == 0 ? white : black, sizeof(black)) ==
              0);
        CHECK(memcmp(state.light_parameters[i].specular, i == 0 ? white : black, sizeof(black)) ==
              0);
        CHECK(memcmp(state.light_parameters[i].position, position, sizeof(position)) == 0);
        CHECK(memcmp(state.light_parameters[i].position_eye, position, sizeof(position)) == 0);
    }
    CHECK(memcmp(state.light_model_ambient, ambient, sizeof(ambient)) == 0);
}

static void span_and_fp_controls(fixture *f) {
    mc_client_display_error error;
    mc_client_source_fog_state before, after;
    const float values[4] = {-1.0f, 0.5f, 2.0f, -0.0f};
    CHECK(mc_client_display_source_fog_inspect(f->display, &before));
    CHECK(!mc_client_display_source_light_parameter(f->display, 0, GL_AMBIENT, values, 3, &error));
    CHECK(error.status == MC_CLIENT_DISPLAY_BUFFER_TOO_SMALL);
    CHECK(!mc_client_display_source_light_model(f->display, GL_LIGHT_MODEL_AMBIENT, values, 3,
                                                &error));
    CHECK(error.status == MC_CLIENT_DISPLAY_BUFFER_TOO_SMALL);
    CHECK(!mc_client_display_source_light_parameter(f->display, 2, GL_AMBIENT, values, 4, &error));
    CHECK(error.status == MC_CLIENT_DISPLAY_UNSUPPORTED_GL);
    CHECK(!mc_client_display_source_light_parameter(f->display, 0, -1, values, 4, &error));
    CHECK(error.status == MC_CLIENT_DISPLAY_UNSUPPORTED_GL);
    CHECK(!mc_client_display_source_light_model(f->display, GL_AMBIENT, values, 4, &error));
    CHECK(error.status == MC_CLIENT_DISPLAY_UNSUPPORTED_GL);
    CHECK(!mc_client_display_source_light_parameter(f->display, 0, GL_AMBIENT, NULL, 4, &error));
    CHECK(error.status == MC_CLIENT_DISPLAY_INVALID_ARGUMENT);
    CHECK(
        !mc_client_display_source_light_model(f->display, GL_LIGHT_MODEL_AMBIENT, NULL, 4, &error));
    CHECK(error.status == MC_CLIENT_DISPLAY_INVALID_ARGUMENT);
    CHECK(mc_client_display_source_fog_inspect(f->display, &after));
    CHECK(memcmp(before.light_parameters, after.light_parameters,
                 sizeof(before.light_parameters)) == 0 &&
          memcmp(before.light_model_ambient, after.light_model_ambient,
                 sizeof(before.light_model_ambient)) == 0);

    /* Remaining FloatBuffer spans can start after earlier elements. Uploads
     * must neither consume the cursor nor clamp/canonicalize float payloads. */
    mc_client_graphics_error graphics;
    mc_client_direct_float_buffer *buffer = NULL;
    CHECK(mc_client_ge_e(f->graphics, &f->graphics->arena, 8, caller, &buffer, &graphics));
    CHECK(mc_client_ge_float_put(buffer, 9, caller, &graphics));
    for (size_t i = 0; i < 4; ++i)
        CHECK(mc_client_ge_float_put(buffer, values[i], caller, &graphics));
    CHECK(mc_client_ge_float_flip(buffer, caller, &graphics));
    float prefix;
    CHECK(mc_client_ge_float_get_array(buffer, &prefix, 1, caller, &graphics) && prefix == 9);
    void *storage = NULL;
    size_t remaining = 0;
    CHECK(mc_client_ge_float_writable_remaining(buffer, &storage, &remaining, caller, &graphics));
    CHECK(remaining == 4);
    CHECK(mc_client_display_source_light_parameter(f->display, 1, GL_AMBIENT, storage, remaining,
                                                   &error));
    CHECK(mc_client_display_source_light_model(f->display, GL_LIGHT_MODEL_AMBIENT, storage,
                                               remaining, &error));
    CHECK(buffer->position == 1 && buffer->limit == 5);
    CHECK(mc_client_display_source_fog_inspect(f->display, &after));
    CHECK(memcmp(after.light_parameters[1].ambient, values, sizeof(values)) == 0 &&
          memcmp(after.light_model_ambient, values, sizeof(values)) == 0);

    before = after;
    const unsigned int saved = _mm_getcsr();
    _mm_setcsr(saved | 0x8000u);
    const bool positioned =
        mc_client_display_source_light_parameter(f->display, 0, GL_POSITION, values, 4, &error);
    const unsigned int retained = _mm_getcsr();
    _mm_setcsr(saved);
    CHECK(!positioned && error.status == MC_CLIENT_DISPLAY_UNSUPPORTED_FP &&
          (retained & 0xffc0u) == ((saved | 0x8000u) & 0xffc0u));
    CHECK(mc_client_display_source_fog_inspect(f->display, &after));
    CHECK(memcmp(before.light_parameters, after.light_parameters,
                 sizeof(before.light_parameters)) == 0);
}

static void compiled_prefix(fixture *f) {
    mc_client_display_list_range *range = NULL;
    mc_client_display_error error;
    int32_t base = 0;
    CHECK(mc_client_display_source_lighting_enable(f->display, false, &error));
    CHECK(mc_client_display_lists_generate(f->display, 1, 32, &range, &base, &error));
    CHECK(mc_client_display_lists_begin(f->display, base, &error));
    mc_entity_vector_snapshot before, after;
    CHECK(mc_entity_vector_get_snapshot(f->entities, &before) == MC_ENTITY_OK);
    const mc_client_direct_float_buffer *buffer = f->graphics->u_a;
    mc_client_lighting_error lighting;
    CHECK(!mc_client_lighting_enable(f->graphics, f->entities, f->math, f->display, caller,
                                     &lighting));
    CHECK(lighting.status == MC_CLIENT_LIGHTING_GRAPHICS_FAILED && lighting.site.pc == 3 &&
          lighting.completed_display_calls == 0 && lighting.completed_buffer_fills == 0 &&
          lighting.pooled_vectors == 0 && !lighting.cleanup_failed &&
          lighting.graphics.display.status == MC_CLIENT_DISPLAY_UNSUPPORTED_GL);
    CHECK(mc_entity_vector_get_snapshot(f->entities, &after) == MC_ENTITY_OK);
    CHECK(before.cursor == after.cursor && before.size == after.size && after.holds == 0 &&
          f->graphics->u_a == buffer && buffer->position == 0 && buffer->limit == 4);
    mc_client_source_fog_state state;
    CHECK(mc_client_display_source_fog_inspect(f->display, &state) && !state.lighting_enabled);
    CHECK(mc_client_display_lists_abort(f->display, base, &error));
    CHECK(mc_client_display_lists_retire(&range, &error) && range == NULL);
}

static void lit_draw_rejected(fixture *f) {
    mc_client_lighting_error lighting;
    CHECK(mc_client_lighting_enable(f->graphics, f->entities, f->math, f->display, caller,
                                    &lighting));
    mc_client_direct_byte_buffer *bytes = NULL;
    mc_client_graphics_error graphics;
    CHECK(mc_client_ge_c(f->graphics, &f->graphics->arena, 96, caller, &bytes, &graphics));
    memset(bytes->data, 0, 96);
    const float positions[3][3] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    for (size_t i = 0; i < 3; ++i)
        memcpy(bytes->data + 32 * i, positions[i], sizeof(positions[i]));
    mc_client_display_context_lease *lease = NULL;
    mc_client_display_error error;
    CHECK(mc_client_display_context_acquire(f->display, &lease, &error));
    CHECK(mc_client_display_source_pointer(lease, MC_CLIENT_SOURCE_VERTEX_ARRAY, bytes, 0, &error));
    CHECK(mc_client_display_source_array(f->display, MC_CLIENT_SOURCE_VERTEX_ARRAY, true, &error));
    CHECK(!mc_client_display_source_batch(lease, bytes, bytes->data, 96, 3, GL_TRIANGLES, &error));
    CHECK(error.status == MC_CLIENT_DISPLAY_UNSUPPORTED_GL && error.operation != NULL &&
          strcmp(error.operation, "source shading state") == 0 &&
          strcmp(error.detail, "enabled lighting requires its renderer") == 0);
    CHECK(mc_client_display_source_array(f->display, MC_CLIENT_SOURCE_VERTEX_ARRAY, false, &error));
    CHECK(mc_client_display_context_release(&lease, &error) && lease == NULL);
}

static void math_failure(void) {
    fixture f;
    create(&f, true, 4096); /* Actual owner fits; its 65,536-float table cannot. */
    default_state(f.display);
    mc_client_lighting_error error;
    CHECK(!mc_client_lighting_enable(f.graphics, f.entities, f.math, f.display, caller, &error));
    CHECK(error.status == MC_CLIENT_LIGHTING_MATH_FAILED && !error.source_returned &&
          !error.cleanup_failed && error.site.pc == 51 && error.child.pc == 29 &&
          strcmp(error.child.class_name, "bt") == 0 && strcmp(error.child.method, "c") == 0 &&
          error.completed_display_calls == 5 && error.pooled_vectors == 1 &&
          error.completed_buffer_fills == 0 && error.math.status == MC_MATH_CLASS_LIMIT &&
          error.math.source_pc == 2);
    mc_math_class_state math;
    CHECK(mc_math_classes_state(f.math, &math, NULL) == MC_MATH_CLASS_OK);
    CHECK(math.phase == MC_MATH_CLASS_FAILED && math.source_a == NULL && math.holds == 1 &&
          math.last_store_pc == 0);
    mc_entity_vector_snapshot pool;
    CHECK(mc_entity_vector_get_snapshot(f.entities, &pool) == MC_ENTITY_OK);
    CHECK(pool.cursor == 1 && pool.size == 1 && pool.holds == 0);
    const mc_entity_vector *raw = f.entities->vector_pool->elements[0];
    CHECK(bits64(raw->x) == bits64(0.20000000298023224) && bits64(raw->y) == bits64(1.0) &&
          bits64(raw->z) == bits64(-0.699999988079071));
    mc_client_source_fog_state state;
    CHECK(mc_client_display_source_fog_inspect(f.display, &state));
    CHECK(state.lighting_enabled && state.lights[0] && state.lights[1] && state.color_material &&
          state.material_face == GL_FRONT_AND_BACK &&
          state.material_mode == GL_AMBIENT_AND_DIFFUSE);
    const float position[4] = {0, 0, 1, 0}, ambient[4] = {0.2f, 0.2f, 0.2f, 1};
    CHECK(memcmp(state.light_parameters[0].position, position, sizeof(position)) == 0 &&
          memcmp(state.light_model_ambient, ambient, sizeof(ambient)) == 0);
    mc_client_direct_buffer_state buffer;
    CHECK(mc_client_ge_float_inspect(f.graphics->u_a, &buffer));
    CHECK(buffer.position == 0 && buffer.limit == 16 && buffer.capacity == 16);
    const uint8_t *bytes = buffer.storage;
    for (size_t i = 0; i < 64; ++i)
        CHECK(bytes[i] == 0);
    CHECK(!mc_client_lighting_enable(f.graphics, f.entities, f.math, f.display, caller, &error));
    CHECK(error.status == MC_CLIENT_LIGHTING_MATH_FAILED && error.site.pc == 51 &&
          error.math.status == MC_MATH_CLASS_INITIALIZATION_FAILED &&
          error.math.initial_cause == MC_MATH_CLASS_LIMIT && error.completed_display_calls == 5 &&
          error.pooled_vectors == 1 && error.completed_buffer_fills == 0 && !error.cleanup_failed);
    CHECK(mc_entity_vector_get_snapshot(f.entities, &pool) == MC_ENTITY_OK && pool.cursor == 2 &&
          pool.size == 2 && pool.holds == 0);
    retire(&f);
}

static void device_controls(void) {
    fixture f;
    create(&f, true, SIZE_MAX);
    default_state(f.display);
    span_and_fp_controls(&f);
    mc_client_display_error display_error;
    uint32_t gl_error = UINT32_MAX;
    CHECK(mc_client_display_source_get_error(f.display, &gl_error, &display_error) &&
          gl_error == GL_NO_ERROR);
    /* The actual negative viewport reaches the driver and leaves its sticky
     * INVALID_VALUE pending. U.b must finish without inspecting that flag. */
    CHECK(mc_client_display_source_viewport(f.display, -1, 64, &display_error));
    mc_client_lighting_error lighting;
    CHECK(mc_client_lighting_enable(f.graphics, f.entities, f.math, f.display, caller, &lighting));
    CHECK(mc_client_display_source_get_error(f.display, &gl_error, &display_error) &&
          gl_error == GL_INVALID_VALUE);
    CHECK(mc_client_display_source_get_error(f.display, &gl_error, &display_error) &&
          gl_error == GL_NO_ERROR);
    compiled_prefix(&f);
    lit_draw_rejected(&f);
    retire(&f);
    math_failure();
}

int main(int argc, char **argv) {
    CHECK(argc == 2);
    if (strcmp(argv[1], "--controls") == 0) {
        device_controls();
        printf("Source lighting controls: %u checks\n", checks);
        return 0;
    }
    if (strcmp(argv[1], "--cpu") == 0 || strcmp(argv[1], "0") == 0) {
        cold_failure();
        printf("Source U.b lighting: cold_no_context; %u checks\n", checks);
        return 0;
    }
    char *end = NULL;
    const long profile = strtol(argv[1], &end, 10);
    CHECK(end != argv[1] && *end == '\0' && profile >= 1 && profile <= 4);
    device_profile((unsigned int)profile);
    static const char *const names[] = {"cold_no_context", "identity_cold", "seeded_projection",
                                        "seeded_replaced_modelview", "seeded_warm"};
    printf("Source U.b lighting: %s; %u checks\n", names[profile], checks);
    return 0;
}
