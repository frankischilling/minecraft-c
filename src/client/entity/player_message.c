#include "player_message.h"
#include "../../entity/entity_internal.h"
#include "../application_state.h"
#include "mc/entity/player/player.h"
#include <stdlib.h>

struct mc_client_player_message_call {
    mc_entity *receiver;
    bool active;
    mc_client_player_message_state state;
    mc_client_player_message_error failure, cleanup;
    mc_client_gui_ingame_chat_call *chat;
    mc_client_gui_ingame_chat_error chat_error, chat_cleanup;
};

static mc_client_player_message_status publish(mc_client_player_message_error *out,
                                               const mc_client_player_message_error *error) {
    if (out != NULL)
        *out = *error;
    return error->status;
}

static void fail(mc_client_player_message_call *call, mc_client_player_message_status status,
                 const char *operation, const char *class_name, unsigned int pc, bool native) {
    call->failure.status = status;
    call->failure.operation = operation;
    call->failure.site = (mc_player_site){class_name, "b", "(Ljava/lang/String;)V", pc};
    call->failure.native_policy = native;
    if (status == MC_CLIENT_PLAYER_MESSAGE_SOURCE_NULL) {
        call->failure.source_exception = true;
        call->failure.exception_class = "java/lang/NullPointerException";
    }
}

static void child_failure(mc_client_player_message_call *call) {
    const mc_client_gui_ingame_chat_error *chat = &call->chat_error;
    const mc_client_gui_ingame_site *site =
        chat->child.class_name == NULL ? &chat->site : &chat->child;
    call->failure.chat = chat;
    call->failure.child =
        (mc_player_site){site->class_name, site->method, site->descriptor, site->pc};
    fail(call,
         chat->status == MC_CLIENT_GUI_INGAME_CHAT_SOURCE_NULL
             ? MC_CLIENT_PLAYER_MESSAGE_SOURCE_NULL
         : chat->status == MC_CLIENT_GUI_INGAME_CHAT_BUSY  ? MC_CLIENT_PLAYER_MESSAGE_BUSY
         : chat->status == MC_CLIENT_GUI_INGAME_CHAT_LIMIT ? MC_CLIENT_PLAYER_MESSAGE_LIMIT
         : chat->status == MC_CLIENT_GUI_INGAME_CHAT_ALLOCATION
             ? MC_CLIENT_PLAYER_MESSAGE_ALLOCATION_FAILED
             : MC_CLIENT_PLAYER_MESSAGE_CHILD_FAILED,
         chat->operation, "dc", 8, chat->native_policy);
    if (!chat->native_policy && chat->status == MC_CLIENT_GUI_INGAME_CHAT_CHILD_FAILED &&
        (chat->registry.status == MC_REGISTRY_NULL_REFERENCE ||
         chat->registry.status == MC_REGISTRY_FAILED_INITIALIZATION)) {
        call->failure.source_error = true;
        call->failure.exception_class = chat->registry.status == MC_REGISTRY_FAILED_INITIALIZATION
                                            ? "java/lang/NoClassDefFoundError"
                                            : "java/lang/ExceptionInInitializerError";
    }
}

