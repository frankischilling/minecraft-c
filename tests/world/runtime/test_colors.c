/* Prepared genuine World prefixes with real Info/Dimension/biome/Vec3 owners. */
#include "mc/world/runtime/colors.h"

#include "../../../src/entity/player/player_internal.h"
#include "../../../src/world/runtime/runtime_internal.h"
#include "mc/resources/translation/translation.h"
#include "mc/world/runtime/entities.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

static unsigned int checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "World color check%u at%d: %s\n", checks, __LINE__, #value);           \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct color_stream {
    const uint8_t *bytes;
    size_t length, offset;
} color_stream;

typedef struct fixture {
    mc_java_random_profile *profile;
    mc_entity_classes *entities;
    mc_registry_classes *registry;
    mc_materials *materials;
    mc_math_classes *math;
    mc_world_runtime_classes *classes;
    mc_world_runtime *world;
    mc_world_info *info;
    mc_world_provider *dimensions[3], *unbound;
    mc_entity *camera;
    color_stream streams[3];
    mc_registry_resources resources;
    size_t opens;
    bool bad;
    mc_entity_vector *old, *current;
} fixture;

static mc_java_properties_status read_resource(void *context, uint8_t *out, size_t capacity,
                                               size_t *count) {
    color_stream *stream = context;
    if (stream == NULL || out == NULL || count == NULL || capacity == 0)
        return MC_JAVA_PROPERTIES_IO_ERROR;
    size_t take = stream->length - stream->offset;
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
    fixture *f = context;
    static const char *const names[] = {"/lang/en_US.lang", "/lang/stats_US.lang",
                                        "/achievement/map.txt"};
    if (name == NULL || out == NULL || f->opens >= 3 || strcmp(name, names[f->opens]) != 0) {
        f->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    *out = (mc_java_properties_input){&f->streams[f->opens++], read_resource};
    return MC_REGISTRY_OK;
}

static mc_registry_status close_resource(void *context, const mc_java_properties_input *input) {
    fixture *f = context;
    if (input == NULL || input->read != read_resource || input->context != &f->streams[2]) {
        f->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    return MC_REGISTRY_OK;
}

static bool ready_resource(void *context, const mc_java_properties_input *input) {
    fixture *f = context;
    if (input == NULL || input->read != read_resource)
        f->bad = true;
    return false;
}

static bool setup(fixture *f) {
    static const uint8_t language[] = "stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                                      "stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n";
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entities) ==
          MC_ENTITY_OK);
    CHECK(mc_math_classes_create(SIZE_MAX, &f->math, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_math_classes_ensure(f->math, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_materials_create(&f->materials));
    for (size_t index = 0; index < 3; ++index)
        f->streams[index] = (color_stream){language, index == 0 ? sizeof(language) - 1u : 0u, 0};
    f->resources = (mc_registry_resources){f, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                         &f->resources};
    CHECK(mc_registry_classes_allocate(&options, &f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile,
                                          mc_math_classes_table(f->math), &f->classes,
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(f->classes, f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_bind_entities(f->classes, f->entities) == MC_ENTITY_OK);
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, f->classes, NULL,
                                          &f->world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_info_allocate(MC_WORLD_INFO_CLIENT, &f->info) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_construct(f->info, 23, mc_world_info_name_empty()) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_retain(f->info) == MC_WORLD_INFO_OK);
    f->world->fields.x = f->info;
    CHECK(mc_local_player_allocate(f->entities, &f->camera, NULL) == MC_ENTITY_OK);
    const int32_t ids[] = {0, -1, 1};
    const mc_world_provider_registry_binding binding = {f->registry, MC_REGISTRY_CLIENT,
                                                        f->profile};
    for (size_t index = 0; index < 3; ++index) {
        CHECK(mc_world_provider_create(ids[index], NULL, &f->dimensions[index], NULL) ==
              MC_WORLD_PROVIDER_OK);
        CHECK(mc_world_provider_bind_game(f->dimensions[index], f->world, &binding, NULL) ==
              MC_WORLD_PROVIDER_OK);
    }
    CHECK(mc_world_provider_create(0, NULL, &f->unbound, NULL) == MC_WORLD_PROVIDER_OK);
    return true;
}

static uint64_t bits(const double *value) {
    uint64_t raw;
    memcpy(&raw, value, sizeof(raw));
    return raw;
}

static bool select_dimension(fixture *f, size_t index) {
    CHECK(mc_world_provider_retain(f->dimensions[index]) == MC_WORLD_PROVIDER_OK);
    mc_world_provider *old = f->world->fields.t;
    f->world->fields.t = f->dimensions[index];
    mc_world_provider_release(old);
    return true;
}

static bool row(fixture *f, size_t dimension, size_t index, const char *kind) {
    mc_entity_vector_snapshot state;
    CHECK(f->current != NULL && mc_entity_vector_get_snapshot(f->entities, &state) == MC_ENTITY_OK);
    printf("COLOR %zu %zu %s %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %d\n", dimension, index,
           kind, bits(&f->current->x), bits(&f->current->y), bits(&f->current->z), state.cursor);
    CHECK(mc_entity_vector_release(f->entities, f->current) == MC_ENTITY_OK);
    f->current = NULL;
    return true;
}

static bool observations(fixture *f) {
    static const int64_t times[] = {-24001, -1, 0, 6000, 18000, INT64_MAX};
    static const float partial[] = {0.0f, 0.625f};
    static const int64_t cloud[] = {0xffffff, 0x804020, -1, INT64_C(0x123456789abcdef0)};
    static const float weather[][4] = {
        {0, 0, 0, 0}, {0.2f, 0.8f, 0.1f, 0.6f}, {1, 1, 1, 1}, {-0.25f, 0.75f, 1, 0.1f}};
    static const double positions[][2] = {
        {-1.25, 2.75}, {128.75, -16.25}, {-32000000.5, 17.5}, {32000000.25, -2048.5}};
    mc_world_color_error error;
    for (size_t dimension = 0; dimension < 3; ++dimension) {
        CHECK(select_dimension(f, dimension));
        for (size_t time = 0; time < sizeof(times) / sizeof(times[0]); ++time) {
            CHECK(mc_world_info_time_set(f->info, times[time]) == MC_WORLD_INFO_OK);
            for (size_t w = 0; w < 4; ++w) {
                f->world->fields.i = weather[w][0];
                f->world->fields.j = weather[w][1];
                f->world->fields.k = weather[w][2];
                f->world->fields.l = weather[w][3];
                f->world->fields.n = w == 2 ? 2 : w == 3 ? 1 : 0;
                f->world->fields.H = cloud[w];
                f->camera->base.x = positions[w][0];
                f->camera->base.z = positions[w][1];
                for (size_t p = 0; p < 2; ++p) {
                    const size_t index = time * 8u + w * 2u + p;
                    CHECK(mc_entity_vector_pool_reset(f->entities, NULL) == MC_ENTITY_OK);
                    CHECK(mc_world_runtime_sky_color(f->world, f->camera, partial[p], f->math,
                                                     &f->current, &error) == MC_WORLD_STORAGE_OK &&
                          error.site.bytecode == 424 && f->world->active_calls == 0);
                    if (f->old != NULL)
                        CHECK(f->old == f->current);
                    else {
                        CHECK(mc_entity_vector_retain(f->entities, f->current) == MC_ENTITY_OK);
                        f->old = f->current;
                    }
                    CHECK(row(f, dimension, index, "S"));
                    CHECK(mc_world_runtime_cloud_color(f->world, partial[p], f->math, &f->current,
                                                       &error) == MC_WORLD_STORAGE_OK &&
                          error.site.bytecode == 313);
                    CHECK(row(f, dimension, index, "C"));
                    CHECK(mc_world_runtime_fog_color(f->world, partial[p], f->math, &f->current,
                                                     &error) == MC_WORLD_STORAGE_OK &&
                          error.site.bytecode == 15);
                    CHECK(row(f, dimension, index, "F"));
                }
            }
        }
    }
    CHECK(!f->bad);
    return true;
}

static bool failures(fixture *f) {
    mc_world_color_error error;
    mc_entity_vector_snapshot before, after;
    CHECK(select_dimension(f, 0));
    CHECK(mc_entity_vector_get_snapshot(f->entities, &before) == MC_ENTITY_OK);
    mc_world_info *info = f->world->fields.x;
    mc_world_provider *dimension = f->world->fields.t;
    f->world->fields.x = NULL;
    f->world->fields.t = NULL;
    mc_world_storage_status status =
        mc_world_runtime_sky_color(f->world, NULL, 0.0f, f->math, &f->current, &error);
    f->world->fields.x = info;
    f->world->fields.t = dimension;
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && f->current == NULL &&
          !error.native_policy && error.site.bytecode == 2 && error.child.bytecode == 8);
    f->world->fields.t = NULL;
    status = mc_world_runtime_fog_color(f->world, 0.0f, f->math, &f->current, &error);
    f->world->fields.t = dimension;
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && f->current == NULL &&
          !error.native_policy && error.site.bytecode == 2 && error.child.bytecode == 12);
    CHECK(mc_world_runtime_sky_color(f->world, NULL, 0.0f, f->math, &f->current, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          !error.native_policy && error.site.bytecode == 43 && f->current == NULL);
    f->world->fields.t = f->unbound;
    status = mc_world_runtime_sky_color(f->world, f->camera, 0.0f, f->math, &f->current, &error);
    f->world->fields.t = dimension;
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && f->current == NULL &&
          !error.native_policy && error.site.bytecode == 68);
    const unsigned int csr = _mm_getcsr();
    _mm_setcsr(csr | 0x8000u);
    status = mc_world_runtime_cloud_color(f->world, 0.0f, f->math, &f->current, &error);
    _mm_setcsr(csr);
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && f->current == NULL &&
          error.native_policy);
    CHECK(mc_entity_vector_get_snapshot(f->entities, &after) == MC_ENTITY_OK &&
          before.cursor == after.cursor && before.size == after.size &&
          before.holds == after.holds && f->world->active_calls == 0 && f->camera->holds == 1);
    return true;
}

