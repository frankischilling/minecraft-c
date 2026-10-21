#include "mc/resources/translation/translation.h"
#include "mc/world/dimension/biome_runtime.h"
#include "mc/world/runtime/runtime.h"

#include "../../../src/core/java/random_profile_internal.h"

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

static const uint8_t english[] = "stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                                 "stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n";
/* Original-derived seed0/x0/z0/2x2 first cell, retained by the existing
 * independently qualified numerical and reference-sampler fixtures. */
static const uint64_t first_temperature = UINT64_C(0x3fed65e1be63da72);
static const uint64_t first_rainfall = UINT64_C(0x3fe085574d0ece45);

typedef struct stream {
    const uint8_t *bytes;
    size_t length, offset, opens, reads, closes;
    bool closed;
} stream;

typedef struct resources {
    mc_registry_resources source;
    stream streams[3];
    mc_registry_classes *registry;
    mc_java_random_profile *profile;
    mc_world_runtime *world;
    mc_world_dimension *dimension;
    mc_world_dimension_biome *manager;
    mc_biome_reference_array **outer_out;
    mc_biome_reference_array *outer_sentinel;
    bool missing, observe, invalid, rebound;
    size_t callbacks;
    double channels[3][4];
} resources;

typedef struct fixture {
    mc_materials *materials;
    mc_java_random_profile *profile, *other_profile;
    mc_world_regions *regions;
    mc_world_store *store;
    mc_world_info_name *name;
    mc_registry_classes *registries[6];
    mc_world_runtime_classes *classes[3];
    mc_world_runtime *worlds[3];
    mc_world_dimension *dimensions[4];
    mc_world_dimension_biome *held[2];
    resources sources[6];
    const mc_biome_reference_array *failed_array;
    bool constructor_retirement_refused;
} fixture;

static uint64_t bits(double value) {
    uint64_t out;
    memcpy(&out, &value, sizeof(out));
    return out;
}

static uint64_t uniquifier(const mc_java_random_profile *profile) {
    return atomic_load_explicit(&profile->uniquifier, memory_order_relaxed);
}

static uint64_t advance(uint64_t value, unsigned int count) {
    while (count-- != 0)
        value *= UINT64_C(1181783497276652981);
    return value;
}

static bool same_cause(const mc_registry_error *a, const mc_registry_error *b) {
    return a->message == b->message && a->initializer_pc == b->initializer_pc &&
           a->block_id == b->block_id && a->constructor_class == b->constructor_class &&
           a->constructor_pc == b->constructor_pc && a->profile_status == b->profile_status &&
           a->active_initializer_class == b->active_initializer_class &&
           a->active_initializer_pc == b->active_initializer_pc &&
           a->item_argument == b->item_argument &&
           a->format_error.status == b->format_error.status &&
           a->format_error.offset == b->format_error.offset &&
           a->format_error.length == b->format_error.length &&
           a->format_error.value == b->format_error.value &&
           a->format_error.flags == b->format_error.flags &&
           a->format_error.conversion == b->format_error.conversion &&
           a->format_error.date_time == b->format_error.date_time;
}

static bool registry_cold(mc_registry_classes *classes) {
    mc_registry_block_view block;
    mc_biome_registry_view biome;
    return mc_registry_classes_inspect_blocks(classes, &block, NULL) == MC_REGISTRY_OK &&
           mc_biome_registry_inspect(classes, &biome, NULL) == MC_REGISTRY_OK &&
           block.phase == MC_REGISTRY_COLD && block.m == NULL && biome.phase == MC_REGISTRY_COLD &&
           biome.a == NULL && biome.x == NULL;
}

static mc_java_properties_status read_bytes(void *context, uint8_t *out, size_t capacity,
                                            size_t *count) {
    stream *s = context;
    ++s->reads;
    if (out == NULL || count == NULL || capacity == 0 || s->closed || s->offset > s->length)
        return MC_JAVA_PROPERTIES_IO_ERROR;
    size_t take = s->length - s->offset;
    if (take > 17)
        take = 17;
    if (take > capacity)
        take = capacity;
    if (take != 0)
        memcpy(out, s->bytes + s->offset, take);
    s->offset += take;
    *count = take;
    return MC_JAVA_PROPERTIES_OK;
}

