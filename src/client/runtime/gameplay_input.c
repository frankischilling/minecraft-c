#include "gameplay_input.h"

#include "../application_state.h"
#include "../display/staged.h"
#include "../gui/source_screen_transitions.h"
#include "mc/core/java/java.h"
#include "mc/entity/hit_result.h"
#include "mc/world/runtime/construction.h"
#include "mc/world/runtime/placement.h"
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

static bool retire_drop(mc_client_gameplay_input *call) {
    if (mc_local_player_drop_retire(&call->drop_call, &call->drop_cleanup) != MC_PLAYER_DROP_OK)
        return false;
    call->drop_state.player = call->drop_state.item = NULL;
    call->drop_state.stack = NULL;
    call->drop_state.error = call->drop_state.cleanup = NULL;
    call->drop_error.publication = call->drop_cleanup.publication = NULL;
    call->drop_error.statistic = call->drop_cleanup.statistic = NULL;
    return true;
}

static bool drop(client_application *app, mc_client_gameplay_input *call) {
    const mc_player_drop_status status = mc_local_player_drop_selected_begin(
        app->registry, app->source_math_classes, app->h, &call->drop_call, &call->drop_error);
    if (call->drop_call != NULL)
        (void)mc_local_player_drop_inspect(call->drop_call, &call->drop_state);
    if (status != MC_PLAYER_DROP_OK) {
        const mc_player_drop_error *e = &call->drop_error;
        if (e->site.class_name == NULL)
            return pending(call, e->operation, 812);
        return fail(call, e->operation, e->site.class_name, e->site.method, e->site.descriptor,
                    e->site.pc);
    }
    ++call->drop_calls;
    call->drop_publications += call->drop_state.publication_returned;
    call->dropped_items += call->drop_state.inserted;
    return retire_drop(call) || pending(call, call->drop_cleanup.operation, 812);
}

static bool retire_attack(mc_client_gameplay_input *call) {
    if (mc_local_player_attack_retire(&call->attack_call, &call->attack_cleanup) != MC_ENTITY_OK)
        return false;
    call->attack_state.player = call->attack_state.target = NULL;
    call->attack_state.error = NULL;
    call->attack_error.effect.death.pending = call->attack_error.effect.death.active = false;
    call->attack_error.effect.death.receiver = NULL;
    call->attack_error.effect.death.drop_error = NULL;
    call->attack_cleanup.effect.death.pending = call->attack_cleanup.effect.death.active = false;
    call->attack_cleanup.effect.death.receiver = NULL;
    call->attack_cleanup.effect.death.drop_error = NULL;
    return true;
}

