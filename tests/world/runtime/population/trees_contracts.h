/* Include fragment for tests/world/runtime/population/trees_contracts.h.
 * Include after the real fixture/helpers in test_queries.c, before main.
 * Call population_tree_contracts(f, side) after query/plant predicate setup
 * and BEFORE population_plant_contracts exhausts provider publication capacity.
 * Source-derived native contracts over actual GAME/Registry/Chunk owners;
 * prepared arrays, ordinary seeded input streams and nullable references are
 * labeled controls. No native-only result is original parity evidence. */
#include "../../../../src/core/java/random_profile_internal.h"
#include "../../../../src/world/runtime/population/tree_internal.h"
#include "../../../support/java_random.h"
#include "mc/world/runtime/population/trees.h"
#include <emmintrin.h>
#include <math.h>

typedef struct tt_profile {
    uint64_t unique, math_state, math_cache;
    unsigned int phase;
    mc_java_math_random_owner *math;
    size_t allocated;
    mc_platform_nanotime clock;
    bool math_cached;
} tt_profile;

typedef struct tt_saved {
    mc_chunk_data data;
    uint8_t blocks[MC_CHUNK_BLOCKS];
    mc_world_chunk_nibble *e, *f, *g;
    uint8_t *h;
    mc_block **m, *elements[MC_REGISTRY_BLOCK_SLOTS], *named[96];
    bool *o, opacity[MC_REGISTRY_BLOCK_SLOTS];
} tt_saved;

typedef enum tt_control {
    TT_FULL,
    TT_DIRT_ALREADY,
    TT_HEIGHT_REJECT,
    TT_CLEARANCE_REJECT,
    TT_NULL_RANDOM,
    TT_NULL_CLEARANCE_LEAF,
    TT_NULL_GRASS,
    TT_NULL_DIRT_COMPARE,
    TT_NULL_DIRT_WRITE,
    TT_NULL_OPACITY,
    TT_NULL_LEAF_WRITE,
    TT_NULL_METADATA,
    TT_LATE_LOG_HOOK
} tt_control;

typedef enum tt_budget { TT_INITIAL, TT_FIRST_LEAF, TT_COMPLETE } tt_budget;

typedef struct tt_draws {
    int32_t height, trunk, foliage_start, maximum_radius;
    unsigned int corners;
} tt_draws;

static const unsigned int tt_clearance_pc[] = {141, 141, 171, 165};
static const unsigned int tt_clearance_field[] = {156, 156, 186, 180};
static const unsigned int tt_grass_field[] = {214, 214, 244, 238};
static const unsigned int tt_dirt_compare_field[] = {225, 225, 255, 249};
static const unsigned int tt_dirt_write_field[] = {257, 257, 287, 281};
static const unsigned int tt_soil_set_pc[] = {260, 260, 290, 284};
static const unsigned int tt_opacity_pc[] = {395, 395, 400, 407};
static const unsigned int tt_leaf_field[] = {409, 409, 414, 421};
static const unsigned int tt_trunk_set_pc[] = {489, 491, 532, 550};
static const char *const tt_client[] = {"yh", "k", "pw", "ws"};
static const char *const tt_server[] = {"pn", "h", "jt", "on"};

static uint64_t tt_double_bits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static tt_profile tt_profile_read(const mc_java_random_profile *p) {
    tt_profile s = {.unique = atomic_load_explicit(&p->uniquifier, memory_order_relaxed),
                    .phase = atomic_load_explicit(&p->holder_phase, memory_order_acquire),
                    .math = p->math,
                    .allocated = p->allocated_bytes,
                    .clock = p->clock};
    if (s.math != NULL) {
        s.math_state = atomic_load_explicit(&s.math->state, memory_order_relaxed);
        s.math_cache = tt_double_bits(s.math->next_next_gaussian);
        s.math_cached = s.math->have_next_next_gaussian;
    }
    return s;
}

static bool tt_profile_is(const mc_java_random_profile *p, const tt_profile *before,
                          unsigned int constructors) {
    tt_profile after = tt_profile_read(p);
    uint64_t expected = before->unique;
    for (unsigned int i = 0; i < constructors; ++i)
        expected *= UINT64_C(1181783497276652981);
    return after.unique == expected && after.phase == before->phase && after.math == before->math &&
           after.math_state == before->math_state && after.math_cache == before->math_cache &&
           after.math_cached == before->math_cached && after.allocated == before->allocated &&
           after.clock.frequency == before->clock.frequency &&
           after.clock.initial_sample == before->clock.initial_sample &&
           after.clock.initialized == before->clock.initialized;
}

static mc_registry_error tt_empty_registry(void) {
    return (mc_registry_error){.initializer_pc = -1,
                               .block_id = -1,
                               .constructor_pc = -1,
                               .active_initializer_pc = -1,
                               .item_argument = -1};
}

static bool tt_cleared(const mc_world_tree_error *e) {
    const mc_registry_error registry = tt_empty_registry();
    return e->status == MC_WORLD_STORAGE_OK && e->operation == NULL && e->feature_class == NULL &&
           e->method == NULL && e->bytecode == 0 && e->entry_bytecode == 0 && !e->native_boundary &&
           e->cause.status == MC_WORLD_STORAGE_OK && e->cause.operation == NULL &&
           e->cause.system_error == 0 && e->profile_status == MC_JAVA_RANDOM_PROFILE_OK &&
           same_registry(&e->registry, &registry) &&
           same_registry(&e->mutation.registry, &registry) &&
           e->mutation.status == MC_WORLD_STORAGE_OK && e->mutation.operation == NULL;
}

/* Read the actual production-private definition, shared with trees.c. No
 * copied feature layout, inspector backend, private field write or clock input. */
static bool tt_big_quiescent(const mc_world_tree_big_oak *tree) {
    return tree != NULL && !tree->entered && tree->context == NULL;
}

static bool tt_big_fresh(const mc_world_tree_big_oak *tree, mc_world_runtime *constructor_world,
                         size_t limit) {
    return tt_big_quiescent(tree) && tree->classes == constructor_world->classes &&
           tree->world == NULL && tree->base[0] == 0 && tree->base[1] == 0 && tree->base[2] == 0 &&
           tree->height_limit == 0 && tree->trunk_height == 0 &&
           tree->height_attenuation == 0.618 && tree->unused_h == 1.0 &&
           tree->branch_slope == 0.381 && tree->width_scale == 1.0 && tree->leaf_density == 1.0 &&
           tree->trunk_size == 1 && tree->height_random_limit == 12 && tree->leaf_distance == 4 &&
           tree->nodes == NULL && tree->node_count == 0 &&
           tree->allocation_bytes == sizeof(*tree) && tree->max_allocation_bytes == limit &&
           !tree->random.have_next_next_gaussian &&
           tt_double_bits(tree->random.next_next_gaussian) == UINT64_C(0);
}

static size_t tt_index(int32_t x, int32_t y, int32_t z) {
    return ((((size_t)(uint32_t)x & 15u) * 16u) + ((uint32_t)z & 15u)) * 128u + (size_t)y;
}

static mc_block **tt_named(mc_registry_classes *r, const char *name) {
    for (size_t i = 0; i < COUNT(r->blocks.named); ++i) {
        const char *n = mc_registry_block_static_name(i);
        if (n != NULL && strcmp(n, name) == 0)
            return &r->blocks.named[i];
    }
    return NULL;
}

static bool tt_id(fixture *f, size_t side, const char *name, int32_t *id) {
    const mc_block *b = NULL;
    if (mc_registry_block_static_get(f->registry[side], name, &b, NULL) != MC_REGISTRY_OK ||
        b == NULL)
        return false;
    *id = b->fields.bn;
    return *id > 0 && *id <= UINT8_MAX;
}

