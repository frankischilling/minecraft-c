/* Reuse genuine World/Registry/Chunk setup; the included main is not run. */
#define MC_PICKING_BODY_MAIN_NAME mc_color_picking_fixture_main
int mc_color_picking_fixture_main(int argc, char **argv);
#include "test_picking.c"
#undef main

#include "client/render/blocks/internal.h"
#include "client/world/chunk_cache_internal.h"

typedef struct block_color_fixture {
    picking_fixture *picking;
    mc_client_chunk_cache *cache;
    mc_world_chunk_manager *manager;
    const double *previous_a, *previous_b;
    const mc_biome_reference_array *previous_d;
    unsigned int rows;
} block_color_fixture;

static void color_values(const mc_biome_double_buffer *array) {
    if (array == NULL || array->data == NULL) {
        fputs("-", stdout);
        return;
    }
    if (array->length == 0) {
        fputs("E", stdout);
        return;
    }
    for (size_t i = 0; i < array->length; ++i) {
        if (i != 0)
            putchar(',');
        printf("%016" PRIx64, picking_bits(array->data[i]));
    }
}

static bool color_row(block_color_fixture *f, const char *name, int32_t id, bool null_access,
                      const char *expected, const char *source, unsigned int pc) {
    picking_fixture *p = f->picking;
    mc_registry_block_view blocks;
    CHECK(mc_registry_classes_inspect_blocks(p->base.registry, &blocks, NULL) == MC_REGISTRY_OK);
    CHECK(blocks.m != NULL && blocks.m[id] != NULL);
    mc_client_render_blocks fields = {.c = null_access ? NULL : f->cache};
    mc_cv_context c = {.fields = &fields,
                       .cache = f->cache,
                       .registry = p->base.registry,
                       .graphics = p->graphics,
                       .models = p->models};
    CHECK(mc_cv_push(&c, "l", "(Luu;III)Z"));
    const size_t depth = c.depth;
    int32_t value = INT32_C(0x12345678);
    const bool returned = mc_cv_block_color(&c, blocks.m[id], 8, 64, 8, 9, &value);
    const bool success = strcmp(expected, "OK") == 0;
    CHECK(returned == success && c.failed == !success);
    if (success) {
        CHECK(c.depth == depth && c.frames[0].pc == 9);
    } else {
        CHECK(value == INT32_C(0x12345678) && !c.failure.native_policy &&
              c.failure.source_pc == pc && strcmp(c.failure.source_class, source) == 0 &&
              c.depth >= 2 && c.frames[0].pc == 9 && !c.failure.source_returned);
        CHECK(c.failure.status == (strcmp(expected, "NULL") == 0
                                       ? MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
                                       : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED));
        if (strcmp(expected, "INDEX") == 0)
            CHECK(c.colorizer_error.array.status == MC_CLIENT_COLOR_ARRAY_INDEX_OUT_OF_RANGE &&
                  c.colorizer_error.array.site.pc == 35 && !c.colorizer_error.native_policy);
    }
    mc_world_chunk_manager_fields climate;
    mc_world_chunk_manager_game_fields game;
    CHECK(mc_world_chunk_manager_fields_get(f->manager, &climate) == MC_WORLD_PROVIDER_OK);
    CHECK(mc_world_chunk_manager_game_fields_get(f->manager, &game) == MC_WORLD_PROVIDER_OK);
    mc_client_colorizer_state grass, foliage;
    CHECK(mc_client_colorizer_inspect(p->graphics, MC_CLIENT_COLORIZER_GRASS, &grass));
    CHECK(mc_client_colorizer_inspect(p->graphics, MC_CLIENT_COLORIZER_FOLIAGE, &foliage));
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    printf("COLOR %s %s %08" PRIx32 " %u %u ", name, expected, bits, grass.a != NULL ? 1u : 0u,
           foliage.a != NULL ? 1u : 0u);
    color_values(climate.a);
    putchar(' ');
    color_values(climate.b);
    printf(" %u %u %u\n", climate.a->data == f->previous_a ? 1u : 0u,
           climate.b->data == f->previous_b ? 1u : 0u, game.d == f->previous_d ? 1u : 0u);
    f->previous_a = climate.a->data;
    f->previous_b = climate.b->data;
    f->previous_d = game.d;
    ++f->rows;
    return true;
}

static bool color_metadata(block_color_fixture *f, uint8_t value) {
    const size_t index = (8u << 11u) | (8u << 7u) | 64u;
    uint8_t *slot = &f->picking->chunks[0]->data->metadata[index / 2u];
    *slot = (uint8_t)((*slot & UINT8_C(0xf0)) | value);
    return true;
}

