#include "mc/world/dimension/temperature.h"
#include "mc/world/runtime/runtime.h"

#include <emmintrin.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
static unsigned int checks;
static mc_math_trig trig;

#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #c);                        \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

/* These exact observed 2x3/length9 and auxiliary6 rows are copied from
 * tests/world/biome/test_biome.c:observed_climates, not computed by this test or
 * the new runtime. That fixture records both untouched originals before C was
 * tested; docs/reference/biome.md describes the retained numerical protocol.
 * Row order is x-major/z-minor. No expected raw-noise failure row is invented. */
typedef struct original_temperature {
    int64_t seed;
    int32_t x, z;
    uint64_t temperature[9], auxiliary[6];
} original_temperature;

static const original_temperature originals[] = {
    {0,
     0,
     0,
     {UINT64_C(0x3fed65e1be63da72), UINT64_C(0x3fed7669139c629f), UINT64_C(0x3fed7cfbe4f1c12c),
      UINT64_C(0x3fed6d9d071b1f24), UINT64_C(0x3fed81eaa789b8c0), UINT64_C(0x3fed86ea0872820c), 0,
      0, 0},
     {UINT64_C(0x3fb44909c6b8b90a), UINT64_C(0x3fd3bfa72f02c26a), UINT64_C(0x3fd6bec0853f6bba),
      UINT64_C(0x3fcc65668b2797db), UINT64_C(0x3fe05f200c0af9d7), UINT64_C(0x3fe0760f2ac9d165)}},
    {-1,
     -17,
     31,
     {UINT64_C(0x3fea90da40c65840), UINT64_C(0x3fea8c4b0a43a808), UINT64_C(0x3fea733224f5a816),
      UINT64_C(0x3feaa2efbb00834c), UINT64_C(0x3feaa713f9e448ab), UINT64_C(0x3fea9b248b1b2684), 0,
      0, 0},
     {UINT64_C(0x3fe82996a4ed3e40), UINT64_C(0x3fe7168ec5d02298), UINT64_C(0x3fd76bff24caf15e),
      UINT64_C(0x3feb8cd740a4869f), UINT64_C(0x3fee28eca55c1efa), UINT64_C(0x3fe8354a6db12de2)}},
    {INT64_MIN,
     INT32_MIN,
     INT32_MAX,
     {UINT64_C(0x3fed8467243c32be), UINT64_C(0x3fed67d5e26d2f3d), UINT64_C(0x3fed40a330ca6f29),
      UINT64_C(0x3fed87f74b07df30), UINT64_C(0x3fed6dd9225953b2), UINT64_C(0x3fed46aae2ea61c4), 0,
      0, 0},
     {UINT64_C(0x3ff11d9e3bc77404), UINT64_C(0x3fec85d2d1f13f9e), UINT64_C(0x3fe0ce015aa26fa3),
      UINT64_C(0x3ff2b2e70cb726df), UINT64_C(0x3ff07f39d4778ce3), UINT64_C(0x3fe501ce49d296a0)}},
    {INT64_MAX,
     INT32_MIN,
     INT32_MAX,
     {UINT64_C(0x3feec43776467e81), UINT64_C(0x3feed24b1e1a7da4), UINT64_C(0x3feeccebb55d453b),
      UINT64_C(0x3feec35b295417c7), UINT64_C(0x3feecf5a9e8ba8a2), UINT64_C(0x3feec7ae4a5d1ab9), 0,
      0, 0},
     {UINT64_C(0xbfb3eddba0fd9a78), UINT64_C(0xbfd354799d350815), UINT64_C(0xbf6bdcb1a95568c0),
      UINT64_C(0xbfdafd2f4e2f7404), UINT64_C(0xbfe274421b0b15c2), UINT64_C(0xbfca197803b031ab)}}};

typedef struct fixture {
    mc_world_regions *regions;
    mc_world_store *store;
    mc_java_random_profile *profile;
    mc_world_runtime_classes *classes;
    mc_world_runtime *world;
    mc_world_info_name *name;
    mc_world_provider *overworld, *fixed[2];
    mc_world_chunk_manager *fixed_manager[2]; /* Borrowed from genuine owners. */
    uint8_t map[256];
} fixture;