static void observe_callback(resources *r) {
    mc_world_dimension_biome_fields fields = {0};
    mc_world_biome_game_fields game;
    mc_world_dimension_error bind_error;
    mc_world_biome_error sample_error;
    mc_biome_reference_array *out = r->outer_sentinel;
    const mc_world_dimension_registry_binding binding = {r->registry, MC_REGISTRY_CLIENT,
                                                         r->profile};
    if (r->world == NULL || r->manager == NULL ||
        mc_world_dimension_biome_fields_get(r->manager, &fields) != MC_WORLD_DIMENSION_OK ||
        fields.fixed || fields.normal == NULL || fields.holds < 2 ||
        mc_world_dimension_biome_game_fields_get(r->manager, &game) != MC_WORLD_DIMENSION_OK ||
        game.registry != r->registry || game.d != r->outer_sentinel || r->outer_out == NULL ||
        *r->outer_out != r->outer_sentinel ||
        mc_registry_classes_initialize_biomes(r->registry, NULL) != MC_REGISTRY_BUSY ||
        mc_registry_classes_free(r->registry, NULL) != MC_REGISTRY_BUSY)
        r->invalid = true;
    if (r->invalid)
        return;
    const mc_biome_double_buffer *channels[] = {fields.a, fields.b, fields.c};
    for (size_t c = 0; c < COUNT(channels); ++c) {
        if (channels[c] == NULL || channels[c]->data == NULL || channels[c]->length != 4) {
            r->invalid = true;
            return;
        }
        memcpy(r->channels[c], channels[c]->data, sizeof(r->channels[c]));
    }
    if (bits(r->channels[0][0]) != first_temperature || bits(r->channels[1][0]) != first_rainfall)
        r->invalid = true;
    if (mc_world_dimension_bind_game(r->dimension, r->world, &binding, &bind_error) !=
            MC_WORLD_DIMENSION_REGISTRY_FAILED ||
        bind_error.registry.status != MC_REGISTRY_BUSY ||
        mc_world_dimension_biome_sample(r->manager, 0, 0, 1, 1, NULL, &out, &sample_error) !=
            MC_WORLD_DIMENSION_REGISTRY_FAILED ||
        sample_error.sampling.registry.status != MC_REGISTRY_BUSY || sample_error.bytecode != 0 ||
        out != r->outer_sentinel)
        r->invalid = true;
    if (!r->rebound) {
        mc_world_dimension_fields before, after;
        if (mc_world_dimension_fields_get(r->dimension, &before) != MC_WORLD_DIMENSION_OK ||
            before.b != r->manager || before.a != r->world ||
            mc_world_dimension_bind(r->dimension, r->world, NULL) != MC_WORLD_DIMENSION_OK ||
            mc_world_dimension_fields_get(r->dimension, &after) != MC_WORLD_DIMENSION_OK ||
            after.a != r->world || after.b == before.b ||
            mc_world_dimension_biome_game_fields_get(after.b, &game) != MC_WORLD_DIMENSION_OK ||
            game.registry != NULL || game.d != NULL)
            r->invalid = true;
        r->rebound = true;
    }
}

static mc_registry_status open_resource(void *context, const char *name,
                                        mc_java_properties_input *out) {
    resources *r = context;
    const char *const names[] = {"/lang/en_US.lang", "/lang/stats_US.lang", "/achievement/map.txt"};
    size_t index = COUNT(names);
    for (size_t i = 0; i < COUNT(names); ++i)
        if (name != NULL && strcmp(name, names[i]) == 0)
            index = i;
    if (index == COUNT(names) || out == NULL) {
        r->invalid = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    if (++r->streams[index].opens != 1)
        r->invalid = true;
    if (r->observe)
        observe_callback(r);
    ++r->callbacks;
    if (r->missing && index == 0)
        return MC_REGISTRY_NULL_REFERENCE;
    *out = (mc_java_properties_input){&r->streams[index], read_bytes};
    return MC_REGISTRY_OK;
}

static mc_registry_status close_resource(void *context, const mc_java_properties_input *input) {
    resources *r = context;
    if (input == NULL || input->context != &r->streams[2] || input->read != read_bytes) {
        r->invalid = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    ++r->streams[2].closes;
    r->streams[2].closed = true;
    return MC_REGISTRY_OK;
}

static bool ready_resource(void *context, const mc_java_properties_input *input) {
    resources *r = context;
    if (input == NULL || input->read != read_bytes)
        r->invalid = true;
    return false;
}

static mc_registry_status create_registry(fixture *f, size_t i, mc_registry_side side,
                                          mc_java_random_profile *profile, bool missing) {
    resources *r = &f->sources[i];
    *r = (resources){.profile = profile, .missing = missing};
    r->streams[0] = (stream){.bytes = english, .length = sizeof(english) - 1};
    r->streams[1] = (stream){.bytes = english};
    r->streams[2] = (stream){.bytes = english};
    r->source = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {side, profile, f->materials, 0, &r->source};
    mc_registry_status status = mc_registry_classes_allocate(&options, &f->registries[i], NULL);
    if (status == MC_REGISTRY_OK)
        r->registry = f->registries[i];
    return status;
}

static bool create_worlds(fixture *f) {
    bool ok = false;
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 2;
    CHECK(create_registry(f, 0, MC_REGISTRY_CLIENT, f->profile, false) == MC_REGISTRY_OK);
    CHECK(create_registry(f, 1, MC_REGISTRY_CLIENT, f->profile, true) == MC_REGISTRY_OK);
    CHECK(create_registry(f, 3, MC_REGISTRY_SERVER, f->profile, false) == MC_REGISTRY_OK);
    CHECK(create_registry(f, 4, MC_REGISTRY_CLIENT, f->other_profile, false) == MC_REGISTRY_OK);
    CHECK(create_registry(f, 5, MC_REGISTRY_CLIENT, f->profile, false) == MC_REGISTRY_OK);
    for (size_t i = 0; i < COUNT(f->worlds); ++i) {
        CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig,
                                              &f->classes[i], NULL) == MC_WORLD_STORAGE_OK);
        if (i < 2)
            CHECK(mc_world_runtime_classes_bind_registry(f->classes[i], f->registries[i], NULL) ==
                  MC_REGISTRY_OK);
        CHECK(mc_world_dimension_create(0, NULL, &f->dimensions[i], NULL) == MC_WORLD_DIMENSION_OK);
        const mc_world_runtime_world_arguments arguments = {
            .store = f->store, .name = f->name, .seed = 0, .dimension = f->dimensions[i]};
        uint64_t before = uniquifier(f->profile);
        CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA,
                                              f->classes[i], &limits, &f->worlds[i],
                                              NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_construct_world(f->worlds[i], &arguments, NULL) ==
              MC_WORLD_STORAGE_OK);
        mc_world_runtime_construction construction;
        mc_world_dimension_fields fields;
        mc_world_biome_game_fields game;
        CHECK(mc_world_runtime_construction_get(f->worlds[i], &construction) ==
              MC_WORLD_STORAGE_OK);
        CHECK(construction.phase == MC_WORLD_RUNTIME_READY);
        CHECK(mc_world_dimension_fields_get(f->dimensions[i], &fields) == MC_WORLD_DIMENSION_OK);
        CHECK(fields.a == f->worlds[i] && fields.b != NULL);
        CHECK(mc_world_dimension_biome_game_fields_get(fields.b, &game) == MC_WORLD_DIMENSION_OK);
        CHECK(game.registry == (i < 2 ? f->registries[i] : NULL) && game.d == NULL &&
              game.fixed_e == NULL);
        CHECK(uniquifier(f->profile) == advance(before, 3));
        if (i < 2) {
            CHECK(mc_world_dimension_biome_retain(fields.b) == MC_WORLD_DIMENSION_OK);
            f->held[i] = fields.b;
            f->sources[i].world = f->worlds[i];
            f->sources[i].dimension = f->dimensions[i];
            f->sources[i].manager = fields.b;
            CHECK(registry_cold(f->registries[i]) && f->sources[i].callbacks == 0);
        }
    }
    ok = true;
