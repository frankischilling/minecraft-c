/* Native equivalence controls only; the 950 original EW rows stay unchanged.
 * Fault inputs and their restoration are outside each independent invocation. */
#include "block/registry/registry_internal.h"
#include "block/registry/runtime_internal.h"
#include "world/runtime/chunk_cache_internal.h"
#include <stddef.h>

typedef enum ew_getter_kind {
    EW_GET_BRIGHTNESS,
    EW_GET_MINIMUM,
    EW_GET_MATERIAL,
    EW_GET_OPAQUE,
    EW_GET_NORMAL,
    EW_GET_MANAGER,
    EW_GET_COUNT
} ew_getter_kind;

typedef union ew_getter_output {
    float brightness;
    bool predicate;
    mc_material *material;
    mc_world_chunk_manager *manager;
} ew_getter_output;

static mc_client_chunk_cache_status ew_getter_call(ew_getter_kind kind, bool failure_only,
                                                   mc_client_chunk_cache *cache, int32_t x,
                                                   int32_t y, int32_t z, int32_t minimum,
                                                   ew_getter_output *out,
                                                   mc_client_chunk_cache_error *error) {
    switch (kind) {
        case EW_GET_BRIGHTNESS:
            return failure_only ? mc_client_chunk_cache_brightness_failure(
                                      cache, x, y, z, out == NULL ? NULL : &out->brightness, error)
                                : mc_client_chunk_cache_brightness(
                                      cache, x, y, z, out == NULL ? NULL : &out->brightness, error);
        case EW_GET_MINIMUM:
            return failure_only
                       ? mc_client_chunk_cache_min_brightness_failure(
                             cache, x, y, z, minimum, out == NULL ? NULL : &out->brightness, error)
                       : mc_client_chunk_cache_min_brightness(
                             cache, x, y, z, minimum, out == NULL ? NULL : &out->brightness, error);
        case EW_GET_MATERIAL:
            return failure_only ? mc_client_chunk_cache_material_failure(
                                      cache, x, y, z, out == NULL ? NULL : &out->material, error)
                                : mc_client_chunk_cache_material(
                                      cache, x, y, z, out == NULL ? NULL : &out->material, error);
        case EW_GET_OPAQUE:
            return failure_only ? mc_client_chunk_cache_opaque_failure(
                                      cache, x, y, z, out == NULL ? NULL : &out->predicate, error)
                                : mc_client_chunk_cache_opaque(
                                      cache, x, y, z, out == NULL ? NULL : &out->predicate, error);
        case EW_GET_NORMAL:
            return failure_only ? mc_client_chunk_cache_normal_cube_failure(
                                      cache, x, y, z, out == NULL ? NULL : &out->predicate, error)
                                : mc_client_chunk_cache_normal_cube(
                                      cache, x, y, z, out == NULL ? NULL : &out->predicate, error);
        case EW_GET_MANAGER:
            return failure_only ? mc_client_chunk_cache_manager_failure(
                                      cache, out == NULL ? NULL : &out->manager, error)
                                : mc_client_chunk_cache_manager(
                                      cache, out == NULL ? NULL : &out->manager, error);
        default:
            return MC_CLIENT_CHUNK_CACHE_INVALID_ARGUMENT;
    }
}

static uint32_t ew_getter_float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool ew_getter_output_equal(ew_getter_kind kind, const ew_getter_output *a,
                                   const ew_getter_output *b) {
    if (kind == EW_GET_BRIGHTNESS || kind == EW_GET_MINIMUM)
        return ew_getter_float_bits(a->brightness) == ew_getter_float_bits(b->brightness);
    if (kind == EW_GET_MATERIAL)
        return a->material == b->material;
    if (kind == EW_GET_MANAGER)
        return a->manager == b->manager;
    return a->predicate == b->predicate;
}

static uint64_t ew_getter_planes(picking_fixture *f) {
    uint64_t hash = UINT64_C(1469598103934665603);
    for (size_t i = 0; i < 4; ++i) {
        const mc_chunk_data *data = f->chunks[i]->data;
        const uint8_t *planes[] = {data->blocks, data->metadata, data->skylight, data->block_light,
                                   data->heightmap};
        const size_t lengths[] = {MC_CHUNK_BLOCKS, MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_NIBBLE_BYTES,
                                  MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_HEIGHTMAP_BYTES};
        for (size_t p = 0; p < 5; ++p)
            if (planes[p] != NULL)
                for (size_t j = 0; j < lengths[p]; ++j)
                    hash = (hash ^ planes[p][j]) * UINT64_C(1099511628211);
    }
    return hash;
}

