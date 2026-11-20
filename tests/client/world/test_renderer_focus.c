/* Prepared component controls on the genuine loaded App graph. Each call is
 * a fresh segment invocation, never a replay of the failed enclosing frame. */
#include "test_renderer_focus.h"
#include "client/application_state.h"
#include "client/display/events.h"
#include "client/display/sdk_input.h"
#include "client/display/staged.h"
#include "client/world/entity_renderer/internal.h"
#include "entity/entity_internal.h"
#include "mc/client/world/renderer_focus.h"

#include <SDL3/SDL.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <xmmintrin.h>

#define CHECK(value)                                                                               \
    do {                                                                                           \
        if (!(value)) {                                                                            \
            fprintf(stderr, "Focus check at%d: %s\n", __LINE__, #value);                           \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static uint32_t bits(const float *value) {
    uint32_t result;
    memcpy(&result, value, sizeof(result));
    return result;
}

static int motion(client_application *app, int32_t dx, int32_t dy) {
    SDL_Event event = {0};
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.windowID = SDL_GetWindowID(mc_client_display_window(app->display));
    event.motion.xrel = (float)dx;
    event.motion.yrel = (float)-dy;
    CHECK(SDL_PushEvent(&event));
    mc_client_events_error events;
    CHECK(mc_client_events_retain_pending(mc_client_display_event_port(app->display), &events) ==
          MC_CLIENT_EVENTS_OK);
    mc_client_sdk_input_error error;
    CHECK(mc_client_sdk_mouse_poll(mc_client_display_inputs(app->display), &error) ==
          MC_CLIENT_SDK_INPUT_OK);
    return 0;
}

int mc_test_renderer_focus(client_application *app) {
    mc_client_source_px *px = app->t;
    mc_entity *player = app->h;
    mc_client_sdk_input *input = mc_client_display_inputs(app->display);
    mc_client_sdk_input_state sdk;
    mc_client_sdk_input_error input_error;
    CHECK(mc_client_sdk_input_inspect(input, &sdk));
    const bool grabbed = sdk.grabbed, focused = app->N;
    const float yaw = player->base.yaw, pitch = player->base.aT;
    const float previous_yaw = player->base.aU, previous_pitch = player->base.aV;
    const mc_player_mouse_filter_fields n = *px->fields.filters[0], o = *px->fields.filters[1];
    mc_game_settings_state settings;
    CHECK(mc_game_settings_read_state(app->settings, &settings));
    const mc_game_settings_values before_settings = settings.values;
    CHECK(mc_client_sdk_mouse_set_grabbed(input, true, &input_error) == MC_CLIENT_SDK_INPUT_OK);
    CHECK(mc_client_sdk_mouse_poll(input, &input_error) == MC_CLIENT_SDK_INPUT_OK);
    int32_t dx, dy;
    CHECK(mc_client_sdk_mouse_take_dx(input, &dx, &input_error) == MC_CLIENT_SDK_INPUT_OK &&
          mc_client_sdk_mouse_take_dy(input, &dy, &input_error) == MC_CLIENT_SDK_INPUT_OK);
    app->N = true;
    static const float sensitivities[] = {0.0f, 0.5f, 1.0f};
    static const int32_t deltas[][2] = {{24, -13},   {-70, 41},   {0, 0},
                                        {200, -200}, {-500, 500}, {1, -1}};
    mc_client_entity_renderer_focus_call *call = NULL;
    mc_client_renderer_focus_error error;
    mc_client_renderer_focus_state state;
    for (int mode = 0; mode < 12; ++mode) {
        settings.values.mouse_sensitivity = sensitivities[mode / 4];
        settings.values.invert_mouse = (mode & 2) != 0;
        settings.values.smooth_camera = (mode & 1) != 0;
        CHECK(mc_game_settings_set_values(app->settings, &settings.values, NULL) ==
              MC_GAME_SETTINGS_OK);
        player->base.yaw = 17.25f;
        player->base.aT = 87.5f;
        player->base.aU = -9.0f;
        player->base.aV = 2.5f;
        *px->fields.filters[0] = (mc_player_mouse_filter_fields){0};
        *px->fields.filters[1] = (mc_player_mouse_filter_fields){0};
        for (int step = 0; step < 6; ++step) {
            CHECK(motion(app, deltas[step][0], deltas[step][1]) == 0);
            CHECK(mc_client_entity_renderer_focus_begin(px, input, &call, &error) ==
                      MC_CLIENT_RENDERER_FOCUS_OK &&
                  mc_client_entity_renderer_focus_inspect(call, &state) &&
                  state.segment_completed && state.source_pc == 194 && state.motion_returned &&
                  state.turn_returned && state.dx == deltas[step][0] &&
                  state.dy == deltas[step][1] && state.n_returned == ((mode & 1) != 0) &&
                  state.o_returned == ((mode & 1) != 0));
            CHECK(mc_client_sdk_mouse_take_dx(input, &dx, &input_error) == MC_CLIENT_SDK_INPUT_OK &&
                  mc_client_sdk_mouse_take_dy(input, &dy, &input_error) == MC_CLIENT_SDK_INPUT_OK);
            mc_player_mouse_filter_fields *a = px->fields.filters[0], *b = px->fields.filters[1];
            printf("FOCUS %d %d %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %08" PRIx32
                   " %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %08" PRIx32
                   " %d %d %d %d\n",
                   mode, step, bits(&player->base.yaw), bits(&player->base.aT),
                   bits(&player->base.aU), bits(&player->base.aV), bits(&a->a), bits(&a->b),
                   bits(&a->c), bits(&b->a), bits(&b->b), bits(&b->c), state.dx, state.dy, dx, dy);
            mc_client_entity_renderer_focus_call *blocked = NULL;
            CHECK(mc_client_entity_renderer_focus_begin(px, input, &blocked, &error) ==
                      MC_CLIENT_RENDERER_FOCUS_BUSY &&
                  blocked == NULL && px->holds >= 2);
            CHECK(mc_client_entity_renderer_focus_retire(&call, &error) ==
                      MC_CLIENT_RENDERER_FOCUS_OK &&
                  call == NULL);
        }
    }
    /* Unfocused branch must not inspect any of the nullable child fields. */
    mc_game_settings *actual_settings = app->settings;
    mc_client_source_vy *helper = app->C;
    app->N = false;
    app->settings = NULL;
    app->C = NULL;
    app->h = NULL;
    CHECK(mc_client_entity_renderer_focus_begin(px, NULL, &call, &error) ==
              MC_CLIENT_RENDERER_FOCUS_OK &&
          mc_client_entity_renderer_focus_inspect(call, &state) && state.segment_completed &&
          !state.focused && !state.motion_called && state.source_pc == 194);
    CHECK(mc_client_entity_renderer_focus_retire(&call, NULL) == MC_CLIENT_RENDERER_FOCUS_OK);
    app->N = true;
    CHECK(mc_client_entity_renderer_focus_begin(px, input, &call, &error) ==
              MC_CLIENT_RENDERER_FOCUS_SOURCE_NULL &&
          error.site.pc == 55 && !error.native_policy);
    CHECK(mc_client_entity_renderer_focus_retire(&call, NULL) == MC_CLIENT_RENDERER_FOCUS_OK);
    app->C = helper;
    CHECK(motion(app, 9, -7) == 0);
    CHECK(mc_client_entity_renderer_focus_begin(px, input, &call, &error) ==
              MC_CLIENT_RENDERER_FOCUS_SOURCE_NULL &&
          error.site.pc == 65 && mc_client_entity_renderer_focus_inspect(call, &state) &&
          state.motion_returned && !state.sensitivity_read);
    CHECK(mc_client_sdk_mouse_take_dx(input, &dx, &input_error) == MC_CLIENT_SDK_INPUT_OK &&
          mc_client_sdk_mouse_take_dy(input, &dy, &input_error) == MC_CLIENT_SDK_INPUT_OK &&
          dx == 0 && dy == 0);
    CHECK(mc_client_entity_renderer_focus_retire(&call, NULL) == MC_CLIENT_RENDERER_FOCUS_OK);
    app->settings = actual_settings;
    settings.values.smooth_camera = true;
    CHECK(mc_game_settings_set_values(app->settings, &settings.values, NULL) ==
          MC_GAME_SETTINGS_OK);
    mc_player_mouse_filter_fields *actual_o = px->fields.filters[1];
    px->fields.filters[1] = NULL;
    CHECK(motion(app, 3, 4) == 0);
    CHECK(mc_client_entity_renderer_focus_begin(px, input, &call, &error) ==
              MC_CLIENT_RENDERER_FOCUS_SOURCE_NULL &&
          error.site.pc == 171 && mc_client_entity_renderer_focus_inspect(call, &state) &&
          state.n_returned && !state.o_returned && !state.turn_called);
    CHECK(mc_client_entity_renderer_focus_retire(&call, NULL) == MC_CLIENT_RENDERER_FOCUS_OK);
    px->fields.filters[1] = actual_o;
    CHECK(motion(app, 3, 4) == 0);
    CHECK(mc_client_entity_renderer_focus_begin(px, input, &call, &error) ==
              MC_CLIENT_RENDERER_FOCUS_SOURCE_NULL &&
          error.site.pc == 191 && mc_client_entity_renderer_focus_inspect(call, &state) &&
          state.n_returned && state.o_returned && !state.turn_called);
    px->active = true;
    CHECK(mc_client_entity_renderer_focus_retire(&call, &error) == MC_CLIENT_RENDERER_FOCUS_BUSY &&
          call != NULL);
    px->active = false;
    CHECK(mc_client_entity_renderer_focus_retire(&call, NULL) == MC_CLIENT_RENDERER_FOCUS_OK);
    app->h = player;
    CHECK(motion(app, 3, 4) == 0);
    const unsigned int csr = _mm_getcsr();
    _mm_setcsr(csr | 0x2000u);
    const mc_client_renderer_focus_status fp =
        mc_client_entity_renderer_focus_begin(px, input, &call, &error);
    _mm_setcsr(csr);
    CHECK(fp == MC_CLIENT_RENDERER_FOCUS_UNSUPPORTED_FP && error.site.pc == 70 &&
          mc_client_entity_renderer_focus_inspect(call, &state) && state.motion_returned &&
          state.sensitivity_read && !state.turn_called);
    CHECK(mc_client_entity_renderer_focus_retire(&call, NULL) == MC_CLIENT_RENDERER_FOCUS_OK);
    player->base.yaw = yaw;
    player->base.aT = pitch;
    player->base.aU = previous_yaw;
    player->base.aV = previous_pitch;
    *px->fields.filters[0] = n;
    *px->fields.filters[1] = o;
    CHECK(mc_game_settings_set_values(app->settings, &before_settings, NULL) ==
          MC_GAME_SETTINGS_OK);
    app->N = focused;
    CHECK(mc_client_sdk_mouse_set_grabbed(input, grabbed, &input_error) == MC_CLIENT_SDK_INPUT_OK);
    puts("RENDERER_FOCUS original-histories=72 unfocused=1 consumed-prefix=1 filter-prefix=1 "
         "turn-null=1 fp-prefix=1 retryable-cleanup=1 segment=194");
    return 0;
}
