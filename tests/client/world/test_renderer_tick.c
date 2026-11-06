/* Prepared body receivers; constructors and a complete graphical frame are outside scope. */
#include "client/application_state.h"
#include "client/view_slot.h"
#include "client/world/entity_renderer/internal.h"
#include "entity/player/player_internal.h"
#include "mc/client/world/renderer_tick.h"
#include "world/runtime/runtime_internal.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "Renderer tick check%u at%d: %s\n", checks, __LINE__, #value);         \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

static uint32_t bits(const float *value) {
    uint32_t result;
    memcpy(&result, value, sizeof(result));
    return result;
}

static void install(float *value, uint32_t raw) {
    memcpy(value, &raw, sizeof(raw));
}

typedef struct fixture {
    mc_java_random_profile *profile;
    mc_entity_classes *entities;
    mc_client_render_classes *models;
    mc_client_render_engine_classes *graphics;
    mc_math_classes *math;
    mc_world_runtime_classes *world_classes;
    mc_world_runtime *world;
    mc_world_provider *dimension;
    mc_entity *player;
    mc_inventory_player *inventory;
    mc_game_settings *settings;
    client_application *app;
    mc_client_source_px *px;
    mc_client_held_item *held; /* Borrowed after ownership transfer to PX.pending_ra. */
    mc_client_entity_renderer_tick_call *call;
} fixture;