static bool tt_prepare(tt_saved *s, mc_world_chunk *c, mc_registry_classes *r, int32_t soil) {
    if (c == NULL || c->data == NULL || c->data->blocks == NULL ||
        c->data->blocks_capacity < MC_CHUNK_BLOCKS || c->e == NULL || c->f == NULL ||
        c->g == NULL || c->e->a != c->data->metadata || c->f->a != c->data->skylight ||
        c->g->a != c->data->block_light || c->h != c->data->heightmap || r->blocks.m == NULL ||
        r->blocks.o == NULL)
        return false;
    s->data = *c->data;
    memcpy(s->blocks, c->data->blocks, sizeof(s->blocks));
    s->e = c->e;
    s->f = c->f;
    s->g = c->g;
    s->h = c->h;
    s->m = r->blocks.m;
    s->o = r->blocks.o;
    memcpy(s->elements, r->blocks.m, sizeof(s->elements));
    memcpy(s->named, r->blocks.named, sizeof(s->named));
    memcpy(s->opacity, r->blocks.o, sizeof(s->opacity));
    /* Prepared native inputs, not naturally generated terrain. Keep genuine
     * loaded Chunk/cache/provider/World identities and all owner counts. */
    memset(c->data->blocks, 0, MC_CHUNK_BLOCKS);
    memset(c->data->metadata, 0xaa, MC_CHUNK_NIBBLE_BYTES);
    memset(c->data->skylight, 0xff, MC_CHUNK_NIBBLE_BYTES);
    memset(c->data->block_light, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(c->data->heightmap, 64, MC_CHUNK_HEIGHTMAP_BYTES);
    c->data->minimum_height = 64;
    for (int32_t x = 0; x < 16; ++x)
        for (int32_t z = 0; z < 16; ++z)
            c->data->blocks[tt_index(x, 63, z)] = (uint8_t)soil;
    return true;
}

static void tt_restore(const tt_saved *s, mc_world_chunk *c, mc_registry_classes *r) {
    *c->data = s->data;
    memcpy(c->data->blocks, s->blocks, sizeof(s->blocks));
    c->e = s->e;
    c->f = s->f;
    c->g = s->g;
    c->h = s->h;
    r->blocks.m = s->m;
    r->blocks.o = s->o;
    memcpy(r->blocks.m, s->elements, sizeof(s->elements));
    memcpy(r->blocks.named, s->named, sizeof(s->named));
    memcpy(r->blocks.o, s->opacity, sizeof(s->opacity));
    /* Restore prepared bytes/references only. Reached lighting queues, dirty
     * flags, diagnostics, bindings and Random continuations are retained. */
}

static bool tt_stale_registry(mc_world_runtime *w, mc_world_chunk *c, mc_registry_classes *r) {
    const size_t index = tt_index(8, 64, 8);
    const uint8_t old = c->data->blocks[index];
    mc_block **m = r->blocks.m;
    c->data->blocks[index] = 1;
    r->blocks.m = NULL;
    mc_material *value = NULL;
    mc_world_material_error e;
    const mc_world_storage_status status =
        mc_world_runtime_material_get_detailed(w, 8, 64, 8, &value, &e);
    r->blocks.m = m;
    c->data->blocks[index] = old;
    return status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && value == NULL && e.bytecode == 23 &&
           e.registry.status == MC_REGISTRY_NULL_REFERENCE;
}

static void tt_random(mc_java_random *r, int64_t seed) {
    mc_java_random_seed(r, seed);
    (void)mc_java_random_next_gaussian(r); /* Actual API prepares a valid cached continuation. */
}

/* This is an invocation-budget oracle, not a second tree/voxel algorithm.
 * Broadleaf has exactly sixteen corner invocations over four admitted layers.
 * Tall foliage has none. Dense's fourth/fifth draws straddle all foliage. */
static bool tt_expected(unsigned int kind, tt_budget budget, mc_java_random *r, tt_draws *d) {
    int32_t a, b;
    *d = (tt_draws){0};
    if (!mc_java_random_next_bounded(r, kind < 2 ? 3 : kind == 2 ? 5 : 4, &a))
        return false;
    d->height = a + (kind == 0 ? 4 : kind == 1 ? 5 : kind == 2 ? 7 : 6);
    d->trunk = d->height;
    d->maximum_radius = 2;
    if (kind == 2) {
        if (!mc_java_random_next_bounded(r, 2, &a))
            return false;
        d->foliage_start = d->height - a - 3;
        if (!mc_java_random_next_bounded(r, d->height - d->foliage_start + 1, &b))
            return false;
        d->maximum_radius = 1 + b;
        d->trunk = d->height - 1;
    } else if (kind == 3) {
        if (!mc_java_random_next_bounded(r, 2, &a) || !mc_java_random_next_bounded(r, 2, &b))
            return false;
        d->foliage_start = a + 1;
        d->maximum_radius = b + 2;
    }
    if (budget == TT_INITIAL)
        return true;
    if (kind < 2) {
        const unsigned int draws = budget == TT_COMPLETE ? 16u : 1u;
        for (unsigned int i = 0; i < draws; ++i) {
            if (!mc_java_random_next_bounded(r, 2, &a))
                return false;
            if (i < 12u && a != 0)
                ++d->corners;
        }
    } else if (kind == 3) {
        if (!mc_java_random_next_bounded(r, 2, &a))
            return false;
        if (budget == TT_COMPLETE) {
            if (!mc_java_random_next_bounded(r, 3, &a))
                return false;
            d->trunk = d->height - a;
        }
    }
    return true;
}

static mc_world_storage_status tt_small(unsigned int kind, mc_world_runtime *w, mc_java_random *r,
                                        int32_t y, bool *out, mc_world_tree_error *e) {
    switch (kind) {
        case 0:
            return mc_world_tree_oak(w, r, 8, y, 8, out, e);
        case 1:
            return mc_world_tree_birch(w, r, 8, y, 8, out, e);
        case 2:
            return mc_world_tree_taiga_tall(w, r, 8, y, 8, out, e);
        case 3:
            return mc_world_tree_taiga_dense(w, r, 8, y, 8, out, e);
    }
    return MC_WORLD_STORAGE_INVALID_ARGUMENT;
}

static bool tt_small_trial(fixture *f, size_t side, unsigned int kind, tt_control control,
                           int64_t seed, bool alias_world_random) {
    bool ok = false, prepared = false;
    tt_saved *saved = NULL;
    uint8_t *before = NULL;
    mc_world_runtime *w = f->worlds[side];
    mc_registry_classes *r = f->registry[side];
    mc_world_chunk *c = mc_world_store_chunk_object(f->stores[side], f->normal[side][0]);
    int32_t grass, dirt, leaves, log, stone;
    CHECK(kind < 4 && !w->fields.B && w->fields.r != NULL && tt_id(f, side, "v", &grass) &&
          tt_id(f, side, "w", &dirt) && tt_id(f, side, "L", &leaves) && tt_id(f, side, "K", &log) &&
          tt_id(f, side, "u", &stone));
    saved = malloc(sizeof(*saved));
    before = malloc(MC_CHUNK_BLOCKS);
    CHECK(saved != NULL && before != NULL && tt_prepare(saved, c, r, grass));
    prepared = true;
    CHECK(tt_stale_registry(w, c, r));
    const mc_registry_error prior = *mc_world_runtime_last_registry_error(w);
    const mc_registry_error empty = tt_empty_registry();
    const size_t origin = tt_index(8, 64, 8), below = tt_index(8, 63, 8);
    if (control == TT_DIRT_ALREADY)
        c->data->blocks[below] = (uint8_t)dirt;
    if (control == TT_CLEARANCE_REJECT || control == TT_NULL_CLEARANCE_LEAF)
        c->data->blocks[origin] = (uint8_t)stone;
    if (control == TT_NULL_DIRT_COMPARE)
        c->data->blocks[below] = (uint8_t)stone;
    if (control == TT_HEIGHT_REJECT) {
        /* Actual nullable references prove the height gate omits future use. */
        r->blocks.m = NULL;
        r->blocks.o = NULL;
        *tt_named(r, "v") = NULL;
        *tt_named(r, "w") = NULL;
        *tt_named(r, "L") = NULL;
    }
    if (control == TT_NULL_CLEARANCE_LEAF || control == TT_NULL_LEAF_WRITE)
        *tt_named(r, "L") = NULL;
    if (control == TT_NULL_GRASS)
        *tt_named(r, "v") = NULL;
    if (control == TT_NULL_DIRT_COMPARE || control == TT_NULL_DIRT_WRITE)
        *tt_named(r, "w") = NULL;
    if (control == TT_NULL_OPACITY)
        r->blocks.o = NULL;
    if (control == TT_NULL_METADATA)
        c->e = NULL;
    if (control == TT_LATE_LOG_HOOK)
        r->blocks.m[log] = NULL;
    memcpy(before, c->data->blocks, MC_CHUNK_BLOCKS);
    mc_java_random local, expected;
    mc_java_random *actual = alias_world_random ? w->fields.r : &local;
    tt_random(actual, seed);
    const mc_java_random world_random = *w->fields.r;
    expected = *actual;
    const bool full = control == TT_FULL || control == TT_DIRT_ALREADY;
    const bool first_leaf = control == TT_NULL_OPACITY || control == TT_NULL_LEAF_WRITE;
    const tt_budget budget = full || control == TT_LATE_LOG_HOOK ? TT_COMPLETE
                             : first_leaf                        ? TT_FIRST_LEAF
                                                                 : TT_INITIAL;
    tt_draws d;
    if (control == TT_NULL_RANDOM)
        d = (tt_draws){0};
    else
        CHECK(tt_expected(kind, budget, &expected, &d));
    const size_t cache_count = f->providers[side].cache->count;
    const size_t inserted = f->providers[side].cache->insertion_count;
    const size_t bindings = w->persistent_bindings;
    const tt_profile profile = tt_profile_read(f->profile);
    const bool initial = (kind & 1u) != 0;
    bool out = initial;
    mc_world_tree_error e = {0};
    const mc_world_storage_status status =
        tt_small(kind, w, control == TT_NULL_RANDOM ? NULL : actual,
                 control == TT_HEIGHT_REJECT ? 0 : 64, &out, &e);
    const bool continued = mc_test_java_random_equal(actual, &expected);
    const bool world_continued =
        alias_world_random || mc_test_java_random_equal(&world_random, w->fields.r);
    const bool globals = tt_profile_is(f->profile, &profile, 0);
    const bool registry_retained = same_registry(&prior, mc_world_runtime_last_registry_error(w));
    const bool retained_cause = same_cause(&e.cause, mc_world_runtime_last_error(w));
    size_t leaf_count = 0, log_count = 0, changes = 0;
    bool bounds = true, species = true, column = true;
    for (int32_t x = 0; x < 16; ++x)
        for (int32_t z = 0; z < 16; ++z)
            for (int32_t y = 0; y < 128; ++y) {
                const size_t i = tt_index(x, y, z);
                const uint8_t id = c->data->blocks[i];
                if (id != before[i])
                    ++changes;
                if (id == leaves || id == log) {
                    const uint8_t metadata =
                        (uint8_t)(((uint32_t)c->data->metadata[i / 2u] >> ((y & 1) * 4)) & 15u);
                    const unsigned int wanted = kind == 0 ? 0u : kind == 1 ? 2u : 1u;
                    if (id == leaves) {
                        ++leaf_count;
                        /* Real removal hooks may set decay bit8 on adjacent leaves. */
                        species = species && (metadata & 7u) == wanted;
                    } else {
                        ++log_count;
                        species = species && metadata == wanted;
                    }
                    bounds = bounds && y >= 64 && y <= 64 + d.height && abs(x - 8) <= 5 &&
                             abs(z - 8) <= 5;
                }
            }
    if (full) {
        for (int32_t layer = 0; layer < d.trunk; ++layer)
            column = column && c->data->blocks[tt_index(8, 64 + layer, 8)] == log;
        for (int32_t layer = d.trunk; layer <= d.height; ++layer)
            column = column && c->data->blocks[tt_index(8, 64 + layer, 8)] == leaves;
    }
    const uint8_t soil_after = c->data->blocks[below];
    const uint8_t origin_after = c->data->blocks[origin];
    tt_restore(saved, c, r);
    prepared = false; /* All controls restored before assertions. */
    CHECK(continued && world_continued && globals && w->active_calls == 0 &&
          w->persistent_bindings == bindings && f->providers[side].cache->count == cache_count &&
          f->providers[side].cache->insertion_count == inserted);
    if (full) {
        CHECK(status == MC_WORLD_STORAGE_OK && out && tt_cleared(&e) && soil_after == dirt &&
              log_count == (size_t)d.trunk && leaf_count > 0 && bounds && species && column);
        if (kind < 2)
            CHECK(leaf_count == 49u + d.corners);
        if (kind == 2)
            CHECK(d.trunk == d.height - 1);
        if (kind == 3)
            CHECK(d.height - d.trunk >= 0 && d.height - d.trunk <= 2);
    } else if (control == TT_HEIGHT_REJECT || control == TT_CLEARANCE_REJECT) {
        CHECK(status == MC_WORLD_STORAGE_OK && !out && tt_cleared(&e) && changes == 0 &&
              registry_retained && leaf_count == 0 && log_count == 0);
    } else {
        CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out == initial &&
              !e.native_boundary && retained_cause && e.feature_class != NULL &&
              strcmp(e.feature_class, side == 0 ? tt_client[kind] : tt_server[kind]) == 0 &&
              e.method != NULL &&
              strcmp(e.method, side == 0 ? "a(Lfd;Ljava/util/Random;III)Z"
                                         : "a(Ldj;Ljava/util/Random;III)Z") == 0);
        if (control == TT_NULL_RANDOM) {
            CHECK(e.bytecode == 2 && changes == 0 && registry_retained &&
                  same_registry(&e.registry, &empty));
        } else if (control == TT_NULL_METADATA) {
            CHECK(e.bytecode == tt_soil_set_pc[kind] && soil_after == dirt && changes == 1 &&
                  e.mutation.status == status && e.mutation.chunk_bytecode == 128 &&
                  same_cause(&e.cause, &e.mutation.cause) && same_registry(&e.registry, &empty));
        } else if (control == TT_LATE_LOG_HOOK) {
            CHECK(e.bytecode == tt_trunk_set_pc[kind] && origin_after == log && log_count == 1 &&
                  leaf_count > 0 && e.mutation.status == status &&
                  e.mutation.chunk_bytecode == (kind == 0 ? 253u : 294u) &&
                  same_registry(&e.registry, &empty) && same_cause(&e.cause, &e.mutation.cause));
            if (kind < 2)
                CHECK(leaf_count == 52u + d.corners);
        } else {
            const unsigned int pc = control == TT_NULL_CLEARANCE_LEAF ? tt_clearance_field[kind]
                                    : control == TT_NULL_GRASS        ? tt_grass_field[kind]
                                    : control == TT_NULL_DIRT_COMPARE ? tt_dirt_compare_field[kind]
                                    : control == TT_NULL_DIRT_WRITE   ? tt_dirt_write_field[kind]
                                    : control == TT_NULL_OPACITY      ? tt_opacity_pc[kind]
                                                                      : tt_leaf_field[kind];
            CHECK(e.bytecode == pc && e.registry.status == MC_REGISTRY_NULL_REFERENCE &&
                  leaf_count == 0 && log_count == 0 && e.registry.initializer_pc == -1 &&
                  e.registry.constructor_pc == -1);
            CHECK(changes == (first_leaf ? 1u : 0u));
            if (first_leaf)
                CHECK(soil_after == dirt);
        }
    }
    ok = true;
cleanup:
    if (prepared)
        tt_restore(saved, c, r);
    free(before);
    free(saved);
    return ok;
}

