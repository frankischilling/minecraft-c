#include "mc/resources/translation/translation.h"
#include "mc/world/runtime/lighting.h"

#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/world/runtime/construction_internal.h"
#include "../../../src/world/runtime/provider_internal.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define WORLD_LIMIT (256u * 1024u * 1024u)
#define PROFILE_LIMIT (4u * 1024u * 1024u)

_Static_assert(MC_LIGHT_QUEUE_LIMIT == 1000000u, "source light queue limit");

static unsigned int checks;
static mc_math_trig trig;
static uint8_t zero_blocks[MC_CHUNK_BLOCKS];

#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #c);                        \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

/* Synthetic byte inputs reach real dependencies, without importing original
 * resource assets or replacing the actual class/World constructor flow. */
static const uint8_t language[] = "stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                                  "stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n";

typedef struct stream {
    const uint8_t *bytes;
    size_t length, offset, opens, closes;
    bool closed;
} stream;

typedef struct fixture {
    mc_world_regions *regions;
    mc_world_store *store;
    mc_world_info_name *name;
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_registry_classes *registry;
    mc_registry_resources resources;
    stream streams[3];
    size_t opens;
    bool resource_error;
    mc_world_runtime_classes *classes[2];
    const mc_world_light_type *types[2][2];
    mc_world_runtime *worlds[9];
    mc_world_dimension *dimensions[9];
    mc_world_provider_fields providers[9];
    bool has_provider[9];
    mc_chunk_state *chunks[4];
} fixture;