mc_client_player_message_status
mc_client_player_message_begin(mc_entity *receiver, mc_registry_classes *literal_owner,
                               const mc_registry_string *key, mc_client_player_message_call **out,
                               mc_client_player_message_error *error) {
    mc_client_player_message_error admission = {0};
    if (out == NULL || *out != NULL) {
        admission.status = MC_CLIENT_PLAYER_MESSAGE_ARGUMENT;
        admission.operation = "Player message native output";
        admission.native_policy = true;
        return publish(error, &admission);
    }
    mc_client_player_message_call *call = calloc(1, sizeof(*call));
    if (call == NULL) {
        admission.status = MC_CLIENT_PLAYER_MESSAGE_ALLOCATION_FAILED;
        admission.operation = "Player message native retained call";
        admission.native_policy = true;
        return publish(error, &admission);
    }
    *out = call;
    call->active = true;
    call->state.receiver_identity = (uintptr_t)receiver;
    if (receiver == NULL) {
        fail(call, MC_CLIENT_PLAYER_MESSAGE_SOURCE_NULL, "Player message null invocation receiver",
             "gs", 0, false);
        goto done;
    }
    mc_entity_class_literal_view literal;
    if (mc_entity_class_literal_inspect(receiver->class_literal, &literal) != MC_ENTITY_OK ||
        literal.source != MC_ENTITY_CLIENT || literal.owner != receiver->classes ||
        !mc_entity_is_kind(receiver, literal.kind) ||
        (literal.kind != MC_ENTITY_CLASS_KIND_LOCAL_PLAYER &&
         literal.kind != MC_ENTITY_CLASS_KIND_PLAYER)) {
        fail(call, MC_CLIENT_PLAYER_MESSAGE_ARGUMENT, "Player message canonical client dispatch",
             NULL, 0, true);
        goto done;
    }
    call->failure.entity = mc_entity_retain(receiver);
    if (call->failure.entity != MC_ENTITY_OK) {
        fail(call, MC_CLIENT_PLAYER_MESSAGE_BUSY, "Player message native receiver occurrence", NULL,
             0, true);
        goto done;
    }
    call->receiver = receiver;
    if (literal.kind == MC_ENTITY_CLASS_KIND_PLAYER) {
        call->state.base_noop = true;
        call->state.returned = call->failure.source_returned = true;
        call->failure.site = (mc_player_site){"gs", "b", "(Ljava/lang/String;)V", 0};
        goto done;
    }
    mc_local_player_snapshot fields;
    if (mc_local_player_get_snapshot(receiver, &fields) != MC_ENTITY_OK) {
        fail(call, MC_CLIENT_PLAYER_MESSAGE_ARGUMENT, "Player message actual DC fields", "dc", 1,
             true);
        goto done;
    }
    client_application *app = fields.minecraft; /* Actual captured DC.b1. */
    call->state.app_identity = (uintptr_t)app;
    if (app == NULL) {
        fail(call, MC_CLIENT_PLAYER_MESSAGE_SOURCE_NULL, "DC message actual App null receiver",
             "dc", 4, false);
        goto done;
    }
    mc_client_source_uq *uq = app->v; /* Actual captured Minecraft.v4. */
    call->state.uq_identity = (uintptr_t)uq;
    if (uq == NULL) {
        fail(call, MC_CLIENT_PLAYER_MESSAGE_SOURCE_NULL, "DC message actual UQ null receiver", "dc",
             8, false);
        goto done;
    }
    if (mc_client_gui_ingame_chat_translate_registry_begin(uq, app->registry, literal_owner, key,
                                                           &call->chat, &call->chat_error) !=
        MC_CLIENT_GUI_INGAME_CHAT_OK) {
        child_failure(call);
        goto done;
    }
    call->state.returned = call->failure.source_returned = true;
    call->failure.site = (mc_player_site){"dc", "b", "(Ljava/lang/String;)V", 11};
done:
    call->active = false;
    return publish(error, &call->failure);
}

bool mc_client_player_message_inspect(const mc_client_player_message_call *call,
                                      mc_client_player_message_state *out) {
    if (call == NULL || out == NULL || call->active)
        return false;
    *out = call->state;
    out->chat = call->chat;
    out->failure = &call->failure;
    out->cleanup = &call->cleanup;
    return true;
}

mc_client_player_message_status
mc_client_player_message_retire(mc_client_player_message_call **slot,
                                mc_client_player_message_error *error) {
    mc_client_player_message_error empty = {0};
    if (slot == NULL) {
        empty.status = MC_CLIENT_PLAYER_MESSAGE_ARGUMENT;
        empty.operation = "Player message native retirement slot";
        empty.native_policy = true;
        return publish(error, &empty);
    }
    mc_client_player_message_call *call = *slot;
    if (call == NULL)
        return publish(error, &empty);
    if (call->active) {
        empty.status = MC_CLIENT_PLAYER_MESSAGE_BUSY;
        empty.operation = "Player message entered retirement";
        empty.native_policy = true;
        return publish(error, &empty);
    }
    call->active = true;
    if (mc_client_gui_ingame_chat_retire(&call->chat, &call->chat_cleanup) !=
        MC_CLIENT_GUI_INGAME_CHAT_OK) {
        call->cleanup = (mc_client_player_message_error){
            .status = MC_CLIENT_PLAYER_MESSAGE_CLEANUP_FAILED,
            .operation = "Player message retained UQ native retirement",
            .native_policy = true,
            .chat = &call->chat_cleanup};
        call->active = false;
        return publish(error, &call->cleanup);
    }
    mc_entity_release(call->receiver);
    free(call);
    *slot = NULL;
    return publish(error, &empty);
}
