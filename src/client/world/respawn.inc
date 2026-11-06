/* Native ownership for the actual Minecraft.a(ZI)V source invocation. */
#include "../../entity/construction_internal.h"
#include "../../entity/player/player_internal.h"
#include "../gui/display_screen.h"
#include "../gui/gui_ingame_chat.h"
#include "../gui/source_screen_transitions.h"
#include "mc/entity/player/coordinates.h"
#include "mc/world/runtime/bed_spawn.h"
#include "mc/world/runtime/respawn.h"

#include <emmintrin.h>

struct mc_client_respawn_call {
    client_application *app;
    mc_client_world_load_call *load;
    mc_client_respawn_state state;
    mc_client_respawn_error failure, cleanup;
    mc_world_respawn_error world;
    mc_world_entity_tick_error entities;
    mc_singleplayer_controller_error controller;
    mc_player_spawn_error spawn;
    mc_player_input_error player_input;
    mc_world_player_load_error player_load;
    mc_movement_input_error input;
    mc_world_provider_error provider;
    mc_client_world_prepare_call *prepare;
    mc_client_world_prepare_state terrain;
    mc_client_display_screen_call *display;
    mc_world_respawn_position *position;
    mc_player_coordinate_pin *saved_bed;
    mc_player_coordinates *bed_exit;
    mc_world_bed_spawn_error bed;
    mc_player_error coordinate;
    mc_entity_position_rotation_error placement;
    mc_client_gui_ingame_chat_call *chat;
    mc_client_gui_ingame_chat_error chat_error, chat_cleanup;
    mc_entity *candidate;
    mc_movement_input *new_input;
};

static void respawn_at(mc_client_respawn_call *c, int pc) {
    c->failure.site = site("a", "(ZI)V", pc);
    c->failure.child = (mc_client_world_load_site){0};
}

static bool respawn_fail(mc_client_respawn_call *c, mc_client_respawn_status status,
                         const char *operation, int pc) {
    c->failure.status = status;
    c->failure.operation = operation;
    c->failure.site = site("a", "(ZI)V", pc);
    return false;
}

static mc_world_runtime *respawn_world(mc_client_respawn_call *c, int pc) {
    respawn_at(c, pc);
    mc_world_runtime *w = c->app->active_world;
    if (w == NULL) {
        c->failure.native_policy = false;
        (void)respawn_fail(c, MC_CLIENT_RESPAWN_NULL_REFERENCE,
                           "Respawn null actual App World receiver", pc);
    } else if (w != c->load->state.world) {
        (void)respawn_fail(c, MC_CLIENT_RESPAWN_BUSY,
                           "Respawn actual World outside owned Load graph", pc);
        w = NULL;
    }
    return w;
}

static mc_entity *respawn_player(mc_client_respawn_call *c, int pc) {
    respawn_at(c, pc);
    mc_entity *p = c->app->h;
    if (p == NULL) {
        c->failure.native_policy = false;
        (void)respawn_fail(c, MC_CLIENT_RESPAWN_NULL_REFERENCE,
                           "Respawn null actual App Player receiver", pc);
    } else if (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) ||
               p->classes != c->app->entities) {
        (void)respawn_fail(c, MC_CLIENT_RESPAWN_BUSY,
                           "Respawn actual Player outside canonical namespace", pc);
        p = NULL;
    }
    return p;
}

static bool respawn_world_result(mc_client_respawn_call *c, mc_world_storage_status status,
                                 int pc) {
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    c->failure.world = &c->world;
    c->failure.child =
        (mc_client_world_load_site){"fd", c->world.method, NULL, (int)c->world.bytecode};
    c->failure.native_policy = !c->world.source_null;
    return respawn_fail(c,
                        status == MC_WORLD_STORAGE_BUSY    ? MC_CLIENT_RESPAWN_BUSY
                        : status == MC_WORLD_STORAGE_LIMIT ? MC_CLIENT_RESPAWN_LIMIT
                        : status == MC_WORLD_STORAGE_ALLOCATION_FAILED
                            ? MC_CLIENT_RESPAWN_ALLOCATION
                            : MC_CLIENT_RESPAWN_CHILD_FAILED,
                        c->world.operation, pc);
}

