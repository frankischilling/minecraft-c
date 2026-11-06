/* Whole NW normal method and unchanged converted-quad normal words. */
#include "client/render_engine/internal.h"
#include "client/render_engine/nw_internal.h"
#include "client/render_engine/textures.h"

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
            fprintf(stderr, "NW normal check%u at%d: %s\n", checks, __LINE__, #value);             \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

static const mc_client_graphics_site caller = {"fixture", "nw_normals", "()V", -1};

static void state(const char *tag, const mc_client_tessellator *owner) {
    const mc_client_nw_fields *f = &owner->fields;
    uint32_t packed;
    memcpy(&packed, &f->v, sizeof(packed));
    printf("NW_STATE %s %u %u %08" PRIx32 " %" PRId32 " %" PRId32 " %" PRId32 " %u %u %u\n", tag,
           f->w ? 1u : 0u, f->n ? 1u : 0u, packed, f->h, f->o, f->p, f->l ? 1u : 0u, f->m ? 1u : 0u,
           f->q ? 1u : 0u);
    CHECK(fflush(stdout) == 0);
}

static float from_bits(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void normal_inputs(mc_client_tessellator *owner) {
    static const struct {
        const char *name;
        uint32_t bits[3];
    } cases[] = {
        {"positive_zero", {0x00000000, 0x00000000, 0x00000000}},
        {"negative_zero", {0x80000000, 0x80000000, 0x80000000}},
        {"positive_x", {0x3f800000, 0x00000000, 0x00000000}},
        {"negative_x", {0xbf800000, 0x00000000, 0x00000000}},
        {"positive_y", {0x00000000, 0x3f800000, 0x00000000}},
        {"negative_y", {0x00000000, 0xbf800000, 0x00000000}},
        {"positive_z", {0x00000000, 0x00000000, 0x3f800000}},
        {"negative_z", {0x00000000, 0x00000000, 0xbf800000}},
        {"fractional", {0x3e800000, 0xbf000000, 0x3f400000}},
        {"nonunit", {0x40000000, 0xc0000000, 0x43000000}},
        {"near_wrap", {0x3f7e0000, 0x3f800000, 0xbf800000}},
        {"quiet_nan", {0x7fc12345, 0xffc54321, 0x7fc00000}},
        {"signaling_nan", {0x7f800001, 0xff800001, 0x7fa12345}},
        {"infinities", {0x7f800000, 0xff800000, 0x7f800000}},
        {"maximum_finite", {0x7f7fffff, 0xff7fffff, 0x7f7fffff}},
        {"subnormal", {0x00000001, 0x80000001, 0x007fffff}},
        {"saturation", {0x4f000000, 0xcf000000, 0x4f000000}},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        mc_client_graphics_error error;
        const int32_t h = owner->fields.h, o = owner->fields.o, p = owner->fields.p;
        CHECK(mc_client_nw_normal(owner, from_bits(cases[i].bits[0]), from_bits(cases[i].bits[1]),
                                  from_bits(cases[i].bits[2]), &error));
        CHECK(error.status == MC_CLIENT_GRAPHICS_OK && !error.cleanup_failed &&
              owner->last_store_pc == 59 && owner->fields.n && owner->fields.w &&
              owner->fields.h == h && owner->fields.o == o && owner->fields.p == p);
        state(cases[i].name, owner);
    }
}

static void geometry(mc_client_tessellator *owner) {
    int32_t *array = (int32_t *)(void *)owner->fields.g;
    array[3 * 8 + 6] = INT32_C(0x11223344);
    array[4 * 8 + 6] = INT32_C(0x55667788);
    static const float normals[4][3] = {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}, {-1, 0, 0}};
    static const double positions[4][3] = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};
    for (size_t i = 0; i < 4; ++i) {
        mc_client_graphics_error error;
        CHECK(mc_client_nw_normal(owner, normals[i][0], normals[i][1], normals[i][2], &error));
        CHECK(
            mc_client_nw_vertex(owner, positions[i][0], positions[i][1], positions[i][2], &error));
    }
    CHECK(array == owner->fields.g && owner->fields.h == 6 && owner->fields.o == 48 &&
          owner->fields.p == 4 && array[3 * 8 + 6] == INT32_C(0x11223344) &&
          array[4 * 8 + 6] == INT32_C(0x55667788));
    state("quad", owner);
    for (size_t i = 0; i < 6; ++i) {
        printf("NW_VERTEX quad %zu ", i);
        for (size_t word = 0; word < 8; ++word) {
            uint32_t bits;
            memcpy(&bits, &array[i * 8 + word], sizeof(bits));
            printf("%s%08" PRIx32, word == 0 ? "" : ",", bits);
        }
        putchar('\n');
    }
    CHECK(fflush(stdout) == 0);
}