cleanup:
    return ok;
}

static bool fields_same(const mc_world_dimension_fields *a, const mc_world_dimension_fields *b) {
    return a->a == b->a && a->b == b->b && a->c == b->c && a->d == b->d && a->e == b->e &&
           a->f == b->f && a->g == b->g && a->h == b->h;
}

static bool test_bindings(fixture *f) {
    bool ok = false;
    mc_world_dimension_fields before, after;
    mc_world_dimension_state state_before, state_after;
    mc_world_dimension_error error;
    CHECK(mc_world_dimension_create(0, NULL, &f->dimensions[3], NULL) == MC_WORLD_DIMENSION_OK);
    CHECK(mc_world_dimension_fields_get(f->dimensions[3], &before) == MC_WORLD_DIMENSION_OK);
    CHECK(mc_world_dimension_state_get(f->dimensions[3], &state_before) == MC_WORLD_DIMENSION_OK);
    uint64_t start = uniquifier(f->profile), other_start = uniquifier(f->other_profile);
    const mc_world_dimension_registry_binding valid = {f->registries[0], MC_REGISTRY_CLIENT,
                                                       f->profile};
    const mc_world_dimension_registry_binding bad[] = {
        {NULL, MC_REGISTRY_CLIENT, f->profile},
        {f->registries[0], MC_REGISTRY_SERVER, f->profile},
        {f->registries[0], MC_REGISTRY_CLIENT, f->other_profile},
        {f->registries[3], MC_REGISTRY_SERVER, f->profile},
        {f->registries[4], MC_REGISTRY_CLIENT, f->other_profile},
        {f->registries[5], MC_REGISTRY_CLIENT, f->profile}};
    CHECK(mc_world_dimension_bind_game(NULL, f->worlds[0], &valid, &error) ==
          MC_WORLD_DIMENSION_INVALID_ARGUMENT);
    CHECK(mc_world_dimension_bind_game(f->dimensions[3], f->worlds[0], NULL, &error) ==
          MC_WORLD_DIMENSION_INVALID_ARGUMENT);
    for (size_t i = 0; i < COUNT(bad); ++i) {
        CHECK(mc_world_dimension_bind_game(f->dimensions[3], f->worlds[0], &bad[i], &error) ==
              MC_WORLD_DIMENSION_INVALID_ARGUMENT);
        CHECK(mc_world_dimension_fields_get(f->dimensions[3], &after) == MC_WORLD_DIMENSION_OK &&
              fields_same(&before, &after));
        CHECK(mc_world_dimension_state_get(f->dimensions[3], &state_after) ==
              MC_WORLD_DIMENSION_OK);
        CHECK(state_after.holds == state_before.holds &&
              state_after.direct_allocation_bytes == state_before.direct_allocation_bytes);
        CHECK(state_after.bind_pc == state_before.bind_pc &&
              state_after.setup_pc == state_before.setup_pc);
    }
    CHECK(mc_world_dimension_bind_game(f->dimensions[3], f->worlds[2], &valid, &error) ==
          MC_WORLD_DIMENSION_INVALID_ARGUMENT);
    CHECK(mc_world_dimension_fields_get(f->dimensions[3], &after) == MC_WORLD_DIMENSION_OK &&
          fields_same(&before, &after));
    for (size_t i = 3; i < COUNT(f->registries); ++i) {
        CHECK(registry_cold(f->registries[i]) && f->sources[i].callbacks == 0);
        CHECK(mc_registry_classes_free(f->registries[i], NULL) == MC_REGISTRY_OK);
        f->registries[i] = NULL; /* Rejected candidates retained no hidden borrow. */
    }
    CHECK(mc_world_dimension_bind_game(f->dimensions[3], f->worlds[0], &valid, &error) ==
          MC_WORLD_DIMENSION_OK);
    CHECK(mc_world_dimension_fields_get(f->dimensions[3], &after) == MC_WORLD_DIMENSION_OK);
    CHECK(after.a == f->worlds[0] && after.b != NULL && after.b != f->held[0]);
    mc_world_biome_game_fields game;
    mc_world_dimension_biome_fields fields;
    CHECK(mc_world_dimension_biome_game_fields_get(after.b, &game) == MC_WORLD_DIMENSION_OK);
    CHECK(game.registry == f->registries[0] && game.d == NULL && game.fixed_e == NULL);
    CHECK(mc_world_dimension_biome_fields_get(after.b, &fields) == MC_WORLD_DIMENSION_OK);
    CHECK(!fields.fixed && fields.normal != NULL && fields.normal->initialized == 1);
    CHECK(fields.a->data == NULL && fields.b->data == NULL && fields.c->data == NULL);
    CHECK(registry_cold(f->registries[0]) && f->sources[0].callbacks == 0);
    CHECK(uniquifier(f->profile) == start && uniquifier(f->other_profile) == other_start);
    ok = true;
cleanup:
    return ok;
}

