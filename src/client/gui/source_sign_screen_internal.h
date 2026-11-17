#ifndef MC_CLIENT_GUI_SOURCE_SIGN_SCREEN_INTERNAL_H
#define MC_CLIENT_GUI_SOURCE_SIGN_SCREEN_INTERNAL_H
#include "source_screen_internal.h"
#include "source_sign_screen.h"

typedef struct mc_sign_initialization {
    struct mc_sign_initialization *next;
    mc_client_source_ke *button;
} mc_sign_initialization;

/* Native custody for the exact dirty invocation. Reserve before dirty19 so a
 * returned producer never needs a fallible allocation in order to stay live.
 * Multiple Done calls keep distinct records; aliases within a call retain one
 * occurrence. The public diagnostic remains a value copy of the last call. */
typedef struct mc_sign_dirty_call {
    struct mc_sign_dirty_call *next;
    mc_world_access_failure captures[3];
    size_t count;
} mc_sign_dirty_call;

struct mc_client_source_yc {
    mc_client_source_da base;
    mc_player_string *title;
    mc_tile_entity *tile, *held_tile;
    mc_client_sdk_input *input;
    mc_tile_entity_classes *literal_classes;
    int32_t tick, row;
    size_t source_calls, initialization_count;
    unsigned int last_pc, row_stores, line_stores, tick_stores;
    const char *method;
    bool terminal_prepared;
    bool button_monitor_lost;
    mc_sign_initialization *initializations;
    mc_sign_dirty_call *dirty_calls;
    mc_client_sign_screen_error detail;
    mc_client_screen_error cleanup_failure;
};

bool mc_screen_sign_active_locked(mc_client_screen_classes *, mc_client_font_mapping *,
                                  mc_client_screen_error *);
bool mc_screen_sign_fail(mc_client_source_yc *, mc_client_screen_error *, mc_client_screen_status,
                         const char *, const char *method, const char *descriptor, unsigned int pc,
                         bool native);
bool mc_screen_sign_initialize_locked(mc_client_source_yc *, mc_client_screen_error *);
bool mc_screen_sign_key_locked(mc_client_source_yc *, uint16_t, int32_t, mc_client_screen_error *);
bool mc_screen_sign_tick_locked(mc_client_source_yc *, mc_client_screen_error *);
bool mc_screen_sign_closed_locked(mc_client_source_yc *, mc_client_screen_error *);
bool mc_screen_sign_cleanup_locked(mc_client_source_yc *, mc_client_screen_error *);
bool mc_screen_sign_button_prefix_locked(mc_client_source_yc *, mc_client_source_ke *, bool *,
                                         client_application **, mc_client_screen_error *);
/* Same failed Minecraft display projection for direct and queued Done calls.
 * The caller retains the complete display child; this copies its reached
 * Source/native classification without replacing enclosing YC.a27. */
bool mc_screen_sign_display_failed(mc_client_source_yc *, const mc_client_display_screen_call *,
                                   mc_client_screen_error *);
#endif
