/* Optional typed EW CPU benchmark. Only the actual getter, shared dispatch,
 * output store and status loop are timed. All authoring, paired controls,
 * assertions, normalization, hashes, snapshots and I/O stay outside samples. */
#define MC_WORLD_QUERIES_ENTRY mc_ew_render_unused_world_benchmark
#include "world_queries.c"
#undef MC_WORLD_QUERIES_ENTRY
#include "chunk_cache_render_adapter.h"
#include "world/chunk/object_state_internal.h"
#include "world/runtime/chunk_cache_internal.h"
#include <stddef.h>

enum { ER_CALLS = 65536, ER_POINTS = 1024, ER_SAMPLES = 7, ER_MODES = 6 };

enum { ER_BRIGHTNESS, ER_MIN_BRIGHTNESS, ER_MATERIAL, ER_OPAQUE, ER_NORMAL_CUBE, ER_MANAGER };

static const char *const er_names[ER_MODES] = {"brightness", "min_brightness", "material",
                                               "opaque",     "normal_cube",    "manager"};

typedef struct er_point {
    int32_t x, y, z, minimum;
} er_point;

typedef struct er_value {
    float light;
    mc_material *material;
    mc_world_chunk_manager *manager;
    bool predicate;
} er_value;

static uint64_t er_hash(uint64_t h, const void *raw, size_t length) {
    const unsigned char *p = raw;
    for (size_t i = 0; i < length; ++i)
        h = (h ^ p[i]) * UINT64_C(1099511628211);
    return h;
}

static uint64_t er_mix(uint64_t h, uint64_t value) {
    /* Explicit little-endian scalar bytes, independent of pointer addresses. */
    for (unsigned int i = 0; i < 8; ++i) {
        h = (h ^ (value & 255u)) * UINT64_C(1099511628211);
        value >>= 8u;
    }
    return h;
}

static uint32_t er_float_bits(float value) {
    uint32_t out;
    memcpy(&out, &value, sizeof(out));
    return out;
}

static bool ew_text_equal(const char *a, const char *b) {
    return a == b || (a != NULL && b != NULL && strcmp(a, b) == 0);
}

/* These captured-cell getters reach no Entity/NBT descendant. The shared
 * comparator checks every active scalar, string, nested cause and identity. */
#include "chunk_cache_error_equal.inc"

static mc_client_chunk_cache_status er_call(const ew_render_adapter *a, unsigned int mode,
                                            mc_client_chunk_cache *cache, const er_point *p,
                                            bool output, er_value *value,
                                            mc_client_chunk_cache_error *error) {
    switch (mode) {
        case ER_BRIGHTNESS:
            return a->brightness(cache, p->x, p->y, p->z, output ? &value->light : NULL, error);
        case ER_MIN_BRIGHTNESS:
            return a->min_brightness(cache, p->x, p->y, p->z, p->minimum,
                                     output ? &value->light : NULL, error);
        case ER_MATERIAL:
            return a->material(cache, p->x, p->y, p->z, output ? &value->material : NULL, error);
        case ER_OPAQUE:
            return a->opaque(cache, p->x, p->y, p->z, output ? &value->predicate : NULL, error);
        case ER_NORMAL_CUBE:
            return a->normal_cube(cache, p->x, p->y, p->z, output ? &value->predicate : NULL,
                                  error);
        case ER_MANAGER:
            return a->manager(cache, output ? &value->manager : NULL, error);
        default:
            return MC_CLIENT_CHUNK_CACHE_INVALID_ARGUMENT;
    }
}

static const ew_render_adapter er_public = {mc_client_chunk_cache_brightness,
                                            mc_client_chunk_cache_min_brightness,
                                            mc_client_chunk_cache_material,
                                            mc_client_chunk_cache_opaque,
                                            mc_client_chunk_cache_normal_cube,
                                            mc_client_chunk_cache_manager,
                                            true,
                                            "public"};