static bool result_matches(mc_world_dimension_biome *manager, const mc_biome_reference_array *array,
                           size_t cells) {
    mc_world_dimension_biome_fields fields;
    mc_world_biome_game_fields game;
    if (mc_world_dimension_biome_fields_get(manager, &fields) != MC_WORLD_DIMENSION_OK ||
        mc_world_dimension_biome_game_fields_get(manager, &game) != MC_WORLD_DIMENSION_OK ||
        mc_biome_reference_array_length(array) < cells || fields.a->length < cells ||
        fields.b->length < cells)
        return false;
    const mc_biome *const *data = mc_biome_reference_array_data(array);
    for (size_t i = 0; i < cells; ++i) {
        const mc_biome *expected = NULL;
        if (mc_biome_registry_lookup(game.registry, fields.a->data[i], fields.b->data[i], &expected,
                                     NULL) != MC_REGISTRY_OK ||
            data[i] != expected || expected == NULL)
            return false;
    }
    return true; /* Actual lookup consistency, not a new climate golden oracle. */
}

static bool test_success(fixture *f) {
    bool ok = false;
    resources *r = &f->sources[0];
    mc_world_dimension_biome *manager = f->held[0];
    mc_world_dimension_biome_fields fields;
    mc_world_biome_game_fields game;
    mc_world_dimension_fields dimension;
    mc_world_biome_error error;
    mc_biome_reference_array *cached = NULL, *direct = NULL, *out = NULL;
    CHECK(mc_world_dimension_biome_fields_get(manager, &fields) == MC_WORLD_DIMENSION_OK);
    size_t holds_before = fields.holds;
    uint64_t start = uniquifier(f->profile);
    r->observe = true;
    r->outer_out = &cached;
    r->outer_sentinel = NULL;
    CHECK(mc_world_dimension_biome_cached(manager, 0, 0, 2, 2, &cached, &error) ==
          MC_WORLD_DIMENSION_OK);
    r->observe = false;
    r->outer_out = NULL;
    CHECK(!r->invalid && r->callbacks == 3 && r->rebound);
    CHECK(cached != NULL && mc_biome_reference_array_length(cached) == 4 &&
          result_matches(manager, cached, 4));
    CHECK(mc_world_dimension_biome_game_fields_get(manager, &game) == MC_WORLD_DIMENSION_OK &&
          game.d == cached);
    CHECK(mc_world_dimension_fields_get(f->dimensions[0], &dimension) == MC_WORLD_DIMENSION_OK);
    CHECK(dimension.b != manager && mc_world_dimension_world(f->dimensions[0]) == f->worlds[0]);
    CHECK(mc_world_dimension_biome_fields_get(manager, &fields) == MC_WORLD_DIMENSION_OK &&
          fields.holds == holds_before - 1);
    CHECK(uniquifier(f->profile) == advance(start, 5) && f->profile->math == NULL);

    /* The new Dimension manager is numerical-only. The completed cached d
     * belongs to the separately held old receiver, not the replacement. */
    mc_world_biome_game_fields replacement;
    CHECK(mc_world_dimension_biome_game_fields_get(dimension.b, &replacement) ==
          MC_WORLD_DIMENSION_OK);
    CHECK(replacement.registry == NULL && replacement.d == NULL);
    out = cached;
    CHECK(mc_world_dimension_biome_sample(dimension.b, 0, 0, 1, 1, NULL, &out, &error) ==
          MC_WORLD_DIMENSION_REGISTRY_FAILED);
    CHECK(error.sampling.registry.status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY &&
          error.bytecode == 0 && out == cached);
    CHECK(mc_world_dimension_biome_sample(manager, 17, -9, 2, 2, NULL, &direct, &error) ==
          MC_WORLD_DIMENSION_OK);
    CHECK(direct != NULL && direct != cached && result_matches(manager, direct, 4));
    CHECK(mc_world_dimension_biome_game_fields_get(manager, &game) == MC_WORLD_DIMENSION_OK &&
          game.d == cached);
    CHECK(mc_world_dimension_biome_cached(manager, 16, 16, 3, 3, &cached, &error) ==
          MC_WORLD_DIMENSION_OK);
    CHECK(mc_biome_reference_array_length(cached) == 9 && result_matches(manager, cached, 9));
    const mc_biome *suffix[9];
    for (size_t i = 0; i < COUNT(suffix); ++i)
        suffix[i] = mc_biome_reference_array_data(cached)[i];
    out = NULL;
    CHECK(mc_world_dimension_biome_sample(manager, -16, 16, 1, 1, cached, &out, &error) ==
          MC_WORLD_DIMENSION_OK);
    CHECK(out == cached && mc_biome_reference_array_length(out) == 9 &&
          result_matches(manager, out, 1));
    for (size_t i = 1; i < COUNT(suffix); ++i)
        CHECK(mc_biome_reference_array_data(cached)[i] == suffix[i]);
    CHECK(mc_world_dimension_biome_game_fields_get(manager, &game) == MC_WORLD_DIMENSION_OK &&
          game.d == cached);
    const mc_biome *point = NULL, *chunk = NULL;
    CHECK(mc_world_dimension_biome_point(manager, 7, -13, &point, &error) == MC_WORLD_DIMENSION_OK);
    CHECK(point == mc_biome_reference_array_data(cached)[0]);
    const int32_t coordinates[][4] = {{INT32_MAX, INT32_MIN, -16, 0},
                                      {INT32_MIN, INT32_MAX, 0, -16},
                                      {-1, 1, -16, 16},
                                      {1, -1, 16, -16}};
    for (size_t i = 0; i < COUNT(coordinates); ++i) {
        CHECK(mc_world_dimension_biome_chunk(manager, coordinates[i][0], coordinates[i][1], &chunk,
                                             &error) == MC_WORLD_DIMENSION_OK);
        CHECK(mc_world_dimension_biome_fields_get(manager, &fields) == MC_WORLD_DIMENSION_OK);
        uint64_t temperature = bits(fields.a->data[0]), rainfall = bits(fields.b->data[0]);
        CHECK(mc_world_dimension_biome_point(manager, coordinates[i][2], coordinates[i][3], &point,
                                             &error) == MC_WORLD_DIMENSION_OK);
        CHECK(point == chunk && bits(fields.a->data[0]) == temperature &&
              bits(fields.b->data[0]) == rainfall);
    }
    out = cached;
    CHECK(mc_world_dimension_biome_sample(manager, 0, 0, 2, 1, cached, &out, &error) ==
          MC_WORLD_DIMENSION_INVALID_ARGUMENT);
    CHECK(error.bytecode == 0 && out == cached);
    CHECK(mc_world_dimension_biome_game_fields_get(manager, &game) == MC_WORLD_DIMENSION_OK &&
          game.d == cached);
    CHECK(mc_world_dimension_biome_sample(NULL, 0, 0, 1, 1, NULL, &out, NULL) ==
              MC_WORLD_DIMENSION_INVALID_ARGUMENT &&
          out == cached);
    CHECK(mc_world_dimension_biome_cached(manager, 0, 0, 1, 1, NULL, NULL) ==
          MC_WORLD_DIMENSION_INVALID_ARGUMENT);
    CHECK(mc_world_dimension_biome_point(manager, 0, 0, NULL, NULL) ==
          MC_WORLD_DIMENSION_INVALID_ARGUMENT);
    CHECK(mc_world_dimension_biome_chunk(manager, 0, 0, NULL, NULL) ==
          MC_WORLD_DIMENSION_INVALID_ARGUMENT);
    CHECK(mc_world_dimension_biome_game_fields_get(NULL, &game) ==
          MC_WORLD_DIMENSION_INVALID_ARGUMENT);
    CHECK(mc_world_dimension_biome_game_fields_get(manager, NULL) ==
          MC_WORLD_DIMENSION_INVALID_ARGUMENT);
    CHECK(uniquifier(f->profile) == advance(start, 5) && r->callbacks == 3 && !r->invalid);
    ok = true;
cleanup:
    r->observe = false;
    r->outer_out = NULL;
    return ok;
}