/* Select a prepared provider continuation by actual Random APIs only. The
 * spread control chooses a first candidate analytically away from the trunk:
 * radial>.8 and angle in (.2,.3) turns, at retained height10. No tree algorithm
 * or voxel replay is used to select or predict a complete result. */
static bool tt_big_seed(int32_t height, bool spread, int64_t *out) {
    for (int64_t seed = 0; seed < 65536; ++seed) {
        mc_java_random provider, private_random;
        tt_random(&provider, seed);
        mc_java_random_seed(&private_random, mc_java_random_next_i64(&provider));
        int32_t draw;
        if (!mc_java_random_next_bounded(&private_random, 6, &draw))
            return false;
        if (draw + 5 != height)
            continue;
        if (spread) {
            const float radial = mc_java_random_next_float(&private_random);
            const float angle = mc_java_random_next_float(&private_random);
            if (!(radial > 0.8f && angle > 0.2f && angle < 0.3f))
                continue;
        }
        *out = seed;
        return true;
    }
    return false;
}

/* Discover the native opaque owner's documented allocation threshold through
 * real admitted budgets. This does not cast a shadow private layout, inject an
 * allocator/clock, or infer Java heap size. Every successful calibration create
 * must consume exactly one real DefaultRandom constructor prefix. */
static bool tt_big_owner_bytes(fixture *f, size_t side, size_t *out) {
    bool ok = false;
    mc_world_tree_big_oak *tree = NULL;
    size_t low = 0, high = 4096;
    for (unsigned int pass = 0; pass < 32; ++pass) {
        const size_t budget = pass == 0 ? high : low + (high - low) / 2u;
        const mc_world_tree_limits limits = {budget};
        const tt_profile profile = tt_profile_read(f->profile);
        mc_world_tree_error e = {0};
        const mc_world_storage_status status =
            mc_world_tree_big_oak_create(f->worlds[side], &limits, &tree, &e);
        const bool created = tree != NULL;
        const bool actual_owner = !created || tt_big_fresh(tree, f->worlds[side], budget);
        if (created) {
            mc_world_tree_big_oak_dispose(tree);
            tree = NULL;
        }
        CHECK(actual_owner && f->worlds[side]->active_calls == 0 &&
              tt_profile_is(f->profile, &profile, created ? 1u : 0u));
        if (status == MC_WORLD_STORAGE_OK) {
            CHECK(created && tt_cleared(&e));
            high = budget;
        } else {
            CHECK(!created && status == MC_WORLD_STORAGE_LIMIT && e.native_boundary &&
                  e.bytecode == 0 && e.profile_status == MC_JAVA_RANDOM_PROFILE_OK);
            CHECK(pass != 0);
            low = budget;
        }
        if (high - low == 1) {
            *out = high;
            ok = true;
            break;
        }
    }
cleanup:
    mc_world_tree_big_oak_dispose(tree);
    return ok;
}

