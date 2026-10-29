#include "gameplay_input.h"

#include "../application_state.h"
#include "../display/staged.h"
#include "../gui/source_screen_transitions.h"
#include "mc/core/java/java.h"
#include "mc/entity/hit_result.h"
#include "mc/world/runtime/construction.h"
#include "millis.h"

#include <string.h>
#include <xmmintrin.h>

static bool fail(mc_client_gameplay_input *call, const char *operation, const char *class_name,
                 const char *method, const char *descriptor, unsigned int pc) {
    call->failed = true;
    call->operation = operation;
    call->site = (mc_client_screen_site){class_name, method, descriptor, pc};
    return false;
}

static bool pending(mc_client_gameplay_input *call, const char *operation, unsigned int pc) {
    return fail(call, operation, "net/minecraft/client/Minecraft", "k", "()V", pc);
}

static bool sdk(mc_client_gameplay_input *call, mc_client_sdk_input_status status,
                unsigned int pc) {
    return status == MC_CLIENT_SDK_INPUT_OK || pending(call, call->input.operation, pc);
}

static bool settings_read(client_application *app, mc_client_gameplay_input *call,
                          mc_game_settings_state *out, unsigned int pc) {
    return mc_game_settings_read_state(app->settings, out) ||
           pending(call, "actual gameplay Settings field access", pc);
}

static bool binding(client_application *app, mc_client_gameplay_input *call, size_t index,
                    int32_t *out, unsigned int settings_pc, unsigned int code_pc) {
    mc_game_settings_state state;
    mc_game_settings_text name;
    if (!settings_read(app, call, &state, settings_pc))
        return false;
    return mc_key_binding_read(state.named[index], &name, out) ||
           pending(call, "actual gameplay null KeyBinding receiver", code_pc);
}

static bool inventory(client_application *app, mc_client_gameplay_input *call,
                      mc_inventory_player **out, unsigned int pc) {
    mc_local_player_snapshot player;
    if (mc_local_player_get_snapshot(app->h, &player) != MC_ENTITY_OK)
        return pending(call, "actual gameplay Player inventory receiver", pc);
    *out = player.player.c;
    return true;
}

static bool screen_result(mc_client_gameplay_input *call, mc_client_screen_status status) {
    if (status == MC_CLIENT_SCREEN_OK)
        return true;
    const mc_client_screen_error *e = &call->screen.screen;
    return fail(call, e->operation, e->site.class_name, e->site.method, e->site.descriptor,
                e->site.pc);
}

static bool mouse_event(client_application *app, mc_client_screen_transitions *transitions,
                        mc_client_gameplay_input *call, const mc_client_sdk_mouse_event *event) {
    int64_t now;
    if (mc_platform_current_time_millis(&now) != MC_PLATFORM_MILLIS_OK)
        return pending(call, "actual gameplay currentTimeMillis query", 343);
    if (mc_java_i64_sub(now, app->P) > 200) {
        ++call->stale_mouse_events;
        return true;
    }
    if (event->wheel != 0) {
        mc_inventory_player *slots;
        if (!inventory(app, call, &slots, 375))
            return false;
        if (mc_inventory_player_scroll(slots, event->wheel, &call->inventory) != MC_INVENTORY_OK)
            return pending(call, call->inventory.operation, 379);
        if (mc_game_settings_source_input_wheel(app->settings, event->wheel, &call->wheel,
                                                &call->settings) != MC_GAME_SETTINGS_OK)
            return pending(call, call->settings.operation, 386);
    }
    if (app->r == NULL) {
        if (!app->N && event->pressed) {
            const mc_client_display_screen_status status = mc_client_display_focus_run(
                app, mc_client_screen_transitions_classes(transitions), &call->focus);
            if (status != MC_CLIENT_DISPLAY_SCREEN_OK) {
                if (call->focus == NULL)
                    return pending(call, "actual gameplay focus native admission", 442);
                return fail(call, call->focus->operation, call->focus->site.class_name,
                            call->focus->site.method, call->focus->site.descriptor,
                            call->focus->site.pc);
            }
            if (mc_client_display_screen_retire(&call->focus) != MC_CLIENT_DISPLAY_SCREEN_OK)
                return pending(call, "actual gameplay focus native retirement", 442);
            return true; /* Source goto533 skips click after focus. */
        }
        if (event->pressed && event->button == 0)
            return pending(call, "actual left click is not connected", 462);
        if (event->pressed && event->button == 1)
            return pending(call, "actual right click is not connected", 488);
        if (event->pressed && event->button == 2)
            return pending(call, "actual pick-block action is not connected", 513);
    } else if (!screen_result(call,
                              mc_client_screen_mouse_event_with_transitions(
                                  app->r, event, transitions, &call->screen_report, &call->screen)))
        return false;
    return true;
}

