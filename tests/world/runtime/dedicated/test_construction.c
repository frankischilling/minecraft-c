#include "../../../../src/core/java/random_profile_internal.h"
#include "../../../../src/entity/entity_internal.h"
#include "../../../../src/world/chunk/object_state_internal.h"
#include "../../../../src/world/runtime/chunk_provider_internal.h"
#include "../../../../src/world/runtime/construction_internal.h"
#include "../../../../src/world/runtime/dedicated/entity_index.h"
#include "../../../../src/world/runtime/entity_publication_internal.h"
#include "mc/world/runtime/entities.h"

#include <stdio.h>
#include <string.h>

/* Constructed subclass/ownership controls. RAW Entity IDs and a typed List are
 * explicit native fixture inputs; no original output or default Random state
 * is supplied. Natural population/unload comparison is a separate profile. */
static unsigned int checks;
static mc_math_trig trig;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static int one_case(mc_world_runtime_classes *classes, mc_entity_classes *entities,
                    mc_server_runtime *server, mc_world_store *store, mc_world_info_name *name,
                    unsigned int scenario) {
    int result = 1;
    mc_world_runtime *world = NULL;
    mc_world_provider *ignored_dimension = NULL;
    mc_entity *a = NULL, *b = NULL, *looked_up = NULL;
    mc_world_runtime_construction base;
    mc_world_runtime_dedicated_fields f;
    mc_world_runtime_dedicated_construction state;
    mc_chunk_provider_fields provider;
    mc_world_entity_index_view index;
    mc_world_entity_batch_error detail;
    mc_server_construction server_state;
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 0;
    if (scenario == 1)
        limits.max_dedicated_allocation_bytes = 0;
    if (scenario == 2)
        limits.max_dedicated_entities = 1;
    if (scenario == 7)
        limits.max_direct_allocation_bytes = sizeof(*world);
    CHECK(mc_world_provider_create(1, NULL, &ignored_dimension, NULL) == MC_WORLD_PROVIDER_OK);
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_SERVER_DEDICATED, classes, &limits,
                                          &world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_dedicated_get(world, &f, &state) == MC_WORLD_STORAGE_OK && f.C == NULL &&
          !f.D && !f.E && f.F == NULL && f.G == NULL && state.last_store_pc == 0 &&
          state.failed_call_pc == 0);
    const mc_world_runtime_world_arguments arguments = {
        .store = store,
        .name = name,
        .seed = -99,
        .dimension = ignored_dimension, /* The dp constructor uses its int factory argument. */
        .server = scenario == 5 ? NULL : server,
        .dimension_id = scenario == 4   ? 1
                        : scenario == 6 ? 2
                                        : 0};
    const mc_world_storage_status constructed =
        mc_world_runtime_construct_world(world, &arguments, NULL);
    CHECK(mc_world_runtime_construction_get(world, &base) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_dedicated_get(world, &f, &state) == MC_WORLD_STORAGE_OK);
    CHECK(mc_server_runtime_inspect(server, NULL, &server_state) == MC_SERVER_OK);
    if (scenario == 7) {
        CHECK(constructed == MC_WORLD_STORAGE_LIMIT &&
              base.phase == MC_WORLD_RUNTIME_FAILED_CONSTRUCTION && base.last_store_pc == 6 &&
              base.failed_call_pc == 14 && state.failed_call_pc == 10 && f.C == NULL &&
              f.G == NULL && f.F == NULL && world->fields.t == NULL &&
              server_state.references == 1);
        result = 0;
        goto cleanup;
    }
    mc_world_provider_state dimension;
    CHECK(world->fields.t != ignored_dimension &&
          mc_world_provider_state_get(world->fields.t, &dimension) == MC_WORLD_PROVIDER_OK);
    if (scenario == 4) {
        CHECK(constructed == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              base.phase == MC_WORLD_RUNTIME_FAILED_CONSTRUCTION && base.failed_call_pc == 403 &&
              dimension.kind == MC_WORLD_PROVIDER_SKY && state.failed_call_pc == 10 &&
              f.C == NULL && f.G == NULL && f.F == NULL && server_state.references == 1);
        result = 0;
        goto cleanup;
    }
    CHECK(dimension.kind == MC_WORLD_PROVIDER_OVERWORLD && f.C != NULL && f.C == world->fields.v);
    CHECK(mc_chunk_provider_bind_fields(f.C, &provider, NULL) == MC_WORLD_STORAGE_OK &&
          provider.world == world && provider.policy == MC_CHUNK_ACQUIRE_DEDICATED &&
          provider.force_load == world->live.provider_force_load && !*provider.force_load &&
          world->options.policy == MC_CHUNK_ACQUIRE_DEDICATED &&
          world->live.saving_disabled == &world->dedicated.E &&
          world->fields.E != (void *)world->live.saving_disabled && !f.E);
    CHECK(base.contexts_bound && base.cache_registered && provider.cache->count == 0 &&
          provider.pending->count == 0);
    if (scenario == 1) {
        CHECK(constructed == MC_WORLD_STORAGE_LIMIT &&
              base.phase == MC_WORLD_RUNTIME_FAILED_CONSTRUCTION && state.last_store_pc == 15 &&
              state.failed_call_pc == 23 && f.G == NULL && f.F == NULL &&
              server_state.references == 1);
        CHECK(mc_world_runtime_construct_world(world, &arguments, NULL) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE);
        result = 0;
        goto cleanup;
    }
    CHECK(constructed == MC_WORLD_STORAGE_OK && base.phase == MC_WORLD_RUNTIME_READY &&
          state.last_store_pc == 31 && state.failed_call_pc == 0 && !f.D && f.G != NULL &&
          f.F == arguments.server && server_state.references == (scenario == 5 ? 1u : 2u));
    mc_scheduled_ordered *scheduled = world->fields.E;
    *world->live.saving_disabled = true;
    CHECK(mc_world_runtime_dedicated_get(world, &f, NULL) == MC_WORLD_STORAGE_OK && f.E &&
          world->fields.E == scheduled && !world->saving_disabled);
    *world->live.saving_disabled = false;
    CHECK(mc_world_entity_index_inspect(f.G, &index, NULL) == MC_WORLD_STORAGE_OK &&
          index.a_length == 16 && index.b == 0 && index.c == 12 && index.d == 0.75f &&
          index.e == 0);
    CHECK(mc_world_runtime_entity_by_id(world, 23, &looked_up, NULL) == MC_WORLD_STORAGE_OK &&
          looked_up == NULL);
    if (scenario == 5 || scenario == 6) {
        result = 0;
        goto cleanup;
    }
    CHECK(mc_falling_entity_allocate(entities, &a, NULL) == MC_ENTITY_OK);
    CHECK(mc_falling_entity_allocate(entities, &b, NULL) == MC_ENTITY_OK);
    mc_entity_runtime_fields(a)->id = 23;
    mc_entity_runtime_fields(b)->id = 23;
    mc_entity *entries[] = {a, b, b};
    if (scenario == 3) {
        entries[1] = NULL;
        entries[2] = b;
    }
    const mc_world_chunk_entity_list source = {entries, scenario == 3 ? 3u : 2u,
                                               sizeof(entries) / sizeof(entries[0])};
    CHECK(mc_world_runtime_call_enter(world, NULL) == MC_WORLD_STORAGE_OK);
    void *world_access_entries[] = {NULL};
    mc_world_reference_list world_access = {world_access_entries, 1, 1};
    mc_world_reference_list *original_world_accesses = world->fields.u;
    if (scenario == 8)
        world->fields.u = &world_access;
    const mc_world_storage_status published =
        mc_world_runtime_entity_batch_held(world, &source, false, &detail);
    world->fields.u = original_world_accesses;
    mc_world_runtime_call_leave(world);
    CHECK(world->fields.b->count == source.count && world->fields.b->items[0] == a &&
          world->fields.b->items[1] == entries[1]);
    CHECK(mc_world_entity_index_inspect(f.G, &index, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_entity_by_id(world, 23, &looked_up, NULL) == MC_WORLD_STORAGE_OK);
    if (scenario == 8) {
        CHECK(published == MC_WORLD_STORAGE_UNSUPPORTED_STATE && detail.index == 0 &&
              detail.publication.bytecode == 34 && detail.publication.world_access_bytecode == 29 &&
              detail.publication.dedicated_bytecode == 0 && index.b == 0 && index.e == 0 &&
              looked_up == NULL && a->holds == 2 && b->holds == 2);
    } else if (scenario == 2) {
        CHECK(published == MC_WORLD_STORAGE_LIMIT && detail.index == 1 &&
              detail.publication.bytecode == 34 && detail.publication.dedicated_bytecode == 14 &&
              detail.publication.index_bytecode == 15 && index.b == 1 && index.e == 2 &&
              looked_up == b && a->holds == 2 && b->holds == 3);
    } else if (scenario == 3) {
        CHECK(published == MC_WORLD_STORAGE_UNSUPPORTED_STATE && detail.index == 1 &&
              detail.publication.bytecode == 34 && detail.publication.dedicated_bytecode == 10 &&
              index.b == 1 && index.e == 1 && looked_up == a && a->holds == 3 && b->holds == 2);
    } else {
        CHECK(published == MC_WORLD_STORAGE_OK && index.b == 2 && index.e == 2 && looked_up == b &&
              a->holds == 2 && b->holds == 4);
        /* Actual deferred World.b(List) changes D only. It must leave dp.G
         * intact until a real later removal hook is implemented and reached. */
        CHECK(mc_world_runtime_call_enter(world, NULL) == MC_WORLD_STORAGE_OK);
        const mc_world_storage_status deferred =
            mc_world_runtime_entity_batch_held(world, &source, true, &detail);
        mc_world_runtime_call_leave(world);
        CHECK(deferred == MC_WORLD_STORAGE_OK && world->fields.D->count == 2 &&
              mc_world_entity_index_inspect(f.G, &index, NULL) == MC_WORLD_STORAGE_OK &&
              index.b == 2 && index.e == 2 && a->holds == 3 && b->holds == 5);
    }
    CHECK(mc_world_runtime_retire_entity_containers(world, NULL) == MC_WORLD_STORAGE_OK &&
          world->dedicated.G == NULL && a->holds == 1 && b->holds == 1);
    result = 0;
cleanup:
    mc_entity_release(a);
    mc_entity_release(b);
    if (world != NULL) {
        mc_world_storage_error cause;
        mc_world_storage_status retired = mc_world_runtime_retire_entity_containers(world, &cause);
        if (retired == MC_WORLD_STORAGE_OK)
            retired = mc_world_runtime_retire_world(world, &cause);
        if (retired == MC_WORLD_STORAGE_OK)
            retired = mc_world_runtime_free(world, &cause);
        if (retired != MC_WORLD_STORAGE_OK) {
            fprintf(stderr, "Dedicated scenario %u cleanup: %u %s\n", scenario,
                    (unsigned int)retired, cause.operation);
            result = 1;
        }
    }
    mc_world_provider_release(ignored_dimension);
    return result;
}

