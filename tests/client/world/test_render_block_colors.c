/* Reuse genuine World/Registry/Chunk setup; the included main is not run. */
#define MC_PICKING_BODY_MAIN_NAME mc_color_picking_fixture_main
int mc_color_picking_fixture_main(int argc, char **argv);
#include "test_picking.c"
#undef main

#include "client/render/blocks/internal.h"
#include "client/render_engine/internal.h"
#include "client/world/chunk_cache_internal.h"
#include "entity/particles/digging_color_internal.h"
#include "entity/particles/particle_internal.h"
#include "world/chunk/object_internal.h"
#include "world/runtime/terminal_inventory_internal.h"
#include "world/storage/objects_internal.h"

#include <xmmintrin.h>

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

static bool color_particle(picking_fixture *p, mc_world_runtime *world, mc_entity **out,
                           mc_particle_error *e) {
    const mc_block *leaf = NULL;
    CHECK(mc_registry_block_get(p->base.registry, 18, &leaf, NULL) == MC_REGISTRY_OK);
    CHECK(mc_digging_particle_allocate(p->base.entities, out, NULL) == MC_ENTITY_OK);
    /* Constructor metadata is deliberately different from the live XP.e
     * operand. QM.a must read the actual World, independently of its icon. */
    CHECK(mc_digging_particle_construct(*out, world, p->base.registry, p->math, 8, 64, 8, 0, 0, 0,
                                        leaf, 0, 7, e) == MC_ENTITY_OK);
    return true;
}

static uint32_t color_float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

typedef struct color_load_observer {
    mc_world_runtime *world;
    mc_entity *particle;
    mc_chunk_load_callback original;
    mc_chunk_state *loaded;
    size_t calls;
    bool bad, rebound;
} color_load_observer;

static color_load_observer *color_loading;

static mc_chunk_load_result color_observed_load(void *loader,
                                                const mc_chunk_acquire_context *context, int32_t x,
                                                int32_t z, mc_chunk_state **out) {
    color_load_observer *o = color_loading;
    if (o == NULL)
        return MC_CHUNK_LOAD_FATAL;
    ++o->calls;
    o->bad = o->bad || context != &o->world->acquisition || x != 0 || z != 0 ||
             o->world->active_calls == 0 || o->particle->base.world != o->world;
    /* Delegate the real saved Chunk load/import exactly once. This observer
     * does not supply a surrogate Chunk, skip activation or change any phase. */
    const mc_chunk_load_result result = o->original(loader, context, x, z, out);
    if (result == MC_CHUNK_LOAD_LOADED) {
        o->loaded = *out;
        o->particle->base.world = NULL; /* Real synchronous mutation of QM.aI. */
        o->rebound = true;
    }
    return result;
}

