#include "load_world.h"
#include "load_world_terminal_internal.h"

#include "../../world/runtime/chunk_provider_internal.h"
#include "../../world/runtime/source_terminal_internal.h"
#include "../../world/runtime/terminal_inventory_internal.h"
#include "../../world/storage/objects_internal.h"
#include "../application_state.h"
#include "../controller/singleplayer_internal.h"
#include "../gui/loading_screen.h"
#include "../particles/application.h"
#include "../particles/effect_renderer_internal.h"
#include "../startup/application_owner_internal.h"
#include "../startup/application_startup_internal.h"
#include "../startup/application_startup_menu.h"
#include "../startup/application_startup_rendering.h"
#include "../view_slot.h"
#include "entity_renderer/internal.h"
#include "mc/client/controller/singleplayer.h"
#include "mc/client/input/movement_input.h"
#include "mc/client/saves/save_format.h"
#include "mc/client/sound/sound_manager.h"
#include "mc/client/statistics/writer.h"
#include "mc/client/world/picking.h"
#include "mc/entity/player/input.h"
#include "mc/entity/player/spawn.h"
#include "mc/statistics/registry/registry.h"
#include "mc/world/runtime/entities.h"
#include "mc/world/runtime/loading/player.h"
#include "mc/world/runtime/source_construction.h"
#include "prepare_world.h"
#include "render_global_access.h"
#include "render_global_internal.h"

#include <stdlib.h>
#include <string.h>

#define MC_CLASS "net/minecraft/client/Minecraft"
#define LOAD_DESC "(Ljava/lang/String;Ljava/lang/String;J)V"
#define CHANGE_DESC "(Lfd;Ljava/lang/String;Lgs;)V"
#define CONVERT_DESC "(Ljava/lang/String;Ljava/lang/String;)V"

struct mc_client_world_load_classes {
    client_application *app;
    mc_client_loading_classes *gui;
    size_t calls;
    bool app_borrow, active, retiring;
};

typedef struct change_prefix {
    bool stats_b_returned, stats_c_returned, i_stored, start_returned, message_returned;
    bool stream_returned, f_stored, h_stored, P_stored, source_returned;
    mc_client_stats_error statistics;
    mc_client_stats_report statistics_report;
    mc_client_stats_file_error statistics_file;
    mc_client_loading_error start, message;
    mc_sound_error sound;
    mc_sound_manager_report sound_report;
} change_prefix;

typedef struct captured_format {
    mc_client_save_format *owner;
    bool was_pending, owns_pending;
} captured_format;

struct mc_client_world_load_call {
    mc_client_world_load_classes *classes;
    mc_client_world_load_state state;
    mc_client_world_load_error failure, cleanup;
    change_prefix changes[2];
    mc_client_save_error save;
    mc_client_save_error save_cleanup;
    mc_world_source_error source_store, handler_cleanup, world_source_cleanup;
    mc_world_saved_data_source_error saved_data, saved_data_cleanup;
    mc_world_storage_error world_error, cleanup_world;
    mc_registry_error registry;
    mc_singleplayer_controller_error controller_error, controller_cleanup;
    mc_player_spawn_error spawn;
    mc_player_input_error player_input;
    mc_movement_input_error input_error;
    mc_client_loading_error conversion_start, conversion_message, loading_cleanup;
    mc_client_world_prepare_state terrain;
    mc_client_world_prepare_call *prepare;
    mc_client_loading_screen *pending_loading;
    bool pending_loading_owned_call;
    mc_client_singleplayer_controller *controllers[4];
    size_t controller_count;
    mc_client_singleplayer_controller *failed_controller;
    mc_entity *failed_candidate;
    captured_format formats[3];
    size_t format_count;
    mc_world_info_name *format_name;
    mc_player_string *conversion_title;
    mc_player_string *level_text;
    mc_entity *h_occurrence, *i_occurrence;
    bool world_slot_owned, folder_held, display_held, world_retired, cache_retired;
    mc_client_render_global *render_owner;
    mc_client_render_global_world_call *render_call;
    mc_client_render_global_world_state render_state;
    mc_client_render_global_error render_cleanup;
    bool render_world_owned;
    mc_client_effect_renderer *effects_owner;
    bool effects_held, effects_world_owned;
    mc_effect_renderer_error effects, effects_cleanup;
    mc_world_player_load_error player_load;
    mc_world_access_failure captured_player_failure;
    mc_client_entity_renderer_pick_error pick_cleanup;
};

static mc_client_world_load_site site(const char *method, const char *descriptor, int pc) {
    return (mc_client_world_load_site){MC_CLASS, method, descriptor, pc};
}

static mc_client_world_load_status fail(mc_client_world_load_call *call,
                                        mc_client_world_load_status status, const char *operation,
                                        const char *method, const char *descriptor, int pc) {
    call->failure.status = status;
    call->failure.operation = operation;
    call->failure.site = site(method, descriptor, pc);
    call->failure.native_policy = status != MC_CLIENT_WORLD_LOAD_NULL_REFERENCE;
    if (strcmp(descriptor, CHANGE_DESC) == 0 || strcmp(descriptor, CONVERT_DESC) == 0)
        call->failure.caller = site("a", LOAD_DESC, call->state.last_outer_pc);
    return status;
}

static mc_client_world_load_status change_fail(mc_client_world_load_call *call,
                                               mc_client_world_load_status status,
                                               const char *operation, int pc) {
    call->state.last_change_pc = pc;
    call->failure.caller = site("a", LOAD_DESC, call->state.last_outer_pc);
    return fail(call, status, operation, "a", CHANGE_DESC, pc);
}

static bool shared_literal(mc_client_world_load_call *call, mc_player_string **slot,
                           const char *ascii, const char *descriptor, int pc,
                           mc_player_string **out) {
    mc_client_application_classes *classes = call->classes->app->source_namespace;
    const char *method = strcmp(descriptor, CONVERT_DESC) == 0 ? "b" : "a";
    if (mtx_lock(&classes->mutex) != thrd_success) {
        fail(call, MC_CLIENT_WORLD_LOAD_BUSY, "World literal namespace lock", method, descriptor,
             pc);
        return false;
    }
    mc_player_status status = MC_PLAYER_OK;
    if (classes->native_retiring || classes->phase != MC_CLIENT_APPLICATION_CLASS_READY) {
        (void)mtx_unlock(&classes->mutex);
        fail(call, MC_CLIENT_WORLD_LOAD_STATE, "World literal live MC namespace", method,
             descriptor, pc);
        return false;
    }
    if (*slot == NULL) {
        uint16_t units[64];
        const size_t length = strlen(ascii);
        if (length > sizeof(units) / sizeof(units[0]))
            status = MC_PLAYER_LIMIT;
        else {
            for (size_t i = 0; i < length; ++i)
                units[i] = (unsigned char)ascii[i];
            status = mc_player_string_create(units, length, slot);
        }
    }
    if (status == MC_PLAYER_OK)
        *out = *slot;
    (void)mtx_unlock(&classes->mutex);
    if (status != MC_PLAYER_OK) {
        fail(call,
             status == MC_PLAYER_ALLOCATION_FAILED ? MC_CLIENT_WORLD_LOAD_ALLOCATION
                                                   : MC_CLIENT_WORLD_LOAD_LIMIT,
             "actual MC World text literal", method, descriptor, pc);
        return false;
    }
    return true;
}

mc_client_world_load_status
mc_client_world_load_classes_create(client_application *app, mc_client_world_load_classes **out) {
    if (app == NULL || out == NULL || *out != NULL)
        return MC_CLIENT_WORLD_LOAD_ARGUMENT;
    if (app->constructor_phase != MC_CLIENT_APP_READY || app->startup == NULL ||
        app->startup->phase == MC_CLIENT_STARTUP_RETIRING || app->startup->menu == NULL ||
        !app->startup->menu->source_startup_returned || app->source_namespace == NULL ||
        app->loading_screen == NULL || app->world_classes == NULL || app->entities == NULL ||
        app->registry == NULL || app->profile == NULL)
        return MC_CLIENT_WORLD_LOAD_STATE;
    mc_client_world_load_classes *classes = calloc(1, sizeof(*classes));
    if (classes == NULL)
        return MC_CLIENT_WORLD_LOAD_ALLOCATION;
    classes->app = app;
    classes->gui = mc_client_loading_screen_classes(app->loading_screen);
    if (mc_client_loading_classes_retain(classes->gui) != MC_CLIENT_LOADING_OK) {
        free(classes);
        return MC_CLIENT_WORLD_LOAD_BUSY;
    }
    if (!mc_client_application_screen_borrow(app, &classes->app_borrow)) {
        (void)mc_client_loading_classes_release(classes->gui);
        free(classes);
        return MC_CLIENT_WORLD_LOAD_BUSY;
    }
    *out = classes;
    return MC_CLIENT_WORLD_LOAD_OK;
}

mc_client_world_load_status
mc_client_world_load_classes_retire(mc_client_world_load_classes **slot) {
    if (slot == NULL)
        return MC_CLIENT_WORLD_LOAD_ARGUMENT;
    mc_client_world_load_classes *classes = *slot;
    if (classes == NULL)
        return MC_CLIENT_WORLD_LOAD_OK;
    if (classes->active || classes->calls != 0)
        return MC_CLIENT_WORLD_LOAD_BUSY;
    classes->retiring = true;
    if (classes->gui != NULL) {
        if (mc_client_loading_classes_release(classes->gui) != MC_CLIENT_LOADING_OK)
            return MC_CLIENT_WORLD_LOAD_BUSY;
        classes->gui = NULL;
    }
    if (!mc_client_application_screen_release(classes->app, classes->app_borrow))
        return MC_CLIENT_WORLD_LOAD_BUSY;
    free(classes);
    *slot = NULL;
    return MC_CLIENT_WORLD_LOAD_OK;
}

mc_client_world_load_status
mc_client_world_load_classes_terminal_end(mc_client_world_load_classes *classes) {
    if (classes == NULL)
        return MC_CLIENT_WORLD_LOAD_ARGUMENT;
    if (classes->active)
        return MC_CLIENT_WORLD_LOAD_BUSY;
    classes->retiring = true;
    return MC_CLIENT_WORLD_LOAD_OK;
}

