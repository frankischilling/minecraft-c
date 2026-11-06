#ifndef MC_CLIENT_GUI_GUI_INGAME_CHAT_H
#define MC_CLIENT_GUI_GUI_INGAME_CHAT_H

#include "bitmap_font.h"
#include "mc/client/gui/gui_ingame.h"
#include "mc/resources/translation/translation.h"

typedef struct mc_client_gui_ingame_chat_call mc_client_gui_ingame_chat_call;

typedef enum mc_client_gui_ingame_chat_status {
    MC_CLIENT_GUI_INGAME_CHAT_OK,
    MC_CLIENT_GUI_INGAME_CHAT_ARGUMENT,
    MC_CLIENT_GUI_INGAME_CHAT_BUSY,
    MC_CLIENT_GUI_INGAME_CHAT_LIMIT,
    MC_CLIENT_GUI_INGAME_CHAT_ALLOCATION,
    MC_CLIENT_GUI_INGAME_CHAT_SOURCE_NULL,
    MC_CLIENT_GUI_INGAME_CHAT_SOURCE_TYPE,
    MC_CLIENT_GUI_INGAME_CHAT_CHILD_FAILED
} mc_client_gui_ingame_chat_status;

typedef struct mc_client_gui_ingame_chat_error {
    mc_client_gui_ingame_chat_status status;
    const char *operation;
    mc_client_gui_ingame_site site, child;
    bool native_policy, source_returned;
    mc_registry_error registry;
    mc_client_bitmap_font_error font;
    mc_player_status string;
} mc_client_gui_ingame_chat_error;

typedef struct mc_client_gui_ingame_chat_state {
    bool translated, returned;
    unsigned int source_pc, widths, substrings, messages, removals;
    size_t scanned_units;
    const mc_client_gui_ingame_chat_error *failure, *cleanup;
} mc_client_gui_ingame_chat_state;

/* Actual UQ.c(String): NH singleton active use0, getProperty/fallback6, UQ.a12.
 * Registry must be the actual client's same-side/profile namespace. It is held
 * without activating the separate translation wrapper. The actual nullable
 * key identity is retained. Source effects and already inserted fragments
 * survive failure; a returned nonnull call owns every failed native prefix.
 * Retire the call before the next ordinary HUD operation. Successful retirement
 * releases its workspace; UQ keeps the genuine SW/String owners in its List.
 * No English text, replacement Font or callback translation is supplied. */
mc_client_gui_ingame_chat_status mc_client_gui_ingame_chat_translate_begin(
    mc_client_source_uq *, mc_registry_classes *, const mc_player_string *nullable_key,
    mc_client_gui_ingame_chat_call **out, mc_client_gui_ingame_chat_error *);
/* Actual UQ.a(String), including null text, width320 UTF16 splitting, recursive
 * prefix insertion, front insertion and cap50. Native work is bounded to16Mi
 * measured units and64 nested prefixes; limits preserve all reached effects. */
mc_client_gui_ingame_chat_status
mc_client_gui_ingame_chat_add_begin(mc_client_source_uq *, const mc_player_string *nullable_text,
                                    mc_client_gui_ingame_chat_call **out,
                                    mc_client_gui_ingame_chat_error *);
bool mc_client_gui_ingame_chat_inspect(const mc_client_gui_ingame_chat_call *,
                                       mc_client_gui_ingame_chat_state *);
mc_client_gui_ingame_chat_status
mc_client_gui_ingame_chat_retire(mc_client_gui_ingame_chat_call **,
                                 mc_client_gui_ingame_chat_error *);

#endif