static bool respawn_controller_result(mc_client_respawn_call *c,
                                      mc_singleplayer_controller_status status, int pc) {
    if (status == MC_SINGLEPLAYER_CONTROLLER_OK)
        return true;
    c->failure.controller = &c->controller;
    c->failure.child =
        (mc_client_world_load_site){c->controller.site.class_name, c->controller.site.method,
                                    c->controller.site.descriptor, (int)c->controller.site.pc};
    return respawn_fail(c,
                        status == MC_SINGLEPLAYER_CONTROLLER_BUSY ? MC_CLIENT_RESPAWN_BUSY
                        : status == MC_SINGLEPLAYER_CONTROLLER_NULL_REFERENCE
                            ? MC_CLIENT_RESPAWN_NULL_REFERENCE
                            : MC_CLIENT_RESPAWN_CHILD_FAILED,
                        c->controller.operation, pc);
}

static mc_client_singleplayer_controller *respawn_controller(mc_client_respawn_call *c, int pc) {
    respawn_at(c, pc);
    mc_client_singleplayer_controller *controller = c->app->controller;
    if (controller == NULL) {
        c->failure.native_policy = false;
        (void)respawn_fail(c, MC_CLIENT_RESPAWN_NULL_REFERENCE,
                           "Respawn null actual Controller receiver", pc);
    }
    return controller;
}

static bool respawn_literal(mc_client_respawn_call *c, bool invalid_bed, mc_player_string **out) {
    const int pc = invalid_bed ? 77 : 352;
    mc_client_application_classes *classes = c->app->source_namespace;
    if (classes == NULL || mtx_lock(&classes->mutex) != thrd_success)
        return respawn_fail(c, MC_CLIENT_RESPAWN_BUSY, "Respawn actual literal namespace", pc);
    mc_player_status status = MC_PLAYER_OK;
    if (classes->native_retiring || classes->phase != MC_CLIENT_APPLICATION_CLASS_READY) {
        (void)mtx_unlock(&classes->mutex);
        return respawn_fail(c, MC_CLIENT_RESPAWN_BUSY, "Respawn retiring literal namespace", pc);
    }
    mc_player_string **slot =
        invalid_bed ? &classes->world_bed_not_valid : &classes->world_respawning;
    if (*slot == NULL) {
        static const uint16_t respawning[] = {'R', 'e', 's', 'p', 'a', 'w', 'n', 'i', 'n', 'g'};
        static const uint16_t invalid[] = {'t', 'i', 'l', 'e', '.', 'b', 'e', 'd', '.',
                                           'n', 'o', 't', 'V', 'a', 'l', 'i', 'd'};
        status = mc_player_string_create(invalid_bed ? invalid : respawning,
                                         invalid_bed ? sizeof(invalid) / sizeof(invalid[0])
                                                     : sizeof(respawning) / sizeof(respawning[0]),
                                         slot);
    }
    if (status == MC_PLAYER_OK)
        *out = *slot;
    (void)mtx_unlock(&classes->mutex);
    return status == MC_PLAYER_OK ||
           respawn_fail(c,
                        status == MC_PLAYER_LIMIT ? MC_CLIENT_RESPAWN_LIMIT
                                                  : MC_CLIENT_RESPAWN_ALLOCATION,
                        "Respawn actual String literal allocation", pc);
}

static bool respawn_coordinate_result(mc_client_respawn_call *c, mc_player_status status, int pc) {
    if (status == MC_PLAYER_OK)
        return true;
    c->failure.saved_spawn = &c->coordinate;
    c->failure.child =
        (mc_client_world_load_site){c->coordinate.site.class_name, c->coordinate.site.method,
                                    c->coordinate.site.descriptor, (int)c->coordinate.site.pc};
    c->failure.native_policy = status != MC_PLAYER_NULL_REFERENCE;
    return respawn_fail(c,
                        status == MC_PLAYER_LIMIT               ? MC_CLIENT_RESPAWN_LIMIT
                        : status == MC_PLAYER_ALLOCATION_FAILED ? MC_CLIENT_RESPAWN_ALLOCATION
                        : status == MC_PLAYER_BUSY              ? MC_CLIENT_RESPAWN_BUSY
                        : status == MC_PLAYER_NULL_REFERENCE    ? MC_CLIENT_RESPAWN_NULL_REFERENCE
                                                                : MC_CLIENT_RESPAWN_CHILD_FAILED,
                        c->coordinate.operation, pc);
}