static void fp_prefix(void) {
    mc_client_render_engine_classes *classes = NULL;
    mc_client_tessellator *owner = NULL;
    mc_client_graphics_error error;
    CHECK(mc_client_render_engine_classes_create(NULL, &classes) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_nw_active_use(classes, caller, &owner, &error));
    CHECK(mc_client_nw_begin_quads(owner, &error));
    CHECK(!owner->fields.n && owner->fields.v == 0);
    const unsigned int saved = _mm_getcsr();
    _mm_setcsr(saved | 0x8000u);
    const bool result = mc_client_nw_normal(owner, 1, 0, 0, &error);
    const unsigned int retained = _mm_getcsr();
    _mm_setcsr(saved);
    CHECK(!result && error.status == MC_CLIENT_GRAPHICS_UNSUPPORTED_FP && error.native_policy &&
          error.site.pc == 23 && strcmp(error.site.method, "b") == 0 &&
          strcmp(error.site.descriptor, "(FFF)V") == 0 && owner->fields.n && owner->fields.v == 0 &&
          owner->last_store_pc == 17 && (retained & 0xffc0u) == ((saved | 0x8000u) & 0xffc0u));
    CHECK(classes->active_source_calls == 0 && !owner->active);
    CHECK(mc_client_nw_normal(owner, 1, 0, 0, &error) && owner->last_store_pc == 59);
    CHECK(mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_OK);
}

int main(void) {
    mc_client_render_engine_classes *classes = NULL;
    mc_client_tessellator *owner = NULL;
    mc_client_graphics_error error;
    mc_java_system_output *system = NULL;
    mc_java_print_stream *actual_out = NULL;
    mc_java_print_error print;
    CHECK(mc_client_render_engine_classes_create(NULL, &classes) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_nw_active_use(classes, caller, &owner, &error));
    CHECK(!mc_client_nw_normal(owner, 1, 0, 0, &error));
    CHECK(error.status == MC_CLIENT_GRAPHICS_UNIMPLEMENTED_DEPENDENCY && error.site.pc == 7 &&
          !owner->fields.w && !owner->fields.n && owner->fields.v == 0 &&
          owner->last_store_pc == 125 && !error.cleanup_failed);
    CHECK(mc_java_system_output_allocate(&system, &print) == MC_JAVA_PRINT_OK);
    CHECK(mc_java_system_output_construct(system, &print) == MC_JAVA_PRINT_OK);
    CHECK(mc_client_nw_bind_system_output(classes, system, &error));
    mc_java_system_output_state output_state;
    CHECK(mc_java_system_output_inspect(system, &output_state) && output_state.holds == 1);
    state("cold", owner);
    CHECK(mc_client_nw_normal(owner, 1, 0, 0, &error));
    CHECK(error.print.source_returned && error.print.status == MC_JAVA_PRINT_OK &&
          !error.cleanup_failed && owner->last_store_pc == 59);
    state("outside0", owner);
    CHECK(mc_client_nw_normal(owner, -1, 0.5f, -0.5f, &error));
    state("outside1", owner);
    CHECK(mc_java_system_output_get(system, &actual_out, &print) == MC_JAVA_PRINT_OK &&
          actual_out != NULL);
    CHECK(mc_java_system_output_bind(system, NULL, &print) == MC_JAVA_PRINT_OK);
    const int32_t before_null = owner->fields.v;
    CHECK(!mc_client_nw_normal(owner, 0, 0, 0, &error));
    CHECK(error.status == MC_CLIENT_GRAPHICS_NULL_REFERENCE && !error.native_policy &&
          error.site.pc == 12 && owner->fields.n && owner->fields.v == before_null &&
          owner->last_store_pc == 59 && !error.cleanup_failed);
    state("null_out", owner);
    CHECK(mc_java_system_output_bind(system, actual_out, &print) == MC_JAVA_PRINT_OK);
    CHECK(mc_java_print_stream_release(&actual_out, &print) == MC_JAVA_PRINT_OK &&
          actual_out == NULL);
    CHECK(mc_client_nw_begin_quads(owner, &error));
    CHECK(!owner->fields.n && owner->fields.v == before_null);
    state("begin", owner);
    normal_inputs(owner);
    const int32_t before_begin = owner->fields.v;
    CHECK(!mc_client_nw_begin_quads(owner, &error));
    CHECK(error.status == MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_STATE && !error.native_policy &&
          error.site.pc == 16 && owner->fields.n && owner->fields.v == before_begin);
    state("begin_failure", owner);
    geometry(owner);
    CHECK(classes->active_source_calls == 0 && !owner->active);
    /* A surviving counted class owner must not retain a borrowed System after
     * its binder retires. Resource retirement still permits disassociation. */
    CHECK(mc_client_render_engine_classes_retain(classes) == MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_render_texture_error retirement;
    CHECK(mc_client_render_engine_retire_resources(classes, &retirement) ==
          MC_CLIENT_RENDER_TEXTURE_OK);
    CHECK(classes->resource_retirement_started && classes->nw_system_output == system);
    CHECK(!mc_client_nw_bind_system_output(classes, system, &error) &&
          error.status == MC_CLIENT_GRAPHICS_INVALID_PHASE);
    CHECK(mc_client_nw_bind_system_output(classes, NULL, &error) &&
          classes->nw_system_output == NULL);
    CHECK(mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_java_system_output_release(&system, &print) == MC_JAVA_PRINT_OK && system == NULL);
    CHECK(classes->nw_system_output == NULL);
    CHECK(mc_client_render_engine_classes_release(classes) == MC_CLIENT_RENDER_ENGINE_OK);
    fp_prefix();
    printf("Source NW normals: %u checks\n", checks);
    return 0;
}