static bool toggle(client_application *app, mc_client_gameplay_input *call, int32_t key) {
    mc_game_settings_status status = MC_GAME_SETTINGS_OK;
    unsigned int pc = 0;
    switch (key) {
        case 59:
            pc = 657;
            status = mc_game_settings_source_input_toggle_hide_gui(app->settings, &call->settings);
            break;
        case 61:
            pc = 687;
            status = mc_game_settings_source_input_toggle_debug(app->settings, &call->settings);
            break;
        case 63:
            pc = 717;
            status =
                mc_game_settings_source_input_toggle_third_person(app->settings, &call->settings);
            break;
        case 66:
            pc = 747;
            status =
                mc_game_settings_source_input_toggle_smooth_camera(app->settings, &call->settings);
            break;
    }
    return status == MC_GAME_SETTINGS_OK || pending(call, call->settings.operation, pc);
}

static bool key_event(client_application *app, mc_client_screen_transitions *transitions,
                      mc_client_gameplay_input *call, const mc_client_sdk_key_event *event) {
    if (mc_local_player_input_key(app->h, event->key, event->pressed, &call->player) !=
        MC_PLAYER_OK)
        return fail(call, call->player.operation, call->player.site.class_name,
                    call->player.site.method, call->player.site.descriptor, call->player.site.pc);
    if (!event->pressed)
        return true;
    if (event->key == 87)
        return pending(call, "actual fullscreen toggle is not connected", 587);
    mc_client_sdk_input *input = mc_client_display_inputs(app->display);
    if (app->r != NULL) {
        if (!screen_result(call,
                           mc_client_screen_key_event_with_transitions(
                               app->r, event, transitions, &call->screen_report, &call->screen)))
            return false;
    } else {
        if (event->key == 1)
            return pending(call, "actual pause-screen entry is not connected", 618);
        if (event->key == 31) {
            bool down;
            if (!sdk(call, mc_client_sdk_keyboard_key_down(input, 61, &down, &call->input), 631))
                return false;
            if (down)
                return pending(call, "actual sound reload is not connected", 638);
        }
        if (!toggle(app, call, event->key))
            return false;
        int32_t code;
        if (!binding(app, call, 5, &code, 768, 771))
            return false;
        if (event->key == code)
            return pending(call, "actual Inventory screen allocation is not connected", 778);
        if (!binding(app, call, 6, &code, 799, 802))
            return false;
        if (event->key == code)
            return pending(call, "actual Player drop action is not connected", 812);
        if (app->active_world != NULL) {
            mc_world_runtime_fields world;
            if (mc_world_runtime_fields_get(app->active_world, &world) != MC_WORLD_STORAGE_OK)
                return pending(call, "actual multiplayer World field access", 816);
            if (world.B) {
                if (!binding(app, call, 7, &code, 829, 832))
                    return false;
                if (event->key == code)
                    return pending(call, "actual Chat screen allocation is not connected", 839);
            }
        }
    }
    if (event->key >= 2 && event->key <= 10) {
        mc_inventory_player *slots;
        if (!inventory(app, call, &slots, 870))
            return false;
        if (mc_inventory_player_select_hotbar(slots, event->key - 2, &call->inventory) !=
            MC_INVENTORY_OK)
            return pending(call, call->inventory.operation, 874);
        ++call->hotbar_stores;
    }
    int32_t code;
    if (!binding(app, call, 8, &code, 890, 893))
        return false;
    if (event->key == code) {
        bool left, right = false;
        if (!sdk(call, mc_client_sdk_keyboard_key_down(input, 42, &left, &call->input), 908))
            return false;
        if (!left &&
            !sdk(call, mc_client_sdk_keyboard_key_down(input, 54, &right, &call->input), 916))
            return false;
        if (mc_game_settings_set_option(app->settings,
                                        mc_game_option_by_id(MC_GAME_OPTION_RENDER_DISTANCE),
                                        left || right ? -1 : 1, NULL, &call->option,
                                        &call->settings) != MC_GAME_SETTINGS_OK)
            return pending(call, call->settings.operation, 927);
    }
    return true;
}