static bool er_value_equal(unsigned int mode, const er_value *a, const er_value *b) {
    if (mode <= ER_MIN_BRIGHTNESS)
        return er_float_bits(a->light) == er_float_bits(b->light);
    if (mode == ER_MATERIAL)
        return a->material == b->material;
    if (mode == ER_MANAGER)
        return a->manager == b->manager;
    return a->predicate == b->predicate;
}

static int er_normalize(source_save_fixture *s, unsigned int mode, const er_value *value,
                        mc_world_chunk_manager *manager, uint64_t *out) {
    if (mode <= ER_MIN_BRIGHTNESS)
        *out = er_float_bits(value->light);
    else if (mode == ER_MATERIAL) {
        *out = 0;
        if (value->material != NULL) {
            unsigned int i = 0;
            for (; i < MC_MATERIAL_COUNT; ++i)
                if (mc_materials_get(s->game.materials, (mc_material_id)i) == value->material)
                    break;
            VERIFY(i != MC_MATERIAL_COUNT);
            *out = (uint64_t)i + 1u;
        }
    } else if (mode == ER_MANAGER) {
        VERIFY(value->manager == manager);
        *out = manager == NULL ? 0u : 1u;
    } else
        *out = value->predicate ? 1u : 0u;
    return 0;
}

static uint64_t er_planes(source_save_fixture *s) {
    uint64_t h = UINT64_C(1469598103934665603);
    const size_t count = mc_world_store_chunk_count(s->game.stores[0]);
    for (size_t i = 0; i < count; ++i) {
        mc_chunk_state *c = NULL;
        if (mc_world_store_chunk_at(s->game.stores[0], i, &c, NULL) != MC_WORLD_STORAGE_OK ||
            c == NULL || c->blank || c->data == NULL)
            return 0;
        const int64_t fields[] = {c->x,
                                  c->z,
                                  c->dirty,
                                  c->active,
                                  c->terrain_populated,
                                  c->never_save,
                                  c->has_entities,
                                  c->last_save_time};
        h = er_hash(h, fields, sizeof(fields));
        h = er_hash(h, c->data->blocks, MC_CHUNK_BLOCKS);
        h = er_hash(h, c->data->metadata, MC_CHUNK_NIBBLE_BYTES);
        h = er_hash(h, c->data->skylight, MC_CHUNK_NIBBLE_BYTES);
        h = er_hash(h, c->data->block_light, MC_CHUNK_NIBBLE_BYTES);
        h = er_hash(h, c->data->heightmap, MC_CHUNK_HEIGHTMAP_BYTES);
    }
    return h;
}

static bool er_provider_equal(const mc_chunk_provider_cache_observation *a,
                              const mc_chunk_provider_cache_observation *b) {
    return a->owned == b->owned && a->map_count == b->map_count &&
           a->insertion_count == b->insertion_count && a->index_capacity == b->index_capacity &&
           a->index_bytes == b->index_bytes && a->mutation_depth == b->mutation_depth &&
           a->direct_bytes == b->direct_bytes && a->hits == b->hits && a->misses == b->misses &&
           a->generic_gets == b->generic_gets && a->raw_exports == b->raw_exports &&
           a->rebuilds == b->rebuilds;
}

static bool er_dimension_equal(const mc_world_provider_state *a, const mc_world_provider_state *b) {
    return a->kind == b->kind && a->phase == b->phase && a->constructor_pc == b->constructor_pc &&
           a->bind_pc == b->bind_pc && a->setup_pc == b->setup_pc && a->holds == b->holds &&
           a->direct_allocation_bytes == b->direct_allocation_bytes;
}

static bool er_dimension_fields_equal(const mc_world_provider_fields *a,
                                      const mc_world_provider_fields *b) {
    return a->a == b->a && a->b == b->b && a->c == b->c && a->d == b->d && a->e == b->e &&
           a->f == b->f && a->g == b->g && a->h == b->h;
}

static bool er_manager_equal(const mc_world_chunk_manager_fields *a,
                             const mc_world_chunk_manager_fields *b) {
    return a->fixed == b->fixed && a->noise_backed == b->noise_backed && a->a == b->a &&
           a->b == b->b && a->c == b->c && a->d == b->d && a->fixed_e == b->fixed_e &&
           a->fixed_f == b->fixed_f && a->fixed_g == b->fixed_g && a->holds == b->holds;
}

