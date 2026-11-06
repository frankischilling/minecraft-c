/* Include fragment: tests/world/runtime/population/ore_clay_contracts.h.
 * Include after the genuine test_queries.c fixture/helpers and call
 * population_ore_clay_contracts(f, side) before population_plant_contracts.
 * Prepared bytes/references below are native input controls, not generated
 * terrain or Java allocation observations. No copied voxel algorithm/golden. */
#include "../../../../src/core/java/random_profile_internal.h"
#include "../../../support/java_random.h"
#include "mc/world/runtime/population/ore_clay.h"
#include <emmintrin.h>
#include <math.h>

typedef struct oc_profile {
    uint64_t unique, math_state, math_bits;
    unsigned int phase;
    mc_java_math_random_owner *math;
    size_t allocated;
    mc_platform_nanotime clock;
    bool math_cached;
} oc_profile;

typedef struct oc_saved {
    mc_chunk_data data;
    uint8_t blocks[MC_CHUNK_BLOCKS];
    mc_world_chunk_nibble *e, *f, *g, metadata;
    uint8_t *h;
    mc_block **m, *elements[MC_REGISTRY_BLOCK_SLOTS], *named[96];
    int32_t *q;
} oc_saved;

typedef struct oc_probe {
    int64_t seed;
    int32_t y_start;
} oc_probe;

typedef struct oc_family {
    bool clay;
    const char *target;
    int32_t size;
} oc_family;

typedef enum oc_control {
    OC_SINGLE,
    OC_VOLUME,
    OC_NO_REPLACEMENT,
    OC_EQUAL,
    OC_LATE_STATIC,
    OC_MUTATION_M,
    OC_MUTATION_Q,
    OC_MUTATION_METADATA,
    OC_TWO_WRITES,
    OC_NONWATER,
    OC_NONWATER_BAD_FP,
    OC_NULL_MATERIAL,
    OC_MATERIAL_M,
    OC_NULL_RANDOM,
    OC_BAD_FP,
    OC_CAPTURED_CLAY_TARGET,
    OC_REJECTED_GEOMETRY,
    OC_REMOTE_GRAVEL
} oc_control;

static uint64_t oc_bits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static oc_profile oc_profile_read(const mc_java_random_profile *p) {
    oc_profile s = {.unique = atomic_load_explicit(&p->uniquifier, memory_order_relaxed),
                    .phase = atomic_load_explicit(&p->holder_phase, memory_order_acquire),
                    .math = p->math,
                    .allocated = p->allocated_bytes,
                    .clock = p->clock};
    if (s.math != NULL) {
        s.math_state = atomic_load_explicit(&s.math->state, memory_order_relaxed);
        s.math_bits = oc_bits(s.math->next_next_gaussian);
        s.math_cached = s.math->have_next_next_gaussian;
    }
    return s;
}

static bool oc_profile_same(const mc_java_random_profile *p, const oc_profile *before) {
    const oc_profile a = oc_profile_read(p);
    return a.unique == before->unique && a.phase == before->phase && a.math == before->math &&
           a.math_state == before->math_state && a.math_bits == before->math_bits &&
           a.math_cached == before->math_cached && a.allocated == before->allocated &&
           a.clock.frequency == before->clock.frequency &&
           a.clock.initial_sample == before->clock.initial_sample &&
           a.clock.initialized == before->clock.initialized;
}

static mc_registry_error oc_empty_registry(void) {
    return (mc_registry_error){.initializer_pc = -1,
                               .block_id = -1,
                               .constructor_pc = -1,
                               .active_initializer_pc = -1,
                               .item_argument = -1};
}

static bool oc_cleared(const mc_population_ore_clay_error *e) {
    const mc_registry_error empty = oc_empty_registry();
    return e->status == MC_WORLD_STORAGE_OK && e->operation == NULL && e->feature_class == NULL &&
           e->feature_method == NULL && e->bytecode == 0 &&
           e->cause.status == MC_WORLD_STORAGE_OK && e->cause.operation == NULL &&
           e->cause.system_error == 0 && same_registry(&e->registry, &empty) &&
           e->material.status == MC_WORLD_STORAGE_OK && e->material.bytecode == 0 &&
           same_registry(&e->material.registry, &empty) &&
           e->mutation.status == MC_WORLD_STORAGE_OK && e->mutation.bytecode == 0 &&
           e->mutation.chunk_bytecode == 0 && same_registry(&e->mutation.registry, &empty);
}

static size_t oc_index(int32_t x, int32_t y, int32_t z) {
    return (((size_t)(uint32_t)x & 15u) << 11u) | (((size_t)(uint32_t)z & 15u) << 7u) | (size_t)y;
}

static uint8_t oc_nibble(const uint8_t *a, size_t index) {
    return (uint8_t)(((uint32_t)a[index / 2u] >> ((index & 1u) * 4u)) & 15u);
}