static bool tt_native_admission(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *retired = NULL;
    mc_world_tree_big_oak *tree = NULL;
    const mc_world_runtime_entry entry =
        side == 0 ? MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE : MC_WORLD_RUNTIME_SERVER_LOAD_OR_CREATE;
    CHECK(mc_world_runtime_allocate_world(entry, f->classes[side], NULL, &retired, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_retire_world(retired, NULL) == MC_WORLD_STORAGE_OK);
    mc_java_random random, expected;
    tt_random(&random, 31);
    expected = random;
    const tt_profile profile = tt_profile_read(f->profile);
    const mc_world_storage_error previous = *mc_world_runtime_last_error(retired);
    for (unsigned int kind = 0; kind < 4; ++kind) {
        bool out = (kind & 1u) != 0;
        const bool initial = out;
        mc_world_tree_error e = {0};
        CHECK(tt_small(kind, retired, &random, 64, &out, &e) ==
                  MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              out == initial && e.native_boundary && e.bytecode == 0 && e.feature_class == NULL &&
              e.registry.status == MC_REGISTRY_OK && retired->active_calls == 0 &&
              mc_test_java_random_equal(&random, &expected) &&
              same_cause(&previous, mc_world_runtime_last_error(retired)));
    }
    CHECK(tt_profile_is(f->profile, &profile, 0));
    /* A legitimate RAW/failed owner reaches a query after its initial draws;
     * that lower source failure must not receive the native-admission flag. */
    for (unsigned int kind = 0; kind < 4; ++kind)
        for (size_t phase = 0; phase < 2; ++phase) {
            mc_world_runtime *raw = f->worlds[4u + phase * 2u + side];
            tt_random(&random, 47);
            expected = random;
            tt_draws d;
            CHECK(tt_expected(kind, TT_INITIAL, &expected, &d));
            bool out = true;
            mc_world_tree_error e = {0};
            CHECK(tt_small(kind, raw, &random, 64, &out, &e) ==
                      MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
                  out && !e.native_boundary && e.bytecode == tt_clearance_pc[kind] &&
                  e.registry.status == MC_REGISTRY_OK && raw->active_calls == 0 &&
                  mc_test_java_random_equal(&random, &expected) &&
                  same_cause(&e.cause, mc_world_runtime_last_error(raw)));
        }
    mc_world_tree_error e = {0};
    CHECK(mc_world_tree_big_oak_create(f->worlds[side], NULL, &tree, &e) == MC_WORLD_STORAGE_OK &&
          tt_big_fresh(tree, f->worlds[side], 1024u * 1024u));
    const mc_java_random private_before = tree->random;
    const mc_java_random provider_before = random;
    const size_t bindings = retired->persistent_bindings;
    bool out = true;
    CHECK(mc_world_tree_big_oak_place(tree, retired, &random, 8, 64, 8, &out, &e) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          out && e.native_boundary && e.bytecode == 0 && retired->persistent_bindings == bindings &&
          retired->active_calls == 0);
    CHECK(tt_big_fresh(tree, f->worlds[side], 1024u * 1024u) &&
          mc_test_java_random_equal(&tree->random, &private_before) &&
          mc_test_java_random_equal(&random, &provider_before));
    ok = true;
cleanup:
    mc_world_tree_big_oak_dispose(tree);
    if (retired != NULL && mc_world_runtime_free(retired, NULL) != MC_WORLD_STORAGE_OK)
        ok = false;
    return ok;
}

static bool tt_big_ownership(fixture *f, size_t side) {
    bool ok = false;
    mc_world_tree_big_oak *tree = NULL, *candidate = NULL;
    mc_world_runtime *peer = NULL;
    mc_world_runtime *w = f->worlds[side];
    const size_t base = w->persistent_bindings;
    const mc_world_runtime_entry entry =
        side == 0 ? MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE : MC_WORLD_RUNTIME_SERVER_LOAD_OR_CREATE;
    const tt_profile before = tt_profile_read(f->profile);
    mc_world_tree_error e = {0};
    CHECK(mc_world_tree_big_oak_create(w, NULL, &tree, &e) == MC_WORLD_STORAGE_OK && tree != NULL &&
          tt_cleared(&e) && w->persistent_bindings == base &&
          tt_profile_is(f->profile, &before, 1) && tt_big_fresh(tree, w, 1024u * 1024u));
    const mc_java_random private_before = tree->random;
    candidate = tree;
    const mc_world_tree_limits denied = {0};
    const tt_profile profile = tt_profile_read(f->profile);
    CHECK(mc_world_tree_big_oak_create(w, &denied, &candidate, &e) == MC_WORLD_STORAGE_LIMIT &&
          candidate == tree && e.native_boundary && e.bytecode == 0 &&
          tt_profile_is(f->profile, &profile, 0) && tt_big_fresh(tree, w, 1024u * 1024u) &&
          mc_test_java_random_equal(&tree->random, &private_before));
    candidate = NULL;
    /* A failed actual DefaultRandom FP check is at constructor9, rather than
     * globally reclassified as the two corrected native wrapper boundaries. */
    const unsigned int csr = _mm_getcsr();
    candidate = tree;
    _mm_setcsr((csr & ~0x6000u) | 0x6000u);
    const mc_world_storage_status ctor_status =
        mc_world_tree_big_oak_create(w, NULL, &candidate, &e);
    _mm_setcsr(csr);
    CHECK(ctor_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && candidate == tree &&
          e.bytecode == 9 && !e.native_boundary &&
          e.profile_status == MC_JAVA_RANDOM_PROFILE_UNSUPPORTED_FP &&
          tt_profile_is(f->profile, &profile, 0) && w->active_calls == 0 &&
          tt_big_fresh(tree, w, 1024u * 1024u) &&
          mc_test_java_random_equal(&tree->random, &private_before));
    candidate = NULL;
    CHECK(mc_world_runtime_allocate_world(entry, f->classes[side], NULL, &peer, NULL) ==
          MC_WORLD_STORAGE_OK);
    const size_t peer_base = peer->persistent_bindings;
    bool out = true;
    CHECK(mc_world_tree_big_oak_place(tree, peer, NULL, 8, 64, 8, &out, &e) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          out && e.bytecode == 6 && e.entry_bytecode == 6 && !e.native_boundary &&
          peer->persistent_bindings == peer_base + 1u && w->persistent_bindings == base &&
          peer->active_calls == 0);
    CHECK(tt_big_quiescent(tree) && tree->classes == w->classes && tree->world == peer &&
          tree->base[0] == 0 && tree->base[1] == 0 && tree->base[2] == 0 &&
          tree->height_limit == 0 && tree->trunk_height == 0 && tree->nodes == NULL &&
          tree->node_count == 0 && tree->allocation_bytes == sizeof(*tree) &&
          mc_test_java_random_equal(&tree->random, &private_before));
    mc_world_storage_error busy = {0};
    CHECK(mc_world_runtime_free(peer, &busy) == MC_WORLD_STORAGE_BUSY && busy.operation != NULL &&
          strcmp(busy.operation, "runtime persistent binding active") == 0);
    CHECK(mc_world_tree_big_oak_place(tree, peer, NULL, 8, 64, 8, &out, &e) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          peer->persistent_bindings == peer_base + 1u && tree->world == peer &&
          tt_big_quiescent(tree) && mc_test_java_random_equal(&tree->random, &private_before));
    mc_java_random random, expected;
    tt_random(&random, 71);
    expected = random;
    CHECK(mc_world_tree_big_oak_place(tree, f->worlds[1u - side], &random, 8, 64, 8, &out, &e) ==
              MC_WORLD_STORAGE_SESSION_MISMATCH &&
          out && e.native_boundary && e.bytecode == 0 &&
          peer->persistent_bindings == peer_base + 1u &&
          mc_test_java_random_equal(&random, &expected) && tree->world == peer &&
          tt_big_quiescent(tree) && mc_test_java_random_equal(&tree->random, &private_before));
    CHECK(mc_world_tree_big_oak_place(tree, w, NULL, 8, 64, 8, &out, &e) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          out && e.bytecode == 6 && !e.native_boundary && peer->persistent_bindings == peer_base &&
          w->persistent_bindings == base + 1u && tree->world == w && tt_big_quiescent(tree) &&
          tree->height_limit == 0 && tree->base[0] == 0 && tree->base[1] == 0 &&
          tree->base[2] == 0 && tree->nodes == NULL && tree->node_count == 0 &&
          tree->allocation_bytes == sizeof(*tree) &&
          mc_test_java_random_equal(&tree->random, &private_before));
    mc_world_tree_big_oak_dispose(tree);
    tree = NULL;
    CHECK(w->persistent_bindings == base && tt_profile_is(f->profile, &profile, 0));
    ok = true;
cleanup:
    if (candidate != NULL && candidate != tree)
        mc_world_tree_big_oak_dispose(candidate);
    mc_world_tree_big_oak_dispose(tree);
    /* candidate can be an alias of tree solely to check failure preservation. */
    if (peer != NULL && (mc_world_runtime_retire_world(peer, NULL) != MC_WORLD_STORAGE_OK ||
                         mc_world_runtime_free(peer, NULL) != MC_WORLD_STORAGE_OK))
        ok = false;
    return ok;
}

typedef enum tt_big_control {
    TT_BIG_FOUR,
    TT_BIG_STICKY_FIVE,
    TT_BIG_SPREAD,
    TT_BIG_CANDIDATE_LIMIT,
    TT_BIG_REPLACEMENT_LIMIT,
    TT_BIG_EXACT_PEAK_REUSE,
    TT_BIG_NULL_METADATA,
    TT_BIG_LATE_LOG,
    TT_BIG_RETAINED_CANDIDATE
} tt_big_control;

static bool tt_big_trial(fixture *f, size_t side, size_t owner_bytes, tt_big_control control) {
    bool ok = false, prepared = false;
    tt_saved *saved = NULL;
    mc_world_tree_big_oak *tree = NULL;
    mc_world_runtime *w = f->worlds[side];
    mc_registry_classes *r = f->registry[side];
    mc_world_chunk *c = mc_world_store_chunk_object(f->stores[side], f->normal[side][0]);
    int32_t grass;
    CHECK(tt_id(f, side, "v", &grass) && grass == 2);
    saved = malloc(sizeof(*saved));
    CHECK(saved != NULL && tt_prepare(saved, c, r, grass));
    prepared = true;
    CHECK(tt_stale_registry(w, c, r));
    const bool ten = control == TT_BIG_SPREAD || control == TT_BIG_CANDIDATE_LIMIT ||
                     control == TT_BIG_REPLACEMENT_LIMIT || control == TT_BIG_EXACT_PEAK_REUSE;
    const int32_t height = ten ? 10 : 5;
    /* For n4/e10 with density0, four admitted layers each draw two floats and
     * append one row in this all-air prepared space. e5 has only its first row.
     * The fixed invocation budget is compared with the real private owner below. */
    const size_t used = ten ? 5u : 1u;
    const size_t candidate_bytes = (size_t)height * sizeof(int32_t[4]);
    const size_t peak = owner_bytes + candidate_bytes + used * sizeof(int32_t[4]);
    mc_world_tree_limits limits = {1024u * 1024u};
    if (control == TT_BIG_CANDIDATE_LIMIT)
        limits.max_allocation_bytes = owner_bytes + candidate_bytes - 1u;
    if (control == TT_BIG_REPLACEMENT_LIMIT)
        limits.max_allocation_bytes = peak - 1u;
    if (control == TT_BIG_EXACT_PEAK_REUSE || control == TT_BIG_RETAINED_CANDIDATE)
        limits.max_allocation_bytes = peak;
    const tt_profile before_create = tt_profile_read(f->profile);
    mc_world_tree_error e = {0};
    CHECK(mc_world_tree_big_oak_create(w, &limits, &tree, &e) == MC_WORLD_STORAGE_OK &&
          tt_cleared(&e) && tt_profile_is(f->profile, &before_create, 1) &&
          tt_big_fresh(tree, w, limits.max_allocation_bytes));
    const mc_java_random constructor_random = tree->random;
    if (control == TT_BIG_STICKY_FIVE) {
        CHECK(mc_world_tree_big_oak_scale(tree, 1.0, 0.0, 0.0, &e) == MC_WORLD_STORAGE_OK &&
              tt_cleared(&e) && tt_big_quiescent(tree) && tree->world == NULL &&
              tree->height_limit == 0 && tree->height_random_limit == 12 &&
              tree->leaf_distance == 5 && tree->width_scale == 0.0 && tree->leaf_density == 0.0 &&
              tree->nodes == NULL && tree->node_count == 0 &&
              tree->allocation_bytes == owner_bytes &&
              mc_test_java_random_equal(&tree->random, &constructor_random));
    }
    CHECK(mc_world_tree_big_oak_scale(tree, 0.5, control == TT_BIG_SPREAD ? 1.0 : 0.0,
                                      control == TT_BIG_SPREAD ? 1.0 : 0.0,
                                      &e) == MC_WORLD_STORAGE_OK &&
          tt_cleared(&e));
    CHECK(tt_big_quiescent(tree) && tree->world == NULL && tree->height_limit == 0 &&
          tree->height_random_limit == 6 &&
          tree->leaf_distance == (control == TT_BIG_STICKY_FIVE ? 5 : 4) &&
          tree->width_scale == (control == TT_BIG_SPREAD ? 1.0 : 0.0) &&
          tree->leaf_density == (control == TT_BIG_SPREAD ? 1.0 : 0.0) && tree->nodes == NULL &&
          tree->node_count == 0 && tree->allocation_bytes == owner_bytes &&
          mc_test_java_random_equal(&tree->random, &constructor_random));
    const tt_profile profile = tt_profile_read(f->profile);
    /* Genuine public height call BETWEEN construction/scale and placement. */
    int32_t y = -99;
    mc_world_light_error height_error;
    CHECK(mc_world_runtime_height(w, 8, 8, &y, &height_error) == MC_WORLD_STORAGE_OK && y == 64 &&
          tt_profile_is(f->profile, &profile, 0) && tt_big_quiescent(tree) && tree->world == NULL &&
          mc_test_java_random_equal(&tree->random, &constructor_random));
    int64_t seed;
    CHECK(tt_big_seed(height, control == TT_BIG_SPREAD, &seed));
    mc_java_random random, expected;
    tt_random(&random, seed);
    expected = random;
    mc_java_random private_expected = tree->random;
    const int64_t private_seed = mc_java_random_next_i64(&expected);
    mc_java_random_reseed(&private_expected, private_seed);
    int32_t private_height;
    CHECK(mc_java_random_next_bounded(&private_expected, 6, &private_height) &&
          private_height + 5 == height);
    /* Independent invocation budgets for zero-density prepared domains only:
     * e5/n4 or n5 has no admitted planning layer; e10/n4 has four one-attempt
     * layers, two floats each. Candidate76 precedes every planning draw.
     * Do not infer an arbitrary/spread successful tail from native geometry. */
    const bool private_tail_known = control != TT_BIG_SPREAD;
    const unsigned int planning_draws = ten && control != TT_BIG_CANDIDATE_LIMIT ? 8u : 0u;
    if (private_tail_known)
        for (unsigned int draw = 0; draw < planning_draws; ++draw)
            (void)mc_java_random_next_float(&private_expected);
    int32_t(*const original_nodes)[4] = tree->nodes;
    const int32_t original_count = tree->node_count;
    const size_t original_allocation = tree->allocation_bytes;
    const mc_java_random world_random = *w->fields.r;
    const size_t bindings = w->persistent_bindings;
    const size_t count = f->providers[side].cache->count;
    const size_t insertions = f->providers[side].cache->insertion_count;
    if (control == TT_BIG_NULL_METADATA)
        c->e = NULL;
    if (control == TT_BIG_LATE_LOG)
        r->blocks.m[17] = NULL;
    const bool initial = (control & 1u) != 0;
    bool out = initial;
    const mc_world_storage_status status =
        mc_world_tree_big_oak_place(tree, w, &random, 8, y, 8, &out, &e);
    const bool continued = mc_test_java_random_equal(&random, &expected);
    const bool private_continued =
        !private_tail_known || mc_test_java_random_equal(&tree->random, &private_expected);
    const bool global_prefix = tt_profile_is(f->profile, &profile, 0);
    const bool retained_cause = same_cause(&e.cause, mc_world_runtime_last_error(w));
    const mc_registry_error empty = tt_empty_registry();
    size_t logs = 0, leaves = 0, offaxis = 0;
    bool bounds = true, metadata = true, column = true;
    for (int32_t x = 0; x < 16; ++x)
        for (int32_t z = 0; z < 16; ++z)
            for (int32_t cy = 0; cy < 128; ++cy) {
                const size_t index = tt_index(x, cy, z);
                const uint8_t id = c->data->blocks[index];
                if (id == 17 || id == 18) {
                    const uint8_t m =
                        (uint8_t)(((uint32_t)c->data->metadata[index / 2u] >> ((cy & 1) * 4)) &
                                  15u);
                    bounds = bounds && cy >= 64 && cy < 64 + height && abs(x - 8) <= 5 &&
                             abs(z - 8) <= 5;
                    if (id == 17) {
                        ++logs;
                        if (x != 8 || z != 8)
                            ++offaxis;
                        metadata = metadata && m == 0;
                    } else {
                        ++leaves;
                        metadata = metadata && (m & 7u) == 0;
                    }
                }
            }
    const int32_t trunk = ten ? 7 : 4; /* floor(.618*10)+1 / floor(.618*5)+1, inclusive log end. */
    for (int32_t layer = 0; layer < trunk; ++layer)
        column = column && c->data->blocks[tt_index(8, 64 + layer, 8)] == 17;
    const uint8_t discriminating = c->data->blocks[tt_index(10, 65, 9)];
    const uint8_t top = c->data->blocks[tt_index(8, 63 + height, 8)];
    const uint8_t soil = c->data->blocks[tt_index(8, 63, 8)];
    c->e = saved->e;
    r->blocks.m = saved->m;
    memcpy(r->blocks.m, saved->elements, sizeof(saved->elements));
    CHECK(continued && private_continued && global_prefix &&
          mc_test_java_random_equal(&world_random, w->fields.r) && w->active_calls == 0 &&
          w->persistent_bindings == bindings + 1u && f->providers[side].cache->count == count &&
          f->providers[side].cache->insertion_count == insertions && soil == 2 &&
          tt_big_quiescent(tree) && tree->world == w && tree->classes == w->classes &&
          tree->base[0] == 8 && tree->base[1] == y && tree->base[2] == 8 &&
          tree->height_limit == height && tree->trunk_height == trunk - 1 &&
          tree->height_random_limit == 6 &&
          tree->leaf_distance == (control == TT_BIG_STICKY_FIVE ? 5 : 4) &&
          tree->max_allocation_bytes == limits.max_allocation_bytes);
    if (control == TT_BIG_CANDIDATE_LIMIT || control == TT_BIG_REPLACEMENT_LIMIT) {
        CHECK(status == MC_WORLD_STORAGE_LIMIT && out == initial && e.native_boundary &&
              e.entry_bytecode == 77 &&
              e.bytecode == (control == TT_BIG_CANDIDATE_LIMIT ? 76u : 542u) && e.method != NULL &&
              strcmp(e.method, "a()V") == 0 && logs == 0 && leaves == 0 &&
              same_registry(&e.registry, &empty) && retained_cause &&
              tree->nodes == original_nodes && tree->node_count == original_count &&
              tree->allocation_bytes == original_allocation);
    } else if (control == TT_BIG_NULL_METADATA) {
        CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out == initial &&
              !e.native_boundary && e.entry_bytecode == 81 && e.bytecode == 237 && logs == 0 &&
              leaves == 1 && e.method != NULL && strcmp(e.method, "a(IIIFBI)V") == 0 &&
              e.mutation.chunk_bytecode == 128 && same_cause(&e.cause, &e.mutation.cause) &&
              same_registry(&e.registry, &empty) && retained_cause);
    } else if (control == TT_BIG_LATE_LOG) {
        CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out == initial &&
              !e.native_boundary && e.entry_bytecode == 85 && e.bytecode == 272 && logs == 1 &&
              leaves == 52 && e.method != NULL && strcmp(e.method, "a([I[II)V") == 0 &&
              e.mutation.chunk_bytecode == 253 && same_cause(&e.cause, &e.mutation.cause) &&
              same_registry(&e.registry, &empty) && retained_cause);
    } else {
        CHECK(status == MC_WORLD_STORAGE_OK && out && tt_cleared(&e) && column && leaves > 0 &&
              logs > 0 && top == 18 && bounds && metadata);
        if (control == TT_BIG_SPREAD)
            CHECK(offaxis > 0 && logs >= (size_t)trunk);
        else
            CHECK(logs == (size_t)trunk && offaxis == 0);
        if (control == TT_BIG_FOUR || control == TT_BIG_RETAINED_CANDIDATE)
            CHECK(leaves == 49 && discriminating == 0);
        if (control == TT_BIG_STICKY_FIVE)
            CHECK(leaves == 69 && discriminating == 18);
        if (control == TT_BIG_EXACT_PEAK_REUSE)
            CHECK(leaves == 131);
    }
    if (control != TT_BIG_CANDIDATE_LIMIT && control != TT_BIG_REPLACEMENT_LIMIT) {
        CHECK(tree->nodes != NULL && tree->node_count >= 1 &&
              tree->allocation_bytes ==
                  owner_bytes + (size_t)tree->node_count * sizeof(*tree->nodes));
        if (private_tail_known)
            CHECK(tree->node_count == (int32_t)used);
    }
    if (control == TT_BIG_FOUR || control == TT_BIG_EXACT_PEAK_REUSE ||
        control == TT_BIG_RETAINED_CANDIDATE) {
        CHECK(tree->node_count == (int32_t)used && used <= 5u);
        int32_t(*const retained_nodes)[4] = tree->nodes;
        const int32_t retained_count = tree->node_count;
        const size_t retained_allocation = tree->allocation_bytes;
        const size_t retained_bytes = used * sizeof(*tree->nodes);
        int32_t retained_rows[5][4];
        memcpy(retained_rows, tree->nodes, retained_bytes);
        const mc_java_random before_null = tree->random;
        bool null_out = true;
        CHECK(mc_world_tree_big_oak_place(tree, w, NULL, -21, 71, 13, &null_out, &e) ==
                  MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              null_out && !e.native_boundary && e.bytecode == 6 && e.entry_bytecode == 6 &&
              same_registry(&e.registry, &empty) && tt_big_quiescent(tree) && tree->world == w &&
              tree->base[0] == 8 && tree->base[1] == 64 && tree->base[2] == 8 &&
              tree->height_limit == height && tree->trunk_height == trunk - 1 &&
              tree->nodes == retained_nodes && tree->node_count == retained_count &&
              tree->allocation_bytes == retained_allocation &&
              memcmp(tree->nodes, retained_rows, retained_bytes) == 0 &&
              mc_test_java_random_equal(&tree->random, &before_null) &&
              tt_profile_is(f->profile, &profile, 0) && w->persistent_bindings == bindings + 1u &&
              w->active_calls == 0);
        /* Scale does not reset retained e. A fresh owner at bound0 fails; this
         * reused owner must skip the height draw. Old retained rows must remain
         * simultaneously accounted during the replacement attempt. */
        const double reuse_density = control == TT_BIG_RETAINED_CANDIDATE ? 3.0 : 0.0;
        CHECK(mc_world_tree_big_oak_scale(tree, 0.0, 0.0, reuse_density, &e) ==
                  MC_WORLD_STORAGE_OK &&
              tt_cleared(&e) && tt_big_quiescent(tree) && tree->world == w &&
              tree->height_limit == height && tree->trunk_height == trunk - 1 &&
              tree->height_random_limit == 0 && tree->leaf_distance == 4 &&
              tree->width_scale == 0.0 && tree->leaf_density == reuse_density &&
              tree->nodes == retained_nodes && tree->node_count == retained_count &&
              tree->allocation_bytes == retained_allocation &&
              memcmp(tree->nodes, retained_rows, retained_bytes) == 0 &&
              mc_test_java_random_equal(&tree->random, &before_null));
        memset(c->data->blocks, 0, MC_CHUNK_BLOCKS);
        for (int32_t x = 0; x < 16; ++x)
            for (int32_t z = 0; z < 16; ++z)
                c->data->blocks[tt_index(x, 63, z)] = 2;
        memset(c->data->heightmap, 64, MC_CHUNK_HEIGHTMAP_BYTES);
        tt_random(&random, seed + 1);
        expected = random;
        private_expected = tree->random;
        mc_java_random_reseed(&private_expected, mc_java_random_next_i64(&expected));
        /* Retained e omits nextInt even though scale installed bound0. The
         * e10/n4 zero-density retry reaches eight floats before replacement542;
         * e5 candidate76 and e5 full retry reach no planning float. */
        if (control == TT_BIG_EXACT_PEAK_REUSE)
            for (unsigned int draw = 0; draw < 8u; ++draw)
                (void)mc_java_random_next_float(&private_expected);
        out = false;
        const mc_world_storage_status repeated =
            mc_world_tree_big_oak_place(tree, w, &random, 8, 64, 8, &out, &e);
        CHECK(mc_test_java_random_equal(&random, &expected) &&
              tt_profile_is(f->profile, &profile, 0) &&
              mc_test_java_random_equal(&tree->random, &private_expected) &&
              w->persistent_bindings == bindings + 1u && w->active_calls == 0 &&
              tt_big_quiescent(tree) && tree->world == w && tree->height_limit == height &&
              tree->height_random_limit == 0 && tree->leaf_distance == 4 && tree->base[0] == 8 &&
              tree->base[1] == 64 && tree->base[2] == 8);
        if (control == TT_BIG_FOUR)
            CHECK(repeated == MC_WORLD_STORAGE_OK && out && tt_cleared(&e) &&
                  c->data->blocks[tt_index(8, 67, 8)] == 17 &&
                  c->data->blocks[tt_index(8, 68, 8)] == 18 && tree->nodes != NULL &&
                  tree->node_count == retained_count &&
                  tree->allocation_bytes == retained_allocation);
        else {
            CHECK(repeated == MC_WORLD_STORAGE_LIMIT && !out && e.native_boundary &&
                  e.bytecode == (control == TT_BIG_RETAINED_CANDIDATE ? 76u : 542u) &&
                  e.entry_bytecode == 77 && c->data->blocks[tt_index(8, 64, 8)] == 0 &&
                  tree->nodes == retained_nodes && tree->node_count == retained_count &&
                  tree->allocation_bytes == retained_allocation &&
                  memcmp(tree->nodes, retained_rows, retained_bytes) == 0);
        }
    }
    mc_world_tree_big_oak_dispose(tree);
    tree = NULL;
    CHECK(w->persistent_bindings == bindings);
    ok = true;
