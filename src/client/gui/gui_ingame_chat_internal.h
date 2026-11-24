#ifndef MC_CLIENT_GUI_GUI_INGAME_CHAT_INTERNAL_H
#define MC_CLIENT_GUI_GUI_INGAME_CHAT_INTERNAL_H

#include "gui_ingame_internal.h"
#include "mc/entity/player/player.h"

typedef struct mc_gui_chat_text {
    const mc_player_string *player;
    const mc_registry_string *registry;
    mc_registry_classes *registry_owner;
} mc_gui_chat_text;

typedef struct mc_gui_chat_message {
    struct mc_gui_chat_message *next;
    mc_gui_chat_text a;
    int32_t b;
    bool registry_held;
} mc_gui_chat_message;

/* Membership is established before reading an arbitrary native List slot.
 * Null and foreign objects remain distinct source null/cast failures. */
mc_gui_chat_message *mc_gui_chat_message_find(const mc_client_source_uq *, const void *);
bool mc_gui_chat_text_read(mc_gui_chat_text, mc_player_string_view *);
bool mc_gui_chat_message_release(mc_gui_chat_message *, mc_registry_error *);
/* Checked native-only cleanup. Source fields/List entries are not rewritten. */
bool mc_gui_chat_terminal(mc_client_source_uq *, mc_client_gui_ingame_error *);
bool mc_gui_chat_collect(mc_client_source_uq *, mc_client_gui_ingame_error *);

#endif