static bool test_failure(fixture *f) {
    bool ok = false;
    resources *r = &f->sources[1];
    mc_world_dimension_biome *manager = f->held[1];
    mc_world_biome_game_fields game;
    mc_world_dimension_biome_fields fields;
    mc_world_biome_error first, retry;
    mc_biome_reference_array *empty = NULL, *out = NULL;
    CHECK(mc_world_dimension_biome_cached(manager, 0, 0, 0, 0, &empty, NULL) ==
          MC_WORLD_DIMENSION_OK);
    CHECK(empty != NULL && mc_biome_reference_array_length(empty) == 0 &&
          mc_biome_reference_array_data(empty) != NULL);
    CHECK(registry_cold(f->registries[1]) && r->callbacks == 0);
    uint64_t start = uniquifier(f->profile);
    out = empty;
    r->observe = true;
    r->outer_out = &out;
    r->outer_sentinel = empty;
    CHECK(mc_world_dimension_biome_cached(manager, 0, 0, 2, 2, &out, &first) ==
          MC_WORLD_DIMENSION_REGISTRY_FAILED);
    r->observe = false;
    r->outer_out = NULL;
    CHECK(!r->invalid && r->callbacks == 1 && r->rebound && out == empty);
    CHECK(first.bytecode == 11 && first.sampling.sampler_pc == 314);
    CHECK(first.sampling.cells_completed == 0 && first.sampling.next_index == 1);
    CHECK(first.sampling.registry.status == MC_REGISTRY_NULL_REFERENCE);
    CHECK(first.sampling.registry.active_initializer_class != NULL &&
          strcmp(first.sampling.registry.active_initializer_class, "nh") == 0);
    CHECK(first.sampling.registry.active_initializer_pc == 4 &&
          first.sampling.registry.constructor_pc == 26);
    CHECK(first.sampling.array != NULL && first.sampling.array != empty);
    CHECK(mc_biome_reference_array_length(first.sampling.array) == 4);
    for (size_t i = 0; i < 4; ++i)
        CHECK(mc_biome_reference_array_data(first.sampling.array)[i] == NULL);
    f->failed_array = first.sampling.array; /* Diagnostic view, never ordinary input. */
    CHECK(mc_world_dimension_biome_game_fields_get(manager, &game) == MC_WORLD_DIMENSION_OK &&
          game.d == empty);
    CHECK(mc_world_dimension_biome_fields_get(manager, &fields) == MC_WORLD_DIMENSION_OK);
    const mc_biome_double_buffer *channels[] = {fields.a, fields.b, fields.c};
    for (size_t c = 0; c < COUNT(channels); ++c) {
        CHECK(channels[c]->length == 4);
        for (size_t i = 0; i < 4; ++i)
            CHECK(bits(channels[c]->data[i]) == bits(r->channels[c][i]));
    }
    CHECK(uniquifier(f->profile) == advance(start, 5));
    CHECK(mc_world_dimension_biome_sample(manager, 1, 2, 2, 2, empty, &out, &retry) ==
          MC_WORLD_DIMENSION_REGISTRY_FAILED);
    CHECK(out == empty && retry.sampling.registry.status == MC_REGISTRY_FAILED_INITIALIZATION);
    CHECK(retry.sampling.cells_completed == 0 && retry.sampling.next_index == 1 &&
          retry.sampling.sampler_pc == 314);
    CHECK(same_cause(&first.sampling.registry, &retry.sampling.registry));
    CHECK(retry.sampling.array != NULL && retry.sampling.array != first.sampling.array);
    CHECK(mc_world_dimension_biome_game_fields_get(manager, &game) == MC_WORLD_DIMENSION_OK &&
          game.d == empty);
    mc_world_biome_game_fields good;
    CHECK(mc_world_dimension_biome_game_fields_get(f->held[0], &good) == MC_WORLD_DIMENSION_OK);
    const mc_biome *sentinel = mc_biome_reference_array_data(good.d)[0];
    const mc_biome *saved_sentinel = sentinel;
    CHECK(sentinel != NULL);
    CHECK(mc_world_dimension_biome_point(manager, 7, 9, &sentinel, &retry) ==
          MC_WORLD_DIMENSION_REGISTRY_FAILED);
    CHECK(sentinel == saved_sentinel && retry.bytecode == 5 &&
          same_cause(&first.sampling.registry, &retry.sampling.registry));
    CHECK(mc_world_dimension_biome_chunk(manager, -1, 1, &sentinel, &retry) ==
          MC_WORLD_DIMENSION_REGISTRY_FAILED);
    CHECK(sentinel == saved_sentinel && retry.bytecode == 13 &&
          same_cause(&first.sampling.registry, &retry.sampling.registry));
    CHECK(r->callbacks == 1 && r->streams[0].reads == 0 &&
          uniquifier(f->profile) == advance(start, 5));
    out = NULL;
    CHECK(mc_world_dimension_biome_cached(manager, 0, 0, 0, 0, &out, NULL) ==
              MC_WORLD_DIMENSION_OK &&
          out == empty);
    CHECK(mc_biome_reference_array_data(f->failed_array) != NULL &&
          mc_biome_reference_array_length(f->failed_array) == 4);
    ok = true;
cleanup:
    r->observe = false;
    r->outer_out = NULL;
    return ok;
}