static bool loading(mc_client_world_load_call *call, mc_client_loading_screen *receiver,
                    mc_player_string *text, bool start, const char *method, const char *descriptor,
                    int pc, mc_client_loading_error *error) {
    if (receiver == NULL) {
        fail(call, MC_CLIENT_WORLD_LOAD_NULL_REFERENCE, "actual mk null receiver", method,
             descriptor, pc);
        return false;
    }
    if (mc_client_loading_screen_retain(receiver) != MC_CLIENT_LOADING_OK) {
        fail(call, MC_CLIENT_WORLD_LOAD_BUSY, "captured actual Loading occurrence", method,
             descriptor, pc);
        return false;
    }
    call->pending_loading = receiver;
    call->pending_loading_owned_call = false;
    mc_client_loading_call_state before = {0};
    (void)mc_client_loading_screen_call_inspect(receiver, &before);
    mc_client_loading_status status = start
                                          ? mc_client_loading_screen_start(receiver, text, error)
                                          : mc_client_loading_screen_message(receiver, text, error);
    mc_client_loading_call_state after = {0};
    if (mc_client_loading_screen_call_inspect(receiver, &after))
        call->pending_loading_owned_call =
            !before.retained && before.failure == NULL && after.retained;
    if (status != MC_CLIENT_LOADING_OK) {
        call->failure.loading = error;
        call->failure.child = (mc_client_world_load_site){
            error->site.class_name, error->site.method, error->site.descriptor, error->site.pc};
        fail(call,
             status == MC_CLIENT_LOADING_BUSY ? MC_CLIENT_WORLD_LOAD_BUSY
             : status == MC_CLIENT_LOADING_UNIMPLEMENTED_DEPENDENCY
                 ? MC_CLIENT_WORLD_LOAD_UNIMPLEMENTED
                 : MC_CLIENT_WORLD_LOAD_CHILD_FAILED,
             error->operation, method, descriptor, pc);
        return false;
    }
    if (mc_client_loading_screen_release(receiver) != MC_CLIENT_LOADING_OK) {
        fail(call, MC_CLIENT_WORLD_LOAD_BUSY, "completed captured Loading native release", method,
             descriptor, pc);
        return false;
    }
    call->pending_loading = NULL;
    call->pending_loading_owned_call = false;
    return true;
}

static bool change_prefix_run(mc_client_world_load_call *call, mc_world_runtime *world,
                              mc_player_string *text, size_t index) {
    client_application *app = call->classes->app;
    change_prefix *prefix = &call->changes[index];
    call->state.last_change_pc = 4;
    if (app->stats_writer == NULL) {
        change_fail(call, MC_CLIENT_WORLD_LOAD_NULL_REFERENCE, "actual xi.b null receiver", 4);
        return false;
    }
    prefix->stats_b_returned = true;                    /* Actual xi.b is literal return0. */
    mc_client_stats_writer *writer = app->stats_writer; /* Independent I8 read. */
    call->state.last_change_pc = 11;
    if (writer == NULL) {
        change_fail(call, MC_CLIENT_WORLD_LOAD_NULL_REFERENCE, "actual xi.c null receiver", 11);
        return false;
    }
    if (mc_client_stats_writer_save(writer, &prefix->statistics_report, &prefix->statistics) !=
        MC_CLIENT_STATS_OK) {
        call->failure.statistics = &prefix->statistics;
        call->failure.statistics_report = &prefix->statistics_report;
        mc_client_stats_file_state source;
        if (mc_client_stats_writer_read_file_state(writer, &source) && source.failure != NULL) {
            prefix->statistics_file = *source.failure;
            call->failure.statistics_file = &prefix->statistics_file;
        }
        call->failure.child = (mc_client_world_load_site){
            prefix->statistics.class_name, prefix->statistics.method, prefix->statistics.descriptor,
            (int)prefix->statistics.bytecode};
        change_fail(call,
                    prefix->statistics.status == MC_CLIENT_STATS_UNIMPLEMENTED_DEPENDENCY
                        ? MC_CLIENT_WORLD_LOAD_UNIMPLEMENTED
                        : MC_CLIENT_WORLD_LOAD_CHILD_FAILED,
                    prefix->statistics.operation, 11);
        return false;
    }
    prefix->stats_c_returned = true;
    call->state.last_change_pc = 16;
    if (app->i != NULL) {
        change_fail(call, MC_CLIENT_WORLD_LOAD_UNIMPLEMENTED,
                    "earlier LivingEntity occurrence outside selected closure", 16);
        return false;
    }
    if (mc_client_view_slot_store(app, NULL) != MC_ENTITY_OK) {
        change_fail(call, MC_CLIENT_WORLD_LOAD_BUSY, "actual i16 native occurrence", 16);
        return false;
    }
    prefix->i_stored = true;
    if (index == 0)
        call->state.initial_i_cleared = true;
    call->state.last_change_pc = 24;
    if (!loading(call, app->loading_screen, text, true, "a", CHANGE_DESC, 24, &prefix->start))
        return false;
    prefix->start_returned = true;
    call->state.last_change_pc = 33;
    if (!loading(call, app->loading_screen, mc_client_loading_classes_empty(call->classes->gui),
                 false, "a", CHANGE_DESC, 33, &prefix->message))
        return false;
    prefix->message_returned = true;
    call->state.last_change_pc = 46;
    if (app->B == NULL) {
        change_fail(call, MC_CLIENT_WORLD_LOAD_NULL_REFERENCE, "actual yo stream null receiver",
                    46);
        return false;
    }
    if (mc_sound_manager_stream(app->B, (mc_sound_text){.is_null = true}, 0, 0, 0, 0, 0,
                                &prefix->sound_report, &prefix->sound) != MC_SOUND_OK) {
        call->failure.sound = &prefix->sound;
        call->failure.child = (mc_client_world_load_site){"yo", "a", "(Ljava/lang/String;FFFFF)V",
                                                          prefix->sound.source_pc};
        change_fail(call, MC_CLIENT_WORLD_LOAD_CHILD_FAILED, "actual yo null stream", 46);
        return false;
    }
    prefix->stream_returned = true;
    if (app->active_world != NULL) {
        change_fail(call, MC_CLIENT_WORLD_LOAD_UNIMPLEMENTED,
                    "actual previous World save with LoadingScreen", 64);
        return false;
    }
    call->state.last_change_pc = 69;
    if (mc_client_application_publish_world(app, world) != MC_EFFECT_RENDERER_OK) {
        change_fail(call, MC_CLIENT_WORLD_LOAD_BUSY, "actual App.f counted publication", 69);
        return false;
    }
    prefix->f_stored = true;
    if (world != NULL) {
        call->world_slot_owned = true;
        call->state.world_published = true;
    }
    return true;
}

static mc_client_singleplayer_controller *controller(mc_client_world_load_call *call, int pc) {
    mc_client_singleplayer_controller *actual = call->classes->app->controller;
    if (actual == NULL) {
        change_fail(call, MC_CLIENT_WORLD_LOAD_NULL_REFERENCE, "actual ob null receiver", pc);
        return NULL;
    }
    if (call->controller_count == sizeof(call->controllers) / sizeof(call->controllers[0]) ||
        mc_client_singleplayer_controller_retain(actual) != MC_SINGLEPLAYER_CONTROLLER_OK) {
        change_fail(call, MC_CLIENT_WORLD_LOAD_BUSY, "captured actual controller occurrence", pc);
        return NULL;
    }
    call->controllers[call->controller_count++] = actual;
    return actual;
}

static bool controller_failure(mc_client_world_load_call *call, int pc) {
    call->failure.controller = &call->controller_error;
    call->failure.child = (mc_client_world_load_site){
        call->controller_error.site.class_name, call->controller_error.site.method,
        call->controller_error.site.descriptor, (int)call->controller_error.site.pc};
    change_fail(call, MC_CLIENT_WORLD_LOAD_CHILD_FAILED, call->controller_error.operation, pc);
    return false;
}