static bool close_fixture(fixture *f) {
    if (f->current != NULL)
        CHECK(mc_entity_vector_release(f->entities, f->current) == MC_ENTITY_OK);
    if (f->old != NULL)
        CHECK(mc_entity_vector_release(f->entities, f->old) == MC_ENTITY_OK);
    mc_entity_release(f->camera);
    for (size_t index = 0; index < 3; ++index)
        mc_world_provider_unbind(f->dimensions[index]);
    CHECK(mc_world_runtime_free(f->world, NULL) == MC_WORLD_STORAGE_OK);
    for (size_t index = 0; index < 3; ++index)
        mc_world_provider_release(f->dimensions[index]);
    mc_world_provider_release(f->unbound);
    mc_world_info_release(f->info);
    CHECK(mc_world_runtime_classes_free(f->classes, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_registry_classes_free(f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_entity_classes_free(f->entities) == MC_ENTITY_OK);
    CHECK(mc_math_classes_release(&f->math, NULL) == MC_MATH_CLASS_OK);
    mc_materials_destroy(f->materials);
    mc_java_random_profile_free(f->profile);
    return true;
}

int main(void) {
    fixture *f = calloc(1, sizeof(*f));
    if (f == NULL)
        return 1;
    const bool okay = setup(f) && observations(f) && failures(f);
    if (!close_fixture(f))
        return 1;
    free(f);
    if (!okay)
        return 1;
    printf("%u World color checks passed\n", checks);
    return 0;
}