static bool color_captured_world(picking_fixture *p, mc_client_render_engine *engine,
                                 mc_particle_error *e, mc_digging_color_error *d) {
    mc_entity *particle = NULL;
    CHECK(color_particle(p, p->base.world, &particle, e));
    mc_chunk_cache *cache = p->provider.cache;
    const size_t stored = mc_world_store_chunk_count(p->base.store);
    /* Explicit valid caller-owned input: an empty native Map populated with
     * the other three real Chunks. No Chunk phase, Source count or hold is
     * forged. The absent saved key must reach the normal loader callback. */
    CHECK(mc_chunk_cache_init(cache, cache->entries, cache->capacity, cache->insertions,
                              cache->insertion_capacity));
    for (int32_t x = 0; x < 2; ++x)
        for (int32_t z = 0; z < 2; ++z)
            if (x != 0 || z != 0)
                CHECK(mc_chunk_cache_publish(cache, x, z, p->chunks[(size_t)(x * 2 + z)]));
    color_load_observer observer = {
        .world = p->base.world, .particle = particle, .original = p->base.world->acquisition.load};
    color_loading = &observer;
    p->base.world->acquisition.load = color_observed_load;
    const mc_entity_status status =
        mc_digging_particle_tint_entered(particle, engine, 8, 64, 8, d, e);
    p->base.world->acquisition.load = observer.original;
    particle->base.world = p->base.world; /* Restore borrowed Source input before retirement. */
    color_loading = NULL;
    const bool result =
        status == MC_ENTITY_OK && observer.calls == 1 && !observer.bad && observer.rebound &&
        observer.loaded != NULL && observer.loaded != p->chunks[0] && observer.loaded->active &&
        !d->failed && d->block_pc == 84 && (uint32_t)d->value == UINT32_C(0xa53f81c7) &&
        e->source_pc == 92 && p->base.world->active_calls == 0 && p->base.world->local_leases == 0;
    /* End the authored cache scope, then retire only the extra empty loaded
     * owner with the ordinary complete structural preflight/native APIs. */
    CHECK(mc_chunk_cache_init(cache, cache->entries, cache->capacity, cache->insertions,
                              cache->insertion_capacity));
    for (int32_t x = 0; x < 2; ++x)
        for (int32_t z = 0; z < 2; ++z)
            CHECK(mc_chunk_cache_publish(cache, x, z, p->chunks[(size_t)(x * 2 + z)]));
    mc_entity_release(particle);
    CHECK(mc_world_runtime_entity_containers_validate(p->base.world, NULL) == MC_WORLD_STORAGE_OK);
    if (observer.loaded != NULL) {
        mc_world_chunk *actual = mc_world_store_chunk_object(p->base.store, observer.loaded);
        CHECK(actual != NULL && mc_world_chunk_entities_validate(actual));
        mc_world_chunk_containers_release(actual);
        CHECK(mc_world_store_retire_chunk(p->base.store, observer.loaded, NULL) ==
              MC_WORLD_STORAGE_OK);
    }
    CHECK(result && mc_world_store_chunk_count(p->base.store) == stored);
    return true;
}

/* CPU-only genuine QM/XW/World and actual JI/graphics owners. The existing
 * original rows above qualify BK's color ints. These controls additionally
 * qualify the reached QM RGB stores and failure prefixes, without a GL owner. */