static bool attack(client_application *app, mc_client_gameplay_input *call, mc_entity *target) {
    if (app->controller == NULL)
        return fail(call, "actual attack null controller invocation",
                    "net/minecraft/client/Minecraft", "a", "(I)V", 87);
    const mc_entity_status status = mc_client_singleplayer_attack_begin(
        app->controller, app->registry, app->source_math_classes, app->h, target,
        &call->attack_call, &call->attack_error);
    if (call->attack_call != NULL)
        (void)mc_local_player_attack_inspect(call->attack_call, &call->attack_state);
    if (status != MC_ENTITY_OK) {
        const mc_player_attack_error *e = &call->attack_error;
        return fail(call, e->operation, e->site.class_name, e->site.method, e->site.descriptor,
                    e->site.pc);
    }
    ++call->attack_calls;
    call->attack_hits += call->attack_state.damage_returned;
    return retire_attack(call) || fail(call, call->attack_cleanup.operation,
                                       "net/minecraft/client/Minecraft", "a", "(I)V", 87);
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

static bool retire_hit(mc_client_gameplay_input *call) {
    if (call->hit_renderer == NULL)
        return !call->hit_block_work;
    if (call->hit_block_work) {
        if (mc_client_effect_renderer_block_cleanup(call->hit_renderer, &call->hit_cleanup) !=
            MC_EFFECT_RENDERER_OK)
            return false;
        /* Drop this right before releasing the producer occurrence. A rejected
         * call, or a later retry after successful cleanup, owns no other work. */
        call->hit_block_work = false;
    }
    if (mc_client_effect_renderer_release(call->hit_renderer) != MC_EFFECT_RENDERER_OK)
        return false;
    call->hit_renderer = NULL;
    call->hit_effect = (mc_effect_renderer_error){0};
    return true;
}

static bool mine(client_application *app, mc_client_gameplay_input *call,
                 mc_singleplayer_mining_action action, int32_t x, int32_t y, int32_t z,
                 int32_t face, unsigned int invoke_pc, const char *descriptor) {
    if (app->controller == NULL)
        return fail(call, "actual mining null controller invocation",
                    "net/minecraft/client/Minecraft", "a", descriptor, invoke_pc);
    const mc_singleplayer_mining_status status = mc_client_singleplayer_mining_begin(
        app->controller, action, x, y, z, face, &call->mining_call, &call->mining_error);
    if (call->mining_call != NULL)
        (void)mc_client_singleplayer_mining_inspect(call->mining_call, &call->mining_state);
    if (status != MC_SINGLEPLAYER_MINING_OK) {
        const mc_singleplayer_mining_error *e = &call->mining_error;
        return fail(call, e->operation, e->site.class_name, e->site.method, e->site.descriptor,
                    e->site.pc);
    }
    if (action == MC_SINGLEPLAYER_MINING_START)
        ++call->mining_starts;
    else if (action == MC_SINGLEPLAYER_MINING_PROGRESS)
        ++call->mining_progress_calls;
    if (call->mining_state.removal_returned && call->mining_state.removed)
        ++call->mined_blocks;
    if (mc_client_singleplayer_mining_retire(&call->mining_call, &call->mining_cleanup) !=
        MC_SINGLEPLAYER_MINING_OK)
        return fail(call, call->mining_cleanup.operation, "net/minecraft/client/Minecraft", "a",
                    descriptor, invoke_pc);
    call->mining_state.error = NULL; /* The per-invocation diagnostic occurrence ended. */
    call->mining_error = (mc_singleplayer_mining_error){0};
    return true;
}

static bool hit_fields(client_application *app, mc_client_gameplay_input *call,
                       mc_entity_hit_result_view *out, mc_entity_hit_kind_class_view *kinds,
                       const char *descriptor, unsigned int pc) {
    if (app->y == NULL)
        return fail(call, "actual click null HitResult field receiver",
                    "net/minecraft/client/Minecraft", "a", descriptor, pc);
    if (mc_entity_hit_result_inspect(app->y, out) != MC_ENTITY_HIT_OK ||
        mc_entity_hit_kind_class_inspect(app->entities, kinds) != MC_ENTITY_HIT_OK ||
        kinds->phase != MC_ENTITY_CLASS_READY)
        return fail(call, "actual click HitResult and kind fields",
                    "net/minecraft/client/Minecraft", "a", descriptor, pc);
    return true;
}

static bool click_fail(mc_client_gameplay_input *call, const char *operation, unsigned int pc) {
    return fail(call, operation, "net/minecraft/client/Minecraft", "a", "(I)V", pc);
}

static bool click_inventory(client_application *app, mc_client_gameplay_input *call,
                            mc_inventory_player **out, unsigned int pc) {
    mc_local_player_snapshot player;
    if (mc_local_player_get_snapshot(app->h, &player) != MC_ENTITY_OK)
        return click_fail(call, "actual click Player inventory field receiver", pc);
    *out = player.player.c;
    return true;
}

static bool click_current(client_application *app, mc_client_gameplay_input *call,
                          mc_item_stack **out, unsigned int field_pc, unsigned int invoke_pc) {
    mc_inventory_player *slots;
    if (!click_inventory(app, call, &slots, field_pc))
        return false;
    if (slots == NULL)
        return click_fail(call, "actual click null Inventory current invocation", invoke_pc);
    const mc_item_stack *stack = NULL;
    if (mc_inventory_player_current(slots, &stack, &call->current) != MC_INVENTORY_OK) {
        const mc_inventory_site *site = &call->current.site;
        return fail(call, call->current.operation, site->class_name, site->method, site->descriptor,
                    (unsigned int)site->pc);
    }
    if (stack != NULL && mc_item_stack_retain((mc_item_stack *)stack) != MC_ITEM_STACK_OK)
        return click_fail(call, "click local Stack native occurrence limit", invoke_pc);
    *out = (mc_item_stack *)stack;
    return true;
}

static void click_stacks_release(mc_client_gameplay_input *call) {
    mc_item_stack_release(call->click_air_stack);
    call->click_air_stack = NULL;
    mc_item_stack_release(call->click_block_stack);
    call->click_block_stack = NULL;
    call->use_state.stack = NULL;
    call->use_state.result_stack = NULL;
}

static bool retire_use(mc_client_gameplay_input *call) {
    if (mc_client_player_message_retire(&call->message_call, &call->message_cleanup) !=
        MC_CLIENT_PLAYER_MESSAGE_OK)
        return false;
    call->message_state.chat = NULL;
    call->message_state.failure = call->message_state.cleanup = NULL;
    call->message_error.chat = call->message_cleanup.chat = NULL;
    if (mc_client_singleplayer_use_retire(&call->use_call, &call->use_cleanup) !=
        MC_SINGLEPLAYER_USE_OK)
        return false;
    call->use_state.error = NULL;
    call->use_state.cleanup = NULL;
    call->use_state.item = NULL;
    /* Keep copied scalar diagnostics; only these views belonged to the ended
     * controller occurrence. The outer input failure site remains unchanged. */
    call->use_error.acquisition = NULL;
    call->use_error.activation = NULL;
    call->use_error.item = NULL;
    call->use_cleanup.acquisition = NULL;
    call->use_cleanup.activation = NULL;
    call->use_cleanup.item = NULL;
    return true;
}

static mc_world_storage_status workbench(void *context, mc_entity *player, int32_t x, int32_t y,
                                         int32_t z, mc_world_placement_error *error) {
    mc_client_gameplay_input *call = context;
    const mc_client_screen_status status = mc_client_screen_transitions_workbench(
        call->transitions, player, x, y, z, &call->screen.screen);
    if (status == MC_CLIENT_SCREEN_OK)
        return MC_WORLD_STORAGE_OK;
    const mc_client_screen_error *screen = &call->screen.screen;
    error->status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    error->operation = screen->operation;
    error->native_policy = screen->native_policy;
    error->source_exception = screen->source_exception ||
                              screen->status == MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE ||
                              screen->status == MC_CLIENT_SCREEN_SOURCE_INDEX ||
                              screen->status == MC_CLIENT_SCREEN_SOURCE_ARITHMETIC;
    error->source_error = screen->source_error;
    error->exception_class =
        screen->exception_class != NULL ? screen->exception_class
        : screen->status == MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE
            ? "java/lang/NullPointerException"
        : screen->status == MC_CLIENT_SCREEN_SOURCE_INDEX ? "java/lang/IndexOutOfBoundsException"
        : screen->status == MC_CLIENT_SCREEN_SOURCE_ARITHMETIC ? "java/lang/ArithmeticException"
                                                               : NULL;
    error->site = (mc_world_placement_site){screen->site.class_name, screen->site.method,
                                            screen->site.descriptor, screen->site.pc};
    return error->status;
}

static mc_world_storage_status player_message(void *context, mc_entity *player,
                                              mc_registry_classes *literal_owner,
                                              const mc_registry_string *key,
                                              mc_world_placement_error *error) {
    mc_client_gameplay_input *call = context;
    ++call->player_message_calls;
    const mc_client_player_message_status status = mc_client_player_message_begin(
        player, literal_owner, key, &call->message_call, &call->message_error);
    if (call->message_call != NULL)
        (void)mc_client_player_message_inspect(call->message_call, &call->message_state);
    if (status == MC_CLIENT_PLAYER_MESSAGE_OK) {
        ++call->player_message_returns;
        return MC_WORLD_STORAGE_OK;
    }
    const mc_client_player_message_error *message = &call->message_error;
    error->status = status == MC_CLIENT_PLAYER_MESSAGE_BUSY    ? MC_WORLD_STORAGE_BUSY
                    : status == MC_CLIENT_PLAYER_MESSAGE_LIMIT ? MC_WORLD_STORAGE_LIMIT
                    : status == MC_CLIENT_PLAYER_MESSAGE_ALLOCATION_FAILED
                        ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                        : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    error->operation = message->operation;
    error->native_policy = message->native_policy;
    error->source_exception = message->source_exception;
    error->source_error = message->source_error;
    error->exception_class = message->exception_class;
    if (message->site.class_name != NULL)
        error->site =
            (mc_world_placement_site){message->site.class_name, message->site.method,
                                      message->site.descriptor, (unsigned int)message->site.pc};
    return error->status;
}

static bool use(client_application *app, mc_client_gameplay_input *call, bool on_block,
                mc_item_stack *stack, int32_t x, int32_t y, int32_t z, int32_t face) {
    const unsigned int pc = on_block ? 239u : 350u;
    if (app->controller == NULL)
        return click_fail(call, "actual click null use controller invocation", pc);
    const mc_world_block_activation_context activation = {
        .context = call, .workbench = workbench, .player_message = player_message};
    const mc_singleplayer_use_context sources = {.activation = &activation,
                                                 .math = app->source_math_classes};
    const mc_singleplayer_use_status status =
        on_block
            ? mc_client_singleplayer_use_on_block_with_sources_begin(
                  app->controller, app->h, app->active_world, stack, x, y, z, face, &sources,
                  &call->use_call, &call->use_error)
            : mc_client_singleplayer_use_in_air_begin(app->controller, app->h, app->active_world,
                                                      stack, &call->use_call, &call->use_error);
    if (call->use_call != NULL)
        (void)mc_client_singleplayer_use_inspect(call->use_call, &call->use_state);
    if (status != MC_SINGLEPLAYER_USE_OK) {
        const mc_singleplayer_use_error *e = &call->use_error;
        if (e->site.class_name == NULL)
            return click_fail(call, e->operation, pc);
        return fail(call, e->operation, e->site.class_name, e->site.method, e->site.descriptor,
                    (unsigned int)e->site.pc);
    }
    if (on_block)
        ++call->block_uses;
    else
        ++call->air_uses;
    call->placements_admitted += call->use_state.placement_admitted;
    call->placed_blocks += call->use_state.setter_returned && call->use_state.placed;
    call->placement_sounds += call->use_state.sound_returned;
    call->stack_count_stores += call->use_state.count_stored;
    call->used_statistics += call->use_state.statistic_returned;
    if (!retire_use(call))
        return click_fail(call,
                          call->message_cleanup.status != MC_CLIENT_PLAYER_MESSAGE_OK
                              ? call->message_cleanup.operation
                              : call->use_cleanup.operation,
                          pc);
    return true;
}

static bool click_swing(client_application *app, mc_client_gameplay_input *call, unsigned int pc) {
    if (app->h == NULL)
        return click_fail(call, "actual click null Player swing receiver", pc);
    if (mc_local_player_swing(app->h, &call->swing) != MC_ENTITY_OK)
        return click_fail(call, call->swing.operation, pc);
    ++call->swing_calls;
    return true;
}

static bool click_equip(client_application *app, mc_client_gameplay_input *call, bool block) {
    const unsigned int field_pc = block ? 307u : 360u, invoke_pc = block ? 310u : 363u;
    if (app->t == NULL)
        return click_fail(call, "actual click null EntityRenderer held-item field", field_pc);
    mc_client_entity_renderer_state renderer;
    if (!mc_client_entity_renderer_inspect(app->t, &renderer))
        return click_fail(call, "actual click EntityRenderer native field access", field_pc);
    if (renderer.fields.c == NULL)
        return click_fail(call, "actual click null held-item reset invocation", invoke_pc);
    if (renderer.ra_occurrence == NULL ||
        mc_client_held_item_identity(renderer.ra_occurrence) != renderer.fields.c)
        return click_fail(call, "click canonical held-item native occurrence", invoke_pc);
    if (mc_client_held_item_reset_equip(renderer.ra_occurrence,
                                        block ? MC_CLIENT_HELD_ITEM_RESET_BLOCK_USE
                                              : MC_CLIENT_HELD_ITEM_RESET_AIR_USE,
                                        &call->equip) != MC_CLIENT_ENTITY_RENDERER_OK)
        return fail(call, call->equip.operation, call->equip.site.class_name,
                    call->equip.site.method, call->equip.site.descriptor,
                    (unsigned int)call->equip.site.pc);
    ++call->equip_resets;
    return true;
}

static bool click_clear_slot(client_application *app, mc_client_gameplay_input *call) {
    mc_inventory_player *array_owner, *index_owner;
    mc_inventory_player_state array, index;
    if (!click_inventory(app, call, &array_owner, 272))
        return false;
    if (!mc_inventory_player_read_state(array_owner, &array))
        return click_fail(call, "actual click Inventory main array field receiver", 275);
    if (!click_inventory(app, call, &index_owner, 282))
        return false;
    if (!mc_inventory_player_read_state(index_owner, &index))
        return click_fail(call, "actual click Inventory selected field receiver", 285);
    /* The main array was captured before evaluating the fresh selected index.
     * No callback occurs between those source reads and the actual aastore. */
    if (array.main_slots == NULL || index.selected < 0 || index.selected >= 36)
        return click_fail(call, "actual click main array null/index at aastore", 289);
    if (mc_inventory_player_set(array_owner, index.selected, NULL, &call->slot) != MC_INVENTORY_OK)
        return click_fail(call, call->slot.operation, 289);
    ++call->selected_slot_clears;
    return true;
}

static bool click_air(client_application *app, mc_client_gameplay_input *call) {
    if (!click_current(app, call, &call->click_air_stack, 326, 329))
        return false;
    if (call->click_air_stack == NULL)
        return true;
    if (!use(app, call, false, call->click_air_stack, 0, 0, 0, 0))
        return false;
    return !call->use_state.result || click_equip(app, call, false);
}

/* Actual Minecraft.a(I), including its Block use and fresh in-air fallback.
 * Distinct combat and Item/Block overrides retain their reached dependencies. */
static bool click(client_application *app, mc_client_gameplay_input *call, int32_t button) {
    if (button == 0 && app->W > 0)
        return true;
    if (button == 0 && !click_swing(app, call, 20))
        return false;
    if (button == 1)
        ++call->right_clicks;
    if (app->y == NULL) {
        if (button == 0)
            app->W = 10; /* Canonical concrete OS is never the source PJ creative class. */
        if (button == 1 && !click_air(app, call))
            return false;
        click_stacks_release(call);
        return true;
    }
    mc_entity_hit_result_view hit;
    mc_entity_hit_kind_class_view kinds;
    if (!hit_fields(app, call, &hit, &kinds, "(I)V", 59))
        return false;
    if (hit.fields.a == kinds.b) {
        if (button == 0)
            return attack(app, call, hit.fields.g);
        return fail(call, "actual Player Entity interaction controller invocation",
                    "net/minecraft/client/Minecraft", "a", "(I)V", 110u);
    }
    if (hit.fields.a == kinds.a) {
        const int32_t x = hit.fields.b, y = hit.fields.c, z = hit.fields.d, face = hit.fields.e;
        if (button == 0) {
            ++call->left_clicks;
            return mine(app, call, MC_SINGLEPLAYER_MINING_START, x, y, z, face, 184, "(I)V");
        }
        if (!click_current(app, call, &call->click_block_stack, 194, 197))
            return false;
        mc_item_stack_fields fields = {0};
        if (call->click_block_stack != NULL &&
            mc_item_stack_fields_get(call->click_block_stack, &fields) != MC_ITEM_STACK_OK)
            return click_fail(call, "actual click captured Stack count field", 209);
        const int32_t before = fields.count;
        if (!use(app, call, true, call->click_block_stack, x, y, z, face))
            return false;
        const bool fallback = !call->use_state.result;
        if (!fallback && !click_swing(app, call, 251))
            return false;
        if (call->click_block_stack == NULL)
            return true; /* Actual return259, including activation with no Stack. */
        if (mc_item_stack_fields_get(call->click_block_stack, &fields) != MC_ITEM_STACK_OK)
            return click_fail(call, "actual click captured Stack count field", 262);
        if (fields.count == 0) {
            if (!click_clear_slot(app, call))
                return false;
        } else if (fields.count != before && !click_equip(app, call, true))
            return false;
        if (fallback && !click_air(app, call))
            return false;
        click_stacks_release(call);
        return true;
    }
    if (button == 1 && !click_air(app, call))
        return false;
    click_stacks_release(call);
    return true;
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
        if (event->pressed && event->button == 0) {
            if (!click(app, call, 0))
                return false;
            app->ag = app->V; /* Only a returned source click reaches putfield470. */
            ++call->click_timer_stores;
        }
        if (event->pressed && event->button == 1)
            return click(app, call, 1);
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
        if (event->key == 1 && !screen_result(call, mc_client_screen_transitions_pause(
                                                        transitions, &call->screen.screen)))
            return false;
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
        if (event->key == code && !screen_result(call, mc_client_screen_transitions_inventory(
                                                           transitions, &call->screen.screen)))
            return false;
        if (!binding(app, call, 6, &code, 799, 802))
            return false;
        if (event->key == code && !drop(app, call))
            return false;
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
    /* The original fcmpl/iflt skips this arm for an unordered comparison. */
    if (elapsed >= delay && app->N) {
        call->source_pc = click_pc;
        if (!click(app, call, button))
            return false;
        app->ag = app->V;
        ++call->click_timer_stores;
    }
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
        if (hit.fields.a == kinds.a) {
            const int32_t x = hit.fields.b, y = hit.fields.c, z = hit.fields.d;
            if (!mine(app, call, MC_SINGLEPLAYER_MINING_PROGRESS, x, y, z, hit.fields.e, 102,
                      "(IZ)V"))
                return false;
            /* Capture DN105 before the fresh Hit side read at118. This native
             * occurrence keeps a failed exact producer alive through retire. */
            call->hit_renderer = app->effect_renderer;
            if (call->hit_renderer != NULL &&
                mc_client_effect_renderer_retain(call->hit_renderer) != MC_EFFECT_RENDERER_OK) {
                call->hit_renderer = NULL;
                return fail(call, "mining hit-particle producer occurrence limit",
                            "net/minecraft/client/Minecraft", "a", "(IZ)V", 121);
            }
            if (!hit_fields(app, call, &hit, &kinds, "(IZ)V", 118))
                return false;
            if (call->hit_renderer == NULL)
                return fail(call, "actual mining null EffectRenderer invocation",
                            "net/minecraft/client/Minecraft", "a", "(IZ)V", 121);
            const mc_effect_renderer_status hit_status = mc_client_effect_renderer_hit_block(
                call->hit_renderer, app->registry, app->source_math_classes, x, y, z, hit.fields.e,
                &call->hit_effect);
            call->hit_block_work = call->hit_effect.block != NULL;
            if (hit_status != MC_EFFECT_RENDERER_OK)
                return fail(call, call->hit_effect.operation, "net/minecraft/client/Minecraft", "a",
                            "(IZ)V", 121);
            if (call->hit_effect.block != NULL) {
                call->hit_particles_constructed += call->hit_effect.block->constructed;
                call->hit_particles_appended += call->hit_effect.block->appended;
            }
            if (!retire_hit(call))
                return fail(call, "mining hit-particle native retirement",
                            "net/minecraft/client/Minecraft", "a", "(IZ)V", 121);
            return true;
        }
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
        call->focus != NULL || call->screen.pending_receiver != NULL || call->mining_call != NULL ||
        call->use_call != NULL || call->drop_call != NULL || call->attack_call != NULL ||
        call->click_block_stack != NULL || call->click_air_stack != NULL ||
        call->hit_renderer != NULL || call->hit_block_work)
        return false;
    memset(call, 0, sizeof(*call));
    call->transitions = transitions;
    call->active = true;
    const bool result = run(app, transitions, call);
    call->active = false;
    return result;
}

bool mc_client_gameplay_input_retire(mc_client_gameplay_input *call) {
    if (call == NULL || call->active)
        return false;
    if (!retire_hit(call))
        return false;
    if (!retire_use(call))
        return false;
    if (!retire_drop(call))
        return false;
    if (!retire_attack(call))
        return false;
    click_stacks_release(call);
    if (mc_client_singleplayer_mining_retire(&call->mining_call, &call->mining_cleanup) !=
        MC_SINGLEPLAYER_MINING_OK)
        return false;
    call->mining_state.error = NULL;
    call->mining_error = (mc_singleplayer_mining_error){0};
    if (mc_client_screen_event_retire(&call->screen) != MC_CLIENT_SCREEN_OK ||
        mc_client_display_screen_retire(&call->focus) != MC_CLIENT_DISPLAY_SCREEN_OK)
        return false;
    return true;
}
