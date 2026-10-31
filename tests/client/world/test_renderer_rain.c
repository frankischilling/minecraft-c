/* Finite body controls: prepared PX fields, genuine RAW World/Entity owners. */
#include "client/application_state.h"
#include "client/world/entity_renderer/internal.h"
#include "client/world/entity_renderer/rain_tick_internal.h"
#include "entity/entity_internal.h"
#include "mc/core/math/math.h"
#include "world/runtime/runtime_internal.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "Rain check%u at%d: %s\n", checks, __LINE__, #value);                  \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static float from_bits(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

int main(void) {
    static const uint32_t inputs[] = {0,
                                      UINT32_C(0x80000000),
                                      1,
                                      UINT32_C(0x3c23d70a),
                                      UINT32_C(0x3dcccccd),
                                      UINT32_C(0xbdcccccd),
                                      UINT32_C(0x7fc12345),
                                      UINT32_C(0x7f800000),
                                      UINT32_C(0xff800000),
                                      UINT32_C(0x3f800000)};
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *entities = NULL;
    mc_world_runtime_classes *world_classes = NULL;
    mc_world_runtime *world = NULL;
    mc_entity *camera = NULL;
    mc_game_settings *settings = NULL;
    mc_math_trig *trig = calloc(1, sizeof(*trig));
    client_application *app = calloc(1, sizeof(*app));
    mc_client_source_px *px = calloc(1, sizeof(*px));
    CHECK(trig != NULL && app != NULL && px != NULL);
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &entities) == MC_ENTITY_OK);
    CHECK(mc_local_player_allocate(entities, &camera, NULL) == MC_ENTITY_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, profile, trig, &world_classes,
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, world_classes,
                                          NULL, &world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_game_settings_create_defaults(NULL, &settings, NULL) == MC_GAME_SETTINGS_OK);
    app->active_world = world;
    app->settings = settings;
    app->i = camera;
    camera->base.x = -1.25;
    camera->base.y = 64.5;
    camera->base.z = 2.75;
    px->phase = MC_CLIENT_ENTITY_RENDERER_READY;
    px->holds = 1;
    px->fields.j = app;
    px->fields.l = -31;
    mc_java_random random;
    px->fields.J = &random;
    mc_client_renderer_rain_call *call = NULL;
    mc_client_renderer_rain_state state;
    mc_client_renderer_rain_error error;
    for (unsigned int fancy = 0; fancy < 2; ++fancy) {
        mc_game_settings_state options;
        CHECK(mc_game_settings_read_state(settings, &options));
        options.values.fancy_graphics = fancy != 0;
        CHECK(mc_game_settings_set_values(settings, &options.values, NULL) == MC_GAME_SETTINGS_OK);
        for (size_t index = 0; index < sizeof(inputs) / sizeof(inputs[0]); ++index) {
            world->fields.i = 0.0f;
            world->fields.j = from_bits(inputs[index]);
            mc_java_random_seed(&random, -7);
            px->fields.K = 17;
            const size_t held = px->holds;
            const mc_client_renderer_rain_status status =
                mc_client_entity_renderer_rain_tick_begin(px, &call, &error);
            CHECK(call != NULL && mc_client_entity_renderer_rain_tick_inspect(call, &state));
            CHECK(state.strength_read && state.fancy_read && state.fancy == (fancy != 0) &&
                  !state.active && px->fields.K == 17 && px->holds == held + 1);
            CHECK(status == MC_CLIENT_RENDERER_RAIN_OK ||
                  (status == MC_CLIENT_RENDERER_RAIN_CHILD_FAILED && error.source_pc == 179));
            CHECK(state.returned == (status == MC_CLIENT_RENDERER_RAIN_OK));
            if (state.reseeded)
                CHECK(state.camera_x == -2 && state.camera_y == 64 && state.camera_z == 2);
            const int64_t next = mc_java_random_next_i64(&random);
            printf("RAIN %u %zu %s %d %016" PRIx64 " %d\n", fancy, index,
                   status == MC_CLIENT_RENDERER_RAIN_OK ? "OK" : "FAIL", state.reseeded,
                   (uint64_t)next, px->fields.K);
            mc_client_renderer_rain_call *same = call;
            const uint64_t random_state = random.state;
            CHECK(mc_client_entity_renderer_rain_tick_begin(px, &call, &error) ==
                      MC_CLIENT_RENDERER_RAIN_ARGUMENT &&
                  call == same && random.state == random_state && px->holds == held + 1);
            CHECK(mc_client_entity_renderer_rain_tick_retire(&call, &error) ==
                      MC_CLIENT_RENDERER_RAIN_OK &&
                  call == NULL && px->holds == held && world->persistent_bindings == 0 &&
                  camera->holds == 1);
        }
    }
    world->fields.j = 0.0f;
    px->phase = MC_CLIENT_ENTITY_RENDERER_RAW;
    CHECK(mc_client_entity_renderer_rain_tick_begin(px, &call, &error) ==
              MC_CLIENT_RENDERER_RAIN_OK &&
          mc_client_entity_renderer_rain_tick_inspect(call, &state) && state.source_pc == 35 &&
          px->phase == MC_CLIENT_ENTITY_RENDERER_RAW);
    CHECK(mc_client_entity_renderer_rain_tick_retire(&call, NULL) == MC_CLIENT_RENDERER_RAIN_OK);
    px->phase = MC_CLIENT_ENTITY_RENDERER_CONSTRUCTING;
    CHECK(mc_client_entity_renderer_rain_tick_begin(px, &call, &error) ==
              MC_CLIENT_RENDERER_RAIN_PHASE &&
          call == NULL);
    px->phase = MC_CLIENT_ENTITY_RENDERER_RETIRING;
    CHECK(mc_client_entity_renderer_rain_tick_begin(px, &call, &error) ==
              MC_CLIENT_RENDERER_RAIN_PHASE &&
          call == NULL);
    px->phase = MC_CLIENT_ENTITY_RENDERER_READY;
    px->active = true;
    CHECK(mc_client_entity_renderer_rain_tick_begin(px, &call, &error) ==
              MC_CLIENT_RENDERER_RAIN_BUSY &&
          call == NULL);
    world->fields.j = 0.0f;
    CHECK(entity_renderer_rain_tick_entered(px, &call, &error) == MC_CLIENT_RENDERER_RAIN_OK &&
          px->active && px->holds == 1);
    CHECK(mc_client_entity_renderer_rain_tick_retire(&call, &error) == MC_CLIENT_RENDERER_RAIN_OK &&
          px->active && px->holds == 1);
    px->active = false;
    app->i = NULL;
    world->fields.j = 0.5f;
    CHECK(mc_client_entity_renderer_rain_tick_begin(px, &call, &error) ==
              MC_CLIENT_RENDERER_RAIN_SOURCE_NULL &&
          error.source_pc == 69 && mc_client_entity_renderer_rain_tick_inspect(call, &state) &&
          state.reseeded);
    CHECK(mc_client_entity_renderer_rain_tick_retire(&call, NULL) == MC_CLIENT_RENDERER_RAIN_OK);
    app->settings = NULL;
    CHECK(mc_client_entity_renderer_rain_tick_begin(px, &call, &error) ==
              MC_CLIENT_RENDERER_RAIN_SOURCE_NULL &&
          error.source_pc == 19 && mc_client_entity_renderer_rain_tick_inspect(call, &state) &&
          state.strength_read && !state.reseeded);
    CHECK(mc_client_entity_renderer_rain_tick_retire(&call, NULL) == MC_CLIENT_RENDERER_RAIN_OK);
    CHECK(mc_game_settings_retire(&settings, NULL) == MC_GAME_SETTINGS_OK);
    mc_entity_release(camera);
    CHECK(mc_entity_classes_free(entities) == MC_ENTITY_OK);
    CHECK(mc_world_runtime_free(world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_free(world_classes, NULL) == MC_WORLD_STORAGE_OK);
    mc_java_random_profile_free(profile);
    free(px);
    free(app);
    free(trig);
    printf("%u rain body checks passed\n", checks);
    return 0;
}
