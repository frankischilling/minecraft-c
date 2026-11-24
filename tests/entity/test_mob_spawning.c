/* Genuine GAME World/Chunk/Mob spawn controls; finite source inputs. */
#include "core/java/random_profile_internal.h"
#include "entity/entity_internal.h"
#include "entity/mob/internal.h"
#include "entity/player/player_internal.h"
#include "mc/entity/appearance.h"
#include "mc/entity/mob/spawning.h"
#include "mc/resources/files/files.h"
#include "mc/statistics/registry/registry.h"
#include "mc/world/dimension/world_provider.h"
#include "mc/world/runtime/entities.h"
#include "world/chunk/object_state_internal.h"
#include "world/runtime/chunk_provider_internal.h"
#include "world/runtime/construction_internal.h"
#include "world/runtime/runtime_internal.h"
#include "world/storage/objects_internal.h"
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned int checks;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "Mob spawning check%u line%d: %s\n", checks, __LINE__, #c);            \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct spawn_fixture {
    mc_resource_files *files;
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_entity_classes *entities;
    mc_registry_classes *registry;
    mc_world_runtime_classes *world_classes;
    mc_world_regions *regions;
    mc_world_store *store;
    mc_world_provider *dimension;
    mc_world_runtime *world;
    mc_world_info_name *name;
    mc_player_session_classes *sessions;
    mc_math_classes *math;
    mc_chunk_provider_fields provider;
    mc_chunk_state *normal;
    mc_world_chunk *chunk;
} spawn_fixture;