static bool change_world(mc_client_world_load_call *call, mc_world_runtime *world,
                         mc_player_string *text) {
    const size_t index = world == NULL ? 0 : 1;
    client_application *app = call->classes->app;
    if (!change_prefix_run(call, world, text, index))
        return false;
    if (world == NULL) {
        call->state.last_change_pc = 355;
        if (app->h != NULL) {
            change_fail(call, MC_CLIENT_WORLD_LOAD_UNIMPLEMENTED,
                        "earlier LocalPlayer occurrence outside selected closure", 355);
            return false;
        }
        app->h = NULL;
        call->changes[0].h_stored = true;
        call->state.initial_h_cleared = true;
        ++call->state.gc_requests; /* Source System.gc358, advisory native request. */
        app->P = 0;
        call->changes[0].P_stored = true;
        call->state.initial_P_stored = true;
        call->changes[0].source_returned = true;
        call->state.last_change_pc = 366;
        return true;
    }
    call->state.last_change_pc = 81;
    mc_client_singleplayer_controller *actual = controller(call, 81);
    if (actual == NULL)
        return false;
    if (mc_client_singleplayer_controller_change_world(actual, world, &call->controller_error) !=
        MC_SINGLEPLAYER_CONTROLLER_OK)
        return controller_failure(call, 81);
    call->state.last_change_pc = 85;
    bool network = false;
    if (app->active_world != NULL) {
        mc_world_runtime_fields live;
        if (mc_world_runtime_fields_get(app->active_world, &live) != MC_WORLD_STORAGE_OK) {
            change_fail(call, MC_CLIENT_WORLD_LOAD_CHILD_FAILED, "actual live f.B", 85);
            return false;
        }
        network = live.B;
    }
    if (network) {
        change_fail(call, MC_CLIENT_WORLD_LOAD_UNIMPLEMENTED,
                    "network World-change branch outside selected closure",
                    app->h == NULL ? 138 : 122);
        return false;
    }
    /* Actual CLIENT fd.a(Class) is aconst_null0/areturn1. Its dc Class argument
     * is unread; this does not search Entities or fabricate a Player. */
    call->state.last_change_pc = 99;
    if (app->h != NULL) {
        change_fail(call, MC_CLIENT_WORLD_LOAD_UNIMPLEMENTED,
                    "earlier h occurrence outside selected closure", 105);
        return false;
    }
    app->h = NULL; /* Actual h105; supplied gs is genuinely NULL in this overload. */
    mc_world_runtime_fields fields;
    if (mc_world_runtime_fields_get(world, &fields) != MC_WORLD_STORAGE_OK) {
        change_fail(call, MC_CLIENT_WORLD_LOAD_CHILD_FAILED, "actual argument World.B read", 139);
        return false;
    }
    if (fields.B) {
        change_fail(call, MC_CLIENT_WORLD_LOAD_UNIMPLEMENTED,
                    "argument network World outside selected closure", 138);
        return false;
    }
    call->state.last_change_pc = 147;
    mc_client_world_prepare_status preparation =
        mc_client_world_prepare_begin(app, text, &call->prepare);
    if (call->prepare != NULL) {
        (void)mc_client_world_prepare_inspect(call->prepare, &call->terrain);
        call->state.terrain_samples = call->terrain.block_samples;
    }
    if (preparation != MC_CLIENT_WORLD_PREPARE_OK) {
        call->failure.terrain = call->prepare == NULL ? NULL : &call->terrain;
        call->failure.child = site("d", "(Ljava/lang/String;)V",
                                   call->prepare == NULL ? -1 : (int)call->terrain.source_pc);
        change_fail(call,
                    preparation == MC_CLIENT_WORLD_PREPARE_BUSY ? MC_CLIENT_WORLD_LOAD_BUSY
                    : preparation == MC_CLIENT_WORLD_PREPARE_UNIMPLEMENTED
                        ? MC_CLIENT_WORLD_LOAD_UNIMPLEMENTED
                        : MC_CLIENT_WORLD_LOAD_CHILD_FAILED,
                    call->prepare == NULL ? "actual terrain call native allocation"
                                          : call->terrain.operation,
                    147);
        return false;
    }
    if (app->h != NULL) {
        change_fail(call, MC_CLIENT_WORLD_LOAD_UNIMPLEMENTED,
                    "nonnull h outside selected Player-creation closure", 154);
        return false;
    }
    call->state.last_change_pc = 163;
    actual = controller(call, 163);
    if (actual == NULL)
        return false;
    mc_singleplayer_controller_snapshot before, after;
    if (!mc_client_singleplayer_controller_read(actual, &before)) {
        change_fail(call, MC_CLIENT_WORLD_LOAD_BUSY, "actual controller failed-candidate admission",
                    163);
        return false;
    }
    if (mc_client_singleplayer_controller_create_player(actual, world, NULL, &call->state.player,
                                                        &call->controller_error) !=
        MC_SINGLEPLAYER_CONTROLLER_OK) {
        if (before.failed_player == NULL &&
            mc_client_singleplayer_controller_read(actual, &after) && after.failed_player != NULL) {
            call->failed_controller = actual;
            call->failed_candidate = after.failed_player;
            call->state.failed_player = after.failed_player;
        }
        return controller_failure(call, 163);
    }
    call->state.last_change_pc = 169;
    if (mc_entity_retain(call->state.player) != MC_ENTITY_OK) {
        change_fail(call, MC_CLIENT_WORLD_LOAD_BUSY, "actual h169 native occurrence", 169);
        return false;
    }
    call->h_occurrence = call->state.player;
    app->h = call->state.player;
    call->state.player_published = true;
    call->state.last_change_pc = 176;
    if (mc_player_spawn_reset(app->h, NULL, &call->spawn) != MC_ENTITY_OK) {
        call->failure.spawn = &call->spawn;
        call->failure.child =
            (mc_client_world_load_site){call->spawn.source_class, call->spawn.source_method, "()V",
                                        (int)call->spawn.player_bytecode};
        change_fail(call, MC_CLIENT_WORLD_LOAD_CHILD_FAILED, call->spawn.operation, 176);
        return false;
    }
    call->state.last_change_pc = 187;
    actual = controller(call, 187);
    if (actual == NULL)
        return false;
    if (mc_client_singleplayer_controller_prepare_player(actual, app->h, &call->controller_error) !=
        MC_SINGLEPLAYER_CONTROLLER_OK)
        return controller_failure(call, 187);
    /* Capture h191 before NEW lr194, then actual settings199/constructor202. */
    mc_entity *input_receiver = app->h;
    call->state.last_change_pc = 202;
    if (mc_movement_input_create_from_options(app->settings, &call->state.input,
                                              &call->input_error) != MC_MOVEMENT_INPUT_OK) {
        call->failure.input = &call->input_error;
        change_fail(call, MC_CLIENT_WORLD_LOAD_CHILD_FAILED, "actual lr constructor", 202);
        return false;
    }
    call->state.last_change_pc = 205;
    if (mc_local_player_set_movement_input(input_receiver, call->state.input,
                                           &call->player_input) != MC_PLAYER_OK) {
        call->failure.player_input = &call->player_input;
        change_fail(call, MC_CLIENT_WORLD_LOAD_CHILD_FAILED, "actual dc.a205 publication", 205);
        return false;
    }
    call->state.input_published = true;
    if (app->g != NULL) {
        call->state.last_change_pc = 220;
        call->render_owner = app->g;
        mc_client_render_global_state before_renderer;
        if (!mc_client_render_global_inspect(call->render_owner, &before_renderer) ||
            before_renderer.fields.k != NULL) {
            change_fail(call, MC_CLIENT_WORLD_LOAD_UNIMPLEMENTED,
                        "prior renderer World outside selected entry closure", 220);
            return false;
        }
        const mc_client_render_global_status status =
            mc_client_render_global_world_begin(call->render_owner, world, &call->render_call);
        if (call->render_call != NULL) {
            (void)mc_client_render_global_world_inspect(call->render_call, &call->render_state);
            call->render_world_owned = call->render_state.manager_returned ||
                                       call->render_state.k_stored || call->render_state.u_stored;
        }
        if (status != MC_CLIENT_RENDER_GLOBAL_OK) {
            call->failure.renderer = call->render_call == NULL ? NULL : &call->render_state;
            if (call->render_state.failure != NULL)
                call->failure.child =
                    (mc_client_world_load_site){call->render_state.failure->source_class,
                                                call->render_state.failure->source_method,
                                                call->render_state.failure->source_descriptor,
                                                (int)call->render_state.failure->source_pc};
            change_fail(call,
                        status == MC_CLIENT_RENDER_GLOBAL_BUSY ? MC_CLIENT_WORLD_LOAD_BUSY
                        : status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
                            ? MC_CLIENT_WORLD_LOAD_NULL_REFERENCE
                            : MC_CLIENT_WORLD_LOAD_CHILD_FAILED,
                        call->render_state.failure == NULL ? "actual n.a(World) native admission"
                                                           : call->render_state.failure->operation,
                        220);
            if (call->render_state.failure != NULL)
                call->failure.native_policy = call->render_state.failure->native_policy;
            return false;
        }
        call->state.renderer_returned = true;
    }
    if (app->effect_renderer != NULL) {
        call->state.last_change_pc = 235;
        call->effects_owner = app->effect_renderer;
        if (mc_client_effect_renderer_retain(call->effects_owner) != MC_EFFECT_RENDERER_OK) {
            change_fail(call, MC_CLIENT_WORLD_LOAD_BUSY, "captured actual particle manager", 235);
            return false;
        }
        call->effects_held = true;
        mc_effect_renderer_snapshot before_effects;
        if (!mc_client_effect_renderer_read(call->effects_owner, &before_effects) ||
            before_effects.world != NULL) {
            change_fail(call, MC_CLIENT_WORLD_LOAD_UNIMPLEMENTED,
                        "prior particle World outside selected entry closure", 235);
            return false;
        }
        const mc_effect_renderer_status status =
            mc_client_effect_renderer_change_world(call->effects_owner, world, &call->effects);
        mc_effect_renderer_snapshot effects;
        if (mc_client_effect_renderer_read(call->effects_owner, &effects))
            call->effects_world_owned = effects.world == world;
        if (status != MC_EFFECT_RENDERER_OK) {
            call->failure.effects = &call->effects;
            call->failure.child = (mc_client_world_load_site){"dn", call->effects.source_method,
                                                              call->effects.source_descriptor,
                                                              (int)call->effects.source_pc};
            change_fail(call, MC_CLIENT_WORLD_LOAD_CHILD_FAILED,
                        "actual EffectRenderer World-change child", 235);
            return false;
        }
        call->state.effects_returned = true;
    }
    actual = controller(call, 246);
    if (actual == NULL)
        return false;
    if (mc_client_singleplayer_controller_finish_player(actual, app->h, &call->controller_error) !=
        MC_SINGLEPLAYER_CONTROLLER_OK)
        return controller_failure(call, 246);
    /* Supplied gs is NULL in this selected overload, so fd.e254 is skipped.
     * Read the argument World's current provider at w258, not its earlier
     * terrain-preparation capture. */
    call->state.last_change_pc = 258;
    if (mc_world_runtime_fields_get(world, &fields) != MC_WORLD_STORAGE_OK) {
        change_fail(call, MC_CLIENT_WORLD_LOAD_CHILD_FAILED, "actual World.w258 fields", 258);
        return false;
    }
    if (mc_chunk_provider_concrete_kind_of(fields.v) != MC_CHUNK_PROVIDER_MAP_CACHE &&
        fields.v != NULL) {
        change_fail(call, MC_CLIENT_WORLD_LOAD_UNIMPLEMENTED,
                    "actual kx recenter branch outside selected provider closure", 316);
        return false;
    }
    call->state.last_change_pc = 324;
    if (mc_world_runtime_load_player(world, app->h, &call->player_load) != MC_WORLD_STORAGE_OK) {
        call->failure.player_load = &call->player_load;
        if (call->player_load.publication.world_access_failure.receipt != NULL &&
            mc_world_access_failure_retain(&call->player_load.publication.world_access_failure,
                                           &call->captured_player_failure) != MC_WORLD_STORAGE_OK) {
            change_fail(call, MC_CLIENT_WORLD_LOAD_BUSY,
                        "actual Player publication failure receipt capture", 324);
            return false;
        }
        call->failure.child = (mc_client_world_load_site){
            "fd", "a", "(Lgs;)V",
            (int)(call->player_load.print_stack_trace_pending ? call->player_load.catch_bytecode
                                                              : call->player_load.bytecode)};
        change_fail(call, MC_CLIENT_WORLD_LOAD_CHILD_FAILED, call->player_load.operation, 324);
        return false;
    }
    call->state.player_load_returned = true;
    call->state.last_change_pc = 331;
    if (mc_world_runtime_fields_get(world, &fields) != MC_WORLD_STORAGE_OK) {
        change_fail(call, MC_CLIENT_WORLD_LOAD_CHILD_FAILED, "actual World.s331 fields", 331);
        return false;
    }
    if (fields.s) {
        change_fail(call, MC_CLIENT_WORLD_LOAD_UNIMPLEMENTED,
                    "actual new-World save with LoadingScreen", 339);
        return false;
    }
    call->state.last_change_pc = 347;
    if (mc_client_view_slot_store(app, app->h) != MC_ENTITY_OK) {
        change_fail(call, MC_CLIENT_WORLD_LOAD_BUSY, "actual i347 native occurrence", 347);
        return false;
    }
    call->i_occurrence = app->h;
    call->state.view_published = true;
    ++call->state.gc_requests;
    app->P = 0;
    call->changes[1].P_stored = true;
    call->changes[1].source_returned = true;
    call->state.last_change_pc = 366;
    return true;
}