static bool respawn_bed_result(mc_client_respawn_call *c, mc_world_storage_status status) {
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    c->failure.bed = &c->bed;
    c->failure.child = (mc_client_world_load_site){
        c->bed.source_class, c->bed.source_method,
        c->bed.source_class != NULL && strcmp(c->bed.source_class, "ve") == 0 ? "(Lfd;IIII)Lbr;"
                                                                              : "(Lfd;Lbr;)Lbr;",
        (int)c->bed.pc};
    c->failure.native_policy = c->bed.native_policy;
    return respawn_fail(c,
                        status == MC_WORLD_STORAGE_BUSY    ? MC_CLIENT_RESPAWN_BUSY
                        : status == MC_WORLD_STORAGE_LIMIT ? MC_CLIENT_RESPAWN_LIMIT
                        : status == MC_WORLD_STORAGE_ALLOCATION_FAILED
                            ? MC_CLIENT_RESPAWN_ALLOCATION
                            : MC_CLIENT_RESPAWN_CHILD_FAILED,
                        c->bed.operation, 63);
}

static bool respawn_invalid_bed_message(mc_client_respawn_call *c) {
    mc_entity *receiver = c->app->h; /* Actual captured h74, before ldc77. */
    if (receiver != NULL && mc_entity_retain(receiver) != MC_ENTITY_OK)
        return respawn_fail(c, MC_CLIENT_RESPAWN_BUSY,
                            "Respawn captured message receiver occurrence", 74);
    bool returned = false;
    mc_player_string *key = NULL;
    if (!respawn_literal(c, true, &key))
        goto done;
    respawn_at(c, 79);
    if (receiver == NULL) {
        c->failure.native_policy = false;
        (void)respawn_fail(c, MC_CLIENT_RESPAWN_NULL_REFERENCE,
                           "Respawn null DC message invocation receiver", 79);
        goto done;
    }
    if (!mc_entity_is_kind(receiver, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) ||
        receiver->classes != c->app->entities) {
        (void)respawn_fail(c, MC_CLIENT_RESPAWN_BUSY, "Respawn canonical DC message receiver", 79);
        goto done;
    }
    client_application *actual_app = receiver->local_player->fields.minecraft; /* DC1. */
    if (actual_app == NULL || actual_app->v == NULL) {
        c->failure.native_policy = false;
        c->failure.child = (mc_client_world_load_site){"dc", "b", "(Ljava/lang/String;)V",
                                                       actual_app == NULL ? 4 : 8};
        (void)respawn_fail(c, MC_CLIENT_RESPAWN_NULL_REFERENCE,
                           actual_app == NULL ? "Respawn DC null App message field"
                                              : "Respawn DC null HUD message receiver",
                           79);
        goto done;
    }
    const mc_client_gui_ingame_chat_status status = mc_client_gui_ingame_chat_translate_begin(
        actual_app->v, actual_app->registry, key, &c->chat, &c->chat_error);
    if (status != MC_CLIENT_GUI_INGAME_CHAT_OK) {
        c->failure.chat = &c->chat_error;
        const mc_client_gui_ingame_site *child =
            c->chat_error.child.class_name == NULL ? &c->chat_error.site : &c->chat_error.child;
        c->failure.child = (mc_client_world_load_site){child->class_name, child->method,
                                                       child->descriptor, (int)child->pc};
        c->failure.native_policy = c->chat_error.native_policy;
        (void)respawn_fail(c,
                           status == MC_CLIENT_GUI_INGAME_CHAT_LIMIT ? MC_CLIENT_RESPAWN_LIMIT
                           : status == MC_CLIENT_GUI_INGAME_CHAT_ALLOCATION
                               ? MC_CLIENT_RESPAWN_ALLOCATION
                               : MC_CLIENT_RESPAWN_CHILD_FAILED,
                           c->chat_error.operation, 79);
        goto done;
    }
    c->state.bed_notified = true;
    if (mc_client_gui_ingame_chat_retire(&c->chat, &c->chat_cleanup) !=
        MC_CLIENT_GUI_INGAME_CHAT_OK) {
        c->failure.chat = &c->chat_cleanup;
        (void)respawn_fail(c, MC_CLIENT_RESPAWN_CLEANUP_FAILED,
                           "Respawn returned invalid-Bed message native cleanup", 79);
        goto done;
    }
    returned = true;
done:
    mc_entity_release(receiver);
    return returned;
}

