#define MC_PLAYER_MOVEMENT_MAIN mc_spawn_movement_fixture_main
#include "../movement/test_player_movement.c"
#undef MC_PLAYER_MOVEMENT_MAIN
#include "mc/entity/player/spawn.h"
#include <math.h>
#include <xmmintrin.h>

#ifdef MC_PLAYER_SPAWN_TEST_ALLOCATOR
static bool spawn_allocation_fails;
static size_t spawn_allocations, spawn_frees;

void *mc_player_spawn_test_allocate(size_t bytes) {
    ++spawn_allocations;
    return spawn_allocation_fails ? NULL : malloc(bytes);
}

void mc_player_spawn_test_free(void *memory) {
    ++spawn_frees;
    free(memory);
}

#include "../../../src/entity/player/spawn.c"
#endif

static bool spawn_prepare(fixture *f, mc_entity *p, int scenario) {
    movement_air(f, 0);
    p->base.world = f->worlds[0];
    p->base.offset = .25f;
    p->base.y_size = .125f;
    if (mc_entity_set_size(p, 1, 1) != MC_ENTITY_OK ||
        mc_entity_set_position(p, 8.5, 8, 8.5) != MC_ENTITY_OK ||
        mc_entity_bounds_pool_reset(p->classes, NULL) != MC_ENTITY_OK ||
        mc_entity_vector_pool_reset(p->classes, NULL) != MC_ENTITY_OK)
        return false;
    p->base.velocity_x = 2;
    p->base.velocity_y = 3;
    p->base.velocity_z = 4;
    p->base.aJ = -9;
    p->base.aK = -10;
    p->base.aL = -11;
    p->base.bl = -12;
    p->base.bm = -13;
    p->base.bn = -14;
    p->base.yaw = 37;
    p->base.aT = -17;
    p->base.aU = -11;
    p->base.aV = -13;
    p->local_player->fields.living.Y = 3;
    p->local_player->fields.living.ad = 9;
    mc_java_random_reseed(p->random, 38);
    mc_java_random_reseed(f->worlds[0]->fields.r, 38);
    if (scenario == 1)
        movement_floor(f, 0);
    if (scenario == 2)
        for (int32_t y = 0; y <= 12; ++y)
            f->states[0]->data->blocks[boxes_index(8, y, 8)] = 1;
    if (scenario == 3 || scenario == 9)
        p->base.y = 0;
    if (scenario == 4)
        p->base.y = -0.0;
    if (scenario == 5)
        p->base.y = -2;
    const uint64_t nan_bits = UINT64_C(0x7ff8123456789abc);
    if (scenario == 6 || scenario == 17)
        memcpy(&p->base.y, &nan_bits, sizeof(nan_bits));
    if (scenario == 7 || scenario == 16)
        p->base.world = NULL;
    if (scenario == 8 || scenario == 9 || scenario == 16 || scenario == 17)
        p->bounds = NULL;
    const uint8_t shape = scenario == 10 ? 44 : scenario == 11 ? 85 : scenario == 12 ? 26 : 0;
    if (shape != 0)
        f->states[0]->data->blocks[boxes_index(8, 7, 8)] = shape;
    if (scenario == 13)
        f->worlds[0]->fields.K = NULL;
    if (scenario == 14)
        f->worlds[0]->fields.v = NULL;
    if (scenario == 15)
        f->worlds[0]->fields.R = NULL;
    if (scenario == 18) {
        const uint64_t bits = 1;
        memcpy(&p->base.y, &bits, sizeof(bits));
    }
    if (scenario == 19)
        p->base.y = -INFINITY;
    if (scenario == 20)
        memcpy(&p->base.x, &nan_bits, sizeof(nan_bits));
    return true;
}

static bool spawn_row(fixture *f, mc_entity *p, int body, int scenario, bool complete,
                      const mc_player_spawn_error *error) {
    const char *failure_class = "-", *failure_method = "-";
    if (!complete) {
        if (error->position_bytecode == 86 && !error->collision_failed) {
            failure_class = "sn";
            failure_method = "e";
        } else if (error->collision_failed) {
            failure_class = "fd";
            if (error->collision.world_bytecode == 4)
                failure_method = "a";
            else if (error->collision.world_bytecode == 94 &&
                     error->collision.nearby.query.provider_bytecode == 6)
                failure_method = "f";
            else if (error->collision.world_bytecode == 186)
                failure_method = "b";
            else
                return false;
        } else
            return false;
    }
    printf("SPAWN %d %d %d %s %s", body, scenario, complete ? 1 : 0, failure_class, failure_method);
    const mc_entity_bounds zero = {0};
    const mc_entity_bounds *box = p->bounds == NULL ? &zero : p->bounds;
    const double values[] = {p->base.x,          p->base.y,          p->base.z,  p->base.velocity_x,
                             p->base.velocity_y, p->base.velocity_z, p->base.aJ, p->base.aK,
                             p->base.aL,         p->base.bl,         p->base.bm, p->base.bn,
                             box->min_x,         box->min_y,         box->min_z, box->max_x,
                             box->max_y,         box->max_z};
    for (size_t i = 0; i < COUNT(values); ++i)
        printf(" %016" PRIx64, movement_double_bits(values[i]));
    const float fields[] = {p->base.offset, p->base.width, p->base.height, p->base.y_size,
                            p->base.yaw,    p->base.aT,    p->base.aU,     p->base.aV};
    for (size_t i = 0; i < COUNT(fields); ++i)
        printf(" %08" PRIx32, movement_float_bits(fields[i]));
    printf(" %" PRId32 " %" PRId32 " %d %d %012" PRIx64 " %012" PRIx64 "\n",
           p->local_player->fields.living.Y, p->local_player->fields.living.ad,
           p->base.world != NULL ? 1 : 0, p->bounds != NULL ? 1 : 0, p->random->state,
           f->worlds[0]->fields.r->state);
    return !ferror(stdout);
}