static bool color_particle_controls(picking_fixture *p) {
    mc_client_render_engine_classes *graphics = NULL;
    mc_client_default_font *font = NULL;
    mc_game_settings *settings = NULL;
    mc_client_render_engine *engine = NULL;
    mc_entity *particle = NULL;
    mc_particle_error *e = calloc(1, sizeof(*e));
    mc_digging_color_error *d = calloc(1, sizeof(*d));
    CHECK(e != NULL && d != NULL);
    CHECK(mc_client_render_engine_classes_create(NULL, &graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_default_font_create(32u * 1024u * 1024u, &font) == MC_CLIENT_FONT_OK);
    CHECK(mc_game_settings_create_defaults(NULL, &settings, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_client_render_engine_allocate(graphics, font, NULL, &engine) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_engine_construct(engine, NULL, settings, NULL) ==
          MC_CLIENT_RENDER_ENGINE_OK);
    mc_client_colorizer_state state;
    CHECK(mc_client_colorizer_inspect(graphics, MC_CLIENT_COLORIZER_FOLIAGE, &state) &&
          state.phase == MC_CLIENT_COLORIZER_RAW);
    CHECK(color_particle(p, NULL, &particle, e));
    CHECK(mc_digging_particle_tint_entered(particle, engine, 8, 64, 8, d, e) ==
              MC_ENTITY_STATE_ERROR &&
          e->source_exception && !e->native_policy && strcmp(e->source_class, "bk") == 0 &&
          e->source_pc == 5 && particle->particle->fields.i == .6f &&
          particle->particle->fields.j == .6f && particle->particle->fields.k == .6f);
    CHECK(mc_client_colorizer_inspect(graphics, MC_CLIENT_COLORIZER_FOLIAGE, &state) &&
          state.phase == MC_CLIENT_COLORIZER_RAW);
    mc_entity_release(particle);
    particle = NULL;
    block_color_fixture fixture = {.picking = p};
    const uint64_t world_random = p->base.world->fields.r->state;
    /* Source integer/float bits: original BK/JH color ints and QM's three
     * i2f/fdiv/fmul sequences, with constructor RGB 0x3f19999a. */
    static const uint32_t pine[3] = {UINT32_C(0x3e69b684), UINT32_C(0x3eb851ec),
                                     UINT32_C(0x3e69b684)};
    static const uint32_t birch[3] = {UINT32_C(0x3e9a33ce), UINT32_C(0x3ec92f97),
                                      UINT32_C(0x3e4cccce)};
    unsigned char untouched[sizeof(d->acquisition)];
    memset(untouched, 0xa5, sizeof(untouched));
    for (uint8_t metadata = 1; metadata <= 16; ++metadata) {
        const uint8_t actual = metadata & 15u;
        CHECK(color_metadata(&fixture, actual) && color_particle(p, p->base.world, &particle, e));
        mc_world_provider *dimension = p->base.world->fields.t;
        if (metadata == 1)
            p->base.world->fields.t = NULL; /* Pine must not reach the manager. */
        memcpy(&d->acquisition, untouched, sizeof(untouched));
        const mc_entity_status status =
            mc_digging_particle_tint_entered(particle, engine, 8, 64, 8, d, e);
        p->base.world->fields.t = dimension;
        const bool is_pine = (actual & 1u) != 0, is_birch = !is_pine && (actual & 2u) != 0;
        const int32_t value = is_pine ? 6396257 : is_birch ? 8431445 : 0;
        CHECK(status == MC_ENTITY_OK && d->reached && !d->failed && d->value == value &&
              memcmp(&d->acquisition, untouched, sizeof(untouched)) == 0 &&
              d->block_pc == (is_pine    ? 23u
                              : is_birch ? 35u
                                         : 84u) &&
              e->source_pc == 92 &&
              color_float_bits(particle->particle->fields.i) == (is_pine    ? pine[0]
                                                                 : is_birch ? birch[0]
                                                                            : 0) &&
              color_float_bits(particle->particle->fields.j) == (is_pine    ? pine[1]
                                                                 : is_birch ? birch[1]
                                                                            : 0) &&
              color_float_bits(particle->particle->fields.k) == (is_pine    ? pine[2]
                                                                 : is_birch ? birch[2]
                                                                            : 0));
        mc_entity_release(particle);
        particle = NULL;
    }
    CHECK(mc_client_colorizer_inspect(graphics, MC_CLIENT_COLORIZER_FOLIAGE, &state) &&
          state.phase == MC_CLIENT_COLORIZER_READY && state.a != NULL && state.last_store_pc == 4);
    /* The earlier CV namespace ends with null JH.a. This independent actual
     * JI namespace is cold-zero initialized and must never read that other one. */
    mc_client_colorizer_state other;
    CHECK(mc_client_colorizer_inspect(p->graphics, MC_CLIENT_COLORIZER_FOLIAGE, &other) &&
          other.a == NULL && p->graphics != graphics);
    mc_client_color_array *palette = NULL, *empty = NULL;
    const mc_client_graphics_site prepared = {"fixture", "palette", "()V", -1};
    CHECK(mc_client_color_array_create(65536, NULL, prepared, &palette, NULL) ==
          MC_CLIENT_COLOR_ARRAY_OK);
    for (size_t i = 0; i < 65536; ++i)
        CHECK(mc_client_color_array_write(palette, i, UINT32_C(0xa53f81c7)) ==
              MC_CLIENT_COLOR_ARRAY_OK);
    CHECK(mc_client_colorizer_set(graphics, MC_CLIENT_COLORIZER_FOLIAGE, palette, NULL, NULL) ==
          MC_CLIENT_COLORIZER_OK);
    CHECK(color_metadata(&fixture, 0) && color_particle(p, p->base.world, &particle, e));
    CHECK(mc_digging_particle_tint_entered(particle, engine, 8, 64, 8, d, e) == MC_ENTITY_OK &&
          (uint32_t)d->value == UINT32_C(0xa53f81c7) &&
          color_float_bits(particle->particle->fields.i) == UINT32_C(0x3e17caff) &&
          color_float_bits(particle->particle->fields.j) == UINT32_C(0x3e9b6836) &&
          color_float_bits(particle->particle->fields.k) == UINT32_C(0x3eefbc8a));
    mc_entity_release(particle);
    particle = NULL;
    CHECK(mc_client_colorizer_set(graphics, MC_CLIENT_COLORIZER_FOLIAGE, NULL, NULL, NULL) ==
          MC_CLIENT_COLORIZER_OK);
    CHECK(color_particle(p, p->base.world, &particle, e));
    CHECK(mc_digging_particle_tint_entered(particle, engine, 8, 64, 8, d, e) ==
              MC_ENTITY_STATE_ERROR &&
          d->failed && d->block_pc == 81 &&
          d->colorizer.array.status == MC_CLIENT_COLOR_ARRAY_NULL_REFERENCE &&
          strcmp(e->source_class, "jh") == 0 && e->source_pc == 35 && e->source_exception &&
          particle->particle->fields.i == .6f && particle->particle->fields.j == .6f &&
          particle->particle->fields.k == .6f);
    mc_entity_release(particle);
    particle = NULL;
    CHECK(color_metadata(&fixture, 2) && color_particle(p, p->base.world, &particle, e));
    CHECK(mc_digging_particle_tint_entered(particle, engine, 8, 64, 8, d, e) == MC_ENTITY_OK &&
          d->value == 8431445);
    mc_entity_release(particle);
    particle = NULL;
    CHECK(mc_client_color_array_create(0, NULL, prepared, &empty, NULL) ==
          MC_CLIENT_COLOR_ARRAY_OK);
    CHECK(mc_client_colorizer_set(graphics, MC_CLIENT_COLORIZER_FOLIAGE, empty, NULL, NULL) ==
          MC_CLIENT_COLORIZER_OK);
    CHECK(color_metadata(&fixture, 0) && color_particle(p, p->base.world, &particle, e));
    CHECK(mc_digging_particle_tint_entered(particle, engine, 8, 64, 8, d, e) ==
              MC_ENTITY_STATE_ERROR &&
          d->colorizer.array.status == MC_CLIENT_COLOR_ARRAY_INDEX_OUT_OF_RANGE &&
          e->source_exception &&
          strcmp(e->exception_class, "java/lang/ArrayIndexOutOfBoundsException") == 0 &&
          particle->particle->fields.i == .6f && particle->particle->fields.j == .6f &&
          particle->particle->fields.k == .6f);
    mc_entity_release(particle);
    particle = NULL;
    CHECK(color_metadata(&fixture, 2) && color_particle(p, p->base.world, &particle, e));
    const unsigned int controls = _mm_getcsr();
    _mm_setcsr(controls | 0x2000u);
    const mc_entity_status fp = mc_digging_particle_tint_entered(particle, engine, 8, 64, 8, d, e);
    _mm_setcsr(controls);
    CHECK(fp == MC_ENTITY_UNSUPPORTED_FP && e->native_policy &&
          strcmp(e->source_class, "qm") == 0 && e->source_pc == 45 && d->value == 8431445 &&
          particle->particle->fields.i == .6f && particle->particle->fields.j == .6f &&
          particle->particle->fields.k == .6f);
    mc_entity_release(particle);
    CHECK(mc_client_colorizer_set(graphics, MC_CLIENT_COLORIZER_FOLIAGE, palette, NULL, NULL) ==
          MC_CLIENT_COLORIZER_OK);
    CHECK(color_captured_world(p, engine, e, d));
    CHECK(p->base.world->fields.r->state == world_random);
    CHECK(mc_client_color_array_release(&palette, NULL) == MC_CLIENT_COLOR_ARRAY_OK);
    CHECK(mc_client_color_array_release(&empty, NULL) == MC_CLIENT_COLOR_ARRAY_OK);
    CHECK(mc_client_render_engine_release(engine) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_default_font_release(font) == MC_CLIENT_FONT_OK);
    CHECK(mc_game_settings_retire(&settings, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_client_render_engine_classes_release(graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    free(d);
    free(e);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 1;
    picking_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !picking_open(f, argv[1]) || !picking_case_open(f, argv[2]) ||
        !colors_observe(f) || !color_particle_controls(f) || !picking_case_close(f) ||
        !picking_close(f))
        return 1;
    free(f);
    printf("%u source Block color checks passed\n", checks);
    return 0;
}