static bool increment(mc_client_world_load_call *call, size_t index, int get_pc, int invoke_pc) {
    mc_client_stats_writer *writer = call->classes->app->stats_writer;
    if (mc_registry_classes_initialize_statistics(call->classes->app->registry, &call->registry) !=
        MC_REGISTRY_OK) {
        call->failure.registry = &call->registry;
        fail(call, MC_CLIENT_WORLD_LOAD_CHILD_FAILED, "actual jl active use", "a", LOAD_DESC,
             get_pc);
        return false;
    }
    mc_registry_statistic_view view;
    if (mc_registry_classes_inspect_statistics(call->classes->app->registry, &view,
                                               &call->registry) != MC_REGISTRY_OK ||
        view.named == NULL) {
        call->failure.registry = &call->registry;
        fail(call, MC_CLIENT_WORLD_LOAD_CHILD_FAILED, "actual jl named statistic", "a", LOAD_DESC,
             get_pc);
        return false;
    }
    if (writer == NULL) {
        fail(call, MC_CLIENT_WORLD_LOAD_NULL_REFERENCE, "actual xi.a statistic null receiver", "a",
             LOAD_DESC, invoke_pc);
        return false;
    }
    change_prefix *prefix = &call->changes[1];
    if (mc_client_stats_writer_increment(writer, view.named[index], 1, &prefix->statistics) !=
        MC_CLIENT_STATS_OK) {
        call->failure.statistics = &prefix->statistics;
        fail(call, MC_CLIENT_WORLD_LOAD_CHILD_FAILED, "actual xi.a statistic increment", "a",
             LOAD_DESC, invoke_pc);
        return false;
    }
    return true;
}

static mc_client_world_load_status save_status(mc_client_save_status status) {
    return status == MC_CLIENT_SAVE_BUSY                       ? MC_CLIENT_WORLD_LOAD_BUSY
           : status == MC_CLIENT_SAVE_LIMIT                    ? MC_CLIENT_WORLD_LOAD_LIMIT
           : status == MC_CLIENT_SAVE_ALLOCATION_FAILED        ? MC_CLIENT_WORLD_LOAD_ALLOCATION
           : status == MC_CLIENT_SAVE_SOURCE_NULL_REFERENCE    ? MC_CLIENT_WORLD_LOAD_NULL_REFERENCE
           : status == MC_CLIENT_SAVE_UNIMPLEMENTED_DEPENDENCY ? MC_CLIENT_WORLD_LOAD_UNIMPLEMENTED
                                                               : MC_CLIENT_WORLD_LOAD_CHILD_FAILED;
}

static bool format_pending(const mc_client_save_format_state *state) {
    return state->pending_argument != NULL || state->pending_world != NULL ||
           state->pending_metadata != NULL || state->pending_candidate != NULL ||
           state->pending_stream != NULL || state->pending_gzip != NULL ||
           state->pending_reader != NULL || state->pending_document != NULL ||
           state->pending_data != NULL || state->pending_files != NULL ||
           state->pending_list != NULL || state->pending_selection != NULL ||
           state->pending_info != NULL || state->pending_name != NULL ||
           state->pending_handler != NULL ||
           (state->handler_error != NULL && state->handler_error->status != MC_WORLD_SOURCE_OK);
}

static void format_finished(mc_client_world_load_call *call) {
    captured_format *capture = &call->formats[call->format_count - 1];
    mc_client_save_format_state after;
    if (mc_client_save_format_read_state(capture->owner, &after, &call->save_cleanup) ==
        MC_CLIENT_SAVE_OK)
        capture->owns_pending = !capture->was_pending && format_pending(&after);
}

static mc_client_save_format *format(mc_client_world_load_call *call, const char *method,
                                     const char *descriptor, int pc) {
    mc_client_save_format *actual = call->classes->app->aa;
    if (actual == NULL) {
        fail(call, MC_CLIENT_WORLD_LOAD_NULL_REFERENCE, "actual nl null receiver", method,
             descriptor, pc);
        return NULL;
    }
    if (call->format_count == sizeof(call->formats) / sizeof(call->formats[0]) ||
        mc_client_save_format_retain(actual, &call->save) != MC_CLIENT_SAVE_OK) {
        call->failure.save = &call->save;
        fail(call, MC_CLIENT_WORLD_LOAD_BUSY, "captured actual SaveFormat occurrence", method,
             descriptor, pc);
        return NULL;
    }
    captured_format *capture = &call->formats[call->format_count++];
    capture->owner = actual;
    mc_client_save_format_state before;
    if (mc_client_save_format_read_state(actual, &before, &call->save_cleanup) !=
        MC_CLIENT_SAVE_OK) {
        fail(call, MC_CLIENT_WORLD_LOAD_STATE, "captured SaveFormat native ownership state", method,
             descriptor, pc);
        return NULL;
    }
    capture->was_pending = format_pending(&before);
    return actual;
}

static bool convert(mc_client_world_load_call *call) {
    client_application *app = call->classes->app;
    /* Capture s1 before the actual Builder and fresh aa17/name20 call. */
    mc_client_loading_screen *captured = app->loading_screen;
    mc_player_string *prefix_literal = NULL;
    if (!shared_literal(call, &app->source_namespace->world_converting, "Converting World to ",
                        CONVERT_DESC, 11, &prefix_literal))
        return false;
    mc_client_save_format *actual = format(call, "b", CONVERT_DESC, 20);
    if (actual == NULL)
        return false;
    const mc_client_save_status format_status =
        mc_client_save_format_name(actual, &call->format_name, &call->save);
    format_finished(call);
    if (format_status != MC_CLIENT_SAVE_OK) {
        call->failure.save = &call->save;
        fail(call, save_status(call->save.status), "actual conversion format name", "b",
             CONVERT_DESC, 20);
        return false;
    }
    static const uint16_t null_name[] = {'n', 'u', 'l', 'l'};
    mc_nbt_string name = call->format_name == NULL ? (mc_nbt_string){null_name, 4}
                                                   : mc_world_info_name_text(call->format_name);
    mc_player_string_view prefix;
    if (!mc_player_string_read(prefix_literal, &prefix)) {
        fail(call, MC_CLIENT_WORLD_LOAD_STATE, "actual conversion prefix String view", "b",
             CONVERT_DESC, 13);
        return false;
    }
    const size_t lead = prefix.length;
    if (name.length > MC_PLAYER_MAX_TEXT_UNITS - lead) {
        fail(call, MC_CLIENT_WORLD_LOAD_LIMIT, "conversion Builder native text bound", "b",
             CONVERT_DESC, 28);
        return false;
    }
    uint16_t *units = malloc((lead + name.length) * sizeof(*units));
    if (units == NULL) {
        fail(call, MC_CLIENT_WORLD_LOAD_ALLOCATION, "conversion Builder native text", "b",
             CONVERT_DESC, 28);
        return false;
    }
    memcpy(units, prefix.units, lead * sizeof(*units));
    if (name.length != 0)
        memcpy(units + lead, name.units, name.length * sizeof(*units));
    mc_player_status status =
        mc_player_string_create(units, lead + name.length, &call->conversion_title);
    free(units);
    if (status != MC_PLAYER_OK) {
        fail(call, MC_CLIENT_WORLD_LOAD_ALLOCATION, "actual conversion Builder.toString", "b",
             CONVERT_DESC, 28);
        return false;
    }
    if (!loading(call, captured, call->conversion_title, true, "b", CONVERT_DESC, 31,
                 &call->conversion_start))
        return false;
    mc_player_string *message = NULL;
    if (!shared_literal(call, &app->source_namespace->world_conversion_message,
                        "This may take a while :)", CONVERT_DESC, 38, &message))
        return false;
    if (!loading(call, app->loading_screen, message, false, "b", CONVERT_DESC, 40,
                 &call->conversion_message))
        return false;
    /* nl.convert has no concrete body yet. It has not returned its ignored
     * boolean57, so the seed0 recursive a62 is genuinely not reached. */
    call->failure.caller = site("a", LOAD_DESC, 24);
    fail(call, MC_CLIENT_WORLD_LOAD_UNIMPLEMENTED, "actual SaveFormat conversion invocation", "b",
         CONVERT_DESC, 52);
    return false;
}