static void respawn_run(mc_client_respawn_call *c) {
    client_application *app = c->app;
    mc_world_runtime *w = respawn_world(c, 4);
    if (w == NULL)
        return;
    mc_world_runtime_fields fields;
    if (mc_world_runtime_fields_get(w, &fields) != MC_WORLD_STORAGE_OK) {
        (void)respawn_fail(c, MC_CLIENT_RESPAWN_CHILD_FAILED, "Respawn actual World fields", 4);
        return;
    }
    if (!fields.B) {
        w = respawn_world(c, 14);
        if (w == NULL)
            return;
        if (mc_world_runtime_fields_get(w, &fields) != MC_WORLD_STORAGE_OK) {
            (void)respawn_fail(c, MC_CLIENT_RESPAWN_CHILD_FAILED, "Respawn actual Dimension field",
                               14);
            return;
        }
        if (fields.t == NULL) {
            c->failure.native_policy = false;
            (void)respawn_fail(c, MC_CLIENT_RESPAWN_NULL_REFERENCE,
                               "Respawn actual Dimension receiver", 17);
            return;
        }
        bool allowed;
        if (mc_world_provider_can_respawn(fields.t, &allowed, &c->provider) !=
            MC_WORLD_PROVIDER_OK) {
            (void)respawn_fail(c, MC_CLIENT_RESPAWN_CHILD_FAILED,
                               "Respawn actual Dimension can-respawn body", 17);
            return;
        }
        if (!allowed) {
            (void)respawn_fail(c, MC_CLIENT_RESPAWN_UNIMPLEMENTED,
                               "Respawn actual Minecraft dimension transition", 23);
            return;
        }
    }
    if (app->h != NULL && !c->state.force) {
        mc_entity *p = respawn_player(c, 50);
        if (p == NULL)
            return;
        if (!respawn_coordinate_result(
                c, mc_player_saved_spawn_get(p, &c->saved_bed, &c->coordinate), 50))
            return;
        c->state.saved_spawn_read = true;
        const mc_player_coordinates *saved = mc_player_coordinate_pin_value(c->saved_bed);
        c->state.saved_bed_identity = (uintptr_t)saved;
        if (saved != NULL) {
            w = app->active_world; /* A nullable argument to the actual static GS.a63. */
            if (w != NULL && w != c->load->state.world) {
                (void)respawn_fail(c, MC_CLIENT_RESPAWN_BUSY,
                                   "Respawn Bed World outside owned Load graph", 63);
                return;
            }
            respawn_at(c, 63);
            if (!respawn_bed_result(c, mc_world_bed_spawn_validate(w, app->registry, saved, NULL,
                                                                   &c->bed_exit, &c->bed)))
                return;
            c->state.bed_checked = true;
            c->state.bed_valid = c->bed_exit != NULL;
            c->state.bed_exit_identity = (uintptr_t)c->bed_exit;
            if (c->bed_exit == NULL && !respawn_invalid_bed_message(c))
                return;
        }
    }
    if (c->bed_exit == NULL) {
        w = respawn_world(c, 91);
        if (w == NULL || !respawn_world_result(
                             c, mc_world_runtime_respawn_spawn_new(w, &c->position, &c->world), 91))
            return;
    }
    w = respawn_world(c, 103);
    if (w == NULL || mc_world_runtime_fields_get(w, &fields) != MC_WORLD_STORAGE_OK) {
        if (w != NULL)
            (void)respawn_fail(c, MC_CLIENT_RESPAWN_CHILD_FAILED,
                               "Respawn actual World provider field", 103);
        return;
    }
    const mc_chunk_provider_concrete_kind kind = mc_chunk_provider_concrete_kind_of(fields.v);
    if (kind != MC_CHUNK_PROVIDER_NULL && kind != MC_CHUNK_PROVIDER_MAP_CACHE) {
        (void)respawn_fail(c, MC_CLIENT_RESPAWN_UNIMPLEMENTED,
                           "Respawn actual centered-provider recenter child", 139);
        return;
    }
    w = respawn_world(c, 146);
    if (w == NULL ||
        !respawn_world_result(c,
                              mc_world_runtime_respawn_repair_spawn(
                                  w, MC_WORLD_RESPAWN_DEFAULT_SPAWN_QUERIES, &c->world),
                              146))
        return;
    w = respawn_world(c, 153);
    if (w == NULL)
        return;
    const mc_world_entity_tick_context context = {
        .player = {.travel = {.code_registry = app->registry,
                              .trig = mc_math_classes_table(app->source_math_classes),
                              .math = app->source_math_classes}},
        .math = app->source_math_classes};
    if (mc_world_runtime_respawn_cleanup(w, &context, NULL, &c->entities) != MC_WORLD_STORAGE_OK) {
        c->failure.entities = &c->entities;
        c->failure.child = (mc_client_world_load_site){
            c->entities.source_class, c->entities.source_method, "()V", (int)c->entities.bytecode};
        (void)respawn_fail(c, MC_CLIENT_RESPAWN_CHILD_FAILED, c->entities.operation, 153);
        return;
    }
    if (app->h != NULL) {
        mc_entity *p = respawn_player(c, 170);
        if (p == NULL)
            return;
        c->state.previous_id = p->base.id;
        c->state.previous_player_identity = (uintptr_t)p;
        w = respawn_world(c, 183);
        if (w == NULL || !respawn_world_result(c,
                                               mc_world_runtime_respawn_remove(
                                                   w, app->h, app->registry,
                                                   app->source_math_classes, NULL, 0, &c->world),
                                               183))
            return;
        c->state.old_player_removed = true;
    }
    respawn_at(c, 188);
    if (mc_client_view_slot_store(app, NULL) != MC_ENTITY_OK) {
        (void)respawn_fail(c, MC_CLIENT_RESPAWN_BUSY, "Respawn actual i188 native occurrence", 188);
        return;
    }
    c->load->i_occurrence = NULL;
    mc_client_singleplayer_controller *controller = respawn_controller(c, 200);
    if (controller == NULL)
        return;
    w = respawn_world(c, 200);
    if (w == NULL)
        return;
    const mc_singleplayer_controller_status created =
        mc_client_singleplayer_controller_create_player(controller, w, NULL, &c->candidate,
                                                        &c->controller);
    if (created != MC_SINGLEPLAYER_CONTROLLER_OK) {
        mc_singleplayer_controller_snapshot state;
        if (mc_client_singleplayer_controller_read(controller, &state) &&
            state.failed_player != NULL) {
            c->load->failed_controller = controller;
            c->load->failed_candidate = c->load->state.failed_player = state.failed_player;
        }
        (void)respawn_controller_result(c, created, 200);
        return;
    }
    c->state.player_created = true;
    respawn_at(c, 206);
    if (app->h != c->load->h_occurrence || mc_entity_retain(c->candidate) != MC_ENTITY_OK) {
        (void)respawn_fail(c, MC_CLIENT_RESPAWN_BUSY, "Respawn actual h206 native occurrence", 206);
        return;
    }
    mc_entity *old_player = c->load->state.player, *old_h = c->load->h_occurrence;
    app->h = c->load->state.player = c->load->h_occurrence = c->candidate;
    c->candidate = NULL; /* Constructor result belongs to Load; h owns its separate retain. */
    mc_entity_release(old_h);
    c->load->respawn_previous_player = old_player; /* Move the replaced Load occurrence. */
    c->load->respawn_input_pending = true;
    c->state.player_identity = (uintptr_t)app->h;
    c->state.player_stored = true;
    mc_entity *p = respawn_player(c, 214);
    if (p == NULL)
        return;
    p->local_player->fields.player.m = c->state.dimension;
    respawn_at(c, 222);
    if (mc_client_view_slot_store(app, app->h) != MC_ENTITY_OK) {
        (void)respawn_fail(c, MC_CLIENT_RESPAWN_BUSY, "Respawn actual i222 native occurrence", 222);
        return;
    }
    c->load->i_occurrence = app->h;
    c->state.view_stored = true;
    p = respawn_player(c, 229);
    if (p == NULL)
        return;
    if (mc_player_spawn_reset(p, NULL, &c->spawn) != MC_ENTITY_OK) {
        c->failure.spawn = &c->spawn;
        c->failure.child = (mc_client_world_load_site){c->spawn.source_class, "t_", "()V",
                                                       (int)c->spawn.player_bytecode};
        (void)respawn_fail(c, MC_CLIENT_RESPAWN_CHILD_FAILED, c->spawn.operation, 229);
        return;
    }
    c->state.spawn_returned = true;
    if (c->state.bed_valid) {
        p = respawn_player(c, 242);
        if (p == NULL || !respawn_coordinate_result(
                             c,
                             mc_player_saved_spawn_set(
                                 p, mc_player_coordinate_pin_value(c->saved_bed), &c->coordinate),
                             242))
            return;
        c->state.bed_stored = true;
        p = app->h; /* Actual h248, before the three float operand expressions. */
        if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
            (void)respawn_fail(c, MC_CLIENT_RESPAWN_CHILD_FAILED,
                               "Respawn Bed float operand environment", 254);
            return;
        }
        volatile float x = (float)c->bed_exit->x;
        volatile float placed_x = x + 0.5f;
        volatile float y = (float)c->bed_exit->y;
        volatile float placed_y = y + 0.1f;
        volatile float z = (float)c->bed_exit->z;
        volatile float placed_z = z + 0.5f;
        respawn_at(c, 281);
        if (p == NULL) {
            c->failure.native_policy = false;
            (void)respawn_fail(c, MC_CLIENT_RESPAWN_NULL_REFERENCE,
                               "Respawn null Bed placement invocation receiver", 281);
            return;
        }
        if (mc_entity_source_position_rotation(p, (double)placed_x, (double)placed_y,
                                               (double)placed_z, 0.0f, 0.0f,
                                               &c->placement) != MC_ENTITY_OK) {
            c->failure.position = &c->placement;
            c->failure.native_policy = c->placement.native_policy;
            const bool position = c->placement.position_bytecode != 0;
            c->failure.child = (mc_client_world_load_site){
                "sn", position ? "e" : "c", position ? "(DDD)V" : "(DDDFF)V",
                (int)(position ? c->placement.position_bytecode : c->placement.bytecode)};
            (void)respawn_fail(c, MC_CLIENT_RESPAWN_CHILD_FAILED, c->placement.operation, 281);
            return;
        }
        c->state.bed_positioned = true;
    }
    controller = respawn_controller(c, 292);
    p = app->h; /* The argument is not dereferenced by the caller. */
    if (controller == NULL ||
        !respawn_controller_result(
            c, mc_client_singleplayer_controller_prepare_player(controller, p, &c->controller),
            292))
        return;
    c->state.prepared = true;
    w = respawn_world(c, 303);
    if (w == NULL)
        return;
    if (mc_world_runtime_load_player(w, app->h, &c->player_load) != MC_WORLD_STORAGE_OK) {
        c->failure.player_load = &c->player_load;
        c->failure.child =
            (mc_client_world_load_site){"fd", "a", "(Lgs;)V", (int)c->player_load.bytecode};
        (void)respawn_fail(c, MC_CLIENT_RESPAWN_CHILD_FAILED, c->player_load.operation, 303);
        return;
    }
    c->state.player_inserted = true;
    p = app->h; /* Capture actual h307 before NEW lr310 and Settings315. */
    respawn_at(c, 318);
    if (mc_movement_input_create_from_options(app->settings, &c->new_input, &c->input) !=
        MC_MOVEMENT_INPUT_OK) {
        c->failure.input = &c->input;
        (void)respawn_fail(c, MC_CLIENT_RESPAWN_CHILD_FAILED, "Respawn actual lr constructor", 318);
        return;
    }
    respawn_at(c, 321);
    if (mc_local_player_set_movement_input(p, c->new_input, &c->player_input) != MC_PLAYER_OK) {
        c->failure.player_input = &c->player_input;
        (void)respawn_fail(c, MC_CLIENT_RESPAWN_CHILD_FAILED, c->player_input.operation, 321);
        return;
    }
    (void)mc_movement_input_retire(&c->load->state.input, NULL);
    c->load->state.input = c->new_input;
    c->new_input = NULL;
    mc_entity_release(c->load->respawn_previous_player);
    c->load->respawn_previous_player = NULL;
    c->load->respawn_input_pending = false;
    c->state.input_stored = true;
    p = respawn_player(c, 330);
    if (p == NULL)
        return;
    p->base.id = c->state.previous_id;
    c->state.id_stored = true;
    if (respawn_player(c, 337) == NULL)
        return; /* Actual DC.v is literal return0 after its invocation receiver. */
    controller = respawn_controller(c, 348);
    p = app->h; /* Actual OB.b leaves this nullable argument unread. */
    if (controller == NULL ||
        !respawn_controller_result(
            c, mc_client_singleplayer_controller_finish_player(controller, p, &c->controller), 348))
        return;
    c->state.player_finished = true;
    mc_player_string *text = NULL;
    if (!respawn_literal(c, false, &text))
        return;
    respawn_at(c, 354);
    const mc_client_world_prepare_status prepared =
        mc_client_world_prepare_begin(app, text, &c->prepare);
    if (c->prepare != NULL)
        (void)mc_client_world_prepare_inspect(c->prepare, &c->terrain);
    if (prepared != MC_CLIENT_WORLD_PREPARE_OK) {
        c->failure.terrain = c->prepare == NULL ? NULL : &c->terrain;
        (void)respawn_fail(
            c, MC_CLIENT_RESPAWN_CHILD_FAILED,
            c->prepare == NULL ? "Respawn terrain native admission" : c->terrain.operation, 354);
        return;
    }
    c->state.terrain_returned = true;
    mc_client_screen_state screen;
    mc_client_screen_error screen_error;
    if (app->r != NULL) {
        if (mc_client_screen_inspect(app->r, &screen, &screen_error) != MC_CLIENT_SCREEN_OK) {
            (void)respawn_fail(c, MC_CLIENT_RESPAWN_BUSY, "Respawn actual CH type test", 361);
            return;
        }
        if (screen.kind == MC_CLIENT_SCREEN_DEATH) {
            respawn_at(c, 369);
            const mc_client_display_screen_status status = mc_client_display_screen_run(
                app, mc_client_screen_transitions_classes(app->screen_transitions), NULL,
                &c->display);
            if (status != MC_CLIENT_DISPLAY_SCREEN_OK) {
                c->failure.display = c->display;
                (void)respawn_fail(c, MC_CLIENT_RESPAWN_CHILD_FAILED,
                                   c->display == NULL ? "Respawn display native admission"
                                                      : c->display->operation,
                                   369);
                return;
            }
            c->state.death_screen_closed = true;
        }
    }
    respawn_at(c, 372);
    c->state.source_returned = true;
}

