#include "mc/resources/translation/translation.h"
#include "mc/world/dimension/biome_runtime.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define PROFILE_BUDGET (4u * 1024u * 1024u)

static unsigned int checks;

#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #c);                        \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

/* Synthetic resource bytes drive the real dependency chain, without supplying
 * any original resource assets or manufactured registry/Biome objects. */
static const uint8_t language[] = "stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                                  "stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n";

typedef struct byte_stream {
    const uint8_t *bytes;
    size_t length, offset, opens, reads, closes;
    bool closed;
} byte_stream;

typedef struct resource_source {
    mc_registry_resources resources;
    byte_stream streams[3];
    mc_registry_classes *registry;
    mc_world_dimension *observed_dimension;
    mc_world_dimension_biome *preceding_biome;
    size_t callbacks;
    bool missing, invalid;
} resource_source;

/* The borrowed resources/profile/materials remain heap-owned even if an
 * unexpected BUSY retirement forces this fixture to remain live. */
typedef struct fixture {
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_registry_classes *registries[3];
    resource_source sources[3];
    mc_world_dimension *dimensions[6];
    mc_world_dimension_biome *aliases[2];
} fixture;

static bool no_biomes(const mc_biome_registry_view *v) {
    return v->a == NULL && v->b == NULL && v->c == NULL && v->d == NULL && v->e == NULL &&
           v->f == NULL && v->g == NULL && v->h == NULL && v->i == NULL && v->j == NULL &&
           v->k == NULL && v->l == NULL && v->m == NULL && v->x == NULL && v->x_length == 0;
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

static mc_java_properties_status read_bytes(void *context, uint8_t *out, size_t capacity,
                                            size_t *count) {
    byte_stream *stream = context;
    ++stream->reads;
    if (out == NULL || count == NULL || capacity == 0 || stream->closed ||
        stream->offset > stream->length)
        return MC_JAVA_PROPERTIES_IO_ERROR;
    size_t take = stream->length - stream->offset;
    if (take > 17)
        take = 17;
    if (take > capacity)
        take = capacity;
    if (take != 0)
        memcpy(out, stream->bytes + stream->offset, take);
    stream->offset += take;
    *count = take;
    return MC_JAVA_PROPERTIES_OK;
}

static mc_registry_status open_resource(void *context, const char *name,
                                        mc_java_properties_input *out) {
    resource_source *source = context;
    const char *const names[] = {"/lang/en_US.lang", "/lang/stats_US.lang", "/achievement/map.txt"};
    size_t index = COUNT(names);
    for (size_t i = 0; i < COUNT(names); ++i)
        if (name != NULL && strcmp(name, names[i]) == 0)
            index = i;
    if (out == NULL || index == COUNT(names)) {
        source->invalid = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    if (++source->streams[index].opens != 1 || index != source->callbacks)
        source->invalid = true;
    ++source->callbacks;

    /* wd/nz and ay/aj load actual l/m at setup5, before the fixed constructor
     * and Dimension.b publication. NULL World is not a prerequisite failure. */
    mc_world_dimension_fields fields;
    mc_world_dimension_state state;
    mc_biome_registry_view biomes;
    if (source->observed_dimension == NULL ||
        mc_world_dimension_fields_get(source->observed_dimension, &fields) !=
            MC_WORLD_DIMENSION_OK ||
        fields.a != NULL || fields.b != source->preceding_biome ||
        mc_world_dimension_state_get(source->observed_dimension, &state) != MC_WORLD_DIMENSION_OK ||
        state.phase != MC_WORLD_DIMENSION_READY || state.setup_pc != 5 ||
        mc_biome_registry_inspect(source->registry, &biomes, NULL) != MC_REGISTRY_OK ||
        biomes.phase != MC_REGISTRY_INITIALIZING || biomes.initializer_pc != 4 ||
        !no_biomes(&biomes) || mc_registry_classes_free(source->registry, NULL) != MC_REGISTRY_BUSY)
        source->invalid = true;
    if (source->missing && index == 0)
        return MC_REGISTRY_NULL_REFERENCE;
    *out = (mc_java_properties_input){&source->streams[index], read_bytes};
    return MC_REGISTRY_OK;
}

static mc_registry_status close_resource(void *context, const mc_java_properties_input *input) {
    resource_source *source = context;
    if (input == NULL || input->context != &source->streams[2] || input->read != read_bytes) {
        source->invalid = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    ++source->streams[2].closes;
    source->streams[2].closed = true;
    return MC_REGISTRY_OK;
}

static bool ready_resource(void *context, const mc_java_properties_input *input) {
    resource_source *source = context;
    if (input == NULL || input->read != read_bytes)
        source->invalid = true;
    return false;
}

static bool registry_create(fixture *f, size_t index, mc_registry_side side, bool missing) {
    resource_source *source = &f->sources[index];
    source->missing = missing;
    source->streams[0] = (byte_stream){.bytes = language, .length = sizeof(language) - 1};
    source->streams[1] = (byte_stream){.bytes = language};
    source->streams[2] = (byte_stream){.bytes = language};
    source->resources =
        (mc_registry_resources){source, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {side, f->profile, f->materials, 0, &source->resources};
    if (mc_registry_classes_allocate(&options, &f->registries[index], NULL) != MC_REGISTRY_OK)
        return false;
    source->registry = f->registries[index];
    return true;
}

static bool resources_unread(const resource_source *source) {
    for (size_t i = 0; i < COUNT(source->streams); ++i)
        if (source->streams[i].opens != 0 || source->streams[i].reads != 0 ||
            source->streams[i].closes != 0)
            return false;
    return source->callbacks == 0 && !source->invalid;
}

static bool resources_complete(const resource_source *source) {
    return !source->invalid && source->callbacks == 3 && source->streams[0].opens == 1 &&
           source->streams[1].opens == 1 && source->streams[2].opens == 1 &&
           source->streams[0].reads != 0 && source->streams[1].reads != 0 &&
           source->streams[2].reads != 0 && source->streams[0].closes == 0 &&
           source->streams[1].closes == 0 && source->streams[2].closes == 1;
}

static bool references_are(const mc_biome_reference_array *array, size_t first, size_t end,
                           const mc_biome *value) {
    const mc_biome *const *data = mc_biome_reference_array_data(array);
    if (array == NULL || data == NULL || end > mc_biome_reference_array_length(array))
        return false;
    for (size_t i = first; i < end; ++i)
        if (data[i] != value)
            return false;
    return true;
}

static bool doubles_are(const mc_biome_double_buffer *buffer, size_t first, size_t end,
                        double value) {
    if (buffer == NULL || buffer->data == NULL || end > buffer->length)
        return false;
    for (size_t i = first; i < end; ++i)
        if (buffer->data[i] != value)
            return false;
    return true;
}

static bool manager_fields(mc_world_dimension_biome *manager, mc_registry_classes *registry,
                           const mc_biome *actual, double temperature,
                           mc_world_dimension_biome_fields *fields) {
    mc_world_biome_game_fields game;
    return mc_world_dimension_biome_fields_get(manager, fields) == MC_WORLD_DIMENSION_OK &&
           mc_world_dimension_biome_game_fields_get(manager, &game) == MC_WORLD_DIMENSION_OK &&
           fields->fixed && fields->normal == NULL && fields->fixed_f == temperature &&
           fields->fixed_g == 0.0 && game.registry == registry && game.fixed_e == actual &&
           fields->a != NULL && fields->b != NULL && fields->c != NULL && fields->c->data == NULL &&
           fields->c->length == 0 && fields->d != NULL && fields->d->data == NULL &&
           fields->d->length == 0;
}

static bool measure_charge(size_t length, size_t *out) {
    bool ok = false;
    mc_biome_memory memory = {0};
    mc_biome_double_buffer buffer = {0};
    CHECK(mc_biome_memory_init(&memory, SIZE_MAX) == MC_BIOME_OK);
    CHECK(mc_biome_double_buffer_resize(&memory, &buffer, length) == MC_BIOME_OK);
    CHECK(memory.live_allocation_bytes > 0);
    *out = memory.live_allocation_bytes;
    ok = true;
cleanup:
    mc_biome_double_buffer_free(&buffer);
    return ok && memory.live_allocation_bytes == 0;
}

/* The fixed paired body returns e without allocating or sampling in point and
 * chunk. Its direct rectangle is e-fill76, b-fill93, a-fill110, return114. */
static bool test_arrays(fixture *f) {
    bool ok = false;
    mc_world_dimension_biome_fields numeric[2];
    mc_world_biome_game_fields game[2];
    mc_biome_reference_array *empty[2] = {NULL, NULL}, *large[2] = {NULL, NULL};
    mc_biome_reference_array *out = NULL, *cached = NULL;
    mc_world_biome_error error;
    const double temperature[] = {1.0, 0.5};
    for (size_t k = 0; k < 2; ++k) {
        CHECK(mc_world_dimension_biome_game_fields_get(f->aliases[k], &game[k]) ==
              MC_WORLD_DIMENSION_OK);
        CHECK(game[k].registry == f->registries[0] && game[k].fixed_e != NULL && game[k].d == NULL);
        CHECK(manager_fields(f->aliases[k], f->registries[0], game[k].fixed_e, temperature[k],
                             &numeric[k]));
        CHECK(numeric[k].a->data == NULL && numeric[k].b->data == NULL);
        const mc_biome *point = NULL;
        CHECK(mc_world_dimension_biome_point(f->aliases[k], INT32_MIN, INT32_MAX, &point, &error) ==
                  MC_WORLD_DIMENSION_OK &&
              point == game[k].fixed_e);
        CHECK(mc_world_dimension_biome_chunk(f->aliases[k], INT32_MAX, INT32_MIN, &point, &error) ==
                  MC_WORLD_DIMENSION_OK &&
              point == game[k].fixed_e);
        CHECK(mc_world_dimension_biome_fields_get(f->aliases[k], &numeric[k]) ==
                  MC_WORLD_DIMENSION_OK &&
              numeric[k].a->data == NULL && numeric[k].b->data == NULL);
        CHECK(mc_world_dimension_biome_sample(f->aliases[k], INT32_MIN, INT32_MAX, 0, 7, NULL,
                                              &empty[k], &error) == MC_WORLD_DIMENSION_OK);
        CHECK(empty[k] != NULL && mc_biome_reference_array_length(empty[k]) == 0 &&
              mc_biome_reference_array_data(empty[k]) != NULL && error.bytecode == 114 &&
              error.sampling.sampler_pc == 114 && error.sampling.cells_completed == 0 &&
              error.sampling.next_index == 0);
        CHECK(mc_world_dimension_biome_fields_get(f->aliases[k], &numeric[k]) ==
                  MC_WORLD_DIMENSION_OK &&
              numeric[k].a->data != NULL && numeric[k].a->length == 0 &&
              numeric[k].b->data != NULL && numeric[k].b->length == 0);
        CHECK(mc_world_dimension_biome_sample(f->aliases[k], -77, 91, 3, 2, empty[k], &large[k],
                                              &error) == MC_WORLD_DIMENSION_OK);
        CHECK(large[k] != empty[k] && mc_biome_reference_array_length(large[k]) == 6 &&
              references_are(large[k], 0, 6, game[k].fixed_e) &&
              error.sampling.cells_completed == 6 && error.sampling.next_index == 0);
        CHECK(manager_fields(f->aliases[k], f->registries[0], game[k].fixed_e, temperature[k],
                             &numeric[k]) &&
              numeric[k].a->length == 6 && numeric[k].b->length == 6 &&
              doubles_are(numeric[k].a, 0, 6, temperature[k]) &&
              doubles_are(numeric[k].b, 0, 6, 0.0));
    }
    CHECK(game[0].fixed_e != game[1].fixed_e && empty[0] != empty[1]);

    /* Use a real other-manager array as input, then distinguish output and
     * climate suffixes from the requested prefix without forged descriptors. */
    for (size_t i = 0; i < 6; ++i) {
        numeric[1].a->data[i] = 10.0 + (double)i;
        numeric[1].b->data[i] = 20.0 + (double)i;
    }
    double *saved_a = numeric[1].a->data, *saved_b = numeric[1].b->data;
    out = empty[1];
    CHECK(mc_world_dimension_biome_sample(f->aliases[1], 9, -11, 1, 2, large[0], &out, &error) ==
              MC_WORLD_DIMENSION_OK &&
          out == large[0] && references_are(out, 0, 2, game[1].fixed_e) &&
          references_are(out, 2, 6, game[0].fixed_e));
    CHECK(numeric[1].a->data == saved_a && numeric[1].b->data == saved_b &&
          doubles_are(numeric[1].a, 0, 2, 0.5) && doubles_are(numeric[1].b, 0, 2, 0.0));
    for (size_t i = 2; i < 6; ++i)
        CHECK(numeric[1].a->data[i] == 10.0 + (double)i &&
              numeric[1].b->data[i] == 20.0 + (double)i);
    CHECK(mc_world_dimension_biome_game_fields_get(f->aliases[1], &game[1]) ==
              MC_WORLD_DIMENSION_OK &&
          game[1].d == NULL);

    CHECK(mc_world_dimension_biome_cached(f->aliases[1], 0, 0, 2, 2, &cached, &error) ==
          MC_WORLD_DIMENSION_OK);
    CHECK(cached != NULL && mc_biome_reference_array_length(cached) == 4 &&
          references_are(cached, 0, 4, game[1].fixed_e));
    CHECK(mc_world_dimension_biome_game_fields_get(f->aliases[1], &game[1]) ==
              MC_WORLD_DIMENSION_OK &&
          game[1].d == cached);
    out = empty[0];
    CHECK(mc_world_dimension_biome_sample(f->aliases[0], 0, 0, 1, 1, cached, &out, &error) ==
              MC_WORLD_DIMENSION_OK &&
          out == cached && references_are(cached, 0, 1, game[0].fixed_e) &&
          references_are(cached, 1, 4, game[1].fixed_e));
    CHECK(mc_world_dimension_biome_game_fields_get(f->aliases[0], &game[0]) ==
              MC_WORLD_DIMENSION_OK &&
          game[0].d == NULL);
    CHECK(mc_world_dimension_biome_cached(f->aliases[1], 99, -99, 1, 1, &out, &error) ==
              MC_WORLD_DIMENSION_OK &&
          out == cached && references_are(cached, 0, 4, game[1].fixed_e));
    CHECK(mc_world_dimension_biome_cached(f->aliases[1], 0, 0, 0, 9, &out, &error) ==
              MC_WORLD_DIMENSION_OK &&
          out == cached && mc_biome_reference_array_length(out) == 4);
    CHECK(resources_complete(&f->sources[0]));
    ok = true;
cleanup:
    return ok;
}

/* Public buffer accounting supplies real platform-specific charges. No limit,
 * descriptor, class phase or owning layout is modified to force a failure. */
static bool test_budget(fixture *f, mc_registry_side side, size_t kind, bool short_b,
                        size_t charge_one, size_t charge_four) {
    bool ok = false;
    const size_t slot = 2 + kind * 2 + (short_b ? 1 : 0);
    mc_world_dimension_limits limits = mc_world_dimension_default_limits();
    limits.max_biome_buffer_bytes = short_b ? 2 * charge_one + charge_four : charge_four;
    const mc_world_dimension_registry_binding binding = {f->registries[0], side, f->profile};
    mc_world_dimension_fields dimension;
    mc_world_dimension_biome_fields numeric;
    mc_world_biome_game_fields game;
    mc_world_biome_error error;
    mc_biome_reference_array *prior = NULL, *out = NULL;
    const int32_t id = kind == 0 ? -1 : 1;
    CHECK(mc_world_dimension_create(id, &limits, &f->dimensions[slot], NULL) ==
          MC_WORLD_DIMENSION_OK);
    CHECK(mc_world_dimension_bind_game(f->dimensions[slot], NULL, &binding, NULL) ==
          MC_WORLD_DIMENSION_OK);
    CHECK(mc_world_dimension_fields_get(f->dimensions[slot], &dimension) == MC_WORLD_DIMENSION_OK);
    CHECK(mc_world_dimension_biome_game_fields_get(dimension.b, &game) == MC_WORLD_DIMENSION_OK &&
          game.fixed_e != NULL && game.d == NULL);
    if (short_b) {
        CHECK(mc_world_dimension_biome_cached(dimension.b, 0, 0, 1, 1, &prior, &error) ==
              MC_WORLD_DIMENSION_OK);
        CHECK(prior != NULL && references_are(prior, 0, 1, game.fixed_e));
    } else {
        CHECK(mc_world_dimension_biome_sample(f->aliases[kind], 0, 0, 0, 0, NULL, &prior, &error) ==
              MC_WORLD_DIMENSION_OK);
    }
    CHECK(mc_world_dimension_biome_fields_get(dimension.b, &numeric) == MC_WORLD_DIMENSION_OK &&
          numeric.a != NULL && numeric.b != NULL);
    double *old_b = numeric.b->data;
    if (short_b) {
        numeric.a->data[0] = 17.0;
        numeric.b->data[0] = 23.0;
    }
    out = prior;
    const mc_world_dimension_status allocation_status =
        short_b ? mc_world_dimension_biome_cached(dimension.b, 3, 4, 2, 2, &out, &error)
                : mc_world_dimension_biome_sample(dimension.b, 3, 4, 2, 2, prior, &out, &error);
    CHECK(allocation_status == MC_WORLD_DIMENSION_BIOME_FAILED);
    CHECK(out == prior && error.bytecode == (short_b ? 11u : 60u) &&
          error.sampling.sampler_pc == 60 && error.sampling.climate_status == MC_BIOME_LIMIT &&
          error.sampling.registry.status == MC_REGISTRY_LIMIT &&
          error.sampling.cells_completed == 0 && error.sampling.next_index == 0);
    const mc_biome_reference_array *first_local = error.sampling.array;
    CHECK(first_local != NULL && first_local != prior &&
          mc_biome_reference_array_length(first_local) == 4 &&
          references_are(first_local, 0, 4, NULL));
    CHECK(mc_world_dimension_biome_fields_get(dimension.b, &numeric) == MC_WORLD_DIMENSION_OK &&
          numeric.a->data != NULL && numeric.a->length == 4 && doubles_are(numeric.a, 0, 4, 0.0) &&
          numeric.b->data == old_b && numeric.b->length == (short_b ? 1u : 0u));
    CHECK(!short_b || numeric.b->data[0] == 23.0);
    CHECK(mc_world_dimension_biome_game_fields_get(dimension.b, &game) == MC_WORLD_DIMENSION_OK &&
          game.d == (short_b ? prior : NULL));
    double *published_a = numeric.a->data;

    /* a is already long enough, so b is not repaired. All four actual refs
     * are stored at76 before the null/short b fill fails at93. a is still
     * zero because its fill110 has not executed. Cached d is not published. */
    CHECK(mc_world_dimension_biome_cached(dimension.b, -3, -4, 2, 2, &out, &error) ==
          MC_WORLD_DIMENSION_REGISTRY_FAILED);
    CHECK(out == prior && error.bytecode == 11 && error.sampling.sampler_pc == 93 &&
          error.sampling.climate_status == MC_BIOME_OK &&
          error.sampling.registry.status ==
              (short_b ? MC_REGISTRY_INDEX_OUT_OF_RANGE : MC_REGISTRY_NULL_REFERENCE) &&
          error.sampling.cells_completed == 4 && error.sampling.next_index == 0 &&
          error.sampling.array != first_local && error.sampling.array != prior &&
          references_are(error.sampling.array, 0, 4, game.fixed_e));
    CHECK(mc_world_dimension_biome_fields_get(dimension.b, &numeric) == MC_WORLD_DIMENSION_OK &&
          numeric.a->data == published_a && doubles_are(numeric.a, 0, 4, 0.0) &&
          numeric.b->data == old_b && numeric.b->length == (short_b ? 1u : 0u));
    CHECK(!short_b || numeric.b->data[0] == 23.0);
    CHECK(mc_world_dimension_biome_game_fields_get(dimension.b, &game) == MC_WORLD_DIMENSION_OK &&
          game.d == (short_b ? prior : NULL) && references_are(first_local, 0, 4, NULL));

    out = prior;
    CHECK(mc_world_dimension_biome_sample(dimension.b, 0, 0, 2, 2, prior, &out, &error) ==
          MC_WORLD_DIMENSION_REGISTRY_FAILED);
    CHECK(out == prior && error.bytecode == 93 && error.sampling.sampler_pc == 93 &&
          error.sampling.cells_completed == 4 && error.sampling.next_index == 0 &&
          references_are(error.sampling.array, 0, 4, game.fixed_e));
    ok = true;
cleanup:
    mc_world_dimension_release(f->dimensions[slot]);
    f->dimensions[slot] = NULL;
    return ok;
}

static bool test_failed_rebind(fixture *f, mc_registry_side side) {
    bool ok = false;
    const mc_world_dimension_registry_binding binding = {f->registries[2], side, f->profile};
    mc_world_dimension_error error;
    mc_registry_error first = {0};
    for (size_t k = 0; k < 2; ++k) {
        mc_world_dimension_fields before, after;
        mc_world_dimension_state state;
        mc_biome_registry_view view;
        float light[16];
        CHECK(mc_world_dimension_fields_get(f->dimensions[k], &before) == MC_WORLD_DIMENSION_OK);
        memcpy(light, before.f, sizeof(light));
        f->sources[2].observed_dimension = f->dimensions[k];
        f->sources[2].preceding_biome = before.b;
        CHECK(mc_world_dimension_bind_game(f->dimensions[k], NULL, &binding, &error) ==
              MC_WORLD_DIMENSION_REGISTRY_FAILED);
        CHECK(error.bytecode == 5 &&
              error.registry.status ==
                  (k == 0 ? MC_REGISTRY_NULL_REFERENCE : MC_REGISTRY_FAILED_INITIALIZATION));
        CHECK(error.registry.active_initializer_class != NULL &&
              strcmp(error.registry.active_initializer_class,
                     side == MC_REGISTRY_CLIENT ? "nh" : "ii") == 0 &&
              error.registry.active_initializer_pc == 4 && error.registry.constructor_pc == 26);
        if (k == 0)
            first = error.registry;
        else
            CHECK(same_cause(&first, &error.registry));
        CHECK(mc_world_dimension_fields_get(f->dimensions[k], &after) == MC_WORLD_DIMENSION_OK &&
              after.a == NULL && after.b == before.b && after.c == before.c &&
              after.d == before.d && after.e == before.e && after.g == before.g &&
              after.f == before.f && after.h == before.h &&
              memcmp(light, after.f, sizeof(light)) == 0);
        CHECK(mc_world_dimension_state_get(f->dimensions[k], &state) == MC_WORLD_DIMENSION_OK &&
              state.phase == MC_WORLD_DIMENSION_READY && state.setup_pc == 5);
        CHECK(mc_biome_registry_inspect(f->registries[2], &view, NULL) == MC_REGISTRY_OK &&
              view.phase == MC_REGISTRY_FAILED && no_biomes(&view));
        CHECK(f->sources[2].callbacks == 1 && f->sources[2].streams[0].opens == 1 &&
              f->sources[2].streams[0].reads == 0 && f->sources[2].streams[1].opens == 0 &&
              f->sources[2].streams[2].opens == 0 && !f->sources[2].invalid);
        mc_world_biome_game_fields game;
        const mc_biome *out = NULL;
        CHECK(mc_world_dimension_biome_game_fields_get(after.b, &game) == MC_WORLD_DIMENSION_OK &&
              game.registry == f->registries[0]);
        CHECK(mc_world_dimension_biome_point(after.b, 0, 0, &out, NULL) == MC_WORLD_DIMENSION_OK &&
              out == game.fixed_e);
    }
    /* Unpublished failed replacements must release their registry borrows. */
    CHECK(mc_registry_classes_free(f->registries[2], NULL) == MC_REGISTRY_OK);
    f->registries[2] = NULL;
    ok = true;
cleanup:
    return ok;
}

static bool cleanup_fixture(fixture *f) {
    if (f == NULL)
        return true;
    for (size_t i = 0; i < COUNT(f->aliases); ++i) {
        mc_world_dimension_biome_release(f->aliases[i]);
        f->aliases[i] = NULL;
    }
    for (size_t i = 0; i < COUNT(f->dimensions); ++i) {
        mc_world_dimension_release(f->dimensions[i]);
        f->dimensions[i] = NULL;
    }
    for (size_t i = 0; i < COUNT(f->registries); ++i) {
        if (mc_registry_classes_free(f->registries[i], NULL) != MC_REGISTRY_OK) {
            fputs("fixed manager retirement refused; dependencies retained\n", stderr);
            return false;
        }
        f->registries[i] = NULL;
    }
    mc_java_random_profile_free(f->profile);
    mc_materials_destroy(f->materials);
    free(f);
    return true;
}

static bool test_side(mc_registry_side side, size_t charge_one, size_t charge_four) {
    bool ok = false;
    fixture *f = calloc(1, sizeof(*f));
    mc_world_dimension_error error;
    mc_biome_registry_view view;
    CHECK(f != NULL);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(PROFILE_BUDGET, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(registry_create(f, 0, side, false) && registry_create(f, 1, side, false) &&
          registry_create(f, 2, side, true));
    for (size_t r = 0; r < COUNT(f->registries); ++r)
        CHECK(mc_biome_registry_inspect(f->registries[r], &view, NULL) == MC_REGISTRY_OK &&
              view.phase == MC_REGISTRY_COLD && no_biomes(&view) &&
              resources_unread(&f->sources[r]));
    const int32_t ids[] = {-1, 1};
    const mc_world_dimension_registry_binding binding = {f->registries[0], side, f->profile};
    for (size_t k = 0; k < COUNT(ids); ++k) {
        mc_world_dimension_fields fields;
        mc_world_dimension_state state;
        CHECK(mc_world_dimension_create(ids[k], NULL, &f->dimensions[k], NULL) ==
              MC_WORLD_DIMENSION_OK);
        f->sources[0].observed_dimension = f->dimensions[k];
        CHECK(mc_world_dimension_bind_game(f->dimensions[k], NULL, &binding, &error) ==
              MC_WORLD_DIMENSION_OK);
        CHECK(mc_world_dimension_fields_get(f->dimensions[k], &fields) == MC_WORLD_DIMENSION_OK &&
              fields.a == NULL && fields.b != NULL && fields.g == ids[k] && fields.c == (k == 0) &&
              fields.d == (k == 0) && fields.e == (k == 0));
        CHECK(mc_world_dimension_state_get(f->dimensions[k], &state) == MC_WORLD_DIMENSION_OK &&
              state.phase == MC_WORLD_DIMENSION_READY);
        CHECK(mc_world_dimension_biome_retain(fields.b) == MC_WORLD_DIMENSION_OK);
        f->aliases[k] = fields.b;
        CHECK(mc_biome_registry_inspect(f->registries[0], &view, NULL) == MC_REGISTRY_OK &&
              view.phase == MC_REGISTRY_READY && view.l != NULL && view.m != NULL);
        mc_world_biome_game_fields game;
        CHECK(mc_world_dimension_biome_game_fields_get(fields.b, &game) == MC_WORLD_DIMENSION_OK &&
              game.fixed_e == (k == 0 ? view.l : view.m) && game.registry == f->registries[0]);
        CHECK(resources_complete(&f->sources[0]));
    }
    CHECK(test_arrays(f));
    for (size_t kind = 0; kind < 2; ++kind) {
        CHECK(test_budget(f, side, kind, false, charge_one, charge_four));
        CHECK(test_budget(f, side, kind, true, charge_one, charge_four));
    }
    CHECK(test_failed_rebind(f, side));

    /* Each old manager owns its own registry borrow. Successful replacement
     * and dimension destruction do not invalidate separately retained aliases. */
    const mc_world_dimension_registry_binding replacement = {f->registries[1], side, f->profile};
    CHECK(mc_registry_classes_free(f->registries[0], NULL) == MC_REGISTRY_BUSY);
    for (size_t n = 0; n < 2; ++n) {
        const size_t k = 1 - n; /* Cold m5 as well as the earlier cold l5. */
        mc_world_dimension_fields fields;
        mc_world_biome_game_fields old_game, new_game;
        f->sources[1].observed_dimension = f->dimensions[k];
        f->sources[1].preceding_biome = f->aliases[k];
        CHECK(mc_world_dimension_bind_game(f->dimensions[k], NULL, &replacement, &error) ==
              MC_WORLD_DIMENSION_OK);
        CHECK(mc_world_dimension_fields_get(f->dimensions[k], &fields) == MC_WORLD_DIMENSION_OK &&
              fields.b != f->aliases[k]);
        CHECK(mc_world_dimension_biome_game_fields_get(f->aliases[k], &old_game) ==
                  MC_WORLD_DIMENSION_OK &&
              mc_world_dimension_biome_game_fields_get(fields.b, &new_game) ==
                  MC_WORLD_DIMENSION_OK &&
              old_game.registry == f->registries[0] && new_game.registry == f->registries[1] &&
              old_game.fixed_e != new_game.fixed_e && old_game.fixed_e != NULL &&
              new_game.fixed_e != NULL);
        CHECK(mc_registry_classes_free(f->registries[0], NULL) == MC_REGISTRY_BUSY &&
              mc_registry_classes_free(f->registries[1], NULL) == MC_REGISTRY_BUSY);
        mc_world_dimension_release(f->dimensions[k]);
        f->dimensions[k] = NULL;
    }
    CHECK(resources_complete(&f->sources[1]));
    CHECK(mc_registry_classes_free(f->registries[1], NULL) == MC_REGISTRY_OK);
    f->registries[1] = NULL;
    for (size_t k = 0; k < 2; ++k) {
        const mc_biome *out = NULL;
        mc_world_biome_game_fields game;
        CHECK(mc_world_dimension_biome_game_fields_get(f->aliases[k], &game) ==
              MC_WORLD_DIMENSION_OK);
        CHECK(mc_world_dimension_biome_chunk(f->aliases[k], INT32_MIN, INT32_MAX, &out, NULL) ==
                  MC_WORLD_DIMENSION_OK &&
              out == game.fixed_e && mc_biome_fields_get(out) != NULL);
        CHECK(mc_registry_classes_free(f->registries[0], NULL) == MC_REGISTRY_BUSY);
        mc_world_dimension_biome_release(f->aliases[k]);
        f->aliases[k] = NULL;
        if (k == 0)
            CHECK(mc_registry_classes_free(f->registries[0], NULL) == MC_REGISTRY_BUSY);
    }
    /* The many class-arena arrays above must not self-retain their registry. */
    CHECK(mc_registry_classes_free(f->registries[0], NULL) == MC_REGISTRY_OK);
    f->registries[0] = NULL;
    ok = true;
cleanup:
    if (!cleanup_fixture(f))
        ok = false;
    return ok;
}

int main(void) {
    size_t charge_one = 0, charge_four = 0;
    if (!measure_charge(1, &charge_one) || !measure_charge(4, &charge_four) ||
        charge_four <= charge_one || charge_one > (SIZE_MAX - charge_four) / 2)
        return 1;
    for (int side = MC_REGISTRY_CLIENT; side <= MC_REGISTRY_SERVER; ++side)
        if (!test_side((mc_registry_side)side, charge_one, charge_four))
            return 1;
    printf("fixed biome runtime: %u checks\n", checks);
    return 0;
}