cleanup:
    mc_world_tree_big_oak_dispose(tree);
    if (prepared)
        tt_restore(saved, c, r);
    free(saved);
    return ok;
}

static bool tt_big_bounds_and_guards(fixture *f, size_t side) {
    bool ok = false, prepared = false;
    tt_saved *saved = NULL;
    mc_world_tree_big_oak *tree = NULL;
    mc_world_runtime *w = f->worlds[side];
    mc_world_chunk *c = mc_world_store_chunk_object(f->stores[side], f->normal[side][0]);
    mc_registry_classes *r = f->registry[side];
    int32_t grass;
    CHECK(tt_id(f, side, "v", &grass) && grass == 2);
    saved = malloc(sizeof(*saved));
    CHECK(saved != NULL && tt_prepare(saved, c, r, grass));
    prepared = true;
    CHECK(tt_stale_registry(w, c, r));
    const mc_registry_error prior = *mc_world_runtime_last_registry_error(w);
    const mc_registry_error empty = tt_empty_registry();
    const double scales[] = {0.0, -1.0, NAN};
    const int32_t bounds[] = {0, -12, 0};
    for (size_t i = 0; i < COUNT(scales); ++i) {
        const tt_profile before = tt_profile_read(f->profile);
        mc_world_tree_error e = {0};
        CHECK(mc_world_tree_big_oak_create(w, NULL, &tree, &e) == MC_WORLD_STORAGE_OK &&
              tt_profile_is(f->profile, &before, 1) && tt_big_fresh(tree, w, 1024u * 1024u));
        const mc_java_random constructor_random = tree->random;
        CHECK(mc_world_tree_big_oak_scale(tree, scales[i], 1.0, 1.0, &e) == MC_WORLD_STORAGE_OK &&
              tt_cleared(&e) && tt_big_quiescent(tree) && tree->height_limit == 0 &&
              tree->height_random_limit == bounds[i] && tree->leaf_distance == 4 &&
              tree->width_scale == 1.0 && tree->leaf_density == 1.0 && tree->world == NULL &&
              tree->nodes == NULL && tree->node_count == 0 &&
              tree->allocation_bytes == sizeof(*tree) &&
              mc_test_java_random_equal(&tree->random, &constructor_random));
        const tt_profile profile = tt_profile_read(f->profile);
        mc_world_runtime *raw = f->worlds[4u + side];
        const size_t bindings = raw->persistent_bindings;
        mc_java_random random, expected;
        tt_random(&random, 101 + (int64_t)i);
        expected = random;
        mc_java_random private_expected = tree->random;
        mc_java_random_reseed(&private_expected, mc_java_random_next_i64(&expected));
        int32_t rejected = 731;
        CHECK(!mc_java_random_next_bounded(&private_expected, bounds[i], &rejected) &&
              rejected == 731);
        const bool initial = (i & 1u) != 0;
        bool out = initial;
        /* Bad bound60 occurs before any call to this genuine RAW provider. */
        CHECK(mc_world_tree_big_oak_place(tree, raw, &random, 8, 64, 8, &out, &e) ==
                  MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              out == initial && !e.native_boundary && e.bytecode == 60 && e.entry_bytecode == 60 &&
              same_registry(&e.registry, &empty) && mc_test_java_random_equal(&random, &expected) &&
              raw->active_calls == 0 && raw->persistent_bindings == bindings + 1u &&
              tt_profile_is(f->profile, &profile, 0) &&
              same_cause(&e.cause, mc_world_runtime_last_error(raw)) && tt_big_quiescent(tree) &&
              tree->world == raw && tree->classes == w->classes && tree->base[0] == 8 &&
              tree->base[1] == 64 && tree->base[2] == 8 && tree->height_limit == 0 &&
              tree->trunk_height == 0 && tree->height_random_limit == bounds[i] &&
              tree->nodes == NULL && tree->node_count == 0 &&
              tree->allocation_bytes == sizeof(*tree) &&
              mc_test_java_random_equal(&tree->random, &private_expected));
        mc_world_tree_big_oak_dispose(tree);
        tree = NULL;
        CHECK(raw->persistent_bindings == bindings);
    }
    mc_world_tree_error e = {0};
    CHECK(mc_world_tree_big_oak_create(w, NULL, &tree, &e) == MC_WORLD_STORAGE_OK &&
          tt_big_fresh(tree, w, 1024u * 1024u));
    const mc_java_random constructor_random = tree->random;
    const size_t bindings = w->persistent_bindings;
    const tt_profile profile = tt_profile_read(f->profile);
    const unsigned int csr = _mm_getcsr();
    _mm_setcsr((csr & ~0x6000u) | 0x6000u);
    const mc_world_storage_status scale_status =
        mc_world_tree_big_oak_scale(tree, 0.0, 0.0, 0.0, &e);
    _mm_setcsr(csr);
    CHECK(scale_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && e.native_boundary &&
          e.bytecode == 5 && tt_profile_is(f->profile, &profile, 0) &&
          tt_big_fresh(tree, w, 1024u * 1024u) &&
          mc_test_java_random_equal(&tree->random, &constructor_random));
    /* Failed scale must not install bound0: original constructor bound12 is
     * still positive. Literal soil mismatch rejects before line FP work. */
    c->data->blocks[tt_index(8, 63, 8)] = 1;
    mc_java_random random, expected;
    tt_random(&random, 109);
    expected = random;
    mc_java_random private_expected = tree->random;
    mc_java_random_reseed(&private_expected, mc_java_random_next_i64(&expected));
    int32_t height_draw;
    CHECK(mc_java_random_next_bounded(&private_expected, 12, &height_draw));
    const int32_t retained_height = 5 + height_draw;
    bool out = true;
    _mm_setcsr((csr & ~0x6000u) | 0x6000u);
    const mc_world_storage_status soil_status =
        mc_world_tree_big_oak_place(tree, w, &random, 8, 64, 8, &out, &e);
    _mm_setcsr(csr);
    CHECK(soil_status == MC_WORLD_STORAGE_OK && !out && tt_cleared(&e) &&
          mc_test_java_random_equal(&random, &expected) && tt_profile_is(f->profile, &profile, 0) &&
          w->persistent_bindings == bindings + 1u &&
          same_registry(&prior, mc_world_runtime_last_registry_error(w)) &&
          tt_big_quiescent(tree) && tree->world == w && tree->height_limit == retained_height &&
          tree->height_random_limit == 12 && tree->leaf_distance == 4 && tree->trunk_height == 0 &&
          tree->base[0] == 8 && tree->base[1] == 64 && tree->base[2] == 8 && tree->nodes == NULL &&
          tree->node_count == 0 && tree->allocation_bytes == sizeof(*tree) &&
          mc_test_java_random_equal(&tree->random, &private_expected));
    c->data->blocks[tt_index(8, 63, 8)] = 2;
    tt_random(&random, 113);
    expected = random;
    private_expected = tree->random;
    mc_java_random_reseed(&private_expected, mc_java_random_next_i64(&expected));
    out = true;
    _mm_setcsr((csr & ~0x6000u) | 0x6000u);
    const mc_world_storage_status line_status =
        mc_world_tree_big_oak_place(tree, w, &random, 8, 64, 8, &out, &e);
    _mm_setcsr(csr);
    CHECK(line_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out && e.native_boundary &&
          e.bytecode == 125 && e.entry_bytecode == 68 && e.method != NULL &&
          strcmp(e.method, "a([I[I)I") == 0 && mc_test_java_random_equal(&random, &expected) &&
          tt_profile_is(f->profile, &profile, 0) && w->active_calls == 0 &&
          w->persistent_bindings == bindings + 1u && same_registry(&e.registry, &empty) &&
          same_registry(&prior, mc_world_runtime_last_registry_error(w)) &&
          same_cause(&e.cause, mc_world_runtime_last_error(w)) && tt_big_quiescent(tree) &&
          tree->world == w && tree->height_limit == retained_height &&
          tree->height_random_limit == 12 && tree->leaf_distance == 4 && tree->trunk_height == 0 &&
          tree->nodes == NULL && tree->node_count == 0 && tree->allocation_bytes == sizeof(*tree) &&
          mc_test_java_random_equal(&tree->random, &private_expected));
    mc_world_tree_big_oak_dispose(tree);
    tree = NULL;
    CHECK(w->persistent_bindings == bindings);
    ok = true;
cleanup:
    mc_world_tree_big_oak_dispose(tree);
    if (prepared)
        tt_restore(saved, c, r);
    free(saved);
    return ok;
}