static bool spawn_setup(spawn_fixture *f, const char *resources, const char *directory,
                        int64_t seed, size_t chunk_limit) {
    CHECK(mc_resource_files_create(resources, &f->files) == MC_REGISTRY_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entities) ==
          MC_ENTITY_OK);
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                         mc_resource_files_source(f->files)};
    CHECK(mc_registry_classes_allocate(&options, &f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_initialize_statistics(f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_math_classes_create(SIZE_MAX, &f->math, NULL) == MC_MATH_CLASS_OK &&
          mc_math_classes_ensure(f->math, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile,
                                          mc_math_classes_table(f->math), &f->world_classes,
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(f->world_classes, f->registry, NULL) ==
          MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_bind_entities(f->world_classes, f->entities) == MC_ENTITY_OK);
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    uint8_t map[256] = {0};
    mc_registry_block_view blocks;
    CHECK(mc_registry_classes_inspect_blocks(f->registry, &blocks, NULL) == MC_REGISTRY_OK);
    for (size_t i = 1; i < 256; ++i)
        map[i] = blocks.m[i] == NULL ? 0 : (uint8_t)i;
    mc_world_store_options store = {.session_token = 6381,
                                    .block_map = map,
                                    .block_map_length = 256,
                                    .limits = mc_world_store_default_limits()};
    store.limits.max_loaded_chunks = chunk_limit;
    CHECK(mc_world_store_open(directory, f->regions, &store, &f->store, NULL) ==
          MC_WORLD_STORAGE_OK);
    static const uint16_t name[] = {'P', 'l', 'a', 'y', 'e', 'r'};
    CHECK(mc_world_info_name_copy((mc_nbt_string){name, 6}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_world_provider_create(0, NULL, &f->dimension, NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 8;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, f->world_classes,
                                          &limits, &f->world, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments args = {
        .store = f->store, .name = f->name, .seed = seed, .dimension = f->dimension};
    mc_world_storage_error error;
    const mc_world_storage_status status =
        mc_world_runtime_construct_world(f->world, &args, &error);
    if (status != MC_WORLD_STORAGE_OK)
        fprintf(stderr, "World: %d %s\n", (int)status, error.operation);
    CHECK(status == MC_WORLD_STORAGE_OK);
    CHECK(mc_player_session_classes_create(f->entities, f->registry, &f->sessions, NULL) ==
          MC_PLAYER_OK);
    CHECK(mc_chunk_provider_bind_fields(f->world->fields.v, &f->provider, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_get(f->world, 0, 0, &f->normal) == MC_CHUNK_ACQUIRE_OK && f->normal &&
          !f->normal->blank);
    f->chunk = mc_world_store_chunk_object(f->store, f->normal);
    CHECK(f->chunk != NULL);
    return true;
}

static bool spawn_teardown(spawn_fixture *f) {
    CHECK(mc_player_session_classes_free(&f->sessions, NULL) == MC_PLAYER_OK);
    CHECK(mc_chunk_pending_clear(f->provider.pending) == MC_CHUNK_ACQUIRE_OK);
    CHECK(mc_chunk_cache_init(f->provider.cache, f->provider.cache->entries,
                              f->provider.cache->capacity, f->provider.cache->insertions,
                              f->provider.cache->insertion_capacity));
    mc_world_provider_unbind(f->dimension);
    CHECK(mc_world_runtime_retire_world(f->world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_retire_chunk(f->store, f->normal, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_chunk_count(f->store) == 0);
    CHECK(mc_world_runtime_free(f->world, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_provider_release(f->dimension);
    CHECK(mc_world_store_free_checked(&f->store, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_regions_free(f->regions);
    mc_world_info_name_release(f->name);
    CHECK(mc_world_runtime_classes_free(f->world_classes, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_math_classes_release(&f->math, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_registry_classes_free(f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_entity_classes_free(f->entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(f->profile);
    mc_materials_destroy(f->materials);
    mc_resource_files_free(f->files);
    return true;
}

static FILE *output;
static const mc_entity_class_kind hostile_kinds[11] = {
    MC_ENTITY_CLASS_KIND_SPIDER,  MC_ENTITY_CLASS_KIND_ZOMBIE, MC_ENTITY_CLASS_KIND_SKELETON,
    MC_ENTITY_CLASS_KIND_CREEPER, MC_ENTITY_CLASS_KIND_SLIME,  MC_ENTITY_CLASS_KIND_PIG,
    MC_ENTITY_CLASS_KIND_SHEEP,   MC_ENTITY_CLASS_KIND_COW,    MC_ENTITY_CLASS_KIND_CHICKEN,
    MC_ENTITY_CLASS_KIND_WOLF,    MC_ENTITY_CLASS_KIND_SQUID};
static const char *const hostile_names[11] = {"cn", "uz", "fr", "gb", "uw", "wh",
                                              "dl", "bx", "ww", "gi", "xt"};

static bool spawn_actor(spawn_fixture *f, size_t kind, mc_entity **out) {
    const mc_entity_class_literal *literal = NULL;
    mc_mob_error error;
    CHECK(mc_entity_class_literal_get(f->entities, hostile_kinds[kind], &literal) == MC_ENTITY_OK);
    CHECK(mc_mob_allocate(f->entities, f->registry, literal, out, &error) == MC_MOB_OK);
    CHECK(mc_mob_construct(*out, f->world, &error) == MC_MOB_OK);
    return true;
}

static bool emit_state(spawn_fixture *f, mc_entity *actor, const char *prefix, unsigned int index,
                       size_t kind, unsigned int scene, mc_world_storage_status status,
                       bool eligible) {
    mc_entity_class_snapshot classes;
    mc_entity_vector_snapshot vectors;
    CHECK(mc_entity_classes_get_snapshot(f->entities, &classes) == MC_ENTITY_OK);
    CHECK(mc_entity_vector_get_snapshot(f->entities, &vectors) == MC_ENTITY_OK);
    CHECK(f->world->fields.R && f->world->fields.K);
    fprintf(output, "%s %u %s %u %d %012" PRIx64 " %012" PRIx64 " %d %d %d %zu %zu %zu\n", prefix,
            index, hostile_names[kind], scene,
            status == MC_WORLD_STORAGE_OK ? eligible ? 1 : 0 : -1, actor->random->state,
            f->world->fields.r->state, f->world->fields.f, classes.bounds_pool_cursor,
            vectors.cursor, f->world->fields.R->count, f->world->fields.K->count,
            f->provider.cache->count);
    return true;
}

static bool eligibility_case(const char *resources, const char *directory, unsigned int index) {
    const bool slime = index >= 44;
    const size_t kind = slime ? 4u : index / 11u;
    const unsigned int scene = slime ? index == 58 ? 12u : index - 44u : index % 11u;
    const int64_t seed = slime ? scene == 2    ? 23
                                 : scene == 12 ? INT64_C(186642153918107)
                                               : INT64_C(987234911)
                               : 23;
    spawn_fixture f = {0};
    CHECK(spawn_setup(&f, resources, directory, seed, 512));
    memset(f.normal->data->blocks, 0, MC_CHUNK_BLOCKS);
    memset(f.normal->data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(f.normal->data->skylight, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(f.normal->data->block_light, 0, MC_CHUNK_NIBBLE_BYTES);
    f.world->fields.B = false;
    f.world->fields.f = 0;
    f.world->fields.q = slime && scene <= 3 ? 0 : 1;
    f.world->fields.i = f.world->fields.j = f.world->fields.k = f.world->fields.l = 0;
    mc_java_random_reseed(f.world->fields.r, 89);
    mc_entity *actor = NULL, *other = NULL;
    CHECK(spawn_actor(&f, kind, &actor));
    if (slime) {
        const int32_t size = scene == 0 || scene == 4 || scene == 5 || scene == 6 ? 2
                             : scene == 7                                         ? -1
                                                                                  : 1;
        mc_mob_error error;
        CHECK(mc_mob_slime_set_size(actor, size, &error) == MC_MOB_OK);
    }
    CHECK(mc_entity_set_position(actor, slime && scene == 9 ? 1048584.5 : 8.5,
                                 slime && scene == 5   ? 16.
                                 : slime && scene == 6 ? NAN
                                                       : 8.5,
                                 8.5) == MC_ENTITY_OK);
    if (!slime) {
        if (scene == 0 || scene == 8 || scene == 9)
            CHECK(
                mc_chunk_nibbles_set(f.normal->data->skylight, MC_CHUNK_NIBBLE_BYTES, 8, 8, 8, 15));
        if (scene == 1)
            CHECK(mc_chunk_nibbles_set(f.normal->data->block_light, MC_CHUNK_NIBBLE_BYTES, 8, 8, 8,
                                       15));
        if (scene == 3 || scene == 4)
            f.normal->data->blocks[8u * 2048u + 8u * 128u + 8u] = scene == 3 ? 1 : 9;
        if (scene >= 5 && scene <= 7) {
            CHECK(spawn_actor(&f, 0, &other));
            CHECK(mc_entity_set_position(other, 8.5, 8.5, 8.5) == MC_ENTITY_OK);
            other->base.be = scene == 6;
            other->base.aF = scene != 7;
            other->base.bq = scene == 7;
            CHECK(mc_world_runtime_chunk_add_entity(f.chunk, other, NULL) == MC_WORLD_STORAGE_OK);
            if (scene == 7)
                CHECK(mc_world_runtime_chunk_add_entity(f.chunk, other, NULL) ==
                      MC_WORLD_STORAGE_OK);
        }
        if (scene == 8 || scene == 9) {
            f.world->fields.f = scene == 8 ? 11 : 0;
            f.world->fields.i = f.world->fields.j = f.world->fields.k = f.world->fields.l = 1;
        }
        if (scene == 10)
            CHECK(mc_world_runtime_chunk_add_entity(f.chunk, actor, NULL) == MC_WORLD_STORAGE_OK);
    }
    mc_java_random_reseed(actor->random, !slime && scene == 0 ? 4096 : slime && scene == 1 ? 1 : 0);
    CHECK(mc_entity_bounds_pool_reset(f.entities, NULL) == MC_ENTITY_OK);
    CHECK(mc_entity_vector_pool_reset(f.entities, NULL) == MC_ENTITY_OK);
    const uint64_t shared_u = atomic_load(&f.profile->uniquifier);
    const unsigned int math_phase = atomic_load(&f.profile->holder_phase);
    const uint64_t math_seed = f.profile->math ? atomic_load(&f.profile->math->state) : 0;
    const size_t leases = f.world->local_leases;
    mc_world_info *info = f.world->fields.x;
    if (slime && scene == 10)
        actor->base.world = NULL;
    if (slime && scene == 11)
        f.world->fields.x = NULL;
    bool eligible = true;
    mc_mob_spawn_state state;
    mc_mob_spawn_error error;
    const mc_world_storage_status status =
        mc_mob_spawn_eligible(actor, f.math, f.world_classes, &eligible, &state, &error);
    if (slime && scene == 10)
        actor->base.world = f.world;
    if (slime && scene == 11)
        f.world->fields.x = info;
    CHECK(atomic_load(&f.profile->uniquifier) == shared_u &&
          atomic_load(&f.profile->holder_phase) == math_phase &&
          (!f.profile->math || atomic_load(&f.profile->math->state) == math_seed));
    CHECK(f.world->local_leases == leases && f.world->active_calls == 0);
    if (slime && (scene == 10 || scene == 11))
        CHECK(status != MC_WORLD_STORAGE_OK && eligible && !state.returned &&
              error.source_exception);
    else {
        if (status != MC_WORLD_STORAGE_OK)
            fprintf(stderr, "Spawn case%u %s.%s:%d %s\n", index, error.site.class_name,
                    error.site.method, error.site.pc, error.operation);
        CHECK(status == MC_WORLD_STORAGE_OK && state.returned && state.eligible == eligible);
    }
    if (!slime && (scene == 8 || scene == 9))
        CHECK(state.temporary_subtraction_stored && state.subtraction_restored &&
              f.world->fields.f == (scene == 8 ? 11 : 0));
    if (slime && scene == 0)
        CHECK(state.chunk_reached && state.own_random_calls == 0);
    if (slime && scene == 9)
        CHECK(state.chunk_reached && state.chunk.x == 0 && state.chunk.z == 0);
    CHECK(emit_state(&f, actor, "ELIG_STATE", index, kind, scene, status, eligible));
    if (status != MC_WORLD_STORAGE_OK)
        fprintf(output, "SPAWN_FAILURE %u %s %s %d %s %s %d\n", index, error.site.class_name,
                error.site.method, error.site.pc,
                error.child.class_name ? error.child.class_name : "-",
                error.child.method ? error.child.method : "-", error.child.pc);
    CHECK(mc_world_runtime_retire_entity_containers(f.world, NULL) == MC_WORLD_STORAGE_OK);
    mc_mob_error retirement;
    CHECK(mc_mob_retire(&actor, &retirement) == MC_MOB_OK);
    CHECK(mc_mob_retire(&other, &retirement) == MC_MOB_OK);
    CHECK(spawn_teardown(&f));
    return true;
}

static bool passive_case(const char *resources, const char *directory, unsigned int index,
                         bool water) {
    const size_t kind = water ? 10u : 5u + index / 10u;
    const unsigned int scene = water ? index : index % 10u;
    spawn_fixture f = {0};
    CHECK(spawn_setup(&f, resources, directory, 23, 512));
    memset(f.normal->data->blocks, 0, MC_CHUNK_BLOCKS);
    memset(f.normal->data->skylight, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(f.normal->data->block_light, 0, MC_CHUNK_NIBBLE_BYTES);
    f.world->fields.B = false;
    f.world->fields.f = !water && scene == 9 ? 15 : 0;
    f.world->fields.q = 1;
    f.world->fields.i = f.world->fields.j = f.world->fields.k = f.world->fields.l = 0;
    mc_java_random_reseed(f.world->fields.r, 89);
    mc_entity *actor = NULL, *other = NULL;
    CHECK(spawn_actor(&f, kind, &actor));
    CHECK(mc_entity_set_position(actor, 8.5, water && scene == 8 ? 200.5 : 8.5, 8.5) ==
          MC_ENTITY_OK);
    if (water) {
        if (scene >= 1 && scene <= 3)
            f.normal->data->blocks[8u * 2048u + 8u * 128u + 8u] = scene == 1   ? 1
                                                                  : scene == 2 ? 9
                                                                               : 11;
    } else {
        if (scene != 0)
            f.normal->data->blocks[8u * 2048u + 8u * 128u + 7u] = 2;
        const uint8_t sky = scene == 1 ? 8 : scene == 2 || scene == 9 ? 9 : scene == 8 ? 0 : 15;
        CHECK(mc_chunk_nibbles_set(f.normal->data->skylight, MC_CHUNK_NIBBLE_BYTES, 8, 8, 8, sky));
        if (scene == 8)
            CHECK(mc_chunk_nibbles_set(f.normal->data->block_light, MC_CHUNK_NIBBLE_BYTES, 8, 8, 8,
                                       9));
        if (scene == 3 || scene == 4)
            f.normal->data->blocks[8u * 2048u + 8u * 128u + 8u] = scene == 3 ? 1 : 9;
    }
    const unsigned int nearby_start = water ? 4u : 5u;
    if (scene >= nearby_start && scene <= nearby_start + 2u) {
        CHECK(spawn_actor(&f, 0, &other));
        CHECK(mc_entity_set_position(other, 8.5, 8.5, 8.5) == MC_ENTITY_OK);
        other->base.be = scene == nearby_start + 1u;
        other->base.aF = scene != nearby_start + 2u;
        other->base.bq = scene == nearby_start + 2u;
        CHECK(mc_world_runtime_chunk_add_entity(f.chunk, other, NULL) == MC_WORLD_STORAGE_OK);
        if (scene == nearby_start + 2u)
            CHECK(mc_world_runtime_chunk_add_entity(f.chunk, other, NULL) == MC_WORLD_STORAGE_OK);
    }
    if (water && scene == 7)
        CHECK(mc_world_runtime_chunk_add_entity(f.chunk, actor, NULL) == MC_WORLD_STORAGE_OK);
    mc_java_random_reseed(actor->random, 0);
    CHECK(mc_entity_bounds_pool_reset(f.entities, NULL) == MC_ENTITY_OK);
    CHECK(mc_entity_vector_pool_reset(f.entities, NULL) == MC_ENTITY_OK);
    const uint64_t u = atomic_load(&f.profile->uniquifier);
    const uint64_t math = atomic_load(&f.profile->math->state);
    const uint64_t own = actor->random->state;
    const size_t leases = f.world->local_leases;
    bool eligible = false;
    mc_mob_spawn_state state;
    mc_mob_spawn_error error;
    const mc_world_storage_status status =
        mc_mob_spawn_eligible(actor, f.math, f.world_classes, &eligible, &state, &error);
    if (status != MC_WORLD_STORAGE_OK)
        fprintf(stderr, "Passive %s%u: %s.%s:%d %s\n", water ? "water" : "animal", index,
                error.site.class_name, error.site.method, error.site.pc, error.operation);
    CHECK(status == MC_WORLD_STORAGE_OK && state.returned && state.eligible == eligible);
    CHECK(actor->random->state == own && state.own_random_calls == 0 &&
          state.chunk_random_calls == 0 && atomic_load(&f.profile->uniquifier) == u &&
          atomic_load(&f.profile->math->state) == math && f.world->local_leases == leases &&
          f.world->active_calls == 0);
    if (water)
        CHECK(!state.living_reached && !state.creature_reached && !state.path_weight_reached &&
              !state.saved_sky_reached && !state.light_reached);
    else if (scene == 0 || scene == 1)
        CHECK(!state.living_reached);
    CHECK(emit_state(&f, actor, water ? "WATER_STATE" : "ANIMAL_STATE", index, kind, scene, status,
                     eligible));
    CHECK(mc_world_runtime_retire_entity_containers(f.world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_mob_retire(&actor, NULL) == MC_MOB_OK && mc_mob_retire(&other, NULL) == MC_MOB_OK);
    CHECK(spawn_teardown(&f));
    return true;
}

static bool chunk_row(spawn_fixture *f, mc_chunk_state *chunk, unsigned int index, bool blank,
                      int32_t x, int32_t z, int64_t salt) {
    const uint64_t u = atomic_load(&f->profile->uniquifier);
    const unsigned int phase = atomic_load(&f->profile->holder_phase);
    const size_t leases = f->world->local_leases;
    mc_java_random random;
    mc_world_chunk_random_state state;
    mc_world_chunk_random_error error;
    CHECK(mc_world_runtime_chunk_random(f->world, chunk, salt, &random, &state, &error) ==
              MC_WORLD_STORAGE_OK &&
          state.returned && state.blank == blank);
    CHECK(state.x == x && state.z == z && state.source_world == f->world);
    const uint64_t before = random.state;
    int32_t draw;
    CHECK(mc_java_random_next_bounded(&random, 10, &draw));
    fprintf(output,
            "CHUNK_RANDOM %u %u %d %d %016" PRIx64 " %016" PRIx64 " %012" PRIx64 " %d %012" PRIx64
            "\n",
            index, blank ? 1u : 0u, x, z, (uint64_t)salt, (uint64_t)state.world_seed, before, draw,
            random.state);
    if (index >= 28) {
        mc_java_random raw = {.state = before};
        const uint32_t first = (uint32_t)mc_java_random_next_i32(&raw);
        (void)mc_java_random_next_i32(&raw);
        CHECK((first >> 1) == INT32_MAX && draw == 9 && random.state == raw.state);
    }
    CHECK(f->world->local_leases == leases && f->world->active_calls == 0 &&
          atomic_load(&f->profile->uniquifier) == u &&
          atomic_load(&f->profile->holder_phase) == phase);
    return true;
}

static bool chunk_cases(const char *resources, const char *directory, bool rejection) {
    spawn_fixture f = {0};
    CHECK(spawn_setup(&f, resources, directory, rejection ? INT64_C(186643064605892) : 23, 512));

    static const struct {
        int32_t x, z;
    } points[] = {{0, 0},         {1, -1}, {-2049, 4097}, {46341, -46341}, {INT32_MAX, INT32_MIN},
                  {65536, 65536}, {-1, -1}};

    const size_t count = rejection ? 1u : sizeof(points) / sizeof(points[0]);
    for (size_t point = 0; point < count; ++point) {
        mc_chunk_state *chunk = f.normal;
        if (point)
            CHECK(mc_world_store_load_game_chunk(f.store, f.world, 0, points[point].x,
                                                 points[point].z, &chunk,
                                                 NULL) == MC_WORLD_STORAGE_OK &&
                  chunk);
        CHECK(chunk_row(&f, chunk, rejection ? 28u : (unsigned int)(point * 2u), false,
                        points[point].x, points[point].z, 0));
        if (!rejection)
            CHECK(chunk_row(&f, chunk, (unsigned int)(point * 2u + 1u), false, points[point].x,
                            points[point].z, INT64_C(987234911)));
        if (point)
            CHECK(mc_world_store_retire_chunk(f.store, chunk, NULL) == MC_WORLD_STORAGE_OK);
    }
    CHECK(chunk_row(&f, &f.world->fallback, rejection ? 29u : 14u, true, 0, 0, 0));
    if (!rejection)
        CHECK(chunk_row(&f, &f.world->fallback, 15u, true, 0, 0, INT64_C(987234911)));
    CHECK(mc_world_runtime_retire_entity_containers(f.world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(spawn_teardown(&f));
    return true;
}

static bool failure_controls(const char *resources, const char *directory, bool chunk_limit) {
    spawn_fixture f = {0};
    CHECK(spawn_setup(&f, resources, directory, 23, chunk_limit ? 1u : 512u));
    mc_entity *actor = NULL;
    CHECK(spawn_actor(&f, chunk_limit ? 4u : 0u, &actor));
    CHECK(mc_entity_set_position(actor, chunk_limit ? 24.5 : 8.5, 8.5, 8.5) == MC_ENTITY_OK);
    mc_java_random_reseed(actor->random, 0);
    const uint64_t own = actor->random->state;
    const uint64_t u = atomic_load(&f.profile->uniquifier);
    const uint64_t math = atomic_load(&f.profile->math->state);
    const size_t leases = f.world->local_leases;
    mc_mob_spawn_state state;
    mc_mob_spawn_error error;
    bool out = true;
    if (chunk_limit) {
        mc_mob_error size_error;
        CHECK(mc_mob_slime_set_size(actor, 2, &size_error) == MC_MOB_OK);
        f.world->fields.q = 0;
        CHECK(mc_mob_spawn_eligible(actor, f.math, f.world_classes, &out, &state, &error) !=
                  MC_WORLD_STORAGE_OK &&
              error.native_policy && !state.returned && out && state.chunk_reached &&
              state.own_random_calls == 0);
        CHECK(f.provider.cache->count == 1 && mc_world_store_chunk_count(f.store) == 1 &&
              actor->random->state == own);
    } else {
        mc_world_chunk_nibble *sky = f.chunk->f;
        f.chunk->f = NULL;
        const mc_world_storage_status failed =
            mc_mob_spawn_eligible(actor, f.math, f.world_classes, &out, &state, &error);
        f.chunk->f = sky;
        CHECK(failed != MC_WORLD_STORAGE_OK && out && !state.returned &&
              state.own_random_calls == 0 && actor->random->state == own);
        actor->base.world = NULL;
        const mc_world_storage_status null_world =
            mc_mob_spawn_eligible(actor, f.math, f.world_classes, &out, &state, &error);
        actor->base.world = f.world;
        CHECK(null_world != MC_WORLD_STORAGE_OK && error.source_exception && error.site.pc == 37 &&
              !strcmp(error.site.class_name, "gz") && state.own_random_calls == 0 && out);
        memset(f.normal->data->skylight, 0, MC_CHUNK_NIBBLE_BYTES);
        memset(f.normal->data->block_light, 0, MC_CHUNK_NIBBLE_BYTES);
        CHECK(mc_mob_spawn_eligible(actor, f.math, f.world_classes, &out, &state, &error) ==
                  MC_WORLD_STORAGE_OK &&
              state.returned && out);
        for (unsigned int mode = 0; mode < 3; ++mode) {
            mc_java_random random = {.state = UINT64_C(0x123456789abc)}, before = random;
            mc_world_chunk_random_state chunk_state;
            mc_world_chunk_random_error chunk_error;
            mc_world_info *info = f.world->fields.x;
            if (mode == 0)
                f.chunk->d = NULL;
            else
                f.world->fields.x = NULL;
            const mc_world_storage_status status =
                mc_world_runtime_chunk_random(f.world, mode == 2 ? &f.world->fallback : f.normal, 0,
                                              &random, &chunk_state, &chunk_error);
            f.chunk->d = f.world;
            f.world->fields.x = info;
            CHECK(status != MC_WORLD_STORAGE_OK && chunk_error.source_exception &&
                  !chunk_error.native_policy && !chunk_state.returned && !chunk_state.constructed &&
                  random.state == before.state && chunk_error.site.pc == 8 &&
                  (mode == 0 || chunk_error.child.pc == 4));
            CHECK(!strcmp(chunk_error.exception_class, "java/lang/NullPointerException") &&
                  !strcmp(chunk_error.site.class_name, mode == 2 ? "li" : "lm") &&
                  (mode == 0 ? chunk_error.child.class_name == NULL
                             : chunk_error.child.class_name &&
                                   !strcmp(chunk_error.child.class_name, "fd")));
            fprintf(output, "CHUNK_FAILURE %u %s %d %s %d\n", mode == 2 ? 3u : mode,
                    chunk_error.site.class_name, chunk_error.site.pc,
                    chunk_error.child.class_name ? chunk_error.child.class_name : "-",
                    chunk_error.child.pc);
        }
        static const uint32_t words[3] = {UINT32_C(0x3f666666), UINT32_C(0x3f666667),
                                          UINT32_C(0x7fc00000)};
        for (unsigned int index = 0; index < 3; ++index) {
            float strength;
            memcpy(&strength, &words[index], sizeof(strength));
            f.world->fields.i = f.world->fields.j = 1;
            f.world->fields.k = f.world->fields.l = strength;
            f.world->fields.f = 11;
            mc_java_random_reseed(actor->random, 0);
            CHECK(mc_mob_spawn_eligible(actor, f.math, f.world_classes, &out, &state, &error) ==
                      MC_WORLD_STORAGE_OK &&
                  state.returned && state.thunder_reached && state.thundering == (index == 1));
            uint32_t actual;
            memcpy(&actual, &state.thunder_strength, sizeof(actual));
            CHECK(actual == words[index] && f.world->fields.f == 11 &&
                  state.temporary_subtraction_stored == (index == 1) &&
                  state.subtraction_restored == (index == 1));
            fprintf(output, "THUNDER %u %08" PRIx32 " %u\n", index, actual,
                    state.thundering ? 1u : 0u);
        }
        bool inserted = false;
        mc_world_entity_error publication;
        CHECK(mc_world_runtime_insert_entity(f.world, actor, &inserted, &publication) ==
                  MC_WORLD_STORAGE_OK &&
              inserted);
        mc_mob_error retirement;
        CHECK(mc_mob_retire(&actor, &retirement) == MC_MOB_BUSY && actor != NULL);
    }
    CHECK(f.world->local_leases == leases && f.world->active_calls == 0 &&
          atomic_load(&f.profile->uniquifier) == u && atomic_load(&f.profile->math->state) == math);
    CHECK(mc_world_runtime_retire_entity_containers(f.world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_mob_retire(&actor, NULL) == MC_MOB_OK);
    CHECK(spawn_teardown(&f));
    fprintf(output, "SPAWN_CONTROL %s\n",
            chunk_limit ? "chunk-budget-before-size-gates"
                        : "source-prefix-retry-null-leaf-publication-ownership");
    return true;
}

static FILE *spawn_open(const char *path) {
#ifdef _MSC_VER
    FILE *file = NULL;
    return fopen_s(&file, path, "wb") == 0 ? file : NULL;
#else
    return fopen(path, "wb");
#endif
}

int main(int argc, char **argv) {
    if (argc != 4)
        return 2;
    output = spawn_open(argv[3]);
    if (!output)
        return 2;
    char directory[8192];
    for (unsigned int index = 0; index <= 58; ++index) {
        if (index == 52 || index == 56 || index == 57)
            continue;
        const int n = snprintf(directory, sizeof(directory), "%s/case%u", argv[2], index);
        if (n < 0 || (size_t)n >= sizeof(directory) || !eligibility_case(argv[1], directory, index))
            return 1;
    }
    for (unsigned int special = 0; special < 2; ++special) {
        const int n = snprintf(directory, sizeof(directory), "%s/chunks%u", argv[2], special);
        if (n < 0 || (size_t)n >= sizeof(directory) ||
            !chunk_cases(argv[1], directory, special != 0))
            return 1;
    }
    for (unsigned int index = 0; index < 50; ++index) {
        const int n = snprintf(directory, sizeof(directory), "%s/animal%u", argv[2], index);
        if (n < 0 || (size_t)n >= sizeof(directory) ||
            !passive_case(argv[1], directory, index, false))
            return 1;
    }
    for (unsigned int index = 0; index < 9; ++index) {
        const int n = snprintf(directory, sizeof(directory), "%s/water%u", argv[2], index);
        if (n < 0 || (size_t)n >= sizeof(directory) ||
            !passive_case(argv[1], directory, index, true))
            return 1;
    }
    for (unsigned int limit = 0; limit < 2; ++limit) {
        const int n = snprintf(directory, sizeof(directory), "%s/control%u", argv[2], limit);
        if (n < 0 || (size_t)n >= sizeof(directory) ||
            !failure_controls(argv[1], directory, limit != 0))
            return 1;
    }
    if (fclose(output) != 0)
        return 1;
    printf("Mob spawning: %u checks; 115 genuine predicates and 18 exact Chunk leaves\n", checks);
    return 0;
}