mc_client_respawn_status mc_client_respawn_begin(client_application *app,
                                                 mc_client_world_load_call *load, bool force,
                                                 int32_t dimension, mc_client_respawn_call **out,
                                                 mc_client_respawn_error *error) {
    if (error == NULL || out == NULL || *out != NULL || load == NULL || app == NULL) {
        if (error != NULL)
            *error = (mc_client_respawn_error){.status = MC_CLIENT_RESPAWN_ARGUMENT,
                                               .operation = "Respawn native owner/output",
                                               .native_policy = true};
        return MC_CLIENT_RESPAWN_ARGUMENT;
    }
    if (load->classes == NULL || load->classes->app != app || load->state.retiring ||
        load->state.active || load->classes->active || load->classes->retiring ||
        load->classes->latest != load || load->respawn_calls != 0 || !load->state.source_returned ||
        load->state.superseded || load->respawn_input_pending) {
        *error = (mc_client_respawn_error){.status = MC_CLIENT_RESPAWN_BUSY,
                                           .operation = "Respawn active/foreign Load graph",
                                           .native_policy = true};
        return error->status;
    }
    mc_client_respawn_call *c = calloc(1, sizeof(*c));
    if (c == NULL) {
        *error = (mc_client_respawn_error){.status = MC_CLIENT_RESPAWN_ALLOCATION,
                                           .operation = "Respawn native invocation allocation",
                                           .native_policy = true};
        return error->status;
    }
    c->app = app;
    c->load = load;
    c->failure.native_policy = true;
    c->state.force = force;
    c->state.dimension = dimension;
    c->state.active = load->classes->active = load->state.active = true;
    ++load->respawn_calls;
    *out = c;
    mc_world_respawn_error_init(&c->world);
    mc_world_entity_tick_error_init(&c->entities);
    mc_world_bed_spawn_error_init(&c->bed);
    respawn_run(c);
    c->state.active = load->classes->active = load->state.active = false;
    *error = c->failure;
    return c->failure.status;
}