static mc_block **oc_named(mc_registry_classes *r, const char *name) {
    for (size_t i = 0; i < COUNT(r->blocks.named); ++i) {
        const char *n = mc_registry_block_static_name(i);
        if (n != NULL && strcmp(n, name) == 0)
            return &r->blocks.named[i];
    }
    return NULL;
}

static bool oc_block(fixture *f, size_t side, const char *name, const mc_block **out) {
    return mc_registry_block_static_get(f->registry[side], name, out, NULL) == MC_REGISTRY_OK &&
           *out != NULL && (*out)->fields.bn > 0 && (*out)->fields.bn <= UINT8_MAX;
}

static bool oc_save(oc_saved *s, mc_world_chunk *c, mc_registry_classes *r) {
    if (c == NULL || c->data == NULL || c->data->blocks == NULL ||
        c->data->blocks_capacity < MC_CHUNK_BLOCKS || c->e == NULL || c->f == NULL ||
        c->g == NULL || c->h != c->data->heightmap || c->e->a != c->data->metadata ||
        c->f->a != c->data->skylight || c->g->a != c->data->block_light || r->blocks.m == NULL ||
        r->blocks.q == NULL || COUNT(r->blocks.named) != 96)
        return false;
    s->data = *c->data;
    memcpy(s->blocks, c->data->blocks, sizeof(s->blocks));
    s->e = c->e;
    s->f = c->f;
    s->g = c->g;
    s->h = c->h;
    s->metadata = *c->e;
    s->m = r->blocks.m;
    s->q = r->blocks.q;
    memcpy(s->elements, r->blocks.m, sizeof(s->elements));
    memcpy(s->named, r->blocks.named, sizeof(s->named));
    return true;
}

static void oc_restore(const oc_saved *s, mc_world_chunk *c, mc_registry_classes *r) {
    *c->data = s->data;
    memcpy(c->data->blocks, s->blocks, sizeof(s->blocks));
    c->e = s->e;
    c->f = s->f;
    c->g = s->g;
    c->h = s->h;
    *s->e = s->metadata;
    r->blocks.m = s->m;
    r->blocks.q = s->q;
    memcpy(r->blocks.m, s->elements, sizeof(s->elements));
    memcpy(r->blocks.named, s->named, sizeof(s->named));
    /* Only controlled arrays/references restored. Actual dirty flags, native
     * leases, class phases, queues, diagnostics and Random effects survive. */
}

static void oc_seed(mc_java_random *r, int64_t seed) {
    mc_java_random_seed(r, seed);
    (void)mc_java_random_next_gaussian(r); /* Actual valid primed cache. */
}

/* Invocation budget only; no centers/radii/voxel loop is computed here.
 * Calling core Random is a shared-module native contract, not JVM replay. */
static bool oc_budget(mc_java_random *r, unsigned int segments) {
    int32_t ignored;
    (void)mc_java_random_next_float(r);
    if (!mc_java_random_next_bounded(r, 3, &ignored) ||
        !mc_java_random_next_bounded(r, 3, &ignored))
        return false;
    for (unsigned int i = 0; i < segments; ++i)
        (void)mc_java_random_next_double(r);
    return true;
}

static bool oc_find_probe(oc_probe *p) {
    for (int64_t seed = 0; seed < 65536; ++seed) {
        mc_java_random r;
        int32_t first, second;
        oc_seed(&r, seed);
        const float fraction = mc_java_random_next_float(&r);
        if (!mc_java_random_next_bounded(&r, 3, &first) ||
            !mc_java_random_next_bounded(&r, 3, &second))
            return false;
        const double radius_fraction = mc_java_random_next_double(&r);
        if (fraction >= 0.24f && fraction <= 0.26f && radius_fraction >= 0.8 && (first & 1) == 0) {
            *p = (oc_probe){seed, 42 + first};
            return true;
        }
    }
    return false; /* Test-input search failure never becomes feature success. */
}

static bool oc_trig_precondition(void) {
    if (trig.values[0] != 0.0f)
        return false;
    for (size_t i = 0; i < MC_MATH_TRIG_VALUES; ++i)
        if (!(trig.values[i] >= -1.0f && trig.values[i] <= 1.0f))
            return false;
    /* Broad conservative lookup ranges for the selected first angle and its
     * cosine. These are input bounds, not C-feature-derived geometry outputs. */
    for (size_t i = 7860; i <= 8525; ++i)
        if (!(trig.values[i] >= 0.68f && trig.values[i] <= 0.74f))
            return false;
    for (size_t i = 24244; i <= 24909; ++i)
        if (!(trig.values[i] >= 0.68f && trig.values[i] <= 0.74f))
            return false;
    return true;
}

