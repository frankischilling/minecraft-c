#ifndef MC_CLIENT_GUI_SOURCE_CREATE_WORLD_INTERNAL_H
#define MC_CLIENT_GUI_SOURCE_CREATE_WORLD_INTERNAL_H

#include "../display/sdk_input.h"
#include "source_create_world.h"
#include "source_screen_internal.h"
#include "source_text_field_internal.h"

typedef struct mc_create_world_initialization {
    struct mc_create_world_initialization *next;
    mc_client_source_ke *buttons[2];
    mc_client_source_ro *name, *seed;
    unsigned int button_count;
    bool returned;
    mc_client_screen_error failure;
} mc_create_world_initialization;
typedef struct mc_create_world_naming mc_create_world_naming;
typedef struct mc_create_world_string mc_create_world_string;
typedef struct mc_create_world_seed mc_create_world_seed;

struct mc_client_source_fj {
    mc_client_source_da base;
    mc_client_screen_fj_fields fields;
    bool parent_held;
    mc_client_font_mapping *mapping; /* Same actual GUI class namespace; native borrow. */
    mc_create_world_initialization *initializations, *last_initialization;
    mc_create_world_naming *namings, *last_naming;
    mc_create_world_string *strings;
    mc_create_world_seed *seeds, *last_seed;
    mc_player_string *folder_hold;
    size_t initialization_count, naming_calls, metadata_queries;
    const char *method;
    unsigned int last_pc;
    mc_client_screen_error failure;
    mc_client_save_error save;
    mc_client_sdk_input_error input;
    mc_client_font_mapping_error mapping_error;
};

/* Caller owns this SAME screen-class monitor. The action bridge executes the
 * actual FJ.a(KE) synchronously, with its own cross-owner unlock/relock. It must
 * return before key85/k89, even if it has changed the current App screen. */
typedef bool (*mc_create_world_enter_fn)(void *, mc_client_source_fj *, mc_client_source_ke *,
                                         mc_client_screen_error *);
mc_client_source_fj *mc_screen_create_world_allocate_locked(mc_client_screen_classes *,
                                                            const mc_client_screen_options *,
                                                            mc_client_screen_error *);
bool mc_screen_create_world_construct_locked(mc_client_source_fj *, mc_client_source_da *,
                                             mc_client_screen_error *);
bool mc_screen_create_world_initialize_locked(mc_client_source_fj *, mc_client_screen_error *);
bool mc_screen_create_world_cleanup_locked(mc_client_source_fj *, mc_client_screen_error *);
bool mc_screen_create_world_tick_locked(mc_client_source_fj *, mc_client_screen_error *);
bool mc_screen_create_world_closed_locked(mc_client_source_fj *, mc_client_screen_error *);
bool mc_screen_create_world_tab_locked(mc_client_source_fj *, mc_client_screen_error *);
bool mc_screen_create_world_name_locked(mc_client_source_fj *, mc_client_screen_error *);
bool mc_screen_create_world_key_locked(mc_client_source_fj *, uint16_t character, int32_t key,
                                       const mc_client_text_field_hooks *, mc_create_world_enter_fn,
                                       void *, mc_client_screen_error *);
/* Enter only after the actual DA.a(III) body returns; never re-read App.r. */
bool mc_screen_create_world_mouse_after_base_locked(mc_client_source_fj *, int32_t x, int32_t y,
                                                    int32_t button, mc_client_screen_error *);
/* FJ.a(KE)62..114 only: actual new Random/default constructor + nextLong, then
 * exact nonzero decimal override or signed UTF16 hash on NumberFormatException.
 * Caller executes display42 and guard55 FIRST. No guard/display/load is added. */
bool mc_screen_create_world_seed_locked(mc_client_source_fj *, int64_t *out,
                                        mc_client_screen_error *);

#endif