static bool er_classes_equal(const unsigned char *before, mc_world_runtime_classes *classes) {
    /* Compare actual unchanged allocation bytes, masking only the explicitly
     * admitted source lm.a flag. No owner is reconstructed or overwritten. */
    unsigned char copy[sizeof(*classes)];
    memcpy(copy, classes, sizeof(copy));
    const size_t offset = offsetof(mc_world_runtime_classes, chunk_a);
    memcpy(copy + offset, before + offset, sizeof(classes->chunk_a));
    return memcmp(copy, before, sizeof(copy)) == 0;
}

static int er_pair(source_save_fixture *s, const ew_render_adapter *a, unsigned int mode,
                   mc_client_chunk_cache *cache, const er_point *point, bool output,
                   bool initial_flag, int wanted, er_value *returned, bool *effect) {
    mc_world_runtime *world = s->game.worlds[0];
    const bool original_flag = world->classes->chunk_a;
    mc_client_chunk_cache_error *reference = malloc(sizeof(*reference)),
                                *actual = malloc(sizeof(*actual));
    unsigned char *sentinel = malloc(sizeof(*actual));
    unsigned char *owner = cache == NULL ? NULL : malloc(sizeof(*cache));
    unsigned char *world_before = malloc(sizeof(*world));
    unsigned char *classes_before = malloc(sizeof(*world->classes));
    VERIFY(reference != NULL && actual != NULL && sentinel != NULL && world_before != NULL &&
           classes_before != NULL && (cache == NULL || owner != NULL));
    memset(actual, 0xa5, sizeof(*actual));
    memcpy(sentinel, actual, sizeof(*actual));
    if (cache != NULL)
        memcpy(owner, cache, sizeof(*cache));
    memcpy(world_before, world, sizeof(*world));
    memcpy(classes_before, world->classes, sizeof(*world->classes));
    mc_world_provider_fields dimension = {0};
    VERIFY(mc_world_provider_fields_get(world->fields.t, &dimension) == MC_WORLD_PROVIDER_OK ||
           world->fields.t == NULL);
    const er_value unchanged = {.light = 777.25f,
                                .material = mc_materials_get(s->game.materials, MC_MATERIAL_B),
                                .manager = world->fields.t == NULL ? NULL : dimension.b,
                                .predicate = true};
    er_value expected = unchanged, value = unchanged;
    world->classes->chunk_a = initial_flag; /* Paired authored precondition, outside all timing. */
    const mc_client_chunk_cache_status public_status =
        er_call(&er_public, mode, cache, point, output, &expected, reference);
    const bool public_effect = world->classes->chunk_a;
    VERIFY((cache == NULL || memcmp(owner, cache, sizeof(*cache)) == 0) &&
           memcmp(world_before, world, sizeof(*world)) == 0 &&
           er_classes_equal(classes_before, world->classes));
    world->classes->chunk_a = initial_flag; /* Same source prestate for the second entered body. */
    const mc_client_chunk_cache_status status =
        er_call(a, mode, cache, point, output, &value, actual);
    VERIFY(status == public_status && (wanted < 0 || (int)status == wanted) &&
           er_value_equal(mode, &expected, &value) && world->classes->chunk_a == public_effect &&
           (cache == NULL || memcmp(owner, cache, sizeof(*cache)) == 0) &&
           memcmp(world_before, world, sizeof(*world)) == 0 &&
           er_classes_equal(classes_before, world->classes));
    VERIFY(status == MC_CLIENT_CHUNK_CACHE_OK || er_value_equal(mode, &value, &unchanged));
    VERIFY((status != MC_CLIENT_CHUNK_CACHE_OK || a->full_success_error)
               ? ew_error_equal(actual, reference)
               : memcmp(actual, sentinel, sizeof(*actual)) == 0);
    world->classes->chunk_a = original_flag; /* Restore only the declared test precondition. */
    if (returned != NULL)
        *returned = expected;
    if (effect != NULL)
        *effect = public_effect;
    free(classes_before);
    free(world_before);
    free(owner);
    free(sentinel);
    free(actual);
    free(reference);
    return 0;
}