mc_client_respawn_status mc_client_respawn_player_begin(mc_entity *player,
                                                        mc_client_world_load_call *load,
                                                        mc_client_respawn_call **out,
                                                        mc_client_respawn_error *error) {
    if (error == NULL)
        return MC_CLIENT_RESPAWN_ARGUMENT;
    if (out == NULL || *out != NULL) {
        *error = (mc_client_respawn_error){.status = MC_CLIENT_RESPAWN_ARGUMENT,
                                           .operation = "Respawn DC native output slot",
                                           .native_policy = true};
        return error->status;
    }
    if (player == NULL || !mc_entity_is_kind(player, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER)) {
        *error =
            (mc_client_respawn_error){.status = player == NULL ? MC_CLIENT_RESPAWN_NULL_REFERENCE
                                                               : MC_CLIENT_RESPAWN_ARGUMENT,
                                      .operation = "Respawn actual DC.p_ receiver",
                                      .site = {"dc", "p_", "()V", 1},
                                      .native_policy = player != NULL};
        return error->status;
    }
    const mc_entity_status held = mc_entity_retain(player);
    if (held != MC_ENTITY_OK) {
        *error = (mc_client_respawn_error){.status = MC_CLIENT_RESPAWN_BUSY,
                                           .operation = "Respawn DC invocation occurrence",
                                           .native_policy = true};
        return error->status;
    }
    client_application *app = player->local_player->fields.minecraft;
    if (app == NULL) {
        *error =
            (mc_client_respawn_error){.status = MC_CLIENT_RESPAWN_NULL_REFERENCE,
                                      .operation = "Respawn actual DC.b App invocation receiver",
                                      .site = {"dc", "p_", "()V", 6}};
        mc_entity_release(player);
        return error->status;
    }
    const mc_client_respawn_status status =
        mc_client_respawn_begin(app, load, false, 0, out, error);
    if (out != NULL && *out != NULL) {
        (*out)->state.wrapper_entered = true;
        (*out)->state.wrapper_returned = status == MC_CLIENT_RESPAWN_OK;
    }
    mc_entity_release(player);
    return status;
}