static bool decode_path(const char *hex, char *out, size_t capacity) {
    const size_t length = strlen(hex);
    if (length == 0 || length % 2u != 0 || length / 2u >= capacity)
        return false;
    for (size_t i = 0; i < length / 2u; ++i) {
        unsigned int value = 0;
        for (size_t j = 0; j < 2; ++j) {
            const unsigned char c = (unsigned char)hex[i * 2u + j];
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
    out[length / 2u] = '\0';
    return true;
}

int main(int argc, char **argv) {
    int result = 1;
    char root[8192];
    mc_java_random_profile *profile = NULL;
    mc_world_runtime_classes *classes = NULL;
    mc_entity_classes *entities = NULL;
    mc_server_runtime *server = NULL;
    mc_world_regions *regions = NULL;
    mc_world_store *store = NULL;
    mc_world_info_name *name = NULL;
    uint8_t map[256];
    const uint16_t title[] = {'D', 'e', 'd', 'i', 'c', 'a', 't', 'e', 'd'};
    if (argc != 3 || strcmp(argv[1], "world-dedicated-construction") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    for (size_t i = 0; i < sizeof(map); ++i)
        map[i] = (uint8_t)i;
    const mc_world_store_options options = {.session_token = 137,
                                            .block_map = map,
                                            .block_map_length = sizeof(map),
                                            .limits = mc_world_store_default_limits()};
    CHECK(mc_world_regions_create(NULL, &regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_open(root, regions, &options, &store, NULL) == MC_WORLD_STORAGE_OK);
    const mc_level_info metadata = {
        .seed = 23, .spawn_y = 64, .level_name = {title, sizeof(title) / sizeof(title[0])}};
    CHECK(mc_world_store_save_info(store, &metadata, 197, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_SERVER, profile, NULL, &entities) == MC_ENTITY_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_SERVER, profile, &trig, &classes, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_entities(classes, entities) == MC_ENTITY_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, sizeof(title) / sizeof(title[0])},
                                  SIZE_MAX, &name) == MC_WORLD_INFO_OK);
    CHECK(mc_server_runtime_allocate(SIZE_MAX, &server, NULL) == MC_SERVER_OK &&
          mc_server_runtime_construct(server, NULL) == MC_SERVER_OK);
    for (unsigned int scenario = 0; scenario < 9; ++scenario) {
        CHECK(one_case(classes, entities, server, store, name, scenario) == 0);
        mc_server_construction server_state;
        CHECK(mc_server_runtime_inspect(server, NULL, &server_state) == MC_SERVER_OK &&
              server_state.references == 1);
    }
    CHECK(atomic_load_explicit(&profile->holder_phase, memory_order_acquire) ==
          (unsigned int)MC_JAVA_RANDOM_HOLDER_COLD);
    result = 0;
cleanup:
    if (mc_server_runtime_release(server) != MC_SERVER_OK)
        result = 1;
    mc_world_info_name_release(name);
    if (mc_world_runtime_classes_free(classes, NULL) != MC_WORLD_STORAGE_OK)
        result = 1;
    if (mc_entity_classes_free(entities) != MC_ENTITY_OK)
        result = 1;
    if (result == 0)
        mc_java_random_profile_free(profile);
    mc_world_store_free(store);
    mc_world_regions_free(regions);
    if (result == 0)
        printf("PASS: %u constructed dedicated World checks (world-dedicated-construction)\n",
               checks);
    return result;
}
