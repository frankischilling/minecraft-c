/* Prepared actual Block/CV/NW fields exercise unchanged source face methods. */
#include "client/render/blocks/internal.h"
#include "client/render/model_graph_internal.h"
#include "client/render_engine/internal.h"
#include "client/render_engine/nw_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/resources/translation/translation.h"

#include <emmintrin.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks, index;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "CV faces check%u at%d: %s\n", checks, __LINE__, #value);              \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

static const double bounds[][6] = {{0, 0, 0, 1, 1, 1},
                                   {.125, .25, .375, .75, .875, 1},
                                   {-.125, -.25, -.375, 1.125, 1.25, 1.375},
                                   {.875, .75, .625, .125, .25, .375}};
static const uint32_t corners[] = {0x3e000000, 0x3e800000, 0x3f000000, 0x3f400000,
                                   0x3f600000, 0x3f200000, 0x3ec00000, 0x3d800000,
                                   0x3f800000, 0x3f000001, 0xbe800000, 0x7fc12345};
static const uint32_t colors[][3] = {{0, 0, 0},
                                     {0x3f000000, 0x3e800000, 0x3f400000},
                                     {0xbe800000, 0x3fa00000, 0x3f800000},
                                     {0x7fc12345, 0x7f800000, 0xff800000},
                                     {0x7f812345, 0xffc54321, 1},
                                     {0x7f7fffff, 0xff7fffff, 0x80000000},
                                     {0x3f000001, 0x3effffff, 0x3f800001},
                                     {0x3b808081, 0x3f7fffff, 0x80000001},
                                     {0x7149f2ca, 0xf149f2ca, 0x3f600000},
                                     {0x00800000, 0x80800000, 0x3f7f7f7f},
                                     {0x3f7ffffe, 0x3b808080, 0x3b808082},
                                     {0x4b008081, 0xcb008081, 0x3eaaaaab}};

typedef struct resource_stream {
    const uint8_t *data;
    size_t length, position;
    bool closed;
} resource_stream;

typedef struct resources {
    resource_stream streams[3];
    size_t opens, closes;
    bool invalid;
} resources;

static mc_java_properties_status read_resource(void *owner, uint8_t *out, size_t capacity,
                                               size_t *count) {
    resource_stream *s = owner;
    if (s == NULL || out == NULL || capacity == 0 || count == NULL || s->closed ||
        s->position > s->length)
        return MC_JAVA_PROPERTIES_IO_ERROR;
    size_t take = s->length - s->position;
    if (take > capacity)
        take = capacity;
    if (take != 0)
        memcpy(out, s->data + s->position, take);
    s->position += take;
    *count = take;
    return MC_JAVA_PROPERTIES_OK;
}

