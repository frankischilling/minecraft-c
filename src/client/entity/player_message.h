#ifndef MC_CLIENT_ENTITY_PLAYER_MESSAGE_H
#define MC_CLIENT_ENTITY_PLAYER_MESSAGE_H

#include "../gui/gui_ingame_chat.h"

typedef struct mc_client_player_message_call mc_client_player_message_call;

typedef enum mc_client_player_message_status {
    MC_CLIENT_PLAYER_MESSAGE_OK,
    MC_CLIENT_PLAYER_MESSAGE_ARGUMENT,
    MC_CLIENT_PLAYER_MESSAGE_BUSY,
    MC_CLIENT_PLAYER_MESSAGE_LIMIT,
    MC_CLIENT_PLAYER_MESSAGE_ALLOCATION_FAILED,
    MC_CLIENT_PLAYER_MESSAGE_SOURCE_NULL,
    MC_CLIENT_PLAYER_MESSAGE_CHILD_FAILED,
    MC_CLIENT_PLAYER_MESSAGE_CLEANUP_FAILED
} mc_client_player_message_status;

typedef struct mc_client_player_message_error {
    mc_client_player_message_status status;
    const char *operation, *exception_class;
    mc_player_site site, child;
    bool native_policy, source_exception, source_error, source_returned;
    mc_entity_status entity;
    const mc_client_gui_ingame_chat_error *chat;
} mc_client_player_message_error;

typedef struct mc_client_player_message_state {
    bool active, returned, base_noop;
    uintptr_t receiver_identity, app_identity, uq_identity;
    const mc_client_gui_ingame_chat_call *chat;
    const mc_client_player_message_error *failure, *cleanup;
} mc_client_player_message_state;

/* Reached virtual client GS.b(String): literal base return0, or actual DC
 * App1/UQ4/UQ.c8. The key is the caller's actual Registry literal; DC null App
 * or UQ faults before key payload validation/NH use. The enclosing gameplay
 * owner keeps the SAME actual App/Registry graph live/exclusive until this
 * call is retired. No active-gameplay App/HUD is substituted for DC's field.
 * Begin may return a retained failed invocation. Retire successful children
 * promptly, and failed children before their placement/use/App producers. */
mc_client_player_message_status
mc_client_player_message_begin(mc_entity *actual_receiver, mc_registry_classes *literal_owner,
                               const mc_registry_string *nullable_key,
                               mc_client_player_message_call **out,
                               mc_client_player_message_error *);
bool mc_client_player_message_inspect(const mc_client_player_message_call *,
                                      mc_client_player_message_state *);
mc_client_player_message_status mc_client_player_message_retire(mc_client_player_message_call **,
                                                                mc_client_player_message_error *);

#endif