static mc_java_properties_status read_bytes(void *context, uint8_t *out, size_t capacity,
                                            size_t *count)
{
    stream *s = context;
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

static mc_registry_status open_resource(void *context, const char *name,
                                        mc_java_properties_input *out)
{
    fixture *f = context;
    const char *const names[] = {"/lang/en_US.lang", "/lang/stats_US.lang", "/achievement/map.txt"};
    for (size_t i = 0; i < COUNT(names); ++i) {
        if (name == NULL || strcmp(name, names[i]) != 0)
            continue;
        if (out == NULL || ++f->streams[i].opens != 1 || i != f->opens) {
            f->resource_error = true;
            return MC_REGISTRY_INVALID_ARGUMENT;
        }
        ++f->opens;
        *out = (mc_java_properties_input){&f->streams[i], read_bytes};
        return MC_REGISTRY_OK;
    }
    f->resource_error = true;
    return MC_REGISTRY_INVALID_ARGUMENT;
}

static mc_registry_status close_resource(void *context, const mc_java_properties_input *input)
{
    fixture *f = context;
    if (input == NULL || input->context != &f->streams[2] || input->read != read_bytes) {
        f->resource_error = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    ++f->streams[2].closes;
    f->streams[2].closed = true;
    return MC_REGISTRY_OK;
}

static bool ready_resource(void *context, const mc_java_properties_input *input)
{
    fixture *f = context;
    if (input == NULL || input->read != read_bytes)
        f->resource_error = true;
    return false;
}

static bool same_bounds(const mc_light_bounds *a, const mc_light_bounds *b)
{
    return a->min_x == b->min_x && a->min_y == b->min_y && a->min_z == b->min_z &&
           a->max_x == b->max_x && a->max_y == b->max_y && a->max_z == b->max_z;
}

static bool same_error(const mc_world_storage_error *a, const mc_world_storage_error *b)
{
    return a != NULL && b != NULL && a->status == b->status && a->operation == b->operation &&
           a->system_error == b->system_error;
}

static bool depth_is(fixture *f, int32_t expected)
{
    mc_world_runtime_class_state state;
    return mc_world_runtime_classes_get(f->classes[0], &state) == MC_WORLD_STORAGE_OK &&
           state.enqueue_depth == expected;
}

static bool list_is(mc_world_runtime *world, const mc_world_reference_list *identity, size_t count)
{
    mc_world_light_list_view view;
    mc_world_runtime_fields fields;
    return mc_world_runtime_light_list_get(world, &view, NULL) == MC_WORLD_STORAGE_OK &&
           mc_world_runtime_fields_get(world, &fields) == MC_WORLD_STORAGE_OK &&
           view.identity == identity && view.identity == fields.C && view.count == count &&
           view.capacity >= view.count;
}

static bool region_is(mc_world_runtime *world, size_t index, const mc_world_light_region *identity,
                      const mc_world_light_type *type, const mc_light_bounds *bounds)
{
    const mc_world_light_region *region = NULL;
    if (mc_world_runtime_light_at(world, index, &region, NULL) != MC_WORLD_STORAGE_OK ||
        region == NULL || (identity != NULL && identity != region))
        return false;
    const mc_world_light_region_fields *fields = mc_world_runtime_light_region_fields_get(region);
    return fields != NULL && fields->a == type && same_bounds(&fields->bounds, bounds);
}

static bool create_world(fixture *f, size_t slot, int32_t dimension_id, size_t direct_limit,
                         mc_world_storage_status expected)
{
    bool ok = false;
    mc_world_storage_error error;
    mc_world_runtime_fields fields;
    mc_world_runtime_construction state;
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_direct_allocation_bytes = direct_limit;
    limits.max_provider_chunks = 8;
    CHECK(mc_world_dimension_create(dimension_id, NULL, &f->dimensions[slot], NULL) ==
          MC_WORLD_DIMENSION_OK);
    const mc_world_runtime_world_arguments arguments = {
        .store = f->store, .name = f->name, .seed = 23, .dimension = f->dimensions[slot]};
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, f->classes[0],
                                          &limits, &f->worlds[slot],
                                          &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_construct_world(f->worlds[slot], &arguments, &error) == expected);
    CHECK(mc_world_runtime_construction_get(f->worlds[slot], &state) == MC_WORLD_STORAGE_OK &&
          state.mode == MC_WORLD_RUNTIME_GAME &&
          state.phase == (expected == MC_WORLD_STORAGE_OK ? MC_WORLD_RUNTIME_READY
                                                          : MC_WORLD_RUNTIME_FAILED_CONSTRUCTION));
    CHECK(mc_world_runtime_fields_get(f->worlds[slot], &fields) == MC_WORLD_STORAGE_OK &&
          fields.C != NULL && fields.t == f->dimensions[slot]);
    if (expected == MC_WORLD_STORAGE_OK) {
        CHECK(fields.v != NULL && state.contexts_bound && state.cache_registered);
        CHECK(mc_world_provider_bind_fields(fields.v, &f->providers[slot], &error) ==
              MC_WORLD_STORAGE_OK);
        f->has_provider[slot] = true;
        CHECK(f->providers[slot].world == f->worlds[slot] && f->providers[slot].store == f->store &&
              mc_chunk_cache_validate(f->providers[slot].cache));
    }
    ok = true;
cleanup:
    return ok;
}

static bool publish(fixture *f, size_t slot, size_t chunk, int32_t x, int32_t z)
{
    return mc_chunk_cache_publish(f->providers[slot].cache, x, z, f->chunks[chunk]) &&
           mc_chunk_cache_validate(f->providers[slot].cache);
}

static bool test_types(fixture *f)
{
    bool ok = false;
    mc_world_runtime_class_state before, after;
    for (size_t c = 0; c < COUNT(f->classes); ++c) {
        CHECK(mc_world_runtime_classes_get(f->classes[c], &before) == MC_WORLD_STORAGE_OK &&
              !before.initialized && before.live_worlds == 0 && before.enqueue_depth == 0);
        const size_t profile_bytes = mc_java_random_profile_allocation_bytes(f->profile);
        /* Quiescent readonly view of the actual profile, before any World or
         * default Random construction. First Light use must not consume U. */
        const uint64_t uniquifier =
            atomic_load_explicit(&f->profile->uniquifier, memory_order_relaxed);
        for (int kind = MC_LIGHT_SKY; kind <= MC_LIGHT_BLOCK; ++kind) {
            CHECK(mc_world_runtime_light_type_get(f->classes[c], (mc_light_kind)kind,
                                                  &f->types[c][kind], NULL) == MC_WORLD_STORAGE_OK);
            const mc_world_light_type *again = NULL;
            CHECK(mc_world_runtime_light_type_get(f->classes[c], (mc_light_kind)kind, &again,
                                                  NULL) == MC_WORLD_STORAGE_OK &&
                  again == f->types[c][kind]);
            const mc_world_light_type_fields *fields =
                mc_world_runtime_light_type_fields_get(again);
            CHECK(fields != NULL && fields->kind == (mc_light_kind)kind &&
                  fields->c == (kind == MC_LIGHT_SKY ? 15 : 0));
        }
        CHECK(f->types[c][0] != f->types[c][1]);
        const mc_world_light_type *out = f->types[c][0];
        CHECK(mc_world_runtime_light_type_get(f->classes[c], (mc_light_kind)17, &out, NULL) ==
                  MC_WORLD_STORAGE_INVALID_ARGUMENT &&
              out == f->types[c][0]);
        CHECK(mc_world_runtime_classes_get(f->classes[c], &after) == MC_WORLD_STORAGE_OK &&
              after.initialized == before.initialized && after.live_worlds == 0 &&
              after.enqueue_depth == 0 &&
              mc_java_random_profile_allocation_bytes(f->profile) == profile_bytes);
        CHECK(atomic_load_explicit(&f->profile->uniquifier, memory_order_relaxed) == uniquifier);
    }
    CHECK(f->types[0][0] != f->types[1][0] && f->types[0][1] != f->types[1][1] && f->opens == 0 &&
          mc_world_runtime_light_type_fields_get(NULL) == NULL &&
          mc_world_runtime_light_region_fields_get(NULL) == NULL);
    ok = true;
cleanup:
    return ok;
}

static bool test_presence_and_measure(fixture *f, size_t *base, size_t *record, size_t *pointers)
{
    bool ok = false;
    mc_world_light_list_view view;
    mc_world_runtime_construction before, first, second;
    mc_world_light_error error;
    mc_light_submit_result result = MC_LIGHT_SUBMIT_FULL;
    const mc_light_bounds missing = {96, 1000, 96, 96, -1000, 96};
    const mc_light_bounds truncating = {-1, -700, -1, 0, 900, 0};
    const mc_light_bounds ordinary = {0, 64, 0, 0, 64, 0};
    CHECK(create_world(f, 0, 0, WORLD_LIMIT, MC_WORLD_STORAGE_OK));
    CHECK(publish(f, 0, 0, 0, 0));
    CHECK(mc_world_runtime_light_list_get(f->worlds[0], &view, NULL) == MC_WORLD_STORAGE_OK &&
          view.identity != NULL && view.count == 0 && view.capacity == 0);
    const mc_world_reference_list *identity = view.identity;
    CHECK(mc_world_runtime_construction_get(f->worlds[0], &before) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_light_submit(f->worlds[0], f->types[0][0], &missing, false, &result,
                                        &error) == MC_WORLD_STORAGE_OK &&
          result == MC_LIGHT_SUBMIT_SKIPPED && error.acquisition == MC_CHUNK_ACQUIRE_OK &&
          list_is(f->worlds[0], identity, 0) && depth_is(f, 0));
    CHECK(mc_world_store_chunk_count(f->store) == COUNT(f->chunks));

    /* (-1+0)/2 truncates to0, not -1. The -1/-1 cache key is still absent. */
    CHECK(mc_world_runtime_light_submit(f->worlds[0], f->types[0][0], &truncating, false, &result,
                                        &error) == MC_WORLD_STORAGE_OK &&
          result == MC_LIGHT_SUBMIT_APPENDED && list_is(f->worlds[0], identity, 1) &&
          depth_is(f, 0));
    CHECK(region_is(f->worlds[0], 0, NULL, f->types[0][0], &truncating));
    CHECK(mc_world_runtime_construction_get(f->worlds[0], &first) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_light_list_get(f->worlds[0], &view, NULL) == MC_WORLD_STORAGE_OK &&
          view.capacity >= 2);
    const size_t first_capacity = view.capacity;
    CHECK(mc_world_runtime_light_submit(f->worlds[0], f->types[0][1], &ordinary, false, &result,
                                        &error) == MC_WORLD_STORAGE_OK &&
          result == MC_LIGHT_SUBMIT_APPENDED && list_is(f->worlds[0], identity, 2));
    CHECK(mc_world_runtime_construction_get(f->worlds[0], &second) == MC_WORLD_STORAGE_OK &&
          second.direct_allocation_bytes > first.direct_allocation_bytes);
    CHECK(mc_world_runtime_light_list_get(f->worlds[0], &view, NULL) == MC_WORLD_STORAGE_OK &&
          view.capacity == first_capacity);
    *base = before.direct_allocation_bytes;
    *record = second.direct_allocation_bytes - first.direct_allocation_bytes;
    CHECK(first.direct_allocation_bytes - before.direct_allocation_bytes > *record);
    *pointers = first.direct_allocation_bytes - before.direct_allocation_bytes - *record;
    CHECK(*base > 0 && *record > 0 && *pointers > 0 && *record <= SIZE_MAX - *base &&
          *pointers <= SIZE_MAX - *base - *record);
    const mc_world_light_region *one = NULL, *two = NULL;
    CHECK(mc_world_runtime_light_at(f->worlds[0], 0, &one, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_light_at(f->worlds[0], 1, &two, NULL) == MC_WORLD_STORAGE_OK &&
          one != two);
    CHECK(region_is(f->worlds[0], 0, one, f->types[0][0], &truncating) &&
          region_is(f->worlds[0], 1, two, f->types[0][1], &ordinary));

    /* The actual provider-owned blank is a real override, not a NULL map cell. */
    CHECK(mc_chunk_cache_publish(f->providers[0].cache, 1, 0, f->providers[0].fallback) &&
          mc_chunk_cache_validate(f->providers[0].cache));
    const mc_light_bounds blank = {16, 0, 0, 16, 127, 0};
    CHECK(mc_world_runtime_light_submit(f->worlds[0], f->types[0][1], &blank, false, &result,
                                        &error) == MC_WORLD_STORAGE_OK &&
          result == MC_LIGHT_SUBMIT_SKIPPED && list_is(f->worlds[0], identity, 2) &&
          depth_is(f, 0));

    CHECK(publish(f, 0, 1, -1, -1) && publish(f, 0, 2, 2000000, 0));
    const mc_light_bounds wrapped = {INT32_MAX, -2, INT32_MAX, INT32_MAX, 300, INT32_MAX};
    CHECK(mc_world_runtime_light_submit(f->worlds[0], NULL, &wrapped, false, &result, &error) ==
              MC_WORLD_STORAGE_OK &&
          result == MC_LIGHT_SUBMIT_APPENDED && region_is(f->worlds[0], 2, NULL, NULL, &wrapped));
    const mc_light_bounds wrapped_zero = {INT32_MIN, 3, INT32_MIN, INT32_MIN, -3, INT32_MIN};
    CHECK(mc_world_runtime_light_submit(f->worlds[0], f->types[0][1], &wrapped_zero, false, &result,
                                        &error) == MC_WORLD_STORAGE_OK &&
          result == MC_LIGHT_SUBMIT_APPENDED &&
          region_is(f->worlds[0], 3, NULL, f->types[0][1], &wrapped_zero));
    const mc_light_bounds border = {32000000, -99, 0, 32000000, 999, 0};
    CHECK(mc_world_runtime_light_submit(f->worlds[0], f->types[0][0], &border, false, &result,
                                        &error) == MC_WORLD_STORAGE_OK &&
          result == MC_LIGHT_SUBMIT_APPENDED && list_is(f->worlds[0], identity, 5) &&
          region_is(f->worlds[0], 4, NULL, f->types[0][0], &border) && depth_is(f, 0));

    /* Foreign class handles are native admission errors, not same-kind enums. */
    result = MC_LIGHT_SUBMIT_FULL;
    CHECK(mc_world_runtime_light_submit(f->worlds[0], f->types[1][0], &ordinary, false, &result,
                                        &error) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          result == MC_LIGHT_SUBMIT_FULL && error.bytecode == 0 &&
          list_is(f->worlds[0], identity, 5) && depth_is(f, 0));
    const mc_world_light_region *guard = one;
    CHECK(mc_world_runtime_light_at(f->worlds[0], 5, &guard, NULL) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          guard == one);
    ok = true;
cleanup:
    return ok;
}

static bool test_merges(fixture *f)
{
    bool ok = false;
    mc_world_light_list_view view;
    mc_world_light_error error;
    mc_light_submit_result result;
    const mc_light_bounds point = {0, 0, 0, 0, 0, 0};
    CHECK(create_world(f, 1, 0, WORLD_LIMIT, MC_WORLD_STORAGE_OK));
    CHECK(publish(f, 1, 0, 0, 0) && publish(f, 1, 3, 2048, 2048));
    CHECK(mc_world_runtime_light_list_get(f->worlds[1], &view, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_reference_list *identity = view.identity;
    const mc_world_light_type *types[] = {f->types[0][0], f->types[0][1], NULL,
                                          f->types[0][1], NULL,           f->types[0][1]};
    const mc_world_light_region *records[9] = {NULL};
    for (size_t i = 0; i < COUNT(types); ++i) {
        CHECK(mc_world_runtime_light_submit(f->worlds[1], types[i], &point, false, &result,
                                            &error) == MC_WORLD_STORAGE_OK &&
              result == MC_LIGHT_SUBMIT_APPENDED);
        CHECK(mc_world_runtime_light_at(f->worlds[1], i, &records[i], NULL) ==
                  MC_WORLD_STORAGE_OK &&
              records[i] != NULL);
        for (size_t j = 0; j < i; ++j)
            CHECK(records[i] != records[j]);
    }
    /* The old Sky is behind FIVE total entries, although none is Sky. */
    CHECK(mc_world_runtime_light_submit(f->worlds[1], f->types[0][0], &point, true, &result,
                                        &error) == MC_WORLD_STORAGE_OK &&
          result == MC_LIGHT_SUBMIT_APPENDED && list_is(f->worlds[1], identity, 7));
    for (size_t i = 7; i < 9; ++i) {
        CHECK(mc_world_runtime_light_submit(f->worlds[1], f->types[0][0], &point, false, &result,
                                            &error) == MC_WORLD_STORAGE_OK &&
              result == MC_LIGHT_SUBMIT_APPENDED);
        CHECK(mc_world_runtime_light_at(f->worlds[1], i, &records[i], NULL) == MC_WORLD_STORAGE_OK);
    }
    const mc_light_bounds expanded = {0, 0, 0, 1, 0, 0};
    CHECK(mc_world_runtime_light_submit(f->worlds[1], f->types[0][0], &expanded, true, &result,
                                        &error) == MC_WORLD_STORAGE_OK &&
          result == MC_LIGHT_SUBMIT_MERGED && list_is(f->worlds[1], identity, 9) &&
          region_is(f->worlds[1], 7, records[7], f->types[0][0], &point) &&
          region_is(f->worlds[1], 8, records[8], f->types[0][0], &expanded));
    const mc_light_bounds nullable = {0, 100, 0, 0, 100, 0};
    const mc_light_bounds nullable_expanded = {0, 100, 0, 1, 100, 0};
    CHECK(mc_world_runtime_light_submit(f->worlds[1], NULL, &nullable, false, &result, &error) ==
              MC_WORLD_STORAGE_OK &&
          result == MC_LIGHT_SUBMIT_APPENDED);
    const mc_world_light_region *null_record = NULL;
    CHECK(mc_world_runtime_light_at(f->worlds[1], 9, &null_record, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_light_submit(f->worlds[1], NULL, &nullable_expanded, true, &result,
                                        &error) == MC_WORLD_STORAGE_OK &&
          result == MC_LIGHT_SUBMIT_MERGED && list_is(f->worlds[1], identity, 10) &&
          region_is(f->worlds[1], 9, null_record, NULL, &nullable_expanded));

    /* Retained original merge fixtures: INT_MIN-1 wraps in the expansion
     * check, inverted bounds are not normalized, and wrapped volume matters. */
    const mc_light_bounds minimum = {0, INT32_MIN, 0, 0, INT32_MIN, 0};
    const mc_light_bounds minimum_next = {0, INT32_MIN, 0, 0, INT32_MIN + 1, 0};
    CHECK(mc_world_runtime_light_submit(f->worlds[1], f->types[0][0], &minimum, false, &result,
                                        &error) == MC_WORLD_STORAGE_OK &&
          result == MC_LIGHT_SUBMIT_APPENDED);
    const mc_world_light_region *minimum_record = NULL;
    CHECK(mc_world_runtime_light_at(f->worlds[1], 10, &minimum_record, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_light_submit(f->worlds[1], f->types[0][0], &minimum_next, true, &result,
                                        &error) == MC_WORLD_STORAGE_OK &&
          result == MC_LIGHT_SUBMIT_APPENDED &&
          region_is(f->worlds[1], 10, minimum_record, f->types[0][0], &minimum));
    const mc_light_bounds inverted = {4, 4, 4, -4, -4, -4};
    CHECK(mc_world_runtime_light_submit(f->worlds[1], f->types[0][0], &inverted, false, &result,
                                        &error) == MC_WORLD_STORAGE_OK &&
          result == MC_LIGHT_SUBMIT_APPENDED);
    CHECK(mc_world_runtime_light_submit(f->worlds[1], f->types[0][0], &inverted, true, &result,
                                        &error) == MC_WORLD_STORAGE_OK &&
          result == MC_LIGHT_SUBMIT_MERGED &&
          region_is(f->worlds[1], 12, NULL, f->types[0][0], &inverted));
    const mc_light_bounds volume = {0, 0, 0, 65536, 65536, 65536};
    const mc_light_bounds volume_next = {0, 0, 0, 65537, 65536, 65536};
    CHECK(mc_world_runtime_light_submit(f->worlds[1], f->types[0][0], &volume, false, &result,
                                        &error) == MC_WORLD_STORAGE_OK &&
          result == MC_LIGHT_SUBMIT_APPENDED);
    const mc_world_light_region *volume_record = NULL;
    CHECK(mc_world_runtime_light_at(f->worlds[1], 13, &volume_record, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_light_submit(f->worlds[1], f->types[0][0], &volume_next, true, &result,
                                        &error) == MC_WORLD_STORAGE_OK &&
          result == MC_LIGHT_SUBMIT_MERGED && list_is(f->worlds[1], identity, 14) &&
          region_is(f->worlds[1], 13, volume_record, f->types[0][0], &volume_next) &&
          depth_is(f, 0));
    ok = true;
cleanup:
    return ok;
}

static bool test_depth_controls(fixture *f)
{
    bool ok = false;
    mc_world_light_list_view before[2], after;
    mc_world_light_error error;
    mc_light_submit_result result;
    const mc_light_bounds point = {0, 64, 0, 0, 64, 0};
    for (size_t i = 0; i < 2; ++i)
        CHECK(mc_world_runtime_light_list_get(f->worlds[i], &before[i], NULL) ==
              MC_WORLD_STORAGE_OK);

    /* Deliberate native/source scalar controls of the ACTUAL declaring class
     * cell, after genuine World construction. These are not naturally reached
     * recursive depth observations, fabricated Worlds, or class-phase writes. */
    f->classes[0]->enqueue_depth = 49;
    for (size_t i = 0; i < 2; ++i) {
        CHECK(mc_world_runtime_light_submit(f->worlds[i], f->types[0][0], &point, false, &result,
                                            &error) == MC_WORLD_STORAGE_OK &&
              result == MC_LIGHT_SUBMIT_SKIPPED && depth_is(f, 49) &&
              list_is(f->worlds[i], before[i].identity, before[i].count));
    }
    f->classes[0]->enqueue_depth = 50;
    CHECK(mc_world_runtime_light_submit(f->worlds[0], f->types[0][0], &point, false, &result,
                                        &error) == MC_WORLD_STORAGE_OK &&
          result == MC_LIGHT_SUBMIT_APPENDED && depth_is(f, 50) &&
          list_is(f->worlds[0], before[0].identity, before[0].count + 1));
    f->classes[0]->enqueue_depth = INT32_MAX;
    CHECK(mc_world_runtime_light_submit(f->worlds[1], NULL, &point, false, &result, &error) ==
              MC_WORLD_STORAGE_OK &&
          result == MC_LIGHT_SUBMIT_APPENDED && depth_is(f, INT32_MAX) &&
          list_is(f->worlds[1], before[1].identity, before[1].count + 1));
    CHECK(mc_world_runtime_light_list_get(f->worlds[0], &after, NULL) == MC_WORLD_STORAGE_OK &&
          after.identity == before[0].identity);
    mc_world_runtime_class_state other;
    CHECK(mc_world_runtime_classes_get(f->classes[1], &other) == MC_WORLD_STORAGE_OK &&
          other.enqueue_depth == 0 && other.live_worlds == 0);
    ok = true;
cleanup:
    f->classes[0]->enqueue_depth = 0;
    return ok;
}

static bool test_budgets(fixture *f, size_t base, size_t record, size_t pointers)
{
    bool ok = false;
    const size_t limits[] = {base + record - 1, base + record + pointers - 1};
    const unsigned int pcs[] = {219, 237};
    const mc_light_bounds bounds = {0, 64, 0, 0, 64, 0};
    for (size_t n = 0; n < COUNT(limits); ++n) {
        const size_t slot = 2 + n;
        mc_world_light_list_view view;
        mc_world_runtime_construction state;
        mc_world_light_error error;
        CHECK(create_world(f, slot, 0, limits[n], MC_WORLD_STORAGE_OK));
        CHECK(publish(f, slot, 0, 0, 0));
        CHECK(mc_world_runtime_construction_get(f->worlds[slot], &state) == MC_WORLD_STORAGE_OK &&
              state.direct_allocation_bytes == base);
        CHECK(mc_world_runtime_light_list_get(f->worlds[slot], &view, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              view.count == 0 && view.capacity == 0);
        for (size_t attempt = 0; attempt < 2; ++attempt) {
            mc_light_submit_result result = MC_LIGHT_SUBMIT_MERGED;
            CHECK(mc_world_runtime_light_submit(f->worlds[slot], f->types[0][0], &bounds, false,
                                                &result, &error) == MC_WORLD_STORAGE_LIMIT);
            CHECK(result == MC_LIGHT_SUBMIT_MERGED && error.status == MC_WORLD_STORAGE_LIMIT &&
                  error.bytecode == pcs[n] && depth_is(f, 0) &&
                  list_is(f->worlds[slot], view.identity, 0));
            CHECK(mc_world_runtime_construction_get(f->worlds[slot], &state) ==
                      MC_WORLD_STORAGE_OK &&
                  state.phase == MC_WORLD_RUNTIME_READY && state.direct_allocation_bytes == base);
            CHECK(mc_world_runtime_light_list_get(f->worlds[slot], &view, NULL) ==
                      MC_WORLD_STORAGE_OK &&
                  view.count == 0 && view.capacity == 0);
            const mc_world_storage_error *last = mc_world_runtime_last_error(f->worlds[slot]);
            CHECK(last != NULL && last->status == MC_WORLD_STORAGE_LIMIT &&
                  last->operation != NULL);
        }
    }
    ok = true;
cleanup:
    return ok;
}

/* Exercise a NONEMPTY list growth whose final retained size fits the limit
 * but whose real old+new pointer-array peak does not. All charges come from
 * actual successful append history, never a private region/allocation size. */
static bool test_nonempty_growth(fixture *f, size_t base, size_t record)
{
    bool ok = false;
    const mc_world_light_region **records = NULL;
    mc_world_light_list_view full, grown, limited;
    mc_world_runtime_construction before, after, current;
    mc_world_light_error error;
    mc_light_submit_result result;
    const mc_light_bounds bounds = {0, 64, 0, 0, 64, 0};
    CHECK(create_world(f, 7, 0, WORLD_LIMIT, MC_WORLD_STORAGE_OK));
    CHECK(publish(f, 7, 0, 0, 0));
    CHECK(mc_world_runtime_light_submit(f->worlds[7], f->types[0][0], &bounds, false, &result,
                                        &error) == MC_WORLD_STORAGE_OK &&
          result == MC_LIGHT_SUBMIT_APPENDED);
    CHECK(mc_world_runtime_light_list_get(f->worlds[7], &full, NULL) == MC_WORLD_STORAGE_OK &&
          full.count == 1 && full.capacity >= full.count && full.capacity < MC_LIGHT_QUEUE_LIMIT);
    const size_t old_capacity = full.capacity;
    for (size_t i = full.count; i < old_capacity; ++i) {
        CHECK(mc_world_runtime_light_submit(f->worlds[7], f->types[0][0], &bounds, false, &result,
                                            &error) == MC_WORLD_STORAGE_OK &&
              result == MC_LIGHT_SUBMIT_APPENDED);
    }
    CHECK(mc_world_runtime_light_list_get(f->worlds[7], &full, NULL) == MC_WORLD_STORAGE_OK &&
          full.count == old_capacity && full.capacity == old_capacity && depth_is(f, 0));
    CHECK(mc_world_runtime_construction_get(f->worlds[7], &before) == MC_WORLD_STORAGE_OK);
    CHECK(old_capacity < SIZE_MAX && record <= (SIZE_MAX - base) / (old_capacity + 1));
    const size_t old_records = old_capacity * record;
    const size_t new_records = (old_capacity + 1) * record;
    CHECK(before.direct_allocation_bytes > base + old_records);
    const size_t old_pointers = before.direct_allocation_bytes - base - old_records;

    CHECK(mc_world_runtime_light_submit(f->worlds[7], f->types[0][0], &bounds, false, &result,
                                        &error) == MC_WORLD_STORAGE_OK &&
          result == MC_LIGHT_SUBMIT_APPENDED);
    CHECK(mc_world_runtime_light_list_get(f->worlds[7], &grown, NULL) == MC_WORLD_STORAGE_OK &&
          grown.identity == full.identity && grown.count == old_capacity + 1 &&
          grown.capacity > old_capacity && depth_is(f, 0));
    CHECK(mc_world_runtime_construction_get(f->worlds[7], &after) == MC_WORLD_STORAGE_OK &&
          after.direct_allocation_bytes > base + new_records);
    const size_t new_pointers = after.direct_allocation_bytes - base - new_records;
    CHECK(new_pointers > old_pointers && before.direct_allocation_bytes <= SIZE_MAX - record &&
          new_pointers <= SIZE_MAX - before.direct_allocation_bytes - record);
    const size_t peak = before.direct_allocation_bytes + record + new_pointers;
    CHECK(peak > after.direct_allocation_bytes &&
          peak - after.direct_allocation_bytes == old_pointers);
    const size_t limit = peak - 1;
    CHECK(limit >= after.direct_allocation_bytes && limit >= before.direct_allocation_bytes);

    CHECK(create_world(f, 8, 0, limit, MC_WORLD_STORAGE_OK));
    CHECK(publish(f, 8, 0, 0, 0));
    CHECK(old_capacity <= SIZE_MAX / sizeof(*records));
    records = malloc(old_capacity * sizeof(*records));
    CHECK(records != NULL);
    for (size_t i = 0; i < old_capacity; ++i) {
        CHECK(mc_world_runtime_light_submit(f->worlds[8], f->types[0][0], &bounds, false, &result,
                                            &error) == MC_WORLD_STORAGE_OK &&
              result == MC_LIGHT_SUBMIT_APPENDED && depth_is(f, 0));
        CHECK(mc_world_runtime_light_at(f->worlds[8], i, &records[i], NULL) ==
                  MC_WORLD_STORAGE_OK &&
              records[i] != NULL);
        for (size_t j = 0; j < i; ++j)
            CHECK(records[i] != records[j]);
    }
    CHECK(mc_world_runtime_light_list_get(f->worlds[8], &limited, NULL) == MC_WORLD_STORAGE_OK &&
          limited.count == old_capacity && limited.capacity == old_capacity);
    CHECK(mc_world_runtime_construction_get(f->worlds[8], &current) == MC_WORLD_STORAGE_OK &&
          current.direct_allocation_bytes == before.direct_allocation_bytes);
    for (size_t attempt = 0; attempt < 2; ++attempt) {
        result = MC_LIGHT_SUBMIT_MERGED;
        CHECK(mc_world_runtime_light_submit(f->worlds[8], f->types[0][0], &bounds, false, &result,
                                            &error) == MC_WORLD_STORAGE_LIMIT);
        CHECK(result == MC_LIGHT_SUBMIT_MERGED && error.status == MC_WORLD_STORAGE_LIMIT &&
              error.bytecode == 237 && depth_is(f, 0) &&
              list_is(f->worlds[8], limited.identity, old_capacity));
        mc_world_light_list_view unchanged;
        CHECK(mc_world_runtime_light_list_get(f->worlds[8], &unchanged, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              unchanged.capacity == old_capacity);
        CHECK(mc_world_runtime_construction_get(f->worlds[8], &current) == MC_WORLD_STORAGE_OK &&
              current.phase == MC_WORLD_RUNTIME_READY &&
              current.direct_allocation_bytes == before.direct_allocation_bytes);
        for (size_t i = 0; i < old_capacity; ++i)
            CHECK(region_is(f->worlds[8], i, records[i], f->types[0][0], &bounds));
        const mc_world_storage_error *last = mc_world_runtime_last_error(f->worlds[8]);
        CHECK(last != NULL && last->status == MC_WORLD_STORAGE_LIMIT && last->operation != NULL);
    }
    /* A no-allocation merge remains usable under the SAME immutable limit. */
    CHECK(mc_world_runtime_light_submit(f->worlds[8], f->types[0][0], &bounds, true, &result,
                                        &error) == MC_WORLD_STORAGE_OK &&
          result == MC_LIGHT_SUBMIT_MERGED &&
          list_is(f->worlds[8], limited.identity, old_capacity) && depth_is(f, 0));
    CHECK(mc_world_runtime_construction_get(f->worlds[8], &current) == MC_WORLD_STORAGE_OK &&
          current.direct_allocation_bytes == before.direct_allocation_bytes);
    ok = true;
cleanup:
    free(records);
    return ok;
}

static bool test_no_sky_and_raw(fixture *f)
{
    bool ok = false;
    mc_world_runtime_fields fields;
    mc_world_dimension_fields dimension;
    mc_world_light_list_view view;
    mc_world_light_error error;
    mc_light_submit_result result = MC_LIGHT_SUBMIT_FULL;
    const mc_light_bounds bounds = {0, 64, 0, 0, 64, 0};
    /* The actual fixed setup reaches e=true; the following unsupported Nether
     * provider leaves a genuinely failed World with t but no v. */
    CHECK(create_world(f, 5, -1, WORLD_LIMIT, MC_WORLD_STORAGE_UNSUPPORTED_STATE));
    CHECK(mc_world_runtime_fields_get(f->worlds[5], &fields) == MC_WORLD_STORAGE_OK &&
          fields.v == NULL &&
          mc_world_dimension_fields_get(fields.t, &dimension) == MC_WORLD_DIMENSION_OK &&
          dimension.e && dimension.a == f->worlds[5]);
    CHECK(f->opens == 3 && !f->resource_error && f->streams[2].closes == 1);
    CHECK(mc_world_runtime_light_list_get(f->worlds[5], &view, NULL) == MC_WORLD_STORAGE_OK &&
          view.count == 0);
    const mc_world_storage_error *previous_view = mc_world_runtime_last_error(f->worlds[5]);
    CHECK(previous_view != NULL);
    const mc_world_storage_error previous = *previous_view;
    CHECK(previous.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE);
    f->classes[0]->enqueue_depth = 49;
    CHECK(mc_world_runtime_light_submit(f->worlds[5], f->types[0][0], &bounds, false, &result,
                                        &error) == MC_WORLD_STORAGE_OK &&
          result == MC_LIGHT_SUBMIT_SKIPPED && depth_is(f, 49) &&
          same_error(&previous, mc_world_runtime_last_error(f->worlds[5])));
    f->classes[0]->enqueue_depth = 0;
    const mc_world_light_type *types[] = {f->types[0][1], NULL};
    for (size_t i = 0; i < COUNT(types); ++i) {
        result = MC_LIGHT_SUBMIT_FULL;
        CHECK(mc_world_runtime_light_submit(f->worlds[5], types[i], &bounds, false, &result,
                                            &error) != MC_WORLD_STORAGE_OK);
        CHECK(result == MC_LIGHT_SUBMIT_FULL && error.bytecode == 67 &&
              error.provider_bytecode == 6 && error.cause.status != MC_WORLD_STORAGE_OK &&
              depth_is(f, 0) && list_is(f->worlds[5], view.identity, 0));
        CHECK(same_error(&error.cause, mc_world_runtime_last_error(f->worlds[5])));
    }

    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, f->classes[0],
                                          NULL, &f->worlds[6], NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_light_list_get(f->worlds[6], &view, NULL) == MC_WORLD_STORAGE_OK &&
          view.identity == NULL && view.count == 0 && view.capacity == 0);
    result = MC_LIGHT_SUBMIT_FULL;
    CHECK(mc_world_runtime_light_submit(f->worlds[6], NULL, &bounds, false, &result, &error) !=
          MC_WORLD_STORAGE_OK);
    CHECK(result == MC_LIGHT_SUBMIT_FULL && error.bytecode == 4 && depth_is(f, 0));
    ok = true;
cleanup:
    f->classes[0]->enqueue_depth = 0;
    return ok;
}

static bool test_million(fixture *f)
{
    bool ok = false;
    mc_world_light_list_view initial, full, cleared;
    mc_light_submit_result result;
    mc_world_light_error error;
    const mc_light_bounds bounds = {0, 64, 0, 0, 64, 0};
    CHECK(create_world(f, 4, 0, WORLD_LIMIT, MC_WORLD_STORAGE_OK));
    CHECK(publish(f, 4, 0, 0, 0));
    CHECK(mc_world_runtime_light_list_get(f->worlds[4], &initial, NULL) == MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < MC_LIGHT_QUEUE_LIMIT; ++i) {
        CHECK(mc_world_runtime_light_submit(f->worlds[4], f->types[0][0], &bounds, false, &result,
                                            &error) == MC_WORLD_STORAGE_OK &&
              result == MC_LIGHT_SUBMIT_APPENDED);
        CHECK(list_is(f->worlds[4], initial.identity, i + 1) && depth_is(f, 0));
    }
    CHECK(mc_world_runtime_light_list_get(f->worlds[4], &full, NULL) == MC_WORLD_STORAGE_OK &&
          full.identity == initial.identity && full.count == MC_LIGHT_QUEUE_LIMIT);
    /* Production must emit the actual diagnostic before clear296. The driver
     * retains stdout; this test never emits, suppresses or substitutes it. */
    CHECK(mc_world_runtime_light_submit(f->worlds[4], f->types[0][0], &bounds, false, &result,
                                        &error) == MC_WORLD_STORAGE_OK &&
          result == MC_LIGHT_SUBMIT_CLEARED && depth_is(f, 0));
    CHECK(mc_world_runtime_light_list_get(f->worlds[4], &cleared, NULL) == MC_WORLD_STORAGE_OK &&
          cleared.identity == initial.identity && cleared.count == 0 &&
          cleared.capacity == full.capacity);
    CHECK(mc_world_runtime_light_submit(f->worlds[4], NULL, &bounds, false, &result, &error) ==
              MC_WORLD_STORAGE_OK &&
          result == MC_LIGHT_SUBMIT_APPENDED && list_is(f->worlds[4], initial.identity, 1));
    ok = true;
cleanup:
    return ok;
}

static bool retire_one(fixture *f, size_t slot)
{
    if (f->worlds[slot] == NULL)
        return true;
    /* Public cache storage permits explicit, validated fixture retirement.
     * This is not C-list manipulation or a claimed gameplay unload. */
    if (f->has_provider[slot]) {
        f->providers[slot].cache->count = 0;
        f->providers[slot].cache->insertion_count = 0;
        if (!mc_chunk_cache_validate(f->providers[slot].cache))
            return false;
    }
    if (mc_world_runtime_retire_world(f->worlds[slot], NULL) != MC_WORLD_STORAGE_OK ||
        mc_world_runtime_free(f->worlds[slot], NULL) != MC_WORLD_STORAGE_OK)
        return false;
    f->worlds[slot] = NULL;
    f->has_provider[slot] = false;
    return true;
}

static bool test_retained_queue_retirement(fixture *f)
{
    bool ok = false;
    mc_world_light_list_view before, retired;
    mc_world_runtime_construction state;
    mc_world_light_error error;
    mc_world_storage_error registry_error;
    mc_light_submit_result result = MC_LIGHT_SUBMIT_FULL;
    const mc_light_bounds bounds = {0, 64, 0, 0, 64, 0};
    CHECK(mc_world_runtime_light_list_get(f->worlds[1], &before, NULL) == MC_WORLD_STORAGE_OK &&
          before.count > 0);
    const mc_world_light_region *region = NULL;
    CHECK(mc_world_runtime_light_at(f->worlds[1], 0, &region, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_light_region_fields *region_fields =
        mc_world_runtime_light_region_fields_get(region);
    CHECK(region_fields != NULL);
    const mc_world_light_region_fields saved = *region_fields;
    f->providers[1].cache->count = 0;
    f->providers[1].cache->insertion_count = 0;
    CHECK(mc_chunk_cache_validate(f->providers[1].cache));
    CHECK(mc_world_runtime_retire_world(f->worlds[1], NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_construction_get(f->worlds[1], &state) == MC_WORLD_STORAGE_OK &&
          state.phase == MC_WORLD_RUNTIME_RETIRED);
    CHECK(mc_world_runtime_light_list_get(f->worlds[1], &retired, NULL) == MC_WORLD_STORAGE_OK &&
          retired.identity == before.identity && retired.count == before.count &&
          region_is(f->worlds[1], 0, region, saved.a, &saved.bounds));
    CHECK(mc_world_runtime_light_submit(f->worlds[1], NULL, &bounds, false, &result, &error) !=
          MC_WORLD_STORAGE_OK);
    CHECK(result == MC_LIGHT_SUBMIT_FULL && error.bytecode == 0 && depth_is(f, 0));
    CHECK(mc_registry_classes_free(f->registry, NULL) == MC_REGISTRY_BUSY &&
          mc_world_runtime_classes_free(f->classes[0], &registry_error) == MC_WORLD_STORAGE_BUSY);
    CHECK(mc_world_runtime_free(f->worlds[1], NULL) == MC_WORLD_STORAGE_OK);
    f->worlds[1] = NULL;
    f->has_provider[1] = false;
    CHECK(mc_registry_classes_free(f->registry, NULL) == MC_REGISTRY_BUSY);
    ok = true;
cleanup:
    return ok;
}

static bool cleanup_fixture(fixture *f)
{
    if (f == NULL)
        return true;
    if (f->classes[0] != NULL)
        f->classes[0]->enqueue_depth = 0;
    for (size_t i = 0; i < COUNT(f->worlds); ++i)
        if (!retire_one(f, i))
            return false;
    for (size_t i = 0; i < COUNT(f->dimensions); ++i) {
        mc_world_dimension_release(f->dimensions[i]);
        f->dimensions[i] = NULL;
    }
    for (size_t i = 0; i < COUNT(f->chunks); ++i) {
        if (f->chunks[i] == NULL)
            continue;
        f->chunks[i]->active = false;
        if (mc_world_store_retire_chunk(f->store, f->chunks[i], NULL) != MC_WORLD_STORAGE_OK)
            return false;
        f->chunks[i] = NULL;
    }
    for (size_t i = 0; i < COUNT(f->classes); ++i) {
        if (mc_world_runtime_classes_free(f->classes[i], NULL) != MC_WORLD_STORAGE_OK)
            return false;
        f->classes[i] = NULL;
    }
    if (mc_registry_classes_free(f->registry, NULL) != MC_REGISTRY_OK)
        return false;
    mc_world_info_name_release(f->name);
    mc_world_store_free(f->store);
    mc_world_regions_free(f->regions);
    mc_java_random_profile_free(f->profile);
    mc_materials_destroy(f->materials);
    free(f);
    return true;
}

static bool decode_path(const char *hex, char *out, size_t capacity)
{
    const size_t n = strlen(hex);
    if (n == 0 || n % 2u != 0 || n / 2u >= capacity)
        return false;
    for (size_t i = 0; i < n / 2u; ++i) {
        unsigned int value = 0;
        for (size_t j = 0; j < 2; ++j) {
            const unsigned char c = (unsigned char)hex[2 * i + j];
            const unsigned int digit = c >= '0' && c <= '9'   ? (unsigned int)(c - '0')
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

int main(int argc, char **argv)
{
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    uint8_t map[256];
    const uint16_t title[] = {'L', 'i', 'g', 'h', 't'};
    size_t base = 0, record = 0, pointers = 0;
    if (argc != 3 || strcmp(argv[1], "world-light-queue-runtime") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL);
    for (size_t i = 0; i < COUNT(map); ++i)
        map[i] = (uint8_t)i;
    const mc_world_store_options options = {.session_token = 123,
                                            .block_map = map,
                                            .block_map_length = sizeof(map),
                                            .limits = mc_world_store_default_limits()};
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_open(root, f->regions, &options, &f->store, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(PROFILE_LIMIT, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    f->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1};
    f->streams[1] = (stream){.bytes = language};
    f->streams[2] = (stream){.bytes = language};
    f->resources = (mc_registry_resources){f, open_resource, close_resource, ready_resource};
    const mc_registry_options registry_options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                                  &f->resources};
    CHECK(mc_registry_classes_allocate(&registry_options, &f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes[0],
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_SERVER, f->profile, &trig, &f->classes[1],
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(f->classes[0], f->registry, NULL) ==
          MC_REGISTRY_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    CHECK(test_types(f));
    const int32_t coordinates[][2] = {{0, 0}, {-1, -1}, {2000000, 0}, {2048, 2048}};
    mc_chunk_data data;
    CHECK(mc_chunk_data_init(&data, zero_blocks, sizeof(zero_blocks)));
    for (size_t i = 0; i < COUNT(coordinates); ++i) {
        const mc_chunk_state source = {.data = &data,
                                       .x = coordinates[i][0],
                                       .z = coordinates[i][1],
                                       .active = true,
                                       .terrain_populated = true};
        CHECK(mc_world_store_retain_chunk(f->store, 0, &source, &f->chunks[i], NULL) ==
              MC_WORLD_STORAGE_OK);
        CHECK(mc_world_store_owns_chunk(f->store, f->chunks[i]));
    }
    CHECK(test_presence_and_measure(f, &base, &record, &pointers));
    CHECK(test_merges(f));
    CHECK(test_depth_controls(f));
    CHECK(test_budgets(f, base, record, pointers));
    CHECK(test_nonempty_growth(f, base, record));
    CHECK(test_no_sky_and_raw(f));
    CHECK(test_million(f));
    CHECK(test_retained_queue_retirement(f));
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("light queue fixture retirement refused; live dependencies retained\n", stderr);
        return 1;
    }
    if (result == 0 &&
        printf("PASS: %u constructed World light queue checks (world-light-queue-runtime)\n",
               checks) < 0)
        result = 1;
    return result;
}
