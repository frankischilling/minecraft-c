#define main mc_controller_existing_construction_fixture_main
#include "../../entity/player/test_construction.c"
#undef main

#include "../../../src/client/application_state.h"
#include "../../../src/client/controller/singleplayer_internal.h"
#include "../../../src/world/runtime/runtime_internal.h"
#include "mc/client/controller/singleplayer.h"
#include <inttypes.h>
#include <xmmintrin.h>

#ifdef MC_SINGLEPLAYER_CONTROLLER_TEST_ALLOCATOR
static size_t allocation_calls, live_allocations, fail_call;

void *mc_singleplayer_controller_test_calloc(size_t count, size_t size) {
    ++allocation_calls;
    if (allocation_calls == fail_call)
        return NULL;
    void *p = calloc(count, size);
    if (p != NULL)
        ++live_allocations;
    return p;
}

void mc_singleplayer_controller_test_free(void *p) {
    if (p != NULL)
        --live_allocations;
    free(p);
}

#include "../../../src/client/controller/singleplayer.c"
#endif

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static uint64_t dbits(double value) {
    uint64_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool controller_case(fixture *f, client_application *app, bool null_app) {
    mc_client_singleplayer_controller *c = NULL;
    mc_singleplayer_controller_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL);
    CHECK(mc_client_singleplayer_controller_allocate(f->entities, f->profile, 1, &c, error) ==
              MC_SINGLEPLAYER_CONTROLLER_LIMIT &&
          c == NULL);
    mc_entity_class_snapshot before, after;
    CHECK(mc_entity_classes_get_snapshot(f->entities, &before) == MC_ENTITY_OK);
    CHECK(mc_client_singleplayer_controller_allocate(f->entities, f->profile, 0, &c, error) ==
          MC_SINGLEPLAYER_CONTROLLER_OK);
    mc_singleplayer_controller_snapshot view;
    CHECK(mc_client_singleplayer_controller_read(c, &view) &&
          view.phase == MC_SINGLEPLAYER_CONTROLLER_RAW && view.a == NULL && view.c == 0);
    CHECK(mc_client_singleplayer_controller_change_world(c, NULL, error) ==
          MC_SINGLEPLAYER_CONTROLLER_INVALID_PHASE);
    CHECK(mc_client_singleplayer_controller_construct(c, null_app ? NULL : app, error) ==
          MC_SINGLEPLAYER_CONTROLLER_OK);
    CHECK(mc_entity_classes_get_snapshot(f->entities, &after) == MC_ENTITY_OK &&
          before.entity_class == after.entity_class && before.next_id == after.next_id &&
          before.live_entities == after.live_entities);
    CHECK(mc_client_singleplayer_controller_read(c, &view) &&
          view.phase == MC_SINGLEPLAYER_CONTROLLER_READY && view.a == (null_app ? NULL : app) &&
          !view.b && view.c == -1 && view.d == -1 && view.e == -1 && bits(view.f) == 0 &&
          bits(view.g) == 0 && bits(view.h) == 0 && view.i == 0 && view.base_constructor_pc == 14 &&
          view.constructor_pc == 40);
    printf("CTRLCTOR %d %d %d %d %d %d %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %d\n",
           null_app ? 1 : 0, view.a == (null_app ? NULL : app), view.b, view.c, view.d, view.e,
           bits(view.f), bits(view.g), bits(view.h), view.i);
    CHECK(mc_client_singleplayer_controller_construct(c, app, error) ==
          MC_SINGLEPLAYER_CONTROLLER_INVALID_PHASE);
    for (int body = 0; body < 2; ++body) {
        CHECK(mc_client_singleplayer_controller_change_world(
                  c, body == 0 ? NULL : f->world, error) == MC_SINGLEPLAYER_CONTROLLER_OK);
        CHECK(mc_client_singleplayer_controller_finish_player(c, NULL, error) ==
              MC_SINGLEPLAYER_CONTROLLER_OK);
        mc_singleplayer_controller_snapshot changed;
        CHECK(mc_client_singleplayer_controller_read(c, &changed) && changed.a == view.a &&
              changed.b == view.b && changed.c == view.c && changed.d == view.d &&
              changed.e == view.e && bits(changed.f) == bits(view.f) &&
              bits(changed.g) == bits(view.g) && bits(changed.h) == bits(view.h) &&
              changed.i == view.i && !changed.entered && changed.failed_player == NULL);
    }
    CHECK(mc_client_singleplayer_controller_retain(c) == MC_SINGLEPLAYER_CONTROLLER_OK);
    CHECK(mc_client_singleplayer_controller_free(&c, error) == MC_SINGLEPLAYER_CONTROLLER_BUSY &&
          c != NULL);
    CHECK(mc_client_singleplayer_controller_release(c) == MC_SINGLEPLAYER_CONTROLLER_OK);
    CHECK(mc_client_singleplayer_controller_free(&c, error) == MC_SINGLEPLAYER_CONTROLLER_OK &&
          c == NULL);
    free(error);
    return true;
}

