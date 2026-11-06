#ifndef MC_CLIENT_GUI_SOURCE_WORLD_SELECTION_INTERNAL_H
#define MC_CLIENT_GUI_SOURCE_WORLD_SELECTION_INTERNAL_H
#include "source_screen_internal.h"
#include "source_world_selection.h"

typedef struct mc_screen_world_selection_initialization {
    struct mc_screen_world_selection_initialization *next;
    mc_client_save_list *list;
    mc_client_source_world_list *scroll;
    mc_client_source_ke *buttons[5], *field_buttons[3];
    unsigned int button_count;
    bool returned;
    mc_client_world_selection_error failure;
} mc_screen_world_selection_initialization;

struct mc_client_source_rq {
    mc_client_source_da base;
    mc_client_screen_rq_fields fields;
    mc_platform_date_format *pending_format;
    bool parent_held;
    mc_screen_world_selection_initialization *initializations, *last_initialization;
    size_t initialization_count;
    const char *method;
    unsigned int last_pc, store_count;
    mc_client_screen_site stores[32];
    mc_client_world_selection_error failure;
};

/* Concrete nested source calls: caller holds this SAME screen-class mutex.
 * No public re-lock, deferred action, alternate namespace or virtual callback. */
mc_client_source_rq *mc_screen_world_selection_allocate_locked(mc_client_screen_classes *,
                                                               const mc_client_screen_options *,
                                                               mc_client_screen_error *);
bool mc_screen_world_selection_construct_locked(mc_client_source_rq *, mc_client_source_da *,
                                                mc_client_screen_error *);
bool mc_screen_world_selection_initialize_locked(mc_client_source_rq *, mc_client_screen_error *);
bool mc_screen_world_selection_cleanup_locked(mc_client_source_rq *, mc_client_screen_error *);
#endif
