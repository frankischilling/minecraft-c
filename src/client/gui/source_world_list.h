#ifndef MC_CLIENT_GUI_SOURCE_WORLD_LIST_H
#define MC_CLIENT_GUI_SOURCE_WORLD_LIST_H
#include "../display/sdk_input.h"
#include "bitmap_font.h"
#include "gui_main_menu.h"
#include "source_world_selection.h"

typedef struct mc_client_screen_transitions mc_client_screen_transitions;

typedef struct mc_client_world_list_fields {
    client_application *a;
    int32_t b, c, h, i, d, e, j, f, g;
    float k, l, m;
    int32_t n;
    int64_t o;
    bool p, q;
    int32_t r;
} mc_client_world_list_fields;

struct mc_client_source_world_list {
    mc_client_source_rq *a;             /* Actual mg.a, before lg constructor entry. */
    mc_client_world_list_fields fields; /* Actual lg instance fields. */
    mc_client_source_rq *native_owner;
    mc_client_screen_phase phase;
    bool active, retired;
    unsigned int last_pc;
};

typedef struct mc_client_world_list_error {
    mc_client_screen_error screen;
    mc_client_sdk_input_error input;
    mc_client_save_error save;
    mc_platform_date_format_error date;
    mc_client_bitmap_font_error font;
    mc_gui_main_menu_error renderer;
    mc_client_display_error display;
    size_t rows_drawn, clock_samples, selections, font_measurements;
    bool source_returned;
} mc_client_world_list_error;

/* Nested RQ source construction. The caller holds the same screen mutex.
 * Native retained-arena storage belongs to RQ, including failed prefixes.
 * Release ends only this native occurrence; it does not rewrite lg fields. */
mc_client_source_world_list *mc_screen_world_list_allocate_locked(mc_client_source_rq *,
                                                                  mc_client_screen_error *);
bool mc_screen_world_list_construct_locked(mc_client_source_world_list *, mc_client_screen_error *);
bool mc_screen_world_list_buttons_locked(mc_client_source_world_list *, mc_client_screen_list *,
                                         int32_t up, int32_t down, mc_client_screen_error *);
bool mc_screen_world_list_release_locked(mc_client_source_world_list **, mc_client_screen_error *);
bool mc_screen_world_list_count(mc_client_source_world_list *, int32_t *,
                                mc_client_world_list_error *);
int32_t mc_screen_world_list_maximum(mc_client_source_world_list *, int32_t count);
void mc_screen_world_list_clamp(mc_client_source_world_list *, int32_t count);

/* Actual lg.a(ke), including negative short-list clamp and k=-2 stores. */
mc_client_screen_status mc_client_world_list_button(mc_client_source_world_list *,
                                                    mc_client_source_ke *,
                                                    mc_client_screen_error *);
/* Actual lg.a(IIF), with synchronous mg.a(index,doubleClick) during drawing.
 * Caller has begun the shared GUI canvas and keeps RQ/SDK/router/Font live.
 * No wheel polling, deferred action, scissor clipping, swap or frame clock. */
bool mc_client_world_list_draw(mc_client_source_world_list *, mc_gui_main_menu *,
                               mc_gui_mouse_position, float partial_tick, mc_client_sdk_input *,
                               mc_gui_main_menu_millis_fn, void *, mc_client_screen_transitions *,
                               mc_client_world_list_error *);
#endif