static mc_registry_status open_resource(void *owner, const char *name,
                                        mc_java_properties_input *out) {
    static const char *const names[] = {"/lang/en_US.lang", "/lang/stats_US.lang",
                                        "/achievement/map.txt"};
    resources *r = owner;
    if (out == NULL || name == NULL || r->opens >= 3 || strcmp(name, names[r->opens]) != 0) {
        r->invalid = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    *out = (mc_java_properties_input){&r->streams[r->opens++], read_resource};
    return MC_REGISTRY_OK;
}

static mc_registry_status close_resource(void *owner, const mc_java_properties_input *input) {
    resources *r = owner;
    if (input == NULL || input->context != &r->streams[2] || input->read != read_resource) {
        r->invalid = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    r->streams[2].closed = true;
    ++r->closes;
    return MC_REGISTRY_OK;
}

static bool ready_resource(void *owner, const mc_java_properties_input *input) {
    resources *r = owner;
    if (input == NULL || input->read != read_resource)
        r->invalid = true;
    return false;
}

typedef struct fixture {
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_entity_classes *entities;
    mc_registry_classes *registry;
    mc_client_render_classes *models;
    mc_client_render_engine_classes *graphics;
    mc_client_tessellator *nw;
    const mc_block *stone;
    mc_client_render_blocks fields;
    mc_cv_context *context;
    const int32_t *original_words;
    int32_t original_capacity;
    int32_t short_words[48];
} fixture;

static float from_bits(uint32_t raw) {
    float value;
    memcpy(&value, &raw, sizeof(value));
    return value;
}

static uint64_t double_bits(double value) {
    uint64_t raw;
    memcpy(&raw, &value, sizeof(raw));
    return raw;
}

static bool prepare(fixture *f, unsigned int shape, int32_t side, int32_t rotation, bool flip,
                    bool ao, unsigned int texture, int kind) {
    memset(&f->fields, 0, sizeof(f->fields));
    mc_client_render_blocks_construct_fields(&f->fields, NULL, false);
    mc_block_fields *b = (mc_block_fields *)(void *)mc_block_fields_get(f->stone);
    CHECK(b != NULL);
    b->bs = bounds[shape][0];
    b->bt = bounds[shape][1];
    b->bu = bounds[shape][2];
    b->bv = bounds[shape][3];
    b->bw = bounds[shape][4];
    b->bx = bounds[shape][5];
    int32_t *rotations[] = {&f->fields.l, &f->fields.k, &f->fields.g,
                            &f->fields.h, &f->fields.j, &f->fields.i};
    *rotations[side] = rotation;
    f->fields.e = flip;
    f->fields.m = ao;
    f->fields.d = texture == 3 ? 302 : -1;
    for (size_t i = 0; i < sizeof(corners) / sizeof(*corners); ++i)
        f->fields.P_to_aa[i] = from_bits(corners[i]);
    static const int32_t capacities[] = {0, 4, 8, 16, 32, 47};
    const int32_t capacity = kind >= 2 ? capacities[kind - 2] : f->original_capacity;
    f->nw->fields.g = kind == 1 ? NULL : kind >= 2 ? f->short_words : f->original_words;
    f->nw->fields.B = capacity;
    for (int32_t i = 0; f->nw->fields.g != NULL && i < capacity && i < 48; ++i)
        ((int32_t *)(void *)f->nw->fields.g)[i] = INT32_C(0x5a6b7c8d);
    /* Explicit prepared batch state; no End/Draw success is credited here. */
    f->nw->fields.w = false;
    mc_client_graphics_error error;
    CHECK(mc_client_nw_translation(f->nw, shape == 3 ? 16777216.0 : 16.0,
                                   shape == 3 ? -64.0 : -48.0, shape == 3 ? -256.0 : -32.0,
                                   &error));
    CHECK(mc_client_nw_begin_quads(f->nw, &error));
    CHECK(mc_client_nw_color(f->nw, 31, 127, 233, 255, &error));
    memset(f->context, 0, sizeof(*f->context));
    f->context->fields = &f->fields;
    f->context->registry = f->registry;
    f->context->graphics = f->graphics;
    f->context->models = f->models;
    return true;
}

static void emit(fixture *f, int32_t side, const char *exception) {
    const mc_client_nw_fields *n = &f->nw->fields;
    printf("FACE %u %" PRId32 " prepared %" PRId32 " %" PRId32 " %" PRId32 " %08" PRIx32
           " %016" PRIx64 " %016" PRIx64 " %u %u %u %u %s ",
           index++, side, n->h, n->p, n->o, (uint32_t)n->k, double_bits(n->i), double_bits(n->j),
           n->l ? 1u : 0u, n->m ? 1u : 0u, n->n ? 1u : 0u, n->q ? 1u : 0u, exception);
    if (n->g == NULL)
        printf("N");
    else if (n->B == 0)
        printf("-");
    else
        for (int32_t i = 0; i < n->B && i < 48; ++i)
            printf("%s%08" PRIx32, i == 0 ? "" : ",", (uint32_t)n->g[i]);
    printf("\n");
}

static bool face(fixture *f, unsigned int shape, int32_t side, int32_t rotation, bool flip, bool ao,
                 unsigned int texture_case, int kind) {
    CHECK(prepare(f, shape, side, rotation, flip, ao, texture_case, kind));
    const mc_client_render_blocks before = f->fields;
    mc_client_render_engine_class_state held, released;
    CHECK(mc_client_render_engine_classes_inspect(f->graphics, &held));
    const int32_t texture = texture_case == 1 ? -1 : texture_case == 2 ? INT32_MAX : 83;
    const bool returned =
        mc_cv_face(f->context, side, kind == 0 ? NULL : f->stone, shape == 3 ? -16777215.75 : -3,
                   shape == 3 ? 65 : 50, shape == 3 ? 256.75 : 39, texture, 0);
    const bool expected = kind == -1 || kind == 7;
    CHECK(returned == expected && f->context->failed == !expected &&
          memcmp(&before, &f->fields, sizeof(before)) == 0 && !f->nw->active &&
          f->graphics->active_source_calls == 0 && f->graphics->active_resource_calls == 0);
    CHECK(mc_client_render_engine_classes_inspect(f->graphics, &released) &&
          released.native_holds == held.native_holds);
    const char *exception = "NONE";
    if (!returned) {
        CHECK(!f->context->failure.native_policy && f->context->depth > 0 &&
              strcmp(f->context->failure.source_class, "cv") == 0 &&
              f->context->failure.source_method[0] == 'a' + side &&
              f->context->failure.source_method[1] == '\0' &&
              strcmp(f->context->failure.source_descriptor, "(Luu;DDDI)V") == 0);
        exception = kind < 2 ? "NPE" : "AIOOBE";
        CHECK(kind < 2 ? f->context->failure.status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
                       : f->context->failure.graphics.status == MC_CLIENT_GRAPHICS_ARRAY_INDEX);
        if (kind == 0)
            CHECK(f->context->failure.source_pc == 39 && f->nw->fields.p == 0 &&
                  f->nw->fields.h == 0 && f->nw->fields.o == 0);
        else {
            static const unsigned int ao_calls[][4] = {{650, 682, 714, 746}, {650, 682, 714, 746},
                                                       {669, 701, 733, 765}, {669, 701, 733, 765},
                                                       {670, 702, 734, 766}, {670, 702, 734, 766}};
            static const unsigned int flat_calls[][4] = {
                {764, 779, 794, 809}, {764, 779, 794, 809}, {783, 798, 813, 828},
                {783, 798, 813, 828}, {784, 799, 814, 829}, {784, 799, 814, 829}};
            const unsigned int vertex = kind <= 3 ? 0 : (unsigned int)kind - 3;
            CHECK(f->context->failure.source_pc ==
                      (ao ? ao_calls[side][vertex] : flat_calls[side][vertex]) &&
                  strcmp(f->context->failure.graphics.site.class_name, "nw") == 0 &&
                  strcmp(f->context->failure.graphics.site.descriptor, "(DDD)V") == 0 &&
                  f->context->failure.graphics.site.pc == (kind == 3   ? 287
                                                           : kind == 6 ? 83
                                                                       : 268) &&
                  f->context->failure.graphics.parent.pc == 13 &&
                  strcmp(f->context->failure.graphics.parent.descriptor, "(DDDDD)V") == 0);
        }
    } else
        CHECK(f->context->depth == 0 && f->nw->fields.p == 4 && f->nw->fields.h == 6 &&
              f->nw->fields.o == 48 && f->nw->fields.m && f->nw->fields.l);
    emit(f, side, exception);
    f->nw->fields.g = f->original_words;
    f->nw->fields.B = f->original_capacity;
    return true;
}

static bool run(void) {
    fixture f = {0};
    f.context = calloc(1, sizeof(*f.context));
    CHECK(f.context != NULL);
    CHECK(mc_materials_create(&f.materials));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f.profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f.profile, NULL, &f.entities) == MC_ENTITY_OK);
    static const uint8_t language[] = "stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                                      "stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n";
    resources r = {.streams = {{language, sizeof(language) - 1, 0, false}, {0}, {0}}};
    const mc_registry_resources resource_api = {&r, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f.profile, f.materials, 0,
                                         &resource_api};
    CHECK(mc_registry_classes_allocate(&options, &f.registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_bind_entities(f.registry, f.entities, NULL) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_initialize_blocks(f.registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_registry_block_get(f.registry, 1, &f.stone, NULL) == MC_REGISTRY_OK &&
          f.stone != NULL);
    CHECK(mc_client_render_classes_create(f.entities, f.profile, &f.models) == MC_CLIENT_MODEL_OK);
    mc_client_model_error model_error;
    CHECK(mc_client_render_blocks_active_use(f.models, &model_error));
    CHECK(mc_client_render_engine_classes_create(NULL, &f.graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_graphics_error error;
    CHECK(mc_client_nw_active_use(
        f.graphics, (mc_client_graphics_site){"native-test", "faces", "()V", 0}, &f.nw, &error));
    f.original_words = f.nw->fields.g;
    f.original_capacity = f.nw->fields.B;
    for (unsigned int shape = 0; shape < 4; ++shape)
        for (int32_t side = 0; side < 6; ++side)
            for (int32_t rotation = 0; rotation < 4; ++rotation)
                for (unsigned int flip = 0; flip < 2; ++flip)
                    for (unsigned int ao = 0; ao < 2; ++ao)
                        for (unsigned int texture = 0; texture < 4; ++texture)
                            CHECK(face(&f, shape, side, rotation, flip != 0, ao != 0, texture, -1));
    static const int32_t unusual[] = {-1, 4};
    for (size_t i = 0; i < sizeof(unusual) / sizeof(*unusual); ++i)
        for (int32_t side = 0; side < 6; ++side)
            for (unsigned int ao = 0; ao < 2; ++ao)
                CHECK(face(&f, 1, side, unusual[i], true, ao != 0, 0, -1));
    for (int32_t side = 0; side < 6; ++side)
        for (unsigned int ao = 0; ao < 2; ++ao)
            for (int kind = 0; kind < 8; ++kind)
                CHECK(face(&f, 1, side, 0, false, ao != 0, 0, kind));
    CHECK(index == 1656);
    unsigned int color_index = 0;
    for (unsigned int blocked = 0; blocked < 2; ++blocked)
        for (size_t i = 0; i < sizeof(colors) / sizeof(*colors); ++i) {
            CHECK(prepare(&f, 0, 0, 0, false, false, 0, -1));
            f.nw->fields.q = blocked != 0;
            CHECK(mc_client_nw_color_float(f.nw, from_bits(colors[i][0]), from_bits(colors[i][1]),
                                           from_bits(colors[i][2]), &error));
            CHECK(!f.nw->active && f.graphics->active_source_calls == 0 && f.nw->fields.l &&
                  (blocked == 0 || (uint32_t)f.nw->fields.k == UINT32_C(0xffe97f1f)));
            printf("FCOLOR %u %u %08" PRIx32 " %u %u NONE\n", color_index++, blocked,
                   (uint32_t)f.nw->fields.k, f.nw->fields.l ? 1u : 0u, f.nw->fields.q ? 1u : 0u);
        }
    CHECK(color_index == 24);
    const int32_t prior = f.nw->fields.k;
    const unsigned int controls = _mm_getcsr();
    _mm_setcsr((controls & ~0x6000u) | 0x2000u);
    const bool bad_fp = mc_client_nw_color_float(f.nw, 1, 0, 0, &error);
    _mm_setcsr(controls);
    CHECK(!bad_fp && error.status == MC_CLIENT_GRAPHICS_UNSUPPORTED_FP && error.native_policy &&
          error.site.pc == 2 && strcmp(error.site.descriptor, "(FFF)V") == 0 &&
          f.nw->fields.k == prior && !f.nw->active && f.graphics->active_source_calls == 0);
    CHECK(!r.invalid && r.opens == 3 && r.closes == 1);
    CHECK(mc_client_render_engine_classes_release(f.graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_classes_free(f.models) == MC_CLIENT_MODEL_OK);
    CHECK(mc_registry_classes_free(f.registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_entity_classes_free(f.entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(f.profile);
    mc_materials_destroy(f.materials);
    free(f.context);
    return true;
}

int main(void) {
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u || !run())
        return 1;
    printf("CV faces: %u checks; 1656 rows and 24 float colors\n", checks);
    return 0;
}
