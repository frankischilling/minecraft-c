#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

/* Genuine saved World, Registry and Chunk owners; earlier mains never run. */
#define MC_PICKING_BODY_MAIN_NAME mc_fluid_picking_fixture_main
int mc_fluid_picking_fixture_main(int argc, char **argv);
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

static uint32_t fluid_float_bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static uint64_t fluid_double_bits(double value) {
    uint64_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static const char *fluid_bool(bool value) {
    return value ? "true" : "false";
}

/* Compare source fields and actual reference identities, not C padding. */
static bool fluid_cv_same(const mc_client_render_blocks *left,
                          const mc_client_render_blocks *right) {
    return left->c == right->c && left->d == right->d && left->e == right->e &&
           left->f == right->f && left->b == right->b && left->g == right->g &&
           left->h == right->h && left->i == right->i && left->j == right->j &&
           left->k == right->k && left->l == right->l && left->m == right->m &&
           left->O == right->O && memcmp(left->n_to_N, right->n_to_N, sizeof(left->n_to_N)) == 0 &&
           memcmp(left->P_to_aa, right->P_to_aa, sizeof(left->P_to_aa)) == 0 &&
           memcmp(left->ab_to_am, right->ab_to_am, sizeof(left->ab_to_am)) == 0;
}

static bool fluid_block_same(const mc_block_fields *left, const mc_block_fields *right) {
    return left->bm == right->bm && left->bn == right->bn &&
           fluid_float_bits(left->bo) == fluid_float_bits(right->bo) &&
           fluid_float_bits(left->bp) == fluid_float_bits(right->bp) && left->bq == right->bq &&
           left->br == right->br && fluid_double_bits(left->bs) == fluid_double_bits(right->bs) &&
           fluid_double_bits(left->bt) == fluid_double_bits(right->bt) &&
           fluid_double_bits(left->bu) == fluid_double_bits(right->bu) &&
           fluid_double_bits(left->bv) == fluid_double_bits(right->bv) &&
           fluid_double_bits(left->bw) == fluid_double_bits(right->bw) &&
           fluid_double_bits(left->bx) == fluid_double_bits(right->bx) && left->by == right->by &&
           fluid_float_bits(left->bz) == fluid_float_bits(right->bz) && left->bA == right->bA &&
           fluid_float_bits(left->bB) == fluid_float_bits(right->bB) && left->a == right->a;
}

static bool fluid_nw_same(const mc_client_nw_fields *left, const mc_client_nw_fields *right) {
    return left->d == right->d && left->e == right->e && left->f == right->f &&
           left->g == right->g && left->h == right->h &&
           fluid_double_bits(left->i) == fluid_double_bits(right->i) &&
           fluid_double_bits(left->j) == fluid_double_bits(right->j) && left->k == right->k &&
           left->l == right->l && left->m == right->m && left->n == right->n &&
           left->o == right->o && left->p == right->p && left->q == right->q &&
           left->r == right->r && fluid_double_bits(left->s) == fluid_double_bits(right->s) &&
           fluid_double_bits(left->t) == fluid_double_bits(right->t) &&
           fluid_double_bits(left->u) == fluid_double_bits(right->u) && left->v == right->v &&
           left->w == right->w && left->x == right->x && left->y == right->y &&
           left->z == right->z && left->A == right->A && left->B == right->B;
}

static void fluid_cv_fields(const mc_client_render_blocks *fields) {
    for (size_t i = 13; i < 27; ++i)
        printf("%s%c=%08" PRIx32, i == 13 ? "" : ",", (int)('A' + i - 13),
               fluid_float_bits(fields->n_to_N[i]));
    printf(",O=%" PRId32, fields->O);
    for (size_t i = 0; i < 11; ++i)
        printf(",%c=%08" PRIx32, (int)('P' + i), fluid_float_bits(fields->P_to_aa[i]));
    printf(",aa=%08" PRIx32, fluid_float_bits(fields->P_to_aa[11]));
    for (size_t i = 0; i < 12; ++i)
        printf(",a%c=%s", (int)('b' + i), fluid_bool(fields->ab_to_am[i]));
    printf(",b=%s,d=%" PRId32 ",e=%s,f=%s,g=%" PRId32 ",h=%" PRId32 ",i=%" PRId32 ",j=%" PRId32
           ",k=%" PRId32 ",l=%" PRId32 ",m=%s",
           fluid_bool(fields->b), fields->d, fluid_bool(fields->e), fluid_bool(fields->f),
           fields->g, fields->h, fields->i, fields->j, fields->k, fields->l, fluid_bool(fields->m));
    for (size_t i = 0; i < 13; ++i)
        printf(",%c=%08" PRIx32, (int)('n' + i), fluid_float_bits(fields->n_to_N[i]));
}

static void fluid_nw_fields(const mc_client_nw_fields *fields) {
    printf("A=%" PRId32 ",B=%" PRId32 ",h=%" PRId32 ",i=%016" PRIx64 ",j=%016" PRIx64 ",k=%" PRId32
           ",l=%s,m=%s,n=%s,o=%" PRId32 ",p=%" PRId32 ",q=%s,r=%" PRId32 ",s=%016" PRIx64
           ",t=%016" PRIx64 ",u=%016" PRIx64 ",v=%" PRId32 ",w=%s,x=%s,z=%" PRId32,
           fields->A, fields->B, fields->h, fluid_double_bits(fields->i),
           fluid_double_bits(fields->j), fields->k, fluid_bool(fields->l), fluid_bool(fields->m),
           fluid_bool(fields->n), fields->o, fields->p, fluid_bool(fields->q), fields->r,
           fluid_double_bits(fields->s), fluid_double_bits(fields->t), fluid_double_bits(fields->u),
           fields->v, fluid_bool(fields->w), fluid_bool(fields->x), fields->z);
}

static void fluid_bounds(const mc_block_fields *fields) {
    const double box[6] = {fields->bs, fields->bt, fields->bu, fields->bv, fields->bw, fields->bx};
    for (size_t i = 0; i < 6; ++i)
        printf("%s%016" PRIx64, i == 0 ? "" : ",", fluid_double_bits(box[i]));
}

typedef struct fluid_buffer_snapshot {
    mc_client_direct_byte_buffer bytes;
    mc_client_direct_int_buffer ints;
    mc_client_direct_float_buffer floats;
} fluid_buffer_snapshot;

static bool fluid_buffers_capture(const mc_client_nw_fields *fields, fluid_buffer_snapshot *out) {
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

static bool fluid_buffers_same(const mc_client_nw_fields *fields,
                               const fluid_buffer_snapshot *before) {
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

static bool fluid_cell(mc_chunk_state *chunk, int32_t x, int32_t y, int32_t z, int id,
                       int metadata) {
    CHECK(x >= 0 && x < 16 && y >= 0 && y < 128 && z >= 0 && z < 16);
    chunk->data->blocks[((size_t)x << 11u) | ((size_t)z << 7u) | (size_t)y] = (uint8_t)id;
    CHECK(mc_chunk_nibbles_set(chunk->data->metadata, MC_CHUNK_NIBBLE_BYTES, x, y, z, metadata));
    return true;
}

static bool fluid_prepare(picking_fixture *fixture_state, const int values[13],
                          const int32_t xyz[3]) {
    for (size_t i = 0; i < 4; ++i) {
        mc_chunk_state *chunk = fixture_state->chunks[i];
        memset(chunk->data->blocks, 0, MC_CHUNK_BLOCKS);
        memset(chunk->data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
        memset(chunk->data->skylight, 0, MC_CHUNK_NIBBLE_BYTES);
        memset(chunk->data->block_light, 0, MC_CHUNK_NIBBLE_BYTES);
    }
    mc_chunk_state *chunk = fixture_state->chunks[0];
    const int32_t x = xyz[0], y = xyz[1], z = xyz[2];
    const int id = values[0], metadata = values[1], layout = values[2];
    for (int32_t dx = -2; dx <= 2; ++dx)
        for (int32_t dy = -2; dy <= 2; ++dy)
            for (int32_t dz = -2; dz <= 2; ++dz) {
                CHECK(mc_chunk_nibbles_set(chunk->data->skylight, MC_CHUNK_NIBBLE_BYTES, x + dx,
                                           y + dy, z + dz, values[3] == 1 ? 15 : 0) &&
                      mc_chunk_nibbles_set(chunk->data->block_light, MC_CHUNK_NIBBLE_BYTES, x + dx,
                                           y + dy, z + dz, values[3] == 2 ? 15 : 0));
            }
    if (layout >= 1 && layout <= 7) {
        for (int32_t dx = -1; dx <= 1; ++dx)
            for (int32_t dy = -1; dy <= 1; ++dy)
                for (int32_t dz = -1; dz <= 1; ++dz)
                    CHECK(fluid_cell(chunk, x + dx, y + dy, z + dz, id, metadata));
        if (layout >= 2)
            for (int32_t a = -1; a <= 1; ++a)
                for (int32_t b = -1; b <= 1; ++b) {
                    int32_t nx = x + a, ny = y + b, nz = z;
                    if (layout == 2 || layout == 3) {
                        ny = y + (layout == 2 ? 1 : -1);
                        nz = z + b;
                    } else if (layout == 4 || layout == 5) {
                        nz = z + (layout == 4 ? -1 : 1);
                    } else {
                        nx = x + (layout == 6 ? -1 : 1);
                        nz = z + a;
                    }
                    CHECK(fluid_cell(chunk, nx, ny, nz, 0, 0));
                }
    } else if (layout != 10 && layout != 13) {
        for (int32_t dx = -1; dx <= 1; ++dx)
            for (int32_t dz = -1; dz <= 1; ++dz)
                CHECK(fluid_cell(chunk, x + dx, y, z + dz, id, metadata));
    }
    CHECK(fluid_cell(chunk, x, y, z, id, metadata));
    switch (layout) {
        case 8:
            CHECK(fluid_cell(chunk, x - 1, y, z - 1, id, 0) &&
                  fluid_cell(chunk, x - 1, y, z, id, 1) && fluid_cell(chunk, x, y, z - 1, id, 7) &&
                  fluid_cell(chunk, x + 1, y, z + 1, id, 8));
            break;
        case 9:
            CHECK(fluid_cell(chunk, x - 1, y + 1, z - 1, id, metadata));
            break;
        case 10:
            CHECK(fluid_cell(chunk, x, y - 1, z, 12, 0));
            break;
        case 11:
            CHECK(fluid_cell(chunk, x - 1, y, z, 79, 0) && fluid_cell(chunk, x + 1, y, z, 0, 0));
            break;
        case 12:
            CHECK(fluid_cell(chunk, x - 1, y, z, id, 1) && fluid_cell(chunk, x + 1, y, z, id, 7));
            break;
        case 13:
            CHECK(fluid_cell(chunk, x + 1, y - 1, z, id, 2));
            break;
        case 14:
            CHECK(fluid_cell(chunk, x, y, z - 1, 1, 0) && fluid_cell(chunk, x, y - 1, z - 1, 1, 0));
            break;
        case 15:
            CHECK(fluid_cell(chunk, x + 1, y, z, id, 3) && fluid_cell(chunk, x, y, z + 1, id, 6) &&
                  fluid_cell(chunk, x, y + 1, z - 1, 1, 0));
            break;
        default:
            break;
    }
    return true;
}

/* Java Arrays.hashCode(byte[]) uses signed bytes and wrapping int arithmetic. */
static int32_t fluid_array_hash(const uint8_t *data, size_t bytes) {
    int32_t hash = 1;
    for (size_t i = 0; i < bytes; ++i) {
        const int32_t value = data[i] < 128 ? data[i] : (int32_t)data[i] - 256;
        hash = mc_java_i32_add(mc_java_i32_mul(hash, 31), value);
    }
    return hash;
}

static void fluid_array_hashes(picking_fixture *fixture_state, int32_t hashes[4][4]) {
    for (size_t i = 0; i < 4; ++i) {
        mc_chunk_state *chunk = fixture_state->chunks[i];
        hashes[i][0] = fluid_array_hash(chunk->data->blocks, MC_CHUNK_BLOCKS);
        hashes[i][1] = fluid_array_hash(chunk->data->metadata, MC_CHUNK_NIBBLE_BYTES);
        hashes[i][2] = fluid_array_hash(chunk->data->skylight, MC_CHUNK_NIBBLE_BYTES);
        hashes[i][3] = fluid_array_hash(chunk->data->block_light, MC_CHUNK_NIBBLE_BYTES);
    }
}

static bool fluid_cells(const mc_chunk_state *chunk, const int32_t xyz[3]) {
    bool first = true;
    for (int32_t dx = -2; dx <= 2; ++dx)
        for (int32_t dy = -2; dy <= 2; ++dy)
            for (int32_t dz = -2; dz <= 2; ++dz) {
                const int32_t x = xyz[0] + dx, y = xyz[1] + dy, z = xyz[2] + dz;
                uint8_t metadata, sky, light;
                CHECK(mc_chunk_nibbles_get(chunk->data->metadata, MC_CHUNK_NIBBLE_BYTES, x, y, z,
                                           &metadata) &&
                      mc_chunk_nibbles_get(chunk->data->skylight, MC_CHUNK_NIBBLE_BYTES, x, y, z,
                                           &sky) &&
                      mc_chunk_nibbles_get(chunk->data->block_light, MC_CHUNK_NIBBLE_BYTES, x, y, z,
                                           &light));
                const uint8_t id =
                    chunk->data->blocks[((size_t)x << 11u) | ((size_t)z << 7u) | (size_t)y];
                printf("%s%" PRId32 ",%" PRId32 ",%" PRId32 ",%u,%u,%u,%u", first ? "" : ";", x, y,
                       z, (unsigned int)id, (unsigned int)metadata, (unsigned int)sky,
                       (unsigned int)light);
                first = false;
            }
    return true;
}

static bool fluid_pool(picking_fixture *fixture_state, mc_entity_vector **identities,
                       size_t *identity_count) {
    mc_entity_classes *classes = fixture_state->base.entities;
    mc_entity_vector_snapshot state;
    CHECK(mc_entity_vector_get_snapshot(classes, &state) == MC_ENTITY_OK &&
          state.phase == MC_ENTITY_CLASS_READY && state.holds == 0 && state.size <= 1024 &&
          state.pool_identity == classes->vector_pool &&
          state.size == classes->vector_pool->length && state.size >= *identity_count &&
          state.cursor >= 0 && (size_t)state.cursor <= state.size);
    printf("{\"cursor\":%" PRId32 ",\"size\":%zu,\"class\":\"java.util.ArrayList\",\"entries\":[",
           state.cursor, state.size);
    for (size_t i = 0; i < state.size; ++i) {
        mc_entity_vector *vector = classes->vector_pool->elements[i];
        CHECK(vector != NULL && (i >= *identity_count || identities[i] == vector));
        for (size_t j = 0; j < i; ++j)
            CHECK(identities[j] != vector);
        identities[i] = vector;
        printf("%s[\"v%zu\",\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\"]",
               i == 0 ? "" : ",", i, fluid_double_bits(vector->x), fluid_double_bits(vector->y),
               fluid_double_bits(vector->z));
    }
    *identity_count = state.size;
    printf("]}");
    return true;
}

static bool fluid_controls(picking_fixture *fixture_state, const char *profiles,
                           unsigned int cohort) {
    static const unsigned int first_indices[3] = {0, 35, 70};
    static const unsigned int profile_counts[3] = {35, 35, 34};
    static const int32_t coordinates[3][3] = {{8, 64, 8}, {8, 64, 8}, {3, 65, 3}};
    static const double translations[3][3] = {{-.25, .5, -.75}, {-8, -64, -8}, {0, -64, 0}};
    static const float boxes[2][6] = {{0, 0, 0, 1, 1, 1}, {.125f, .25f, .375f, .875f, .75f, .625f}};
    CHECK(cohort < 3);
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
                                  (mc_client_graphics_site){"fixture", "fluid", "()V", 0},
                                  &tessellator, &graphics) &&
          mc_client_nw_begin_quads(tessellator, &graphics) &&
          mc_entity_vector_pool_clear_client(fixture_state->base.entities, NULL) == MC_ENTITY_OK);
    fixture_state->app->source_math_classes = fixture_state->math;
    fixture_state->base.world->fields.f = 3;
    mc_client_nw_state initial;
    mc_entity_vector_snapshot pool_initial;
    CHECK(mc_client_nw_inspect(fixture_state->graphics, &initial) && initial.static_b &&
          !initial.static_c && initial.fields.B == 2097152 && initial.fields.o == 0 &&
          initial.fields.g != NULL &&
          mc_entity_vector_get_snapshot(fixture_state->base.entities, &pool_initial) ==
              MC_ENTITY_OK &&
          pool_initial.size == 0 && pool_initial.cursor == 0 && pool_initial.holds == 0);
    const size_t capacity = (size_t)initial.fields.B;
    int32_t *prior_words = malloc(capacity * sizeof(*prior_words));
    mc_entity_vector **identities = calloc(1024, sizeof(*identities));
    size_t identity_count = 0;
    CHECK(prior_words != NULL && identities != NULL);
    FILE *input = fopen(profiles, "rb");
    CHECK(input != NULL);
    char label[128];
    int values[13], scanned;
    unsigned int calls = 0;
    size_t own_unchanged_words = 0;
    while (
        (scanned = fscanf(input, "%127s%d%d%d%d%d%d%d%d%d%d%d%d%d", label, &values[0], &values[1],
                          &values[2], &values[3], &values[4], &values[5], &values[6], &values[7],
                          &values[8], &values[9], &values[10], &values[11], &values[12])) == 14) {
        CHECK(calls < profile_counts[cohort] && values[0] >= 8 && values[0] <= 11 &&
              values[1] >= 0 && values[1] <= 15 && values[2] >= 0 && values[2] <= 15 &&
              values[3] >= 0 && values[3] <= 2 && values[4] >= 0 && values[4] <= 1 &&
              values[7] >= 0 && values[7] <= 1 && values[8] >= 0 && values[8] <= 2 &&
              values[9] >= 0 && values[9] <= 1 && values[10] >= 0 && values[10] <= 2 &&
              values[11] == 0 && values[12] == 0);
        const unsigned int profile_index = first_indices[cohort] + calls;
        const int32_t *xyz = coordinates[values[10]];
        const double *translation = translations[values[10]];
        const mc_block *view = NULL;
        mc_block_render_description description;
        CHECK(mc_registry_block_get(fixture_state->base.registry, values[0], &view, NULL) ==
                  MC_REGISTRY_OK &&
              view != NULL && view->fields.bn == values[0] &&
              view->kind == (values[0] % 2 == 0 ? MC_BLOCK_FLOWING_FLUID : MC_BLOCK_STILL_FLUID) &&
              mc_block_render_describe(view, &description) == MC_REGISTRY_OK &&
              description.type == 4 && description.pass == (values[0] < 10 ? 1 : 0));
        mc_block *block = (mc_block *)(void *)view;
        const mc_block_fields original_block = block->fields;
        block->fields.bm = mc_java_i32_add(original_block.bm, values[6]);
        const float *box = boxes[values[7]];
        mc_registry_block_bounds(block, box[0], box[1], box[2], box[3], box[4], box[5]);
        CHECK(fluid_prepare(fixture_state, values, xyz) &&
              mc_client_nw_translation(tessellator, translation[0], translation[1], translation[2],
                                       &graphics));
        const mc_block_fields entered_block = block->fields;
        int32_t input_arrays[4][4], after_arrays[4][4];
        fluid_array_hashes(fixture_state, input_arrays);
        mc_client_render_blocks fields = {0};
        mc_client_render_blocks_construct_fields(&fields, values[8] == 1 ? NULL : cache, true);
        fields.f = values[4] != 0;
        fields.d = values[5];
        if (values[9] != 0) {
            fields.m = fields.e = true;
            fields.g = fields.h = fields.i = fields.j = fields.k = fields.l = 2;
            for (size_t i = 0; i < 27; ++i)
                fields.n_to_N[i] = (float)(i + 1) * .03125f;
            for (size_t i = 0; i < 12; ++i) {
                fields.P_to_aa[i] = (float)(i + 1) * .015625f;
                fields.ab_to_am[i] = (i & 1u) != 0;
            }
        }
        const mc_client_render_blocks entered_cv = fields;
        mc_client_nw_state before, after;
        fluid_buffer_snapshot buffers;
        CHECK(mc_client_nw_inspect(fixture_state->graphics, &before) &&
              before.fields.g == initial.fields.g &&
              fluid_buffers_capture(&before.fields, &buffers));
        memcpy(prior_words, before.fields.g, capacity * sizeof(*prior_words));
        fixture_state->base.world_classes->chunk_a = false;
        printf("FLUID_POOL\t%u\t", profile_index);
        CHECK(fluid_pool(fixture_state, identities, &identity_count));
        mc_cv_context context = {.fields = &fields,
                                 .cache = values[8] == 1 ? NULL : cache,
                                 .registry = fixture_state->base.registry,
                                 .models = fixture_state->models,
                                 .graphics = fixture_state->graphics,
                                 .application = fixture_state->app};
        CHECK(mc_cv_push(&context, "k", "(Luu;III)Z"));
        const bool expected_null = values[8] != 0;
        const bool hidden = values[2] == 1;
        bool result = expected_null || hidden;
        const bool completed =
            mc_cv_fluid(&context, values[8] == 2 ? NULL : block, xyz[0], xyz[1], xyz[2], &result);
        CHECK(completed != expected_null);
        if (completed) {
            CHECK(!context.failed && result == !hidden && context.depth == 1 &&
                  context.source_pc == (hidden ? 215u : 1244u) &&
                  context.last_store_pc == (hidden ? 0u : 1239u));
        } else {
            CHECK(result && context.failed &&
                  context.failure.status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
                  !context.failure.native_policy && context.last_store_pc == 0);
            if (values[8] == 2) {
                CHECK(context.depth == 1 && context.source_pc == 14);
            } else {
                CHECK(context.depth == 2 && context.source_pc == 5 && context.frames[0].pc == 73 &&
                      strcmp(context.frames[1].class_name, "rp") == 0 &&
                      strcmp(context.frames[1].method, "b") == 0);
            }
        }
        putchar('\t');
        CHECK(fluid_pool(fixture_state, identities, &identity_count));
        putchar('\n');
        CHECK(context.tessellator == tessellator && fluid_cv_same(&entered_cv, &fields) &&
              mc_client_nw_inspect(fixture_state->graphics, &after));
        mc_block_fields expected_block = entered_block;
        if (completed && result) {
            expected_block.bt = 0;
            expected_block.bw = 1;
        }
        CHECK(fluid_block_same(&expected_block, &block->fields));
        fluid_array_hashes(fixture_state, after_arrays);
        CHECK(memcmp(input_arrays, after_arrays, sizeof(input_arrays)) == 0 &&
              after.fields.d == before.fields.d && after.fields.e == before.fields.e &&
              after.fields.f == before.fields.f && after.fields.g == before.fields.g &&
              after.fields.y == before.fields.y && after.fields.B == before.fields.B &&
              fluid_buffers_same(&after.fields, &buffers) && after.fields.w &&
              after.fields.r == 7 && after.fields.o >= before.fields.o &&
              (size_t)after.fields.o <= capacity && after.fields.o - before.fields.o <= 288 &&
              (after.fields.o - before.fields.o) % 48 == 0 &&
              after.fields.h - before.fields.h == (after.fields.o - before.fields.o) / 8 &&
              after.fields.p - before.fields.p == (after.fields.o - before.fields.o) / 12 &&
              fluid_double_bits(after.fields.s) == fluid_double_bits(before.fields.s) &&
              fluid_double_bits(after.fields.t) == fluid_double_bits(before.fields.t) &&
              fluid_double_bits(after.fields.u) == fluid_double_bits(before.fields.u) &&
              fixture_state->base.world->persistent_bindings == bindings + 1);
        if (!completed || !result)
            CHECK(fluid_nw_same(&before.fields, &after.fields));
        CHECK(memcmp(prior_words, after.fields.g, (size_t)before.fields.o * sizeof(*prior_words)) ==
                  0 &&
              memcmp(prior_words + after.fields.o, after.fields.g + after.fields.o,
                     (capacity - (size_t)after.fields.o) * sizeof(*prior_words)) == 0);
        own_unchanged_words += capacity - (size_t)(after.fields.o - before.fields.o);
        printf("FLUID_INPUT\t%u\t%s\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\t", profile_index,
               values[0] % 2 == 0 ? "om" : "nx", block->fields.bn, original_block.bm,
               entered_block.bm);
        for (size_t i = 0; i < 13; ++i)
            printf("%s%d", i == 0 ? "" : ",", values[i]);
        printf("\t%" PRId32 ",%" PRId32 ",%" PRId32 "\t%" PRId32 "\n", xyz[0], xyz[1], xyz[2],
               description.pass);
        printf("FLUID_CELLS\t%u\t", profile_index);
        CHECK(fluid_cells(fixture_state->chunks[0], xyz));
        putchar('\t');
        for (size_t i = 0; i < 4; ++i)
            printf("%s%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32, i == 0 ? "" : ";",
                   input_arrays[i][0], input_arrays[i][1], input_arrays[i][2], input_arrays[i][3]);
        putchar('\n');
        printf("FLUID\t%u\t%s\t%s\t%s\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\t%d\t%016" PRIx64
               ",%016" PRIx64 ",%016" PRIx64 "\t",
               profile_index, label, completed ? fluid_bool(result) : "UNRETURNED",
               completed ? "NONE" : "NULL", before.fields.o, after.fields.o, after.fields.B,
               fixture_state->base.world_classes->chunk_a, fluid_double_bits(after.fields.s),
               fluid_double_bits(after.fields.t), fluid_double_bits(after.fields.u));
        fluid_bounds(&entered_block);
        putchar('\t');
        fluid_bounds(&block->fields);
        putchar('\t');
        fluid_cv_fields(&entered_cv);
        putchar('\t');
        fluid_nw_fields(&before.fields);
        putchar('\t');
        fluid_nw_fields(&after.fields);
        putchar('\t');
        const int32_t extent =
            after.fields.o > before.fields.o + 100 ? after.fields.o : before.fields.o + 100;
        CHECK((size_t)extent <= capacity);
        for (int32_t i = before.fields.o; i < extent; ++i)
            printf("%s%08" PRIx32, i == before.fields.o ? "" : ",", (uint32_t)after.fields.g[i]);
        CHECK(putchar('\n') != EOF && fflush(stdout) == 0);
        block->fields = original_block;
        ++calls;
    }
    CHECK(scanned == EOF && !ferror(input) && fclose(input) == 0 &&
          calls == profile_counts[cohort]);
    free(identities);
    free(prior_words);
    fixture_state->app->source_math_classes = NULL;
    CHECK(mc_client_nw_translation(tessellator, 0, 0, 0, &graphics) &&
          mc_client_chunk_cache_retire(&cache, &cache_error) == MC_CLIENT_CHUNK_CACHE_OK &&
          cache == NULL && fixture_state->base.world->persistent_bindings == bindings);
    printf("FLUID_NATIVE cohort=%u calls=%u own-unchanged-words=%zu cleanup=1\n", cohort, calls,
           own_unchanged_words);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 5 || strlen(argv[4]) != 1 || argv[4][0] < '0' || argv[4][0] > '2')
        return 2;
    const unsigned int cohort = (unsigned int)(argv[4][0] - '0');
    picking_fixture *fixture_state = calloc(1, sizeof(*fixture_state));
    if (fixture_state == NULL || !picking_open(fixture_state, argv[1]) ||
        !picking_case_open(fixture_state, argv[2]) ||
        !fluid_controls(fixture_state, argv[3], cohort) || !picking_case_close(fixture_state) ||
        !picking_close(fixture_state))
        return 1;
    free(fixture_state);
    printf("Source Fluid world: %u checks; original calls, fields, bounds, NW words, pool and "
           "cleanup\n",
           checks);
    return 0;
}