static bool ew_getter_registry_equal(const mc_registry_error *a, const mc_registry_error *b) {
    mc_client_chunk_cache_error *left = calloc(1, sizeof(*left));
    mc_client_chunk_cache_error *right = calloc(1, sizeof(*right));
    CHECK(left != NULL && right != NULL);
    left->registry = *a;
    right->registry = *b;
    const bool equal = ew_metadata_error_equal(left, right);
    free(left);
    free(right);
    return equal;
}

static bool ew_getter_provider_equal(const mc_chunk_provider_cache_observation *a,
                                     const mc_chunk_provider_cache_observation *b) {
    return a->owned == b->owned && a->map_count == b->map_count &&
           a->insertion_count == b->insertion_count && a->index_capacity == b->index_capacity &&
           a->index_bytes == b->index_bytes && a->mutation_depth == b->mutation_depth &&
           a->direct_bytes == b->direct_bytes && a->hits == b->hits && a->misses == b->misses &&
           a->generic_gets == b->generic_gets && a->raw_exports == b->raw_exports &&
           a->rebuilds == b->rebuilds;
}

/* Each variant starts from the same caller input. Source effects are observed
 * before restoration: positive SkyLight may set actual lm.a, and a failed
 * Registry element/active use may update its actual diagnostic record. */
