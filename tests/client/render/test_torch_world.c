#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

/* Genuine saved World, Registry and Chunk owners; earlier mains never run. */
#define MC_PICKING_BODY_MAIN_NAME mc_torch_picking_fixture_main
int mc_torch_picking_fixture_main(int argc, char **argv);
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

static uint32_t torch_float_bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static uint64_t torch_double_bits(double value) {
    uint64_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static const char *torch_bool(bool value) {
    return value ? "true" : "false";
}

/* Compare source fields and actual reference identities, not C padding. */
static bool torch_cv_same(const mc_client_render_blocks *left,
                          const mc_client_render_blocks *right) {
    return left->c == right->c && left->d == right->d && left->e == right->e &&
           left->f == right->f && left->b == right->b && left->g == right->g &&
           left->h == right->h && left->i == right->i && left->j == right->j &&
           left->k == right->k && left->l == right->l && left->m == right->m &&
           left->O == right->O && memcmp(left->n_to_N, right->n_to_N, sizeof(left->n_to_N)) == 0 &&
           memcmp(left->P_to_aa, right->P_to_aa, sizeof(left->P_to_aa)) == 0 &&
           memcmp(left->ab_to_am, right->ab_to_am, sizeof(left->ab_to_am)) == 0;
}

static bool torch_block_same(const mc_block_fields *left, const mc_block_fields *right) {
    return left->bm == right->bm && left->bn == right->bn &&
           torch_float_bits(left->bo) == torch_float_bits(right->bo) &&
           torch_float_bits(left->bp) == torch_float_bits(right->bp) && left->bq == right->bq &&
           left->br == right->br && torch_double_bits(left->bs) == torch_double_bits(right->bs) &&
           torch_double_bits(left->bt) == torch_double_bits(right->bt) &&
           torch_double_bits(left->bu) == torch_double_bits(right->bu) &&
           torch_double_bits(left->bv) == torch_double_bits(right->bv) &&
           torch_double_bits(left->bw) == torch_double_bits(right->bw) &&
           torch_double_bits(left->bx) == torch_double_bits(right->bx) && left->by == right->by &&
           torch_float_bits(left->bz) == torch_float_bits(right->bz) && left->bA == right->bA &&
           torch_float_bits(left->bB) == torch_float_bits(right->bB) && left->a == right->a;
}

static bool torch_nw_same(const mc_client_nw_fields *left, const mc_client_nw_fields *right) {
    return left->d == right->d && left->e == right->e && left->f == right->f &&
           left->g == right->g && left->h == right->h &&
           torch_double_bits(left->i) == torch_double_bits(right->i) &&
           torch_double_bits(left->j) == torch_double_bits(right->j) && left->k == right->k &&
           left->l == right->l && left->m == right->m && left->n == right->n &&
           left->o == right->o && left->p == right->p && left->q == right->q &&
           left->r == right->r && torch_double_bits(left->s) == torch_double_bits(right->s) &&
           torch_double_bits(left->t) == torch_double_bits(right->t) &&
           torch_double_bits(left->u) == torch_double_bits(right->u) && left->v == right->v &&
           left->w == right->w && left->x == right->x && left->y == right->y &&
           left->z == right->z && left->A == right->A && left->B == right->B;
}

static void torch_cv_fields(const mc_client_render_blocks *fields) {
    for (size_t i = 13; i < 27; ++i)
        printf("%s%c=%08" PRIx32, i == 13 ? "" : ",", (int)('A' + i - 13),
               torch_float_bits(fields->n_to_N[i]));
    printf(",O=%" PRId32, fields->O);
    for (size_t i = 0; i < 11; ++i)
        printf(",%c=%08" PRIx32, (int)('P' + i), torch_float_bits(fields->P_to_aa[i]));
    printf(",aa=%08" PRIx32, torch_float_bits(fields->P_to_aa[11]));
    for (size_t i = 0; i < 12; ++i)
        printf(",a%c=%s", (int)('b' + i), torch_bool(fields->ab_to_am[i]));
    printf(",b=%s,d=%" PRId32 ",e=%s,f=%s,g=%" PRId32 ",h=%" PRId32 ",i=%" PRId32 ",j=%" PRId32
           ",k=%" PRId32 ",l=%" PRId32 ",m=%s",
           torch_bool(fields->b), fields->d, torch_bool(fields->e), torch_bool(fields->f),
           fields->g, fields->h, fields->i, fields->j, fields->k, fields->l, torch_bool(fields->m));
    for (size_t i = 0; i < 13; ++i)
        printf(",%c=%08" PRIx32, (int)('n' + i), torch_float_bits(fields->n_to_N[i]));
}

static void torch_nw_fields(const mc_client_nw_fields *fields) {
    printf("A=%" PRId32 ",B=%" PRId32 ",h=%" PRId32 ",i=%016" PRIx64 ",j=%016" PRIx64 ",k=%" PRId32
           ",l=%s,m=%s,n=%s,o=%" PRId32 ",p=%" PRId32 ",q=%s,r=%" PRId32 ",s=%016" PRIx64
           ",t=%016" PRIx64 ",u=%016" PRIx64 ",v=%" PRId32 ",w=%s,x=%s,z=%" PRId32,
           fields->A, fields->B, fields->h, torch_double_bits(fields->i),
           torch_double_bits(fields->j), fields->k, torch_bool(fields->l), torch_bool(fields->m),
           torch_bool(fields->n), fields->o, fields->p, torch_bool(fields->q), fields->r,
           torch_double_bits(fields->s), torch_double_bits(fields->t), torch_double_bits(fields->u),
           fields->v, torch_bool(fields->w), torch_bool(fields->x), fields->z);
}

static void torch_bounds(const mc_block_fields *fields) {
    const double box[6] = {fields->bs, fields->bt, fields->bu, fields->bv, fields->bw, fields->bx};
    for (size_t i = 0; i < 6; ++i)
        printf("%s%016" PRIx64, i == 0 ? "" : ",", torch_double_bits(box[i]));
}

typedef struct torch_buffer_snapshot {
    mc_client_direct_byte_buffer bytes;
    mc_client_direct_int_buffer ints;
    mc_client_direct_float_buffer floats;
} torch_buffer_snapshot;

static bool torch_buffers_capture(const mc_client_nw_fields *fields, torch_buffer_snapshot *out) {
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

static bool torch_buffers_same(const mc_client_nw_fields *fields,
                               const torch_buffer_snapshot *before) {
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

static bool torch_prepare(mc_chunk_state *chunk, const int values[13]) {
    memset(chunk->data->blocks, 0, MC_CHUNK_BLOCKS);
    memset(chunk->data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(chunk->data->skylight, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(chunk->data->block_light, 0, MC_CHUNK_NIBBLE_BYTES);
    for (int32_t x = 7; x <= 9; ++x)
        for (int32_t y = 63; y <= 65; ++y)
            for (int32_t z = 7; z <= 9; ++z) {
                CHECK(mc_chunk_nibbles_set(chunk->data->skylight, MC_CHUNK_NIBBLE_BYTES, x, y, z,
                                           values[3] == 1 ? 15 : 0));
                CHECK(mc_chunk_nibbles_set(chunk->data->block_light, MC_CHUNK_NIBBLE_BYTES, x, y, z,
                                           values[3] == 2 ? 15 : 0));
            }
    chunk->data->blocks[(8u << 11u) | (8u << 7u) | 64u] = (uint8_t)values[1];
    CHECK(mc_chunk_nibbles_set(chunk->data->metadata, MC_CHUNK_NIBBLE_BYTES, 8, 64, 8, values[2]));
    return true;
}

static bool torch_set_bounds(mc_block *block, int index) {
    static const float boxes[3][6] = {{0, 0, 0, 1, 1, 1},
                                      {.125f, .25f, .375f, .875f, .75f, .625f},
                                      {-.25f, -.5f, -.125f, 1.5f, 1.25f, 1.75f}};
    CHECK(index >= 0 && index < 3);
    const float *box = boxes[index];
    mc_registry_block_bounds(block, box[0], box[1], box[2], box[3], box[4], box[5]);
    return true;
}

static bool torch_failure(const mc_cv_context *context, const int values[13],
                          const mc_client_tessellator *tessellator) {
    const bool wrapper = values[0] == 0;
    const unsigned int pc = wrapper ? (values[7] == 1 || values[7] == 4 ? 8u : 29u) : 7u;
    CHECK(context->failed && context->failure.status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
          !context->failure.native_policy && context->last_store_pc == 0 && context->depth == 1 &&
          context->source_pc == pc && context->frames[0].pc == pc &&
          strcmp(context->frames[0].class_name, "cv") == 0 &&
          strcmp(context->frames[0].method, wrapper ? "c" : "a") == 0 &&
          strcmp(context->frames[0].descriptor, wrapper ? "(Luu;III)Z" : "(Luu;DDDDD)V") == 0 &&
          context->failure.source_class != NULL && context->failure.source_method != NULL &&
          context->failure.source_descriptor != NULL &&
          strcmp(context->failure.source_class, "cv") == 0 &&
          strcmp(context->failure.source_method, wrapper ? "c" : "a") == 0 &&
          strcmp(context->failure.source_descriptor, wrapper ? "(Luu;III)Z" : "(Luu;DDDDD)V") ==
              0 &&
          context->failure.source_pc == pc && context->failure.child.class_name == NULL);
    CHECK(context->tessellator ==
          (wrapper && (values[7] == 1 || values[7] == 4) ? NULL : tessellator));
    return true;
}

/* Native admission is separate from the 66 original calls. These failures
 * precede every append and preserve the output sentinel and the begun NW. */
static bool torch_fp_controls(picking_fixture *fixture_state, mc_client_chunk_cache *cache,
                              const mc_block *block, mc_client_tessellator *tessellator) {
    mc_client_render_blocks fields = {0};
    mc_client_render_blocks_construct_fields(&fields, cache, true);
    const mc_client_render_blocks prior = fields;
    mc_client_nw_state before, after;
    CHECK(mc_client_nw_inspect(fixture_state->graphics, &before));
    const unsigned int csr = _mm_getcsr();
    for (unsigned int primitive = 0; primitive < 2; ++primitive) {
        mc_cv_context context = {.fields = &fields,
                                 .cache = cache,
                                 .registry = fixture_state->base.registry,
                                 .graphics = fixture_state->graphics};
        if (!primitive)
            CHECK(mc_cv_push(&context, "c", "(Luu;III)Z"));
        bool result = true;
        _mm_setcsr(csr | 0x8040u);
        const bool completed = primitive ? mc_cv_torch_geometry(&context, block, 8, 64, 8, 0, 0)
                                         : mc_cv_torch(&context, block, 8, 64, 8, &result);
        _mm_setcsr(csr);
        CHECK(!completed && result && context.failed && context.depth == 1 &&
              context.source_pc == (primitive ? 47u : 56u) &&
              context.frames[0].pc == context.source_pc && context.last_store_pc == 0 &&
              context.failure.status == MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP &&
              context.failure.native_policy && context.tessellator == tessellator &&
              torch_cv_same(&fields, &prior));
        if (!primitive)
            CHECK(context.failure.child.class_name != NULL &&
                  strcmp(context.failure.child.class_name, "nw") == 0 &&
                  strcmp(context.failure.child.method, "a") == 0 &&
                  strcmp(context.failure.child.descriptor, "(FFF)V") == 0 &&
                  context.failure.child.pc == 2);
        CHECK(mc_client_nw_inspect(fixture_state->graphics, &after) &&
              torch_nw_same(&before.fields, &after.fields));
    }
    return true;
}

static bool torch_controls(picking_fixture *fixture_state, const char *profiles) {
    mc_client_chunk_cache *cache = NULL;
    mc_client_chunk_cache_error cache_error;
    const size_t bindings = fixture_state->base.world->persistent_bindings;
    CHECK(mc_client_chunk_cache_create(fixture_state->base.world, 0, 0, 0, 31, 127, 31, &cache,
                                       &cache_error) == MC_CLIENT_CHUNK_CACHE_OK);
    mc_client_model_error model_error;
    CHECK(mc_client_render_blocks_active_use(fixture_state->models, &model_error));
    mc_client_tessellator *tessellator = NULL;
    mc_client_graphics_error graphics;
    CHECK(mc_client_nw_active_use(fixture_state->graphics,
                                  (mc_client_graphics_site){"fixture", "torch", "()V", 0},
                                  &tessellator, &graphics) &&
          mc_client_nw_begin_quads(tessellator, &graphics) &&
          mc_client_nw_translation(tessellator, -.25, .5, -.75, &graphics));
    fixture_state->base.world->fields.f = 3;
    mc_client_nw_state initial;
    CHECK(mc_client_nw_inspect(fixture_state->graphics, &initial) && initial.static_b &&
          !initial.static_c && initial.fields.B == 2097152 && initial.fields.o == 0 &&
          initial.fields.g != NULL);
    const size_t capacity = (size_t)initial.fields.B;
    int32_t *prior_words = malloc(capacity * sizeof(*prior_words));
    CHECK(prior_words != NULL);
    int32_t *const emissions = fixture_state->base.registry->blocks.s;
    CHECK(emissions != NULL);
    FILE *input = fopen(profiles, "rb");
    CHECK(input != NULL);
    char label[128];
    int values[13], scanned;
    unsigned int profile_index = 0, wrapper_returns = 0, primitive_returns = 0, nulls = 0;
    size_t own_unchanged_words = 0;
    while (
        (scanned = fscanf(input, "%127s%d%d%d%d%d%d%d%d%d%d%d%d%d", label, &values[0], &values[1],
                          &values[2], &values[3], &values[4], &values[5], &values[6], &values[7],
                          &values[8], &values[9], &values[10], &values[11], &values[12])) == 14) {
        CHECK(values[0] >= 0 && values[0] <= 1 &&
              (values[1] == 50 || values[1] == 75 || values[1] == 76) && values[2] >= 0 &&
              values[2] <= 15 && values[3] >= 0 && values[3] <= 2 && values[7] >= 0 &&
              values[7] <= 4 && values[8] >= 0 && values[8] < 3 && values[10] >= 0 &&
              values[10] <= 1 && values[11] >= 0 && values[11] <= 1 && values[12] >= 0 &&
              values[12] < 4);
        const mc_block *view = NULL;
        CHECK(mc_registry_block_get(fixture_state->base.registry, values[1], &view, NULL) ==
                  MC_REGISTRY_OK &&
              view != NULL && view->fields.bn == values[1] &&
              view->kind == (values[1] == 50 ? MC_BLOCK_TORCH : MC_BLOCK_REDSTONE_TORCH));
        mc_block *block = (mc_block *)(void *)view;
        const int32_t original_bm = block->fields.bm;
        const int32_t original_emission = emissions[block->fields.bn];
        mc_material *const original_material = block->fields.bA;
        block->fields.bm = mc_java_i32_add(original_bm, values[5]);
        if (values[4] != INT_MIN)
            emissions[block->fields.bn] = values[4];
        if (values[7] == 3)
            block->fields.bA = NULL;
        CHECK(torch_prepare(fixture_state->chunks[0], values) &&
              torch_set_bounds(block, values[8]));
        const mc_block_fields entered_block = block->fields;
        int32_t entered_emissions[256];
        memcpy(entered_emissions, emissions, sizeof(entered_emissions));
        mc_client_render_blocks fields = {0};
        mc_client_render_blocks_construct_fields(
            &fields, values[7] == 1 || values[7] == 4 ? NULL : cache, true);
        fields.m = values[11] != 0;
        fields.f = (profile_index & 1u) != 0;
        fields.d = values[6];
        fields.e = values[10] != 0;
        fields.g = fields.h = fields.i = fields.j = fields.k = fields.l = values[9];
        for (size_t i = 0; i < 27; ++i)
            fields.n_to_N[i] = (float)(i + 1) * .03125f;
        for (size_t i = 0; i < 12; ++i) {
            fields.P_to_aa[i] = (float)(i + 1) * .015625f;
            fields.ab_to_am[i] = (i & 1u) != 0;
        }
        const mc_client_render_blocks entered_cv = fields;
        mc_client_nw_state before, after;
        torch_buffer_snapshot buffers;
        CHECK(mc_client_nw_inspect(fixture_state->graphics, &before) &&
              before.fields.g == initial.fields.g &&
              torch_buffers_capture(&before.fields, &buffers));
        memcpy(prior_words, before.fields.g, capacity * sizeof(*prior_words));
        fixture_state->base.world_classes->chunk_a = false;
        /* Leave the captured NW unset until the real reached mc_cv_nw call. */
        mc_cv_context context = {.fields = &fields,
                                 .cache = cache,
                                 .registry = fixture_state->base.registry,
                                 .models = fixture_state->models,
                                 .graphics = fixture_state->graphics,
                                 .application = fixture_state->app};
        if (values[0] == 0)
            CHECK(mc_cv_push(&context, "c", "(Luu;III)Z"));
        static const double geometry[4][5] = {{8, 64, 8, 0, 0},
                                              {-.5, -.5, -.5, 0, 0},
                                              {-0.0, -0.0, -0.0, -0.0, 0},
                                              {1.25, -2.5, 3.75, (double).4f, -(double).4f}};
        const double *coordinates = geometry[values[12]];
        const mc_block *operand = values[7] == 2 || values[7] == 4 ? NULL : block;
        const bool expected_null = values[0] == 0
                                       ? values[7] == 1 || values[7] == 2 || values[7] == 4
                                       : values[7] == 2 || values[7] == 4;
        /* A failed wrapper must not overwrite its true sentinel. Successful
         * wrappers start false so the reached return store is also proven.
         * The primitive has no result operand and preserves either sentinel. */
        bool result = expected_null;
        const bool completed =
            values[0] == 0 ? mc_cv_torch(&context, operand, 8, 64, 8, &result)
                           : mc_cv_torch_geometry(&context, operand, coordinates[0], coordinates[1],
                                                  coordinates[2], coordinates[3], coordinates[4]);
        CHECK(completed != expected_null);
        if (completed) {
            CHECK(!context.failed && context.last_store_pc == 0 &&
                  context.tessellator == tessellator &&
                  context.depth == (values[0] == 0 ? 1u : 0u) &&
                  context.source_pc == (values[0] == 0 ? 214u : 718u));
            if (values[0] == 0) {
                CHECK(result);
                ++wrapper_returns;
            } else {
                CHECK(!result);
                ++primitive_returns;
            }
        } else {
            CHECK(result && torch_failure(&context, values, tessellator));
            ++nulls;
        }
        CHECK(torch_cv_same(&entered_cv, &fields) &&
              torch_block_same(&entered_block, &block->fields) &&
              fixture_state->base.registry->blocks.s == emissions &&
              memcmp(entered_emissions, emissions, sizeof(entered_emissions)) == 0 &&
              mc_client_nw_inspect(fixture_state->graphics, &after));
        CHECK(after.fields.d == before.fields.d && after.fields.e == before.fields.e &&
              after.fields.f == before.fields.f && after.fields.g == before.fields.g &&
              after.fields.y == before.fields.y && after.fields.B == before.fields.B &&
              torch_buffers_same(&after.fields, &buffers) && after.fields.w &&
              after.fields.r == 7 && after.fields.o == before.fields.o + (completed ? 240 : 0) &&
              after.fields.h == before.fields.h + (completed ? 30 : 0) &&
              after.fields.p == before.fields.p + (completed ? 20 : 0) && after.fields.o >= 0 &&
              (size_t)after.fields.o <= capacity &&
              torch_double_bits(after.fields.s) == torch_double_bits(before.fields.s) &&
              torch_double_bits(after.fields.t) == torch_double_bits(before.fields.t) &&
              torch_double_bits(after.fields.u) == torch_double_bits(before.fields.u) &&
              fixture_state->base.world->persistent_bindings == bindings + 1 &&
              fixture_state->base.world_classes->chunk_a ==
                  (completed && values[0] == 0 && values[3] == 1));
        if (!completed)
            CHECK(torch_nw_same(&before.fields, &after.fields));
        CHECK(memcmp(prior_words, after.fields.g, (size_t)before.fields.o * sizeof(*prior_words)) ==
                  0 &&
              memcmp(prior_words + after.fields.o, after.fields.g + after.fields.o,
                     (capacity - (size_t)after.fields.o) * sizeof(*prior_words)) == 0);
        own_unchanged_words += capacity - (size_t)(after.fields.o - before.fields.o);
        printf("TORCH_INPUT\t%u\t%s\t%" PRId32 "\t%" PRId32 "\t%d\t%" PRId32 "\t%" PRId32
               "\t%" PRId32 "\t%d\t%d\n",
               profile_index, values[1] == 50 ? "vm" : "db", block->fields.bn, original_bm,
               values[5], entered_block.bm, original_emission, entered_emissions[block->fields.bn],
               values[2], values[3]);
        printf("TORCH\t%u\t%s\t%d\t%s\t%s\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\t%d\t%016" PRIx64
               ",%016" PRIx64 ",%016" PRIx64 "\t",
               profile_index, label, values[0],
               completed ? (values[0] == 0 ? "true" : "VOID") : "UNRETURNED",
               completed ? "NONE" : "NULL", before.fields.o, after.fields.o, after.fields.B,
               fixture_state->base.world_classes->chunk_a, torch_double_bits(after.fields.s),
               torch_double_bits(after.fields.t), torch_double_bits(after.fields.u));
        torch_bounds(&entered_block);
        putchar('\t');
        torch_cv_fields(&entered_cv);
        putchar('\t');
        torch_nw_fields(&before.fields);
        putchar('\t');
        torch_nw_fields(&after.fields);
        putchar('\t');
        const int32_t extent =
            after.fields.o > before.fields.o + 100 ? after.fields.o : before.fields.o + 100;
        CHECK((size_t)extent <= capacity);
        for (int32_t i = before.fields.o; i < extent; ++i)
            printf("%s%08" PRIx32, i == before.fields.o ? "" : ",", (uint32_t)after.fields.g[i]);
        CHECK(putchar('\n') != EOF && fflush(stdout) == 0);
        emissions[block->fields.bn] = original_emission;
        block->fields.bm = original_bm;
        block->fields.bA = original_material;
        CHECK(fixture_state->base.registry->blocks.s == emissions &&
              block->fields.bm == original_bm && block->fields.bA == original_material &&
              emissions[block->fields.bn] == original_emission);
        ++profile_index;
    }
    CHECK(scanned == EOF && !ferror(input) && fclose(input) == 0 && profile_index == 66 &&
          wrapper_returns == 57 && primitive_returns == 4 && nulls == 5 &&
          own_unchanged_words == (size_t)138397392);
    mc_client_nw_state final;
    CHECK(mc_client_nw_inspect(fixture_state->graphics, &final) && final.fields.o == 14640 &&
          final.fields.h == 1830 && final.fields.p == 1220 && final.fields.w &&
          final.fields.g == initial.fields.g && final.fields.B == initial.fields.B);
    const mc_block *block;
    CHECK(mc_registry_block_get(fixture_state->base.registry, 50, &block, NULL) == MC_REGISTRY_OK &&
          block != NULL && torch_fp_controls(fixture_state, cache, block, tessellator));
    free(prior_words);
    CHECK(mc_client_nw_translation(tessellator, 0, 0, 0, &graphics) &&
          mc_client_chunk_cache_retire(&cache, &cache_error) == MC_CLIENT_CHUNK_CACHE_OK &&
          cache == NULL && fixture_state->base.world->persistent_bindings == bindings);
    printf("TORCH_NATIVE own-unchanged-words=%zu fp-admission=2 cleanup=1\n", own_unchanged_words);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4)
        return 2;
    picking_fixture *fixture_state = calloc(1, sizeof(*fixture_state));
    if (fixture_state == NULL || !picking_open(fixture_state, argv[1]) ||
        !picking_case_open(fixture_state, argv[2]) || !torch_controls(fixture_state, argv[3]) ||
        !picking_case_close(fixture_state) || !picking_close(fixture_state))
        return 1;
    free(fixture_state);
    printf(
        "Source Torch world: %u checks; 66 original calls, fields, bounds, NW words and cleanup\n",
        checks);
    return 0;
}
