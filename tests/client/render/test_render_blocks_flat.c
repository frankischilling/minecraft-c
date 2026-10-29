#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

/* Genuine saved World/Registry/Chunk helpers; earlier main is not executed. */
#define MC_PICKING_BODY_MAIN_NAME mc_flat_picking_fixture_main
int mc_flat_picking_fixture_main(int argc, char **argv);
#include "../world/test_picking.c"
#undef main

#include "block/registry/registry_internal.h"
#include "client/render/blocks/internal.h"
#include "client/render/model_graph_internal.h"
#include "client/render_engine/nw.h"
#include "world/chunk/object_state_internal.h"

#include <xmmintrin.h>

static const double flat_box[4][6] = {{0, 0, 0, 1, 1, 1},
                                      {.125, .25, .375, .875, .75, .625},
                                      {-.25, -.5, -.125, 1.5, 1.25, 1.75},
                                      {0, 0, 0, 1, .5, 1}};
static const float flat_rgb[3][3] = {{.3f, .6f, .9f}, {-.25f, 1.5f, 2.f}, {0, 0, 0}};

static uint32_t flat_bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static uint64_t flat_double_bits(double value) {
    uint64_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool flat_prepare(mc_chunk_state *chunk, int32_t id, int pattern) {
    memset(chunk->data->blocks, 0, MC_CHUNK_BLOCKS);
    memset(chunk->data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(chunk->data->skylight, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(chunk->data->block_light, 0, MC_CHUNK_NIBBLE_BYTES);
    for (int32_t x = 7; x <= 9; ++x)
        for (int32_t y = 63; y <= 65; ++y)
            for (int32_t z = 7; z <= 9; ++z) {
                const int block = pattern == 0   ? 0
                                  : pattern == 1 ? 1
                                                 : ((x + 2 * y + 4 * z) & 2) != 0;
                const size_t index = ((uint32_t)x << 11u) | ((uint32_t)z << 7u) | (uint32_t)y;
                chunk->data->blocks[index] = (uint8_t)block;
                CHECK(mc_chunk_nibbles_set(chunk->data->metadata, MC_CHUNK_NIBBLE_BYTES, x, y, z,
                                           (x + y + z) & 3));
                CHECK(mc_chunk_nibbles_set(chunk->data->skylight, MC_CHUNK_NIBBLE_BYTES, x, y, z,
                                           (x * 3 + y * 5 + z * 7) & 15));
                CHECK(mc_chunk_nibbles_set(chunk->data->block_light, MC_CHUNK_NIBBLE_BYTES, x, y, z,
                                           (x * 7 + y * 11 + z * 13) & 15));
            }
    chunk->data->blocks[(8u << 11u) | (8u << 7u) | 64u] = (uint8_t)id;
    return true;
}

static bool flat_observe(unsigned int index, const int profile[11],
                         const mc_client_render_blocks *b, const mc_block *block, int result,
                         const char *exception, int32_t before, const mc_client_nw_state *t,
                         bool chunk_light) {
    CHECK(printf("FLAT %u", index) >= 0);
    for (size_t i = 0; i < 11; ++i)
        CHECK(printf(" %d", profile[i]) >= 0);
    CHECK(printf(" %d %s %d", result, exception, b->m) >= 0);
    for (size_t i = 0; i < 27; ++i)
        CHECK(printf(" %08" PRIx32, flat_bits(b->n_to_N[i])) >= 0);
    unsigned int flags = 0;
    for (unsigned int i = 0; i < 12; ++i)
        if (b->ab_to_am[i])
            flags |= 1u << i;
    CHECK(printf(" %u", flags) >= 0);
    for (size_t i = 0; i < 12; ++i)
        CHECK(printf(" %08" PRIx32, flat_bits(b->P_to_aa[i])) >= 0);
    const mc_block_fields *f = &block->fields;
    const double bounds[6] = {f->bs, f->bt, f->bu, f->bv, f->bw, f->bx};
    for (size_t i = 0; i < 6; ++i)
        CHECK(printf(" %016" PRIx64, flat_double_bits(bounds[i])) >= 0);
    const int features = (t->fields.l ? 1 : 0) | (t->fields.m ? 2 : 0) | (t->fields.n ? 4 : 0);
    CHECK(printf(" %" PRId32 " %" PRId32 " %" PRId32 " %08" PRIx32 " %d %d %" PRId32, t->fields.h,
                 t->fields.o, t->fields.p, (uint32_t)t->fields.k, features, chunk_light,
                 t->fields.o - before) >= 0);
    for (int32_t i = before; i < t->fields.o; ++i)
        CHECK(printf(" %08" PRIx32, (uint32_t)t->fields.g[i]) >= 0);
    CHECK(putchar('\n') != EOF);
    return true;
}

static bool flat_controls(picking_fixture *f, const char *profiles) {
    mc_client_chunk_cache *cache = NULL;
    mc_client_chunk_cache_error cache_error;
    const size_t bindings = f->base.world->persistent_bindings;
    CHECK(mc_client_chunk_cache_create(f->base.world, 0, 0, 0, 31, 127, 31, &cache, &cache_error) ==
          MC_CLIENT_CHUNK_CACHE_OK);
    mc_client_model_error model_error;
    CHECK(mc_client_render_blocks_active_use(f->models, &model_error));
    const bool prior_overlay = f->models->cv_a;
    mc_client_tessellator *tessellator = NULL;
    mc_client_graphics_error graphics;
    CHECK(mc_client_nw_active_use(f->graphics,
                                  (mc_client_graphics_site){"native-test", "flat", "()V", -1},
                                  &tessellator, &graphics));
    CHECK(mc_client_nw_begin_quads(tessellator, &graphics));
    CHECK(mc_client_nw_translation(tessellator, -.25, .5, -.75, &graphics));
    f->base.world->fields.f = 3;
    FILE *input = fopen(profiles, "r");
    CHECK(input != NULL);
    int p[11], scanned;
    unsigned int index = 0;
    while ((scanned = fscanf(input, "%d%d%d%d%d%d%d%d%d%d%d", &p[0], &p[1], &p[2], &p[3], &p[4],
                             &p[5], &p[6], &p[7], &p[8], &p[9], &p[10])) == 11) {
        CHECK(p[0] >= 1 && p[0] <= 20 && p[1] >= 0 && p[1] <= 2 && p[2] >= 0 && p[2] <= 1 &&
              p[3] >= -1 && p[3] <= 17 && p[4] >= 0 && p[4] <= 1 && p[5] >= 0 && p[5] <= 3 &&
              p[6] >= 0 && p[6] <= 3 && p[7] >= -1 && p[7] <= 3 && p[8] >= 0 && p[8] <= 3 &&
              p[9] >= 0 && p[9] <= 1 && p[10] >= 0 && p[10] <= 2);
        CHECK(flat_prepare(f->chunks[0], p[0], p[1]));
        const mc_block *block;
        CHECK(mc_registry_block_get(f->base.registry, p[0], &block, NULL) == MC_REGISTRY_OK &&
              block != NULL);
        mc_block_fields *bf = &((mc_block *)block)->fields;
        const mc_block_fields saved = *bf;
        const double *box = flat_box[p[6]];
        bf->bs = box[0];
        bf->bt = box[1];
        bf->bu = box[2];
        bf->bv = box[3];
        bf->bw = box[4];
        bf->bx = box[5];
        if (p[7] >= 0)
            bf->bm = p[7];
        if (p[5] == 3)
            bf->bA = NULL;
        mc_client_render_blocks fields;
        mc_client_render_blocks_construct_fields(&fields, p[5] == 1 ? NULL : cache, true);
        fields.m = true;
        fields.f = p[2] != 0;
        fields.d = p[3];
        fields.e = p[9] != 0;
        fields.g = fields.h = fields.i = fields.j = fields.k = fields.l = p[8];
        f->models->cv_a = p[4] != 0;
        for (size_t i = 0; i < 27; ++i)
            fields.n_to_N[i] = (float)(i + 1) * .03125f;
        for (size_t i = 0; i < 12; ++i) {
            fields.P_to_aa[i] = (float)(i + 1) * .015625f;
            fields.ab_to_am[i] = (i & 1u) != 0;
        }
        mc_client_nw_state state;
        CHECK(mc_client_nw_inspect(f->graphics, &state));
        const int32_t before = state.fields.o;
        f->base.world_classes->chunk_a = false;
        mc_cv_context c = {.fields = &fields,
                           .cache = cache,
                           .registry = f->base.registry,
                           .models = f->models,
                           .graphics = f->graphics,
                           .application = f->app,
                           .tessellator = tessellator};
        CHECK(mc_cv_push(&c, "b", "(Luu;IIIFFF)Z"));
        bool rendered = true;
        const float *rgb = flat_rgb[p[10]];
        const bool completed =
            mc_cv_flat(&c, p[5] == 2 ? NULL : block, 8, 64, 8, rgb[0], rgb[1], rgb[2], &rendered);
        const char *exception = "NONE";
        if (!completed) {
            CHECK(p[5] >= 1 && p[5] <= 3 && c.failed && !fields.m && c.last_store_pc == 2 &&
                  rendered && c.failure.status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
                  !c.failure.native_policy);
            CHECK(c.frames[0].pc == (p[5] == 3 ? 312u : 164u));
            if (p[5] == 1)
                CHECK(c.depth == 2 && c.frames[1].pc == 13);
            else
                CHECK(c.depth == 1);
            exception = "NULL";
        } else {
            CHECK(p[5] == 0 || (p[5] == 3 && p[6] == 0));
            CHECK(!c.failed && !fields.m && c.source_pc == 1044 && c.last_store_pc == 2 &&
                  c.depth == 1);
        }
        CHECK(fields.c == (p[5] == 1 ? NULL : cache) && fields.d == p[3] &&
              fields.e == (p[9] != 0) && fields.f == (p[2] != 0) && fields.b && fields.O == 1 &&
              fields.g == p[8] && fields.h == p[8] && fields.i == p[8] && fields.j == p[8] &&
              fields.k == p[8] && fields.l == p[8]);
        CHECK(bf->bs == box[0] && bf->bt == box[1] && bf->bu == box[2] && bf->bv == box[3] &&
              bf->bw == box[4] && bf->bx == box[5]);
        CHECK(mc_client_nw_inspect(f->graphics, &state) && state.fields.w && state.fields.r == 7 &&
              state.fields.s == -.25 && state.fields.t == .5 && state.fields.u == -.75 &&
              f->base.world->persistent_bindings == bindings + 1u);
        CHECK(flat_observe(index++, p, &fields, block, completed ? (rendered ? 1 : 0) : -1,
                           exception, before, &state, f->base.world_classes->chunk_a));
        *bf = saved;
    }
    CHECK(scanned == EOF && !ferror(input) && fclose(input) == 0 && index == 25);
    /* Source m=false precedes both actual NW use and the first FP product. */
    mc_client_render_blocks fields;
    mc_client_render_blocks_construct_fields(&fields, cache, true);
    fields.m = true;
    mc_cv_context fp = {.fields = &fields,
                        .cache = cache,
                        .registry = f->base.registry,
                        .models = f->models,
                        .graphics = f->graphics};
    CHECK(mc_cv_push(&fp, "b", "(Luu;IIIFFF)Z"));
    const unsigned int csr = _mm_getcsr();
    bool result = true;
    _mm_setcsr(csr | 0x8040u);
    const bool completed = mc_cv_flat(&fp, NULL, 8, 64, 8, .3f, .6f, .9f, &result);
    _mm_setcsr(csr);
    CHECK(!completed && fp.failed && !fields.m && fp.source_pc == 32 && fp.last_store_pc == 2 &&
          fp.tessellator == tessellator && result &&
          fp.failure.status == MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP && fp.failure.native_policy);
    f->models->cv_a = prior_overlay;
    CHECK(mc_client_nw_translation(tessellator, 0, 0, 0, &graphics));
    CHECK(mc_client_chunk_cache_retire(&cache, &cache_error) == MC_CLIENT_CHUNK_CACHE_OK &&
          cache == NULL && f->base.world->persistent_bindings == bindings);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4)
        return 2;
    picking_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !picking_open(f, argv[1]) || !picking_case_open(f, argv[2]) ||
        !flat_controls(f, argv[3]) || !picking_case_close(f) || !picking_close(f))
        return 1;
    free(f);
    printf("Source flat: %u checks; 25 original whole-body rows, retained fields and NW vertices\n",
           checks);
    return 0;
}