static bool ew_getter_pair(picking_fixture *f, mc_client_chunk_cache *cache, ew_getter_kind kind,
                           int32_t x, int32_t y, int32_t z, int32_t minimum, bool output,
                           mc_client_chunk_cache_status wanted, unsigned int wanted_pc,
                           bool sets_chunk_a, const ew_getter_output *wanted_output,
                           mc_client_chunk_cache_error *failure_receipt) {
    mc_client_chunk_cache_error *errors = malloc(3 * sizeof(*errors));
    unsigned char *sentinel = malloc(sizeof(*errors));
    unsigned char *world_before = malloc(sizeof(*f->base.world));
    unsigned char *classes_before = malloc(sizeof(*f->base.world_classes));
    unsigned char *classes_after = malloc(sizeof(*f->base.world_classes));
    mc_registry_classes *registry = f->base.world_classes->registry;
    unsigned char *registry_before = registry == NULL ? NULL : malloc(sizeof(*registry));
    unsigned char *registry_after = registry == NULL ? NULL : malloc(sizeof(*registry));
    CHECK(errors != NULL && sentinel != NULL && world_before != NULL && classes_before != NULL &&
          classes_after != NULL &&
          (registry == NULL || (registry_before != NULL && registry_after != NULL)));
    mc_registry_error prior_registry = {0}, returned_registry = {0};
    if (registry != NULL) {
        memcpy(registry_before, registry, sizeof(*registry));
        prior_registry = registry->last_error;
        memset(registry_before + offsetof(mc_registry_classes, last_error), 0,
               sizeof(registry->last_error));
    }
    memcpy(world_before, f->base.world, sizeof(*f->base.world));
    memcpy(classes_before, f->base.world_classes, sizeof(*f->base.world_classes));
    memset(classes_before + offsetof(mc_world_runtime_classes, chunk_a), 0, sizeof(bool));
    mc_client_chunk_cache owner;
    mc_client_chunk_cache_occurrence occurrences[4];
    store_chunk *entries[4] = {0};
    size_t leases[4] = {0};
    const size_t count = cache == NULL ? 0 : cache->cell_count;
    CHECK(count <= 4);
    if (cache != NULL) {
        memcpy(&owner, cache, sizeof(owner));
        memcpy(occurrences, cache->occurrences, count * sizeof(*occurrences));
        for (size_t i = 0; i < count; ++i)
            if (occurrences[i].leased) {
                entries[i] = store_find_chunk(occurrences[i].store, occurrences[i].state);
                CHECK(entries[i] != NULL);
                leases[i] = entries[i]->leases;
            }
    }
    const bool prior_chunk_a = f->base.world_classes->chunk_a;
    const uint64_t planes = ew_getter_planes(f);
    const uint64_t unique = atomic_load(&f->base.profile->uniquifier);
    const size_t profile_bytes = f->base.profile->allocated_bytes;
    const unsigned int holder_phase = atomic_load(&f->base.profile->holder_phase);
    mc_java_math_random_owner *math_owner = f->base.profile->math;
    const uint64_t math_state = math_owner == NULL ? 0 : atomic_load(&math_owner->state);
    const size_t constructor_consumed = f->base.profile->constructor_input_consumed;
    mc_chunk_provider_cache_observation provider_before, provider_after;
    CHECK(mc_chunk_provider_cache_observe(f->base.world->fields.v, &provider_before));
    unsigned char random[sizeof(mc_java_random)];
    CHECK(f->base.world->fields.r != NULL);
    memcpy(random, f->base.world->fields.r, sizeof(random));
    mc_world_provider *dimension = f->base.world->fields.t;
    const size_t dimension_holds = dimension == NULL ? 0 : mc_world_provider_hold_count(dimension);
    ew_getter_output values[3], initial = {0};
    if (kind == EW_GET_BRIGHTNESS || kind == EW_GET_MINIMUM)
        initial.brightness = 9.25f;
    else if (kind == EW_GET_MATERIAL)
        initial.material = mc_materials_get(f->base.materials, MC_MATERIAL_a);
    else if (kind == EW_GET_MANAGER) {
        mc_world_provider_fields fields;
        CHECK(mc_world_provider_fields_get(f->base.dimension, &fields) == MC_WORLD_PROVIDER_OK);
        initial.manager = fields.b;
    } else
        initial.predicate = true;
    static const char *const methods[] = {"c", "a", "f", "g", "h", "a"};
    static const char *const descriptors[] = {"(III)F", "(IIII)F", "(III)Lln;",
                                              "(III)Z", "(III)Z",  "()Lxv;"};
    for (size_t variant = 0; variant < 3; ++variant) {
        /* These are independent native controls, never retries of a retained
         * failed call and never a repair within a Source invocation. */
        f->base.world_classes->chunk_a = prior_chunk_a;
        if (registry != NULL)
            registry->last_error = prior_registry;
        values[variant] = initial;
        memset(&errors[variant], variant == 1 ? 0xa5 : variant == 0 ? 0x5a : 0xc3, sizeof(*errors));
        if (variant == 1)
            memcpy(sentinel, &errors[variant], sizeof(*errors));
        const mc_client_chunk_cache_status status =
            ew_getter_call(kind, variant == 1, cache, x, y, z, minimum,
                           output ? &values[variant] : NULL, &errors[variant]);
        CHECK(status == wanted &&
              f->base.world_classes->chunk_a == (prior_chunk_a || sets_chunk_a) &&
              memcmp(world_before, f->base.world, sizeof(*f->base.world)) == 0 &&
              memcmp(random, f->base.world->fields.r, sizeof(random)) == 0 &&
              unique == atomic_load(&f->base.profile->uniquifier) &&
              profile_bytes == f->base.profile->allocated_bytes &&
              holder_phase == atomic_load(&f->base.profile->holder_phase) &&
              math_owner == f->base.profile->math &&
              (math_owner == NULL || math_state == atomic_load(&math_owner->state)) &&
              constructor_consumed == f->base.profile->constructor_input_consumed &&
              mc_chunk_provider_cache_observe(f->base.world->fields.v, &provider_after) &&
              ew_getter_provider_equal(&provider_before, &provider_after) &&
              ew_getter_planes(f) == planes &&
              (dimension == NULL || mc_world_provider_hold_count(dimension) == dimension_holds));
        CHECK(cache == NULL ||
              (memcmp(cache, &owner, sizeof(owner)) == 0 &&
               memcmp(cache->occurrences, occurrences, count * sizeof(*occurrences)) == 0));
        memcpy(classes_after, f->base.world_classes, sizeof(*f->base.world_classes));
        memset(classes_after + offsetof(mc_world_runtime_classes, chunk_a), 0, sizeof(bool));
        CHECK(memcmp(classes_before, classes_after, sizeof(*f->base.world_classes)) == 0);
        for (size_t i = 0; i < count; ++i)
            CHECK(entries[i] == NULL || entries[i]->leases == leases[i]);
        if (registry != NULL) {
            memcpy(registry_after, registry, sizeof(*registry));
            memset(registry_after + offsetof(mc_registry_classes, last_error), 0,
                   sizeof(registry->last_error));
            CHECK(memcmp(registry_before, registry_after, sizeof(*registry)) == 0);
            if (variant == 0)
                returned_registry = registry->last_error;
            else
                CHECK(ew_getter_registry_equal(&returned_registry, &registry->last_error));
        }
        if (status != MC_CLIENT_CHUNK_CACHE_OK) {
            CHECK(ew_getter_output_equal(kind, &values[variant], &initial) &&
                  errors[variant].bytecode == wanted_pc);
        } else if (wanted_output != NULL && output)
            CHECK(ew_getter_output_equal(kind, &values[variant], wanted_output));
        if (variant == 1 && status == MC_CLIENT_CHUNK_CACHE_OK)
            CHECK(memcmp(sentinel, &errors[variant], sizeof(*errors)) == 0);
        else {
            CHECK(errors[variant].status == status &&
                  strcmp(errors[variant].method, methods[kind]) == 0 &&
                  strcmp(errors[variant].descriptor, descriptors[kind]) == 0);
            if (variant != 0)
                CHECK(ew_metadata_error_equal(&errors[0], &errors[variant]));
        }
        if (variant != 0)
            CHECK(ew_getter_output_equal(kind, &values[0], &values[variant]));
    }
    if (failure_receipt != NULL)
        *failure_receipt = errors[0];
    f->base.world_classes->chunk_a = prior_chunk_a;
    if (registry != NULL)
        registry->last_error = prior_registry;
    free(registry_after);
    free(registry_before);
    free(world_before);
    free(classes_after);
    free(classes_before);
    free(sentinel);
    free(errors);
    return true;
}