static uint64_t bits(double value) {
    uint64_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static double value_of(uint64_t value) {
    double result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static void poison_error(mc_temperature_error *error) {
    *error = (mc_temperature_error){.status = MC_BIOME_LIMIT,
                                    .operation = "previous caller diagnostic",
                                    .body = MC_TEMPERATURE_BODY_FIXED,
                                    .bytecode = 999u,
                                    .raw_bytecode = 998u,
                                    .native_boundary = true,
                                    .manager_status = MC_WORLD_PROVIDER_REGISTRY_FAILED,
                                    .reused_input = true,
                                    .temperature_filled = true,
                                    .auxiliary_published = true,
                                    .cells_completed = SIZE_MAX};
}

static bool fresh_error(const mc_temperature_error *error) {
    return error->status == MC_BIOME_OK && error->operation == NULL &&
           error->body == MC_TEMPERATURE_BODY_NONE && error->bytecode == 0 &&
           error->raw_bytecode == 0 && !error->native_boundary &&
           error->manager_status == MC_WORLD_PROVIDER_OK && !error->reused_input &&
           !error->temperature_filled && !error->auxiliary_published && error->cells_completed == 0;
}

static bool owner_same(const mc_temperature_owner_state *a, const mc_temperature_owner_state *b) {
    return a->holds == b->holds && a->arrays == b->arrays &&
           a->direct_allocation_bytes == b->direct_allocation_bytes &&
           a->buffer_allocation_bytes == b->buffer_allocation_bytes &&
           a->limits.max_direct_allocation_bytes == b->limits.max_direct_allocation_bytes &&
           a->limits.max_buffer_allocation_bytes == b->limits.max_buffer_allocation_bytes;
}

static bool failure_at(const mc_temperature_error *error, mc_biome_status status,
                       mc_temperature_body body, unsigned int pc, unsigned int raw_pc, bool reused,
                       bool filled) {
    return error->status == status && error->operation != NULL && error->body == body &&
           error->bytecode == pc && error->raw_bytecode == raw_pc && error->native_boundary &&
           error->manager_status == MC_WORLD_PROVIDER_OK && error->reused_input == reused &&
           error->temperature_filled == filled && !error->auxiliary_published &&
           error->cells_completed == 0;
}

static bool close_world(fixture *f) {
    mc_world_provider_unbind(f->overworld);
    if (f->world != NULL) {
        if (mc_world_runtime_retire_world(f->world, NULL) != MC_WORLD_STORAGE_OK ||
            mc_world_runtime_free(f->world, NULL) != MC_WORLD_STORAGE_OK)
            return false;
        f->world = NULL;
    }
    mc_world_provider_release(f->overworld);
    f->overworld = NULL;
    return true;
}

static bool open_world(fixture *f, int64_t seed) {
    bool ok = false;
    CHECK(f->world == NULL && f->overworld == NULL);
    CHECK(mc_world_provider_create(0, NULL, &f->overworld, NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 2;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, f->classes,
                                          &limits, &f->world, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments arguments = {
        .store = f->store, .name = f->name, .seed = seed, .dimension = f->overworld};
    CHECK(mc_world_runtime_construct_world(f->world, &arguments, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_runtime_construction state;
    CHECK(mc_world_runtime_construction_get(f->world, &state) == MC_WORLD_STORAGE_OK &&
          state.phase == MC_WORLD_RUNTIME_READY);
    ok = true;
cleanup:
    return ok;
}

static bool noise_backed_manager(fixture *f, mc_world_chunk_manager **out) {
    mc_world_provider_fields fields;
    mc_world_chunk_manager_fields biome;
    if (mc_world_provider_fields_get(f->overworld, &fields) != MC_WORLD_PROVIDER_OK ||
        fields.a != f->world || fields.b == NULL ||
        mc_world_chunk_manager_fields_get(fields.b, &biome) != MC_WORLD_PROVIDER_OK ||
        biome.fixed || biome.noise_backed == NULL)
        return false;
    *out = fields.b;
    return true;
}

/* Calibration observes only real allocation accounting, never sampled values
 * or private header sizes. The resulting limits are explicit native controls. */
static bool double_request(size_t length, size_t *out) {
    bool ok = false;
    mc_biome_memory memory = {0};
    mc_biome_double_buffer buffer = {0};
    CHECK(mc_biome_memory_init(&memory, MC_BIOME_DEFAULT_ALLOCATION_BYTES) == MC_BIOME_OK);
    CHECK(mc_biome_double_buffer_resize(&memory, &buffer, length) == MC_BIOME_OK);
    *out = memory.live_allocation_bytes;
    CHECK(*out >= length * sizeof(double));
    mc_biome_double_buffer_free(&buffer);
    CHECK(memory.live_allocation_bytes == 0);
    ok = true;
cleanup:
    mc_biome_double_buffer_free(&buffer);
    return ok;
}

static bool make_limited_manager(fixture *f, size_t buffer_limit, mc_world_provider **dimension,
                                 mc_world_chunk_manager **manager) {
    mc_world_provider_limits limits = mc_world_provider_default_limits();
    limits.max_biome_buffer_bytes = buffer_limit;
    if (mc_world_provider_create(0, &limits, dimension, NULL) != MC_WORLD_PROVIDER_OK ||
        mc_world_provider_bind(*dimension, f->world, NULL) != MC_WORLD_PROVIDER_OK)
        return false;
    mc_world_provider_fields fields;
    if (mc_world_provider_fields_get(*dimension, &fields) != MC_WORLD_PROVIDER_OK)
        return false;
    *manager = fields.b;
    return true;
}

static bool test_original_rectangles(fixture *f) {
    bool ok = false;
    mc_temperature_owner *owner = NULL;
    mc_temperature_array *array = NULL, *result = NULL, *fresh = NULL, *short_input = NULL;
    mc_temperature_owner_state baseline, final_state;
    mc_temperature_error error;
    size_t request6 = 0;
    CHECK(mc_temperature_owner_create(NULL, &owner, NULL) == MC_BIOME_OK);
    CHECK(mc_temperature_owner_get(owner, &baseline) == MC_BIOME_OK);
    CHECK(double_request(6, &request6));
    for (size_t row = 0; row < COUNT(originals); ++row) {
        const original_temperature *source = &originals[row];
        CHECK(open_world(f, source->seed));
        mc_world_chunk_manager *receiver = NULL;
        CHECK(noise_backed_manager(f, &receiver));
        mc_world_chunk_manager_fields fields;
        CHECK(mc_world_chunk_manager_fields_get(receiver, &fields) == MC_WORLD_PROVIDER_OK);
        CHECK(mc_biome_cached_square(fields.noise_backed, source->x, source->z, 2) == MC_BIOME_OK);
        CHECK(fields.a->length == 4 && fields.b->length == 4 && fields.d->length == 4);
        double saved_a[4], saved_b[4];
        mc_biome_id saved_d[4];
        memcpy(saved_a, fields.a->data, sizeof(saved_a));
        memcpy(saved_b, fields.b->data, sizeof(saved_b));
        memcpy(saved_d, fields.d->data, sizeof(saved_d));
        const void *a_data = fields.a->data, *b_data = fields.b->data, *d_data = fields.d->data;
        const size_t manager_bytes = fields.noise_backed->memory->live_allocation_bytes;
        const size_t manager_holds = fields.holds;
        if (row == 0) {
            /* Actual successful fresh return, both source NULL and a genuine
             * short allocated-empty input. Values still come ONLY from the
             * retained original 2x3 row, never this C sampler or raw-fill seam. */
            for (size_t mode = 0; mode < 2; ++mode) {
                if (mode != 0)
                    CHECK(mc_temperature_sample(f->fixed_manager[0], owner, NULL, 0, 0, 0, 3,
                                                &short_input, NULL) == MC_BIOME_OK);
                mc_temperature_owner_state before_new, after_new;
                mc_temperature_array_view fresh_view;
                CHECK(mc_temperature_owner_get(owner, &before_new) == MC_BIOME_OK);
                poison_error(&error);
                CHECK(mc_temperature_sample(receiver, owner, short_input, source->x, source->z, 2,
                                            3, &fresh, &error) == MC_BIOME_OK);
                CHECK(fresh != NULL && fresh != short_input && fresh_error(&error) &&
                      mc_temperature_array_get(fresh, &fresh_view) == MC_BIOME_OK &&
                      fresh_view.length == 6 && fresh_view.holds == 1 && fresh_view.owner == owner);
                for (size_t i = 0; i < 6; ++i)
                    CHECK(bits(fresh_view.data[i]) == source->temperature[i]);
                CHECK(mc_temperature_owner_get(owner, &after_new) == MC_BIOME_OK &&
                      after_new.arrays == before_new.arrays + 1 &&
                      after_new.holds == before_new.holds + 1 &&
                      after_new.direct_allocation_bytes > before_new.direct_allocation_bytes &&
                      after_new.buffer_allocation_bytes ==
                          before_new.buffer_allocation_bytes + request6);
                if (short_input != NULL) {
                    mc_temperature_array_view short_view;
                    CHECK(mc_temperature_array_get(short_input, &short_view) == MC_BIOME_OK &&
                          short_view.length == 0 && short_view.holds == 1 &&
                          short_view.data != fresh_view.data);
                }
                mc_temperature_array_release(fresh);
                fresh = NULL;
                CHECK(mc_temperature_owner_get(owner, &after_new) == MC_BIOME_OK &&
                      owner_same(&before_new, &after_new));
                mc_temperature_array_release(short_input);
                short_input = NULL;
            }
        }
        if (array == NULL)
            CHECK(mc_temperature_sample(f->fixed_manager[0], owner, NULL, 0, 0, 3, 3, &array,
                                        NULL) == MC_BIOME_OK);
        mc_temperature_array_view before, after;
        CHECK(mc_temperature_array_get(array, &before) == MC_BIOME_OK);
        for (size_t i = 0; i < before.length; ++i)
            before.data[i] = -17.25;
        poison_error(&error);
        CHECK(mc_temperature_sample(receiver, NULL, array, source->x, source->z, 2, 3, &result,
                                    &error) == MC_BIOME_OK);
        CHECK(fresh_error(&error));
        CHECK(result == array && mc_temperature_array_get(result, &after) == MC_BIOME_OK &&
              after.owner == owner && after.data == before.data && after.length == 9 &&
              after.holds == before.holds + 1);
        for (size_t i = 0; i < COUNT(source->temperature); ++i)
            CHECK(bits(after.data[i]) == source->temperature[i]);
        CHECK(mc_world_chunk_manager_fields_get(receiver, &fields) == MC_WORLD_PROVIDER_OK &&
              fields.holds == manager_holds && fields.c->length == 6);
        for (size_t i = 0; i < COUNT(source->auxiliary); ++i)
            CHECK(bits(fields.c->data[i]) == source->auxiliary[i]);
        CHECK(fields.a->data == a_data && fields.b->data == b_data && fields.d->data == d_data &&
              fields.a->length == 4 && fields.b->length == 4 && fields.d->length == 4 &&
              memcmp(saved_a, fields.a->data, sizeof(saved_a)) == 0 &&
              memcmp(saved_b, fields.b->data, sizeof(saved_b)) == 0 &&
              memcmp(saved_d, fields.d->data, sizeof(saved_d)) == 0);
        CHECK(fields.noise_backed->memory->live_allocation_bytes ==
              manager_bytes + 2 * sizeof(double));
        mc_temperature_array_release(result);
        result = NULL;
        CHECK(close_world(f));
    }
    mc_temperature_array_release(array);
    array = NULL;
    CHECK(mc_temperature_owner_get(owner, &final_state) == MC_BIOME_OK &&
          owner_same(&baseline, &final_state));
    ok = true;
cleanup:
    mc_temperature_array_release(fresh);
    mc_temperature_array_release(short_input);
    mc_temperature_array_release(result);
    mc_temperature_array_release(array);
    mc_temperature_owner_release(owner);
    return ok;
}

static bool test_empty_fixed_and_admission(fixture *f) {
    bool ok = false, result_owned = false;
    mc_temperature_owner *owner = NULL;
    mc_temperature_array *empty = NULL, *array = NULL, *result = NULL;
    mc_temperature_array_view view, before;
    mc_temperature_owner_state state, after;
    mc_temperature_error error;
    mc_biome_status status;
    CHECK(mc_temperature_owner_create(NULL, &owner, NULL) == MC_BIOME_OK);
    CHECK(mc_temperature_owner_get(owner, &state) == MC_BIOME_OK && state.holds == 1 &&
          state.arrays == 0 && state.buffer_allocation_bytes == 0);
    CHECK(mc_temperature_owner_retain(owner) == MC_BIOME_OK);
    mc_temperature_owner_release(owner);
    CHECK(mc_temperature_owner_get(owner, &after) == MC_BIOME_OK && owner_same(&state, &after));
    CHECK(mc_temperature_sample(f->fixed_manager[0], owner, NULL, INT32_MIN, INT32_MAX, 0, 3,
                                &empty, NULL) == MC_BIOME_OK);
    CHECK(mc_temperature_array_get(empty, &view) == MC_BIOME_OK && view.data != NULL &&
          view.length == 0 && view.holds == 1);
    status = mc_temperature_sample(f->fixed_manager[0], NULL, empty, 0, 0, 0, 3, &result, NULL);
    result_owned = status == MC_BIOME_OK;
    CHECK(status == MC_BIOME_OK && result == empty);
    mc_temperature_array_release(result);
    result = NULL;
    result_owned = false;
    CHECK(mc_temperature_sample(f->fixed_manager[0], owner, NULL, 0, 0, 3, 3, &array, NULL) ==
          MC_BIOME_OK);
    CHECK(mc_temperature_array_get(array, &view) == MC_BIOME_OK && view.length == 9);
    for (size_t i = 0; i < view.length; ++i)
        view.data[i] = -33.5;
    view.data[6] = value_of(UINT64_C(0x8000000000000000));
    view.data[7] = value_of(UINT64_C(0x7ff8000000000123));
    uint64_t suffix[3] = {bits(view.data[6]), bits(view.data[7]), bits(view.data[8])};
    for (size_t fixed = 0; fixed < COUNT(f->fixed_manager); ++fixed) {
        mc_world_chunk_manager_fields prior, current;
        CHECK(mc_world_chunk_manager_fields_get(f->fixed_manager[fixed], &prior) ==
              MC_WORLD_PROVIDER_OK);
        const uint64_t expected =
            fixed == 0 ? UINT64_C(0x3ff0000000000000) : UINT64_C(0x3fe0000000000000);
        CHECK(prior.fixed && prior.noise_backed == NULL && bits(prior.fixed_f) == expected);
        poison_error(&error);
        status = mc_temperature_sample(f->fixed_manager[fixed], NULL, array, INT32_MAX, INT32_MIN,
                                       2, 3, &result, &error);
        result_owned = status == MC_BIOME_OK;
        CHECK(status == MC_BIOME_OK && result == array && fresh_error(&error));
        for (size_t i = 0; i < 6; ++i)
            CHECK(bits(view.data[i]) == expected);
        for (size_t i = 0; i < COUNT(suffix); ++i)
            CHECK(bits(view.data[6 + i]) == suffix[i]);
        mc_temperature_array_release(result);
        result = NULL;
        result_owned = false;
        CHECK(mc_world_chunk_manager_fields_get(f->fixed_manager[fixed], &current) ==
                  MC_WORLD_PROVIDER_OK &&
              current.holds == prior.holds);
        CHECK(current.a == prior.a && current.b == prior.b && current.c == prior.c &&
              current.d == prior.d && current.a->data == NULL && current.b->data == NULL &&
              current.c->data == NULL && current.d->data == NULL && current.a->length == 0 &&
              current.b->length == 0 && current.c->length == 0 && current.d->length == 0);
    }
    double saved[9];
    memcpy(saved, view.data, sizeof(saved));
    status = mc_temperature_sample(f->fixed_manager[1], NULL, array, 0, 0, 9, 0, &result, NULL);
    result_owned = status == MC_BIOME_OK;
    CHECK(status == MC_BIOME_OK && result == array);
    CHECK(memcmp(saved, view.data, sizeof(saved)) == 0);
    mc_temperature_array_release(result);
    result = array; /* Borrowed sentinel, no extra owning reference. */
    result_owned = false;
    poison_error(&error);
    status = mc_temperature_sample(NULL, owner, NULL, 0, 0, 1, 1, &result, &error);
    result_owned = status == MC_BIOME_OK;
    CHECK(status == MC_BIOME_INVALID_ARGUMENT && result == array &&
          error.manager_status == MC_WORLD_PROVIDER_INVALID_ARGUMENT &&
          error.body == MC_TEMPERATURE_BODY_NONE && error.bytecode == 0 &&
          error.raw_bytecode == 0 && !error.temperature_filled && error.native_boundary);
    poison_error(&error);
    status =
        mc_temperature_sample(NULL, NULL, NULL, 0, 0, (size_t)INT32_MAX + 1u, 0, &result, &error);
    result_owned = status == MC_BIOME_OK;
    CHECK(status == MC_BIOME_LIMIT && result == array &&
          failure_at(&error, MC_BIOME_LIMIT, MC_TEMPERATURE_BODY_NONE, 0, 0, false, false));
    status = mc_temperature_sample(f->fixed_manager[0], NULL, NULL, 0, 0, 0, 0, &result, &error);
    result_owned = status == MC_BIOME_OK;
    CHECK(status == MC_BIOME_INVALID_ARGUMENT && result == array &&
          failure_at(&error, MC_BIOME_INVALID_ARGUMENT, MC_TEMPERATURE_BODY_FIXED, 19, 0, false,
                     false)); /* NULL and allocated-empty are distinct. */
    CHECK(mc_temperature_array_get(array, &before) == MC_BIOME_OK);
    view = before;
    CHECK(mc_temperature_array_get(NULL, &view) == MC_BIOME_INVALID_ARGUMENT &&
          view.data == before.data && view.owner == before.owner && view.length == before.length &&
          view.holds == before.holds);
    CHECK(mc_temperature_array_retain(NULL) == MC_BIOME_INVALID_ARGUMENT);
    CHECK(mc_temperature_owner_retain(NULL) == MC_BIOME_INVALID_ARGUMENT);
    CHECK(mc_temperature_array_get(array, NULL) == MC_BIOME_INVALID_ARGUMENT);
    CHECK(mc_temperature_sample(f->fixed_manager[0], owner, array, 0, 0, 1, 1, NULL, &error) ==
              MC_BIOME_INVALID_ARGUMENT &&
          error.bytecode == 0);
    CHECK(memcmp(saved, before.data, sizeof(saved)) == 0);
    ok = true;
cleanup:
    if (result_owned)
        mc_temperature_array_release(result);
    mc_temperature_array_release(array);
    mc_temperature_array_release(empty);
    mc_temperature_owner_release(owner);
    return ok;
}

static bool test_lifetime_and_rebinding(fixture *f) {
    bool ok = false;
    mc_temperature_owner *owner = NULL;
    mc_temperature_array *field = NULL, *alias = NULL, *result = NULL;
    mc_world_provider *dimension = NULL;
    mc_world_chunk_manager *old = NULL;
    mc_world_provider_fields fields;
    mc_world_chunk_manager_fields biome;
    mc_temperature_array_view initial, current;
    mc_temperature_owner_state state;
    CHECK(mc_temperature_owner_create(NULL, &owner, NULL) == MC_BIOME_OK);
    CHECK(mc_temperature_sample(f->fixed_manager[0], owner, NULL, 0, 0, 3, 3, &field, NULL) ==
          MC_BIOME_OK);
    CHECK(mc_temperature_array_retain(field) == MC_BIOME_OK);
    alias = field; /* SAME handle independently held, never an owning copy. */
    CHECK(mc_temperature_array_get(field, &initial) == MC_BIOME_OK && initial.holds == 2);
    CHECK(mc_temperature_owner_get(owner, &state) == MC_BIOME_OK && state.holds == 2 &&
          state.arrays == 1); /* Handle aliases do not add ledger holds. */
    CHECK(mc_world_provider_create(0, NULL, &dimension, NULL) == MC_WORLD_PROVIDER_OK);
    CHECK(mc_world_provider_bind(dimension, f->world, NULL) == MC_WORLD_PROVIDER_OK);
    CHECK(mc_world_provider_fields_get(dimension, &fields) == MC_WORLD_PROVIDER_OK);
    CHECK(mc_world_chunk_manager_retain(fields.b) == MC_WORLD_PROVIDER_OK);
    old = fields.b;
    CHECK(mc_temperature_sample(old, NULL, field, 0, 0, 2, 3, &result, NULL) == MC_BIOME_OK);
    CHECK(result == field);
    mc_temperature_array_release(result);
    result = NULL;
    CHECK(mc_world_provider_bind(dimension, f->world, NULL) == MC_WORLD_PROVIDER_OK);
    CHECK(mc_world_provider_fields_get(dimension, &fields) == MC_WORLD_PROVIDER_OK &&
          fields.b != old);
    CHECK(mc_world_chunk_manager_fields_get(old, &biome) == MC_WORLD_PROVIDER_OK &&
          biome.holds == 1 && biome.c->length == 6);
    CHECK(mc_temperature_sample(fields.b, NULL, field, 0, 0, 2, 3, &result, NULL) == MC_BIOME_OK);
    CHECK(result == field && mc_temperature_array_get(field, &current) == MC_BIOME_OK &&
          current.data == initial.data && current.owner == owner);
    for (size_t i = 0; i < current.length; ++i)
        CHECK(bits(current.data[i]) == originals[0].temperature[i]);
    mc_temperature_array_release(result);
    result = NULL;
    mc_world_chunk_manager_release(old);
    old = NULL;
    mc_world_provider_unbind(dimension);
    mc_world_provider_release(dimension);
    dimension = NULL; /* Neither old nor replacement receiver supplies w's life. */
    CHECK(mc_temperature_sample(f->fixed_manager[1], NULL, field, 0, 0, 1, 2, &result, NULL) ==
              MC_BIOME_OK &&
          result == field);
    mc_temperature_array_release(result);
    result = NULL;
    uint64_t retained[9];
    for (size_t i = 0; i < COUNT(retained); ++i)
        retained[i] = bits(current.data[i]);
    CHECK(mc_temperature_sample(f->fixed_manager[0], owner, field, 0, 0, 4, 4, &result, NULL) ==
              MC_BIOME_OK &&
          result != field);
    mc_temperature_array_release(field); /* Controller-style successful field store. */
    field = result;
    result = NULL;
    CHECK(mc_temperature_array_get(field, &current) == MC_BIOME_OK && current.length == 16 &&
          current.data != initial.data);
    CHECK(mc_temperature_owner_get(owner, &state) == MC_BIOME_OK && state.holds == 3 &&
          state.arrays == 2);
    mc_temperature_owner_release(owner);
    owner = NULL; /* Genuine arrays keep their allocation owner alive. */
    CHECK(mc_temperature_array_get(alias, &current) == MC_BIOME_OK && current.holds == 1 &&
          current.data == initial.data && current.length == 9);
    CHECK(mc_temperature_owner_get(current.owner, &state) == MC_BIOME_OK && state.holds == 2 &&
          state.arrays == 2);
    for (size_t i = 0; i < COUNT(retained); ++i)
        CHECK(bits(current.data[i]) == retained[i]);
    mc_temperature_array_release(field);
    field = NULL;
    CHECK(mc_temperature_owner_get(current.owner, &state) == MC_BIOME_OK && state.holds == 1 &&
          state.arrays == 1);
    CHECK(mc_temperature_array_get(alias, &current) == MC_BIOME_OK && current.data == initial.data);
    ok = true;
cleanup:
    mc_temperature_array_release(result);
    mc_temperature_array_release(field);
    mc_temperature_array_release(alias);
    mc_temperature_owner_release(owner);
    mc_world_chunk_manager_release(old);
    mc_world_provider_unbind(dimension);
    mc_world_provider_release(dimension);
    return ok;
}

static bool test_quota_and_raw_prefix(fixture *f) {
    bool ok = false, result_owned = false;
    mc_temperature_owner *owner = NULL, *limited = NULL;
    mc_temperature_array *array = NULL, *result = NULL, *empty = NULL;
    mc_biome_memory raw_memory = {0};
    mc_biome_double_buffer expected_raw = {0};
    mc_world_provider *dimension = NULL, *zero_dimension = NULL;
    mc_world_chunk_manager *manager = NULL, *zero_manager = NULL;
    mc_temperature_owner_state initial, before, after;
    mc_temperature_array_view view, held_view;
    mc_world_chunk_manager_fields fields;
    mc_temperature_error error;
    mc_biome_status status;
    size_t request4 = 0, request6 = 0;
    CHECK(double_request(4, &request4) && double_request(6, &request6));
    CHECK(make_limited_manager(f, request4 + request6 - 1, &dimension, &manager));
    CHECK(make_limited_manager(f, 0, &zero_dimension, &zero_manager));
    CHECK(mc_temperature_owner_create(NULL, &owner, NULL) == MC_BIOME_OK);
    CHECK(mc_temperature_owner_get(owner, &initial) == MC_BIOME_OK);
    mc_world_chunk_manager *noise_backed = NULL;
    CHECK(noise_backed_manager(f, &noise_backed));
    CHECK(mc_temperature_sample(noise_backed, owner, NULL, 0, 0, 0, 3, &empty, NULL) ==
          MC_BIOME_OK);
    CHECK(mc_temperature_array_get(empty, &held_view) == MC_BIOME_OK && held_view.data != NULL &&
          held_view.length == 0);
    CHECK(mc_world_chunk_manager_fields_get(noise_backed, &fields) == MC_WORLD_PROVIDER_OK &&
          fields.c->data != NULL && fields.c->length == 0 && fields.a->data == NULL &&
          fields.b->data == NULL && fields.d->data == NULL);
    mc_temperature_array_release(empty);
    empty = NULL;
    CHECK(mc_temperature_sample(f->fixed_manager[0], owner, NULL, 0, 0, 3, 3, &array, NULL) ==
          MC_BIOME_OK);
    CHECK(mc_temperature_array_get(array, &view) == MC_BIOME_OK);
    status = mc_temperature_sample(manager, NULL, array, 0, 0, 2, 2, &result, NULL);
    result_owned = status == MC_BIOME_OK;
    CHECK(status == MC_BIOME_OK && result == array);
    mc_temperature_array_release(result);
    result = NULL;
    result_owned = false;
    CHECK(mc_world_chunk_manager_fields_get(manager, &fields) == MC_WORLD_PROVIDER_OK);
    CHECK(fields.c->length == 4 && fields.a->data == NULL && fields.b->data == NULL &&
          fields.d->data == NULL);
    const double *old_c = fields.c->data;
    double saved_c[4];
    memcpy(saved_c, old_c, sizeof(saved_c));
    const size_t manager_bytes = fields.noise_backed->memory->live_allocation_bytes;
    const size_t holds = fields.holds;
    CHECK(manager_bytes == request4 && request6 < request4 + request6 - 1);
    /* Explicit NATIVE lower-interface sequencing contract, not an original
     * golden value or another temperature formula: obtain the reached raw
     * channel through the existing raw-fill API into disjoint owned storage.
     * Comparing this with the late-failure image catches premature processing
     * before c allocation. Successful originals[] remain the parity oracle. */
    CHECK(mc_biome_memory_init(&raw_memory, MC_BIOME_DEFAULT_ALLOCATION_BYTES) == MC_BIOME_OK);
    CHECK(mc_biome_double_buffer_resize(&raw_memory, &expected_raw, 9) == MC_BIOME_OK);
    const mc_noise_simplex_octaves raw_noise = {fields.noise_backed->temperature_layers, 4};
    const mc_noise_simplex_grid raw_grid = {0.0, 0.0, 2, 3, (double)0.025f, (double)0.025f};
    CHECK(mc_noise_simplex_octaves_fill(&raw_noise, &raw_grid, 0.25,
                                        MC_NOISE_SIMPLEX_DEFAULT_DIVISOR_MULTIPLIER,
                                        expected_raw.data, expected_raw.length));
    CHECK(mc_temperature_owner_get(owner, &before) == MC_BIOME_OK);
    for (size_t i = 0; i < view.length; ++i)
        view.data[i] = 99.0;
    result = array; /* Borrowed output sentinel; failure must preserve it. */
    poison_error(&error);
    status = mc_temperature_sample(manager, owner, array, 0, 0, 2, 3, &result, &error);
    result_owned = status == MC_BIOME_OK;
    CHECK(status == MC_BIOME_LIMIT && result == array &&
          failure_at(&error, MC_BIOME_LIMIT, MC_TEMPERATURE_BODY_NORMAL, 74, 35, true, true));
    for (size_t i = 0; i < view.length; ++i)
        CHECK(bits(view.data[i]) == bits(expected_raw.data[i]));
    CHECK(mc_temperature_array_get(array, &held_view) == MC_BIOME_OK && held_view.holds == 1 &&
          held_view.data == view.data && held_view.length == 9);
    CHECK(mc_world_chunk_manager_fields_get(manager, &fields) == MC_WORLD_PROVIDER_OK &&
          fields.holds == holds && fields.c->data == old_c && fields.c->length == 4 &&
          memcmp(saved_c, old_c, sizeof(saved_c)) == 0 &&
          fields.noise_backed->memory->live_allocation_bytes == manager_bytes &&
          fields.a->data == NULL && fields.b->data == NULL && fields.d->data == NULL);
    CHECK(mc_temperature_owner_get(owner, &after) == MC_BIOME_OK && owner_same(&before, &after));
    double unrelated_output[9];
    for (size_t i = 0; i < view.length; ++i)
        view.data[i] = -41.25;
    memcpy(unrelated_output, view.data, sizeof(unrelated_output));
    poison_error(&error);
    status = mc_temperature_sample(manager, owner, NULL, 0, 0, 2, 3, &result, &error);
    result_owned = status == MC_BIOME_OK;
    CHECK(status == MC_BIOME_LIMIT && result == array &&
          failure_at(&error, MC_BIOME_LIMIT, MC_TEMPERATURE_BODY_NORMAL, 74, 35, false, true));
    CHECK(mc_temperature_owner_get(owner, &after) == MC_BIOME_OK && owner_same(&before, &after));
    CHECK(mc_world_chunk_manager_fields_get(manager, &fields) == MC_WORLD_PROVIDER_OK &&
          fields.holds == holds);
    CHECK(mc_temperature_array_get(array, &held_view) == MC_BIOME_OK && held_view.holds == 1);
    CHECK(memcmp(unrelated_output, view.data, sizeof(unrelated_output)) == 0);
    CHECK(fields.c->data == old_c && memcmp(saved_c, old_c, sizeof(saved_c)) == 0 &&
          fields.noise_backed->memory->live_allocation_bytes == manager_bytes);
    /* A zero-cell late c allocation still follows the entire raw output clear.
     * Every zero here is independently source-derived, not sampled by C as an
     * expected numerical oracle. Allocated-empty needs its real native header. */
    for (size_t i = 0; i < view.length; ++i)
        view.data[i] = -71.0;
    status = mc_temperature_sample(zero_manager, owner, array, INT32_MAX, INT32_MIN, 0, 3, &result,
                                   &error);
    result_owned = status == MC_BIOME_OK;
    CHECK(status == MC_BIOME_LIMIT && result == array &&
          failure_at(&error, MC_BIOME_LIMIT, MC_TEMPERATURE_BODY_NORMAL, 74, 35, true, true));
    for (size_t i = 0; i < view.length; ++i)
        CHECK(bits(view.data[i]) == 0);
    CHECK(mc_world_chunk_manager_fields_get(zero_manager, &fields) == MC_WORLD_PROVIDER_OK &&
          fields.c->data == NULL && fields.a->data == NULL && fields.b->data == NULL &&
          fields.d->data == NULL && fields.noise_backed->memory->live_allocation_bytes == 0);
    /* Source19 output budgets are independent of current manager.c's budget. */
    mc_temperature_limits limits = before.limits;
    limits.max_buffer_allocation_bytes = 0;
    CHECK(mc_temperature_owner_create(&limits, &limited, NULL) == MC_BIOME_OK);
    status = mc_temperature_sample(manager, limited, NULL, 0, 0, 1, 1, &result, &error);
    result_owned = status == MC_BIOME_OK;
    CHECK(status == MC_BIOME_LIMIT && result == array &&
          failure_at(&error, MC_BIOME_LIMIT, MC_TEMPERATURE_BODY_NORMAL, 19, 0, false, false));
    CHECK(mc_temperature_owner_get(limited, &after) == MC_BIOME_OK && after.arrays == 0 &&
          after.holds == 1 && after.buffer_allocation_bytes == 0 &&
          after.direct_allocation_bytes == initial.direct_allocation_bytes);
    mc_temperature_owner_release(limited);
    limited = NULL;
    limits = before.limits;
    limits.max_direct_allocation_bytes = initial.direct_allocation_bytes;
    CHECK(mc_temperature_owner_create(&limits, &limited, NULL) == MC_BIOME_OK);
    status = mc_temperature_sample(manager, limited, NULL, 0, 0, 0, 0, &result, &error);
    result_owned = status == MC_BIOME_OK;
    CHECK(status == MC_BIOME_LIMIT && result == array &&
          failure_at(&error, MC_BIOME_LIMIT, MC_TEMPERATURE_BODY_NORMAL, 19, 0, false, false));
    CHECK(mc_temperature_owner_get(limited, &after) == MC_BIOME_OK && after.arrays == 0 &&
          after.holds == 1 && after.buffer_allocation_bytes == 0 &&
          after.direct_allocation_bytes == initial.direct_allocation_bytes);
    ok = true;
cleanup:
    if (result_owned)
        mc_temperature_array_release(result);
    mc_temperature_array_release(empty);
    mc_temperature_array_release(array);
    mc_biome_double_buffer_free(&expected_raw);
    mc_temperature_owner_release(limited);
    mc_temperature_owner_release(owner);
    mc_world_provider_unbind(dimension);
    mc_world_provider_release(dimension);
    mc_world_provider_unbind(zero_dimension);
    mc_world_provider_release(zero_dimension);
    return ok;
}

static bool test_fp_order(fixture *f) {
    bool ok = false, result_owned = false;
    const unsigned int saved_csr = _mm_getcsr();
    mc_temperature_owner *owner = NULL, *limited = NULL;
    mc_temperature_array *array = NULL, *result = NULL;
    mc_world_provider *dimension = NULL;
    mc_world_chunk_manager *manager = NULL;
    mc_world_chunk_manager_fields fields;
    mc_temperature_owner_state before, after;
    mc_temperature_array_view view;
    mc_temperature_error error;
    mc_biome_status status;
    CHECK(make_limited_manager(f, 0, &dimension, &manager));
    CHECK(mc_temperature_owner_create(NULL, &owner, NULL) == MC_BIOME_OK);
    CHECK(mc_temperature_sample(f->fixed_manager[0], owner, NULL, 0, 0, 3, 3, &array, NULL) ==
          MC_BIOME_OK);
    CHECK(mc_temperature_array_get(array, &view) == MC_BIOME_OK);
    for (size_t i = 0; i < view.length; ++i)
        view.data[i] = -13.5;
    CHECK(mc_temperature_owner_get(owner, &before) == MC_BIOME_OK);
    const unsigned int refused_csr = (saved_csr & ~0x6000u) | 0x2000u;
    result = array;
    poison_error(&error);
    _mm_setcsr(refused_csr);
    status = mc_temperature_sample(manager, owner, NULL, 0, 0, 2, 3, &result, &error);
    _mm_setcsr(saved_csr);
    result_owned = status == MC_BIOME_OK;
    CHECK(status == MC_BIOME_INVALID_ARGUMENT && result == array &&
          failure_at(&error, MC_BIOME_INVALID_ARGUMENT, MC_TEMPERATURE_BODY_NORMAL, 44, 0, false,
                     false)); /* Allocation19 succeeded; unpublished result was freed. */
    CHECK(mc_temperature_owner_get(owner, &after) == MC_BIOME_OK && owner_same(&before, &after));
    CHECK(mc_world_chunk_manager_fields_get(manager, &fields) == MC_WORLD_PROVIDER_OK &&
          fields.c->data == NULL && fields.noise_backed->memory->live_allocation_bytes == 0);
    _mm_setcsr(refused_csr);
    status = mc_temperature_sample(manager, NULL, array, 0, 0, 2, 3, &result, &error);
    _mm_setcsr(saved_csr);
    result_owned = status == MC_BIOME_OK;
    CHECK(status == MC_BIOME_INVALID_ARGUMENT && result == array &&
          failure_at(&error, MC_BIOME_INVALID_ARGUMENT, MC_TEMPERATURE_BODY_NORMAL, 44, 0, true,
                     false));
    for (size_t i = 0; i < view.length; ++i)
        CHECK(bits(view.data[i]) == bits(-13.5));
    mc_temperature_limits limits = before.limits;
    limits.max_buffer_allocation_bytes = 0;
    CHECK(mc_temperature_owner_create(&limits, &limited, NULL) == MC_BIOME_OK);
    _mm_setcsr(refused_csr);
    status = mc_temperature_sample(manager, limited, NULL, 0, 0, 2, 3, &result, &error);
    _mm_setcsr(saved_csr);
    result_owned = status == MC_BIOME_OK;
    CHECK(status == MC_BIOME_LIMIT && result == array &&
          failure_at(&error, MC_BIOME_LIMIT, MC_TEMPERATURE_BODY_NORMAL, 19, 0, false, false));
    result = NULL;
    poison_error(&error);
    _mm_setcsr(refused_csr);
    status = mc_temperature_sample(f->fixed_manager[1], NULL, array, 0, 0, 2, 3, &result, &error);
    _mm_setcsr(saved_csr);
    result_owned = status == MC_BIOME_OK;
    CHECK(status == MC_BIOME_OK && result == array && fresh_error(&error));
    for (size_t i = 0; i < 6; ++i)
        CHECK(bits(view.data[i]) == UINT64_C(0x3fe0000000000000));
    for (size_t i = 6; i < view.length; ++i)
        CHECK(bits(view.data[i]) == bits(-13.5));
    ok = true;
cleanup:
    _mm_setcsr(saved_csr);
    if (result_owned)
        mc_temperature_array_release(result);
    mc_temperature_array_release(array);
    mc_temperature_owner_release(limited);
    mc_temperature_owner_release(owner);
    mc_world_provider_unbind(dimension);
    mc_world_provider_release(dimension);
    return ok;
}

static bool decode_path(const char *hex, char *out, size_t capacity) {
    const size_t length = strlen(hex);
    if (length == 0 || length % 2u != 0 || length / 2u >= capacity)
        return false;
    for (size_t i = 0; i < length / 2u; ++i) {
        unsigned int byte = 0;
        for (size_t j = 0; j < 2; ++j) {
            const unsigned char c = (unsigned char)hex[2 * i + j];
            const unsigned int digit = c >= '0' && c <= '9'   ? (unsigned int)(c - '0')
                                       : c >= 'a' && c <= 'f' ? (unsigned int)(c - 'a') + 10u
                                                              : 16u;
            if (digit >= 16u)
                return false;
            byte = byte * 16u + digit;
        }
        if (byte == 0)
            return false;
        out[i] = (char)(unsigned char)byte;
    }
    out[length / 2u] = '\0';
    return true;
}

static bool fixture_close(fixture *f) {
    if (f == NULL)
        return true;
    if (!close_world(f))
        return false; /* Keep every supplying dependency alive if retirement refuses. */
    for (size_t i = 0; i < COUNT(f->fixed); ++i) {
        mc_world_provider_release(f->fixed[i]);
        f->fixed[i] = NULL;
    }
    if (f->classes != NULL &&
        mc_world_runtime_classes_free(f->classes, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    mc_world_info_name_release(f->name);
    mc_world_store_free(f->store);
    mc_world_regions_free(f->regions);
    mc_java_random_profile_free(f->profile);
    free(f);
    return true;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    if (argc != 3 || strcmp(argv[1], "temperature-runtime") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1u, sizeof(*f));
    if (f == NULL)
        return 1;
    for (size_t i = 0; i < COUNT(f->map); ++i)
        f->map[i] = (uint8_t)i;
    const mc_world_store_options options = {.session_token = 125,
                                            .block_map = f->map,
                                            .block_map_length = sizeof(f->map),
                                            .limits = mc_world_store_default_limits()};
    const uint16_t title[] = {'T', 'e', 'm', 'p', 'e', 'r', 'a', 't', 'u', 'r', 'e'};
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_open(root, f->regions, &options, &f->store, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes,
                                          NULL) == MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < COUNT(f->fixed); ++i) {
        CHECK(mc_world_provider_create(i == 0 ? -1 : 1, NULL, &f->fixed[i], NULL) ==
              MC_WORLD_PROVIDER_OK);
        CHECK(mc_world_provider_bind(f->fixed[i], NULL, NULL) == MC_WORLD_PROVIDER_OK);
        mc_world_provider_fields fields;
        CHECK(mc_world_provider_fields_get(f->fixed[i], &fields) == MC_WORLD_PROVIDER_OK);
        f->fixed_manager[i] = fields.b;
    }
    CHECK(test_original_rectangles(f));
    CHECK(test_empty_fixed_and_admission(f));
    CHECK(open_world(f, 0));
    CHECK(test_lifetime_and_rebinding(f));
    CHECK(test_quota_and_raw_prefix(f));
    CHECK(test_fp_order(f));
    result = 0;
cleanup:
    if (!fixture_close(f)) {
        fputs("Temperature fixture retirement refused; dependencies retained\n", stderr);
        return 1;
    }
    if (result == 0 && printf("PASS: %u temperature checks (temperature-runtime)\n", checks) < 0)
        result = 1;
    return result;
}