static bool test_world_constructor_causes(fixture *f) {
    bool ok = false;
    mc_world_runtime_classes *classes = NULL;
    mc_world_runtime *worlds[3] = {NULL};
    mc_world_dimension *dimensions[3] = {NULL};
    mc_registry_error first = {0};
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    const mc_world_runtime_entry entries[] = {MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA,
                                              MC_WORLD_RUNTIME_CLIENT_COPY,
                                              MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE};
    const unsigned int call_pcs[] = {300, 315, 398};
    limits.max_provider_chunks = 2;
    CHECK(create_registry(f, 2, MC_REGISTRY_CLIENT, f->profile, true) == MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &classes,
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(classes, f->registries[2], NULL) ==
          MC_REGISTRY_OK);
    CHECK(registry_cold(f->registries[2]) && f->sources[2].callbacks == 0);
    for (size_t i = 0; i < COUNT(worlds); ++i) {
        mc_world_dimension_fields before, after;
        mc_world_dimension_state dimension_state;
        mc_world_biome_game_fields game;
        mc_world_runtime_fields fields;
        mc_world_runtime_construction state;
        mc_world_storage_error error;
        CHECK(mc_world_dimension_create(-1, NULL, &dimensions[i], NULL) == MC_WORLD_DIMENSION_OK);
        /* This preceding numerical manager is real. Failed GAME setup must
         * preserve it while publishing the newly constructing World at a2. */
        CHECK(mc_world_dimension_bind(dimensions[i], NULL, NULL) == MC_WORLD_DIMENSION_OK);
        CHECK(mc_world_dimension_fields_get(dimensions[i], &before) == MC_WORLD_DIMENSION_OK);
        CHECK(before.a == NULL && before.b != NULL && before.g == -1);
        CHECK(mc_world_dimension_biome_game_fields_get(before.b, &game) == MC_WORLD_DIMENSION_OK &&
              game.registry == NULL && game.d == NULL);
        const mc_world_runtime_world_arguments arguments = {.store = f->store,
                                                            .name = f->name,
                                                            .seed = 0,
                                                            .dimension = dimensions[i],
                                                            .parent = i == 1 ? f->worlds[0] : NULL};
        uint64_t start = uniquifier(f->profile);
        CHECK(mc_world_runtime_allocate_world(entries[i], classes, &limits, &worlds[i], NULL) ==
              MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_registry(worlds[i]) == f->registries[2]);
        CHECK(mc_world_runtime_construct_world(worlds[i], &arguments, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE);
        CHECK(error.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.system_error == 0);
        CHECK(mc_world_runtime_construction_get(worlds[i], &state) == MC_WORLD_STORAGE_OK);
        CHECK(state.phase == MC_WORLD_RUNTIME_FAILED_CONSTRUCTION && state.entry == entries[i] &&
              state.failed_call_pc == call_pcs[i] && !state.contexts_bound &&
              !state.cache_registered);
        CHECK(mc_world_runtime_fields_get(worlds[i], &fields) == MC_WORLD_STORAGE_OK);
        CHECK(fields.w == f->store && fields.x != NULL && fields.z != NULL &&
              fields.t == dimensions[i] && fields.v == NULL);
        CHECK(mc_world_dimension_fields_get(dimensions[i], &after) == MC_WORLD_DIMENSION_OK);
        CHECK(after.a == worlds[i] && after.b == before.b && after.c == before.c &&
              after.d == before.d && after.e == before.e && after.f == before.f &&
              after.g == before.g && after.h == before.h);
        CHECK(mc_world_dimension_state_get(dimensions[i], &dimension_state) ==
                  MC_WORLD_DIMENSION_OK &&
              dimension_state.setup_pc == 5);
        const mc_registry_error *cause = mc_world_runtime_last_registry_error(worlds[i]);
        CHECK(cause != NULL && cause->status == (i == 0 ? MC_REGISTRY_NULL_REFERENCE
                                                        : MC_REGISTRY_FAILED_INITIALIZATION));
        CHECK(cause->active_initializer_class != NULL &&
              strcmp(cause->active_initializer_class, "nh") == 0 &&
              cause->active_initializer_pc == 4 && cause->constructor_pc == 26);
        if (i == 0)
            first = *cause;
        else
            CHECK(same_cause(&first, cause));
        CHECK(f->sources[2].callbacks == 1 && f->sources[2].streams[0].opens == 1 &&
              f->sources[2].streams[0].reads == 0 && f->sources[2].streams[1].opens == 0 &&
              f->sources[2].streams[2].opens == 0 && !f->sources[2].invalid);
        /* Two real World-prefix defaults, plus five actual shared registry
         * defaults on the cold first attempt. The provider cave is unreached. */
        CHECK(uniquifier(f->profile) == advance(start, i == 0 ? 7u : 2u) &&
              f->profile->math == NULL);
        start = uniquifier(f->profile);
        const mc_registry_error saved = *cause;
        CHECK(mc_world_runtime_construct_world(worlds[i], &arguments, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE);
        cause = mc_world_runtime_last_registry_error(worlds[i]);
        CHECK(cause != NULL && cause->status == saved.status && same_cause(&saved, cause));
        CHECK(uniquifier(f->profile) == start && f->sources[2].callbacks == 1);
    }
    ok = true;
cleanup:
    for (size_t i = 0; i < COUNT(dimensions); ++i)
        mc_world_dimension_unbind(dimensions[i]);
    for (size_t i = 0; i < COUNT(worlds); ++i) {
        if (worlds[i] == NULL)
            continue;
        if (mc_world_runtime_retire_world(worlds[i], NULL) != MC_WORLD_STORAGE_OK ||
            mc_world_runtime_free(worlds[i], NULL) != MC_WORLD_STORAGE_OK) {
            f->constructor_retirement_refused = true;
            return false; /* Keep every dependency alive beneath this borrower. */
        }
        worlds[i] = NULL;
    }
    for (size_t i = 0; i < COUNT(dimensions); ++i)
        mc_world_dimension_release(dimensions[i]);
    if (mc_world_runtime_classes_free(classes, NULL) != MC_WORLD_STORAGE_OK) {
        f->constructor_retirement_refused = true;
        return false;
    }
    return ok;
}

static bool retire_worlds(fixture *f) {
    for (size_t i = 0; i < COUNT(f->dimensions); ++i)
        mc_world_dimension_unbind(f->dimensions[i]);
    for (size_t i = 0; i < COUNT(f->worlds); ++i) {
        if (f->worlds[i] == NULL)
            continue;
        if (mc_world_runtime_retire_world(f->worlds[i], NULL) != MC_WORLD_STORAGE_OK ||
            mc_world_runtime_free(f->worlds[i], NULL) != MC_WORLD_STORAGE_OK)
            return false;
        f->worlds[i] = NULL;
        f->sources[i].world = NULL;
    }
    for (size_t i = 0; i < COUNT(f->dimensions); ++i) {
        mc_world_dimension_release(f->dimensions[i]);
        f->dimensions[i] = NULL;
    }
    for (size_t i = 0; i < COUNT(f->classes); ++i) {
        if (mc_world_runtime_classes_free(f->classes[i], NULL) != MC_WORLD_STORAGE_OK)
            return false;
        f->classes[i] = NULL;
    }
    return true;
}

static bool test_lifetime(fixture *f) {
    bool ok = false;
    CHECK(retire_worlds(f));
    mc_world_dimension_biome_fields fields;
    for (size_t i = 0; i < COUNT(f->held); ++i) {
        CHECK(mc_world_dimension_biome_fields_get(f->held[i], &fields) == MC_WORLD_DIMENSION_OK);
        CHECK(fields.holds == 1 && fields.normal != NULL && fields.normal->initialized == 1);
        CHECK(mc_registry_classes_free(f->registries[i], NULL) == MC_REGISTRY_BUSY);
    }
    const mc_biome *point = NULL;
    CHECK(mc_world_dimension_biome_point(f->held[0], 4, -8, &point, NULL) ==
              MC_WORLD_DIMENSION_OK &&
          point != NULL);
    mc_world_biome_error failure;
    CHECK(mc_world_dimension_biome_point(f->held[1], 4, -8, &point, &failure) ==
          MC_WORLD_DIMENSION_REGISTRY_FAILED);
    CHECK(point != NULL && failure.sampling.registry.status == MC_REGISTRY_FAILED_INITIALIZATION);
    CHECK(f->sources[0].callbacks == 3 && f->sources[1].callbacks == 1);
    CHECK(mc_biome_reference_array_length(f->failed_array) == 4);
    for (size_t i = 0; i < COUNT(f->held); ++i) {
        mc_world_dimension_biome_release(f->held[i]);
        f->held[i] = NULL;
        f->sources[i].manager = NULL;
        CHECK(mc_registry_classes_free(f->registries[i], NULL) == MC_REGISTRY_OK);
        f->registries[i] = NULL;
    }
    f->failed_array = NULL;
    ok = true;
cleanup:
    return ok;
}

static bool cleanup_fixture(fixture *f) {
    if (f == NULL)
        return true;
    if (f->constructor_retirement_refused)
        return false;
    if (!retire_worlds(f))
        return false; /* Never free dependencies beneath a surviving borrower. */
    for (size_t i = 0; i < COUNT(f->held); ++i) {
        mc_world_dimension_biome_release(f->held[i]);
        f->held[i] = NULL;
    }
    for (size_t i = 0; i < COUNT(f->registries); ++i) {
        if (mc_registry_classes_free(f->registries[i], NULL) != MC_REGISTRY_OK)
            return false;
        f->registries[i] = NULL;
    }
    mc_world_info_name_release(f->name);
    mc_world_store_free(f->store);
    mc_world_regions_free(f->regions);
    mc_java_random_profile_free(f->other_profile);
    mc_java_random_profile_free(f->profile);
    mc_materials_destroy(f->materials);
    free(f);
    return true;
}

static bool decode_path(const char *hex, char *out, size_t capacity) {
    size_t n = strlen(hex);
    if (n == 0 || n % 2u != 0 || n / 2u >= capacity)
        return false;
    for (size_t i = 0; i < n / 2u; ++i) {
        unsigned int value = 0;
        for (size_t j = 0; j < 2; ++j) {
            unsigned char c = (unsigned char)hex[2 * i + j];
            unsigned int digit = c >= '0' && c <= '9'   ? (unsigned int)(c - '0')
                                 : c >= 'a' && c <= 'f' ? (unsigned int)(c - 'a') + 10u
                                                        : 16u;
            if (digit > 15u)
                return false;
            value = value * 16u + digit;
        }
        if (value == 0)
            return false;
        out[i] = (char)(unsigned char)value;
    }
    out[n / 2u] = '\0';
    return true;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    if (argc != 3 || strcmp(argv[1], "world-biome-runtime") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    if (f == NULL)
        return 1;
    uint8_t map[256];
    for (size_t i = 0; i < COUNT(map); ++i)
        map[i] = (uint8_t)i;
    const mc_world_store_options options = {.session_token = 124,
                                            .block_map = map,
                                            .block_map_length = sizeof(map),
                                            .limits = mc_world_store_default_limits()};
    const uint16_t title[] = {'B', 'i', 'o', 'm', 'e'};
    const size_t profile_budget =
        sizeof(mc_java_random_profile) + sizeof(mc_java_math_random_owner);
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_open(root, f->regions, &options, &f->store, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(profile_budget, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_java_random_profile_create(profile_budget, &f->other_profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    CHECK(create_worlds(f));
    CHECK(test_bindings(f));
    CHECK(test_success(f));
    CHECK(test_failure(f));
    CHECK(test_world_constructor_causes(f));
    CHECK(test_lifetime(f));
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("World manager fixture retirement refused; dependencies retained\n", stderr);
        return 1;
    }
    if (result == 0 && printf("PASS: %u World manager checks (world-biome-runtime)\n", checks) < 0)
        result = 1;
    return result;
}