static bool ew_getter_failure_controls(picking_fixture *f) {
    mc_world_runtime *world = f->base.world;
    const int32_t prior_subtraction = world->fields.f;
    mc_client_chunk_cache *cache = NULL;
    mc_client_chunk_cache_error *error = malloc(sizeof(*error));
    CHECK(error != NULL &&
          mc_client_chunk_cache_create(world, 0, 0, 0, 31, 127, 31, &cache, error) ==
              MC_CLIENT_CHUNK_CACHE_OK &&
          cache != NULL && cache->state.leased == 4);
    for (size_t i = 0; i < 4; ++i) {
        memset(f->chunks[i]->data->blocks, 0, MC_CHUNK_BLOCKS);
        memset(f->chunks[i]->data->skylight, 0, MC_CHUNK_NIBBLE_BYTES);
        memset(f->chunks[i]->data->block_light, 0, MC_CHUNK_NIBBLE_BYTES);
    }
    const size_t index = (8u << 11u) | (8u << 7u) | 64u;
    mc_world_chunk *object = cache->occurrences[0].object;
    CHECK(object != NULL && object->data != NULL && object->f != NULL && object->g != NULL);
    object->data->blocks[index] = 1;
    const mc_block *stone = NULL;
    mc_world_provider_fields dimension;
    CHECK(mc_registry_block_get(f->base.registry, 1, &stone, NULL) == MC_REGISTRY_OK &&
          stone != NULL &&
          mc_world_provider_fields_get(world->fields.t, &dimension) == MC_WORLD_PROVIDER_OK &&
          dimension.f != NULL && dimension.b != NULL);
    ew_getter_output wanted[EW_GET_COUNT] = {0};
    wanted[EW_GET_BRIGHTNESS].brightness = dimension.f[0];
    wanted[EW_GET_MINIMUM].brightness = dimension.f[7];
    wanted[EW_GET_MATERIAL].material = stone->fields.bA;
    wanted[EW_GET_OPAQUE].predicate = true;
    wanted[EW_GET_NORMAL].predicate = true;
    wanted[EW_GET_MANAGER].manager = dimension.b;
    const bool prior_chunk_a = world->classes->chunk_a;
    /* Independent native input: the preceding original EW rows leave f=3.
     * Here SkyLight15 must address brightness-table level15 exactly. */
    world->fields.f = 0;
    world->classes->chunk_a = false;
    for (ew_getter_kind kind = EW_GET_BRIGHTNESS; kind < EW_GET_COUNT; ++kind) {
        CHECK(ew_getter_pair(f, cache, kind, 8, 64, 8, 7, true, MC_CLIENT_CHUNK_CACHE_OK, 0, false,
                             &wanted[kind], NULL));
        CHECK(ew_getter_pair(f, NULL, kind, 8, 64, 8, 7, true,
                             MC_CLIENT_CHUNK_CACHE_INVALID_ARGUMENT, 0, false, NULL, NULL) &&
              ew_getter_pair(f, cache, kind, 8, -1, 8, 7, false,
                             MC_CLIENT_CHUNK_CACHE_INVALID_ARGUMENT, 0, false, NULL, NULL));
        cache->state.active = true;
        CHECK(ew_getter_pair(f, cache, kind, 8, -1, 8, 7, true, MC_CLIENT_CHUNK_CACHE_BUSY, 0,
                             false, NULL, NULL));
        cache->state.active = false;
        cache->state.retiring = true;
        CHECK(ew_getter_pair(f, cache, kind, 8, 64, 8, 7, true, MC_CLIENT_CHUNK_CACHE_BUSY, 0,
                             false, NULL, NULL));
        cache->state.retiring = false;
    }
    object->data->blocks[index] = 0;
    ew_getter_output air = {.material = mc_materials_get(f->base.materials, MC_MATERIAL_a)};
    ew_getter_output no = {.predicate = false};
    CHECK(ew_getter_pair(f, cache, EW_GET_MATERIAL, 8, 64, 8, 7, true, MC_CLIENT_CHUNK_CACHE_OK, 0,
                         false, &air, NULL) &&
          ew_getter_pair(f, cache, EW_GET_OPAQUE, 8, 64, 8, 7, true, MC_CLIENT_CHUNK_CACHE_OK, 0,
                         false, &no, NULL) &&
          ew_getter_pair(f, cache, EW_GET_NORMAL, 8, 64, 8, 7, true, MC_CLIENT_CHUNK_CACHE_OK, 0,
                         false, &no, NULL));
    object->data->blocks[index] = 1;
    /* Both false->true and already-true lm.a cases; no output or class effect
     * is repaired until after each observed independent call has completed. */
    CHECK(mc_chunk_nibbles_set(object->data->skylight, MC_CHUNK_NIBBLE_BYTES, 8, 64, 8, 15));
    ew_getter_output full = {.brightness = dimension.f[15]};
    for (unsigned int already = 0; already < 2; ++already) {
        world->classes->chunk_a = already != 0;
        CHECK(ew_getter_pair(f, cache, EW_GET_BRIGHTNESS, 8, 64, 8, 7, true,
                             MC_CLIENT_CHUNK_CACHE_OK, 0, true, &full, NULL) &&
              ew_getter_pair(f, cache, EW_GET_MINIMUM, 8, 64, 8, 7, true, MC_CLIENT_CHUNK_CACHE_OK,
                             0, true, &full, NULL));
    }
    world->classes->chunk_a = false;
    CHECK(mc_chunk_nibbles_set(object->data->skylight, MC_CHUNK_NIBBLE_BYTES, 8, 64, 8, 0) &&
          mc_chunk_nibbles_set(object->data->block_light, MC_CHUNK_NIBBLE_BYTES, 8, 64, 8, 15));
    CHECK(ew_getter_pair(f, cache, EW_GET_BRIGHTNESS, 8, 64, 8, 7, true, MC_CLIENT_CHUNK_CACHE_OK,
                         0, false, &full, NULL));
    CHECK(mc_chunk_nibbles_set(object->data->block_light, MC_CHUNK_NIBBLE_BYTES, 8, 64, 8, 0));
    /* The actual source float array is a declared operand. Preserve raw
     * negative-zero and quiet-NaN bits, including public success records. */
    float *table = (float *)(void *)dimension.f;
    const uint32_t table_bits = ew_getter_float_bits(table[0]);
    static const uint32_t patterns[] = {UINT32_C(0x80000000), UINT32_C(0x7fc12345)};
    for (size_t i = 0; i < sizeof(patterns) / sizeof(*patterns); ++i) {
        memcpy(&table[0], &patterns[i], sizeof(table[0]));
        ew_getter_output bits;
        memcpy(&bits.brightness, &patterns[i], sizeof(bits.brightness));
        CHECK(ew_getter_pair(f, cache, EW_GET_BRIGHTNESS, 8, 64, 8, 0, true,
                             MC_CLIENT_CHUNK_CACHE_OK, 0, false, &bits, NULL) &&
              ew_getter_pair(f, cache, EW_GET_MINIMUM, 8, 64, 8, 0, true, MC_CLIENT_CHUNK_CACHE_OK,
                             0, false, &bits, NULL));
    }
    memcpy(&table[0], &table_bits, sizeof(table[0]));
    CHECK(ew_getter_pair(f, cache, EW_GET_MINIMUM, 8, 64, 8, 16, true,
                         MC_CLIENT_CHUNK_CACHE_SOURCE_INDEX, 32, false, NULL, NULL));

    mc_world_runtime *supplying = cache->state.d;
    cache->state.d = NULL;
    CHECK(ew_getter_pair(f, cache, EW_GET_BRIGHTNESS, -32000001, 64, 8, 7, true,
                         MC_CLIENT_CHUNK_CACHE_SOURCE_NULL, 4, false, NULL, NULL) &&
          ew_getter_pair(f, cache, EW_GET_MINIMUM, -32000001, 64, 8, 7, true,
                         MC_CLIENT_CHUNK_CACHE_SOURCE_NULL, 24, false, NULL, NULL) &&
          ew_getter_pair(f, cache, EW_GET_MANAGER, 8, 64, 8, 7, true,
                         MC_CLIENT_CHUNK_CACHE_SOURCE_NULL, 4, false, NULL, NULL));
    for (ew_getter_kind kind = EW_GET_MATERIAL; kind <= EW_GET_NORMAL; ++kind)
        CHECK(ew_getter_pair(f, cache, kind, 8, 64, 8, 7, true, MC_CLIENT_CHUNK_CACHE_CHILD_FAILED,
                             kind == EW_GET_MATERIAL ? 18u : 0u, false, NULL, NULL));
    cache->state.d = supplying;
    mc_world_provider *real_dimension = world->fields.t;
    world->fields.t = NULL;
    CHECK(ew_getter_pair(f, cache, EW_GET_BRIGHTNESS, 8, 64, 8, 7, true,
                         MC_CLIENT_CHUNK_CACHE_SOURCE_NULL, 7, false, NULL, NULL) &&
          ew_getter_pair(f, cache, EW_GET_MINIMUM, 8, 64, 8, 7, true,
                         MC_CLIENT_CHUNK_CACHE_SOURCE_NULL, 27, false, NULL, NULL) &&
          ew_getter_pair(f, cache, EW_GET_MANAGER, 8, 64, 8, 7, true,
                         MC_CLIENT_CHUNK_CACHE_SOURCE_NULL, 4, false, NULL, NULL));
    uint8_t *dimension_order_blocks = object->data->blocks;
    object->data->blocks = NULL;
    CHECK(ew_getter_pair(f, cache, EW_GET_BRIGHTNESS, 8, 64, 8, 7, true,
                         MC_CLIENT_CHUNK_CACHE_SOURCE_NULL, 7, false, NULL, error) &&
          error->nested_count == 0 &&
          ew_getter_pair(f, cache, EW_GET_MINIMUM, 8, 64, 8, 7, true,
                         MC_CLIENT_CHUNK_CACHE_SOURCE_NULL, 4, false, NULL, error) &&
          error->nested_count == 3);
    object->data->blocks = dimension_order_blocks;
    world->fields.t = real_dimension;
    mc_world_provider *raw_dimension = NULL;
    CHECK(mc_world_provider_allocate(MC_WORLD_PROVIDER_OVERWORLD, NULL, &raw_dimension, NULL) ==
              MC_WORLD_PROVIDER_OK &&
          raw_dimension != NULL);
    world->fields.t = raw_dimension;
    ew_getter_output null_manager = {.manager = NULL};
    CHECK(ew_getter_pair(f, cache, EW_GET_BRIGHTNESS, 8, 64, 8, 7, true,
                         MC_CLIENT_CHUNK_CACHE_SOURCE_NULL, 17, false, NULL, NULL) &&
          ew_getter_pair(f, cache, EW_GET_MINIMUM, 8, 64, 8, 7, true,
                         MC_CLIENT_CHUNK_CACHE_SOURCE_NULL, 32, false, NULL, NULL) &&
          ew_getter_pair(f, cache, EW_GET_MANAGER, 8, 64, 8, 7, true, MC_CLIENT_CHUNK_CACHE_OK, 0,
                         false, &null_manager, NULL));
    world->fields.t = real_dimension;
    CHECK(mc_world_provider_hold_count(raw_dimension) == 1);
    mc_world_provider_release(raw_dimension);

    mc_chunk_state *const *const *rows = cache->state.c;
    cache->state.c = NULL;
    for (ew_getter_kind kind = EW_GET_BRIGHTNESS; kind < EW_GET_MANAGER; ++kind) {
        const unsigned int pc = kind == EW_GET_BRIGHTNESS ? 14u
                                : kind == EW_GET_MINIMUM  ? 4u
                                : kind == EW_GET_MATERIAL ? 4u
                                                          : 7u;
        CHECK(ew_getter_pair(f, cache, kind, 8, 64, 8, 7, true, MC_CLIENT_CHUNK_CACHE_SOURCE_NULL,
                             pc, false, NULL, NULL));
    }
    cache->state.c = rows;
    for (ew_getter_kind kind = EW_GET_BRIGHTNESS; kind <= EW_GET_MINIMUM; ++kind)
        CHECK(ew_getter_pair(f, cache, kind, 32, 64, 8, 7, true, MC_CLIENT_CHUNK_CACHE_SOURCE_INDEX,
                             kind == EW_GET_BRIGHTNESS ? 14u : 4u, false, NULL, error) &&
              error->nested_count == 2 && error->nested[1].bytecode == 254);
    uint8_t *blocks = object->data->blocks;
    const size_t capacity = object->data->blocks_capacity;
    for (unsigned int malformed = 0; malformed < 2; ++malformed) {
        if (malformed == 0)
            object->data->blocks = NULL;
        else
            object->data->blocks_capacity = index;
        for (ew_getter_kind kind = EW_GET_BRIGHTNESS; kind < EW_GET_MANAGER; ++kind) {
            const unsigned int pc = kind == EW_GET_BRIGHTNESS ? 14u
                                    : kind == EW_GET_MINIMUM  ? 4u
                                    : kind == EW_GET_MATERIAL ? 4u
                                                              : 7u;
            CHECK(ew_getter_pair(f, cache, kind, 8, 64, 8, 7, true,
                                 malformed == 0 ? MC_CLIENT_CHUNK_CACHE_SOURCE_NULL
                                                : MC_CLIENT_CHUNK_CACHE_SOURCE_INDEX,
                                 pc, false, NULL, NULL));
        }
        object->data->blocks = blocks;
        object->data->blocks_capacity = capacity;
    }
    mc_world_chunk_nibble *sky = object->f;
    uint8_t *backing = sky->a;
    const size_t length = sky->length;
    for (unsigned int malformed = 0; malformed < 3; ++malformed) {
        if (malformed == 0)
            object->f = NULL;
        else if (malformed == 1)
            sky->a = NULL;
        else
            sky->length = index / 2u;
        for (ew_getter_kind kind = EW_GET_BRIGHTNESS; kind <= EW_GET_MINIMUM; ++kind)
            CHECK(ew_getter_pair(f, cache, kind, 8, 64, 8, 7, true,
                                 malformed == 2 ? MC_CLIENT_CHUNK_CACHE_SOURCE_INDEX
                                                : MC_CLIENT_CHUNK_CACHE_SOURCE_NULL,
                                 kind == EW_GET_BRIGHTNESS ? 14u : 4u, false, NULL, error));
        CHECK(error->nested_count == 2 && error->nested[0].bytecode == 5 &&
              error->nested[1].bytecode == 274 && error->child_bytecode == 7);
        object->f = sky;
        sky->a = backing;
        sky->length = length;
    }
    mc_world_chunk_nibble *block_light = object->g;
    object->g = NULL;
    CHECK(mc_chunk_nibbles_set(object->data->skylight, MC_CHUNK_NIBBLE_BYTES, 8, 64, 8, 15));
    CHECK(ew_getter_pair(f, cache, EW_GET_BRIGHTNESS, 8, 64, 8, 7, true,
                         MC_CLIENT_CHUNK_CACHE_SOURCE_NULL, 14, true, NULL, error) &&
          error->child_bytecode == 35);
    object->g = block_light;
    CHECK(mc_chunk_nibbles_set(object->data->skylight, MC_CHUNK_NIBBLE_BYTES, 8, 64, 8, 0));

    /* Match the actual canonical light-neighbor identity, not a guessed ID.
     * The upward sample succeeds, then X+ crosses into the corrupted Chunk. */
    const mc_block *neighbor_shape = NULL;
    CHECK(mc_registry_block_static_get(f->base.registry, "al", &neighbor_shape, NULL) ==
              MC_REGISTRY_OK &&
          neighbor_shape != NULL);
    const size_t border_index = (15u << 11u) | (8u << 7u) | 64u;
    const uint8_t border = object->data->blocks[border_index];
    object->data->blocks[border_index] = (uint8_t)neighbor_shape->fields.bn;
    mc_world_chunk *east = cache->occurrences[2].object;
    CHECK(east != NULL);
    mc_world_chunk_nibble *east_sky = east->f;
    east->f = NULL;
    for (ew_getter_kind kind = EW_GET_BRIGHTNESS; kind <= EW_GET_MINIMUM; ++kind) {
        CHECK(ew_getter_pair(f, cache, kind, 15, 64, 8, 7, true, MC_CLIENT_CHUNK_CACHE_SOURCE_NULL,
                             kind == EW_GET_BRIGHTNESS ? 14u : 4u, false, NULL, error) &&
              error->nested_count == 3 && error->nested[0].bytecode == 5 &&
              error->nested[1].bytecode == 107 && error->nested[2].bytecode == 274 &&
              error->child_bytecode == 7);
    }
    east->f = east_sky;
    object->data->blocks[border_index] = border;

    mc_registry_classes *real_registry = world->classes->registry;
    world->classes->registry = NULL;
    for (ew_getter_kind kind = EW_GET_BRIGHTNESS; kind < EW_GET_MANAGER; ++kind)
        CHECK(ew_getter_pair(f, cache, kind, 8, 64, 8, 7, true, MC_CLIENT_CHUNK_CACHE_CHILD_FAILED,
                             kind == EW_GET_BRIGHTNESS ? 14u
                             : kind == EW_GET_MINIMUM  ? 4u
                             : kind == EW_GET_MATERIAL ? 18u
                                                       : 0u,
                             false, NULL, NULL));
    world->classes->registry = real_registry;
    CHECK(real_registry->entered == 0);
    real_registry->entered = 1;
    for (ew_getter_kind kind = EW_GET_BRIGHTNESS; kind < EW_GET_MANAGER; ++kind)
        CHECK(ew_getter_pair(f, cache, kind, 8, 64, 8, 7, true, MC_CLIENT_CHUNK_CACHE_CHILD_FAILED,
                             kind == EW_GET_BRIGHTNESS ? 14u
                             : kind == EW_GET_MINIMUM  ? 4u
                             : kind == EW_GET_MATERIAL ? 18u
                                                       : 0u,
                             false, NULL, NULL));
    real_registry->entered = 0;
    mc_block *mutable_stone = real_registry->blocks.m[1];
    CHECK(mutable_stone == stone);
    mc_material *stone_material = mutable_stone->fields.bA;
    mutable_stone->fields.bA = NULL;
    ew_getter_output null_material = {.material = NULL};
    CHECK(ew_getter_pair(f, cache, EW_GET_MATERIAL, 8, 64, 8, 7, true, MC_CLIENT_CHUNK_CACHE_OK, 0,
                         false, &null_material, NULL) &&
          ew_getter_pair(f, cache, EW_GET_NORMAL, 8, 64, 8, 7, true,
                         MC_CLIENT_CHUNK_CACHE_SOURCE_NULL, 25, false, NULL, NULL));
    mutable_stone->fields.bA = stone_material;
    real_registry->blocks.m[1] = NULL;
    CHECK(ew_getter_pair(f, cache, EW_GET_MATERIAL, 8, 64, 8, 7, true,
                         MC_CLIENT_CHUNK_CACHE_SOURCE_NULL, 24, false, NULL, NULL) &&
          ew_getter_pair(f, cache, EW_GET_OPAQUE, 8, 64, 8, 7, true, MC_CLIENT_CHUNK_CACHE_OK, 0,
                         false, &no, NULL) &&
          ew_getter_pair(f, cache, EW_GET_NORMAL, 8, 64, 8, 7, true, MC_CLIENT_CHUNK_CACHE_OK, 0,
                         false, &no, NULL));
    real_registry->blocks.m[1] = mutable_stone;
    mc_block **array = real_registry->blocks.m;
    real_registry->blocks.m = NULL;
    CHECK(ew_getter_pair(f, cache, EW_GET_MATERIAL, 8, 64, 8, 7, true,
                         MC_CLIENT_CHUNK_CACHE_SOURCE_NULL, 18, false, NULL, NULL));
    for (ew_getter_kind kind = EW_GET_OPAQUE; kind <= EW_GET_NORMAL; ++kind) {
        CHECK(ew_getter_pair(f, cache, kind, 8, 64, 8, 7, true, MC_CLIENT_CHUNK_CACHE_SOURCE_NULL,
                             10, false, NULL, error) &&
              error->registry.status == MC_REGISTRY_NULL_REFERENCE);
        object->data->blocks = NULL;
        CHECK(ew_getter_pair(f, cache, kind, 8, 64, 8, 7, true, MC_CLIENT_CHUNK_CACHE_SOURCE_NULL,
                             7, false, NULL, error) &&
              error->registry.status == MC_REGISTRY_OK);
        object->data->blocks = blocks;
    }
    real_registry->blocks.m = array;
    mc_registry_classes *failed_registry = NULL;
    mc_registry_options options = real_registry->options;
    options.max_allocation_bytes = sizeof(*failed_registry);
    CHECK(mc_registry_classes_allocate(&options, &failed_registry, NULL) == MC_REGISTRY_OK &&
          failed_registry != NULL &&
          mc_registry_classes_initialize_blocks(failed_registry, NULL) != MC_REGISTRY_OK &&
          failed_registry->blocks_phase == MC_REGISTRY_FAILED);
    world->classes->registry = failed_registry;
    for (ew_getter_kind kind = EW_GET_BRIGHTNESS; kind < EW_GET_MANAGER; ++kind)
        CHECK(ew_getter_pair(f, cache, kind, 8, 64, 8, 7, true, MC_CLIENT_CHUNK_CACHE_CHILD_FAILED,
                             kind == EW_GET_BRIGHTNESS ? 14u
                             : kind == EW_GET_MINIMUM  ? 4u
                             : kind == EW_GET_MATERIAL ? 18u
                                                       : 0u,
                             false, NULL, error) &&
              error->registry.status == MC_REGISTRY_FAILED_INITIALIZATION &&
              error->registry.initializer_pc >= 0);
    world->classes->registry = real_registry;
    CHECK(mc_registry_classes_free(failed_registry, NULL) == MC_REGISTRY_OK);

    world->classes->chunk_a = prior_chunk_a;
    world->fields.f = prior_subtraction;
    CHECK(mc_client_chunk_cache_retire(&cache, error) == MC_CLIENT_CHUNK_CACHE_OK && cache == NULL);
    free(error);
    return true;
}