static mc_world_storage_status oc_place(bool clay, const mc_population_ore_feature *ore,
                                        const mc_population_clay_feature *deposit,
                                        mc_world_runtime *w, mc_java_random *r, int32_t x,
                                        int32_t y, int32_t z, bool *out,
                                        mc_population_ore_clay_error *e) {
    return clay ? mc_population_clay_place(deposit, w, r, x, y, z, out, e)
                : mc_population_ore_place(ore, w, r, x, y, z, out, e);
}

static bool oc_prime_registry_error(mc_world_runtime *w, mc_world_chunk *c, mc_registry_classes *r,
                                    uint8_t stone) {
    const size_t cell = oc_index(0, 40, 0);
    const uint8_t old = c->data->blocks[cell];
    mc_block **m = r->blocks.m;
    c->data->blocks[cell] = stone;
    r->blocks.m = NULL;
    mc_material *value = NULL;
    mc_world_material_error e;
    const mc_world_storage_status status =
        mc_world_runtime_material_get_detailed(w, 0, 40, 0, &value, &e);
    r->blocks.m = m;
    c->data->blocks[cell] = old;
    return status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && value == NULL && e.bytecode == 23 &&
           e.registry.status == MC_REGISTRY_NULL_REFERENCE;
}

static bool oc_constructor_contracts(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *w = f->worlds[side], *unbound = NULL, *retired = NULL;
    mc_world_runtime_classes *classes = NULL;
    const mc_world_runtime_entry entry =
        side == 0 ? MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE : MC_WORLD_RUNTIME_SERVER_LOAD_OR_CREATE;
    const mc_block *clay;
    CHECK(oc_block(f, side, "aX", &clay));
    const oc_profile profile = oc_profile_read(f->profile);
    const mc_java_random world_random = *w->fields.r;
    mc_population_ore_feature ore = {0};
    CHECK(mc_population_ore_feature_init(&ore, INT32_MIN, INT32_MAX) == MC_WORLD_STORAGE_OK &&
          ore.a == INT32_MIN && ore.b == INT32_MAX);
    mc_population_clay_feature deposit = {0};
    mc_population_ore_clay_error e;
    CHECK(mc_population_clay_feature_init(&deposit, w, -7, &e) == MC_WORLD_STORAGE_OK &&
          deposit.a == clay->fields.bn && deposit.b == -7 && oc_cleared(&e));
    CHECK(mc_population_clay_feature_init(NULL, w, 32, &e) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          e.bytecode == 0 && w->active_calls == 0);
    mc_population_clay_feature null_world = {0};
    CHECK(mc_population_clay_feature_init(&null_world, NULL, 32, &e) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          null_world.a == 0 && null_world.b == 0 && e.bytecode == 0);
    const mc_population_ore_feature before_ore = ore;
    const mc_population_clay_feature before_clay = deposit;
    const unsigned int csr = _mm_getcsr();
    _mm_setcsr((csr & ~0x6000u) | 0x2000u);
    const mc_world_storage_status os = mc_population_ore_feature_scale(&ore, NAN, INFINITY, -0.0);
    const mc_world_storage_status cs =
        mc_population_clay_feature_scale(&deposit, -INFINITY, NAN, 0.0);
    _mm_setcsr(csr);
    CHECK(os == MC_WORLD_STORAGE_OK && cs == MC_WORLD_STORAGE_OK && ore.a == before_ore.a &&
          ore.b == before_ore.b && deposit.a == before_clay.a && deposit.b == before_clay.b &&
          mc_population_ore_feature_init(NULL, 0, 0) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          mc_population_ore_feature_scale(NULL, 0, 0, 0) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          mc_population_clay_feature_scale(NULL, 0, 0, 0) == MC_WORLD_STORAGE_INVALID_ARGUMENT);
    mc_block **cell = oc_named(f->registry[side], "aX");
    CHECK(cell != NULL);
    mc_block *saved = *cell;
    *cell = NULL;
    mc_population_clay_feature failed = {0};
    const mc_world_storage_status status = mc_population_clay_feature_init(&failed, w, 32, &e);
    *cell = saved; /* Restore before any assertion can escape. */
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && failed.a == 0 && failed.b == 0 &&
          e.bytecode == 8 && e.registry.status == MC_REGISTRY_NULL_REFERENCE &&
          e.feature_method != NULL && strcmp(e.feature_method, "<init>") == 0 &&
          same_cause(&e.cause, mc_world_runtime_last_error(w)) && w->active_calls == 0);
    /* Genuine allocated RAW World, actual warm bound class; no fake phase. */
    mc_population_clay_feature raw = {0};
    CHECK(mc_population_clay_feature_init(&raw, f->worlds[4u + side], 32, &e) ==
              MC_WORLD_STORAGE_OK &&
          raw.a == clay->fields.bn && raw.b == 32 && oc_cleared(&e));
    /* Separate real unbound class/RAW owner tests the native join at static5.
     * This is NOT Java failed-clinit or a detached successful Registry. */
    CHECK(mc_world_runtime_classes_create(side == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER,
                                          f->profile, &trig, &classes,
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_allocate_world(entry, classes, NULL, &unbound, NULL) ==
          MC_WORLD_STORAGE_OK);
    mc_population_clay_feature missing = {0};
    CHECK(mc_population_clay_feature_init(&missing, unbound, 16, &e) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          e.bytecode == 5 && e.registry.status == MC_REGISTRY_INVALID_ARGUMENT && missing.a == 0 &&
          missing.b == 0 && unbound->active_calls == 0);
    CHECK(mc_world_runtime_allocate_world(entry, f->classes[side], NULL, &retired, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_retire_world(retired, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_storage_error prior = *mc_world_runtime_last_error(retired);
    mc_java_random random, expected;
    oc_seed(&random, 301);
    expected = random;
    bool out = true;
    CHECK(mc_population_ore_place(&ore, retired, &random, 0, 40, 0, &out, &e) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          out && e.bytecode == 0 && mc_test_java_random_equal(&random, &expected) &&
          same_cause(&prior, mc_world_runtime_last_error(retired)));
    CHECK(oc_profile_same(f->profile, &profile) &&
          mc_test_java_random_equal(w->fields.r, &world_random) && w->active_calls == 0);
    ok = true;
cleanup:
    if (retired != NULL && mc_world_runtime_free(retired, NULL) != MC_WORLD_STORAGE_OK)
        ok = false;
    if (unbound != NULL && (mc_world_runtime_retire_world(unbound, NULL) != MC_WORLD_STORAGE_OK ||
                            mc_world_runtime_free(unbound, NULL) != MC_WORLD_STORAGE_OK))
        ok = false;
    if (classes != NULL && mc_world_runtime_classes_free(classes, NULL) != MC_WORLD_STORAGE_OK)
        ok = false;
    return ok;
}

static bool oc_trial(fixture *f, size_t side, const oc_family *family, oc_control control,
                     const oc_probe *probe, bool alias_world_random) {
    bool ok = false, prepared = false, water_changed = false;
    oc_saved *saved = NULL;
    uint8_t *before = NULL, *expected_blocks = NULL, *short_metadata = NULL;
    mc_world_runtime *w = f->worlds[side];
    mc_registry_classes *r = f->registry[side];
    mc_world_chunk *c = mc_world_store_chunk_object(f->stores[side], f->normal[side][0]);
    const mc_block *stone = NULL, *sand = NULL, *dirt = NULL, *water_block = NULL, *target = NULL;
    const bool clay = family->clay;
    const bool remote = control == OC_REMOTE_GRAVEL;
    const bool old_remote = w->fields.B;
    mc_material *water_material = NULL;
    CHECK(!old_remote && oc_block(f, side, "u", &stone) && oc_block(f, side, "F", &sand) &&
          oc_block(f, side, "w", &dirt) && oc_block(f, side, "C", &water_block) &&
          oc_block(f, side, family->target, &target));
    CHECK(family->size <= 32 && oc_named(r, clay ? "F" : "u") != NULL &&
          oc_named(r, "aX") != NULL && oc_trig_precondition());
    saved = malloc(sizeof(*saved));
    before = malloc(MC_CHUNK_BLOCKS);
    expected_blocks = malloc(MC_CHUNK_BLOCKS);
    CHECK(saved != NULL && before != NULL && expected_blocks != NULL && oc_save(saved, c, r));
    prepared = true;
    /* Actual prepared arrays, with no cache, phase, lease or ledger mutation. */
    memset(c->data->blocks,
           (uint8_t)(control == OC_VOLUME ? (clay ? sand->fields.bn : stone->fields.bn)
                                          : dirt->fields.bn),
           MC_CHUNK_BLOCKS);
    memset(c->data->metadata, 0xaa, MC_CHUNK_NIBBLE_BYTES);
    memset(c->data->skylight, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(c->data->block_light, 0x88, MC_CHUNK_NIBBLE_BYTES);
    memset(c->data->heightmap, 127, MC_CHUNK_HEIGHTMAP_BYTES);
    c->data->minimum_height = 127;
    const int32_t offset = family->size == 32 ? 2 : family->size == 16 ? 1 : 0;
    const int32_t px = 8 + offset, pz = 8 + offset, py = probe->y_start;
    const size_t cell = oc_index(px, py, pz), lower = oc_index(px, py - 1, pz);
    const size_t origin = oc_index(0, 40, 0);
    const int32_t replace = clay ? sand->fields.bn : stone->fields.bn;
    if (control != OC_VOLUME && control != OC_NO_REPLACEMENT && control != OC_REJECTED_GEOMETRY)
        c->data->blocks[cell] = (uint8_t)replace;
    if (control == OC_TWO_WRITES)
        c->data->blocks[lower] = (uint8_t)replace;
    if (clay)
        c->data->blocks[origin] = (uint8_t)water_block->fields.bn;
    CHECK(oc_prime_registry_error(w, c, r, (uint8_t)stone->fields.bn));
    const mc_registry_error ambient = *mc_world_runtime_last_registry_error(w);
    mc_population_ore_feature ore = {0};
    mc_population_clay_feature deposit = {0};
    mc_population_ore_clay_error e;
    const int32_t argument = control == OC_REJECTED_GEOMETRY || control == OC_NO_REPLACEMENT
                                 ? INT32_MIN
                                 : target->fields.bn;
    CHECK(mc_population_ore_feature_init(&ore, argument, family->size) == MC_WORLD_STORAGE_OK);
    if (clay)
        CHECK(mc_population_clay_feature_init(&deposit, w, family->size, &e) ==
                  MC_WORLD_STORAGE_OK &&
              deposit.a == target->fields.bn && deposit.b == family->size && oc_cleared(&e));
    if (control == OC_EQUAL) {
        /* Prepared actual retained a field, preserving the real replacement
         * comparison and old==requestedID ordinary false setter path. */
        ore.a = replace;
        deposit.a = replace;
    }
    if (control == OC_NO_REPLACEMENT && clay)
        deposit.a = INT32_MIN;
    if (control == OC_TWO_WRITES) {
        /* A real allocated shorter byte array installed into the actual
         * existing Nibble.a field. length equals its actual allocation, not a
         * fabricated owner/counter. Even py separates consecutive byte cells. */
        CHECK((py & 1) == 0);
        const size_t cut = cell / 2u;
        CHECK(lower / 2u < cut && cut < MC_CHUNK_NIBBLE_BYTES);
        short_metadata = malloc(cut);
        CHECK(short_metadata != NULL);
        memcpy(short_metadata, c->data->metadata, cut);
        c->e->a = short_metadata;
        c->e->length = cut;
    }
    if (control == OC_LATE_STATIC || control == OC_REJECTED_GEOMETRY)
        *oc_named(r, clay ? "F" : "u") = NULL;
    if (control == OC_CAPTURED_CLAY_TARGET)
        *oc_named(r, "aX") = NULL;
    if (control == OC_MUTATION_M || control == OC_MATERIAL_M || (control == OC_EQUAL && !clay))
        r->blocks.m = NULL;
    if (control == OC_MUTATION_Q || control == OC_EQUAL)
        r->blocks.q = NULL;
    if (control == OC_MUTATION_METADATA || control == OC_EQUAL)
        c->e = NULL;
    if (control == OC_NONWATER || control == OC_NONWATER_BAD_FP)
        c->data->blocks[origin] = 0;
    if (control == OC_NULL_MATERIAL) {
        water_material = water_block->fields.bA;
        ((mc_block *)water_block)->fields.bA = NULL;
        water_changed = true;
    }
    if (remote)
        w->fields.B = true; /* Explicit actual source-field input control. */
    memcpy(before, c->data->blocks, MC_CHUNK_BLOCKS);
    memcpy(expected_blocks, before, MC_CHUNK_BLOCKS);
    const bool gate_false = clay && (control == OC_NONWATER || control == OC_NONWATER_BAD_FP ||
                                     control == OC_NULL_MATERIAL);
    const bool before_random =
        gate_false || control == OC_MATERIAL_M || control == OC_NULL_RANDOM || control == OC_BAD_FP;
    const bool mutation_failure = control == OC_MUTATION_M || control == OC_MUTATION_Q ||
                                  control == OC_MUTATION_METADATA || control == OC_TWO_WRITES;
    const bool failure = control == OC_MATERIAL_M || control == OC_NULL_RANDOM ||
                         control == OC_BAD_FP || control == OC_LATE_STATIC || mutation_failure;
    const bool changes = (!failure && !gate_false && control != OC_NO_REPLACEMENT &&
                          control != OC_EQUAL && control != OC_REJECTED_GEOMETRY) ||
                         mutation_failure;
    if (changes && control != OC_VOLUME)
        expected_blocks[cell] = (uint8_t)(clay ? deposit.a : ore.a);
    if (control == OC_TWO_WRITES)
        expected_blocks[lower] = (uint8_t)(clay ? deposit.a : ore.a);
    mc_java_random local, expected;
    mc_java_random *actual = alias_world_random ? w->fields.r : &local;
    oc_seed(actual, probe->seed);
    expected = *actual;
    const mc_java_random world_random = *w->fields.r;
    bool budget_ok = true;
    const unsigned int segments = family->size < 0 ? 0u : (unsigned int)family->size + 1u;
    if (!before_random)
        budget_ok =
            oc_budget(&expected, control == OC_LATE_STATIC || mutation_failure ? 1u : segments);
    const oc_profile profile = oc_profile_read(f->profile);
    const size_t count = f->providers[side].cache->count;
    const size_t insertions = f->providers[side].cache->insertion_count;
    const bool initial_output = ((unsigned int)control & 1u) != 0;
    bool out = initial_output;
    mc_java_random *supplied =
        control == OC_NULL_RANDOM || control == OC_NONWATER_BAD_FP ? NULL : actual;
    const unsigned int csr = _mm_getcsr();
    if (control == OC_BAD_FP || control == OC_NONWATER_BAD_FP)
        _mm_setcsr((csr & ~0x6000u) | 0x2000u);
    const mc_world_storage_status status =
        oc_place(clay, &ore, &deposit, w, supplied, 0, 40, 0, &out, &e);
    if (control == OC_BAD_FP || control == OC_NONWATER_BAD_FP)
        _mm_setcsr(csr); /* Restore only the deliberately changed FP control. */
    bool layout = true, metadata_ok = true, secondary = true;
    size_t changed = 0;
    const int32_t wanted = clay ? deposit.a : ore.a;
    if (control != OC_VOLUME)
        layout = memcmp(c->data->blocks, expected_blocks, MC_CHUNK_BLOCKS) == 0;
    for (int32_t x = 0; x < 16; ++x)
        for (int32_t z = 0; z < 16; ++z)
            for (int32_t y = 0; y < 128; ++y) {
                const size_t i = oc_index(x, y, z);
                const bool different = c->data->blocks[i] != before[i];
                if (different)
                    ++changed;
                if (control == OC_VOLUME && different)
                    layout = layout && before[i] == replace && c->data->blocks[i] == wanted &&
                             x >= 1 && x <= 14 && z >= 1 && z <= 14 && y >= 36 && y <= 50;
                unsigned int wanted_metadata = 10u;
                if (control == OC_VOLUME && different)
                    wanted_metadata = 0;
                else if (i == cell && changes && control != OC_MUTATION_M &&
                         control != OC_MUTATION_METADATA && control != OC_TWO_WRITES)
                    wanted_metadata = 0;
                metadata_ok = metadata_ok && oc_nibble(c->data->metadata, i) == wanted_metadata;
            }
    if (control == OC_VOLUME)
        layout = layout && changed > 0;
    else
        layout = layout && changed == (changes ? (control == OC_TWO_WRITES ? 2u : 1u) : 0u);
    if (control == OC_TWO_WRITES) {
        secondary = c->e != NULL && c->e->a == short_metadata && c->e->length == cell / 2u &&
                    oc_nibble(short_metadata, lower) == 0 && mc_world_chunk_state(c)->dirty;
        /* The second source byte was stored before its metadata byte failed;
         * the first call's actual external Nibble byte remains committed. */
    }
    for (size_t i = 0; i < MC_CHUNK_NIBBLE_BYTES; ++i)
        secondary = secondary && c->data->skylight[i] == 0 && c->data->block_light[i] == 0x88;
    for (size_t i = 0; i < MC_CHUNK_HEIGHTMAP_BYTES; ++i)
        secondary = secondary && c->data->heightmap[i] == 127;
    secondary = secondary && c->data->minimum_height == 127;
    const bool continuation = mc_test_java_random_equal(actual, &expected);
    const bool separate_world =
        alias_world_random || mc_test_java_random_equal(w->fields.r, &world_random);
    const bool globals = oc_profile_same(f->profile, &profile);
    const bool retained = same_cause(&e.cause, mc_world_runtime_last_error(w));
    const bool ambient_retained =
        clay || same_registry(&ambient, mc_world_runtime_last_registry_error(w));
    const bool registry_matches_world =
        same_registry(&e.registry, mc_world_runtime_last_registry_error(w));
    if (water_changed) {
        ((mc_block *)water_block)->fields.bA = water_material;
        water_changed = false;
    }
    w->fields.B = old_remote;
    oc_restore(saved, c, r);
    prepared = false; /* No assertion can strand a nullable/static/array control. */
    CHECK(budget_ok && continuation && separate_world && globals && layout && metadata_ok &&
          secondary && w->active_calls == 0 && f->providers[side].cache->count == count &&
          f->providers[side].cache->insertion_count == insertions &&
          mc_chunk_cache_validate(f->providers[side].cache) &&
          mc_chunk_pending_contains(f->providers[side].pending, mc_chunk_cache_key(0, 0)));
    if (!failure) {
        CHECK(status == MC_WORLD_STORAGE_OK && out == !gate_false && oc_cleared(&e));
    } else {
        CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out == initial_output && retained &&
              e.feature_class != NULL &&
              strcmp(e.feature_class,
                     clay ? (side == 0 ? "ms" : "ib") : (side == 0 ? "fl" : "dr")) == 0 &&
              e.feature_method != NULL && strcmp(e.feature_method, "a") == 0 &&
              e.operation != NULL);
        if (control == OC_MATERIAL_M) {
            CHECK(e.bytecode == 6 && e.material.bytecode == 23 &&
                  e.registry.status == MC_REGISTRY_NULL_REFERENCE &&
                  same_registry(&e.registry, &e.material.registry) &&
                  same_cause(&e.cause, &e.material.cause) && registry_matches_world);
        } else if (control == OC_NULL_RANDOM || control == OC_BAD_FP) {
            const mc_registry_error empty = oc_empty_registry();
            CHECK(e.bytecode == (clay ? 18u : 1u) && same_registry(&e.registry, &empty) &&
                  same_registry(&e.material.registry, &empty) &&
                  same_registry(&e.mutation.registry, &empty) && ambient_retained);
        } else if (control == OC_LATE_STATIC) {
            CHECK(e.bytecode == (clay ? 497u : 502u) &&
                  e.registry.status == MC_REGISTRY_NULL_REFERENCE && registry_matches_world);
        } else {
            CHECK(e.bytecode == (clay ? 514u : 519u) && e.mutation.status == status &&
                  same_cause(&e.cause, &e.mutation.cause) &&
                  same_registry(&e.registry, &e.mutation.registry));
            if (control == OC_MUTATION_M || control == OC_MUTATION_Q)
                CHECK(e.mutation.chunk_bytecode == (control == OC_MUTATION_M ? 107u : 140u) &&
                      e.registry.status == MC_REGISTRY_NULL_REFERENCE && registry_matches_world);
            else {
                CHECK(control == OC_MUTATION_METADATA || control == OC_TWO_WRITES);
                const mc_registry_error empty = oc_empty_registry();
                CHECK(e.mutation.chunk_bytecode == 128 && same_registry(&e.registry, &empty) &&
                      ambient_retained);
            }
        }
    }
    ok = true;
cleanup:
    if (water_changed)
        ((mc_block *)water_block)->fields.bA = water_material;
    w->fields.B = old_remote;
    if (prepared)
        oc_restore(saved, c, r);
    free(short_metadata);
    free(expected_blocks);
    free(before);
    free(saved);
    return ok;
}

static bool oc_raw_and_admission(fixture *f, size_t side, const oc_probe *probe) {
    bool ok = false;
    const mc_registry_error empty = oc_empty_registry();
    mc_population_ore_feature ore = {0};
    mc_population_clay_feature deposit = {0};
    mc_population_ore_clay_error e;
    mc_java_random random, expected;
    CHECK(mc_population_ore_feature_init(&ore, INT32_MIN, -1) == MC_WORLD_STORAGE_OK &&
          mc_population_clay_feature_init(&deposit, f->worlds[side], 32, &e) ==
              MC_WORLD_STORAGE_OK);
    for (size_t phase = 0; phase < 2; ++phase) {
        mc_world_runtime *raw = f->worlds[4u + 2u * phase + side];
        CHECK(raw != NULL);
        const oc_profile profile = oc_profile_read(f->profile);
        for (unsigned int zero = 0; zero < 2; ++zero) {
            ore.b = zero == 0 ? -1 : 0;
            oc_seed(&random, probe->seed);
            expected = random;
            CHECK(oc_budget(&expected, zero));
            bool out = false;
            CHECK(mc_population_ore_place(&ore, raw, &random, 0, 40, 0, &out, &e) ==
                      MC_WORLD_STORAGE_OK &&
                  out && oc_cleared(&e) && raw->active_calls == 0 &&
                  mc_test_java_random_equal(&random, &expected));
        }
        bool out = true;
        const unsigned int csr = _mm_getcsr();
        _mm_setcsr((csr & ~0x6000u) | 0x2000u);
        const mc_world_storage_status rejected =
            mc_population_clay_place(&deposit, raw, NULL, INT32_MAX, 40, 0, &out, &e);
        _mm_setcsr(csr);
        CHECK(rejected == MC_WORLD_STORAGE_OK && !out && oc_cleared(&e));
        oc_seed(&random, probe->seed);
        expected = random;
        CHECK(oc_budget(&expected, 1));
        ore.b = 8;
        out = true;
        mc_block **late = oc_named(f->registry[side], "u");
        CHECK(late != NULL);
        mc_block *saved_late = *late;
        *late = NULL; /* Missing future static cannot preempt the actual getter. */
        const mc_world_storage_status missing_getter =
            mc_population_ore_place(&ore, raw, &random, 0, 40, 0, &out, &e);
        *late = saved_late;
        CHECK(missing_getter == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out && e.bytecode == 496 &&
              same_registry(&e.registry, &empty) && mc_test_java_random_equal(&random, &expected) &&
              raw->active_calls == 0 && oc_profile_same(f->profile, &profile));
    }
    oc_seed(&random, probe->seed);
    expected = random;
    bool out = true;
    CHECK(mc_population_ore_place(NULL, f->worlds[side], &random, 0, 40, 0, &out, &e) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          out && e.bytecode == 0 && mc_test_java_random_equal(&random, &expected));
    CHECK(mc_population_ore_place(&ore, NULL, &random, 0, 40, 0, &out, &e) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          out && e.bytecode == 0 && mc_test_java_random_equal(&random, &expected));
    CHECK(mc_population_clay_place(&deposit, f->worlds[side], &random, 0, 40, 0, NULL, &e) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          e.bytecode == 0 && mc_test_java_random_equal(&random, &expected));
    ok = true;
cleanup:
    return ok;
}

static bool oc_wrapped_no_candidates(fixture *f, size_t side, const oc_probe *probe) {
    bool ok = false;
    mc_world_runtime *w = f->worlds[side];
    const oc_profile profile = oc_profile_read(f->profile);
    const mc_java_random world_random = *w->fields.r;
    for (unsigned int zero = 0; zero < 2; ++zero) {
        mc_population_ore_feature ore = {0};
        CHECK(mc_population_ore_feature_init(&ore, INT32_MIN, zero == 0 ? INT32_MIN : 0) ==
              MC_WORLD_STORAGE_OK);
        mc_java_random random, expected;
        oc_seed(&random, probe->seed);
        expected = random;
        CHECK(oc_budget(&expected, zero));
        bool out = false;
        mc_population_ore_clay_error e;
        CHECK(mc_population_ore_place(&ore, w, &random, INT32_MAX, INT32_MAX, INT32_MIN, &out,
                                      &e) == MC_WORLD_STORAGE_OK &&
              out && oc_cleared(&e) && mc_test_java_random_equal(&random, &expected) &&
              w->active_calls == 0);
    }
    CHECK(oc_profile_same(f->profile, &profile) &&
          mc_test_java_random_equal(w->fields.r, &world_random));
    ok = true;
cleanup:
    return ok;
}

static bool population_ore_clay_contracts(fixture *f, size_t side) {
    bool ok = false;
    oc_probe probe;
    CHECK(side < 2 && (_mm_getcsr() & 0xffc0u) == 0x1f80u && oc_find_probe(&probe) &&
          oc_constructor_contracts(f, side) && oc_raw_and_admission(f, side, &probe) &&
          oc_wrapped_no_candidates(f, side, &probe));
    /* Actual provider target names and signed sizes; no cached numeric IDs. */
    const oc_family families[] = {{true, "aX", 32}, {false, "w", 32}, {false, "G", 32},
                                  {false, "J", 16}, {false, "I", 8},  {false, "H", 8},
                                  {false, "aO", 7}, {false, "ax", 7}, {false, "O", 6}};
    for (size_t i = 0; i < COUNT(families); ++i) {
        const oc_family *family = &families[i];
        CHECK(oc_trial(f, side, family, OC_SINGLE, &probe, (i & 1u) != 0));
        CHECK(oc_trial(f, side, family, OC_NO_REPLACEMENT, &probe, false));
        CHECK(oc_trial(f, side, family, OC_EQUAL, &probe, true));
        /* G now reaches its complete ordinary added hook on both single-cell
         * and volume paths. Keep the separate remote-field control below. */
        CHECK(oc_trial(f, side, family, OC_VOLUME, &probe, (i & 1u) == 0));
        if (!family->clay && strcmp(family->target, "G") == 0)
            CHECK(oc_trial(f, side, family, OC_REMOTE_GRAVEL, &probe, false));
    }
    const oc_family ore = {false, "J", 8}, clay = {true, "aX", 32};
    const oc_control ore_errors[] = {
        OC_LATE_STATIC, OC_MUTATION_M,  OC_MUTATION_Q, OC_MUTATION_METADATA,
        OC_TWO_WRITES,  OC_NULL_RANDOM, OC_BAD_FP};
    for (size_t i = 0; i < COUNT(ore_errors); ++i)
        CHECK(oc_trial(f, side, &ore, ore_errors[i], &probe, (i & 1u) != 0));
    const oc_control clay_controls[] = {
        OC_LATE_STATIC, OC_MUTATION_Q,      OC_MUTATION_METADATA,   OC_TWO_WRITES,
        OC_NONWATER,    OC_NONWATER_BAD_FP, OC_NULL_MATERIAL,       OC_MATERIAL_M,
        OC_NULL_RANDOM, OC_BAD_FP,          OC_CAPTURED_CLAY_TARGET};
    for (size_t i = 0; i < COUNT(clay_controls); ++i)
        CHECK(oc_trial(f, side, &clay, clay_controls[i], &probe, (i & 1u) == 0));
    const int32_t sizes[] = {INT32_MIN, -32, -1, 0, 1};
    for (size_t kind = 0; kind < 2; ++kind)
        for (size_t i = 0; i < COUNT(sizes); ++i) {
            const oc_family degenerate = {kind != 0, kind == 0 ? "J" : "aX", sizes[i]};
            CHECK(oc_trial(f, side, &degenerate, OC_REJECTED_GEOMETRY, &probe, (i & 1u) != 0));
        }
    CHECK(f->worlds[side]->active_calls == 0 && mc_chunk_cache_validate(f->providers[side].cache));
    ok = true;
cleanup:
    return ok;
}
