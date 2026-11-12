#ifndef MC_CLIENT_GUI_SOURCE_TEXT_FIELD_H
#define MC_CLIENT_GUI_SOURCE_TEXT_FIELD_H

#include "font_mapping.h"
#include "source_screen.h"

typedef struct mc_client_source_ro mc_client_source_ro;

typedef struct mc_client_text_field_fields {
    float k; /* Inherited UB field, left at its default by this constructor. */
    mc_client_bitmap_font *c;
    int32_t d, e, f, g;
    mc_client_screen_text h;
    int32_t i, j;
    bool a, b;
    mc_client_source_da *l;
} mc_client_text_field_fields;

typedef struct mc_client_text_field_state {
    mc_client_screen_phase phase;
    mc_client_text_field_fields fields;
    bool active, retired;
    mc_client_screen_site site;
    mc_client_screen_error failure;
    mc_client_font_mapping_error mapping;
    size_t tab_calls, clipboard_calls, text_stores;
} mc_client_text_field_state;

typedef struct mc_client_text_field_hooks {
    mc_client_font_mapping *mapping; /* Actual invoking FP class namespace. */
    void *context;
    bool (*tab)(void *, mc_client_source_da *actual_parent, mc_client_screen_error *);
    /* Actual DA.d() source call. Success transfers one owning occurrence of
     * its nullable returned Player String. Failure preserves the NULL output
     * and keeps its diagnostic producer live under the enclosing screen. */
    bool (*clipboard)(void *, mc_player_string **out, mc_client_screen_error *);
} mc_client_text_field_hooks;

/* Nested RO storage belongs to the exact enclosing screen's retained arena.
 * These entered helpers require its screen-class lock. That screen and its
 * initialization record keep the actual parent/Font/Registry/FP dependencies
 * live and exclusive through every call, failed prefix and final release.
 * Parent and Font references do not own recursive screen holds. Player String
 * fields own exact occurrences; initial Registry text keeps its identity.
 * No source phase/provenance gate is added to nullable String reads/stores. */
mc_client_source_ro *mc_screen_text_field_allocate_locked(mc_client_source_da *native_owner,
                                                          mc_client_screen_error *);
bool mc_screen_text_field_construct_locked(mc_client_source_ro *, mc_client_source_da *parent,
                                           mc_client_bitmap_font *, int32_t x, int32_t y,
                                           int32_t width, int32_t height, mc_client_screen_text,
                                           mc_client_screen_error *);
bool mc_screen_text_field_set_text_locked(mc_client_source_ro *, mc_client_screen_text,
                                          mc_client_screen_error *);
bool mc_screen_text_field_set_maximum_locked(mc_client_source_ro *, int32_t,
                                             mc_client_screen_error *);
bool mc_screen_text_field_tick_locked(mc_client_source_ro *, mc_client_screen_error *);
bool mc_screen_text_field_focus_locked(mc_client_source_ro *, bool, mc_client_screen_error *);
bool mc_screen_text_field_mouse_locked(mc_client_source_ro *, int32_t x, int32_t y,
                                       int32_t unused_button, mc_client_screen_error *);
bool mc_screen_text_field_key_locked(mc_client_source_ro *, uint16_t character, int32_t key,
                                     const mc_client_text_field_hooks *, mc_client_screen_error *);
bool mc_client_text_field_inspect(const mc_client_source_ro *, mc_client_text_field_state *);
/* Active refusal preserves the exact field and String occurrence for retry.
 * Success ends borrowed views and consumes only this native slot; retained
 * arena storage is freed by its enclosing screen. */
bool mc_screen_text_field_release_locked(mc_client_source_ro **, mc_client_screen_error *);

#endif