static int er_controls(source_save_fixture *s, const ew_render_adapter *a,
                       mc_client_chunk_cache *cache) {
    const er_point p = {0, 64, 0, 7};
    for (unsigned int mode = 0; mode < ER_MODES; ++mode) {
        VERIFY(er_pair(s, a, mode, NULL, &p, true, false, MC_CLIENT_CHUNK_CACHE_INVALID_ARGUMENT,
                       NULL, NULL) == 0 &&
               er_pair(s, a, mode, cache, &p, false, false, MC_CLIENT_CHUNK_CACHE_INVALID_ARGUMENT,
                       NULL, NULL) == 0);
        cache->state.active = true;
        VERIFY(er_pair(s, a, mode, cache, &p, true, false, MC_CLIENT_CHUNK_CACHE_BUSY, NULL,
                       NULL) == 0);
        cache->state.active = false;
        cache->state.retiring = true;
        VERIFY(er_pair(s, a, mode, cache, &p, true, false, MC_CLIENT_CHUNK_CACHE_BUSY, NULL,
                       NULL) == 0);
        cache->state.retiring = false;
        mc_world_runtime *saved_world = cache->state.d;
        cache->state.d = NULL;
        VERIFY(er_pair(s, a, mode, cache, &p, true, false, -1, NULL, NULL) == 0);
        cache->state.d = saved_world;
        mc_world_provider *dimension = saved_world->fields.t;
        saved_world->fields.t = NULL;
        VERIFY(er_pair(s, a, mode, cache, &p, true, false, -1, NULL, NULL) == 0);
        saved_world->fields.t = dimension;
        static const er_point boundaries[] = {{0, -1, 0, 0},         {0, 128, 0, 0},
                                              {-17, 64, 0, 0},       {32, 64, 0, 0},
                                              {INT32_MIN, 64, 0, 0}, {INT32_MAX, 64, 0, 0}};
        for (size_t i = 0; i < COUNT(boundaries); ++i)
            VERIFY(er_pair(s, a, mode, cache, &boundaries[i], true, false, -1, NULL, NULL) == 0);
        VERIFY(er_pair(s, a, mode, cache, &p, true, true, MC_CLIENT_CHUNK_CACHE_OK, NULL, NULL) ==
               0);
    }
    mc_world_chunk *chunk = cache->occurrences[4].object;
    VERIFY(chunk != NULL && chunk->f != NULL && chunk->f->a != NULL && chunk->g != NULL &&
           chunk->g->a != NULL);
    uint8_t *backing = chunk->g->a;
    chunk->g->a = NULL;
    for (unsigned int mode = 0; mode <= ER_MIN_BRIGHTNESS; ++mode) {
        bool effect = false;
        VERIFY(er_pair(s, a, mode, cache, &p, true, false, MC_CLIENT_CHUNK_CACHE_SOURCE_NULL, NULL,
                       &effect) == 0 &&
               effect);
    }
    chunk->g->a = backing;
    backing = chunk->f->a;
    chunk->f->a = NULL;
    for (unsigned int mode = 0; mode <= ER_MIN_BRIGHTNESS; ++mode)
        VERIFY(er_pair(s, a, mode, cache, &p, true, false, MC_CLIENT_CHUNK_CACHE_SOURCE_NULL, NULL,
                       NULL) == 0);
    chunk->f->a = backing;
    er_point invalid = p;
    invalid.minimum = 16;
    bool effect = false;
    VERIFY(er_pair(s, a, ER_MIN_BRIGHTNESS, cache, &invalid, true, false,
                   MC_CLIENT_CHUNK_CACHE_SOURCE_INDEX, NULL, &effect) == 0 &&
           effect);
    const mc_block *stone = NULL;
    VERIFY(mc_registry_block_get(s->game.registry[0], 1, &stone, NULL) == MC_REGISTRY_OK &&
           stone != NULL);
    mc_material *saved_material = ((mc_block *)stone)->fields.bA;
    ((mc_block *)stone)->fields.bA = NULL;
    VERIFY(er_pair(s, a, ER_MATERIAL, cache, &p, true, false, MC_CLIENT_CHUNK_CACHE_OK, NULL,
                   NULL) == 0 &&
           er_pair(s, a, ER_NORMAL_CUBE, cache, &p, true, false, MC_CLIENT_CHUNK_CACHE_SOURCE_NULL,
                   NULL, NULL) == 0);
    ((mc_block *)stone)->fields.bA = saved_material;
    return 0;
}