static bool setup(fixture *f) {
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entities) ==
          MC_ENTITY_OK);
    CHECK(mc_client_render_classes_create(f->entities, f->profile, &f->models) ==
          MC_CLIENT_MODEL_OK);
    CHECK(mc_client_render_engine_classes_create(NULL, &f->graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    const mc_client_entity_renderer_services services = {f->models, f->graphics, f->profile, NULL};
    CHECK(mc_client_entity_renderer_allocate(&services, NULL, &f->px, NULL) ==
          MC_CLIENT_ENTITY_RENDERER_OK);
    CHECK(mc_client_held_item_allocate(&services, NULL, &f->held, NULL) ==
          MC_CLIENT_ENTITY_RENDERER_OK);
    f->px->pending_ra = f->held;
    CHECK(mc_math_classes_create(SIZE_MAX, &f->math, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_math_classes_ensure(f->math, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile,
                                          mc_math_classes_table(f->math), &f->world_classes,
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, f->world_classes,
                                          NULL, &f->world, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_provider_create(0, NULL, &f->dimension, NULL) == MC_WORLD_PROVIDER_OK);
    CHECK(mc_world_provider_retain(f->dimension) == MC_WORLD_PROVIDER_OK);
    f->world->fields.t = f->dimension;
    mc_world_provider_fields dimension;
    CHECK(mc_world_provider_fields_get(f->dimension, &dimension) == MC_WORLD_PROVIDER_OK);
    install(&((float *)dimension.f)[15], UINT32_C(0x3e99999a));
    CHECK(mc_local_player_allocate(f->entities, &f->player, NULL) == MC_ENTITY_OK);
    CHECK(mc_inventory_player_create(MC_REGISTRY_CLIENT, NULL, NULL, &f->inventory, NULL) ==
          MC_INVENTORY_OK);
    f->player->local_player->fields.player.c = f->inventory;
    f->player->base.x = 32000000.0;
    f->player->base.y = 64.0;
    f->player->base.z = -2.0;
    CHECK(mc_game_settings_create_defaults(NULL, &f->settings, NULL) == MC_GAME_SETTINGS_OK);
    f->app = calloc(1, sizeof(*f->app));
    CHECK(f->app != NULL);
    return true;
}

static bool one(fixture *f, int mode, size_t index, int32_t distance, uint32_t m) {
    mc_client_entity_renderer_fields *fields = &f->px->fields;
    mc_client_source_ra *held = f->held->source;
    CHECK(mc_client_view_slot_store(f->app, NULL) == MC_ENTITY_OK);
    f->app->h = f->player;
    f->app->active_world = f->world;
    f->app->settings = f->settings;
    if ((index & 1u) != 0)
        CHECK(mc_client_view_slot_store(f->app, f->player) == MC_ENTITY_OK);
    fields->j = f->app;
    fields->c = held;
    held->a = f->app;
    held->g = -1;
    install(&held->c, UINT32_C(0x3f000000));
    install(&held->d, UINT32_C(0x7fc23456));
    install(&fields->M, m);
    install(&fields->L, UINT32_C(0x7fc23456));
    install(&fields->t, UINT32_C(0x7fa12345));
    install(&fields->u, UINT32_C(0x3f800000));
    install(&fields->v_to_C[0], UINT32_C(0x80000000));
    install(&fields->v_to_C[2], UINT32_C(1));
    install(&fields->v_to_C[4], UINT32_C(0xff800000));
    install(&fields->v_to_C[6], UINT32_C(0x7fc12345));
    fields->l = (index & 1u) == 0 ? INT32_MAX : -1;
    f->player->local_player->fields.player.c = f->inventory;
    mc_game_settings_state settings;
    CHECK(mc_game_settings_read_state(f->settings, &settings));
    settings.values.render_distance = distance;
    settings.values.fancy_graphics = true;
    CHECK(mc_game_settings_set_values(f->settings, &settings.values, NULL) == MC_GAME_SETTINGS_OK);
    if (mode == 1)
        fields->j = NULL;
    if (mode == 2)
        f->app->active_world = NULL;
    if (mode == 3) {
        CHECK(mc_client_view_slot_store(f->app, NULL) == MC_ENTITY_OK);
        f->app->h = NULL;
    }
    if (mode == 4)
        f->app->settings = NULL;
    if (mode == 5)
        fields->c = NULL;
    if (mode == 6)
        held->a = NULL;
    if (mode == 7) {
        CHECK(mc_client_view_slot_store(f->app, f->player) == MC_ENTITY_OK);
        f->app->h = NULL;
    }
    if (mode == 8)
        f->player->local_player->fields.player.c = NULL;
    mc_client_renderer_tick_error error;
    const mc_client_renderer_tick_status status =
        mc_client_entity_renderer_tick_begin(f->px, f->math, &f->call, &error);
    mc_client_renderer_tick_state state;
    CHECK(f->call != NULL && mc_client_entity_renderer_tick_inspect(f->call, &state) &&
          state.interpolation_stores == 6 && !state.active && bits(&fields->L) == m);
    if (mode == 0) {
        CHECK(status == MC_CLIENT_RENDERER_TICK_OK && state.source_returned &&
              state.source_pc == 186 && state.brightness_returned && state.brightness == 0.3f &&
              state.held_item_returned && state.rain_returned && state.rain != NULL &&
              state.rain->source_pc == 35 && !state.rain->reseeded);
    } else {
        static const unsigned int pcs[] = {0, 52, 118, 86, 130, 179, 179, 179, 179};
        CHECK(status == (mode < 6 ? MC_CLIENT_RENDERER_TICK_SOURCE_NULL
                                  : MC_CLIENT_RENDERER_TICK_CHILD_FAILED) &&
              !state.source_returned && error.site.pc == (int32_t)pcs[mode] &&
              !error.native_policy);
        if (mode >= 6)
            CHECK(error.child.pc == (mode == 6 ? 12 : mode == 7 ? 17 : 20));
    }
    printf("RTICK %d %zu %s %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %08" PRIx32
           " %08" PRIx32 " %08" PRIx32 " %d %d %08" PRIx32 " %08" PRIx32 " %d\n",
           mode, index, mode == 0 ? "OK" : "NULL", bits(&fields->L), bits(&fields->M),
           bits(&fields->u), bits(&fields->v_to_C[1]), bits(&fields->v_to_C[3]),
           bits(&fields->v_to_C[5]), bits(&fields->v_to_C[7]), fields->l, f->app->i == f->player,
           bits(&held->c), bits(&held->d), held->g);
    mc_client_entity_renderer_tick_call *same = f->call;
    const size_t holds = f->px->holds;
    CHECK(mc_client_entity_renderer_tick_begin(f->px, f->math, &f->call, &error) ==
              MC_CLIENT_RENDERER_TICK_ARGUMENT &&
          f->call == same && f->px->holds == holds);
    CHECK(mc_client_entity_renderer_retire(&f->px, NULL) == MC_CLIENT_ENTITY_RENDERER_BUSY);
    CHECK(mc_client_entity_renderer_tick_retire(&f->call, NULL) == MC_CLIENT_RENDERER_TICK_OK &&
          f->call == NULL && f->px->holds == 1 && f->px->tick_call == NULL &&
          f->world->active_calls == 0 && f->world->persistent_bindings == 0);
    CHECK(mc_client_view_slot_store(f->app, NULL) == MC_ENTITY_OK && f->player->holds == 1);
    return true;
}

static bool run(fixture *f) {
    static const int32_t distances[] = {0, 1, 2, 3, -1, INT32_MAX, INT32_MIN};
    static const uint32_t inputs[] = {0, UINT32_C(0x80000000), UINT32_C(0x3e4ccccd),
                                      UINT32_C(0xbf800000), UINT32_C(0x41200000)};
    for (size_t d = 0; d < sizeof(distances) / sizeof(distances[0]); ++d)
        for (size_t index = 0; index < sizeof(inputs) / sizeof(inputs[0]); ++index)
            CHECK(one(f, 0, d * 5u + index, distances[d], inputs[index]));
    for (int mode = 1; mode <= 8; ++mode)
        CHECK(one(f, mode, 0, 2, UINT32_C(0x3e4ccccd)));
    return true;
}

static bool close_fixture(fixture *f) {
    CHECK(mc_client_entity_renderer_tick_retire(&f->call, NULL) == MC_CLIENT_RENDERER_TICK_OK);
    CHECK(mc_client_view_slot_store(f->app, NULL) == MC_ENTITY_OK);
    if (f->player != NULL)
        f->player->local_player->fields.player.c = NULL;
    CHECK(mc_inventory_player_retire(&f->inventory, NULL) == MC_INVENTORY_OK);
    mc_entity_release(f->player);
    CHECK(mc_client_entity_renderer_retire(&f->px, NULL) == MC_CLIENT_ENTITY_RENDERER_OK);
    CHECK(mc_game_settings_retire(&f->settings, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_world_runtime_free(f->world, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_provider_release(f->dimension);
    CHECK(mc_world_runtime_classes_free(f->world_classes, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_math_classes_release(&f->math, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_client_render_engine_classes_release(f->graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_classes_free(f->models) == MC_CLIENT_MODEL_OK);
    CHECK(mc_entity_classes_free(f->entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(f->profile);
    free(f->app);
    return true;
}

int main(void) {
    fixture *f = calloc(1, sizeof(*f));
    if (f == NULL)
        return 1;
    const bool okay = setup(f) && run(f);
    if (!close_fixture(f))
        return 1;
    free(f);
    if (!okay)
        return 1;
    printf("%u renderer tick checks passed\n", checks);
    return 0;
}