static bool population_tree_contracts(fixture *f, size_t side) {
    bool ok = false;
    CHECK(side < 2 && f->resources[side].opens == 3 && !f->resources[side].bad &&
          f->worlds[side]->fields.r != NULL && f->worlds[side]->active_calls == 0 &&
          f->normal[side][0] != NULL && !f->normal[side][0]->blank);
    CHECK(tt_native_admission(f, side));
    for (unsigned int kind = 0; kind < 4; ++kind) {
        for (unsigned int control = TT_FULL; control <= TT_LATE_LOG_HOOK; ++control)
            CHECK(tt_small_trial(f, side, kind, (tt_control)control,
                                 17 + (int64_t)kind * 37 + (int64_t)control * 13, false));
        const int64_t seeds[] = {0, 1, INT64_C(0x1fffffffffffff)};
        for (size_t seed = 0; seed < COUNT(seeds); ++seed)
            CHECK(tt_small_trial(f, side, kind, TT_FULL, seeds[seed], seed == 2));
    }
    size_t owner_bytes = 0;
    CHECK(tt_big_owner_bytes(f, side, &owner_bytes) &&
          owner_bytes == sizeof(mc_world_tree_big_oak) && owner_bytes > 0 && owner_bytes <= 4096);
    CHECK(tt_big_ownership(f, side) && tt_big_bounds_and_guards(f, side));
    for (unsigned int control = TT_BIG_FOUR; control <= TT_BIG_RETAINED_CANDIDATE; ++control)
        CHECK(tt_big_trial(f, side, owner_bytes, (tt_big_control)control));
    ok = true;
cleanup:
    /* No forged counts, queue reset, provider substitution or hidden retry. */
    if (!remember(f, side))
        ok = false;
    return ok;
}