typedef struct er_guard {
    mc_client_chunk_cache owner;
    mc_client_chunk_cache_occurrence occurrences[9];
    mc_world_chunk chunks[9];
    mc_chunk_state states[9];
    unsigned char world[sizeof(mc_world_runtime)], classes[sizeof(mc_world_runtime_classes)];
    unsigned char random[sizeof(mc_java_random)];
    uint64_t planes, unique, math_state, math_gaussian;
    unsigned int holder_phase;
    mc_java_math_random_owner *math;
    bool math_have;
    mc_chunk_provider_cache_observation provider;
    mc_world_provider_fields dimension;
    mc_world_provider_state dimension_state;
    mc_world_chunk_manager_fields manager;
    float light_table[16];
} er_guard;

static int er_guard_take(source_save_fixture *s, mc_client_chunk_cache *cache, er_guard *g) {
    mc_world_runtime *world = s->game.worlds[0];
    memcpy(&g->owner, cache, sizeof(g->owner));
    memcpy(g->occurrences, cache->occurrences, sizeof(g->occurrences));
    for (size_t i = 0; i < 9; ++i) {
        memcpy(&g->chunks[i], cache->occurrences[i].object, sizeof(g->chunks[i]));
        memcpy(&g->states[i], cache->occurrences[i].state, sizeof(g->states[i]));
    }
    memcpy(g->world, world, sizeof(g->world));
    memcpy(g->classes, world->classes, sizeof(g->classes));
    memcpy(g->random, world->fields.r, sizeof(g->random));
    g->planes = er_planes(s);
    g->unique = atomic_load(&s->game.profile->uniquifier);
    g->holder_phase = atomic_load(&s->game.profile->holder_phase);
    g->math = s->game.profile->math;
    g->math_state = g->math == NULL ? 0 : atomic_load(&g->math->state);
    g->math_gaussian = 0;
    if (g->math != NULL)
        memcpy(&g->math_gaussian, &g->math->next_next_gaussian, sizeof(g->math_gaussian));
    g->math_have = g->math != NULL && g->math->have_next_next_gaussian;
    VERIFY(g->planes != 0 && mc_chunk_provider_cache_observe(world->fields.v, &g->provider) &&
           mc_world_provider_fields_get(world->fields.t, &g->dimension) == MC_WORLD_PROVIDER_OK &&
           mc_world_provider_state_get(world->fields.t, &g->dimension_state) ==
               MC_WORLD_PROVIDER_OK &&
           g->dimension.f != NULL && g->dimension.b != NULL &&
           mc_world_chunk_manager_fields_get(g->dimension.b, &g->manager) == MC_WORLD_PROVIDER_OK);
    memcpy(g->light_table, g->dimension.f, sizeof(g->light_table));
    return 0;
}