static bool colors_observe(picking_fixture *p) {
    block_color_fixture f = {.picking = p};
    CHECK(mc_client_chunk_cache_create(p->base.world, 0, -1, 0, 31, 129, 31, &f.cache, NULL) ==
          MC_CLIENT_CHUNK_CACHE_OK);
    CHECK(mc_client_chunk_cache_manager(f.cache, &f.manager, NULL) == MC_CLIENT_CHUNK_CACHE_OK &&
          f.manager != NULL);
    mc_biome_reference_array *primed = NULL;
    CHECK(mc_world_chunk_manager_cached(f.manager, 0, 0, 1, 1, &primed, NULL) ==
          MC_WORLD_PROVIDER_OK);
    mc_world_chunk_manager_fields fields;
    mc_world_chunk_manager_game_fields game;
    CHECK(mc_world_chunk_manager_fields_get(f.manager, &fields) == MC_WORLD_PROVIDER_OK);
    CHECK(fields.noise_backed != NULL);
    mc_biome_manager *m = fields.noise_backed;
    CHECK(mc_biome_double_buffer_resize(m->memory, &m->a, 3) == MC_BIOME_OK);
    CHECK(mc_biome_double_buffer_resize(m->memory, &m->b, 3) == MC_BIOME_OK);
    CHECK(mc_biome_double_buffer_resize(m->memory, &m->c, 3) == MC_BIOME_OK);
    for (size_t i = 0; i < 3; ++i) {
        m->a.data[i] = 9.0 - (double)i;
        m->b.data[i] = -1.0 - (double)i;
        m->c.data[i] = 6.0 - (double)i;
    }
    CHECK(mc_world_chunk_manager_game_fields_get(f.manager, &game) == MC_WORLD_PROVIDER_OK);
    f.previous_a = m->a.data;
    f.previous_b = m->b.data;
    f.previous_d = game.d;
    mc_client_colorizer_state state;
    CHECK(mc_client_colorizer_inspect(p->graphics, MC_CLIENT_COLORIZER_GRASS, &state) &&
          state.phase == MC_CLIENT_COLORIZER_RAW && state.a == NULL);
    CHECK(mc_client_colorizer_inspect(p->graphics, MC_CLIENT_COLORIZER_FOLIAGE, &state) &&
          state.phase == MC_CLIENT_COLORIZER_RAW && state.a == NULL);
    CHECK(color_row(&f, "BASE_NULL_XP", 3, true, "OK", NULL, 0));
    CHECK(color_row(&f, "REDSTONE_NULL_XP", 55, true, "OK", NULL, 0));
    CHECK(color_row(&f, "FLUID_NULL_XP", 8, true, "OK", NULL, 0));
    CHECK(color_metadata(&f, 1) && color_row(&f, "LEAF_PINE", 18, false, "OK", NULL, 0));
    CHECK(color_metadata(&f, 2) && color_row(&f, "LEAF_BIRCH", 18, false, "OK", NULL, 0));
    CHECK(color_metadata(&f, 3) && color_row(&f, "LEAF_BOTH", 18, false, "OK", NULL, 0));
    CHECK(color_metadata(&f, 0) && color_row(&f, "TALL_DEAD", 31, false, "OK", NULL, 0));
    CHECK(color_row(&f, "GRASS_COLD", 2, false, "OK", NULL, 0));
    CHECK(color_row(&f, "LEAF_COLD", 18, false, "OK", NULL, 0));
    CHECK(color_metadata(&f, 1) && color_row(&f, "TALL_COLD", 31, false, "OK", NULL, 0));
    mc_client_color_array *grass = NULL, *foliage = NULL, *empty = NULL;
    const mc_client_graphics_site prepared = {"fixture", "palette", "()V", -1};
    CHECK(mc_client_color_array_create(65536, NULL, prepared, &grass, NULL) ==
          MC_CLIENT_COLOR_ARRAY_OK);
    CHECK(mc_client_color_array_create(65536, NULL, prepared, &foliage, NULL) ==
          MC_CLIENT_COLOR_ARRAY_OK);
    for (size_t i = 0; i < 65536; ++i) {
        CHECK(mc_client_color_array_write(grass, i, UINT32_C(0xa5000000) | (uint32_t)i) ==
              MC_CLIENT_COLOR_ARRAY_OK);
        CHECK(mc_client_color_array_write(foliage, i, UINT32_C(0x5a000000) | (uint32_t)i) ==
              MC_CLIENT_COLOR_ARRAY_OK);
    }
    CHECK(mc_client_colorizer_set(p->graphics, MC_CLIENT_COLORIZER_GRASS, grass, NULL, NULL) ==
          MC_CLIENT_COLORIZER_OK);
    CHECK(mc_client_colorizer_set(p->graphics, MC_CLIENT_COLORIZER_FOLIAGE, foliage, NULL, NULL) ==
          MC_CLIENT_COLORIZER_OK);
    CHECK(color_row(&f, "GRASS_ARGB", 2, false, "OK", NULL, 0));
    CHECK(color_metadata(&f, 0) && color_row(&f, "LEAF_ARGB", 18, false, "OK", NULL, 0));
    CHECK(color_metadata(&f, 1) && color_row(&f, "TALL_ARGB_1", 31, false, "OK", NULL, 0));
    CHECK(color_metadata(&f, 2) && color_row(&f, "TALL_ARGB_2", 31, false, "OK", NULL, 0));
    CHECK(color_row(&f, "GRASS_REUSE", 2, false, "OK", NULL, 0));
    CHECK(color_row(&f, "GRASS_NULL_XP", 2, true, "NULL", "wp", 1));
    CHECK(color_row(&f, "LEAF_NULL_XP", 18, true, "NULL", "bk", 5));
    f.cache->state.d = NULL;
    CHECK(color_row(&f, "GRASS_NULL_WORLD", 2, false, "NULL", "wp", 1));
    CHECK(color_metadata(&f, 1) && color_row(&f, "LEAF_PINE_NULL_WORLD", 18, false, "OK", NULL, 0));
    f.cache->state.d = p->base.world;
    mc_world_provider *original = p->base.world->fields.t, *unbound = NULL;
    p->base.world->fields.t = NULL;
    CHECK(color_row(&f, "GRASS_NULL_DIM", 2, false, "NULL", "wp", 1));
    CHECK(mc_world_provider_create(0, NULL, &unbound, NULL) == MC_WORLD_PROVIDER_OK);
    p->base.world->fields.t = unbound;
    CHECK(color_row(&f, "GRASS_NULL_MANAGER", 2, false, "NULL", "wp", 11));
    p->base.world->fields.t = original;
    mc_world_provider_release(unbound);
    CHECK(mc_client_colorizer_set(p->graphics, MC_CLIENT_COLORIZER_GRASS, NULL, NULL, NULL) ==
          MC_CLIENT_COLORIZER_OK);
    CHECK(color_row(&f, "GRASS_NULL_COLOR_ARRAY", 2, false, "NULL", "ia", 35));
    CHECK(mc_client_color_array_create(0, NULL, prepared, &empty, NULL) ==
          MC_CLIENT_COLOR_ARRAY_OK);
    CHECK(mc_client_colorizer_set(p->graphics, MC_CLIENT_COLORIZER_FOLIAGE, empty, NULL, NULL) ==
          MC_CLIENT_COLORIZER_OK);
    CHECK(color_metadata(&f, 0) &&
          color_row(&f, "LEAF_SHORT_COLOR_ARRAY", 18, false, "INDEX", "jh", 35));
    CHECK(mc_client_colorizer_set(p->graphics, MC_CLIENT_COLORIZER_FOLIAGE, NULL, NULL, NULL) ==
          MC_CLIENT_COLORIZER_OK);
    CHECK(color_metadata(&f, 1) &&
          color_row(&f, "LEAF_PINE_NULL_COLOR_ARRAY", 18, false, "OK", NULL, 0));
    CHECK(color_row(&f, "TALL_NULL_COLOR_ARRAY", 31, false, "NULL", "ia", 35));
    CHECK(mc_client_colorizer_inspect(p->graphics, MC_CLIENT_COLORIZER_FOLIAGE, &state) &&
          state.phase == MC_CLIENT_COLORIZER_READY && state.a == NULL && state.last_store_pc == 1);
    CHECK(mc_client_color_array_release(&grass, NULL) == MC_CLIENT_COLOR_ARRAY_OK);
    CHECK(mc_client_color_array_release(&foliage, NULL) == MC_CLIENT_COLOR_ARRAY_OK);
    CHECK(mc_client_color_array_release(&empty, NULL) == MC_CLIENT_COLOR_ARRAY_OK);
    CHECK(mc_client_chunk_cache_retire(&f.cache, NULL) == MC_CLIENT_CHUNK_CACHE_OK);
    CHECK(f.rows == 25);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 1;
    picking_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !picking_open(f, argv[1]) || !picking_case_open(f, argv[2]) ||
        !colors_observe(f) || !picking_case_close(f) || !picking_close(f))
        return 1;
    free(f);
    printf("%u source Block color checks passed\n", checks);
    return 0;
}
