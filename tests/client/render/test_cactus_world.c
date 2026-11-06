#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

/* Genuine saved World, Registry and Chunk owners; earlier mains never run. */
#define MC_PICKING_BODY_MAIN_NAME mc_cactus_picking_fixture_main
int mc_cactus_picking_fixture_main(int argc, char **argv);
#include "../world/test_picking.c"
#undef main

#include "block/registry/registry_internal.h"
#include "client/render/blocks/internal.h"
#include "client/render/model_graph_internal.h"
#include "client/render_engine/nw.h"
#include "mc/core/java/java.h"
#include "world/chunk/object_state_internal.h"

#include <xmmintrin.h>

static uint32_t cactus_float_bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static uint64_t cactus_double_bits(double value) {
    uint64_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static const char *cactus_bool(bool value) {
    return value ? "true" : "false";
}

/* Compare source fields and actual reference identities, not C padding. */
static bool cactus_cv_same(const mc_client_render_blocks *left,
                           const mc_client_render_blocks *right) {
    return left->c == right->c && left->d == right->d && left->e == right->e &&
           left->f == right->f && left->b == right->b && left->g == right->g &&
           left->h == right->h && left->i == right->i && left->j == right->j &&
           left->k == right->k && left->l == right->l && left->m == right->m &&
           left->O == right->O && memcmp(left->n_to_N, right->n_to_N, sizeof(left->n_to_N)) == 0 &&
           memcmp(left->P_to_aa, right->P_to_aa, sizeof(left->P_to_aa)) == 0 &&
           memcmp(left->ab_to_am, right->ab_to_am, sizeof(left->ab_to_am)) == 0;
}

static bool cactus_block_same(const mc_block_fields *left, const mc_block_fields *right) {
    return left->bm == right->bm && left->bn == right->bn &&
           cactus_float_bits(left->bo) == cactus_float_bits(right->bo) &&
           cactus_float_bits(left->bp) == cactus_float_bits(right->bp) && left->bq == right->bq &&
           left->br == right->br && cactus_double_bits(left->bs) == cactus_double_bits(right->bs) &&
           cactus_double_bits(left->bt) == cactus_double_bits(right->bt) &&
           cactus_double_bits(left->bu) == cactus_double_bits(right->bu) &&
           cactus_double_bits(left->bv) == cactus_double_bits(right->bv) &&
           cactus_double_bits(left->bw) == cactus_double_bits(right->bw) &&
           cactus_double_bits(left->bx) == cactus_double_bits(right->bx) && left->by == right->by &&
           cactus_float_bits(left->bz) == cactus_float_bits(right->bz) && left->bA == right->bA &&
           cactus_float_bits(left->bB) == cactus_float_bits(right->bB) && left->a == right->a;
}

static bool cactus_nw_same(const mc_client_nw_fields *left, const mc_client_nw_fields *right) {
    return left->d == right->d && left->e == right->e && left->f == right->f &&
           left->g == right->g && left->h == right->h &&
           cactus_double_bits(left->i) == cactus_double_bits(right->i) &&
           cactus_double_bits(left->j) == cactus_double_bits(right->j) && left->k == right->k &&
           left->l == right->l && left->m == right->m && left->n == right->n &&
           left->o == right->o && left->p == right->p && left->q == right->q &&
           left->r == right->r && cactus_double_bits(left->s) == cactus_double_bits(right->s) &&
           cactus_double_bits(left->t) == cactus_double_bits(right->t) &&
           cactus_double_bits(left->u) == cactus_double_bits(right->u) && left->v == right->v &&
           left->w == right->w && left->x == right->x && left->y == right->y &&
           left->z == right->z && left->A == right->A && left->B == right->B;
}

static void cactus_cv_fields(const mc_client_render_blocks *fields) {
    for (size_t i = 13; i < 27; ++i)
        printf("%s%c=%08" PRIx32, i == 13 ? "" : ",", (int)('A' + i - 13),
               cactus_float_bits(fields->n_to_N[i]));
    printf(",O=%" PRId32, fields->O);
    for (size_t i = 0; i < 11; ++i)
        printf(",%c=%08" PRIx32, (int)('P' + i), cactus_float_bits(fields->P_to_aa[i]));
    printf(",aa=%08" PRIx32, cactus_float_bits(fields->P_to_aa[11]));
    for (size_t i = 0; i < 12; ++i)
        printf(",a%c=%s", (int)('b' + i), cactus_bool(fields->ab_to_am[i]));
    printf(",b=%s,d=%" PRId32 ",e=%s,f=%s,g=%" PRId32 ",h=%" PRId32 ",i=%" PRId32 ",j=%" PRId32
           ",k=%" PRId32 ",l=%" PRId32 ",m=%s",
           cactus_bool(fields->b), fields->d, cactus_bool(fields->e), cactus_bool(fields->f),
           fields->g, fields->h, fields->i, fields->j, fields->k, fields->l,
           cactus_bool(fields->m));
    for (size_t i = 0; i < 13; ++i)
        printf(",%c=%08" PRIx32, (int)('n' + i), cactus_float_bits(fields->n_to_N[i]));
}

static void cactus_nw_fields(const mc_client_nw_fields *fields) {
    printf("A=%" PRId32 ",B=%" PRId32 ",h=%" PRId32 ",i=%016" PRIx64 ",j=%016" PRIx64 ",k=%" PRId32
           ",l=%s,m=%s,n=%s,o=%" PRId32 ",p=%" PRId32 ",q=%s,r=%" PRId32 ",s=%016" PRIx64
           ",t=%016" PRIx64 ",u=%016" PRIx64 ",v=%" PRId32 ",w=%s,x=%s,z=%" PRId32,
           fields->A, fields->B, fields->h, cactus_double_bits(fields->i),
           cactus_double_bits(fields->j), fields->k, cactus_bool(fields->l), cactus_bool(fields->m),
           cactus_bool(fields->n), fields->o, fields->p, cactus_bool(fields->q), fields->r,
           cactus_double_bits(fields->s), cactus_double_bits(fields->t),
           cactus_double_bits(fields->u), fields->v, cactus_bool(fields->w), cactus_bool(fields->x),
           fields->z);
}

static void cactus_bounds(const mc_block_fields *fields) {
    const double box[6] = {fields->bs, fields->bt, fields->bu, fields->bv, fields->bw, fields->bx};
    for (size_t i = 0; i < 6; ++i)
        printf("%s%016" PRIx64, i == 0 ? "" : ",", cactus_double_bits(box[i]));
}

static bool cactus_prepare(mc_chunk_state *chunk, int pattern) {
    memset(chunk->data->blocks, 0, MC_CHUNK_BLOCKS);
    memset(chunk->data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(chunk->data->skylight, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(chunk->data->block_light, 0, MC_CHUNK_NIBBLE_BYTES);
    for (int32_t x = 7; x <= 9; ++x)
        for (int32_t y = 63; y <= 65; ++y)
            for (int32_t z = 7; z <= 9; ++z) {
                const size_t offset = ((uint32_t)x << 11u) | ((uint32_t)z << 7u) | (uint32_t)y;
                chunk->data->blocks[offset] =
                    (uint8_t)(pattern == 0   ? 0
                              : pattern == 1 ? 1
                                             : ((x + 2 * y + 4 * z) & 2) != 0);
                CHECK(mc_chunk_nibbles_set(chunk->data->metadata, MC_CHUNK_NIBBLE_BYTES, x, y, z,
                                           (x + y + z) & 3));
                CHECK(mc_chunk_nibbles_set(chunk->data->skylight, MC_CHUNK_NIBBLE_BYTES, x, y, z,
                                           (x * 3 + y * 5 + z * 7) & 15));
                CHECK(mc_chunk_nibbles_set(chunk->data->block_light, MC_CHUNK_NIBBLE_BYTES, x, y, z,
                                           (x * 7 + y * 11 + z * 13) & 15));
            }
    chunk->data->blocks[(8u << 11u) | (8u << 7u) | 64u] = 81;
    return true;
}

static bool cactus_set_bounds(mc_block *block_receiver, int box_index) {
    static const float ordinary[4][6] = {{0, 0, 0, 1, 1, 1},
                                         {.125f, .25f, .375f, .875f, .75f, .625f},
                                         {-.25f, -.5f, -.125f, 1.5f, 1.25f, 1.75f},
                                         {0, 0, 0, 1, .5f, 1}};
    float box[6];
    CHECK(box_index >= 0 && box_index <= 10);
    memcpy(box, ordinary[box_index < 4 ? box_index : 0], sizeof(box));
    if (box_index >= 4 && box_index <= 9) {
        const uint32_t nan_bits = UINT32_C(0x7fc00000);
        memcpy(&box[box_index - 4], &nan_bits, sizeof(nan_bits));
    } else if (box_index == 10) {
        box[0] = box[1] = box[2] = -0.0f;
    }
    mc_registry_block_bounds(block_receiver, box[0], box[1], box[2], box[3], box[4], box[5]);
    return true;
}

static bool cactus_failure(const mc_cv_context *context, const int case_values[13]) {
    const bool wrapper = case_values[0] == 1;
    CHECK(context->failed && context->failure.status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
          !context->failure.native_policy && context->last_store_pc == 0);
    if (case_values[5] == 1) {
        CHECK(context->depth == (wrapper ? 3u : 2u) && context->source_pc == 13 &&
              context->frames[context->depth - 1].pc == 13 &&
              strcmp(context->frames[context->depth - 1].class_name, "uu") == 0 &&
              strcmp(context->frames[context->depth - 1].method, "d") == 0 &&
              context->frames[context->depth - 2].pc == 120);
        if (wrapper)
            CHECK(context->frames[0].pc == 140);
    } else {
        CHECK(context->depth == 1 && context->source_pc == (case_values[5] == 3 ? 268u
                                                            : wrapper           ? 9u
                                                                                : 120u));
    }
    return true;
}

static bool cactus_fp_controls(picking_fixture *fixture_state, mc_client_chunk_cache *cache,
                               const mc_block *block_receiver, mc_client_tessellator *tessellator) {
    mc_client_render_blocks fields = {0};
    mc_client_render_blocks_construct_fields(&fields, cache, true);
    fields.m = true;
    const mc_client_render_blocks prior = fields;
    mc_client_nw_state before, after;
    CHECK(mc_client_nw_inspect(fixture_state->graphics, &before));
    const unsigned int csr = _mm_getcsr();
    for (unsigned int wrapper = 0; wrapper < 2; ++wrapper) {
        mc_cv_context context = {.fields = &fields,
                                 .cache = cache,
                                 .registry = fixture_state->base.registry,
                                 .models = fixture_state->models,
                                 .graphics = fixture_state->graphics,
                                 .tessellator = tessellator};
        CHECK(mc_cv_push(&context, wrapper ? "m" : "c", wrapper ? "(Luu;III)Z" : "(Luu;IIIFFF)Z"));
        bool result = true;
        _mm_setcsr(csr | 0x8040u);
        const bool completed = wrapper ? mc_cv_cactus(&context, block_receiver, 8, 64, 8, &result)
                                       : mc_cv_cactus_geometry(&context, block_receiver, 8, 64, 8,
                                                               .3f, .6f, .9f, &result);
        _mm_setcsr(csr);
        CHECK(!completed && result && context.failed && context.depth == 1 &&
              context.source_pc == (wrapper ? 26u : 27u) && context.last_store_pc == 0 &&
              context.failure.status == MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP &&
              context.failure.native_policy && cactus_cv_same(&fields, &prior));
    }
    CHECK(mc_client_nw_inspect(fixture_state->graphics, &after) &&
          cactus_nw_same(&before.fields, &after.fields));
    return true;
}

static bool cactus_controls(picking_fixture *fixture_state, const char *profiles) {
    mc_client_chunk_cache *cache = NULL;
    mc_client_chunk_cache_error cache_error;
    const size_t bindings = fixture_state->base.world->persistent_bindings;
    CHECK(mc_client_chunk_cache_create(fixture_state->base.world, 0, 0, 0, 31, 127, 31, &cache,
                                       &cache_error) == MC_CLIENT_CHUNK_CACHE_OK);
    mc_client_model_error model_error;
    CHECK(mc_client_render_blocks_active_use(fixture_state->models, &model_error));
    const bool prior_anaglyph = fixture_state->models->px_a;
    CHECK(mc_client_entity_renderer_class_active_use(fixture_state->models, NULL) ==
          MC_CLIENT_ENTITY_RENDERER_OK);
    mc_client_tessellator *tessellator = NULL;
    mc_client_graphics_error graphics;
    CHECK(mc_client_nw_active_use(fixture_state->graphics,
                                  (mc_client_graphics_site){"fixture", "cactus", "()V", 0},
                                  &tessellator, &graphics) &&
          mc_client_nw_begin_quads(tessellator, &graphics) &&
          mc_client_nw_translation(tessellator, -.25, .5, -.75, &graphics));
    fixture_state->base.world->fields.f = 3;
    const mc_block *view;
    CHECK(mc_registry_block_get(fixture_state->base.registry, 81, &view, NULL) == MC_REGISTRY_OK &&
          view != NULL && view->kind == MC_BLOCK_CACTUS);
    mc_block *block_receiver = (mc_block *)(void *)view;
    const int32_t original_bm = block_receiver->fields.bm;
    mc_client_nw_state initial;
    CHECK(mc_client_nw_inspect(fixture_state->graphics, &initial) && initial.fields.B == 2097152 &&
          initial.fields.o == 0);
    const size_t capacity = (size_t)initial.fields.B;
    int32_t *prior_words = malloc(capacity * sizeof(*prior_words));
    CHECK(prior_words != NULL);
    FILE *input_file = fopen(profiles, "rb");
    CHECK(input_file != NULL);
    char label[128];
    int case_values[13], scanned;
    unsigned int profile_index = 0;
    size_t own_unchanged_words = 0;
    while ((scanned = fscanf(input_file, "%127s%d%d%d%d%d%d%d%d%d%d%d%d%d", label, &case_values[0],
                             &case_values[1], &case_values[2], &case_values[3], &case_values[4],
                             &case_values[5], &case_values[6], &case_values[7], &case_values[8],
                             &case_values[9], &case_values[10], &case_values[11],
                             &case_values[12])) == 14) {
        CHECK(case_values[0] >= 0 && case_values[0] <= 1 && case_values[1] >= 0 &&
              case_values[1] <= 2 && case_values[2] >= 0 && case_values[2] <= 1 &&
              case_values[3] >= 0 && case_values[3] <= 10 && case_values[4] >= 0 &&
              case_values[4] <= 1 && case_values[5] >= 0 && case_values[5] <= 3 &&
              case_values[6] >= 0 && case_values[6] <= 3 && case_values[7] >= 0 &&
              case_values[7] <= 1 && case_values[9] >= 0 && case_values[9] <= 2 &&
              case_values[10] >= 0 && case_values[10] <= 1 && case_values[11] == 0);
        CHECK(cactus_prepare(fixture_state->chunks[0], case_values[1]) &&
              cactus_set_bounds(block_receiver, case_values[3]));
        block_receiver->fields.bm = mc_java_i32_add(original_bm, case_values[12]);
        mc_material *saved_material = block_receiver->fields.bA;
        if (case_values[5] == 3)
            block_receiver->fields.bA = NULL;
        const mc_block_fields entered_block = block_receiver->fields;
        mc_client_render_blocks fields = {0};
        mc_client_render_blocks_construct_fields(&fields, case_values[5] == 1 ? NULL : cache, true);
        fields.m = case_values[4] != 0;
        fields.f = case_values[2] != 0;
        fields.d = case_values[8];
        fields.e = case_values[7] != 0;
        fields.g = fields.h = fields.i = fields.j = fields.k = fields.l = case_values[6];
        for (size_t i = 0; i < 27; ++i)
            fields.n_to_N[i] = (float)(i + 1) * .03125f;
        for (size_t i = 0; i < 12; ++i) {
            fields.P_to_aa[i] = (float)(i + 1) * .015625f;
            fields.ab_to_am[i] = (i & 1u) != 0;
        }
        fixture_state->models->px_a = case_values[10] != 0;
        const mc_client_render_blocks entered_cv = fields;
        mc_client_nw_state before, after;
        CHECK(mc_client_nw_inspect(fixture_state->graphics, &before));
        memcpy(prior_words, before.fields.g, capacity * sizeof(*prior_words));
        fixture_state->base.world_classes->chunk_a = false;
        mc_cv_context context = {.fields = &fields,
                                 .cache = cache,
                                 .registry = fixture_state->base.registry,
                                 .models = fixture_state->models,
                                 .graphics = fixture_state->graphics,
                                 .application = fixture_state->app,
                                 .tessellator = tessellator};
        CHECK(mc_cv_push(&context, case_values[0] == 0 ? "c" : "m",
                         case_values[0] == 0 ? "(Luu;IIIFFF)Z" : "(Luu;III)Z"));
        static const float rgb[3][3] = {{.3f, .6f, .9f}, {-.25f, 1.5f, 2}, {0, 0, 0}};
        const float *channels = rgb[case_values[9]];
        bool result = true;
        const bool completed =
            case_values[0] == 0
                ? mc_cv_cactus_geometry(&context, case_values[5] == 2 ? NULL : block_receiver, 8,
                                        64, 8, channels[0], channels[1], channels[2], &result)
                : mc_cv_cactus(&context, case_values[5] == 2 ? NULL : block_receiver, 8, 64, 8,
                               &result);
        if (completed) {
            CHECK(!context.failed && context.depth == 1 && context.last_store_pc == 0 &&
                  context.source_pc == (case_values[0] == 0 ? 812u : 143u));
        } else {
            CHECK(result && cactus_failure(&context, case_values));
        }
        CHECK(cactus_cv_same(&entered_cv, &fields) &&
              cactus_block_same(&entered_block, &block_receiver->fields) &&
              mc_client_nw_inspect(fixture_state->graphics, &after) &&
              after.fields.g == before.fields.g && after.fields.B == before.fields.B &&
              after.fields.w && after.fields.r == 7 && after.fields.o >= before.fields.o &&
              (size_t)after.fields.o <= capacity && after.fields.s == -.25 &&
              after.fields.t == .5 && after.fields.u == -.75 &&
              fixture_state->base.world->persistent_bindings == bindings + 1);
        CHECK(memcmp(prior_words, after.fields.g, (size_t)before.fields.o * sizeof(*prior_words)) ==
                  0 &&
              memcmp(prior_words + after.fields.o, after.fields.g + after.fields.o,
                     (capacity - (size_t)after.fields.o) * sizeof(*prior_words)) == 0);
        own_unchanged_words += capacity - (size_t)(after.fields.o - before.fields.o);
        printf("CACTUS_INPUT\t%u\t%" PRId32 "\t%d\t%" PRId32 "\n", profile_index, original_bm,
               case_values[12], block_receiver->fields.bm);
        printf("CACTUS\t%u\t%s\t%d\t%d\t%s\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\t%d\t%016" PRIx64
               ",%016" PRIx64 ",%016" PRIx64 "\t",
               profile_index, label, case_values[0], completed ? (result ? 1 : 0) : -1,
               completed ? "NONE" : "NULL", before.fields.o, after.fields.o, after.fields.B,
               fixture_state->base.world_classes->chunk_a, cactus_double_bits(after.fields.s),
               cactus_double_bits(after.fields.t), cactus_double_bits(after.fields.u));
        cactus_bounds(&entered_block);
        putchar('\t');
        cactus_cv_fields(&fields);
        putchar('\t');
        cactus_nw_fields(&before.fields);
        putchar('\t');
        cactus_nw_fields(&after.fields);
        putchar('\t');
        const int32_t extent =
            after.fields.o > before.fields.o + 100 ? after.fields.o : before.fields.o + 100;
        CHECK(extent <= after.fields.B);
        for (int32_t i = before.fields.o; i < extent; ++i)
            printf("%s%08" PRIx32, i == before.fields.o ? "" : ",", (uint32_t)after.fields.g[i]);
        CHECK(putchar('\n') != EOF && fflush(stdout) == 0);
        /* Restore the actual registered Material captured before this profile,
         * rather than supplying a material inferred from the Block's name. */
        block_receiver->fields.bA = saved_material;
        block_receiver->fields.bm = original_bm;
        ++profile_index;
    }
    CHECK(scanned == EOF && !ferror(input_file) && fclose(input_file) == 0 && profile_index == 25);
    free(prior_words);
    block_receiver->fields.bm = original_bm;
    CHECK(cactus_fp_controls(fixture_state, cache, block_receiver, tessellator));
    fixture_state->models->px_a = prior_anaglyph;
    CHECK(mc_client_nw_translation(tessellator, 0, 0, 0, &graphics) &&
          mc_client_chunk_cache_retire(&cache, &cache_error) == MC_CLIENT_CHUNK_CACHE_OK &&
          cache == NULL && fixture_state->base.world->persistent_bindings == bindings);
    printf("CACTUS_NATIVE own-unchanged-words=%zu fp-admission=2 cleanup=1\n", own_unchanged_words);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4)
        return 2;
    picking_fixture *fixture_state = calloc(1, sizeof(*fixture_state));
    if (fixture_state == NULL || !picking_open(fixture_state, argv[1]) ||
        !picking_case_open(fixture_state, argv[2]) || !cactus_controls(fixture_state, argv[3]) ||
        !picking_case_close(fixture_state) || !picking_close(fixture_state))
        return 1;
    free(fixture_state);
    printf(
        "Source Cactus world: %u checks; 25 original calls, fields, bounds, NW words and cleanup\n",
        checks);
    return 0;
}