static int er_guard_check(source_save_fixture *s, mc_client_chunk_cache *cache, const er_guard *g) {
    mc_world_runtime *world = s->game.worlds[0];
    mc_chunk_provider_cache_observation provider;
    mc_world_provider_fields dimension = {0};
    mc_world_provider_state state;
    mc_world_chunk_manager_fields manager;
    VERIFY(memcmp(cache, &g->owner, sizeof(g->owner)) == 0 &&
           memcmp(cache->occurrences, g->occurrences, sizeof(g->occurrences)) == 0 &&
           memcmp(world, g->world, sizeof(g->world)) == 0 &&
           er_classes_equal(g->classes, world->classes) &&
           memcmp(world->fields.r, g->random, sizeof(g->random)) == 0 &&
           er_planes(s) == g->planes && atomic_load(&s->game.profile->uniquifier) == g->unique &&
           atomic_load(&s->game.profile->holder_phase) == g->holder_phase &&
           s->game.profile->math == g->math &&
           mc_chunk_provider_cache_observe(world->fields.v, &provider) &&
           er_provider_equal(&g->provider, &provider) &&
           mc_world_provider_fields_get(world->fields.t, &dimension) == MC_WORLD_PROVIDER_OK &&
           er_dimension_fields_equal(&g->dimension, &dimension) &&
           mc_world_provider_state_get(world->fields.t, &state) == MC_WORLD_PROVIDER_OK &&
           er_dimension_equal(&g->dimension_state, &state) &&
           mc_world_chunk_manager_fields_get(dimension.b, &manager) == MC_WORLD_PROVIDER_OK &&
           er_manager_equal(&g->manager, &manager) &&
           memcmp(dimension.f, g->light_table, sizeof(g->light_table)) == 0);
    for (size_t i = 0; i < 9; ++i)
        VERIFY(memcmp(cache->occurrences[i].object, &g->chunks[i], sizeof(g->chunks[i])) == 0 &&
               memcmp(cache->occurrences[i].state, &g->states[i], sizeof(g->states[i])) == 0);
    if (g->math != NULL) {
        uint64_t gaussian;
        memcpy(&gaussian, &g->math->next_next_gaussian, sizeof(gaussian));
        VERIFY(atomic_load(&g->math->state) == g->math_state && gaussian == g->math_gaussian &&
               g->math->have_next_next_gaussian == g->math_have);
    }
    return 0;
}

static int er_authored_cells(source_save_fixture *s, mc_client_chunk_cache *cache) {
    static const uint8_t ids[] = {0, 1, 20, 18, 44, 60, 53, 67, 55, 50, 69, 8};
    for (size_t i = 0; i < COUNT(ids); ++i) {
        const mc_block *block = NULL;
        VERIFY(mc_registry_block_get(s->game.registry[0], ids[i], &block, NULL) == MC_REGISTRY_OK);
        VERIFY(ids[i] == 0 || block != NULL);
    }
    for (size_t cell = 0; cell < 9; ++cell) {
        mc_world_chunk *chunk = cache->occurrences[cell].object;
        VERIFY(chunk != NULL && chunk->data != NULL && chunk->data->blocks != NULL &&
               chunk->e != NULL && chunk->f != NULL && chunk->g != NULL &&
               chunk->e->length == MC_CHUNK_NIBBLE_BYTES &&
               chunk->f->length == MC_CHUNK_NIBBLE_BYTES &&
               chunk->g->length == MC_CHUNK_NIBBLE_BYTES);
        for (size_t index = 0; index < MC_CHUNK_BLOCKS; ++index)
            chunk->data->blocks[index] = ids[(index * 7u + cell * 5u) % COUNT(ids)];
        for (size_t index = 0; index < MC_CHUNK_NIBBLE_BYTES; ++index) {
            chunk->e->a[index] = (uint8_t)(index * 37u + cell * 13u);
            chunk->f->a[index] = (uint8_t)(index * 11u + cell * 7u);
            chunk->g->a[index] = (uint8_t)(index * 19u + cell * 3u);
        }
    }
    /* One exact ordinary Stone cell gives failure controls a reached positive
     * SkyLight store before a deliberately missing BlockLight child. */
    mc_world_chunk *chunk = cache->occurrences[4].object;
    chunk->data->blocks[64] = 1;
    chunk->f->a[32] = (uint8_t)((chunk->f->a[32] & 240u) | 15u);
    chunk->g->a[32] = (uint8_t)(chunk->g->a[32] & 240u);
    return 0;
}