bool mc_client_respawn_inspect(const mc_client_respawn_call *c, mc_client_respawn_state *out) {
    if (c == NULL || out == NULL || c->state.active)
        return false;
    *out = c->state;
    out->failure = &c->failure;
    out->cleanup = &c->cleanup;
    return true;
}

mc_client_respawn_status mc_client_respawn_retire(mc_client_respawn_call **slot,
                                                  mc_client_respawn_error *error) {
    if (slot == NULL || error == NULL)
        return MC_CLIENT_RESPAWN_ARGUMENT;
    mc_client_respawn_call *c = *slot;
    *error = (mc_client_respawn_error){0};
    if (c == NULL)
        return MC_CLIENT_RESPAWN_OK;
    if (c->state.active || c->load->classes->active) {
        *error = (mc_client_respawn_error){.status = MC_CLIENT_RESPAWN_BUSY,
                                           .operation = "Respawn entered native cleanup",
                                           .native_policy = true};
        return error->status;
    }
    c->state.retiring = true;
    if (mc_client_display_screen_retire(&c->display) != MC_CLIENT_DISPLAY_SCREEN_OK ||
        (c->prepare != NULL &&
         mc_client_world_prepare_retire(&c->prepare) != MC_CLIENT_WORLD_PREPARE_OK) ||
        !mc_world_respawn_error_retire(&c->world) ||
        !mc_world_entity_tick_error_retire(&c->entities) ||
        mc_client_gui_ingame_chat_retire(&c->chat, &c->chat_cleanup) !=
            MC_CLIENT_GUI_INGAME_CHAT_OK ||
        !mc_world_bed_spawn_error_retire(&c->bed)) {
        c->cleanup =
            (mc_client_respawn_error){.status = MC_CLIENT_RESPAWN_CLEANUP_FAILED,
                                      .operation = "Respawn retained child native retirement",
                                      .native_policy = true,
                                      .world = &c->world,
                                      .entities = &c->entities,
                                      .chat = &c->chat_cleanup,
                                      .bed = &c->bed};
        *error = c->cleanup;
        return error->status;
    }
    mc_world_respawn_position_free(c->position);
    mc_world_bed_spawn_position_free(c->bed_exit);
    mc_player_coordinate_pin_release(&c->saved_bed);
    (void)mc_movement_input_retire(&c->new_input, NULL);
    mc_entity_release(c->candidate);
    --c->load->respawn_calls;
    free(c);
    *slot = NULL;
    return MC_CLIENT_RESPAWN_OK;
}
