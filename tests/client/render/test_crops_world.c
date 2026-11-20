#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

/* Genuine saved World, Registry and Chunk owners; earlier mains never run. */
#define MC_PICKING_BODY_MAIN_NAME mc_crops_picking_fixture_main
int mc_crops_picking_fixture_main(int argc, char **argv);
#include "../world/test_picking.c"
#undef main

#include "block/registry/registry_internal.h"
#include "client/render/blocks/internal.h"
#include "client/render/model_graph_internal.h"
#include "client/render_engine/internal.h"
#include "client/render_engine/nw.h"
#include "mc/core/java/java.h"
#include "world/chunk/object_state_internal.h"

#include <limits.h>
#include <xmmintrin.h>

static uint32_t crops_float_bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static uint64_t crops_double_bits(double value) {
    uint64_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static const char *crops_bool(bool value) {
    return value ? "true" : "false";
}

/* Compare source fields and actual reference identities, not C padding. */
static bool crops_cv_same(const mc_client_render_blocks *left,
                          const mc_client_render_blocks *right) {
    return left->c == right->c && left->d == right->d && left->e == right->e &&
           left->f == right->f && left->b == right->b && left->g == right->g &&
           left->h == right->h && left->i == right->i && left->j == right->j &&
           left->k == right->k && left->l == right->l && left->m == right->m &&
           left->O == right->O && memcmp(left->n_to_N, right->n_to_N, sizeof(left->n_to_N)) == 0 &&
           memcmp(left->P_to_aa, right->P_to_aa, sizeof(left->P_to_aa)) == 0 &&
           memcmp(left->ab_to_am, right->ab_to_am, sizeof(left->ab_to_am)) == 0;
}

static bool crops_block_same(const mc_block_fields *left, const mc_block_fields *right) {
    return left->bm == right->bm && left->bn == right->bn &&
           crops_float_bits(left->bo) == crops_float_bits(right->bo) &&
           crops_float_bits(left->bp) == crops_float_bits(right->bp) && left->bq == right->bq &&
           left->br == right->br && crops_double_bits(left->bs) == crops_double_bits(right->bs) &&
           crops_double_bits(left->bt) == crops_double_bits(right->bt) &&
           crops_double_bits(left->bu) == crops_double_bits(right->bu) &&
           crops_double_bits(left->bv) == crops_double_bits(right->bv) &&
           crops_double_bits(left->bw) == crops_double_bits(right->bw) &&
           crops_double_bits(left->bx) == crops_double_bits(right->bx) && left->by == right->by &&
           crops_float_bits(left->bz) == crops_float_bits(right->bz) && left->bA == right->bA &&
           crops_float_bits(left->bB) == crops_float_bits(right->bB) && left->a == right->a;
}

static bool crops_nw_same(const mc_client_nw_fields *left, const mc_client_nw_fields *right) {
    return left->d == right->d && left->e == right->e && left->f == right->f &&
           left->g == right->g && left->h == right->h &&
           crops_double_bits(left->i) == crops_double_bits(right->i) &&
           crops_double_bits(left->j) == crops_double_bits(right->j) && left->k == right->k &&
           left->l == right->l && left->m == right->m && left->n == right->n &&
           left->o == right->o && left->p == right->p && left->q == right->q &&
           left->r == right->r && crops_double_bits(left->s) == crops_double_bits(right->s) &&
           crops_double_bits(left->t) == crops_double_bits(right->t) &&
           crops_double_bits(left->u) == crops_double_bits(right->u) && left->v == right->v &&
           left->w == right->w && left->x == right->x && left->y == right->y &&
           left->z == right->z && left->A == right->A && left->B == right->B;
}

static void crops_cv_fields(const mc_client_render_blocks *fields) {
    for (size_t i = 13; i < 27; ++i)
        printf("%s%c=%08" PRIx32, i == 13 ? "" : ",", (int)('A' + i - 13),
               crops_float_bits(fields->n_to_N[i]));
    printf(",O=%" PRId32, fields->O);
    for (size_t i = 0; i < 11; ++i)
        printf(",%c=%08" PRIx32, (int)('P' + i), crops_float_bits(fields->P_to_aa[i]));
    printf(",aa=%08" PRIx32, crops_float_bits(fields->P_to_aa[11]));
    for (size_t i = 0; i < 12; ++i)
        printf(",a%c=%s", (int)('b' + i), crops_bool(fields->ab_to_am[i]));
    printf(",b=%s,d=%" PRId32 ",e=%s,f=%s,g=%" PRId32 ",h=%" PRId32 ",i=%" PRId32 ",j=%" PRId32
           ",k=%" PRId32 ",l=%" PRId32 ",m=%s",
           crops_bool(fields->b), fields->d, crops_bool(fields->e), crops_bool(fields->f),
           fields->g, fields->h, fields->i, fields->j, fields->k, fields->l, crops_bool(fields->m));
    for (size_t i = 0; i < 13; ++i)
        printf(",%c=%08" PRIx32, (int)('n' + i), crops_float_bits(fields->n_to_N[i]));
}

static void crops_nw_fields(const mc_client_nw_fields *fields) {
    printf("A=%" PRId32 ",B=%" PRId32 ",h=%" PRId32 ",i=%016" PRIx64 ",j=%016" PRIx64 ",k=%" PRId32
           ",l=%s,m=%s,n=%s,o=%" PRId32 ",p=%" PRId32 ",q=%s,r=%" PRId32 ",s=%016" PRIx64
           ",t=%016" PRIx64 ",u=%016" PRIx64 ",v=%" PRId32 ",w=%s,x=%s,z=%" PRId32,
           fields->A, fields->B, fields->h, crops_double_bits(fields->i),
           crops_double_bits(fields->j), fields->k, crops_bool(fields->l), crops_bool(fields->m),
           crops_bool(fields->n), fields->o, fields->p, crops_bool(fields->q), fields->r,
           crops_double_bits(fields->s), crops_double_bits(fields->t), crops_double_bits(fields->u),
           fields->v, crops_bool(fields->w), crops_bool(fields->x), fields->z);
}

static void crops_bounds(const mc_block_fields *fields) {
    const double box[6] = {fields->bs, fields->bt, fields->bu, fields->bv, fields->bw, fields->bx};
    for (size_t i = 0; i < 6; ++i)
        printf("%s%016" PRIx64, i == 0 ? "" : ",", crops_double_bits(box[i]));
}

typedef struct crops_buffer_snapshot {
    mc_client_direct_byte_buffer bytes;
    mc_client_direct_int_buffer ints;
    mc_client_direct_float_buffer floats;
} crops_buffer_snapshot;

static bool crops_buffers_capture(const mc_client_nw_fields *fields, crops_buffer_snapshot *out) {
    CHECK(fields->d != NULL && fields->e != NULL && fields->f != NULL && fields->y == NULL);
    out->bytes = *fields->d;
    out->ints = *fields->e;
    out->floats = *fields->f;
    CHECK(out->ints.bytes == fields->d && out->floats.bytes == fields->d &&
          out->bytes.data != NULL && out->bytes.position == 0 &&
          out->bytes.limit == out->bytes.capacity && out->bytes.capacity == 8388608 &&
          out->ints.position == 0 && out->ints.limit == out->ints.capacity &&
          out->ints.capacity == 2097152 && out->floats.position == 0 &&
          out->floats.limit == out->floats.capacity && out->floats.capacity == 2097152);
    return true;
}

static bool crops_buffers_same(const mc_client_nw_fields *fields,
                               const crops_buffer_snapshot *before) {
    CHECK(
        fields->d != NULL && fields->e != NULL && fields->f != NULL && fields->y == NULL &&
        fields->d->position == before->bytes.position && fields->d->limit == before->bytes.limit &&
        fields->d->capacity == before->bytes.capacity && fields->d->data == before->bytes.data &&
        fields->e->position == before->ints.position && fields->e->limit == before->ints.limit &&
        fields->e->capacity == before->ints.capacity && fields->e->bytes == before->ints.bytes &&
        fields->f->position == before->floats.position &&
        fields->f->limit == before->floats.limit &&
        fields->f->capacity == before->floats.capacity && fields->f->bytes == before->floats.bytes);
    return true;
}

static bool crops_prepare(picking_fixture *f, int32_t metadata, int32_t light,
                          const int32_t xyz[3]) {
    const int32_t safe[3] = {xyz[0], xyz[1] >= 1 && xyz[1] < 127 ? xyz[1] : 64, xyz[2]};
    xyz = safe;
    for (size_t i = 0; i < 4; ++i) {
        memset(f->chunks[i]->data->blocks, 0, MC_CHUNK_BLOCKS);
        memset(f->chunks[i]->data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
        memset(f->chunks[i]->data->skylight, 0, MC_CHUNK_NIBBLE_BYTES);
        memset(f->chunks[i]->data->block_light, 0, MC_CHUNK_NIBBLE_BYTES);
    }
    mc_chunk_state *chunk = f->chunks[0];
    for (int32_t dx = -1; dx <= 1; ++dx)
        for (int32_t dy = -1; dy <= 1; ++dy)
            for (int32_t dz = -1; dz <= 1; ++dz)
                CHECK(mc_chunk_nibbles_set(chunk->data->skylight, MC_CHUNK_NIBBLE_BYTES,
                                           xyz[0] + dx, xyz[1] + dy, xyz[2] + dz,
                                           light == 1 ? 15 : 0) &&
                      mc_chunk_nibbles_set(chunk->data->block_light, MC_CHUNK_NIBBLE_BYTES,
                                           xyz[0] + dx, xyz[1] + dy, xyz[2] + dz,
                                           light == 2 ? 15 : 0));
    chunk->data->blocks[((size_t)xyz[0] << 11u) | ((size_t)xyz[2] << 7u) | (size_t)xyz[1]] = 59;
    CHECK(mc_chunk_nibbles_set(chunk->data->metadata, MC_CHUNK_NIBBLE_BYTES, xyz[0], xyz[1], xyz[2],
                               metadata & 15));
    return true;
}

static bool crops_dispatch(picking_fixture *f, mc_client_chunk_cache *cache, const mc_block *block,
                           uint32_t words[4][384]) {
    static const int32_t stages[4] = {0, 7, 8, 15};
    const int32_t xyz[3] = {8, 64, 8};
    mc_client_tessellator *nw = NULL;
    mc_client_graphics_error graphics;
    CHECK(mc_client_nw_active_use(f->graphics,
                                  (mc_client_graphics_site){"native-control", "crops", "", -1}, &nw,
                                  &graphics) &&
          mc_client_nw_translation(nw, -8, -64, -8, &graphics));
    for (size_t stage = 0; stage < 4; ++stage) {
        CHECK(crops_prepare(f, stages[stage], 1, xyz));
        mc_client_render_blocks fields = {0};
        mc_client_render_blocks_construct_fields(&fields, cache, true);
        const mc_client_render_blocks original_cv = fields;
        const mc_block_fields original_block = *mc_block_fields_get(block);
        mc_cv_context context = {.fields = &fields,
                                 .cache = cache,
                                 .registry = f->base.registry,
                                 .models = f->models,
                                 .graphics = f->graphics,
                                 .application = f->app};
        mc_client_nw_state before, after;
        bool rendered = false;
        CHECK(mc_client_nw_inspect(f->graphics, &before) &&
              mc_cv_render(&context, block, 8, 64, 8, &rendered) && rendered && context.returned &&
              !context.failed && context.depth == 0 && context.source_pc == 98 &&
              context.last_store_pc == 0 && crops_cv_same(&fields, &original_cv) &&
              crops_block_same(mc_block_fields_get(block), &original_block) &&
              mc_client_nw_inspect(f->graphics, &after) && after.fields.o == before.fields.o + 384);
        for (size_t i = 0; i < 384; ++i)
            CHECK((uint32_t)after.fields.g[(size_t)before.fields.o + i] == words[stage][i]);
    }
    for (int primitive = 0; primitive < 2; ++primitive) {
        mc_client_render_blocks fields = {0};
        mc_client_render_blocks_construct_fields(&fields, cache, true);
        mc_cv_context context = {.fields = &fields,
                                 .cache = cache,
                                 .registry = f->base.registry,
                                 .models = f->models,
                                 .graphics = f->graphics,
                                 .application = f->app};
        mc_client_nw_state before, after;
        CHECK(mc_client_nw_inspect(f->graphics, &before));
        if (!primitive)
            CHECK(mc_cv_push(&context, "j", "(Luu;III)Z"));
        bool output = true;
        const unsigned int csr = _mm_getcsr();
        _mm_setcsr(csr | 0x8040u);
        const bool completed = primitive ? mc_cv_crops_geometry(&context, block, 7, 8, 64, 8)
                                         : mc_cv_crops(&context, block, 8, 64, 8, &output);
        _mm_setcsr(csr);
        CHECK(!completed && output && context.failed &&
              context.source_pc == (primitive ? 48u : 27u) &&
              context.failure.status == MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP &&
              context.failure.native_policy && mc_client_nw_inspect(f->graphics, &after) &&
              crops_nw_same(&before.fields, &after.fields));
    }
    return true;
}

static bool crops_controls(picking_fixture *f, const char *profiles) {
    static const int32_t coordinates[7][3] = {{8, 64, 8},       {8, 64, 8},        {3, 65, 3},
                                              {8, 16777217, 8}, {8, -16777217, 8}, {8, INT_MAX, 8},
                                              {8, INT_MIN, 8}};
    static const double translations[3][3] = {{-.25, .5, -.75}, {-8, -64, -8}, {0, -64, 0}};
    static const float boxes[3][6] = {{0, 0, 0, 1, 1, 1},
                                      {.125f, .25f, .375f, .875f, .75f, .625f},
                                      {-.25f, -.5f, -.125f, 1.5f, 1.25f, 1.75f}};
    mc_client_chunk_cache *cache = NULL;
    mc_client_chunk_cache_error cache_error;
    const size_t bindings = f->base.world->persistent_bindings;
    CHECK(mc_client_chunk_cache_create(f->base.world, 0, 0, 0, 31, 127, 31, &cache, &cache_error) ==
          MC_CLIENT_CHUNK_CACHE_OK);
    mc_client_model_error model_error;
    CHECK(mc_client_render_blocks_active_use(f->models, &model_error));
    mc_client_tessellator *nw = NULL;
    mc_client_graphics_error graphics;
    CHECK(mc_client_nw_active_use(f->graphics,
                                  (mc_client_graphics_site){"fixture", "crops", "()V", 0}, &nw,
                                  &graphics) &&
          mc_client_nw_begin_quads(nw, &graphics));
    f->base.world->fields.f = 3;
    mc_client_nw_state initial;
    CHECK(mc_client_nw_inspect(f->graphics, &initial) && initial.static_b && !initial.static_c &&
          initial.fields.B == 2097152 && initial.fields.o == 0 && initial.fields.g != NULL);
    const size_t capacity = (size_t)initial.fields.B;
    int32_t *prior_words = malloc(capacity * sizeof(*prior_words));
    CHECK(prior_words != NULL);
    const mc_block *view = NULL;
    mc_block_render_description description;
    CHECK(mc_registry_block_get(f->base.registry, 59, &view, NULL) == MC_REGISTRY_OK &&
          view != NULL && view->kind == MC_BLOCK_CROPS && view->fields.bn == 59 &&
          view->fields.bm == 88 && mc_block_render_describe(view, &description) == MC_REGISTRY_OK &&
          description.type == 6 && description.pass == 0);
    mc_block *block = (mc_block *)(void *)view;
    const mc_block_fields original_block = block->fields;
    int32_t *const emissions = f->base.registry->blocks.s;
    CHECK(emissions != NULL && emissions[59] == 0);
    FILE *input = fopen(profiles, "rb");
    CHECK(input != NULL);
    char name[128];
    int values[13], scanned;
    unsigned int index = 0, visible = 0, failed = 0;
    size_t own_unchanged_words = 0;
    uint32_t section_words[4][384] = {{0}};
    while ((scanned = fscanf(input, "%127s%d%d%d%d%d%d%d%d%d%d%d%d%d", name, &values[0], &values[1],
                             &values[2], &values[3], &values[4], &values[5], &values[6], &values[7],
                             &values[8], &values[9], &values[10], &values[11], &values[12])) ==
           14) {
        CHECK(index < 44 && (values[12] == 1 || (values[0] >= 0 && values[0] <= 15)) &&
              values[1] >= 0 && values[1] <= 2 && values[4] >= 0 && values[4] <= 2 &&
              values[5] >= 0 && values[5] <= 3 && values[6] >= 0 && values[6] <= 1 &&
              values[8] >= 0 && values[8] <= 6 && values[9] >= 0 && values[9] <= 1 &&
              values[10] >= 0 && values[10] <= 1 && values[11] >= 0 && values[11] <= 3 &&
              values[12] >= 0 && values[12] <= 1);
        block->fields.bm = mc_java_i32_add(original_block.bm, values[3]);
        emissions[59] = values[7] == INT_MIN ? 0 : values[7];
        const int32_t *xyz = coordinates[values[8]];
        const double *translation = translations[values[8] < 3 ? values[8] : 0];
        const float *box = boxes[values[4]];
        mc_registry_block_bounds(block, box[0], box[1], box[2], box[3], box[4], box[5]);
        CHECK(crops_prepare(f, values[0], values[1], xyz) &&
              mc_client_nw_translation(nw, translation[0], translation[1], translation[2],
                                       &graphics));
        const mc_block_fields entered_block = block->fields;
        mc_client_render_blocks fields = {0};
        const bool null_cache = values[5] == 1 || values[5] == 3;
        mc_client_render_blocks_construct_fields(&fields, null_cache ? NULL : cache, true);
        fields.m = values[6] != 0;
        fields.f = values[9] != 0;
        fields.d = values[2];
        fields.e = values[10] != 0;
        fields.g = fields.h = fields.i = fields.j = fields.k = fields.l = values[11];
        for (size_t i = 0; i < 27; ++i)
            fields.n_to_N[i] = (float)(i + 1) * .03125f;
        for (size_t i = 0; i < 12; ++i) {
            fields.P_to_aa[i] = (float)(i + 1) * .015625f;
            fields.ab_to_am[i] = (i & 1u) != 0;
        }
        const mc_client_render_blocks entered_cv = fields;
        int32_t entered_emissions[256];
        memcpy(entered_emissions, emissions, sizeof(entered_emissions));
        mc_client_nw_state before, after;
        crops_buffer_snapshot buffers;
        CHECK(mc_client_nw_inspect(f->graphics, &before) && before.fields.g == initial.fields.g &&
              crops_buffers_capture(&before.fields, &buffers));
        memcpy(prior_words, before.fields.g, capacity * sizeof(*prior_words));
        f->base.world_classes->chunk_a = false;
        mc_cv_context context = {.fields = &fields,
                                 .cache = null_cache ? NULL : cache,
                                 .registry = f->base.registry,
                                 .models = f->models,
                                 .graphics = f->graphics,
                                 .application = f->app};
        if (!values[12])
            CHECK(mc_cv_push(&context, "j", "(Luu;III)Z"));
        const bool expected_failure = values[5] >= 2 || (!values[12] && values[5] == 1);
        bool output = expected_failure;
        const bool completed =
            values[12] ? mc_cv_crops_geometry(&context, values[5] >= 2 ? NULL : block, values[0],
                                              (double)xyz[0] + .125, (double)xyz[1] + .375,
                                              (double)xyz[2] - .25)
                       : mc_cv_crops(&context, values[5] >= 2 ? NULL : block, xyz[0], xyz[1],
                                     xyz[2], &output);
        CHECK(completed == !expected_failure && context.tessellator == nw);
        if (completed) {
            CHECK((values[12] || output) && !context.failed &&
                  context.depth == (values[12] ? 0u : 1u) &&
                  context.source_pc == (values[12] ? 721u : 60u) && context.last_store_pc == 0);
        } else {
            CHECK(output && context.failed && context.last_store_pc == 0 &&
                  context.failure.status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
                  !context.failure.native_policy);
            if (values[12])
                CHECK(context.depth == 1 && context.source_pc == 8 &&
                      strcmp(context.frames[0].method, "b") == 0);
            else if (null_cache && values[5] < 2)
                CHECK(context.depth == 2 && context.source_pc == 13 && context.frames[0].pc == 14 &&
                      strcmp(context.frames[1].class_name, "uu") == 0 &&
                      strcmp(context.frames[1].method, "d") == 0);
            else
                CHECK(context.depth == 1 && context.source_pc == 14);
            ++failed;
        }
        CHECK(crops_cv_same(&fields, &entered_cv) &&
              crops_block_same(&block->fields, &entered_block) &&
              memcmp(entered_emissions, emissions, sizeof(entered_emissions)) == 0 &&
              f->base.registry->blocks.s == emissions &&
              mc_client_nw_inspect(f->graphics, &after) && after.fields.g == before.fields.g &&
              after.fields.d == before.fields.d && after.fields.e == before.fields.e &&
              after.fields.f == before.fields.f && crops_buffers_same(&after.fields, &buffers) &&
              after.fields.o == before.fields.o + (completed ? 384 : 0) &&
              after.fields.h == before.fields.h + (completed ? 48 : 0) &&
              after.fields.p == before.fields.p + (completed ? 32 : 0));
        if (!completed)
            CHECK(crops_nw_same(&before.fields, &after.fields));
        CHECK(memcmp(prior_words, after.fields.g, (size_t)before.fields.o * sizeof(*prior_words)) ==
                  0 &&
              memcmp(prior_words + after.fields.o, after.fields.g + after.fields.o,
                     (capacity - (size_t)after.fields.o) * sizeof(*prior_words)) == 0 &&
              f->base.world->persistent_bindings == bindings + 1u);
        own_unchanged_words += capacity - (size_t)(after.fields.o - before.fields.o);
        if (completed) {
            ++visible;
            if (values[8] == 1) {
                size_t slot = values[0] == 0 ? 0u : values[0] == 7 ? 1u : values[0] == 8 ? 2u : 3u;
                memcpy(section_words[slot], after.fields.g + before.fields.o,
                       sizeof(section_words[0]));
            }
        }
        printf("CROPS_INPUT\t%u\t59\t88\t%d\t%" PRId32 "\t0\t%" PRId32 "\t", index, values[3],
               entered_block.bm, emissions[59]);
        for (size_t i = 0; i < 13; ++i)
            printf("%s%d", i == 0 ? "" : ",", values[i]);
        printf("\t%" PRId32 ",%" PRId32 ",%" PRId32 "\t0\n", xyz[0], xyz[1], xyz[2]);
        printf("CROPS\t%u\t%s\t%d\t%s\t%s\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\t%d\t%016" PRIx64
               ",%016" PRIx64 ",%016" PRIx64 "\t",
               index, name, values[0], completed ? (values[12] ? "VOID" : "true") : "UNRETURNED",
               completed ? "NONE" : "NULL", before.fields.o, after.fields.o, after.fields.B,
               f->base.world_classes->chunk_a, crops_double_bits(after.fields.s),
               crops_double_bits(after.fields.t), crops_double_bits(after.fields.u));
        crops_bounds(&entered_block);
        putchar('\t');
        crops_cv_fields(&entered_cv);
        putchar('\t');
        crops_nw_fields(&before.fields);
        putchar('\t');
        crops_nw_fields(&after.fields);
        putchar('\t');
        for (int32_t i = before.fields.o; i < before.fields.o + 384; ++i)
            printf("%s%08" PRIx32, i == before.fields.o ? "" : ",", (uint32_t)after.fields.g[i]);
        CHECK(putchar('\n') != EOF && fflush(stdout) == 0);
        block->fields = original_block;
        emissions[59] = 0;
        ++index;
    }
    CHECK(scanned == EOF && !ferror(input) && fclose(input) == 0 && index == 44 && visible == 41 &&
          failed == 3 && own_unchanged_words == 92258944 &&
          mc_client_nw_inspect(f->graphics, &initial) && initial.fields.o == 15744);
    CHECK(crops_dispatch(f, cache, block, section_words));
    free(prior_words);
    CHECK(mc_client_nw_translation(nw, 0, 0, 0, &graphics) &&
          mc_client_chunk_cache_retire(&cache, &cache_error) == MC_CLIENT_CHUNK_CACHE_OK &&
          cache == NULL && f->base.world->persistent_bindings == bindings);
    printf("CROPS_NATIVE own-unchanged-words=%zu dispatch=4 fp-admission=2 cleanup=1\n",
           own_unchanged_words);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4)
        return 2;
    picking_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !picking_open(f, argv[1]) || !picking_case_open(f, argv[2]) ||
        !crops_controls(f, argv[3]) || !picking_case_close(f) || !picking_close(f))
        return 1;
    free(f);
    printf(
        "Source Crops world: %u checks; 44 original calls, fields, bounds, NW words and cleanup\n",
        checks);
    return 0;
}