static bool spawn_cases(fixture *f, mc_entity *p, bool reference) {
    bool ok = false;
    mc_player_spawn_error *error = malloc(sizeof(*error));
    mc_entity_bounds *bounds = p->bounds;
    mc_world_reference_list *list = f->worlds[0]->fields.K;
    mc_world_reference_list *nearby = f->worlds[0]->fields.R;
    mc_chunk_provider *provider = f->worlds[0]->fields.v;
    const size_t holds = p->holds, world_holds = f->worlds[0]->persistent_bindings;
    CHECK(error != NULL);
    for (int body = 0; body < 2; ++body)
        for (int scenario = 0; scenario < 21; ++scenario) {
            CHECK(spawn_prepare(f, p, scenario));
            const bool expected =
                scenario != 8 && scenario != 13 && scenario != 14 && scenario != 15;
            memset(error, 0xa5, sizeof(*error));
            const mc_entity_status status = body == 0 ? mc_player_spawn_reset(p, NULL, error)
                                                      : mc_player_spawn_reset_body(p, NULL, error);
            const bool row =
                !reference || spawn_row(f, p, body, scenario, status == MC_ENTITY_OK, error);
            p->bounds = bounds;
            p->base.world = f->worlds[0];
            f->worlds[0]->fields.K = list;
            f->worlds[0]->fields.R = nearby;
            f->worlds[0]->fields.v = provider;
            CHECK(row && (status == MC_ENTITY_OK) == expected && status == error->status);
            CHECK(p->holds == holds && f->worlds[0]->persistent_bindings == world_holds &&
                  f->worlds[0]->active_calls == 0 && f->worlds[0]->local_leases == 0);
            CHECK(error->player_bytecode == (expected || body != 0 ? 0u : 15u));
            if (expected)
                CHECK(error->operation == NULL && error->cause == MC_PLAYER_SPAWN_CAUSE_NONE &&
                      !error->collision_failed && error->search_work == 0);
            else if (scenario == 8)
                CHECK(error->cause == MC_PLAYER_SPAWN_CAUSE_NULL_REFERENCE &&
                      error->base_bytecode == 30 && error->position_bytecode == 86 &&
                      !error->collision_failed);
            else
                CHECK(error->collision_failed && error->base_bytecode == 42 &&
                      error->collision.nearby.query.status != MC_WORLD_STORAGE_OK &&
                      error->collision.world_bytecode == (scenario == 13   ? 4u
                                                          : scenario == 14 ? 94u
                                                                           : 186u));
        }
    CHECK(spawn_prepare(f, p, 2));
    const mc_player_spawn_limits limited = {.max_search_work = 1};
    CHECK(mc_player_spawn_reset(p, &limited, error) == MC_ENTITY_LIMIT &&
          error->base_bytecode == 17 && error->search_work == 1 && p->base.y == 9 &&
          p->base.velocity_x == 2 && p->local_player->fields.living.Y == 3);
    CHECK(p->holds == holds && f->worlds[0]->local_leases == 0);
    CHECK(spawn_prepare(f, p, 0));
    const mc_entity_bounds before_zero_limit = *p->bounds;
    const mc_player_spawn_limits zero_limit = {.max_search_work = 0};
    CHECK(mc_player_spawn_reset(p, &zero_limit, error) == MC_ENTITY_LIMIT &&
          error->search_work == 0 && error->base_bytecode == 17 && p->base.y == 8 &&
          memcmp(p->bounds, &before_zero_limit, sizeof(before_zero_limit)) == 0 &&
          p->base.offset == 1.62f && p->local_player->fields.living.Y == 3);
    CHECK(spawn_prepare(f, p, 0));
    const unsigned int original_csr = _mm_getcsr();
    _mm_setcsr(original_csr | 0x8000u);
    const mc_entity_status rejected_fp = mc_player_spawn_reset(p, NULL, error);
    _mm_setcsr(original_csr);
    CHECK(rejected_fp == MC_ENTITY_UNSUPPORTED_FP && error->base_bytecode == 13 &&
          p->base.offset == 1.62f && p->base.width == .6f && p->base.height == 1.8f &&
          p->base.velocity_y == 3 && p->local_player->fields.living.Y == 3);
    CHECK(spawn_prepare(f, p, 7));
    _mm_setcsr(original_csr | 0x8000u);
    const mc_entity_status bypass_fp = mc_player_spawn_reset(p, NULL, error);
    _mm_setcsr(original_csr);
    p->base.world = f->worlds[0];
    CHECK(bypass_fp == MC_ENTITY_OK && p->base.aT == -17 && p->base.velocity_x == 2 &&
          p->local_player->fields.living.Y == 20);
    CHECK(mc_player_spawn_reset(NULL, NULL, error) == MC_ENTITY_INVALID_ARGUMENT);
    mc_entity *raw = NULL;
    CHECK(mc_local_player_allocate(p->classes, &raw, NULL) == MC_ENTITY_OK);
    const mc_entity_status raw_status = mc_player_spawn_reset(raw, NULL, error);
    mc_entity_release(raw);
    CHECK(raw_status == MC_ENTITY_INVALID_PHASE && error->cause == MC_PLAYER_SPAWN_CAUSE_OWNER);
    CHECK(spawn_prepare(f, p, 0));
    CHECK(mc_player_spawn_reset(p, NULL, NULL) == MC_ENTITY_OK);
#ifdef MC_PLAYER_SPAWN_TEST_ALLOCATOR
    CHECK(spawn_prepare(f, p, 0));
    const mc_entity_base_fields before = p->base;
    const size_t allocated = spawn_allocations, freed = spawn_frees;
    spawn_allocation_fails = true;
    const mc_entity_status failed_allocation = mc_player_spawn_reset(p, NULL, error);
    spawn_allocation_fails = false;
    CHECK(failed_allocation == MC_ENTITY_ALLOCATION_FAILED &&
          memcmp(&before, &p->base, sizeof(before)) == 0 && p->holds == holds &&
          spawn_allocations == allocated + 1 && spawn_frees == freed);
    CHECK(spawn_allocations == spawn_frees + 1);
#endif
    ok = true;
cleanup:
    p->bounds = bounds;
    p->base.world = f->worlds[0];
    f->worlds[0]->fields.K = list;
    f->worlds[0]->fields.R = nearby;
    f->worlds[0]->fields.v = provider;
    free(error);
    return ok;
}

