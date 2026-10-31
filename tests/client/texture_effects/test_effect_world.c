/* Retain the genuine existing World/LocalPlayer fixture's alternate entry,
 * but do not execute its unrelated scenarios. No new World/Player proxy. */
#define main mc_effect_unused_player_constructor_main
#include "../../entity/player/test_construction.c"
#undef main
#include "../../../src/world/runtime/runtime_internal.h"
#include "client/application_state.h"
#include "client/texture_effects/effect_internal.h"
#include "fixtures.h"

static bool world_effects(fixture *f) {
    mc_player_string *name = NULL, *token = NULL;
    mc_player_session *session = NULL;
    mc_entity *player = NULL;
    static const uint16_t ascii[] = {'E', 'f', 'f', 'e', 'c', 't'};
    CHECK(mc_player_string_create(ascii, 6, &name) == MC_PLAYER_OK);
    CHECK(mc_player_string_create(NULL, 0, &token) == MC_PLAYER_OK);
    CHECK(mc_player_session_create(f->sessions, name, token, &session, NULL) == MC_PLAYER_OK);
    CHECK(mc_local_player_allocate(f->entities, &player, NULL) == MC_ENTITY_OK);
    CHECK(mc_local_player_construct(player, f->world, NULL, session, 0, NULL, NULL) ==
          MC_PLAYER_OK);
    client_application *app = calloc(1, sizeof(*app));
    CHECK(app != NULL);
    app->active_world = f->world;
    app->h = player;
    const mc_texture_effect_services service = {f->registry, f->profile, &trig, f->files,
                                                mc_resource_files_source(f->files)};
    mc_texture_effect_error *e = calloc(1, sizeof(*e));
    CHECK(e != NULL);
    mc_client_texture_effect *compass = NULL, *watch = NULL;
    CHECK(mc_client_texture_effect_allocate(&service, MC_TEXTURE_EFFECT_COMPASS, NULL, &compass,
                                            e) == MC_TEXTURE_EFFECT_OK);
    CHECK(mc_client_texture_effect_construct(compass, app, 0, e) == MC_TEXTURE_EFFECT_OK);
    CHECK(mc_client_texture_effect_allocate(&service, MC_TEXTURE_EFFECT_WATCH, NULL, &watch, e) ==
          MC_TEXTURE_EFFECT_OK);
    CHECK(mc_client_texture_effect_construct(watch, app, 0, e) == MC_TEXTURE_EFFECT_OK);
    CHECK(mc_world_info_spawn_set(mc_world_runtime_info(f->world), 100, 64, 0) == MC_WORLD_INFO_OK);
    CHECK(mc_entity_set_position(player, 0, 64, 0) == MC_ENTITY_OK);
    player->base.yaw = 90.0f;
    const uint64_t before = atomic_load(&f->profile->math->state);
    CHECK(mc_client_texture_effect_tick(compass, e) == MC_TEXTURE_EFFECT_OK);
    mc_texture_effect_state state;
    CHECK(mc_client_texture_effect_inspect(compass, &state));
    CHECK(state.angle == 0.0 && state.velocity == 0.0);
    CHECK(mc_world_info_spawn_set(mc_world_runtime_info(f->world), 0, 64, 100) == MC_WORLD_INFO_OK);
    CHECK(mc_client_texture_effect_tick(compass, e) == MC_TEXTURE_EFFECT_OK);
    CHECK(mc_client_texture_effect_inspect(compass, &state) && state.angle == ed_mul(-0.1, 0.8));
    CHECK(atomic_load(&f->profile->math->state) == before);
    CHECK(mc_world_info_time_set(mc_world_runtime_info(f->world), 6000) == MC_WORLD_INFO_OK);
    CHECK(mc_client_texture_effect_tick(watch, e) == MC_TEXTURE_EFFECT_OK);
    CHECK(mc_client_texture_effect_inspect(watch, &state) && state.angle < 0.0 &&
          state.icon != state.dial);
    CHECK(mc_world_info_time_set(mc_world_runtime_info(f->world), 18000) == MC_WORLD_INFO_OK);
    CHECK(mc_client_texture_effect_tick(watch, e) == MC_TEXTURE_EFFECT_OK);
    CHECK(mc_client_texture_effect_inspect(watch, &state) && state.angle > 0.0);
    CHECK(atomic_load(&f->profile->math->state) == before);
    mc_world_provider *nether = NULL;
    CHECK(mc_world_provider_create(-1, NULL, &nether, NULL) == MC_WORLD_PROVIDER_OK);
    CHECK(mc_world_provider_bind(nether, f->world, NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_provider *normal = f->world->fields.t;
    f->world->fields.t = nether;
    mc_java_random oracle = {.state = atomic_load(&f->profile->math->state)};
    (void)mc_java_random_next_double(&oracle);
    (void)mc_java_random_next_double(&oracle);
    CHECK(mc_client_texture_effect_tick(compass, e) == MC_TEXTURE_EFFECT_OK);
    CHECK(mc_client_texture_effect_tick(watch, e) == MC_TEXTURE_EFFECT_OK);
    CHECK(atomic_load(&f->profile->math->state) == oracle.state);
    f->world->fields.t = normal;
    mc_world_provider_unbind(nether);
    mc_world_provider_release(nether);
    app->active_world = NULL;
    CHECK(mc_client_texture_effect_tick(compass, e) == MC_TEXTURE_EFFECT_OK);
    CHECK(mc_client_texture_effect_tick(watch, e) == MC_TEXTURE_EFFECT_OK);
    CHECK(atomic_load(&f->profile->math->state) == oracle.state);
    app->constructor_phase = MC_CLIENT_APP_RETIRING;
    CHECK(mc_client_texture_effect_tick(compass, e) == MC_TEXTURE_EFFECT_INVALID_PHASE);
    CHECK(mc_client_texture_effect_tick(watch, e) == MC_TEXTURE_EFFECT_INVALID_PHASE);
    app->constructor_phase = MC_CLIENT_APP_RAW;
    CHECK(mc_client_texture_effect_release(&compass, e) == MC_TEXTURE_EFFECT_OK);
    CHECK(mc_client_texture_effect_release(&watch, e) == MC_TEXTURE_EFFECT_OK);
    CHECK(mc_client_texture_effect_allocate(&service, MC_TEXTURE_EFFECT_COMPASS, NULL, &compass,
                                            e) == MC_TEXTURE_EFFECT_OK);
    CHECK(mc_client_texture_effect_construct(compass, NULL, 0, e) == MC_TEXTURE_EFFECT_OK);
    CHECK(mc_client_texture_effect_tick(compass, e) == MC_TEXTURE_EFFECT_NULL_REFERENCE &&
          e->source_pc == 197);
    CHECK(mc_client_texture_effect_release(&compass, e) == MC_TEXTURE_EFFECT_OK);
    CHECK(mc_client_texture_effect_allocate(&service, MC_TEXTURE_EFFECT_WATCH, NULL, &watch, e) ==
          MC_TEXTURE_EFFECT_OK);
    CHECK(mc_client_texture_effect_construct(watch, NULL, 0, e) == MC_TEXTURE_EFFECT_OK);
    CHECK(mc_client_texture_effect_tick(watch, e) == MC_TEXTURE_EFFECT_NULL_REFERENCE &&
          e->source_pc == 6);
    CHECK(mc_client_texture_effect_release(&watch, e) == MC_TEXTURE_EFFECT_OK);
    free(app);
    free(e);
    mc_entity_release(player);
    mc_player_session_retire(&session);
    mc_player_string_release(name);
    mc_player_string_release(token);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 2 || !fx_make(argv[1]) || !mc_math_trig_init(&trig))
        return 2;
    char world[4096];
    if (!fx_path(world, sizeof(world), argv[1], "world"))
        return 2;
    fixture f = {0};
    if (!setup(&f, argv[1], world) || !world_effects(&f) || !teardown(&f))
        return 1;
    printf("Actual effect World/Player controls: %u\n", checks);
    return 0;
}