static void run(mc_client_world_load_call *call) {
    client_application *app = call->classes->app;
    call->state.last_outer_pc = 2;
    if (!change_world(call, NULL, mc_client_loading_classes_empty(call->classes->gui)))
        return;
    call->state.initial_detach_returned = true;
    ++call->state.gc_requests; /* Actual outer gc5; no JVM collector is executed. */
    call->state.last_outer_pc = 13;
    mc_client_save_format *actual = format(call, "a", LOAD_DESC, 13);
    if (actual == NULL)
        return;
    const mc_client_save_status conversion = mc_client_save_format_needs_conversion(
        actual, call->state.folder, &call->state.needs_conversion, &call->save);
    format_finished(call);
    if (conversion != MC_CLIENT_SAVE_OK) {
        call->failure.save = &call->save;
        fail(call, save_status(call->save.status), "actual nl.needsConversion", "a", LOAD_DESC, 13);
        return;
    }
    call->state.conversion_checked = true;
    if (call->state.needs_conversion) {
        call->state.last_outer_pc = 24;
        (void)convert(call);
        return;
    }
    call->state.last_outer_pc = 36;
    actual = format(call, "a", LOAD_DESC, 36);
    if (actual == NULL)
        return;
    const mc_client_save_status opened = mc_client_save_format_open(
        actual, call->state.folder, false, &call->state.handler, &call->save);
    format_finished(call);
    if (opened != MC_CLIENT_SAVE_OK) {
        call->failure.save = &call->save;
        if (call->save.handler != NULL) {
            call->source_store = *call->save.handler;
            call->failure.source_store = &call->source_store;
        }
        fail(call, save_status(call->save.status), "actual nl.open source SaveHandler", "a",
             LOAD_DESC, 36);
        return;
    }
    call->state.handler_opened = true;
    if (!mc_world_store_is_source(call->state.handler)) {
        fail(call, MC_CLIENT_WORLD_LOAD_STATE,
             "actual SaveFormat did not return its source SaveHandler", "a", LOAD_DESC, 36);
        return;
    }
    call->state.last_outer_pc = 46;
    if (mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE, app->world_classes,
                                        NULL, &call->state.world,
                                        &call->world_error) != MC_WORLD_STORAGE_OK) {
        call->failure.world = &call->world_error;
        fail(call, MC_CLIENT_WORLD_LOAD_CHILD_FAILED, "actual NEW fd native allocation", "a",
             LOAD_DESC, 46);
        return;
    }
    call->state.last_outer_pc = 54;
    if (mc_world_runtime_random_profile(call->state.world) != app->profile ||
        mc_world_runtime_registry(call->state.world) != app->registry) {
        fail(call, MC_CLIENT_WORLD_LOAD_STATE, "actual World/App profile and Registry namespace",
             "a", LOAD_DESC, 54);
        return;
    }
    mc_world_runtime_world_arguments arguments = {
        .store = call->state.handler, .name = call->state.display, .seed = call->state.seed};
    if (mc_world_runtime_construct_world(call->state.world, &arguments, &call->world_error) !=
        MC_WORLD_STORAGE_OK) {
        call->failure.world = &call->world_error;
        mc_world_runtime_source_construction_state source;
        if (mc_world_runtime_source_construction_inspect(call->state.world, &source)) {
            if (source.saved_data_failure != NULL &&
                source.saved_data_failure->status != MC_WORLD_STORAGE_OK) {
                call->saved_data = *source.saved_data_failure;
                call->failure.saved_data = &call->saved_data;
            }
            const mc_world_source_error *cause =
                source.loader_failure != NULL && source.loader_failure->status != MC_WORLD_SOURCE_OK
                    ? source.loader_failure
                : source.metadata_called ? source.metadata
                                         : source.retention;
            if (cause != NULL && cause->status != MC_WORLD_SOURCE_OK) {
                call->source_store = *cause;
                call->failure.source_store = &call->source_store;
            }
        }
        mc_world_runtime_construction state;
        if (mc_world_runtime_construction_get(call->state.world, &state) == MC_WORLD_STORAGE_OK)
            call->failure.child = (mc_client_world_load_site){
                "fd", "<init>", "(Lwt;Ljava/lang/String;JLxa;)V", (int)state.failed_call_pc};
        fail(call,
             call->world_error.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE
                 ? MC_CLIENT_WORLD_LOAD_UNIMPLEMENTED
                 : MC_CLIENT_WORLD_LOAD_CHILD_FAILED,
             "actual fd constructor", "a", LOAD_DESC, 54);
        return;
    }
    mc_world_runtime_construction constructed;
    if (mc_world_runtime_construction_get(call->state.world, &constructed) != MC_WORLD_STORAGE_OK ||
        constructed.phase != MC_WORLD_RUNTIME_READY) {
        fail(call, MC_CLIENT_WORLD_LOAD_STATE, "actual World constructor has not returned READY",
             "a", LOAD_DESC, 54);
        return;
    }
    call->state.world_constructed = true;
    mc_world_runtime_fields fields;
    (void)mc_world_runtime_fields_get(call->state.world, &fields);
    call->state.last_outer_pc = fields.s ? 75 : 108;
    if (!increment(call, fields.s ? 1u : 2u, fields.s ? 71 : 104, fields.s ? 75 : 108))
        return;
    call->state.statistics_first_returned = true;
    call->state.last_outer_pc = fields.s ? 86 : 119;
    if (!increment(call, 0, fields.s ? 82 : 115, fields.s ? 86 : 119))
        return;
    call->state.statistics_second_returned = true;
    if (!shared_literal(call,
                        fields.s ? &app->source_namespace->world_generating
                                 : &app->source_namespace->world_loading,
                        fields.s ? "Generating level" : "Loading level", LOAD_DESC,
                        fields.s ? 92 : 125, &call->level_text))
        return;
    call->state.last_outer_pc = fields.s ? 94 : 127;
    if (!change_world(call, call->state.world, call->level_text))
        return;
    call->state.last_outer_pc = 130;
    call->state.source_returned = true;
}

mc_client_world_load_status mc_client_world_load_begin(mc_client_world_load_classes *classes,
                                                       mc_world_info_name *folder,
                                                       mc_world_info_name *display, int64_t seed,
                                                       mc_client_world_load_call **out) {
    if (classes == NULL || out == NULL || *out != NULL)
        return MC_CLIENT_WORLD_LOAD_ARGUMENT;
    if (classes->active || classes->retiring || classes->calls != 0)
        return MC_CLIENT_WORLD_LOAD_BUSY;
    mc_client_world_load_call *call = calloc(1, sizeof(*call));
    if (call == NULL)
        return MC_CLIENT_WORLD_LOAD_ALLOCATION;
    call->classes = classes;
    call->state.app = classes->app;
    call->state.seed = seed;
    call->state.last_outer_pc = call->state.last_change_pc = -1;
    call->state.active = classes->active = true;
    ++classes->calls;
    *out = call;
    if (folder != NULL && mc_world_info_name_retain(folder) != MC_WORLD_INFO_OK) {
        fail(call, MC_CLIENT_WORLD_LOAD_LIMIT, "native folder argument hold", "a", LOAD_DESC, -1);
        goto done;
    }
    call->state.folder = folder;
    call->folder_held = folder != NULL;
    if (display != NULL && mc_world_info_name_retain(display) != MC_WORLD_INFO_OK) {
        fail(call, MC_CLIENT_WORLD_LOAD_LIMIT, "native display argument hold", "a", LOAD_DESC, -1);
        goto done;
    }
    call->state.display = display;
    call->display_held = display != NULL;
    run(call);
done:
    call->state.active = classes->active = false;
    return call->failure.status;
}

bool mc_client_world_load_inspect(const mc_client_world_load_call *call,
                                  mc_client_world_load_state *out) {
    if (call == NULL || out == NULL || call->state.active)
        return false;
    *out = call->state;
    out->prepare = call->prepare;
    out->failure = &call->failure;
    out->cleanup = &call->cleanup;
    return true;
}

static mc_client_world_load_status cleanup_fail(mc_client_world_load_call *call,
                                                mc_client_world_load_status status,
                                                const char *operation) {
    call->cleanup.status = status;
    call->cleanup.operation = operation;
    call->cleanup.native_policy = true;
    call->state.active = call->classes->active = false;
    return status;
}

static void world_cleanup_cause(mc_client_world_load_call *call) {
    mc_world_runtime_source_construction_state source;
    if (!mc_world_runtime_source_construction_inspect(call->state.world, &source))
        return;
    if (source.saved_data_cleanup != NULL &&
        source.saved_data_cleanup->status != MC_WORLD_STORAGE_OK) {
        call->saved_data_cleanup = *source.saved_data_cleanup;
        call->cleanup.saved_data = &call->saved_data_cleanup;
    }
    const mc_world_source_error *cause =
        source.loader_cleanup != NULL && source.loader_cleanup->status != MC_WORLD_SOURCE_OK
            ? source.loader_cleanup
            : source.store_cleanup;
    if (cause != NULL && cause->status != MC_WORLD_SOURCE_OK) {
        call->world_source_cleanup = *cause;
        call->cleanup.source_store = &call->world_source_cleanup;
    }
}

/* Exact existing cleanup prefix shared by full retirement and the new native
 * terminal preparation. Both active guards belong to the admitted caller.
 * Every successful slot consumption is final; failure clears the guards via
 * cleanup_fail and preserves every remaining occurrence. */
static mc_client_world_load_status cleanup_prefix(mc_client_world_load_call *call) {
    client_application *app = call->classes->app;
    if (call->prepare != NULL) {
        if (mc_client_world_prepare_retire(&call->prepare) != MC_CLIENT_WORLD_PREPARE_OK) {
            (void)mc_client_world_prepare_inspect(call->prepare, &call->terrain);
            call->cleanup.terrain = &call->terrain;
            return cleanup_fail(call, MC_CLIENT_WORLD_LOAD_BUSY,
                                "actual terrain child native retirement");
        }
        call->failure.terrain = NULL;
    }
    if (call->pending_loading != NULL) {
        if (call->pending_loading_owned_call) {
            if (mc_client_loading_screen_retire_call(
                    call->pending_loading, &call->loading_cleanup) != MC_CLIENT_LOADING_OK) {
                call->cleanup.loading = &call->loading_cleanup;
                return cleanup_fail(call, MC_CLIENT_WORLD_LOAD_BUSY,
                                    "actual retained Loading call retirement");
            }
            call->pending_loading_owned_call = false;
        }
        /* A BUSY invocation can have observed a foreign retained frame. Its
         * original caller alone owns that retirement right; only the captured
         * native mk occurrence belongs to this load attempt in that case. */
        call->failure.loading = NULL;
        if (mc_client_loading_screen_release(call->pending_loading) != MC_CLIENT_LOADING_OK)
            return cleanup_fail(call, MC_CLIENT_WORLD_LOAD_BUSY,
                                "captured Loading occurrence release");
        call->pending_loading = NULL;
    }
    mc_world_access_failure_release(&call->captured_player_failure);
    call->player_load.publication.world_access_failure = (mc_world_access_failure){0};
    if (call->render_call != NULL) {
        if (mc_client_render_global_world_retire(&call->render_call) !=
            MC_CLIENT_RENDER_GLOBAL_OK) {
            (void)mc_client_render_global_world_inspect(call->render_call, &call->render_state);
            call->cleanup.renderer = &call->render_state;
            return cleanup_fail(call, MC_CLIENT_WORLD_LOAD_BUSY,
                                "actual renderer World-call native retirement");
        }
        call->failure.renderer = NULL;
        call->render_state = (mc_client_render_global_world_state){0};
    }
    if (call->render_world_owned) {
        const mc_client_render_global_status detached =
            call->state.terminal_prepare_selected
                ? mc_client_render_global_terminal_detach(call->render_owner, &call->render_cleanup)
                : mc_client_render_global_world_detach(call->render_owner, &call->render_cleanup);
        if (detached != MC_CLIENT_RENDER_GLOBAL_OK) {
            call->cleanup.render_global = &call->render_cleanup;
            return cleanup_fail(call, MC_CLIENT_WORLD_LOAD_BUSY,
                                "actual renderer World native detachment");
        }
        call->render_world_owned = false;
        if (call->state.terminal_prepare_selected)
            call->render_owner = NULL; /* This terminal N source view has ended. */
    }
    if (call->world_slot_owned && app->t != NULL &&
        mc_client_entity_renderer_pick_detach(app->t, &call->pick_cleanup) != MC_CLIENT_PICK_OK) {
        call->cleanup.picking = &call->pick_cleanup;
        return cleanup_fail(call, MC_CLIENT_WORLD_LOAD_BUSY,
                            "actual EntityRenderer picking native detachment");
    }
    return MC_CLIENT_WORLD_LOAD_OK;
}

