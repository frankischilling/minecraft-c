#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

/* Genuine saved World, Registry and Chunk owners; earlier mains never run. */
#define MC_PICKING_BODY_MAIN_NAME mc_redstone_picking_fixture_main
int mc_redstone_picking_fixture_main(int argc, char **argv);
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
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static uint32_t redstone_float_bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static uint64_t redstone_double_bits(double value) {
    uint64_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static const char *redstone_bool(bool value) {
    return value ? "true" : "false";
}

/* Compare source fields and actual reference identities, not C padding. */
static bool redstone_cv_same(const mc_client_render_blocks *left,
                             const mc_client_render_blocks *right) {
    return left->c == right->c && left->d == right->d && left->e == right->e &&
           left->f == right->f && left->b == right->b && left->g == right->g &&
           left->h == right->h && left->i == right->i && left->j == right->j &&
           left->k == right->k && left->l == right->l && left->m == right->m &&
           left->O == right->O && memcmp(left->n_to_N, right->n_to_N, sizeof(left->n_to_N)) == 0 &&
           memcmp(left->P_to_aa, right->P_to_aa, sizeof(left->P_to_aa)) == 0 &&
           memcmp(left->ab_to_am, right->ab_to_am, sizeof(left->ab_to_am)) == 0;
}

static bool redstone_block_same(const mc_block_fields *left, const mc_block_fields *right) {
    return left->bm == right->bm && left->bn == right->bn &&
           redstone_float_bits(left->bo) == redstone_float_bits(right->bo) &&
           redstone_float_bits(left->bp) == redstone_float_bits(right->bp) &&
           left->bq == right->bq && left->br == right->br &&
           redstone_double_bits(left->bs) == redstone_double_bits(right->bs) &&
           redstone_double_bits(left->bt) == redstone_double_bits(right->bt) &&
           redstone_double_bits(left->bu) == redstone_double_bits(right->bu) &&
           redstone_double_bits(left->bv) == redstone_double_bits(right->bv) &&
           redstone_double_bits(left->bw) == redstone_double_bits(right->bw) &&
           redstone_double_bits(left->bx) == redstone_double_bits(right->bx) &&
           left->by == right->by &&
           redstone_float_bits(left->bz) == redstone_float_bits(right->bz) &&
           left->bA == right->bA &&
           redstone_float_bits(left->bB) == redstone_float_bits(right->bB) && left->a == right->a;
}

static void redstone_cv_fields(const mc_client_render_blocks *fields) {
    for (size_t i = 13; i < 27; ++i)
        printf("%s%c=%08" PRIx32, i == 13 ? "" : ",", (int)('A' + i - 13),
               redstone_float_bits(fields->n_to_N[i]));
    printf(",O=%" PRId32, fields->O);
    for (size_t i = 0; i < 11; ++i)
        printf(",%c=%08" PRIx32, (int)('P' + i), redstone_float_bits(fields->P_to_aa[i]));
    printf(",aa=%08" PRIx32, redstone_float_bits(fields->P_to_aa[11]));
    for (size_t i = 0; i < 12; ++i)
        printf(",a%c=%s", (int)('b' + i), redstone_bool(fields->ab_to_am[i]));
    printf(",b=%s,d=%" PRId32 ",e=%s,f=%s,g=%" PRId32 ",h=%" PRId32 ",i=%" PRId32 ",j=%" PRId32
           ",k=%" PRId32 ",l=%" PRId32 ",m=%s",
           redstone_bool(fields->b), fields->d, redstone_bool(fields->e), redstone_bool(fields->f),
           fields->g, fields->h, fields->i, fields->j, fields->k, fields->l,
           redstone_bool(fields->m));
    for (size_t i = 0; i < 13; ++i)
        printf(",%c=%08" PRIx32, (int)('n' + i), redstone_float_bits(fields->n_to_N[i]));
}

static void redstone_nw_fields(const mc_client_nw_fields *fields) {
    printf("A=%" PRId32 ",B=%" PRId32 ",h=%" PRId32 ",i=%016" PRIx64 ",j=%016" PRIx64 ",k=%" PRId32
           ",l=%s,m=%s,n=%s,o=%" PRId32 ",p=%" PRId32 ",q=%s,r=%" PRId32 ",s=%016" PRIx64
           ",t=%016" PRIx64 ",u=%016" PRIx64 ",v=%" PRId32 ",w=%s,x=%s,z=%" PRId32,
           fields->A, fields->B, fields->h, redstone_double_bits(fields->i),
           redstone_double_bits(fields->j), fields->k, redstone_bool(fields->l),
           redstone_bool(fields->m), redstone_bool(fields->n), fields->o, fields->p,
           redstone_bool(fields->q), fields->r, redstone_double_bits(fields->s),
           redstone_double_bits(fields->t), redstone_double_bits(fields->u), fields->v,
           redstone_bool(fields->w), redstone_bool(fields->x), fields->z);
}

static void redstone_bounds(const mc_block_fields *fields) {
    const double box[6] = {fields->bs, fields->bt, fields->bu, fields->bv, fields->bw, fields->bx};
    for (size_t i = 0; i < 6; ++i)
        printf("%s%016" PRIx64, i == 0 ? "" : ",", redstone_double_bits(box[i]));
}

typedef struct redstone_buffer_snapshot {
    mc_client_direct_byte_buffer bytes;
    mc_client_direct_int_buffer ints;
    mc_client_direct_float_buffer floats;
} redstone_buffer_snapshot;

static bool redstone_buffers_capture(const mc_client_nw_fields *fields,
                                     redstone_buffer_snapshot *out) {
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

static bool redstone_buffers_same(const mc_client_nw_fields *fields,
                                  const redstone_buffer_snapshot *before) {
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

/* Same constructor/terminal sequence as the inherited picking fixture. The
 * signed cohort declares twelve saved Chunks and admits that finite roster
 * before World construction; no live policy or provider cache is repaired. */
static const int32_t redstone_signed_chunks[12][2] = {
    {0, 0},        {0, 1},        {1, 0},       {1, 1},       {-1048577, 0}, {-1048577, 1},
    {-1048576, 0}, {-1048576, 1}, {1048575, 0}, {1048575, 1}, {1048576, 0},  {1048576, 1}};

static bool redstone_signed_case_open(picking_fixture *f, const char *directory,
                                      mc_chunk_state **chunks, size_t *first_owners) {
    fixture *b = &f->base;
    uint8_t ids[256];
    for (size_t i = 0; i < sizeof(ids); ++i)
        ids[i] = (uint8_t)i;
    const mc_world_store_options options = {.session_token = 6381,
                                            .block_map = ids,
                                            .block_map_length = sizeof(ids),
                                            .limits = mc_world_store_default_limits()};
    CHECK(mc_world_store_open(directory, b->regions, &options, &b->store, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_provider_create(0, NULL, &b->dimension, NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 12;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, b->world_classes,
                                          &limits, &b->world, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments args = {
        .store = b->store, .name = b->name, .seed = 23, .dimension = b->dimension};
    CHECK(mc_world_runtime_construct_world(b->world, &args, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(b->world->construction.mode == MC_WORLD_RUNTIME_GAME &&
          b->world->construction.phase == MC_WORLD_RUNTIME_READY);
    CHECK(mc_chunk_provider_bind_fields(b->world->fields.v, &f->provider, NULL) ==
          MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < 12; ++i) {
        CHECK(mc_world_runtime_get(b->world, redstone_signed_chunks[i][0],
                                   redstone_signed_chunks[i][1],
                                   &chunks[i]) == MC_CHUNK_ACQUIRE_OK);
        CHECK(chunks[i] != NULL && chunks[i]->active && chunks[i]->terrain_populated);
        first_owners[i] = i;
        for (size_t j = 0; j < i; ++j)
            if (chunks[j] == chunks[i]) {
                first_owners[i] = j;
                break;
            }
        /* Source yy.a packs low coordinate bits; ok.b returns its first hit. */
        const size_t expected_owner = i < 10 ? i : i - 10;
        CHECK(first_owners[i] == expected_owner &&
              chunks[i]->x == redstone_signed_chunks[expected_owner][0] &&
              chunks[i]->z == redstone_signed_chunks[expected_owner][1]);
        if (i < 4)
            f->chunks[i] = chunks[i];
    }
    CHECK(picking_player(f, &f->actors[0]) && picking_player(f, &f->actors[1]));
    CHECK(mc_minecart_entity_allocate(b->entities, &f->actors[2], NULL) == MC_ENTITY_OK);
    CHECK(mc_minecart_entity_construct(f->actors[2], b->world, 8.5, 64.5, 10.5, 0, NULL) ==
          MC_ENTITY_OK);
    for (size_t i = 3; i < 5; ++i) {
        CHECK(mc_falling_entity_allocate(b->entities, &f->actors[i], NULL) == MC_ENTITY_OK);
        CHECK(mc_falling_entity_construct(f->actors[i], b->world, 8.5, 64.5, 10.5, 13, NULL) ==
              MC_ENTITY_OK);
    }
    CHECK(mc_item_entity_allocate(b->entities, &f->actors[5], NULL) == MC_ENTITY_OK);
    CHECK(mc_item_entity_construct(f->actors[5], b->world, 8.5, 64.5, 10.5, f->stack, NULL) ==
          MC_ENTITY_OK);
    for (size_t i = 0; i < 6; ++i)
        CHECK(mc_entity_set_position(f->actors[i], 8.5, 64.5, 10.5) == MC_ENTITY_OK);
    mc_entity *camera = f->actors[0];
    camera->base.offset = 0;
    CHECK(mc_entity_set_position(camera, 8.5, 64.5, 8.5) == MC_ENTITY_OK);
    camera->base.aJ = camera->base.x;
    camera->base.aK = camera->base.y;
    camera->base.aL = camera->base.z;
    camera->base.yaw = camera->base.aT = camera->base.aU = camera->base.aV = 0;
    f->app->i = camera;
    f->app->h = camera;
    f->app->active_world = b->world;
    f->app->controller = f->controller;
    f->app->y = NULL;
    f->px->fields.m = f->actors[2]; /* Prepared borrowed source field, no native m occurrence. */
    CHECK(mc_entity_bounds_pool_reset(b->entities, NULL) == MC_ENTITY_OK);
    CHECK(mc_entity_vector_pool_reset(b->entities, NULL) == MC_ENTITY_OK);
    return true;
}

static bool redstone_signed_case_close(picking_fixture *f, mc_chunk_state **chunks,
                                       const size_t *first_owners) {
    fixture *b = &f->base;
    CHECK(mc_client_entity_renderer_pick_retire(&f->call) == MC_CLIENT_PICK_OK && f->call == NULL);
    CHECK(mc_client_entity_renderer_pick_detach(f->px, &f->detach) == MC_CLIENT_PICK_OK);
    f->px->fields.m = NULL; /* End an unowned prepared early-return borrow. */
    f->app->y = NULL;
    CHECK(mc_entity_hit_result_release(&f->sentinel, NULL) == MC_ENTITY_HIT_OK);
    f->app->i = f->app->h = NULL;
    f->app->active_world = NULL;
    CHECK(mc_world_runtime_retire_entity_containers(b->world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_chunk_pending_clear(f->provider.pending) == MC_CHUNK_ACQUIRE_OK);
    CHECK(mc_chunk_cache_init(f->provider.cache, f->provider.cache->entries,
                              f->provider.cache->capacity, f->provider.cache->insertions,
                              f->provider.cache->insertion_capacity));
    mc_world_provider_unbind(b->dimension);
    CHECK(mc_world_runtime_retire_world(b->world, NULL) == MC_WORLD_STORAGE_OK);
    /* The first-owner map was captured with every owner alive. No pointer
     * comparison or owner inspection follows the first retirement. */
    for (size_t i = 0; i < 12; ++i)
        if (first_owners[i] == i)
            CHECK(mc_world_store_retire_chunk(b->store, chunks[i], NULL) == MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < 12; ++i) {
        chunks[i] = NULL;
        if (i < 4)
            f->chunks[i] = NULL;
    }
    for (size_t i = 0; i < 6; ++i) {
        mc_entity_release(f->actors[i]);
        f->actors[i] = NULL;
    }
    if (f->external_result != NULL) {
        mc_entity_hit_result_view view;
        CHECK(mc_world_runtime_free(b->world, NULL) == MC_WORLD_STORAGE_BUSY);
        CHECK(mc_entity_hit_result_inspect(f->external_result, &view) == MC_ENTITY_HIT_OK &&
              view.phase == MC_ENTITY_HIT_READY && view.native_holds == 1 &&
              view.fields.g != NULL && view.fields.f->x == 91 && view.fields.f->y == 92 &&
              view.fields.f->z == 93);
        CHECK(mc_entity_hit_result_release(&f->external_result, NULL) == MC_ENTITY_HIT_OK &&
              f->external_result == NULL);
    }
    if (f->foreign_world_hold) {
        CHECK(mc_world_runtime_free(b->world, NULL) == MC_WORLD_STORAGE_BUSY);
        mc_world_runtime_binding_release(b->world);
        f->foreign_world_hold = false;
    }
    CHECK(mc_world_runtime_free(b->world, NULL) == MC_WORLD_STORAGE_OK);
    b->world = NULL;
    mc_world_provider_release(b->dimension);
    b->dimension = NULL;
    CHECK(mc_world_store_free_checked(&b->store, NULL) == MC_WORLD_STORAGE_OK);
    return true;
}

static bool redstone_voxel(mc_chunk_state *chunk, int32_t x, int32_t y, int32_t z, int32_t id,
                           int32_t metadata) {
    CHECK(x >= 0 && x < 16 && y >= 0 && y < 128 && z >= 0 && z < 16 && id >= 0 && id <= 255);
    chunk->data->blocks[((size_t)x << 11u) | ((size_t)z << 7u) | (size_t)y] = (uint8_t)id;
    CHECK(mc_chunk_nibbles_set(chunk->data->metadata, MC_CHUNK_NIBBLE_BYTES, x, y, z, metadata));
    return true;
}

static bool redstone_scene_voxel(mc_chunk_state **chunks, size_t count, int32_t x, int32_t y,
                                 int32_t z, int32_t id, int32_t metadata) {
    CHECK(count == 12);
    const int32_t cx = mc_java_i32_shr(x, 4), cz = mc_java_i32_shr(z, 4);
    for (size_t i = 0; i < count; ++i)
        /* Select the original helper's request slot, then write its actual
         * returned Chunk, including the two packed-key aliases. */
        if (redstone_signed_chunks[i][0] == cx && redstone_signed_chunks[i][1] == cz)
            return redstone_voxel(chunks[i], (int32_t)((uint32_t)x & 15u), y,
                                  (int32_t)((uint32_t)z & 15u), id, metadata);
    CHECK(false);
}

static bool redstone_prepare(mc_chunk_state **chunks, size_t count, const int p[13], bool vertical,
                             bool capability, bool signed_coordinates, bool nullable) {
    for (size_t i = 0; i < count; ++i) {
        memset(chunks[i]->data->blocks, 0, MC_CHUNK_BLOCKS);
        memset(chunks[i]->data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
        memset(chunks[i]->data->skylight, 0, MC_CHUNK_NIBBLE_BYTES);
        memset(chunks[i]->data->block_light, 0, MC_CHUNK_NIBBLE_BYTES);
    }
    if (signed_coordinates) {
        const int32_t x = p[7], y = p[8], z = p[9];
        CHECK(redstone_scene_voxel(chunks, count, x, mc_java_i32_sub(y, 1), z, 1, 0) &&
              redstone_scene_voxel(chunks, count, x, y, z, 55, p[2]));
        if (p[10] != 0)
            CHECK(redstone_scene_voxel(chunks, count, mc_java_i32_sub(x, 1), y, z, 1, 0) &&
                  redstone_scene_voxel(chunks, count, mc_java_i32_sub(x, 1), mc_java_i32_add(y, 1),
                                       z, 55, 0));
        return true;
    }
    mc_chunk_state *center = chunks[0];
    if (!capability && !nullable)
        for (int32_t x = 7; x <= 9; ++x)
            for (int32_t y = 63; y <= 65; ++y)
                for (int32_t z = 7; z <= 9; ++z) {
                    CHECK(mc_chunk_nibbles_set(center->data->skylight, MC_CHUNK_NIBBLE_BYTES, x, y,
                                               z, p[3] == 1 ? 15 : 0));
                    CHECK(mc_chunk_nibbles_set(center->data->block_light, MC_CHUNK_NIBBLE_BYTES, x,
                                               y, z, p[3] == 2 ? 15 : 0));
                }
    CHECK(redstone_voxel(center, 8, 63, 8, 1, 0) && redstone_voxel(center, 8, 64, 8, 55, p[2]));
    if (capability)
        return redstone_voxel(center, 7, 64, 8, p[7], p[8]);
    if (nullable) {
        if (p[9] == 1)
            CHECK(redstone_voxel(center, 7, 64, 8, 1, 0));
        if (p[9] == 2)
            CHECK(redstone_voxel(center, 8, 65, 8, 1, 0));
        return true;
    }
    static const int32_t directions[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (size_t i = 0; i < 4; ++i) {
        if ((p[7] & (1 << i)) == 0)
            continue;
        const int32_t x = 8 + directions[i][0], z = 8 + directions[i][1];
        CHECK(redstone_voxel(center, x, 64, z, vertical ? p[8] : 55, 0));
        if (vertical)
            CHECK(redstone_voxel(center, x, 63, z, p[9], 0) &&
                  redstone_voxel(center, x, 65, z, p[10], 0));
    }
    if (vertical)
        CHECK(redstone_voxel(center, 8, 65, 8, p[11], 0));
    return true;
}

/* Arrays.hashCode over the actual whole NW backing, with Java int wrapping. */
static int32_t redstone_words_hash(const int32_t *words, size_t count) {
    uint32_t hash = 1;
    for (size_t i = 0; i < count; ++i)
        hash = hash * 31u + (uint32_t)words[i];
    int32_t result;
    memcpy(&result, &hash, sizeof(result));
    return result;
}

static bool redstone_operands(const int p[13], bool vertical, bool capability,
                              bool signed_coordinates, bool nullable) {
    CHECK(p[0] == 0 && p[1] == 55 && p[2] >= 0 && p[2] <= 15 && p[3] >= 0 && p[3] <= 2 &&
          p[4] == INT_MIN);
    if (signed_coordinates) {
        CHECK(p[2] == 7 && p[3] == 0 && p[5] == 0 && p[6] == -1 &&
              (p[7] == -16777217 || p[7] == -16777216 || p[7] == 16777215 || p[7] == 16777216) &&
              p[8] == 64 && p[9] == 8 && (p[10] == 0 || p[10] == 1) && p[11] == 0 && p[12] == 0);
    } else if (nullable) {
        CHECK(p[2] == 7 && p[3] == 0 && p[5] == 0 && (p[6] == -1 || p[6] == 0) &&
              (p[7] == 0 || p[7] == 1) && (p[8] == 0 || p[8] == 1) && p[9] >= 0 && p[9] <= 2 &&
              p[10] == 0 && p[11] == 0 && p[12] == 0 &&
              (p[6] != 0 || (p[7] == 0 && p[8] == 1 && p[9] == 0)) &&
              (p[9] == 0 || (p[7] == 0 && p[8] == 0 && p[6] == -1)));
    } else if (capability) {
        CHECK(p[2] == 7 && p[3] == 0 && p[5] == 0 && p[6] == -1 &&
              (p[7] == 93 || p[7] == 94 || p[7] == 70 || p[7] == 1) && p[8] >= 0 && p[8] <= 3 &&
              ((p[7] == 93 || p[7] == 94) || p[8] == 0));
        for (size_t i = 9; i < 13; ++i)
            CHECK(p[i] == 0);
    } else if (vertical) {
        CHECK(p[3] == 0 && p[5] == 0 && p[6] == -1 &&
              (p[7] == 1 || p[7] == 2 || p[7] == 4 || p[7] == 8 || p[7] == 15) &&
              (p[8] == 0 || p[8] == 1) && (p[9] == 0 || p[9] == 55) &&
              (p[10] == 0 || p[10] == 55 || p[10] == 75) && (p[11] == 0 || p[11] == 1) &&
              p[12] == 0);
    } else {
        CHECK(p[7] >= 0 && p[7] <= 15);
        for (size_t i = 8; i < 13; ++i)
            CHECK(p[i] == 0);
    }
    return true;
}

static const char *redstone_west_class(const mc_block *block) {
    if (block == NULL)
        return NULL;
    switch (block->kind) {
        case MC_BLOCK_REPEATER:
            return "wo";
        case MC_BLOCK_PRESSURE_PLATE:
            return "bv";
        case MC_BLOCK_STONE:
            return "eo";
        default:
            return NULL;
    }
}

static unsigned int redstone_dispatcher_checks;

/* Same-namespace native dispatcher join; its direct-g words are independently
 * compared with the original row by the driver. No extra original row or
 * whole-dispatcher equivalence is claimed. Keep the existing begun batch. */
static bool redstone_dispatcher_control(picking_fixture *f, mc_client_chunk_cache *cache,
                                        mc_chunk_state **scene, const int first[13],
                                        const int32_t expected[96], int32_t *prior,
                                        size_t capacity) {
    const unsigned int start = checks;
    CHECK(redstone_prepare(scene, 4, first, false, false, false, false));
    const mc_block *block = NULL;
    CHECK(mc_registry_block_get(f->base.registry, 55, &block, NULL) == MC_REGISTRY_OK &&
          block != NULL && block->kind == MC_BLOCK_REDSTONE);
    mc_client_render_blocks fields = {0};
    mc_client_render_blocks_construct_fields(&fields, cache, true);
    fields.d = first[6];
    const mc_client_render_blocks entered_cv = fields;
    const mc_block_fields entered_block = block->fields;
    mc_client_nw_state before, after;
    redstone_buffer_snapshot buffers;
    CHECK(mc_client_nw_inspect(f->graphics, &before) && before.fields.o >= 0 &&
          (size_t)before.fields.o + 96u < capacity && before.fields.B == (int32_t)capacity &&
          redstone_buffers_capture(&before.fields, &buffers));
    memcpy(prior, before.fields.g, capacity * sizeof(*prior));
    mc_java_random *const random = f->base.world->fields.r;
    CHECK(random != NULL);
    const mc_java_random random_before = *random;
    const size_t bindings = f->base.world->persistent_bindings;
    f->base.world_classes->chunk_a = false; /* The same first-profile input. */
    mc_cv_context call = {.fields = &fields,
                          .cache = cache,
                          .registry = f->base.registry,
                          .models = f->models,
                          .graphics = f->graphics,
                          .application = f->app};
    bool rendered = false;
    CHECK(mc_cv_render(&call, block, 8, 64, 8, &rendered) && rendered && call.returned &&
          !call.failed && call.depth == 0 && call.source_pc == 146 && call.last_store_pc == 0 &&
          redstone_cv_same(&fields, &entered_cv) &&
          redstone_block_same(&block->fields, &entered_block));
    CHECK(f->base.world->fields.r == random && random_before.state == random->state &&
          random_before.have_next_next_gaussian == random->have_next_next_gaussian &&
          redstone_double_bits(random_before.next_next_gaussian) ==
              redstone_double_bits(random->next_next_gaussian) &&
          f->base.world->persistent_bindings == bindings &&
          mc_client_nw_inspect(f->graphics, &after));
    CHECK(after.fields.o == before.fields.o + 96 && after.fields.h == before.fields.h + 12 &&
          after.fields.p == before.fields.p + 8 && after.fields.d == before.fields.d &&
          after.fields.e == before.fields.e && after.fields.f == before.fields.f &&
          after.fields.g == before.fields.g && after.fields.y == before.fields.y &&
          after.fields.B == before.fields.B && after.fields.w && after.fields.r == 7 &&
          redstone_buffers_same(&after.fields, &buffers) &&
          redstone_double_bits(after.fields.s) == redstone_double_bits(before.fields.s) &&
          redstone_double_bits(after.fields.t) == redstone_double_bits(before.fields.t) &&
          redstone_double_bits(after.fields.u) == redstone_double_bits(before.fields.u));
    CHECK(memcmp(expected, after.fields.g + before.fields.o, 96u * sizeof(*expected)) == 0 &&
          memcmp(prior, after.fields.g, (size_t)before.fields.o * sizeof(*prior)) == 0 &&
          memcmp(prior + after.fields.o, after.fields.g + after.fields.o,
                 (capacity - (size_t)after.fields.o) * sizeof(*prior)) == 0);
    redstone_dispatcher_checks = checks - start;
    return true;
}

static bool redstone_controls(picking_fixture *f, const char *profiles, const char *layout,
                              mc_chunk_state **scene, const size_t *first_owners) {
    const bool vertical = strcmp(layout, "vertical") == 0;
    const bool capability = strcmp(layout, "capability") == 0;
    const bool signed_coordinates = strcmp(layout, "signed") == 0;
    const bool nullable = strcmp(layout, "nullable") == 0;
    const bool flat = strcmp(layout, "flat") == 0;
    const char *tag = signed_coordinates ? "SIGNED"
                      : nullable         ? "NULLABLE"
                      : capability       ? "CAPABILITY"
                      : vertical         ? "VERTICAL"
                                         : "REDSTONE";
    const size_t bindings = f->base.world->persistent_bindings;
    mc_client_chunk_cache *caches[3] = {0};
    const size_t cache_count = signed_coordinates ? 3u : 1u;
    static const int32_t cache_minima[3] = {0, -16777232, 16777200};
    mc_client_chunk_cache_error cache_error;
    for (size_t i = 0; i < cache_count; ++i)
        CHECK(mc_client_chunk_cache_create(f->base.world, cache_minima[i], 0, 0,
                                           cache_minima[i] + 31, 127, 31, &caches[i],
                                           &cache_error) == MC_CLIENT_CHUNK_CACHE_OK);
    mc_client_model_error model_error;
    CHECK(mc_client_render_blocks_active_use(f->models, &model_error));
    mc_client_tessellator *tessellator = NULL;
    mc_client_graphics_error graphics;
    CHECK(mc_client_nw_active_use(f->graphics,
                                  (mc_client_graphics_site){"fixture", "redstone", "()V", 0},
                                  &tessellator, &graphics) &&
          mc_client_nw_begin_quads(tessellator, &graphics) &&
          mc_client_nw_translation(tessellator, -.25, .5, -.75, &graphics));
    f->base.world->fields.f = 3; /* Declared original sky subtraction, before the cohort. */
    mc_client_nw_state initial;
    CHECK(mc_client_nw_inspect(f->graphics, &initial) && initial.static_b && !initial.static_c &&
          initial.fields.B == 2097152 && initial.fields.g != NULL && initial.fields.o == 0);
    const size_t capacity = (size_t)initial.fields.B;
    int32_t *prior = malloc(capacity * sizeof(*prior));
    CHECK(prior != NULL);
    int32_t *const emissions = f->base.registry->blocks.s;
    mc_block **const blocks = f->base.registry->blocks.m;
    CHECK(emissions != NULL && blocks != NULL && f->base.registry->blocks.m_length == 256);
    if (signed_coordinates) {
        size_t unique = 0;
        for (size_t i = 0; i < 12; ++i)
            unique += first_owners[i] == i ? 1u : 0u;
        printf("SIGNED_OWNERS\t12\t%zu", unique);
        for (size_t i = 0; i < 12; ++i)
            printf("\t%zu,%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%zu", i,
                   redstone_signed_chunks[i][0], redstone_signed_chunks[i][1], scene[i]->x,
                   scene[i]->z, first_owners[i]);
        putchar('\n');
    }
    printf("%s_STATIC\tNW.b=%s\tNW.c=%s\tUU.s.length=256\tUU.s.final=true\n", tag,
           redstone_bool(initial.static_b), redstone_bool(initial.static_c));
    FILE *input = fopen(profiles, "rb");
    CHECK(input != NULL);
    char label[128];
    int p[13], scanned;
    unsigned int index = 0;
    size_t total_unchanged = 0;
    int first_profile[13] = {0};
    int32_t first_words[96] = {0};
    while ((scanned =
                fscanf(input, "%127s%d%d%d%d%d%d%d%d%d%d%d%d%d", label, &p[0], &p[1], &p[2], &p[3],
                       &p[4], &p[5], &p[6], &p[7], &p[8], &p[9], &p[10], &p[11], &p[12])) == 14) {
        CHECK(redstone_operands(p, vertical, capability, signed_coordinates, nullable));
        const int32_t x = signed_coordinates ? p[7] : 8;
        const int32_t y = signed_coordinates ? p[8] : 64;
        const int32_t z = signed_coordinates ? p[9] : 8;
        const size_t cache_index = signed_coordinates ? (x < 0 ? 1u : 2u) : 0u;
        mc_client_chunk_cache *cache = caches[cache_index];
        const mc_block *view = NULL;
        CHECK(mc_registry_block_get(f->base.registry, 55, &view, NULL) == MC_REGISTRY_OK &&
              view != NULL && view->fields.bn == 55 && view->kind == MC_BLOCK_REDSTONE);
        mc_block *block = (mc_block *)(void *)view;
        const int32_t original_bm = block->fields.bm, original_emission = emissions[55];
        if (flat)
            block->fields.bm = mc_java_i32_add(original_bm, p[5]);
        CHECK(redstone_prepare(scene, signed_coordinates ? 12u : 4u, p, vertical, capability,
                               signed_coordinates, nullable));
        mc_block *const stone = blocks[1];
        CHECK(stone != NULL && stone->kind == MC_BLOCK_STONE && stone->fields.bA != NULL);
        mc_material *const stone_material = stone->fields.bA;
        if (nullable && p[9] != 0)
            stone->fields.bA = NULL;
        const bool stone_null_at_entry = stone->fields.bA == NULL;
        const mc_block_fields entered_stone = stone->fields;
        const mc_block *west = capability ? blocks[p[7]] : NULL;
        const char *west_class = capability ? redstone_west_class(west) : NULL;
        CHECK(!capability || (west != NULL && west->fields.bn == p[7] && west_class != NULL));
        const mc_block_fields entered_block = block->fields;
        int32_t entered_emissions[256];
        memcpy(entered_emissions, emissions, sizeof(entered_emissions));
        mc_client_render_blocks fields = {0};
        mc_client_render_blocks_construct_fields(&fields, nullable && p[7] != 0 ? NULL : cache,
                                                 true);
        fields.d = p[6];
        const mc_client_render_blocks entered_cv = fields;
        mc_client_nw_state before, after;
        redstone_buffer_snapshot buffers;
        CHECK(mc_client_nw_inspect(f->graphics, &before) && before.fields.g == initial.fields.g &&
              before.fields.o >= 0 && (size_t)before.fields.o < capacity &&
              redstone_buffers_capture(&before.fields, &buffers));
        memcpy(prior, before.fields.g, capacity * sizeof(*prior));
        CHECK(f->base.world->fields.r != NULL);
        const mc_java_random random_before = *f->base.world->fields.r;
        mc_java_random *const random_identity = f->base.world->fields.r;
        f->base.world_classes->chunk_a = false; /* Declared LM.a input, not a result repair. */
        mc_cv_context context = {.fields = &fields,
                                 .cache = cache,
                                 .registry = f->base.registry,
                                 .models = f->models,
                                 .graphics = f->graphics,
                                 .application = f->app};
        CHECK(mc_cv_push(&context, "g", "(Luu;III)Z"));
        bool value = nullable;
        const bool completed =
            mc_cv_redstone(&context, nullable && p[8] != 0 ? NULL : block, x, y, z, &value);
        const bool block_unchanged = redstone_block_same(&entered_block, &block->fields);
        const bool stone_unchanged = redstone_block_same(&entered_stone, &stone->fields);
        if (nullable && p[9] != 0)
            stone->fields.bA = stone_material;
        if (flat)
            block->fields.bm = original_bm;
        const bool source_null = !completed && nullable && context.failed &&
                                 !context.failure.native_policy &&
                                 context.failure.status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE;
        if (source_null) {
            const unsigned int pc = p[7] != 0 ? 13u : p[8] != 0 ? 24u : p[9] == 1 ? 235u : 462u;
            CHECK(context.depth == 1 && context.source_pc == pc && context.frames[0].pc == pc &&
                  context.failure.source_pc == pc && !context.returned &&
                  !context.failure.source_returned &&
                  strcmp(context.failure.source_class, "cv") == 0 &&
                  strcmp(context.failure.source_method, "g") == 0 &&
                  strcmp(context.failure.source_descriptor, "(Luu;III)Z") == 0);
            if (p[9] != 0)
                CHECK(context.failure.child.class_name != NULL &&
                      strcmp(context.failure.child.class_name, "ew") == 0 &&
                      strcmp(context.failure.child.method, "h") == 0 &&
                      strcmp(context.failure.child.descriptor, "(III)Z") == 0 &&
                      context.failure.child.pc == 25 &&
                      context.cache_error.status == MC_CLIENT_CHUNK_CACHE_SOURCE_NULL &&
                      context.cache_error.bytecode == 25 && !context.cache_error.native_policy);
            else
                CHECK(context.failure.child.class_name == NULL &&
                      context.failure.child.method == NULL &&
                      context.failure.child.descriptor == NULL);
        }
        if (!completed && !source_null)
            fprintf(stderr, "Redstone row=%u name=%s pc=%u depth=%zu status=%d native=%d op=%s\n",
                    index, label, context.source_pc, context.depth, (int)context.failure.status,
                    (int)context.failure.native_policy,
                    context.failure.operation == NULL ? "-" : context.failure.operation);
        CHECK(value &&
              ((completed && !context.failed && context.depth == 1 && context.source_pc == 2781 &&
                context.frames[0].pc == 2781) ||
               source_null) &&
              context.last_store_pc == 0 && context.tessellator == tessellator && block_unchanged &&
              stone_unchanged && stone->fields.bA == stone_material &&
              redstone_cv_same(&fields, &entered_cv) && f->base.registry->blocks.m == blocks &&
              f->base.registry->blocks.s == emissions &&
              memcmp(entered_emissions, emissions, sizeof(entered_emissions)) == 0 &&
              f->base.world->fields.r == random_identity &&
              random_before.state == random_identity->state &&
              random_before.have_next_next_gaussian == random_identity->have_next_next_gaussian &&
              redstone_double_bits(random_before.next_next_gaussian) ==
                  redstone_double_bits(random_identity->next_next_gaussian) &&
              f->base.world->fields.f == 3 && mc_client_nw_inspect(f->graphics, &after));
        CHECK(after.fields.d == before.fields.d && after.fields.e == before.fields.e &&
              after.fields.f == before.fields.f && after.fields.g == before.fields.g &&
              after.fields.y == before.fields.y && after.fields.B == before.fields.B &&
              redstone_buffers_same(&after.fields, &buffers) && after.fields.w &&
              after.fields.r == 7 && after.fields.o >= before.fields.o && after.fields.o >= 0 &&
              (size_t)after.fields.o < capacity && after.fields.h >= before.fields.h &&
              after.fields.p >= before.fields.p &&
              (int64_t)after.fields.o - before.fields.o ==
                  ((int64_t)after.fields.h - before.fields.h) * 8 &&
              redstone_double_bits(after.fields.s) == redstone_double_bits(before.fields.s) &&
              redstone_double_bits(after.fields.t) == redstone_double_bits(before.fields.t) &&
              redstone_double_bits(after.fields.u) == redstone_double_bits(before.fields.u) &&
              f->base.world->persistent_bindings == bindings + cache_count);
        CHECK(memcmp(prior, after.fields.g, (size_t)before.fields.o * sizeof(*prior)) == 0 &&
              memcmp(prior + after.fields.o, after.fields.g + after.fields.o,
                     (capacity - (size_t)after.fields.o) * sizeof(*prior)) == 0);
        const size_t untouched = capacity - (size_t)(after.fields.o - before.fields.o);
        total_unchanged += untouched;
        if (flat && index == 0) {
            CHECK(after.fields.o - before.fields.o == 96);
            memcpy(first_profile, p, sizeof(first_profile));
            memcpy(first_words, after.fields.g + before.fields.o, sizeof(first_words));
        }
        printf("%s_INPUT\t%u\tsm\t55\t%" PRId32 "\t%d\t%" PRId32 "\t%" PRId32 "\t%" PRId32
               "\t%d\t%d\t%d",
               tag, index, original_bm, p[5], entered_block.bm, original_emission, emissions[55],
               p[2], p[3], p[7]);
        if (vertical)
            printf("\t%d\t%d\t%d\t%d", p[8], p[9], p[10], p[11]);
        if (capability)
            printf("\t%d\t%s", p[8], west_class);
        if (signed_coordinates)
            printf("\t%d\t%d\t%d\t%" PRId32, p[8], p[9], p[10], cache_minima[cache_index]);
        if (nullable)
            printf("\t%d\t%d\t%d\t%d", p[8], p[9], p[6], (int)stone_null_at_entry);
        printf("\n%s\t%u\t%s\t0\t%s\t%s\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\t%d\t%016" PRIx64
               ",%016" PRIx64 ",%016" PRIx64 "\t",
               tag, index, label, completed ? redstone_bool(value) : "UNRETURNED",
               source_null ? "NULL" : "NONE", before.fields.o, after.fields.o, after.fields.B,
               (int)f->base.world_classes->chunk_a, redstone_double_bits(after.fields.s),
               redstone_double_bits(after.fields.t), redstone_double_bits(after.fields.u));
        redstone_bounds(&entered_block);
        putchar('\t');
        redstone_cv_fields(&entered_cv);
        putchar('\t');
        redstone_nw_fields(&before.fields);
        putchar('\t');
        redstone_nw_fields(&after.fields);
        putchar('\t');
        const int32_t extent =
            after.fields.o > before.fields.o + 100 ? after.fields.o : before.fields.o + 100;
        CHECK((size_t)extent <= capacity);
        for (int32_t i = before.fields.o; i < extent; ++i)
            printf("%s%08" PRIx32, i == before.fields.o ? "" : ",", (uint32_t)after.fields.g[i]);
        printf("\n%s_BACKING\t%u\t%zu\t%" PRId32 "\t%" PRId32
               "\tjava.nio.DirectByteBuffer:0:8388608:8388608,java.nio.DirectIntBufferU:0:2097152:"
               "2097152,"
               "java.nio.DirectFloatBufferU:0:2097152:2097152,int[]:2097152,NULL\n",
               tag, index, untouched, redstone_words_hash(prior, capacity),
               redstone_words_hash(after.fields.g, capacity));
        CHECK(fflush(stdout) == 0 && !ferror(stdout) && block->fields.bm == original_bm &&
              emissions[55] == original_emission);
        ++index;
    }
    CHECK(scanned == EOF && !ferror(input) && fclose(input) == 0 &&
          index == (signed_coordinates ? 8u
                    : nullable         ? 6u
                    : capability       ? 10u
                    : vertical         ? 20u
                                       : 56u));
    if (flat)
        CHECK(redstone_dispatcher_control(f, caches[0], scene, first_profile, first_words, prior,
                                          capacity));
    free(prior);
    for (size_t i = 0; i < cache_count; ++i)
        CHECK(mc_client_chunk_cache_retire(&caches[i], &cache_error) == MC_CLIENT_CHUNK_CACHE_OK &&
              caches[i] == NULL);
    CHECK(f->base.world->persistent_bindings == bindings);
    printf("%s_COMPLETE rows=%u own-unchanged-words=%zu genuine-backing=2097152 begins=1 "
           "draw=0 raw-allocations=0 source-replacements=0\n",
           tag, index, total_unchanged);
    return true;
}

#include "redstone_callbacks.inc"

int main(int argc, char **argv) {
    if (argc != 5 ||
        (strcmp(argv[4], "flat") != 0 && strcmp(argv[4], "vertical") != 0 &&
         strcmp(argv[4], "capability") != 0 && strcmp(argv[4], "signed") != 0 &&
         strcmp(argv[4], "nullable") != 0 && strcmp(argv[4], "callbacks") != 0) ||
        (strcmp(argv[4], "callbacks") == 0 && strcmp(argv[3], "-") != 0))
        return 2;
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1)
        return 2;
#endif
    picking_fixture *f = calloc(1, sizeof(*f));
    mc_chunk_state *scene[12] = {0};
    size_t first_owners[12] = {0};
    const bool signed_coordinates = strcmp(argv[4], "signed") == 0;
    if (f == NULL || !picking_open(f, argv[1]) ||
        !(signed_coordinates ? redstone_signed_case_open(f, argv[2], scene, first_owners)
                             : picking_case_open(f, argv[2])))
        return 1;
    if (!signed_coordinates)
        memcpy(scene, f->chunks, sizeof(f->chunks));
    if (!(strcmp(argv[4], "callbacks") == 0
              ? redstone_callback_controls(f)
              : redstone_controls(f, argv[3], argv[4], scene, first_owners)) ||
        !(signed_coordinates ? redstone_signed_case_close(f, scene, first_owners)
                             : picking_case_close(f)) ||
        !picking_close(f))
        return 1;
    free(f);
    printf("REDSTONE_NATIVE checks=%u cleanup=1 dispatcher=%u dispatcher-checks=%u\n", checks,
           redstone_dispatcher_checks != 0 ? 1u : 0u, redstone_dispatcher_checks);
    return 0;
}