static bool held_click(client_application *app, mc_client_gameplay_input *call, int32_t button,
                       unsigned int query_pc, unsigned int timer_pc, unsigned int click_pc) {
    bool down;
    if (!sdk(call,
             mc_client_sdk_mouse_button_down(mc_client_display_inputs(app->display), button, &down,
                                             &call->input),
             query_pc))
        return false;
    if (!down)
        return true;
    mc_timer_view timer;
    if (!mc_timer_view_get(app->timer, &timer))
        return pending(call, "actual held-click Timer receiver", timer_pc);
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return pending(call, "held-click native floating environment", timer_pc);
    const float elapsed =
        _mm_cvtss_f32(_mm_cvtsi32_ss(_mm_setzero_ps(), mc_java_i32_sub(app->V, app->ag)));
    const float delay =
        _mm_cvtss_f32(_mm_div_ss(_mm_set_ss(timer.ticks_per_second), _mm_set_ss(4.0f)));
    if (elapsed >= delay && app->N)
        return pending(call, "actual held click is not connected", click_pc);
    return true;
}

static bool mining(client_application *app, mc_client_gameplay_input *call, bool held) {
    mc_singleplayer_controller_snapshot controller;
    if (!mc_client_singleplayer_controller_read(app->controller, &controller))
        return fail(call, "actual mining controller receiver", "net/minecraft/client/Minecraft",
                    "a", "(IZ)V", 4);
    if (controller.b)
        return true;
    if (!held)
        app->W = 0;
    if (app->W > 0)
        return true;
    if (held && app->y != NULL) {
        mc_entity_hit_result_view hit;
        mc_entity_hit_kind_class_view kinds;
        if (mc_entity_hit_result_inspect(app->y, &hit) != MC_ENTITY_HIT_OK ||
            mc_entity_hit_kind_class_inspect(app->entities, &kinds) != MC_ENTITY_HIT_OK ||
            kinds.phase != MC_ENTITY_CLASS_READY)
            return fail(call, "actual mining HitResult fields", "net/minecraft/client/Minecraft",
                        "a", "(IZ)V", 47);
        if (hit.fields.a == kinds.a)
            return fail(call, "actual progressive block mining is not connected",
                        "net/minecraft/client/Minecraft", "a", "(IZ)V", 102);
    }
    if (mc_client_singleplayer_controller_reset(app->controller, &call->controller) !=
        MC_SINGLEPLAYER_TICK_OK)
        return fail(call, call->controller.operation, "os", "a", "()V", 0);
    return true;
}

static bool run(client_application *app, mc_client_screen_transitions *transitions,
                mc_client_gameplay_input *call) {
    mc_client_sdk_input *input = mc_client_display_inputs(app->display);
    for (;;) {
        bool next;
        mc_client_sdk_mouse_event event;
        call->source_pc = 337;
        if (!sdk(call, mc_client_sdk_mouse_next(input, &next, &event, &call->input), 337))
            return false;
        if (!next)
            break;
        ++call->mouse_events;
        if (!mouse_event(app, transitions, call, &event))
            return false;
    }
    if (app->W > 0)
        app->W = mc_java_i32_sub(app->W, 1);
    for (;;) {
        bool next;
        mc_client_sdk_key_event event;
        call->source_pc = 553;
        if (!sdk(call, mc_client_sdk_keyboard_next(input, &next, &event, &call->input), 553))
            return false;
        if (!next)
            break;
        ++call->keyboard_events;
        if (!key_event(app, transitions, call, &event))
            return false;
    }
    if (app->r == NULL &&
        (!held_click(app, call, 0, 941, 961, 980) || !held_click(app, call, 1, 992, 1012, 1031)))
        return false;
    bool held = false;
    if (app->r == NULL) {
        if (!sdk(call, mc_client_sdk_mouse_button_down(input, 0, &held, &call->input), 1052))
            return false;
        held = held && app->N;
    }
    if (!mining(app, call, held))
        return false;
    call->source_pc = 1073;
    call->returned = true;
    return true;
}

bool mc_client_gameplay_input_run(client_application *app,
                                  mc_client_screen_transitions *transitions,
                                  mc_client_gameplay_input *call) {
    if (app == NULL || transitions == NULL || call == NULL || call->active || call->failed ||
        call->focus != NULL || call->screen.pending_receiver != NULL)
        return false;
    memset(call, 0, sizeof(*call));
    call->active = true;
    const bool result = run(app, transitions, call);
    call->active = false;
    return result;
}

bool mc_client_gameplay_input_retire(mc_client_gameplay_input *call) {
    if (call == NULL || call->active)
        return false;
    if (mc_client_screen_event_retire(&call->screen) != MC_CLIENT_SCREEN_OK ||
        mc_client_display_screen_retire(&call->focus) != MC_CLIENT_DISPLAY_SCREEN_OK)
        return false;
    return true;
}