mc_client_world_load_status mc_client_world_load_terminal_prepare(mc_client_world_load_call *call) {
    if (call == NULL)
        return MC_CLIENT_WORLD_LOAD_ARGUMENT;
    if (call->state.active || call->classes->active)
        return MC_CLIENT_WORLD_LOAD_BUSY;
    if (call->state.retiring && !call->state.terminal_prepare_selected)
        return MC_CLIENT_WORLD_LOAD_BUSY;
    const mc_client_world_load_status admission =
        mc_client_world_load_classes_terminal_end(call->classes);
    if (admission != MC_CLIENT_WORLD_LOAD_OK)
        return admission;
    call->state.retiring = call->state.terminal_prepare_selected = true;
    if (call->state.terminal_prepared)
        return MC_CLIENT_WORLD_LOAD_OK;
    call->state.active = call->classes->active = true;
    call->cleanup = (mc_client_world_load_error){0};
    if (cleanup_prefix(call) != MC_CLIENT_WORLD_LOAD_OK)
        return call->cleanup.status;
    call->state.terminal_prepared = true;
    call->state.active = call->classes->active = false;
    return MC_CLIENT_WORLD_LOAD_OK;
}

static mc_world_storage_status terminal_admission_result(mc_world_storage_error *error,
                                                         mc_world_storage_status status,
                                                         const char *operation) {
    if (error != NULL)
        *error = (mc_world_storage_error){status, operation, 0};
    return status;
}

static mc_world_storage_status terminal_empty_app(const client_application *app,
                                                  mc_world_storage_error *error) {
    if (app->active_world != NULL || app->h != NULL || app->i != NULL ||
        app->view_occurrence != NULL || app->y != NULL)
        return terminal_admission_result(error, MC_WORLD_STORAGE_BUSY,
                                         "Load terminal empty App has World/Player/view roots");
    if (app->startup == NULL || app->startup->app != app || app->startup->rendering == NULL ||
        app->effect_renderer_constructing || app->effect_renderer == NULL ||
        app->effect_renderer != app->effect_renderer_retained ||
        app->startup->rendering->particle != app->effect_renderer ||
        app->startup->rendering->global == NULL || app->g != app->startup->rendering->global ||
        app->aa == NULL || app->aa != app->startup->format_candidate || app->t == NULL ||
        app->t != app->startup->entity_renderer_candidate ||
        app->t->phase != MC_CLIENT_ENTITY_RENDERER_READY)
        return terminal_admission_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                         "Load terminal empty App owner provenance");
    if (app->startup->rendering->world_held || app->startup->rendering->particle_world != NULL)
        return terminal_admission_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                         "Load terminal empty startup rendering World occurrence");
    if (app->startup->active || app->startup->rendering->active || app->t->active ||
        app->t->pick_call != NULL || app->t->tick_call != NULL || app->t->focus_call != NULL ||
        app->t->world_frame_call != NULL)
        return terminal_admission_result(error, MC_WORLD_STORAGE_BUSY,
                                         "Load terminal empty App child operation entered");
    if (app->t->pick_m_occurrence != NULL || app->t->pick_y_occurrence != NULL ||
        app->t->pick_m_published || app->t->pick_y_app != NULL || app->t->fields.m != NULL)
        return terminal_admission_result(error, MC_WORLD_STORAGE_BUSY,
                                         "Load terminal empty App picking occurrences");
    mc_effect_renderer_terminal_view effects;
    const mc_effect_renderer_status effect_status =
        mc_client_effect_renderer_terminal_inspect(app->effect_renderer, &effects, NULL);
    if (effect_status != MC_EFFECT_RENDERER_OK)
        return terminal_admission_result(error,
                                         effect_status == MC_EFFECT_RENDERER_BUSY
                                             ? MC_WORLD_STORAGE_BUSY
                                             : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                         "Load terminal empty App particle owner");
    if (effects.classes != app->entities || effects.render_engine != app->render_engine ||
        effects.world_slot == NULL || *effects.world_slot != NULL)
        return terminal_admission_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                         "Load terminal empty App particle World provenance");
    if (app->controller != NULL) {
        mc_singleplayer_controller_terminal_view controller;
        const mc_singleplayer_controller_status controller_status =
            mc_client_singleplayer_controller_terminal_inspect(app->controller, &controller, NULL);
        if (controller_status != MC_SINGLEPLAYER_CONTROLLER_OK)
            return terminal_admission_result(error,
                                             controller_status == MC_SINGLEPLAYER_CONTROLLER_BUSY
                                                 ? MC_WORLD_STORAGE_BUSY
                                                 : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                             "Load terminal empty App controller owner");
        if (controller.app != app || controller.classes != app->entities ||
            controller.profile != app->profile || controller.failed_player_slot == NULL ||
            *controller.failed_player_slot != NULL)
            return terminal_admission_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                             "Load terminal empty App controller candidate");
    }
    mc_client_save_format_state format_state;
    if (mc_client_save_format_read_state(app->aa, &format_state, NULL) != MC_CLIENT_SAVE_OK ||
        format_state.phase != MC_CLIENT_SAVE_READY)
        return terminal_admission_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                         "Load terminal empty App SaveFormat owner");
    if (format_state.active || format_state.retiring)
        return terminal_admission_result(error, MC_WORLD_STORAGE_BUSY,
                                         "Load terminal empty App SaveFormat active");
    if (format_state.pending_handler != NULL)
        return terminal_admission_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                         "Load terminal empty App retained SaveHandler");
    const mc_client_render_global *renderer = app->startup->rendering->global;
    if (renderer != NULL) {
        if (renderer->phase != MC_CLIENT_RENDER_GLOBAL_READY &&
            renderer->phase != MC_CLIENT_RENDER_GLOBAL_RETIRING)
            return terminal_admission_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                             "Load terminal empty App RenderGlobal owner phase");
        /* scene_guard is the persistent graphics-context lease acquired by
         * allocation, not an entered operation or a World occurrence. */
        if (renderer->active || renderer->world_call != NULL || renderer->sky_call != NULL ||
            renderer->cull_call != NULL || renderer->update_call != NULL ||
            renderer->submit_call != NULL || renderer->entities_call != NULL)
            return terminal_admission_result(error, MC_WORLD_STORAGE_BUSY,
                                             "Load terminal empty App RenderGlobal call");
        if (renderer->fields.k != NULL || renderer->world_occurrence != NULL ||
            renderer->manager_world_occurrence != NULL || renderer->world_manager != NULL ||
            renderer->fields.u != NULL || renderer->cv_owners != NULL ||
            renderer->chunk_owners != NULL)
            return terminal_admission_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                             "Load terminal empty App RenderGlobal World rights");
        if (renderer->access != NULL) {
            mc_render_global_access_state access;
            if (!mc_render_global_access_inspect(renderer->access, &access))
                return terminal_admission_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                                 "Load terminal empty App WorldAccess owner");
            if (access.registrations != 0 || access.active_callbacks != 0 ||
                access.retained_failures != 0)
                return terminal_admission_result(error, MC_WORLD_STORAGE_BUSY,
                                                 "Load terminal empty App WorldAccess rights");
        }
    }
    return terminal_admission_result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_client_world_load_classes_terminal_validate(
    const mc_client_world_load_classes *classes, client_application *expected_app,
    size_t owned_call_count, mc_world_storage_error *error) {
    if (classes == NULL || expected_app == NULL)
        return terminal_admission_result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                         "Load terminal class owner and actual App required");
    if (classes->app != expected_app || !classes->app_borrow || classes->gui == NULL ||
        !classes->retiring || expected_app->startup == NULL ||
        expected_app->startup->app != expected_app ||
        expected_app->constructor_phase != MC_CLIENT_APP_READY ||
        expected_app->loading_screen == NULL ||
        mc_client_loading_screen_classes(expected_app->loading_screen) != classes->gui)
        return terminal_admission_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                         "Load terminal prepared class/App provenance");
    if (classes->active || expected_app->startup->active)
        return terminal_admission_result(error, MC_WORLD_STORAGE_BUSY,
                                         "Load terminal class/App operation entered");
    if (classes->calls > owned_call_count)
        return terminal_admission_result(error, MC_WORLD_STORAGE_BUSY,
                                         "Load terminal unrepresented call occurrence");
    if (classes->calls < owned_call_count)
        return terminal_admission_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                         "Load terminal call occurrence deficit");
    return owned_call_count == 0 ? terminal_empty_app(expected_app, error)
                                 : terminal_admission_result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_client_world_load_terminal_classify(
    const mc_client_world_load_call *call, const mc_client_world_load_classes *expected_classes,
    mc_client_terminal_storage_scope *out, mc_world_storage_error *error) {
    if (call == NULL || expected_classes == NULL || out == NULL)
        return terminal_admission_result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                         "Load terminal classification owners and output required");
    const mc_client_world_load_classes *classes = call->classes;
    if (classes != expected_classes || classes->app == NULL || call->state.app != classes->app ||
        !classes->retiring || !call->state.retiring || !call->state.terminal_prepare_selected ||
        !call->state.terminal_prepared || classes->calls == 0 ||
        call->controller_count > sizeof(call->controllers) / sizeof(call->controllers[0]) ||
        call->format_count > sizeof(call->formats) / sizeof(call->formats[0]))
        return terminal_admission_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                         "Load terminal classification intact prepared scope");
    client_application *app = classes->app;
    if (call->state.active || classes->active || app->startup == NULL || app->startup->app != app ||
        app->startup->active)
        return terminal_admission_result(error, MC_WORLD_STORAGE_BUSY,
                                         "Load terminal classification active App/call");
    if (call->world_retired || call->cache_retired || call->prepare != NULL ||
        call->pending_loading != NULL || call->pending_loading_owned_call ||
        call->render_call != NULL || call->render_world_owned || call->failed_controller != NULL ||
        call->failed_candidate != NULL || call->state.failed_player != NULL)
        return terminal_admission_result(
            error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
            "Load terminal classification partial cleanup or candidate");
    for (size_t i = 0; i < call->controller_count; ++i)
        if (call->controllers[i] == NULL)
            return terminal_admission_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                             "Load terminal consumed controller capture");
    for (size_t i = 0; i < call->format_count; ++i)
        if (call->formats[i].owner == NULL ||
            (call->formats[i].was_pending && call->formats[i].owns_pending))
            return terminal_admission_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                             "Load terminal consumed Format capture");

    if (call->state.world != NULL) {
        mc_world_runtime_construction world;
        if (!call->state.source_returned || call->failure.status != MC_CLIENT_WORLD_LOAD_OK ||
            !call->state.handler_opened || !call->state.world_constructed ||
            !call->state.world_published || !call->state.player_published ||
            !call->state.input_published || !call->state.view_published ||
            call->state.handler == NULL || call->state.player == NULL ||
            call->state.input == NULL || !call->world_slot_owned || !call->effects_world_owned ||
            !call->effects_held || call->effects_owner == NULL || call->render_owner != NULL ||
            call->h_occurrence != call->state.player || call->i_occurrence != call->state.player ||
            app->active_world != call->state.world || app->h != call->state.player ||
            app->i != call->state.player || app->view_occurrence != call->state.player ||
            mc_world_runtime_construction_get(call->state.world, &world) != MC_WORLD_STORAGE_OK ||
            world.mode != MC_WORLD_RUNTIME_GAME || world.phase != MC_WORLD_RUNTIME_READY ||
            world.entry != MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE)
            return terminal_admission_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                             "Load terminal classification partial selected World");
        *out = MC_CLIENT_TERMINAL_SELECTED_LOAD;
        return terminal_admission_result(error, MC_WORLD_STORAGE_OK, NULL);
    }
    if (call->state.handler != NULL || call->state.player != NULL || call->state.input != NULL ||
        call->h_occurrence != NULL || call->i_occurrence != NULL || call->world_slot_owned ||
        call->effects_world_owned || call->effects_held || call->effects_owner != NULL ||
        call->render_owner != NULL || call->state.handler_opened || call->state.world_constructed ||
        call->state.world_published || call->state.player_published ||
        call->state.input_published || call->state.renderer_returned ||
        call->state.effects_returned || call->state.player_load_returned ||
        call->state.view_published || call->state.source_returned)
        return terminal_admission_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                         "Load terminal classification acquired storage graph");
    mc_world_storage_status status = terminal_empty_app(app, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    for (size_t i = 0; i < call->controller_count; ++i) {
        mc_singleplayer_controller_terminal_view controller;
        const mc_singleplayer_controller_status child =
            mc_client_singleplayer_controller_terminal_inspect(call->controllers[i], &controller,
                                                               NULL);
        if (child != MC_SINGLEPLAYER_CONTROLLER_OK)
            return terminal_admission_result(error,
                                             child == MC_SINGLEPLAYER_CONTROLLER_BUSY
                                                 ? MC_WORLD_STORAGE_BUSY
                                                 : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                             "Load terminal empty captured controller");
        if (controller.app != app || controller.classes != app->entities ||
            controller.profile != app->profile || controller.failed_player_slot == NULL ||
            *controller.failed_player_slot != NULL)
            return terminal_admission_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                             "Load terminal empty captured controller candidate");
    }
    for (size_t i = 0; i < call->format_count; ++i) {
        mc_client_save_format_state format_state;
        const captured_format *capture = &call->formats[i];
        if (mc_client_save_format_read_state(capture->owner, &format_state, NULL) !=
                MC_CLIENT_SAVE_OK ||
            format_state.phase != MC_CLIENT_SAVE_READY)
            return terminal_admission_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                             "Load terminal empty captured Format owner");
        if (format_state.active || format_state.retiring)
            return terminal_admission_result(error, MC_WORLD_STORAGE_BUSY,
                                             "Load terminal empty captured Format active");
        if (format_state.pending_handler != NULL)
            return terminal_admission_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                             "Load terminal empty captured Format Handler");
        if (format_pending(&format_state)) {
            size_t pending_owners = 0;
            for (size_t j = 0; j < call->format_count; ++j)
                if (call->formats[j].owner == capture->owner && call->formats[j].owns_pending)
                    ++pending_owners;
            if (pending_owners != 1)
                return terminal_admission_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                                 "Load terminal empty Format prefix ownership");
        }
    }
    *out = MC_CLIENT_TERMINAL_EMPTY_STORAGE;
    return terminal_admission_result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status
