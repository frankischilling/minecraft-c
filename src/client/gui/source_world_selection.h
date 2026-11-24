#ifndef MC_CLIENT_GUI_SOURCE_WORLD_SELECTION_H
#define MC_CLIENT_GUI_SOURCE_WORLD_SELECTION_H
#include "../../platform/time/date_format.h"
#include "mc/client/saves/save_format.h"
#include "source_screen.h"

typedef struct mc_client_source_rq mc_client_source_rq;
typedef struct mc_client_source_world_list mc_client_source_world_list;

typedef struct mc_client_screen_rq_fields {
    mc_client_source_da *a; /* Exact parent da identity, distinct from inherited da.a. */
    mc_client_screen_text i, p, q;
    mc_platform_date_format *j;
    bool l, r;
    int32_t m;
    mc_client_save_list *n;
    mc_client_source_world_list *o;
    mc_client_source_ke *s, *t, *u;
} mc_client_screen_rq_fields;

typedef struct mc_client_world_selection_error {
    mc_client_screen_error screen;
    mc_client_save_error save;
    mc_platform_date_format_error date;
} mc_client_world_selection_error;

typedef struct mc_client_world_selection_state {
    mc_client_screen_phase phase;
    size_t holds, initialization_count;
    mc_client_screen_rq_fields fields;
    const char *method;
    unsigned int last_pc, store_count;
    mc_client_screen_site stores[32];
    const mc_client_world_selection_error *failure;
} mc_client_world_selection_state;

/* Actual rq(da), with da construction before SimpleDateFormat9, j12, i18,
 * l23 and parent28. The parent belongs to the same screen namespace, remains
 * the SAME source identity and is held through all fallible native cleanup.
 * Cold allocation has no source constructor effects. Failed prefixes remain
 * inspectable and retire through the common da lifetime, without source stores.
 * DateFormat captures the genuine default profile even for an empty save list. */
mc_client_screen_status mc_client_screen_world_selection_allocate(mc_client_screen_classes *,
                                                                  const mc_client_screen_options *,
                                                                  mc_client_source_rq **,
                                                                  mc_client_screen_error *);
mc_client_screen_status mc_client_screen_world_selection_construct(mc_client_source_rq *,
                                                                   mc_client_source_da *parent,
                                                                   mc_client_screen_error *);
mc_client_source_da *mc_client_screen_world_selection_base(mc_client_source_rq *);
/* Protected rq.b() only. Normal callers use the common da.a(App,II), whose
 * virtual b45 dispatch invokes this same body under its screen-class lock.
 * Every invocation owns its reached old/new lists, MG and button occurrences;
 * n15 publishes the actual App.aa list BEFORE stable sort22 and m=-1 at27. */
mc_client_screen_status mc_client_screen_world_selection_initialize(mc_client_source_rq *,
                                                                    mc_client_screen_error *);
mc_client_screen_status mc_client_screen_world_selection_inspect(mc_client_source_rq *,
                                                                 mc_client_world_selection_state *,
                                                                 mc_client_screen_error *);
mc_client_screen_status mc_client_screen_world_selection_release(mc_client_source_rq **,
                                                                 mc_client_screen_error *);
mc_client_screen_status mc_client_screen_world_selection_retire(mc_client_source_rq **,
                                                                mc_client_screen_error *);
#endif