static bool create_case(fixture *f, client_application *app, int scenario) {
    mc_client_singleplayer_controller *c = NULL;
    mc_singleplayer_controller_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL);
    const bool null_app = scenario == 3 || scenario == 8;
    const bool null_world = scenario == 3 || scenario == 4 || scenario == 7;
    const bool null_provider = scenario == 5 || scenario == 8;
    const bool null_session = scenario == 6 || scenario == 7;
    mc_world_provider *saved_provider = f->world->fields.t;
    mc_player_session *saved_session = app->session;
    if (null_provider)
        f->world->fields.t = NULL;
    if (null_session)
        app->session = NULL;
    const int32_t y = scenario == 1 ? INT32_MAX : scenario == 2 ? INT32_MIN : 64;
    CHECK(mc_world_info_spawn_set(mc_world_runtime_info(f->world), -7, y, 11) == MC_WORLD_INFO_OK);
    CHECK(mc_client_singleplayer_controller_allocate(f->entities, f->profile, 0, &c, error) ==
              MC_SINGLEPLAYER_CONTROLLER_OK &&
          mc_client_singleplayer_controller_construct(c, null_app ? NULL : app, error) ==
              MC_SINGLEPLAYER_CONTROLLER_OK);
    mc_entity_class_snapshot before, after;
    CHECK(mc_entity_classes_get_snapshot(f->entities, &before) == MC_ENTITY_OK);
    const size_t world_bindings = f->world->persistent_bindings;
    const size_t session_holds = saved_session->holds;
    mc_entity *p = NULL;
    const mc_singleplayer_controller_status status =
        mc_client_singleplayer_controller_create_player(c, null_world ? NULL : f->world, NULL, &p,
                                                        error);
    CHECK(mc_entity_classes_get_snapshot(f->entities, &after) == MC_ENTITY_OK);
    CHECK(saved_session->holds == session_holds &&
          f->world->persistent_bindings ==
              world_bindings + (scenario < 3 || scenario == 6 ? 1u : 0u));
    const bool complete = scenario < 3;
    const char *failure_class = "-", *failure_method = "-";
    if (!complete) {
        failure_class = scenario == 6 ? "dc" : "ob";
        failure_method = scenario == 6 ? "<init>" : "b";
    }
    CHECK((status == MC_SINGLEPLAYER_CONTROLLER_OK) == complete);
    if (complete) {
        CHECK(p != NULL && p->phase == MC_ENTITY_READY && p->base.world == f->world &&
              p->local_player->fields.minecraft == app && p->local_player->fields.input == NULL &&
              p->local_player->fields.player.m == 0);
        mc_player_session_state session_state;
        CHECK(mc_player_session_read(saved_session, &session_state) &&
              p->local_player->fields.player.l == session_state.username);
    } else {
        CHECK(p == NULL && c->fields.failed_player != NULL &&
              error->site.pc == (scenario == 6 ? 23u
                                 : null_app    ? 13u
                                 : null_world  ? 17u
                                               : 20u));
        if (scenario == 6) {
            CHECK(status == MC_SINGLEPLAYER_CONTROLLER_PLAYER_FAILED &&
                  error->player.status == MC_PLAYER_NULL_REFERENCE &&
                  error->player.site.pc == 103 &&
                  c->fields.failed_player->phase == MC_ENTITY_FAILED_CONSTRUCTION &&
                  c->fields.failed_player->local_player->fields.local_pc == 46);
        } else
            CHECK(status == MC_SINGLEPLAYER_CONTROLLER_NULL_REFERENCE &&
                  c->fields.failed_player->phase == MC_ENTITY_RAW);
        CHECK(mc_client_singleplayer_controller_create_player(c, NULL, NULL, &p, NULL) ==
                  MC_SINGLEPLAYER_CONTROLLER_BUSY &&
              p == NULL);
        mc_entity *kept = c->fields.failed_player;
        CHECK(mc_entity_retain(kept) == MC_ENTITY_OK);
        kept->base.aU = -11;
        CHECK(mc_client_singleplayer_controller_prepare_player(c, kept, error) ==
                  MC_SINGLEPLAYER_CONTROLLER_OK &&
              bits(kept->base.yaw) == 0xc3340000u && kept->base.aU == -11);
        CHECK(mc_client_singleplayer_controller_clear_failed_player(c, error) ==
                  MC_SINGLEPLAYER_CONTROLLER_OK &&
              c->fields.failed_player == NULL && kept->holds == 1);
        mc_entity_release(kept);
    }
    printf("CTRLNEW %d %d %s %s %d %d %d %d %d %016" PRIx64 " %016" PRIx64 " %016" PRIx64
           " %08" PRIx32 " %08" PRIx32 "\n",
           scenario, complete, failure_class, failure_method,
           mc_java_i32_sub(after.next_id, before.next_id), p != NULL,
           p != NULL && p->local_player->fields.minecraft == app,
           p != NULL && p->base.world == f->world, p == NULL ? 0 : p->local_player->fields.player.m,
           p == NULL ? UINT64_C(0) : dbits(p->base.x), p == NULL ? UINT64_C(0) : dbits(p->base.y),
           p == NULL ? UINT64_C(0) : dbits(p->base.z), p == NULL ? 0u : bits(p->base.yaw),
           p == NULL ? 0u : bits(p->base.aU));
    if (p != NULL) {
        const uint32_t raw_yaw[] = {0x42140000u, 0x80000000u, 0x7fc12345u};
        for (unsigned int k = 0; k < 3; ++k) {
            memcpy(&p->base.yaw, &raw_yaw[k], sizeof(float));
            p->base.aU = -11.0f;
            mc_entity_base_fields expected;
            memcpy(&expected, &p->base, sizeof(expected));
            expected.yaw = -180.0f;
            const unsigned int csr = _mm_getcsr();
            _mm_setcsr(csr | 0x8040u);
            CHECK(mc_client_singleplayer_controller_prepare_player(c, p, error) ==
                  MC_SINGLEPLAYER_CONTROLLER_OK);
            const unsigned int after_csr = _mm_getcsr();
            _mm_setcsr(csr);
            CHECK(after_csr == (csr | 0x8040u));
            CHECK(memcmp(&expected, &p->base, sizeof(expected)) == 0 && p->holds == 1);
            CHECK(mc_client_singleplayer_controller_finish_player(c, p, error) ==
                  MC_SINGLEPLAYER_CONTROLLER_OK);
            printf("CTRLYAW %d %u %08" PRIx32 " %08" PRIx32 "\n", scenario, k, bits(p->base.yaw),
                   bits(p->base.aU));
        }
        mc_entity_release(p);
    }
    CHECK(mc_client_singleplayer_controller_prepare_player(c, NULL, error) ==
              MC_SINGLEPLAYER_CONTROLLER_NULL_REFERENCE &&
          error->site.pc == 3 && strcmp(error->site.class_name, "os") == 0);
    CHECK(mc_client_singleplayer_controller_free(&c, error) == MC_SINGLEPLAYER_CONTROLLER_OK);
    f->world->fields.t = saved_provider;
    app->session = saved_session;
    free(error);
    return true;
}