static int er_benchmark(source_save_fixture *s, bool measure) {
    const ew_render_adapter *a = ew_render_adapter_get();
    VERIFY(a != NULL && a->brightness != NULL && a->min_brightness != NULL && a->material != NULL &&
           a->opaque != NULL && a->normal_cube != NULL && a->manager != NULL);
    mc_world_runtime *world = s->game.worlds[0];
    mc_client_chunk_cache *cache = NULL;
    VERIFY(mc_client_chunk_cache_create(world, -16, 0, -16, 31, 127, 31, &cache, NULL) ==
               MC_CLIENT_CHUNK_CACHE_OK &&
           cache != NULL && cache->state.acquired == 9 && cache->state.leased == 9 &&
           cache->cell_count == 9 && cache->state.returned);
    VERIFY(er_authored_cells(s, cache) == 0);
    er_point *points = calloc(ER_POINTS, sizeof(*points));
    er_value *expected = calloc(ER_MODES * ER_POINTS, sizeof(*expected)),
             *values = calloc(ER_CALLS, sizeof(*values));
    mc_client_chunk_cache_error *error = malloc(sizeof(*error)),
                                *reference = malloc(sizeof(*reference));
    unsigned char *sentinel = malloc(sizeof(*error));
    er_guard *guard = malloc(sizeof(*guard));
    VERIFY(points != NULL && expected != NULL && values != NULL && error != NULL &&
           reference != NULL && sentinel != NULL && guard != NULL);
    const bool original_flag = world->classes->chunk_a;
    const uint64_t before_planes = er_planes(s);
    bool effects[ER_MODES] = {false};
    for (unsigned int i = 0; i < ER_POINTS; ++i) {
        const unsigned int cx = (i * 5u) % 3u, cz = (i * 7u / 3u) % 3u;
        points[i] =
            (er_point){((int32_t)cx - 1) * 16 + 2 + (int32_t)((i * 13u) % 12u),
                       8 + (int32_t)((i * 17u) % 104u),
                       ((int32_t)cz - 1) * 16 + 2 + (int32_t)((i * 11u) % 12u), (int32_t)(i % 8u)};
        for (unsigned int mode = 0; mode < ER_MODES; ++mode) {
            bool effect = false;
            VERIFY(er_pair(s, a, mode, cache, &points[i], true, false, MC_CLIENT_CHUNK_CACHE_OK,
                           &expected[mode * ER_POINTS + i], &effect) == 0);
            effects[mode] = effects[mode] || effect;
        }
    }
    VERIFY(er_controls(s, a, cache) == 0 && er_planes(s) == before_planes &&
           world->classes->chunk_a == original_flag && er_guard_take(s, cache, guard) == 0);
    uint64_t coordinates = UINT64_C(1469598103934665603);
    for (unsigned int i = 0; i < ER_POINTS; ++i) {
        coordinates = er_mix(coordinates, (uint32_t)points[i].x);
        coordinates = er_mix(coordinates, (uint32_t)points[i].y);
        coordinates = er_mix(coordinates, (uint32_t)points[i].z);
        coordinates = er_mix(coordinates, (uint32_t)points[i].minimum);
    }
    printf("EW_RENDER_CPU_CONTEXT adapter=%s success-error=%s error-bytes=%zu prefix-bytes=%zu "
           "registry-bytes=%zu "
           "calls=%u points=%u samples=%u chunks=9 planes=%" PRIu64 " coordinates=%" PRIu64 "\n",
           a->name, a->full_success_error ? "full" : "untouched", sizeof(*error),
           offsetof(mc_client_chunk_cache_error, storage), sizeof(error->registry), ER_CALLS,
           ER_POINTS, measure ? ER_SAMPLES : 0u, guard->planes, coordinates);
    for (unsigned int mode = 0; mode < ER_MODES; ++mode) {
        uint64_t expected_hash = UINT64_C(1469598103934665603);
        for (unsigned int i = 0; i < ER_CALLS; ++i) {
            uint64_t normalized;
            VERIFY(er_normalize(s, mode, &expected[mode * ER_POINTS + i % ER_POINTS],
                                guard->dimension.b, &normalized) == 0);
            expected_hash = er_mix(expected_hash, normalized);
        }
        printf("EW_RENDER_CPU_MODE mode=%s expected=%" PRIu64 " chunk-a-after=%d\n", er_names[mode],
               expected_hash, (int)effects[mode]);
        for (unsigned int sample = 0; measure && sample < ER_SAMPLES; ++sample) {
            world->classes->chunk_a = false; /* Identical explicit precondition for every sample. */
            memset(error, 0xa5, sizeof(*error));
            memcpy(sentinel, error, sizeof(*error));
            uint64_t start, end, hz, end_hz;
            const bool started = bench_clock(&start, &hz);
            mc_client_chunk_cache_status status = MC_CLIENT_CHUNK_CACHE_OK;
            unsigned int completed = 0;
            for (; completed < ER_CALLS; ++completed) {
                const er_point *p = &points[completed % ER_POINTS];
                status = er_call(a, mode, cache, p, true, &values[completed], error);
                if (status != MC_CLIENT_CHUNK_CACHE_OK)
                    break;
            }
            const bool ended = bench_clock(&end, &end_hz);
            VERIFY(started && ended && end > start && hz == end_hz &&
                   status == MC_CLIENT_CHUNK_CACHE_OK && completed == ER_CALLS);
            VERIFY(world->classes->chunk_a == effects[mode]);
            uint64_t result = UINT64_C(1469598103934665603);
            for (unsigned int i = 0; i < ER_CALLS; ++i) {
                uint64_t normalized;
                VERIFY(
                    er_value_equal(mode, &values[i], &expected[mode * ER_POINTS + i % ER_POINTS]) &&
                    er_normalize(s, mode, &values[i], guard->dimension.b, &normalized) == 0);
                result = er_mix(result, normalized);
            }
            VERIFY(result == expected_hash);
            if (a->full_success_error) {
                er_value last = {0};
                world->classes->chunk_a = false;
                VERIFY(er_call(&er_public, mode, cache, &points[(ER_CALLS - 1u) % ER_POINTS], true,
                               &last, reference) == MC_CLIENT_CHUNK_CACHE_OK &&
                       er_value_equal(mode, &last, &values[ER_CALLS - 1u]) &&
                       ew_error_equal(error, reference));
            } else
                VERIFY(memcmp(error, sentinel, sizeof(*error)) == 0);
            world->classes->chunk_a =
                original_flag; /* Only after checking the actual source flag effect. */
            VERIFY(er_guard_check(s, cache, guard) == 0);
            printf("EW_RENDER_CPU_SAMPLE mode=%s sample=%u ticks=%" PRIu64 " frequency=%" PRIu64
                   " calls=%u "
                   "result=%" PRIu64 " planes=%" PRIu64
                   " chunk-a-before=0 chunk-a-after=%d owners-same=1 "
                   "world-same=1 classes-except-chunk-a-same=1 random-same=1 holds-same=1 "
                   "acquisition-gets=0\n",
                   er_names[mode], sample, end - start, hz, completed, result, guard->planes,
                   (int)effects[mode]);
        }
    }
    world->classes->chunk_a = original_flag;
    VERIFY(er_guard_check(s, cache, guard) == 0);
    const size_t bindings = world->persistent_bindings;
    VERIFY(mc_client_chunk_cache_retire(&cache, NULL) == MC_CLIENT_CHUNK_CACHE_OK &&
           cache == NULL && world->persistent_bindings + 1u == bindings);
    free(guard);
    free(sentinel);
    free(reference);
    free(error);
    free(values);
    free(expected);
    free(points);
    return 0;
}

int main(int argc, char **argv) {
    VERIFY(argc == 3 && (strcmp(argv[2], "measure") == 0 || strcmp(argv[2], "controls") == 0));
    source_save_fixture *s = calloc(1, sizeof(*s));
    VERIFY(s != NULL && ss_setup(s, argv[1]) == 0 && bench_open(s) == 0);
    const bool measure = strcmp(argv[2], "measure") == 0;
    VERIFY(er_benchmark(s, measure) == 0 && bench_stop(s) == 0 && ss_finish(s) == 0);
    printf("EW_RENDER_CPU_FINISH checks=%u cleanup=1 genuine-world=1 genuine-ew=1 modes=6 "
           "measure=%d\n",
           checks, (int)measure);
    free(s);
    return 0;
}
