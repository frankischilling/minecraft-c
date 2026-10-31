/* Authored source-field/failed-prefix controls on genuine RAW native owners.
 * The loaded-World helper separately covers normal acquisition and membership. */
#include "../../../src/entity/entity_internal.h"
#include "../../../src/world/runtime/runtime_internal.h"
#include "mc/core/math/math.h"
#include "mc/world/runtime/client_tick_state.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <xmmintrin.h>

static unsigned int checks;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c);                                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static mc_math_trig trig;

int main(void) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_world_runtime_classes *classes = NULL, *server_classes = NULL;
    mc_world_runtime *world = NULL, *server = NULL;
    mc_entity_classes *entities = NULL;
    mc_entity *entity = NULL;
    mc_world_refresh_entity_call *call = NULL;
    mc_world_client_tick_state_error error;
    mc_world_refresh_entity_error refresh;
    mc_world_refresh_entity_state state;
    CHECK(mc_math_trig_init(&trig));
    CHECK(mc_java_random_profile_create(1024u * 1024u, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &entities) == MC_ENTITY_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, profile, &trig, &classes, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_entities(classes, entities) == MC_ENTITY_OK);
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE, classes, NULL,
                                          &world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_SERVER, profile, &trig, &server_classes,
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_SERVER_LOAD_OR_CREATE, server_classes,
                                          NULL, &server, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_falling_entity_allocate(entities, &entity, NULL) == MC_ENTITY_OK);
    const mc_world_runtime_fields original = world->fields;
    const int32_t difficulties[] = {INT32_MIN, -1, 0, 1, 3, INT32_MAX};
    for (size_t i = 0; i < sizeof(difficulties) / sizeof(difficulties[0]); ++i) {
        CHECK(mc_world_runtime_client_difficulty_store(world, difficulties[i], &error) ==
                  MC_WORLD_STORAGE_OK &&
              world->fields.q == difficulties[i] && !error.native_policy);
        mc_world_runtime_fields fields = world->fields;
        fields.q = original.q;
        CHECK(memcmp(&fields, &original, sizeof(fields)) == 0);
    }
    world->fields.q = original.q;
    const int32_t countdowns[] = {INT32_MIN, -1, 0, 1, 2, INT32_MAX};
    for (size_t i = 0; i < sizeof(countdowns) / sizeof(countdowns[0]); ++i) {
        world->fields.n = countdowns[i];
        CHECK(mc_world_runtime_client_lightning_decrement(world, &error) == MC_WORLD_STORAGE_OK &&
              world->fields.n == (countdowns[i] > 0 ? countdowns[i] - 1 : countdowns[i]));
        mc_world_runtime_fields fields = world->fields;
        fields.n = original.n;
        CHECK(memcmp(&fields, &original, sizeof(fields)) == 0);
    }
    world->fields.n = original.n;
    for (unsigned int bits = 0; bits < 4; ++bits) {
        CHECK(mc_world_runtime_client_spawn_flags(world, (bits & 1) != 0, (bits & 2) != 0,
                                                  &error) == MC_WORLD_STORAGE_OK &&
              error.bytecode == 10 && world->fields.N == ((bits & 1) != 0) &&
              world->fields.O == ((bits & 2) != 0));
        mc_world_runtime_fields fields = world->fields;
        fields.N = original.N;
        fields.O = original.O;
        CHECK(memcmp(&fields, &original, sizeof(fields)) == 0);
    }
    world->fields.N = original.N;
    world->fields.O = original.O;
    CHECK(mc_world_runtime_client_difficulty_store(NULL, 9, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          error.native_policy);
    CHECK(mc_world_runtime_client_difficulty_store(server, 9, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          server->fields.q == 0);
    CHECK(mc_world_runtime_refresh_entity_begin(world, NULL, &call, &refresh) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          call != NULL);
    CHECK(mc_world_runtime_refresh_entity_inspect(call, &state) && state.source_pc == 1 &&
          !state.error->native_policy && !state.returned && state.chunk_calls == 0);
    mc_world_refresh_entity_call *saved = call;
    CHECK(mc_world_runtime_refresh_entity_begin(world, entity, &call, &refresh) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          call == saved && refresh.native_policy);
    CHECK(mc_world_runtime_refresh_entity_retire(&call, &refresh) == MC_WORLD_STORAGE_OK &&
          call == NULL && world->persistent_bindings == 0);
    entity->base.x = -0.25;
    entity->base.z = 32.0;
    CHECK(mc_world_runtime_refresh_entity_begin(world, entity, &call, &refresh) ==
          MC_WORLD_STORAGE_UNSUPPORTED_STATE);
    CHECK(mc_world_runtime_refresh_entity_inspect(call, &state) && state.source_pc == 62 &&
          state.center_x == -1 && state.center_z == 2 && state.chunk_calls == 1 &&
          state.chunk_returns == 0 && state.chunks[0].x == -3 && state.chunks[0].z == 0);
    CHECK(mc_world_runtime_refresh_entity_retire(&call, NULL) == MC_WORLD_STORAGE_OK &&
          entity->holds == 1 && world->persistent_bindings == 0 && world->active_calls == 0);
    const unsigned int csr = _mm_getcsr();
    _mm_setcsr(csr | 0x8000u);
    const mc_world_storage_status fp_status =
        mc_world_runtime_refresh_entity_begin(world, entity, &call, &refresh);
    _mm_setcsr(csr);
    CHECK(fp_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          mc_world_runtime_refresh_entity_inspect(call, &state) && state.source_pc == 7 &&
          state.error->native_policy && state.chunk_calls == 0);
    CHECK(mc_world_runtime_refresh_entity_retire(&call, NULL) == MC_WORLD_STORAGE_OK);
    entity->base.x = (double)INT32_MAX * 16.0;
    entity->base.z = 0.0;
    CHECK(mc_world_runtime_refresh_entity_begin(world, entity, &call, &refresh) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          mc_world_runtime_refresh_entity_inspect(call, &state) && state.source_pc == 83 &&
          state.chunk_calls == 0); /* Original wrapping bounds skip the outer loop. */
    CHECK(mc_world_runtime_refresh_entity_retire(&call, NULL) == MC_WORLD_STORAGE_OK);
    entity->base.x = (double)(INT32_MAX - 2) * 16.0;
    entity->base.z = (double)INT32_MAX * 16.0;
    CHECK(mc_world_runtime_refresh_entity_begin(world, entity, &call, &refresh) ==
              MC_WORLD_STORAGE_LIMIT &&
          mc_world_runtime_refresh_entity_inspect(call, &state) && state.source_pc == 33 &&
          state.chunk_calls == 0 && state.error->native_policy);
    CHECK(mc_world_runtime_refresh_entity_retire(&call, NULL) == MC_WORLD_STORAGE_OK);
    result = 0;
cleanup:
    (void)mc_world_runtime_refresh_entity_retire(&call, NULL);
    mc_entity_release(entity);
    if (world != NULL && mc_world_runtime_free(world, NULL) != MC_WORLD_STORAGE_OK)
        result = 1;
    if (server != NULL && mc_world_runtime_free(server, NULL) != MC_WORLD_STORAGE_OK)
        result = 1;
    if (mc_world_runtime_classes_free(classes, NULL) != MC_WORLD_STORAGE_OK ||
        mc_world_runtime_classes_free(server_classes, NULL) != MC_WORLD_STORAGE_OK ||
        mc_entity_classes_free(entities) != MC_ENTITY_OK)
        result = 1;
    mc_java_random_profile_free(profile);
    if (result == 0)
        printf("%u client World tick-state checks passed\n", checks);
    return result;
}