static bool session_reload(fixture *f, client_application *app) {
    mc_client_singleplayer_controller *c = NULL;
    mc_player_session *saved = app->session;
    CHECK(mc_client_singleplayer_controller_allocate(f->entities, f->profile, 0, &c, NULL) ==
              MC_SINGLEPLAYER_CONTROLLER_OK &&
          mc_client_singleplayer_controller_construct(c, app, NULL) ==
              MC_SINGLEPLAYER_CONTROLLER_OK);
    mc_player_session *replacement = NULL;
    CHECK(mc_player_session_create(f->sessions, NULL, NULL, &replacement, NULL) == MC_PLAYER_OK);
    app->session = replacement;
    mc_entity *p = NULL;
    CHECK(mc_client_singleplayer_controller_create_player(c, f->world, NULL, &p, NULL) ==
              MC_SINGLEPLAYER_CONTROLLER_OK &&
          p != NULL && p->local_player->fields.player.l == NULL);
    mc_entity_release(p);
    app->session = saved;
    CHECK(mc_client_singleplayer_controller_create_player(c, f->world, NULL, &p, NULL) ==
              MC_SINGLEPLAYER_CONTROLLER_OK &&
          p != NULL && p->local_player->fields.player.l == saved->username);
    mc_entity_release(p);
    mc_player_session_retire(&replacement);
    CHECK(mc_client_singleplayer_controller_free(&c, NULL) == MC_SINGLEPLAYER_CONTROLLER_OK);
    return true;
}