#ifndef MC_PLAYER_SPAWN_MAIN
#define MC_PLAYER_SPAWN_MAIN main
#endif
int MC_PLAYER_SPAWN_MAIN(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    mc_player_session_classes *sessions = NULL;
    mc_player_session *session = NULL;
    mc_player_string *username = NULL;
    mc_entity *player = NULL;
    char root[8192];
    const uint16_t title[] = {'S', 'p', 'a', 'w', 'n'},
                   name[] = {'S', 'p', 'a', 'w', 'n', 'P', 'l', 'a', 'y', 'e', 'r'};
    if (argc != 3 || !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    const bool reference = strcmp(argv[1], "spawn-reference") == 0;
    CHECK(reference || strcmp(argv[1], "spawn") == 0);
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(8u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    resources *r = &f->sources[0];
    r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
    r->streams[1] = r->streams[2] = (stream){.bytes = language};
    r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0, &r->api};
    CHECK(mc_registry_classes_allocate(&options, &f->registry[0], NULL) == MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes[0],
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(f->classes[0], f->registry[0], NULL) ==
          MC_REGISTRY_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entity_classes[0]) ==
          MC_ENTITY_OK);
    CHECK(mc_world_runtime_classes_bind_entities(f->classes[0], f->entity_classes[0]) ==
          MC_ENTITY_OK);
    CHECK(mc_registry_classes_initialize_blocks(f->registry[0], NULL) == MC_REGISTRY_OK);
    CHECK(boxes_open_store(f, 0, root) && boxes_create_world(f, 0));
    CHECK(mc_registry_classes_initialize_statistics(f->registry[0], NULL) == MC_REGISTRY_OK);
    CHECK(mc_player_session_classes_create(f->entity_classes[0], f->registry[0], &sessions, NULL) ==
          MC_PLAYER_OK);
    CHECK(mc_player_string_create(name, COUNT(name), &username) == MC_PLAYER_OK);
    CHECK(mc_player_session_create(sessions, username, NULL, &session, NULL) == MC_PLAYER_OK);
    CHECK(mc_local_player_allocate(f->entity_classes[0], &player, NULL) == MC_ENTITY_OK);
    CHECK(mc_local_player_construct(player, f->worlds[0], NULL, session, 0, NULL, NULL) ==
          MC_PLAYER_OK);
    CHECK(spawn_cases(f, player, reference));
    result = 0;
cleanup:
    mc_entity_release(player);
    mc_player_session_retire(&session);
    mc_player_string_release(username);
    if (mc_player_session_classes_free(&sessions, NULL) != MC_PLAYER_OK || !cleanup_fixture(f))
        return 1;
    if (result == 0 && printf("PASS: %u Player spawn reset checks\n", checks) < 0)
        return 1;
    return result;
}