mc_client_world_load_terminal_admit(const mc_client_world_load_call *call,
                                    mc_client_singleplayer_controller *const *controller_owner,
                                    const mc_world_terminal_limits *limits,
                                    mc_world_terminal_inventory *out,
                                    mc_world_storage_error *error) {
    if (call == NULL || controller_owner == NULL || out == NULL)
        return terminal_admission_result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                         "Load terminal actual owner slots and output required");
    const mc_client_world_load_classes *classes = call->classes;
    if (classes == NULL || classes->app == NULL)
        return terminal_admission_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                         "Load terminal live class/App provenance");
    client_application *app = classes->app;
    if (call->state.active || classes->active || (app->startup != NULL && app->startup->active))
        return terminal_admission_result(error, MC_WORLD_STORAGE_BUSY,
                                         "Load terminal owner operation entered");
    if (!classes->retiring || classes->calls != 1 || !call->state.terminal_prepare_selected ||
        !call->state.terminal_prepared || !call->state.source_returned ||
        call->failure.status != MC_CLIENT_WORLD_LOAD_OK || call->state.app != app ||
        call->state.world == NULL || call->state.player == NULL || call->state.input == NULL ||
        call->state.handler == NULL || call->world_retired || call->cache_retired ||
        call->failed_controller != NULL || call->failed_candidate != NULL ||
        call->state.failed_player != NULL || call->prepare != NULL ||
        call->pending_loading != NULL || call->render_call != NULL || call->render_world_owned)
        return terminal_admission_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                         "Load terminal intact fully published prepared scope");
    if (app->startup == NULL || app->startup->app != app || app->startup->rendering == NULL ||
        app->effect_renderer_constructing || app->startup->rendering->active)
        return terminal_admission_result(error, MC_WORLD_STORAGE_BUSY,
                                         "Load terminal live inactive startup/rendering owners");
    if (call->h_occurrence != call->state.player || call->i_occurrence != call->state.player ||
        app->h != call->h_occurrence || app->i != call->i_occurrence ||
        app->view_occurrence != call->i_occurrence || !call->world_slot_owned ||
        app->active_world != call->state.world || !call->effects_world_owned ||
        !call->effects_held || call->effects_owner == NULL ||
        app->effect_renderer != call->effects_owner ||
        app->effect_renderer_retained != call->effects_owner ||
        app->startup->rendering->particle != call->effects_owner)
        return terminal_admission_result(error, MC_WORLD_STORAGE_BUSY,
                                         "Load terminal actual App/Player/World slots changed");
    mc_world_runtime_fields fields;
    if (mc_world_runtime_fields_get(call->state.world, &fields) != MC_WORLD_STORAGE_OK ||
        fields.w != call->state.handler ||
        mc_world_runtime_random_profile(call->state.world) != app->profile)
        return terminal_admission_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                         "Load terminal actual World/handler/profile provenance");

    mc_client_singleplayer_controller *controller = *controller_owner;
    if (controller == NULL || app->controller != controller || call->controller_count == 0 ||
        call->controller_count > sizeof(call->controllers) / sizeof(call->controllers[0]))
        return terminal_admission_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                         "Load terminal actual transition controller owner");
    size_t controller_holds = 1; /* The genuine transition-owned slot. */
    for (size_t i = 0; i < call->controller_count; ++i) {
        if (call->controllers[i] != controller)
            return terminal_admission_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                             "Load terminal multiple captured controllers");
        ++controller_holds; /* Each real captured owning slot, not App's alias. */
    }
    mc_singleplayer_controller_terminal_view controller_view;
    mc_singleplayer_controller_error controller_error;
    const mc_singleplayer_controller_status controller_status =
        mc_client_singleplayer_controller_terminal_inspect(controller, &controller_view,
                                                           &controller_error);
    if (controller_status != MC_SINGLEPLAYER_CONTROLLER_OK)
        return terminal_admission_result(error,
                                         controller_status == MC_SINGLEPLAYER_CONTROLLER_BUSY
                                             ? MC_WORLD_STORAGE_BUSY
                                             : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                         controller_error.operation);
    if (controller_view.app != app || controller_view.classes != app->entities ||
        controller_view.profile != app->profile || *controller_view.failed_player_slot != NULL)
        return terminal_admission_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                         "Load terminal controller namespace/candidate provenance");
    if (controller_view.holds != controller_holds)
        return terminal_admission_result(error, MC_WORLD_STORAGE_BUSY,
                                         "Load terminal external controller occurrence");

    mc_effect_renderer_terminal_view effects_view;
    mc_effect_renderer_error effects_error;
    const mc_effect_renderer_status effects_status = mc_client_effect_renderer_terminal_inspect(
        call->effects_owner, &effects_view, &effects_error);
    if (effects_status != MC_EFFECT_RENDERER_OK)
        return terminal_admission_result(error,
                                         effects_status == MC_EFFECT_RENDERER_BUSY
                                             ? MC_WORLD_STORAGE_BUSY
                                             : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                         effects_error.operation);
    if (effects_view.classes != app->entities || effects_view.render_engine != app->render_engine ||
        *effects_view.world_slot != call->state.world)
        return terminal_admission_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                         "Load terminal effect namespace/Engine/World provenance");
    const size_t effects_holds = 1u + (call->effects_held ? 1u : 0u);
    if (effects_view.holds != effects_holds)
        return terminal_admission_result(error, MC_WORLD_STORAGE_BUSY,
                                         "Load terminal external effect occurrence");

    mc_local_player_snapshot player;
    mc_movement_input_state input;
    if (mc_local_player_get_snapshot(call->state.player, &player) != MC_ENTITY_OK ||
        player.minecraft != app || player.input != call->state.input ||
        !mc_movement_input_read_state(call->state.input, &input) ||
        input.kind != MC_MOVEMENT_INPUT_FROM_OPTIONS || input.settings != app->settings)
        return terminal_admission_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                         "Load terminal actual Player/Input/Settings provenance");
    size_t input_holds = 1; /* The Load's allocated Input slot. */
    if (player.input != NULL)
        ++input_holds; /* The SAME Player's actual dc.a owning field, counted once. */
    if (input.native_holds != input_holds)
        return terminal_admission_result(error, MC_WORLD_STORAGE_BUSY,
                                         "Load terminal external Input occurrence");

    mc_world_terminal_source_view source;
    mc_world_storage_status source_status =
        mc_world_runtime_terminal_source_inspect(call->state.world, &source, error);
    if (source_status != MC_WORLD_STORAGE_OK)
        return source_status;
    const mc_world_terminal_store_anchor stores[] = {{&call->state.handler},
                                                     {source.store_slots[0]},
                                                     {source.store_slots[1]},
                                                     {source.store_slots[2]}};
    source_status = mc_world_store_terminal_source_admit(call->state.handler, stores,
                                                         sizeof(stores) / sizeof(stores[0]), error);
    if (source_status != MC_WORLD_STORAGE_OK)
        return source_status;

    const mc_world_terminal_entity_anchor entities[] = {
        {&call->state.player}, {&call->h_occurrence}, {&app->view_occurrence}};
    const mc_world_terminal_world_anchor worlds[] = {{&app->active_world},
                                                     {effects_view.world_slot}};
    const mc_world_terminal_anchors anchors = {entities, sizeof(entities) / sizeof(entities[0]),
                                               worlds, sizeof(worlds) / sizeof(worlds[0])};
    return mc_world_runtime_terminal_admit_owned(call->state.world, &anchors, limits, out, error);
}