static bool allocation_cases(fixture *f, client_application *app) {
#ifdef MC_SINGLEPLAYER_CONTROLLER_TEST_ALLOCATOR
    mc_client_singleplayer_controller *c = NULL;
    mc_singleplayer_controller_error *e = calloc(1, sizeof(*e));
    CHECK(e != NULL);
    for (size_t index = 1; index <= 2; ++index) {
        fail_call = allocation_calls + index;
        CHECK(mc_client_singleplayer_controller_allocate(f->entities, f->profile, 0, &c, e) ==
                  MC_SINGLEPLAYER_CONTROLLER_ALLOCATION_FAILED &&
              c == NULL && live_allocations == 0);
    }
    fail_call = 0;
    CHECK(mc_client_singleplayer_controller_allocate(f->entities, f->profile, 0, &c, e) ==
              MC_SINGLEPLAYER_CONTROLLER_OK &&
          live_allocations == 1);
    fail_call = allocation_calls + 1;
    CHECK(mc_client_singleplayer_controller_construct(c, app, e) ==
              MC_SINGLEPLAYER_CONTROLLER_ALLOCATION_FAILED &&
          c->fields.phase == MC_SINGLEPLAYER_CONTROLLER_RAW && live_allocations == 1);
    fail_call = 0;
    CHECK(mc_client_singleplayer_controller_construct(c, app, e) == MC_SINGLEPLAYER_CONTROLLER_OK);
    mc_entity *p = NULL;
    mc_entity_class_snapshot before, after;
    CHECK(mc_entity_classes_get_snapshot(f->entities, &before) == MC_ENTITY_OK);
    fail_call = allocation_calls + 1;
    CHECK(mc_client_singleplayer_controller_create_player(c, f->world, NULL, &p, e) ==
              MC_SINGLEPLAYER_CONTROLLER_ALLOCATION_FAILED &&
          p == NULL && c->fields.failed_player == NULL);
    CHECK(mc_entity_classes_get_snapshot(f->entities, &after) == MC_ENTITY_OK &&
          before.next_id == after.next_id && before.live_entities == after.live_entities);
    fail_call = 0;
    CHECK(mc_client_singleplayer_controller_create_player(c, NULL, NULL, &p, e) ==
              MC_SINGLEPLAYER_CONTROLLER_NULL_REFERENCE &&
          p == NULL && c->fields.failed_player != NULL &&
          c->fields.failed_player->phase == MC_ENTITY_RAW);
    mc_entity *kept = c->fields.failed_player;
    CHECK(mc_entity_retain(kept) == MC_ENTITY_OK);
    const size_t cleanup_calls = allocation_calls;
    fail_call = allocation_calls + 1;
    memset(e, 0xa5, sizeof(*e));
    CHECK(mc_client_singleplayer_controller_clear_failed_player(c, e) ==
              MC_SINGLEPLAYER_CONTROLLER_OK &&
          e->status == MC_SINGLEPLAYER_CONTROLLER_OK && e->operation == NULL &&
          e->player.site.class_name == NULL && e->entity.status == MC_ENTITY_OK &&
          c->fields.failed_player == NULL && kept->holds == 1 &&
          allocation_calls == cleanup_calls && live_allocations == 1);
    CHECK(mc_client_singleplayer_controller_retain(c) == MC_SINGLEPLAYER_CONTROLLER_OK);
    CHECK(mc_client_singleplayer_controller_free(&c, e) == MC_SINGLEPLAYER_CONTROLLER_BUSY &&
          c != NULL && e->status == MC_SINGLEPLAYER_CONTROLLER_BUSY &&
          e->player.site.class_name == NULL && allocation_calls == cleanup_calls);
    CHECK(mc_client_singleplayer_controller_release(c) == MC_SINGLEPLAYER_CONTROLLER_OK);
    CHECK(mc_client_singleplayer_controller_clear_failed_player(c, NULL) ==
              MC_SINGLEPLAYER_CONTROLLER_OK &&
          allocation_calls == cleanup_calls);
    CHECK(mc_client_singleplayer_controller_free(&c, e) == MC_SINGLEPLAYER_CONTROLLER_OK &&
          c == NULL && e->status == MC_SINGLEPLAYER_CONTROLLER_OK && e->operation == NULL &&
          allocation_calls == cleanup_calls && live_allocations == 0);
    mc_entity_release(kept);
    CHECK(mc_client_singleplayer_controller_free(&c, NULL) == MC_SINGLEPLAYER_CONTROLLER_OK &&
          allocation_calls == cleanup_calls);
    CHECK(mc_client_singleplayer_controller_free(NULL, e) ==
              MC_SINGLEPLAYER_CONTROLLER_INVALID_ARGUMENT &&
          allocation_calls == cleanup_calls);
    CHECK(mc_client_singleplayer_controller_clear_failed_player(NULL, e) ==
              MC_SINGLEPLAYER_CONTROLLER_INVALID_ARGUMENT &&
          allocation_calls == cleanup_calls);
    fail_call = 0;
    CHECK(mc_client_singleplayer_controller_allocate(f->entities, f->profile, 0, &c, e) ==
          MC_SINGLEPLAYER_CONTROLLER_OK);
    const size_t raw_cleanup_calls = allocation_calls;
    fail_call = allocation_calls + 1;
    CHECK(mc_client_singleplayer_controller_clear_failed_player(c, e) ==
              MC_SINGLEPLAYER_CONTROLLER_INVALID_PHASE &&
          allocation_calls == raw_cleanup_calls);
    CHECK(mc_client_singleplayer_controller_free(&c, NULL) == MC_SINGLEPLAYER_CONTROLLER_OK &&
          c == NULL && allocation_calls == raw_cleanup_calls && live_allocations == 0);
    fail_call = 0;
    CHECK(mc_client_singleplayer_controller_allocate(f->entities, f->profile, 0, &c, e) ==
              MC_SINGLEPLAYER_CONTROLLER_OK &&
          mc_client_singleplayer_controller_construct(c, app, e) == MC_SINGLEPLAYER_CONTROLLER_OK);
    mc_player_session *saved = app->session;
    app->session = NULL;
    CHECK(mc_client_singleplayer_controller_create_player(c, f->world, NULL, &p, e) ==
              MC_SINGLEPLAYER_CONTROLLER_PLAYER_FAILED &&
          c->fields.failed_player != NULL &&
          c->fields.failed_player->phase == MC_ENTITY_FAILED_CONSTRUCTION);
    app->session = saved;
    const size_t failed_cleanup_calls = allocation_calls;
    fail_call = allocation_calls + 1;
    CHECK(mc_client_singleplayer_controller_free(&c, e) == MC_SINGLEPLAYER_CONTROLLER_OK &&
          c == NULL && allocation_calls == failed_cleanup_calls && live_allocations == 0);
    fail_call = 0;
    free(e);
#else
    (void)f;
    (void)app;
#endif
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3 || !mc_math_trig_init(&trig))
        return 2;
    fixture f = {0};
    if (!setup(&f, argv[1], argv[2]))
        return 1;
    client_application *app = calloc(1, sizeof(*app));
    if (app == NULL)
        return 1;
    app->entities = f.entities;
    app->profile = f.profile;
    static const uint16_t name[] = {'C', 'o', 'n', 't', 'r', 'o', 'l', 'l', 'e', 'r'};
    mc_player_string *username = NULL;
    if (mc_player_string_create(name, 10, &username) != MC_PLAYER_OK ||
        mc_player_session_create(f.sessions, username, NULL, &app->session, NULL) != MC_PLAYER_OK)
        return 1;
    mc_player_string_release(username);
    if (!controller_case(&f, app, false) || !controller_case(&f, app, true))
        return 1;
    for (int scenario = 0; scenario <= 8; ++scenario)
        if (!create_case(&f, app, scenario))
            return 1;
    if (!session_reload(&f, app) || !allocation_cases(&f, app))
        return 1;
    mc_player_session_retire(&app->session);
    free(app);
    if (!teardown(&f))
        return 1;
    printf("PASS: %u single-player controller checks\n", checks);
    return 0;
}