mc_client_world_load_status mc_client_world_load_retire(mc_client_world_load_call **slot) {
    if (slot == NULL)
        return MC_CLIENT_WORLD_LOAD_ARGUMENT;
    mc_client_world_load_call *call = *slot;
    if (call == NULL)
        return MC_CLIENT_WORLD_LOAD_OK;
    if (call->state.active || call->classes->active)
        return MC_CLIENT_WORLD_LOAD_BUSY;
    call->state.retiring = true;
    call->state.active = call->classes->active = true;
    call->cleanup = (mc_client_world_load_error){0};
    client_application *app = call->classes->app;
    if (!call->state.terminal_prepared) {
        if (cleanup_prefix(call) != MC_CLIENT_WORLD_LOAD_OK)
            return call->cleanup.status;
        if (call->state.terminal_prepare_selected)
            call->state.terminal_prepared = true;
    }
    if (call->effects_held) {
        if (mc_client_effect_renderer_release(call->effects_owner) != MC_EFFECT_RENDERER_OK)
            return cleanup_fail(call, MC_CLIENT_WORLD_LOAD_BUSY,
                                "captured particle manager occurrence release");
        call->effects_held = false;
    }
    if (call->effects_world_owned) {
        if (mc_client_effect_renderer_retire_world(call->effects_owner, call->state.world,
                                                   &call->effects_cleanup) !=
            MC_EFFECT_RENDERER_OK) {
            call->cleanup.effects = &call->effects_cleanup;
            return cleanup_fail(call, MC_CLIENT_WORLD_LOAD_BUSY,
                                "actual particle manager World native detachment");
        }
        call->effects_world_owned = false;
    }
    if (call->failed_controller != NULL) {
        mc_singleplayer_controller_snapshot current;
        if (!mc_client_singleplayer_controller_read(call->failed_controller, &current) ||
            (current.failed_player != NULL && current.failed_player != call->failed_candidate))
            return cleanup_fail(call, MC_CLIENT_WORLD_LOAD_BUSY,
                                "actual owned failed Player candidate changed");
        if (current.failed_player != NULL &&
            mc_client_singleplayer_controller_clear_failed_player(call->failed_controller,
                                                                  &call->controller_cleanup) !=
                MC_SINGLEPLAYER_CONTROLLER_OK) {
            call->cleanup.controller = &call->controller_cleanup;
            return cleanup_fail(call, MC_CLIENT_WORLD_LOAD_BUSY,
                                "actual controller failed Player retirement");
        }
        call->failed_controller = NULL;
        call->failed_candidate = NULL;
        call->state.failed_player = NULL;
    }
    if (call->i_occurrence != NULL) {
        if (app->i != call->i_occurrence)
            return cleanup_fail(call, MC_CLIENT_WORLD_LOAD_BUSY,
                                "actual owned App.i occurrence changed");
        if (mc_client_view_slot_store(app, NULL) != MC_ENTITY_OK)
            return cleanup_fail(call, MC_CLIENT_WORLD_LOAD_BUSY,
                                "actual App.i native occurrence cleanup");
        call->i_occurrence = NULL;
    }
    if (call->h_occurrence != NULL) {
        if (app->h != call->h_occurrence)
            return cleanup_fail(call, MC_CLIENT_WORLD_LOAD_BUSY,
                                "actual owned App.h occurrence changed");
        app->h = NULL;
        mc_entity_release(call->h_occurrence);
        call->h_occurrence = NULL;
    }
    mc_entity_release(call->state.player);
    call->state.player = NULL;
    if (call->world_slot_owned) {
        if (app->active_world != call->state.world)
            return cleanup_fail(call, MC_CLIENT_WORLD_LOAD_BUSY,
                                "actual owned App.f occurrence changed");
        if (mc_client_application_publish_world(app, NULL) != MC_EFFECT_RENDERER_OK)
            return cleanup_fail(call, MC_CLIENT_WORLD_LOAD_BUSY,
                                "actual owned App.f native release");
        call->world_slot_owned = false;
    }
    if (call->state.world != NULL) {
        call->cleanup.world = &call->cleanup_world;
        if (!call->world_retired) {
            if (mc_world_runtime_retire_entity_containers(
                    call->state.world, &call->cleanup_world) != MC_WORLD_STORAGE_OK)
                return cleanup_fail(call, MC_CLIENT_WORLD_LOAD_BUSY,
                                    "World joint Entity container retirement");
            mc_world_runtime_fields fields;
            if (mc_world_runtime_fields_get(call->state.world, &fields) != MC_WORLD_STORAGE_OK)
                return cleanup_fail(call, MC_CLIENT_WORLD_LOAD_CLEANUP_FAILED,
                                    "World native retirement fields");
            if (!call->cache_retired && fields.v != NULL) {
                mc_chunk_provider_fields provider;
                if (mc_chunk_provider_bind_fields(fields.v, &provider, &call->cleanup_world) !=
                        MC_WORLD_STORAGE_OK ||
                    mc_chunk_pending_clear(provider.pending) != MC_CHUNK_ACQUIRE_OK ||
                    !mc_chunk_cache_init(provider.cache, provider.cache->entries,
                                         provider.cache->capacity, provider.cache->insertions,
                                         provider.cache->insertion_capacity))
                    return cleanup_fail(call, MC_CLIENT_WORLD_LOAD_BUSY,
                                        "owned World terminal cache retirement");
                call->cache_retired = true;
            }
            if (mc_world_runtime_retire_world(call->state.world, &call->cleanup_world) !=
                MC_WORLD_STORAGE_OK)
                return cleanup_fail(call, MC_CLIENT_WORLD_LOAD_BUSY,
                                    "actual World native retirement");
            call->world_retired = true;
        }
        while (mc_world_store_chunk_count(call->state.handler) != 0) {
            mc_chunk_state *chunk = NULL;
            if (mc_world_store_chunk_at(call->state.handler, 0, &chunk, &call->cleanup_world) !=
                    MC_WORLD_STORAGE_OK ||
                mc_world_store_retire_chunk(call->state.handler, chunk, &call->cleanup_world) !=
                    MC_WORLD_STORAGE_OK)
                return cleanup_fail(call, MC_CLIENT_WORLD_LOAD_BUSY,
                                    "owned source Chunk native retirement");
        }
        if (mc_world_runtime_free(call->state.world, &call->cleanup_world) != MC_WORLD_STORAGE_OK) {
            world_cleanup_cause(call);
            return cleanup_fail(call, MC_CLIENT_WORLD_LOAD_BUSY, "actual World checked free");
        }
        call->state.world = NULL;
    }
    /* Publication gives World/Chunk Lists independent Player occurrences.
     * Their terminal retirement precedes the last Input/Settings occurrence;
     * waiting for Input alone earlier would deadlock these owned memberships. */
    if (call->state.input != NULL) {
        mc_movement_input_state input;
        if (!mc_movement_input_read_state(call->state.input, &input) || input.native_holds != 1)
            return cleanup_fail(call, MC_CLIENT_WORLD_LOAD_BUSY,
                                "external Player/input occurrences still borrow App Settings");
        if (mc_movement_input_retire(&call->state.input, &call->input_error) !=
            MC_MOVEMENT_INPUT_OK) {
            call->cleanup.input = &call->input_error;
            return cleanup_fail(call, MC_CLIENT_WORLD_LOAD_CLEANUP_FAILED,
                                "actual owned input retirement");
        }
    }
    if (mc_world_store_source_retire(&call->state.handler, &call->handler_cleanup) !=
        MC_WORLD_SOURCE_OK) {
        call->cleanup.source_store = &call->handler_cleanup;
        return cleanup_fail(call, MC_CLIENT_WORLD_LOAD_BUSY,
                            "actual source SaveHandler strict retirement");
    }
    for (size_t i = 0; i < call->controller_count; ++i) {
        if (call->controllers[i] != NULL) {
            if (mc_client_singleplayer_controller_release(call->controllers[i]) !=
                MC_SINGLEPLAYER_CONTROLLER_OK)
                return cleanup_fail(call, MC_CLIENT_WORLD_LOAD_BUSY,
                                    "captured controller native release");
            call->controllers[i] = NULL;
        }
    }
    for (size_t i = 0; i < call->format_count; ++i) {
        captured_format *capture = &call->formats[i];
        if (capture->owner != NULL) {
            if (capture->owns_pending) {
                if (mc_client_save_format_clear_failure(capture->owner, &call->save_cleanup) !=
                    MC_CLIENT_SAVE_OK) {
                    call->cleanup.save = &call->save_cleanup;
                    return cleanup_fail(call, MC_CLIENT_WORLD_LOAD_BUSY,
                                        "actual owned SaveFormat call-prefix retirement");
                }
                capture->owns_pending = false;
                call->save.handler = NULL;
            }
            if (mc_client_save_format_release(&capture->owner, &call->save_cleanup) !=
                MC_CLIENT_SAVE_OK) {
                call->cleanup.save = &call->save_cleanup;
                return cleanup_fail(call, MC_CLIENT_WORLD_LOAD_BUSY,
                                    "actual SaveFormat call-prefix retirement");
            }
        }
    }
    mc_world_info_name_release(call->format_name);
    mc_player_string_release(call->conversion_title);
    if (call->folder_held)
        mc_world_info_name_release(call->state.folder);
    if (call->display_held)
        mc_world_info_name_release(call->state.display);
    --call->classes->calls;
    call->classes->active = false;
    free(call);
    *slot = NULL;
    return MC_CLIENT_WORLD_LOAD_OK;
}
